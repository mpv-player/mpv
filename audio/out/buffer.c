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

#include "filters/f_async_queue.h"
#include "filters/filter_internal.h"

#include "osdep/timer.h"
#include "osdep/threads.h"

struct buffer_state {
    // Buffer and AO
    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // Playthread sleep
    pthread_mutex_t pt_lock;
    pthread_cond_t pt_wakeup;

    // Access from AO driver's thread only.
    char *convert_buffer;

    // Immutable.
    struct mp_async_queue *queue;

    // --- protected by lock

    struct mp_filter *filter_root;
    struct mp_filter *input;    // connected to queue
    struct mp_aframe *pending;  // last, not fully consumed output

    bool streaming;             // AO streaming active
    bool playing;               // logically playing audio from buffer
    bool paused;                // logically paused

    int64_t end_time_ns;        // absolute output time of last played sample

    bool initial_unblocked;

    // "Push" AOs only (AOs with driver->write).
    bool hw_paused;             // driver->set_pause() was used successfully
    bool recover_pause;         // non-hw_paused: needs to recover delay
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

    if (p->paused && p->playing && !ao->stream_silence) {
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

struct mp_async_queue *ao_get_queue(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    return p->queue;
}

// Special behavior with data==NULL: caller uses p->pending.
static int read_buffer(struct ao *ao, void **data, int samples, bool *eof)
{
    struct buffer_state *p = ao->buffer_state;
    int pos = 0;
    *eof = false;

    while (p->playing && !p->paused && pos < samples) {
        if (!p->pending || !mp_aframe_get_size(p->pending)) {
            TA_FREEP(&p->pending);
            struct mp_frame frame = mp_pin_out_read(p->input->pins[0]);
            if (!frame.type)
                break; // we can't/don't want to block
            if (frame.type != MP_FRAME_AUDIO) {
                if (frame.type == MP_FRAME_EOF)
                    *eof = true;
                mp_frame_unref(&frame);
                continue;
            }
            p->pending = frame.data;
        }

        if (!data)
            break;

        int copy = mp_aframe_get_size(p->pending);
        uint8_t **fdata = mp_aframe_get_data_ro(p->pending);
        copy = MPMIN(copy, samples - pos);
        for (int n = 0; n < ao->num_planes; n++) {
            memcpy((char *)data[n] + pos * ao->sstride,
                   fdata[n], copy * ao->sstride);
        }
        mp_aframe_skip_samples(p->pending, copy);
        pos += copy;
        *eof = false;
    }

    if (!data) {
        if (!p->pending)
            return 0;
        void **pd = (void *)mp_aframe_get_data_rw(p->pending);
        if (pd)
            ao_post_process_data(ao, pd, mp_aframe_get_size(p->pending));
        return 1;
    }

    // pad with silence (underflow/paused/eof)
    for (int n = 0; n < ao->num_planes; n++) {
        af_fill_silence((char *)data[n] + pos * ao->sstride,
                        (samples - pos) * ao->sstride,
                        ao->format);
    }

    ao_post_process_data(ao, data, pos);
    return pos;
}

// Read the given amount of samples in the user-provided data buffer. Returns
// the number of samples copied. If there is not enough data (buffer underrun
// or EOF), return the number of samples that could be copied, and fill the
// rest of the user-provided buffer with silence.
// This basically assumes that the audio device doesn't care about underruns.
// If this is called in paused mode, it will always return 0.
// The caller should set out_time_ns to the expected delay until the last sample
// reaches the speakers, in nanoseconds, using mp_time_ns() as reference.
int ao_read_data(struct ao *ao, void **data, int samples, int64_t out_time_ns)
{
    struct buffer_state *p = ao->buffer_state;
    assert(!ao->driver->write);

    if (pthread_mutex_trylock(&p->lock))
        return 0;

    int pos = read_buffer(ao, data, samples, &(bool){0});

    if (pos > 0)
        p->end_time_ns = out_time_ns;

    if (pos < samples && p->playing && !p->paused) {
        p->playing = false;
        ao->wakeup_cb(ao->wakeup_ctx);
        // For ao_drain().
        pthread_cond_broadcast(&p->wakeup);
    }

    pthread_mutex_unlock(&p->lock);

    return pos;
}

// Same as ao_read_data(), but convert data according to *fmt.
// fmt->src_fmt and fmt->channels must be the same as the AO parameters.
int ao_read_data_converted(struct ao *ao, struct ao_convert_fmt *fmt,
                           void **data, int samples, int64_t out_time_ns)
{
    struct buffer_state *p = ao->buffer_state;
    void *ndata[MP_NUM_CHANNELS] = {0};

    if (!ao_need_conversion(fmt))
        return ao_read_data(ao, data, samples, out_time_ns);

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

    int res = ao_read_data(ao, ndata, samples, out_time_ns);

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

double ao_get_delay(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);

    double driver_delay;
    if (ao->driver->write) {
        struct mp_pcm_state state;
        get_dev_state(ao, &state);
        driver_delay = state.delay;
    } else {
        int64_t end = p->end_time_ns;
        int64_t now = mp_time_ns();
        driver_delay = MPMAX(0, MP_TIME_NS_TO_S(end - now));
    }

    int pending = mp_async_queue_get_samples(p->queue);
    if (p->pending)
        pending += mp_aframe_get_size(p->pending);

    pthread_mutex_unlock(&p->lock);
    return driver_delay + pending / (double)ao->samplerate;
}

// Fully stop playback; clear buffers, including queue.
void ao_reset(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    bool wakeup = false;
    bool do_reset = false;

    pthread_mutex_lock(&p->lock);

    TA_FREEP(&p->pending);
    mp_async_queue_reset(p->queue);
    mp_filter_reset(p->filter_root);
    mp_async_queue_resume_reading(p->queue);

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
    wakeup = p->playing;
    p->playing = false;
    p->recover_pause = false;
    p->hw_paused = false;
    p->end_time_ns = 0;

    pthread_mutex_unlock(&p->lock);

    if (do_reset)
        ao->driver->reset(ao);

    if (wakeup)
        ao_wakeup_playthread(ao);
}

// Initiate playback. This moves from the stop/underrun state to actually
// playing (orthogonally taking the paused state into account). Plays all
// data in the queue, and goes into underrun state if no more data available.
// No-op if already running.
void ao_start(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;
    bool do_start = false;

    pthread_mutex_lock(&p->lock);

    p->playing = true;

    if (!ao->driver->write && !p->paused && !p->streaming) {
        p->streaming = true;
        do_start = true;
    }

    pthread_mutex_unlock(&p->lock);

    // Pull AOs might call ao_read_data() so do this outside the lock.
    if (do_start)
        ao->driver->start(ao);

    ao_wakeup_playthread(ao);
}

void ao_set_paused(struct ao *ao, bool paused, bool eof)
{
    struct buffer_state *p = ao->buffer_state;
    bool wakeup = false;
    bool do_reset = false, do_start = false;

    // If we are going to pause on eof and ao is still playing,
    // be sure to drain the ao first for gapless.
    if (eof && paused && ao_is_playing(ao))
        ao_drain(ao);

    pthread_mutex_lock(&p->lock);

    if ((p->playing || !ao->driver->write) && !p->paused && paused) {
        if (p->streaming && !ao->stream_silence) {
            if (ao->driver->write) {
                if (!p->recover_pause)
                    get_dev_state(ao, &p->prepause_state);
                if (ao->driver->set_pause && ao->driver->set_pause(ao, true)) {
                    p->hw_paused = true;
                } else {
                    ao->driver->reset(ao);
                    p->streaming = false;
                    p->recover_pause = !ao->untimed;
                }
            } else if (ao->driver->reset) {
                // See ao_reset() why this is done outside of the lock.
                do_reset = true;
                p->streaming = false;
            }
        }
        wakeup = true;
    } else if (p->playing && p->paused && !paused) {
        if (ao->driver->write) {
            if (p->hw_paused)
                ao->driver->set_pause(ao, false);
            p->hw_paused = false;
        } else {
            if (!p->streaming)
                do_start = true;
            p->streaming = true;
        }
        wakeup = true;
    }
    p->paused = paused;

    pthread_mutex_unlock(&p->lock);

    if (do_reset)
        ao->driver->reset(ao);
    if (do_start)
        ao->driver->start(ao);

    if (wakeup)
        ao_wakeup_playthread(ao);
}

// Whether audio is playing. This means that there is still data in the buffers,
// and ao_start() was called. This returns true even if playback was logically
// paused. On false, EOF was reached, or an underrun happened, or ao_reset()
// was called.
bool ao_is_playing(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);
    bool playing = p->playing;
    pthread_mutex_unlock(&p->lock);

    return playing;
}

// Block until the current audio buffer has played completely.
void ao_drain(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    pthread_mutex_lock(&p->lock);
    while (!p->paused && p->playing) {
        pthread_mutex_unlock(&p->lock);
        double delay = ao_get_delay(ao);
        pthread_mutex_lock(&p->lock);

        // Limit to buffer + arbitrary ~250ms max. waiting for robustness.
        delay += mp_async_queue_get_samples(p->queue) / (double)ao->samplerate;
        struct timespec ts = mp_rel_time_to_timespec(MPMAX(delay, 0) + 0.25);

        // Wait for EOF signal from AO.
        if (pthread_cond_timedwait(&p->wakeup, &p->lock, &ts)) {
            MP_VERBOSE(ao, "drain timeout\n");
            break;
        }

        if (!p->playing && mp_async_queue_get_samples(p->queue)) {
            MP_WARN(ao, "underrun during draining\n");
            pthread_mutex_unlock(&p->lock);
            ao_start(ao);
            pthread_mutex_lock(&p->lock);
        }
    }
    pthread_mutex_unlock(&p->lock);

    ao_reset(ao);
}

static void wakeup_filters(void *ctx)
{
    struct ao *ao = ctx;
    ao_wakeup_playthread(ao);
}

void ao_uninit(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    if (p && p->thread_valid) {
        pthread_mutex_lock(&p->pt_lock);
        p->terminate = true;
        pthread_cond_broadcast(&p->pt_wakeup);
        pthread_mutex_unlock(&p->pt_lock);

        pthread_join(p->thread, NULL);
        p->thread_valid = false;
    }

    if (ao->driver_initialized)
        ao->driver->uninit(ao);

    if (p) {
        talloc_free(p->filter_root);
        talloc_free(p->queue);
        talloc_free(p->pending);
        talloc_free(p->convert_buffer);
        talloc_free(p->temp_buf);

        pthread_cond_destroy(&p->wakeup);
        pthread_mutex_destroy(&p->lock);

        pthread_cond_destroy(&p->pt_wakeup);
        pthread_mutex_destroy(&p->pt_lock);
    }

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

    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->wakeup, NULL);

    pthread_mutex_init(&p->pt_lock, NULL);
    pthread_cond_init(&p->pt_wakeup, NULL);

    p->queue = mp_async_queue_create();
    p->filter_root = mp_filter_create_root(ao->global);
    p->input = mp_async_queue_create_filter(p->filter_root, MP_PIN_OUT, p->queue);

    mp_async_queue_resume_reading(p->queue);

    struct mp_async_queue_config cfg = {
        .sample_unit = AQUEUE_UNIT_SAMPLES,
        .max_samples = ao->buffer,
        .max_bytes = INT64_MAX,
    };
    mp_async_queue_set_config(p->queue, cfg);

    if (ao->driver->write) {
        mp_filter_graph_set_wakeup_cb(p->filter_root, wakeup_filters, ao);

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

    if (ao->stream_silence) {
        MP_WARN(ao, "The --audio-stream-silence option is set. This will break "
                "certain player behavior.\n");
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
static bool ao_play_data(struct ao *ao)
{
    struct buffer_state *p = ao->buffer_state;

    if ((!p->playing || p->paused) && !ao->stream_silence)
        return false;

    struct mp_pcm_state state;
    get_dev_state(ao, &state);

    if (p->streaming && !state.playing && !ao->untimed)
        goto eof;

    void **planes = NULL;
    int space = state.free_samples;
    if (!space)
        return false;
    assert(space >= 0);

    int samples = 0;
    bool got_eof = false;
    if (ao->driver->write_frames) {
        TA_FREEP(&p->pending);
        samples = read_buffer(ao, NULL, 1, &got_eof);
        planes = (void **)&p->pending;
    } else {
        if (!realloc_buf(ao, space)) {
            MP_ERR(ao, "Failed to allocate buffer.\n");
            return false;
        }
        planes = (void **)mp_aframe_get_data_rw(p->temp_buf);
        assert(planes);

        if (p->recover_pause) {
            samples = MPCLAMP(p->prepause_state.delay * ao->samplerate, 0, space);
            p->recover_pause = false;
            mp_aframe_set_silence(p->temp_buf, 0, space);
        }

        if (!samples) {
            samples = read_buffer(ao, planes, space, &got_eof);
            if (p->paused || (ao->stream_silence && !p->playing))
                samples = space; // read_buffer() sets remainder to silent
        }
    }

    if (samples) {
        MP_STATS(ao, "start ao fill");
        if (!ao->driver->write(ao, planes, samples))
            MP_ERR(ao, "Error writing audio to device.\n");
        MP_STATS(ao, "end ao fill");

        if (!p->streaming) {
            MP_VERBOSE(ao, "starting AO\n");
            ao->driver->start(ao);
            p->streaming = true;
            state.playing = true;
        }
    }

    MP_TRACE(ao, "in=%d space=%d(%d) pl=%d, eof=%d\n",
             samples, space, state.free_samples, p->playing, got_eof);

    if (got_eof)
        goto eof;

    return samples > 0 && (samples < space || ao->untimed);

eof:
    MP_VERBOSE(ao, "audio end or underrun\n");
    // Normal AOs signal EOF on underrun, untimed AOs never signal underruns.
    if (ao->untimed || !state.playing || ao->stream_silence) {
        p->streaming = state.playing && !ao->untimed;
        p->playing = false;
    }
    ao->wakeup_cb(ao->wakeup_ctx);
    // For ao_drain().
    pthread_cond_broadcast(&p->wakeup);
    return true;
}

static void *playthread(void *arg)
{
    struct ao *ao = arg;
    struct buffer_state *p = ao->buffer_state;
    mpthread_set_name("ao");
    while (1) {
        pthread_mutex_lock(&p->lock);

        bool retry = false;
        if (!ao->driver->initially_blocked || p->initial_unblocked)
            retry = ao_play_data(ao);

        // Wait until the device wants us to write more data to it.
        // Fallback to guessing.
        double timeout = INFINITY;
        if (p->streaming && !retry && (!p->paused || ao->stream_silence)) {
            // Wake up again if half of the audio buffer has been played.
            // Since audio could play at a faster or slower pace, wake up twice
            // as often as ideally needed.
            timeout = ao->device_buffer / (double)ao->samplerate * 0.25;
        }

        pthread_mutex_unlock(&p->lock);

        pthread_mutex_lock(&p->pt_lock);
        if (p->terminate) {
            pthread_mutex_unlock(&p->pt_lock);
            break;
        }
        if (!p->need_wakeup && !retry) {
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
