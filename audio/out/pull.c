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
#include <inttypes.h>
#include <assert.h>

#include "ao.h"
#include "internal.h"
#include "audio/format.h"

#include "common/msg.h"
#include "common/common.h"

#include "input/input.h"

#include "osdep/timer.h"
#include "osdep/threads.h"
#include "osdep/atomic.h"
#include "misc/ring.h"

/*
 * Note: there is some stupid stuff in this file in order to avoid mutexes.
 * This requirement is dictated by several audio APIs, at least jackaudio.
 */

enum {
    AO_STATE_NONE,  // idle (e.g. before playback started, or after playback
                    // finished, but device is open)
    AO_STATE_WAIT,  // wait for callback to go into AO_STATE_NONE state
    AO_STATE_PLAY,  // play the buffer
    AO_STATE_BUSY,  // like AO_STATE_PLAY, but ao_read_data() is being called
};

#define IS_PLAYING(st) ((st) == AO_STATE_PLAY || (st) == AO_STATE_BUSY)

struct ao_pull_state {
    // Be very careful with the order when accessing planes.
    struct mp_ring *buffers[MP_NUM_CHANNELS];

    // AO_STATE_*
    atomic_int state;

    // Set when the buffer is intentionally not fed anymore in PLAY state.
    atomic_bool draining;

    // Set by the audio thread when an underflow was detected.
    // It adds the number of samples.
    atomic_int underflow;

    // Device delay of the last written sample, in realtime.
    atomic_llong end_time_us;

    char *convert_buffer;
};

static void set_state(struct ao *ao, int new_state)
{
    struct ao_pull_state *p = ao->api_priv;
    while (1) {
        int old = atomic_load(&p->state);
        if (old == AO_STATE_BUSY) {
            // A spinlock, because some audio APIs don't want us to use mutexes.
            mp_sleep_us(1);
            continue;
        }
        if (atomic_compare_exchange_strong(&p->state, &old, new_state))
            break;
    }
}

static int get_space(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;
    // Since the reader will read the last plane last, its free space is the
    // minimum free space across all planes.
    return mp_ring_available(p->buffers[ao->num_planes - 1]) / ao->sstride;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct ao_pull_state *p = ao->api_priv;

    int write_samples = get_space(ao);
    write_samples = MPMIN(write_samples, samples);

    // Write starting from the last plane - this way, the first plane will
    // always contain the minimum amount of data readable across all planes
    // (assumes the reader starts with the first plane).
    int write_bytes = write_samples * ao->sstride;
    for (int n = ao->num_planes - 1; n >= 0; n--) {
        int r = mp_ring_write(p->buffers[n], data[n], write_bytes);
        assert(r == write_bytes);
    }

    int state = atomic_load(&p->state);
    if (!IS_PLAYING(state)) {
        atomic_store(&p->draining, false);
        atomic_store(&p->underflow, 0);
        set_state(ao, AO_STATE_PLAY);
        if (!ao->stream_silence)
            ao->driver->resume(ao);
    }

    bool draining = write_samples == samples && (flags & AOPLAY_FINAL_CHUNK);
    atomic_store(&p->draining, draining);

    int underflow = atomic_fetch_and(&p->underflow, 0);
    if (underflow)
        MP_WARN(ao, "Audio underflow by %d samples.\n", underflow);

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
    assert(ao->api == &ao_api_pull);

    struct ao_pull_state *p = ao->api_priv;
    int full_bytes = samples * ao->sstride;
    bool need_wakeup = false;
    int bytes = 0;

    // Play silence in states other than AO_STATE_PLAY.
    if (!atomic_compare_exchange_strong(&p->state, &(int){AO_STATE_PLAY},
                                        AO_STATE_BUSY))
        goto end;

    // Since the writer will write the first plane last, its buffered amount
    // of data is the minimum amount across all planes.
    int buffered_bytes = mp_ring_buffered(p->buffers[0]);
    bytes = MPMIN(buffered_bytes, full_bytes);

    if (buffered_bytes < bytes && !atomic_load(&p->draining))
        atomic_fetch_add(&p->underflow, (bytes - buffered_bytes) / ao->sstride);

    if (bytes > 0)
        atomic_store(&p->end_time_us, out_time_us);

    for (int n = 0; n < ao->num_planes; n++) {
        int r = mp_ring_read(p->buffers[n], data[n], bytes);
        bytes = MPMIN(bytes, r);
    }

    // Half of the buffer played -> request more.
    need_wakeup = buffered_bytes - bytes <= mp_ring_size(p->buffers[0]) / 2;

    // Should never fail.
    atomic_compare_exchange_strong(&p->state, &(int){AO_STATE_BUSY}, AO_STATE_PLAY);

end:

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
    assert(ao->api == &ao_api_pull);

    struct ao_pull_state *p = ao->api_priv;
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

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    if (ao->driver->control)
        return ao->driver->control(ao, cmd, arg);
    return CONTROL_UNKNOWN;
}

// Return size of the buffered data in seconds. Can include the device latency.
// Basically, this returns how much data there is still to play, and how long
// it takes until the last sample in the buffer reaches the speakers. This is
// used for audio/video synchronization, so it's very important to implement
// this correctly.
static double get_delay(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;

    int64_t end = atomic_load(&p->end_time_us);
    int64_t now = mp_time_us();
    double driver_delay = MPMAX(0, (end - now) / (1000.0 * 1000.0));
    return mp_ring_buffered(p->buffers[0]) / (double)ao->bps + driver_delay;
}

static void reset(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;
    if (!ao->stream_silence && ao->driver->reset)
        ao->driver->reset(ao); // assumes the audio callback thread is stopped
    set_state(ao, AO_STATE_NONE);
    for (int n = 0; n < ao->num_planes; n++)
        mp_ring_reset(p->buffers[n]);
    atomic_store(&p->end_time_us, 0);
}

static void pause(struct ao *ao)
{
    if (!ao->stream_silence && ao->driver->reset)
        ao->driver->reset(ao);
    set_state(ao, AO_STATE_NONE);
}

static void resume(struct ao *ao)
{
    set_state(ao, AO_STATE_PLAY);
    if (!ao->stream_silence)
        ao->driver->resume(ao);
}

static bool get_eof(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;
    // For simplicity, ignore the latency. Otherwise, we would have to run an
    // extra thread to time it.
    return mp_ring_buffered(p->buffers[0]) == 0;
}

static void drain(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;
    int state = atomic_load(&p->state);
    if (IS_PLAYING(state)) {
        atomic_store(&p->draining, true);
        // Wait for lower bound.
        mp_sleep_us(mp_ring_buffered(p->buffers[0]) / (double)ao->bps * 1e6);
        // And then poll for actual end. (Unfortunately, this code considers
        // audio APIs which do not want you to use mutexes in the audio
        // callback, and an extra semaphore would require slightly more effort.)
        // Limit to arbitrary ~250ms max. waiting for robustness.
        int64_t max = mp_time_us() + 250000;
        while (mp_time_us() < max && !get_eof(ao))
            mp_sleep_us(1);
    }
    reset(ao);
}

static void uninit(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;

    ao->driver->uninit(ao);

    talloc_free(p->convert_buffer);
}

static int init(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;
    for (int n = 0; n < ao->num_planes; n++)
        p->buffers[n] = mp_ring_new(ao, ao->buffer * ao->sstride);
    atomic_store(&p->state, AO_STATE_NONE);
    assert(ao->driver->resume);

    if (ao->stream_silence)
        ao->driver->resume(ao);

    return 0;
}

const struct ao_driver ao_api_pull = {
    .init = init,
    .control = control,
    .uninit = uninit,
    .drain = drain,
    .reset = reset,
    .get_space = get_space,
    .play = play,
    .get_delay = get_delay,
    .get_eof = get_eof,
    .pause = pause,
    .resume = resume,
    .priv_size = sizeof(struct ao_pull_state),
};
