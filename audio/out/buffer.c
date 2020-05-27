/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <pthread.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "ao.h"
#include "internal.h"
#include "audio/aframe.h"
#include "audio/format.h"

#include "common/msg.h"
#include "common/common.h"

#include "input/input.h"

#include "osdep/io.h"
#include "osdep/timer.h"
#include "osdep/threads.h"
#include "osdep/atomic.h"
#include "misc/ring.h"

struct buffer_state {
    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // Access from AO driver's thread only.
    char *convert_buffer;

    // --- protected by lock

    struct mp_ring *buffers[MP_NUM_CHANNELS];


    bool streaming;             // AO streaming active
    bool playing;               // logically playing audio from buffer
    bool paused;                // logically paused; implies playing=true
    bool final_chunk;           // if buffer contains EOF

    int64_t end_time_us;        // absolute output time of last played sample
    int64_t underflow;          // number of samples missing since last check

    bool need_wakeup;
    bool initial_unblocked;

    // "Push" AOs only (AOs with driver->play).
    bool still_playing;
    double expected_end_time;
    bool wait_on_ao;
    pthread_t thread;           // thread shoveling data to AO
    bool thread_valid;          // thread is running
    bool terminate;             // exit thread
    struct mp_aframe *temp_buf;

    int wakeup_pipe[2];
};

static void *playthread(void *arg);

// lock must be held
static void wakeup_playthread(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    if (ao->driver->wakeup)
        ao->driver->wakeup(ao);
    p->need_wakeup = true;
    pthread_cond_signal(&p->wakeup);
}

static int unlocked_get_space(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    int space = mp_ring_available(p->buffers[0]) / ao->sstride;

    // The following code attempts to keep the total buffered audio at
    // ao->buffer in order to improve latency.
    if (ao->driver->play && ao->driver->get_space) {
        int align = af_format_sample_alignment(ao->format);
        int device_space = ao->driver->get_space(ao);
        int device_buffered = ao->device_buffer - device_space;
        int soft_buffered = mp_ring_size(p->buffers[0]) / ao->sstride - space;
        // The extra margin helps avoiding too many wakeups if the AO is fully
        // byte based and doesn't do proper chunked processing.
        int min_buffer = ao->buffer + 64;
        int missing = min_buffer - device_buffered - soft_buffered;
        missing = (missing + align - 1) / align * align;
        // But always keep the device's buffer filled as much as we can.
        int device_missing = device_space - soft_buffered;
        missing = MPMAX(missing, device_missing);
        space = MPMIN(space, missing);
        space = MPMAX(0, space);
    }

    return space;
}

// Return free size of the internal audio buffer. This controls how much audio
// the core should decode and try to queue with ao_play().
int ao_get_space(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    pthread_mutex_lock(&p->lock);
    int space = unlocked_get_space(ao);
    pthread_mutex_unlock(&p->lock);
    return space;
}

// Queue the given audio data. Start playback if it hasn't started yet. Return
// the number of samples that was accepted (the core will try to queue the rest
// again later). Should never block.
//  data: start pointer for each plane. If the audio data is packed, only
//        data[0] is valid, otherwise there is a plane for each channel.
//  samples: size of the audio data (see ao->sstride)
//  flags: currently AOPLAY_FINAL_CHUNK can be set
int ao_play(struct ao *ao, void **data, int samples, int flags)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);

    int write_samples = mp_ring_available(p->buffers[0]) / ao->sstride;
    write_samples = MPMIN(write_samples, samples);

    int write_bytes = write_samples * ao->sstride;
    for (int n = 0; n < ao->num_planes; n++) {
        int r = mp_ring_write(p->buffers[n], data[n], write_bytes);
        assert(r == write_bytes);
    }

    p->paused = false;
    p->final_chunk = write_samples == samples && (flags & AOPLAY_FINAL_CHUNK);

    if (p->underflow)
        MP_DBG(ao, "Audio underrun by %lld samples.\n", (long long)p->underflow);
    p->underflow = 0;

    if (write_samples) {
        p->playing = true;
        p->still_playing = true;
        p->expected_end_time = 0;

        if (!ao->driver->play && !p->streaming) {
            p->streaming = true;
            ao->driver->resume(ao);
        }

        wakeup_playthread(ao);
    }
    pthread_mutex_unlock(&p->lock);

    return write_samples;
}

// Read the given amount of samples in the user-provided data buffer. Returns
// the number of samples copied. If there is not enough data (buffer underrun
// or EOF), return the number of samples that could be copied, and fill the
// rest of the user-provided buffer with silence.
// This basically assumes that the audio device doesn't care about underruns.
// If this is called in paused mode, it will always return 0.
// The caller should set out_time_us to the expected delay until the last sample
// reaches the speakers, in microseconds, using mp_time_us() as reference.
int ao_read_data(struct ao *ao, void **data, int samples, int64_t out_time_us)
{
    struct buffer_state *p = ao->buffer_state;
    int full_bytes = samples * ao->sstride;
    bool need_wakeup = false;
    int bytes = 0;

    pthread_mutex_lock(&p->lock);

    if (!p->playing || p->paused)
        goto end;

    int buffered_bytes = mp_ring_buffered(p->buffers[0]);
    bytes = MPMIN(buffered_bytes, full_bytes);

    if (full_bytes > bytes && !p->final_chunk) {
        p->underflow += (full_bytes - bytes) / ao->sstride;
        ao_underrun_event(ao);
    }

    if (bytes > 0)
        p->end_time_us = out_time_us;

    for (int n = 0; n < ao->num_planes; n++)
        mp_ring_read(p->buffers[n], data[n], bytes);

    // Half of the buffer played -> request more.
    need_wakeup = buffered_bytes - bytes <= mp_ring_size(p->buffers[0]) / 2;

end:

    pthread_mutex_unlock(&p->lock);

    if (need_wakeup)
        ao->wakeup_cb(ao->wakeup_ctx);

    // pad with silence (underflow/paused/eof)
    for (int n = 0; n < ao->num_planes; n++)
        af_fill_silence((char *)data[n] + bytes, full_bytes - bytes, ao->format);

    ao_post_process_data(ao, data, samples);

    return bytes / ao->sstride;
}

// Same as ao_read_data(), but convert data according to *fmt.
// fmt->src_fmt and fmt->channels must be the same as the AO parameters.
int ao_read_data_converted(struct ao *ao, struct ao_convert_fmt *fmt,
                           void **data, int samples, int64_t out_time_us)
{
    struct buffer_state *p = ao->buffer_state;
    void *ndata[MP_NUM_CHANNELS] = {0};

    if (!ao_need_conversion(fmt))
        return ao_read_data(ao, data, samples, out_time_us);

    assert(ao->format == fmt->src_fmt);
    assert(ao->channels.num == fmt->channels);

    bool planar = af_fmt_is_planar(fmt->src_fmt);
    int planes = planar ? fmt->channels : 1;
    int plane_samples = samples * (planar ? 1: fmt->channels);
    int src_plane_size = plane_samples * af_fmt_to_bytes(fmt->src_fmt);
    int dst_plane_size = plane_samples * fmt->dst_bits / 8;

    int needed = src_plane_size * planes;
    if (needed > talloc_get_size(p->convert_buffer) || !p->convert_buffer) {
        talloc_free(p->convert_buffer);
        p->convert_buffer = talloc_size(NULL, needed);
    }

    for (int n = 0; n < planes; n++)
        ndata[n] = p->convert_buffer + n * src_plane_size;

    int res = ao_read_data(ao, ndata, samples, out_time_us);

    ao_convert_inplace(fmt, ndata, samples);
    for (int n = 0; n < planes; n++)
        memcpy(data[n], ndata[n], dst_plane_size);

    return res;
}

int ao_control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct buffer_state *p = ao->buffer_state;
    int r = CONTROL_UNKNOWN;
    if (ao->driver->control) {
        pthread_mutex_lock(&p->lock);
        r = ao->driver->control(ao, cmd, arg);
        pthread_mutex_unlock(&p->lock);
    }
    return r;
}

static double unlocked_get_delay(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    double driver_delay = 0;

    if (ao->driver->get_delay)
        driver_delay = ao->driver->get_delay(ao);

    if (!ao->driver->play) {
        int64_t end = p->end_time_us;
        int64_t now = mp_time_us();
        driver_delay += MPMAX(0, (end - now) / (1000.0 * 1000.0));
    }

    return mp_ring_buffered(p->buffers[0]) / (double)ao->bps + driver_delay;
}

// Return size of the buffered data in seconds. Can include the device latency.
// Basically, this returns how much data there is still to play, and how long
// it takes until the last sample in the buffer reaches the speakers. This is
// used for audio/video synchronization, so it's very important to implement
// this correctly.
double ao_get_delay(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);
    double delay = unlocked_get_delay(ao);
    pthread_mutex_unlock(&p->lock);
    return delay;
}

// Stop playback and empty buffers. Essentially go back to the state after
// ao->init().
void ao_reset(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);

    for (int n = 0; n < ao->num_planes; n++)
        mp_ring_reset(p->buffers[n]);

    if (!ao->stream_silence && ao->driver->reset) {
        ao->driver->reset(ao); // assumes the audio callback thread is stopped
        p->streaming = false;
    }
    p->paused = false;
    p->playing = false;
    if (p->still_playing)
        wakeup_playthread(ao);
    p->still_playing = false;
    p->end_time_us = 0;

    atomic_fetch_and(&ao->events_, ~(unsigned int)AO_EVENT_UNDERRUN);

    pthread_mutex_unlock(&p->lock);
}

// Pause playback. Keep the current buffer. ao_get_delay() must return the
// same value as before pausing.
void ao_pause(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);

    if (p->playing && !p->paused) {
        if (p->streaming && !ao->stream_silence) {
            if (ao->driver->pause) {
                ao->driver->pause(ao);
            } else if (ao->driver->reset) {
                ao->driver->reset(ao);
                p->streaming = false;
            }
        }
        p->paused = true;
        wakeup_playthread(ao);
    }

    pthread_mutex_unlock(&p->lock);
}

// Resume playback. Play the remaining buffer. If the driver doesn't support
// pausing, it has to work around this and e.g. use ao_play_silence() to fill
// the lost audio.
void ao_resume(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);

    if (p->playing && p->paused) {
        if (p->streaming && ao->driver->resume)
            ao->driver->resume(ao);
        p->paused = false;
        p->expected_end_time = 0;
        wakeup_playthread(ao);
    }

    pthread_mutex_unlock(&p->lock);
}

bool ao_eof_reached(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);
    bool eof = !p->playing;
    if (ao->driver->play) {
        eof |= !p->still_playing;
    } else {
        // For simplicity, ignore the latency. Otherwise, we would have to run
        // an extra thread to time it.
        eof |= mp_ring_buffered(p->buffers[0]) == 0;
    }
    pthread_mutex_unlock(&p->lock);

    return eof;
}

// Block until the current audio buffer has played completely.
void ao_drain(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);
    p->final_chunk = true;
    wakeup_playthread(ao);
    double left = 0;
    if (p->playing && !p->paused && !ao->driver->encode)
        left = mp_ring_buffered(p->buffers[0]) / (double)ao->bps * 1e6;
    pthread_mutex_unlock(&p->lock);

    if (left > 0) {
        // Wait for lower bound.
        mp_sleep_us(left);
        // And then poll for actual end. (Unfortunately, this code considers
        // audio APIs which do not want you to use mutexes in the audio
        // callback, and an extra semaphore would require slightly more effort.)
        // Limit to arbitrary ~250ms max. waiting for robustness.
        int64_t max = mp_time_us() + 250000;
        while (mp_time_us() < max && !ao_eof_reached(ao))
            mp_sleep_us(1);
    }

    ao_reset(ao);
}

// Uninitialize and destroy the AO. Remaining audio must be dropped.
void ao_uninit(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    if (p->thread_valid) {
        pthread_mutex_lock(&p->lock);
        p->terminate = true;
        wakeup_playthread(ao);
        pthread_mutex_unlock(&p->lock);

        pthread_join(p->thread, NULL);
        p->thread_valid = false;
    }

    if (ao->driver_initialized)
        ao->driver->uninit(ao);

    talloc_free(p->convert_buffer);
    talloc_free(p->temp_buf);

    for (int n = 0; n < 2; n++) {
        int h = p->wakeup_pipe[n];
        if (h >= 0)
            close(h);
    }

    pthread_cond_destroy(&p->wakeup);
    pthread_mutex_destroy(&p->lock);

    talloc_free(ao);
}

void init_buffer_pre(struct ao *ao)
{
    ao->buffer_state = talloc_zero(ao, struct buffer_state);
}

bool init_buffer_post(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    if (!ao->driver->play)
        assert(ao->driver->resume);

    for (int n = 0; n < ao->num_planes; n++)
        p->buffers[n] = mp_ring_new(ao, ao->buffer * ao->sstride);

    mpthread_mutex_init_recursive(&p->lock);
    pthread_cond_init(&p->wakeup, NULL);
    mp_make_wakeup_pipe(p->wakeup_pipe);

    if (ao->driver->play) {
        if (ao->device_buffer <= 0) {
            MP_FATAL(ao, "Couldn't probe device buffer size.\n");
            return false;
        }

        p->thread_valid = true;
        if (pthread_create(&p->thread, NULL, playthread, ao)) {
            p->thread_valid = false;
            return false;
        }
    } else {
        if (ao->stream_silence) {
            ao->driver->resume(ao);
            p->streaming = true;
        }
    }

    return true;
}

static bool realloc_buf(struct ao *ao, int samples)
{
    struct buffer_state *p = ao->buffer_state;

    samples = MPMAX(1, samples);

    if (!p->temp_buf || samples > mp_aframe_get_size(p->temp_buf)) {
        TA_FREEP(&p->temp_buf);
        p->temp_buf = mp_aframe_create();
        if (!mp_aframe_set_format(p->temp_buf, ao->format) ||
            !mp_aframe_set_chmap(p->temp_buf, &ao->channels) ||
            !mp_aframe_set_rate(p->temp_buf, ao->samplerate) ||
            !mp_aframe_alloc_data(p->temp_buf, samples))
        {
            TA_FREEP(&p->temp_buf);
            return false;
        }
    }

    return true;
}

// called locked
static void ao_play_data(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    int space = ao->driver->get_space(ao);
    bool play_silence = p->paused || (ao->stream_silence && !p->still_playing);
    space = MPMAX(space, 0);
    // Most AOs want period-size aligned audio, and preferably as much as
    // possible in one go, so the audio data is "linearized" into this buffer.
    if (space % ao->period_size)
        MP_ERR(ao, "Audio device reports unaligned available buffer size.\n");
    if (!realloc_buf(ao, space)) {
        MP_ERR(ao, "Failed to allocate buffer.\n");
        return;
    }
    void **planes = (void **)mp_aframe_get_data_rw(p->temp_buf);
    assert(planes);
    int samples = mp_ring_buffered(p->buffers[0]) / ao->sstride;
    if (samples > space)
        samples = space;
    if (play_silence)
        samples = space;
    samples = ao_read_data(ao, planes, samples, 0);
    if (play_silence)
        samples = space; // ao_read_data() sets remainder to silent
    int max = samples;
    int flags = 0;
    if (p->final_chunk && samples < space) {
        flags |= AOPLAY_FINAL_CHUNK;
    } else {
        samples = samples / ao->period_size * ao->period_size;
    }
    MP_STATS(ao, "start ao fill");
    int r = 0;
    if (samples)
        r = ao->driver->play(ao, planes, samples, flags);
    MP_STATS(ao, "end ao fill");
    if (r > samples) {
        MP_ERR(ao, "Audio device returned nonsense value.\n");
        r = samples;
    } else if (r < 0) {
        MP_ERR(ao, "Error writing audio to device.\n");
    } else if (r != samples) {
        MP_ERR(ao, "Audio device returned broken buffer state (sent %d samples, "
               "got %d samples, %d period%s)! Discarding audio.\n", samples, r,
               ao->period_size, flags & AOPLAY_FINAL_CHUNK ? " final" : "");
    }
    r = MPMAX(r, 0);
    // Probably can't copy the rest of the buffer due to period alignment.
    bool stuck_eof = r <= 0 && space >= max && samples > 0;
    if ((flags & AOPLAY_FINAL_CHUNK) && stuck_eof) {
        MP_ERR(ao, "Audio output driver seems to ignore AOPLAY_FINAL_CHUNK.\n");
        r = max;
    }
    if (r > 0) {
        p->expected_end_time = 0;
        p->streaming = true;
    }
    // Nothing written, but more input data than space - this must mean the
    // AO's get_space() doesn't do period alignment correctly.
    bool stuck = r == 0 && max >= space && space > 0;
    if (stuck)
        MP_ERR(ao, "Audio output is reporting incorrect buffer status.\n");
    // Wait until space becomes available. Also wait if we actually wrote data,
    // so the AO wakes us up properly if it needs more data.
    p->wait_on_ao = space == 0 || r > 0 || stuck;
    p->still_playing |= r > 0 && !play_silence;
    // If we just filled the AO completely (r == space), don't refill for a
    // while. Prevents wakeup feedback with byte-granular AOs.
    int needed = unlocked_get_space(ao);
    bool more = needed >= (r == space ? ao->device_buffer / 4 : 1) && !stuck &&
                !(flags & AOPLAY_FINAL_CHUNK);
    if (more)
        ao->wakeup_cb(ao->wakeup_ctx); // request more data
    if (!samples && space && !ao->driver->reports_underruns && p->still_playing)
        ao_underrun_event(ao);
    MP_TRACE(ao, "in=%d flags=%d space=%d r=%d wa/pl=%d/%d needed=%d more=%d\n",
             max, flags, space, r, p->wait_on_ao, p->still_playing, needed, more);
}

static void *playthread(void *arg)
{
    struct ao *ao = arg;
    struct buffer_state *p = ao->buffer_state;
    mpthread_set_name("ao");
    pthread_mutex_lock(&p->lock);
    while (!p->terminate) {
        bool blocked = ao->driver->initially_blocked && !p->initial_unblocked;
        bool playing = (!p->paused || ao->stream_silence) && !blocked;
        if (playing)
            ao_play_data(ao);

        if (!p->need_wakeup) {
            MP_STATS(ao, "start audio wait");
            if (!p->wait_on_ao || !playing) {
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
                    !mp_ring_buffered(p->buffers[0]))
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
                    ao->wakeup_cb(ao->wakeup_ctx);
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

void ao_unblock(struct ao *ao)
{
    if (ao->driver->play) {
        struct buffer_state *p = ao->buffer_state;
        pthread_mutex_lock(&p->lock);
        p->need_wakeup = true;
        p->initial_unblocked = true;
        wakeup_playthread(ao);
        pthread_cond_signal(&p->wakeup);
        pthread_mutex_unlock(&p->lock);
    }
}

// Must be called locked.
int ao_play_silence(struct ao *ao, int samples)
{
    assert(ao->driver->play);

    struct buffer_state *p = ao->buffer_state;

    if (!realloc_buf(ao, samples) || !ao->driver->play)
        return 0;

    void **planes = (void **)mp_aframe_get_data_rw(p->temp_buf);
    assert(planes);

    for (int n = 0; n < ao->num_planes; n++)
        af_fill_silence(planes[n], ao->sstride * samples, ao->format);

    return ao->driver->play(ao, planes, samples, 0);
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
    struct buffer_state *p = ao->buffer_state;
    assert(ao->driver->play);
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
        // might "drown" some wakeups, but that's ok for our use-case
        mp_flush_wakeup_pipe(p->wakeup_pipe[0]);
    }
    return (r >= 0 || r == -EINTR) ? wakeup : -1;
}

void ao_wakeup_poll(struct ao *ao)
{
    assert(ao->driver->play);
    struct buffer_state *p = ao->buffer_state;

    (void)write(p->wakeup_pipe[1], &(char){0}, 1);
}

#endif
