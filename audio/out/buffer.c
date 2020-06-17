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
#include <math.h>
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
    // Buffer and AO
    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // Playthread sleep
    pthread_mutex_t pt_lock;
    pthread_cond_t pt_wakeup;

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

    bool initial_unblocked;

    // "Push" AOs only (AOs with driver->write).
    bool still_playing;
    bool hw_paused;             // driver->set_pause() was used successfully
    bool recover_pause;         // non-hw_paused: needs to recover delay
    bool draining;
    bool ao_wait_low_buffer;
    struct mp_pcm_state prepause_state;
    pthread_t thread;           // thread shoveling data to AO
    bool thread_valid;          // thread is running
    struct mp_aframe *temp_buf;

    // --- protected by pt_lock
    bool need_wakeup;
    bool terminate;             // exit thread
};

static void *playthread(void *arg);

void ao_wakeup_playthread(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    pthread_mutex_lock(&p->pt_lock);
    p->need_wakeup = true;
    pthread_cond_broadcast(&p->pt_wakeup);
    pthread_mutex_unlock(&p->pt_lock);
}

// called locked
static void get_dev_state(struct ao *ao, struct mp_pcm_state *state)
{
    struct buffer_state *p = ao->buffer_state;

    if (p->paused) {
        *state = p->prepause_state;
        return;
    }

    *state = (struct mp_pcm_state){
        .free_samples = -1,
        .queued_samples = -1,
        .delay = -1,
    };
    ao->driver->get_state(ao, state);
}

static int unlocked_get_space(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    int space = mp_ring_available(p->buffers[0]) / ao->sstride;

    // The following code attempts to keep the total buffered audio at
    // ao->buffer in order to improve latency.
    if (ao->driver->write) {
        struct mp_pcm_state state;
        get_dev_state(ao, &state);
        int align = af_format_sample_alignment(ao->format);
        int device_space = MPMAX(state.free_samples, 0);
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

int ao_get_space(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    pthread_mutex_lock(&p->lock);
    int space = unlocked_get_space(ao);
    pthread_mutex_unlock(&p->lock);
    return space;
}

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
    p->final_chunk = write_samples == samples && (flags & PLAYER_FINAL_CHUNK);

    if (p->underflow)
        MP_DBG(ao, "Audio underrun by %lld samples.\n", (long long)p->underflow);
    p->underflow = 0;

    if (write_samples) {
        p->playing = true;
        p->still_playing = true;
        p->draining = false;

        if (!ao->driver->write && !p->streaming) {
            p->streaming = true;
            ao->driver->start(ao);
        }

    }
    pthread_mutex_unlock(&p->lock);

    if (write_samples)
        ao_wakeup_playthread(ao);

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
        ao_add_events(ao, AO_EVENT_UNDERRUN);
    }

    if (bytes > 0)
        p->end_time_us = out_time_us;

    for (int n = 0; n < ao->num_planes; n++)
        mp_ring_read(p->buffers[n], data[n], bytes);

    // Half of the buffer played -> request more.
    if (!ao->driver->write)
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
        // Only need to lock in push mode.
        if (ao->driver->write)
            pthread_mutex_lock(&p->lock);

        r = ao->driver->control(ao, cmd, arg);

        if (ao->driver->write)
            pthread_mutex_unlock(&p->lock);
    }
    return r;
}

static double unlocked_get_delay(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    double driver_delay = 0;

    if (ao->driver->write) {
        struct mp_pcm_state state;
        get_dev_state(ao, &state);
        driver_delay = state.delay;
    } else {
        int64_t end = p->end_time_us;
        int64_t now = mp_time_us();
        driver_delay += MPMAX(0, (end - now) / (1000.0 * 1000.0));
    }

    return mp_ring_buffered(p->buffers[0]) / (double)ao->bps + driver_delay;
}

double ao_get_delay(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);
    double delay = unlocked_get_delay(ao);
    pthread_mutex_unlock(&p->lock);
    return delay;
}

void ao_reset(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    bool wakeup = false;
    bool do_reset = false;

    pthread_mutex_lock(&p->lock);

    for (int n = 0; n < ao->num_planes; n++)
        mp_ring_reset(p->buffers[n]);

    if (!ao->stream_silence && ao->driver->reset) {
        if (ao->driver->write) {
            ao->driver->reset(ao);
        } else {
            // Pull AOs may wait for ao_read_data() to return.
            // That would deadlock if called from within the lock.
            do_reset = true;
        }
        p->streaming = false;
    }
    p->paused = false;
    p->playing = false;
    p->recover_pause = false;
    p->hw_paused = false;
    wakeup = p->still_playing || p->draining;
    p->draining = false;
    p->still_playing = false;
    p->end_time_us = 0;

    atomic_fetch_and(&ao->events_, ~(unsigned int)AO_EVENT_UNDERRUN);

    pthread_mutex_unlock(&p->lock);

    if (do_reset)
        ao->driver->reset(ao);

    if (wakeup)
        ao_wakeup_playthread(ao);
}

void ao_pause(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    bool wakeup = false;
    bool do_reset = false;

    pthread_mutex_lock(&p->lock);

    if (p->playing && !p->paused) {
        if (p->streaming && !ao->stream_silence) {
            if (ao->driver->write) {
                if (!p->recover_pause)
                    get_dev_state(ao, &p->prepause_state);
                if (ao->driver->set_pause && ao->driver->set_pause(ao, true)) {
                    p->hw_paused = true;
                } else {
                    ao->driver->reset(ao);
                    p->streaming = false;
                }
            } else if (ao->driver->reset) {
                // See ao_reset() why this is done outside of the lock.
                do_reset = true;
                p->streaming = false;
            }
        }
        p->paused = true;
        wakeup = true;
    }

    pthread_mutex_unlock(&p->lock);

    if (do_reset)
        ao->driver->reset(ao);

    if (wakeup)
        ao_wakeup_playthread(ao);
}

void ao_resume(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    bool wakeup = false;

    pthread_mutex_lock(&p->lock);

    if (p->playing && p->paused) {
        if (ao->driver->write) {
            if (p->streaming && p->hw_paused) {
                ao->driver->set_pause(ao, false);
            } else {
                p->recover_pause = true;
            }
            p->hw_paused = false;
        } else {
            if (!p->streaming)
                ao->driver->start(ao);
            p->streaming = true;
        }
        p->paused = false;
        wakeup = true;
    }

    pthread_mutex_unlock(&p->lock);

    if (wakeup)
        ao_wakeup_playthread(ao);
}

bool ao_eof_reached(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);
    bool eof = !p->playing;
    if (ao->driver->write) {
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
    while (!p->paused && p->still_playing && p->streaming) {
        if (ao->driver->write) {
            if (p->draining) {
                // Wait for EOF signal from AO.
                pthread_cond_wait(&p->wakeup, &p->lock);
            } else {
                p->draining = true;
                MP_VERBOSE(ao, "waiting for draining...\n");
                pthread_mutex_unlock(&p->lock);
                ao_wakeup_playthread(ao);
                pthread_mutex_lock(&p->lock);
            }
        } else {
            double left = mp_ring_buffered(p->buffers[0]) / (double)ao->bps * 1e6;
            pthread_mutex_unlock(&p->lock);

            if (left > 0) {
                // Wait for lower bound.
                mp_sleep_us(left);
                // And then poll for actual end. No other way.
                // Limit to arbitrary ~250ms max. waiting for robustness.
                int64_t max = mp_time_us() + 250000;
                while (mp_time_us() < max && !ao_eof_reached(ao))
                    mp_sleep_us(1);
            } else {
                p->still_playing = false;
            }

            pthread_mutex_lock(&p->lock);
        }
    }
    pthread_mutex_unlock(&p->lock);

    ao_reset(ao);
}

void ao_uninit(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    if (p->thread_valid) {
        pthread_mutex_lock(&p->pt_lock);
        p->terminate = true;
        pthread_cond_broadcast(&p->pt_wakeup);
        pthread_mutex_unlock(&p->pt_lock);

        pthread_join(p->thread, NULL);
        p->thread_valid = false;
    }

    if (ao->driver_initialized)
        ao->driver->uninit(ao);

    talloc_free(p->convert_buffer);
    talloc_free(p->temp_buf);

    pthread_cond_destroy(&p->wakeup);
    pthread_mutex_destroy(&p->lock);

    pthread_cond_destroy(&p->pt_wakeup);
    pthread_mutex_destroy(&p->pt_lock);

    talloc_free(ao);
}

void init_buffer_pre(struct ao *ao)
{
    ao->buffer_state = talloc_zero(ao, struct buffer_state);
}

bool init_buffer_post(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    assert(ao->driver->start);
    if (ao->driver->write) {
        assert(ao->driver->reset);
        assert(ao->driver->get_state);
    }

    for (int n = 0; n < ao->num_planes; n++)
        p->buffers[n] = mp_ring_new(ao, ao->buffer * ao->sstride);

    mpthread_mutex_init_recursive(&p->lock);
    pthread_cond_init(&p->wakeup, NULL);

    pthread_mutex_init(&p->pt_lock, NULL);
    pthread_cond_init(&p->pt_wakeup, NULL);

    if (ao->driver->write) {
        p->thread_valid = true;
        if (pthread_create(&p->thread, NULL, playthread, ao)) {
            p->thread_valid = false;
            return false;
        }
    } else {
        if (ao->stream_silence) {
            ao->driver->start(ao);
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
    struct mp_pcm_state state;
    get_dev_state(ao, &state);

    if (p->streaming && !state.playing && !ao->untimed) {
        if (p->draining) {
            MP_VERBOSE(ao, "underrun signaled for audio end\n");
            p->still_playing = false;
            pthread_cond_broadcast(&p->wakeup);
        } else {
            ao_add_events(ao, AO_EVENT_UNDERRUN);
        }

        p->streaming = false;
    }

    // Round free space to period sizes to reduce number of write() calls.
    int space = state.free_samples / ao->period_size * ao->period_size;
    bool play_silence = p->paused || (ao->stream_silence && !p->still_playing);
    space = MPMAX(space, 0);
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
    if (p->recover_pause) {
        samples = MPCLAMP(p->prepause_state.delay * ao->samplerate, 0, space);
        p->recover_pause = false;
        mp_aframe_set_silence(p->temp_buf, 0, space);
    } else {
        samples = ao_read_data(ao, planes, samples, 0);
    }
    if (play_silence)
        samples = space; // ao_read_data() sets remainder to silent

    bool is_eof = p->final_chunk && samples < space;
    bool ok = true;
    int written = 0;
    if (samples) {
        p->draining |= is_eof;
        MP_STATS(ao, "start ao fill");
        ok = ao->driver->write(ao, planes, samples);
        MP_STATS(ao, "end ao fill");
    }

    if (!ok)
        MP_ERR(ao, "Error writing audio to device.\n");

    if (samples > 0 && ok) {
        written = samples;
        if (!p->streaming) {
            MP_VERBOSE(ao, "starting AO\n");
            ao->driver->start(ao);
            p->streaming = true;
        }
        p->still_playing = !play_silence;
    }

    if (p->draining && p->still_playing && ao->untimed) {
        p->still_playing = false;
        pthread_cond_broadcast(&p->wakeup);
    }

    // Wait until space becomes available. Also wait if we actually wrote data,
    // so the AO wakes us up properly if it needs more data.
    p->ao_wait_low_buffer = space == 0 || written > 0 || p->draining;

    // Request more data if we're below some random buffer level.
    int needed = unlocked_get_space(ao);
    bool more = needed >= ao->device_buffer / 4 && !p->final_chunk;
    if (more)
        ao->wakeup_cb(ao->wakeup_ctx); // request more data
    MP_TRACE(ao, "in=%d eof=%d space=%d r=%d wa/pl/dr=%d/%d/%d needed=%d more=%d\n",
             samples, is_eof, space, written, p->ao_wait_low_buffer,
             p->still_playing, p->draining, needed, more);
}

static void *playthread(void *arg)
{
    struct ao *ao = arg;
    struct buffer_state *p = ao->buffer_state;
    mpthread_set_name("ao");
    while (1) {
        pthread_mutex_lock(&p->lock);

        bool blocked = ao->driver->initially_blocked && !p->initial_unblocked;
        bool playing = !p->paused && (p->playing || ao->stream_silence);
        if (playing && !blocked)
            ao_play_data(ao);

        // Wait until the device wants us to write more data to it.
        // Fallback to guessing.
        double timeout = INFINITY;
        if (p->ao_wait_low_buffer) {
            // Wake up again if half of the audio buffer has been played.
            // Since audio could play at a faster or slower pace, wake up twice
            // as often as ideally needed.
            timeout = ao->device_buffer / (double)ao->samplerate * 0.25;
            p->ao_wait_low_buffer = false;
        }

        pthread_mutex_unlock(&p->lock);

        pthread_mutex_lock(&p->pt_lock);
        if (p->terminate) {
            pthread_mutex_unlock(&p->pt_lock);
            break;
        }
        if (!p->need_wakeup) {
            MP_STATS(ao, "start audio wait");
            struct timespec ts = mp_rel_time_to_timespec(timeout);
            pthread_cond_timedwait(&p->pt_wakeup, &p->pt_lock, &ts);
            MP_STATS(ao, "end audio wait");
        }
        p->need_wakeup = false;
        pthread_mutex_unlock(&p->pt_lock);
    }
    return NULL;
}

void ao_unblock(struct ao *ao)
{
    if (ao->driver->write) {
        struct buffer_state *p = ao->buffer_state;
        pthread_mutex_lock(&p->lock);
        p->initial_unblocked = true;
        pthread_mutex_unlock(&p->lock);
        ao_wakeup_playthread(ao);
    }
}
