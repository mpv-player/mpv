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
#include "osdep/atomics.h"
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

    // Device delay of the last written sample, in realtime.
    atomic_llong end_time_us;
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
        set_state(ao, AO_STATE_PLAY);
        ao->driver->resume(ao);
    }

    return write_samples;
}

// Read the given amount of samples in the user-provided data buffer. Returns
// the number of samples copied. If there is not enough data (buffer underrun
// or EOF), return the number of samples that could be copied, and fill the
// rest of the user-provided buffer with silence.
// This basically assumes that the audio device doesn't care about underruns.
// If this is called in paused mode, it will always return 0.
// The caller should set out_time_us to the expected delay the last sample
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
        mp_input_wakeup_nolock(ao->input_ctx);

    // pad with silence (underflow/paused/eof)
    for (int n = 0; n < ao->num_planes; n++)
        af_fill_silence(data[n], full_bytes - bytes, ao->format);

    return bytes / ao->sstride;
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
    if (ao->driver->reset)
        ao->driver->reset(ao); // assumes the audio callback thread is stopped
    set_state(ao, AO_STATE_NONE);
    for (int n = 0; n < ao->num_planes; n++)
        mp_ring_reset(p->buffers[n]);
    atomic_store(&p->end_time_us, 0);
}

static void pause(struct ao *ao)
{
    if (ao->driver->reset)
        ao->driver->reset(ao);
    set_state(ao, AO_STATE_NONE);
}

static void resume(struct ao *ao)
{
    set_state(ao, AO_STATE_PLAY);
    ao->driver->resume(ao);
}

static void drain(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;
    int state = atomic_load(&p->state);
    if (IS_PLAYING(state))
        mp_sleep_us(get_delay(ao) * 1000000);
    reset(ao);
}

static bool get_eof(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;
    // For simplicity, ignore the latency. Otherwise, we would have to run an
    // extra thread to time it.
    return mp_ring_buffered(p->buffers[0]) == 0;
}

static void uninit(struct ao *ao)
{
    ao->driver->uninit(ao);
}

static int init(struct ao *ao)
{
    struct ao_pull_state *p = ao->api_priv;
    for (int n = 0; n < ao->num_planes; n++)
        p->buffers[n] = mp_ring_new(ao, ao->buffer * ao->sstride);
    atomic_store(&p->state, AO_STATE_NONE);
    assert(ao->driver->resume);
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
