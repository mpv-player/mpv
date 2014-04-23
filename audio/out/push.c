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
#include <limits.h>
#include <assert.h>

#include "ao.h"
#include "internal.h"
#include "audio/format.h"

#include "common/msg.h"
#include "common/common.h"

#include "input/input.h"

#include "osdep/threads.h"
#include "osdep/timer.h"
#include "compat/atomics.h"

#include "audio/audio.h"
#include "audio/audio_buffer.h"

struct ao_push_state {
    pthread_t thread;
    pthread_mutex_t lock;

    // uses a separate lock to avoid lock order issues with ao_need_data()
    pthread_mutex_t wakeup_lock;
    pthread_cond_t wakeup;

    // --- protected by lock

    struct mp_audio_buffer *buffer;

    bool terminate;
    bool playing;

    // Whether the current buffer contains the complete audio.
    bool final_chunk;
    double expected_end_time;

    // -- protected by wakeup_lock
    bool need_wakeup;
};

static void wakeup_playthread(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->wakeup_lock);
    p->need_wakeup = true;
    pthread_cond_signal(&p->wakeup);
    pthread_mutex_unlock(&p->wakeup_lock);
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

static float get_delay(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    double driver_delay = 0;
    if (ao->driver->get_delay)
        driver_delay = ao->driver->get_delay(ao);
    double delay = driver_delay + mp_audio_buffer_seconds(p->buffer);
    pthread_mutex_unlock(&p->lock);
    if (delay >= AO_EOF_DELAY && p->expected_end_time) {
        if (mp_time_sec() > p->expected_end_time) {
            MP_ERR(ao, "Audio device EOF reporting is broken!\n");
            MP_ERR(ao, "Please report this problem.\n");
            delay = 0;
        }
    }
    return delay;
}

static void reset(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    if (ao->driver->reset)
        ao->driver->reset(ao);
    mp_audio_buffer_clear(p->buffer);
    p->playing = false;
    wakeup_playthread(ao);
    pthread_mutex_unlock(&p->lock);
}

static void pause(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    if (ao->driver->pause)
        ao->driver->pause(ao);
    p->playing = false;
    wakeup_playthread(ao);
    pthread_mutex_unlock(&p->lock);
}

static void resume(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    if (ao->driver->resume)
        ao->driver->resume(ao);
    p->playing = true; // tentatively
    p->expected_end_time = 0;
    wakeup_playthread(ao);
    pthread_mutex_unlock(&p->lock);
}

static void drain(struct ao *ao)
{
    if (ao->driver->drain) {
        struct ao_push_state *p = ao->api_priv;
        pthread_mutex_lock(&p->lock);
        ao->driver->drain(ao);
        pthread_mutex_unlock(&p->lock);
    } else {
        ao_wait_drain(ao);
    }
}

static int get_space(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    pthread_mutex_lock(&p->lock);
    int space = mp_audio_buffer_get_write_available(p->buffer);
    if (ao->driver->get_space) {
        // The following code attempts to keep the total buffered audio to
        // MIN_BUFFER in order to improve latency.
        int device_space = ao->driver->get_space(ao);
        int device_buffered = ao->device_buffer - device_space;
        int soft_buffered = mp_audio_buffer_samples(p->buffer);
        int min_buffer = MIN_BUFFER * ao->samplerate;
        int missing = min_buffer - device_buffered - soft_buffered;
        // But always keep the device's buffer filled as much as we can.
        int device_missing = device_space - soft_buffered;
        missing = MPMAX(missing, device_missing);
        space = MPMIN(space, missing);
        space = MPMAX(0, space);
    }
    pthread_mutex_unlock(&p->lock);
    return space;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct ao_push_state *p = ao->api_priv;

    pthread_mutex_lock(&p->lock);

    int write_samples = mp_audio_buffer_get_write_available(p->buffer);
    write_samples = MPMIN(write_samples, samples);

    struct mp_audio audio;
    mp_audio_buffer_get_format(p->buffer, &audio);
    for (int n = 0; n < ao->num_planes; n++)
        audio.planes[n] = data[n];
    audio.samples = write_samples;
    mp_audio_buffer_append(p->buffer, &audio);

    p->final_chunk = !!(flags & AOPLAY_FINAL_CHUNK);
    p->playing = true;
    p->expected_end_time = 0;

    wakeup_playthread(ao);
    pthread_mutex_unlock(&p->lock);
    return write_samples;
}

// called locked
static int ao_play_data(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;
    struct mp_audio data;
    mp_audio_buffer_peek(p->buffer, &data);
    int max = data.samples;
    int space = ao->driver->get_space ? ao->driver->get_space(ao) : INT_MAX;
    if (data.samples > space)
        data.samples = space;
    if (data.samples <= 0)
        return 0;
    int flags = 0;
    if (p->final_chunk && data.samples == max)
        flags |= AOPLAY_FINAL_CHUNK;
    int r = ao->driver->play(ao, data.planes, data.samples, flags);
    if (r > data.samples) {
        MP_WARN(ao, "Audio device returned non-sense value.");
        r = data.samples;
    }
    if (r > 0)
        mp_audio_buffer_skip(p->buffer, r);
    if (p->final_chunk && mp_audio_buffer_samples(p->buffer) == 0) {
        p->playing = false;
        p->expected_end_time = mp_time_sec() + AO_EOF_DELAY + 0.25; // + margin
        if (ao->driver->get_delay)
            p->expected_end_time += ao->driver->get_delay(ao);
    }
    return r;
}

static void *playthread(void *arg)
{
    struct ao *ao = arg;
    struct ao_push_state *p = ao->api_priv;
    while (1) {
        pthread_mutex_lock(&p->lock);
        if (p->terminate) {
            pthread_mutex_unlock(&p->lock);
            return NULL;
        }
        double timeout = 2.0;
        if (p->playing) {
            double min_wait = ao->device_buffer / (double)ao->samplerate;
            min_wait *= 0.75;
            int r = ao_play_data(ao);
            // The device buffers are not necessarily full, but writing to the
            // AO buffer will wake up this thread anyway.
            bool buffers_full = r <= 0;
            // We have to estimate when the AO needs data again.
            if (buffers_full && ao->driver->get_delay) {
                float buffered_audio = ao->driver->get_delay(ao);
                timeout = buffered_audio - 0.050;
                // Keep extra safety margin if the buffers are large
                if (timeout > 0.100)
                    timeout = MPMAX(timeout - 0.200, 0.100);
            } else {
                timeout = 0;
            }
            // Half of the buffer played -> wakeup playback thread to get more.
            if (timeout <= min_wait / 2 + 0.001)
                mp_input_wakeup(ao->input_ctx);
            // Avoid wasting CPU - this assumes ao_play_data() usually fills the
            // audio buffer as far as possible, so even if the device buffer
            // is not full, we can only wait for the core.
            timeout = MPMAX(timeout, min_wait);
        }
        pthread_mutex_unlock(&p->lock);
        pthread_mutex_lock(&p->wakeup_lock);
        if (!p->need_wakeup)
            mpthread_cond_timedwait(&p->wakeup, &p->wakeup_lock, timeout);
        p->need_wakeup = false;
        pthread_mutex_unlock(&p->wakeup_lock);
    }
    return NULL;
}

static void uninit(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;

    pthread_mutex_lock(&p->lock);
    p->terminate = true;
    wakeup_playthread(ao);
    pthread_mutex_unlock(&p->lock);

    pthread_join(p->thread, NULL);

    ao->driver->uninit(ao);

    pthread_cond_destroy(&p->wakeup);
    pthread_mutex_destroy(&p->lock);
    pthread_mutex_destroy(&p->wakeup_lock);
}

static int init(struct ao *ao)
{
    struct ao_push_state *p = ao->api_priv;

    pthread_mutex_init(&p->lock, NULL);
    pthread_mutex_init(&p->wakeup_lock, NULL);
    pthread_cond_init(&p->wakeup, NULL);

    p->buffer = mp_audio_buffer_create(ao);
    mp_audio_buffer_reinit_fmt(p->buffer, ao->format,
                               &ao->channels, ao->samplerate);
    mp_audio_buffer_preallocate_min(p->buffer, ao->buffer);
    if (pthread_create(&p->thread, NULL, playthread, ao)) {
        ao->driver->uninit(ao);
        return -1;
    }
    return 0;
}

const struct ao_driver ao_api_push = {
    .init = init,
    .control = control,
    .uninit = uninit,
    .reset = reset,
    .get_space = get_space,
    .play = play,
    .get_delay = get_delay,
    .pause = pause,
    .resume = resume,
    .drain = drain,
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

// Notify the core that new data should be sent to the AO. Normally, the core
// uses a heuristic based on ao_delay() when to refill the buffers, but this
// can be used to reduce wait times. Can be called from any thread.
void ao_need_data(struct ao *ao)
{
    assert(ao->api == &ao_api_push);

    // wakeup the play thread at least once
    wakeup_playthread(ao);
}
