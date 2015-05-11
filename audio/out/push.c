/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <pthread.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "osdep/io.h"

#include "ao.h"
#include "internal.h"
#include "audio/format.h"

#include "common/msg.h"
#include "common/common.h"

#include "input/input.h"

#include "osdep/threads.h"
#include "osdep/timer.h"
#include "osdep/atomics.h"

#include "audio/audio.h"
#include "audio/audio_buffer.h"

struct ao_push_state {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // --- protected by lock

    struct mp_audio_buffer *buffer;

    bool terminate;
    bool wait_on_ao;
    bool still_playing;
    bool need_wakeup;
    bool paused;

    // Whether the current buffer contains the complete audio.
    bool final_chunk;
    double expected_end_time;

    int wakeup_pipe[2];
};

// lock must be held
static void wakeup_playthread(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    if (ao->driver->wakeup)
        ao->driver->wakeup(ao);
    p->need_wakeup = true;
    pthread_cond_signal(&p->wakeup);
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    int r = CONTROL_UNKNOWN;
    if (ao->driver->control) {
        struct ao_push_state *p = ao->api_priv;
        pthread_mutex_lock(&p->lock);
        r = ao->driver->control(ao, cmd, arg);
        pthread_mutex_unlock(&p->lock);
    }
    return r;
}

static double unlocked_get_delay(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    double driver_delay = 0;
    if (ao->driver->get_delay)
        driver_delay = ao->driver->get_delay(ao);
    return driver_delay + mp_audio_buffer_seconds(p->buffer);
}

static double get_delay(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    double delay = unlocked_get_delay(ao);
    pthread_mutex_unlock(&p->lock);
    return delay;
}

static void reset(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    if (ao->driver->reset)
        ao->driver->reset(ao);
    mp_audio_buffer_clear(p->buffer);
    p->paused = false;
    if (p->still_playing)
        wakeup_playthread(ao);
    p->still_playing = false;
    pthread_mutex_unlock(&p->lock);
}

static void audio_pause(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    if (ao->driver->pause)
        ao->driver->pause(ao);
    p->paused = true;
    wakeup_playthread(ao);
    pthread_mutex_unlock(&p->lock);
}

static void resume(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    if (ao->driver->resume)
        ao->driver->resume(ao);
    p->paused = false;
    p->expected_end_time = 0;
    wakeup_playthread(ao);
    pthread_mutex_unlock(&p->lock);
}

static void drain(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;

    MP_VERBOSE(ao, "draining...\n");

    pthread_mutex_lock(&p->lock);
    if (p->paused)
        goto done;

    p->final_chunk = true;
    wakeup_playthread(ao);
    while (p->still_playing && mp_audio_buffer_samples(p->buffer) > 0)
        pthread_cond_wait(&p->wakeup, &p->lock);

    if (ao->driver->drain) {
        ao->driver->drain(ao);
    } else {
        double time = unlocked_get_delay(ao);
        mp_sleep_us(MPMIN(time, ao->buffer / (double)ao->samplerate + 1) * 1e6);
    }

done:
    pthread_mutex_unlock(&p->lock);

    reset(ao);
}

static int unlocked_get_space(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    int space = mp_audio_buffer_get_write_available(p->buffer);
    if (ao->driver->get_space) {
        // The following code attempts to keep the total buffered audio to
        // ao->buffer in order to improve latency.
        int device_space = ao->driver->get_space(ao);
        int device_buffered = ao->device_buffer - device_space;
        int soft_buffered = mp_audio_buffer_samples(p->buffer);
        // The extra margin helps avoiding too many wakeups if the AO is fully
        // byte based and doesn't do proper chunked processing.
        int min_buffer = ao->buffer + 64;
        int missing = min_buffer - device_buffered - soft_buffered;
        // But always keep the device's buffer filled as much as we can.
        int device_missing = device_space - soft_buffered;
        missing = MPMAX(missing, device_missing);
        space = MPMIN(space, missing);
        space = MPMAX(0, space);
    }
    return space;
}

static int get_space(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    int space = unlocked_get_space(ao);
    pthread_mutex_unlock(&p->lock);
    return space;
}

static bool get_eof(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    bool eof = !p->still_playing;
    pthread_mutex_unlock(&p->lock);
    return eof;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct ao_push_state *p = ao->api_priv;

    pthread_mutex_lock(&p->lock);

    int write_samples = mp_audio_buffer_get_write_available(p->buffer);
    write_samples = MPMIN(write_samples, samples);

    MP_TRACE(ao, "samples=%d flags=%d r=%d\n", samples, flags, write_samples);

    if (write_samples < samples)
        flags = flags & ~AOPLAY_FINAL_CHUNK;
    bool is_final = flags & AOPLAY_FINAL_CHUNK;

    struct mp_audio audio;
    mp_audio_buffer_get_format(p->buffer, &audio);
    for (int n = 0; n < ao->num_planes; n++)
        audio.planes[n] = data[n];
    audio.samples = write_samples;
    mp_audio_buffer_append(p->buffer, &audio);

    bool got_data = write_samples > 0 || p->paused || p->final_chunk != is_final;

    p->final_chunk = is_final;
    p->paused = false;
    if (got_data) {
        p->still_playing = true;
        p->expected_end_time = 0;
    }

    // If we don't have new data, the decoder thread basically promises it
    // will send new data as soon as it's available.
    if (got_data)
        wakeup_playthread(ao);
    pthread_mutex_unlock(&p->lock);
    return write_samples;
}

// called locked
static void ao_play_data(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    struct mp_audio data;
    mp_audio_buffer_peek(p->buffer, &data);
    int max = data.samples;
    int space = ao->driver->get_space(ao);
    space = MPMAX(space, 0);
    if (data.samples > space)
        data.samples = space;
    int flags = 0;
    if (p->final_chunk && data.samples == max)
        flags |= AOPLAY_FINAL_CHUNK;
    MP_STATS(ao, "start ao fill");
    int r = 0;
    if (data.samples)
        r = ao->driver->play(ao, data.planes, data.samples, flags);
    MP_STATS(ao, "end ao fill");
    if (r > data.samples) {
        MP_WARN(ao, "Audio device returned non-sense value.\n");
        r = data.samples;
    }
    r = MPMAX(r, 0);
    // Probably can't copy the rest of the buffer due to period alignment.
    bool stuck_eof = r <= 0 && space >= max && data.samples > 0;
    if ((flags & AOPLAY_FINAL_CHUNK) && stuck_eof) {
        MP_ERR(ao, "Audio output driver seems to ignore AOPLAY_FINAL_CHUNK.\n");
        r = max;
    }
    mp_audio_buffer_skip(p->buffer, r);
    if (r > 0)
        p->expected_end_time = 0;
    // Nothing written, but more input data than space - this must mean the
    // AO's get_space() doesn't do period alignment correctly.
    bool stuck = r == 0 && max >= space && space > 0;
    if (stuck)
        MP_ERR(ao, "Audio output is reporting incorrect buffer status.\n");
    // Wait until space becomes available. Also wait if we actually wrote data,
    // so the AO wakes us up properly if it needs more data.
    p->wait_on_ao = space == 0 || r > 0 || stuck;
    p->still_playing |= r > 0;
    // If we just filled the AO completely (r == space), don't refill for a
    // while. Prevents wakeup feedback with byte-granular AOs.
    int needed = unlocked_get_space(ao);
    bool more = needed >= (r == space ? ao->device_buffer / 4 : 1) && !stuck;
    if (more)
        mp_input_wakeup(ao->input_ctx); // request more data
    MP_TRACE(ao, "in=%d flags=%d space=%d r=%d wa=%d needed=%d more=%d\n",
             max, flags, space, r, p->wait_on_ao, needed, more);
}

static void *playthread(void *arg)
{
    struct ao *ao = arg;
    struct ao_push_state *p = ao->api_priv;
    mpthread_set_name("ao");
    pthread_mutex_lock(&p->lock);
    while (!p->terminate) {
        if (!p->paused)
            ao_play_data(ao);

        if (!p->need_wakeup) {
            MP_STATS(ao, "start audio wait");
            if (!p->wait_on_ao || p->paused) {
                // Avoid busy waiting, because the audio API will still report
                // that it needs new data, even if we're not ready yet, or if
                // get_space() decides that the amount of audio buffered in the
                // device is enough, and p->buffer can be empty.
                // The most important part is that the decoder is woken up, so
                // that the decoder will wake up us in turn.
                MP_TRACE(ao, "buffer inactive.\n");

                bool was_playing = p->still_playing;
                double timeout = -1;
                if (p->still_playing && !p->paused && p->final_chunk &&
                    !mp_audio_buffer_samples(p->buffer))
                {
                    double now = mp_time_sec();
                    if (!p->expected_end_time)
                        p->expected_end_time = now + unlocked_get_delay(ao);
                    if (p->expected_end_time < now) {
                        p->still_playing = false;
                    } else {
                        timeout = p->expected_end_time - now;
                    }
                }

                if (was_playing && !p->still_playing)
                    mp_input_wakeup(ao->input_ctx);
                pthread_cond_signal(&p->wakeup); // for draining

                if (p->still_playing && timeout > 0) {
                    struct timespec ts = mp_rel_time_to_timespec(timeout);
                    pthread_cond_timedwait(&p->wakeup, &p->lock, &ts);
                } else {
                    pthread_cond_wait(&p->wakeup, &p->lock);
                }
            } else {
                // Wait until the device wants us to write more data to it.
                if (!ao->driver->wait || ao->driver->wait(ao, &p->lock) < 0) {
                    // Fallback to guessing.
                    double timeout = 0;
                    if (ao->driver->get_delay)
                        timeout = ao->driver->get_delay(ao);
                    timeout *= 0.25; // wake up if 25% played
                    if (!p->need_wakeup) {
                        struct timespec ts = mp_rel_time_to_timespec(timeout);
                        pthread_cond_timedwait(&p->wakeup, &p->lock, &ts);
                    }
                }
            }
            MP_STATS(ao, "end audio wait");
        }
        p->need_wakeup = false;
    }
    pthread_mutex_unlock(&p->lock);
    return NULL;
}

static void destroy_no_thread(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;

    ao->driver->uninit(ao);

    for (int n = 0; n < 2; n++)
        close(p->wakeup_pipe[n]);

    pthread_cond_destroy(&p->wakeup);
    pthread_mutex_destroy(&p->lock);
}

static void uninit(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;

    pthread_mutex_lock(&p->lock);
    p->terminate = true;
    wakeup_playthread(ao);
    pthread_mutex_unlock(&p->lock);

    pthread_join(p->thread, NULL);

    destroy_no_thread(ao);
}

static int init(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;

    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->wakeup, NULL);
    mp_make_wakeup_pipe(p->wakeup_pipe);

    if (ao->device_buffer <= 0) {
        MP_FATAL(ao, "Couldn't probe device buffer size.\n");
        goto err;
    }

    p->buffer = mp_audio_buffer_create(ao);
    mp_audio_buffer_reinit_fmt(p->buffer, ao->format,
                               &ao->channels, ao->samplerate);
    mp_audio_buffer_preallocate_min(p->buffer, ao->buffer);
    if (pthread_create(&p->thread, NULL, playthread, ao))
        goto err;
    return 0;
err:
    destroy_no_thread(ao);
    return -1;
}

const struct ao_driver ao_api_push = {
    .init = init,
    .control = control,
    .uninit = uninit,
    .reset = reset,
    .get_space = get_space,
    .play = play,
    .get_delay = get_delay,
    .pause = audio_pause,
    .resume = resume,
    .drain = drain,
    .get_eof = get_eof,
    .priv_size = sizeof(struct ao_push_state),
};

// Must be called locked.
int ao_play_silence(struct ao *ao, int samples)
{
    assert(ao->api == &ao_api_push);
    if (samples <= 0 || AF_FORMAT_IS_SPECIAL(ao->format) || !ao->driver->play)
        return 0;
    char *p = talloc_size(NULL, samples * ao->sstride);
    af_fill_silence(p, samples * ao->sstride, ao->format);
    void *tmp[MP_NUM_CHANNELS];
    for (int n = 0; n < MP_NUM_CHANNELS; n++)
        tmp[n] = p;
    int r = ao->driver->play(ao, tmp, samples, 0);
    talloc_free(p);
    return r;
}

#ifndef __MINGW32__

#include <poll.h>

#define MAX_POLL_FDS 20

// Call poll() for the given fds. This will extend the given fds with the
// wakeup pipe, so ao_wakeup_poll() will basically interrupt this function.
// Unlocks the lock temporarily.
// Returns <0 on error, 0 on success, 1 if the caller should return immediately.
int ao_wait_poll(struct ao *ao, struct pollfd *fds, int num_fds,
                 pthread_mutex_t *lock)
{
    struct ao_push_state *p = ao->api_priv;
    assert(ao->api == &ao_api_push);
    assert(&p->lock == lock);

    if (num_fds >= MAX_POLL_FDS || p->wakeup_pipe[0] < 0)
        return -1;

    struct pollfd p_fds[MAX_POLL_FDS];
    memcpy(p_fds, fds, num_fds * sizeof(p_fds[0]));
    p_fds[num_fds] = (struct pollfd){
        .fd = p->wakeup_pipe[0],
        .events = POLLIN,
    };

    pthread_mutex_unlock(&p->lock);
    int r = poll(p_fds, num_fds + 1, -1);
    r = r < 0 ? -errno : 0;
    pthread_mutex_lock(&p->lock);

    memcpy(fds, p_fds, num_fds * sizeof(fds[0]));
    bool wakeup = false;
    if (p_fds[num_fds].revents & POLLIN) {
        wakeup = true;
        // flush the wakeup pipe contents - might "drown" some wakeups, but
        // that's ok for our use-case
        char buf[100];
        read(p->wakeup_pipe[0], buf, sizeof(buf));
    }
    return (r >= 0 || r == -EINTR) ? wakeup : -1;
}

void ao_wakeup_poll(struct ao *ao)
{
    assert(ao->api == &ao_api_push);
    struct ao_push_state *p = ao->api_priv;

    write(p->wakeup_pipe[1], &(char){0}, 1);
}

#endif
