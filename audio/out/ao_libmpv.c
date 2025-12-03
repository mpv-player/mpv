/*
 * libmpv audio output driver
 *
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

#include "ao.h"
#include "audio/format.h"
#include "ao_libmpv.h"
#include "internal.h"
#include "common/msg.h"

struct priv {
    void (*write_cb)(void *userdata, const void *data, int bytes);
    void *userdata;
};

void ao_libmpv_set_cb(struct ao *ao, void (*cb)(void *userdata, const void *data, int bytes), void *userdata)
{
    struct priv *p = ao->priv;
    p->write_cb = cb;
    p->userdata = userdata;
}

static int init(struct ao *ao)
{
    ao->format = af_fmt_from_planar(ao->format);

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        return -1;

    ao->bps = ao->channels.num * (int64_t)ao->samplerate * af_fmt_to_bytes(ao->format);

    MP_INFO(ao, "libmpv: Samplerate: %d Hz Channels: %d Format: %s\n",
        ao->samplerate, ao->channels.num, af_fmt_to_str(ao->format));

    ao->untimed = true;
    ao->device_buffer = 1 << 16;

    return 0;
}

static void uninit(struct ao *ao)
{
}

static bool audio_write(struct ao *ao, void **data, int samples)
{
    struct priv *priv = ao->priv;
    const int len = samples * ao->sstride;

    if (priv->write_cb)
        priv->write_cb(priv->userdata, data[0], len);

    return true;
}

static void get_state(struct ao *ao, struct mp_pcm_state *state)
{
    state->free_samples = ao->device_buffer;
    state->queued_samples = 0;
    state->delay = 0;
}

static bool set_pause(struct ao *ao, bool paused)
{
    return true; // signal support so common code doesn't write silence
}

static void start(struct ao *ao)
{
    // we use data immediately
}

static void reset(struct ao *ao)
{
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_libmpv = {
    .description = "libmpv audio output with a callback",
    .name      = "libmpv",
    .init      = init,
    .uninit    = uninit,
    .get_state = get_state,
    .set_pause = set_pause,
    .write     = audio_write,
    .start     = start,
    .reset     = reset,
    .priv_size = sizeof(struct priv),
};
