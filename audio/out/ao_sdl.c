/*
 * audio output driver for SDL 1.2+
 * Copyright (C) 2012 Rudolf Polzer <divVerent@xonotic.org>
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

#include "config.h"
#include "audio/format.h"
#include "mpv_talloc.h"
#include "ao.h"
#include "internal.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "osdep/timer.h"

#include <SDL.h>

struct priv
{
    bool paused;

    float buflen;
};

static const int fmtmap[][2] = {
    {AF_FORMAT_U8,      AUDIO_U8},
    {AF_FORMAT_S16,     AUDIO_S16SYS},
#ifdef AUDIO_S32SYS
    {AF_FORMAT_S32,     AUDIO_S32SYS},
#endif
#ifdef AUDIO_F32SYS
    {AF_FORMAT_FLOAT,   AUDIO_F32SYS},
#endif
    {0}
};

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    struct ao *ao = userdata;

    void *data[1] = {stream};

    if (len % ao->sstride)
        MP_ERR(ao, "SDL audio callback not sample aligned");

    // Time this buffer will take, plus assume 1 period (1 callback invocation)
    // fixed latency.
    double delay = 2 * len / (double)ao->bps;

    ao_read_data(ao, data, len / ao->sstride, mp_time_us() + 1000000LL * delay);
}

static void uninit(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (!priv)
        return;

    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        // make sure the callback exits
        SDL_LockAudio();

        // close audio device
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

static unsigned int ceil_power_of_two(unsigned int x)
{
    int y = 1;
    while (y < x)
        y *= 2;
    return y;
}

static int init(struct ao *ao)
{
    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        MP_ERR(ao, "already initialized\n");
        return -1;
    }

    struct priv *priv = ao->priv;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        if (!ao->probing)
            MP_ERR(ao, "SDL_Init failed\n");
        uninit(ao);
        return -1;
    }

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext_def(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels)) {
        uninit(ao);
        return -1;
    }

    ao->format = af_fmt_from_planar(ao->format);

    SDL_AudioSpec desired = {0};
    desired.format = AUDIO_S16SYS;
    for (int n = 0; fmtmap[n][0]; n++) {
        if (ao->format == fmtmap[n][0]) {
            desired.format = fmtmap[n][1];
            break;
        }
    }
    desired.freq = ao->samplerate;
    desired.channels = ao->channels.num;
    if (priv->buflen) {
        desired.samples = MPMIN(32768, ceil_power_of_two(ao->samplerate *
                                                         priv->buflen));
    }
    desired.callback = audio_callback;
    desired.userdata = ao;

    MP_VERBOSE(ao, "requested format: %d Hz, %d channels, %x, "
               "buffer size: %d samples\n",
               (int) desired.freq, (int) desired.channels,
               (int) desired.format, (int) desired.samples);

    SDL_AudioSpec obtained = desired;
    if (SDL_OpenAudio(&desired, &obtained)) {
        if (!ao->probing)
            MP_ERR(ao, "could not open audio: %s\n", SDL_GetError());
        uninit(ao);
        return -1;
    }

    MP_VERBOSE(ao, "obtained format: %d Hz, %d channels, %x, "
               "buffer size: %d samples\n",
               (int) obtained.freq, (int) obtained.channels,
               (int) obtained.format, (int) obtained.samples);

    // The sample count is usually the number of samples the callback requests,
    // which we assume is the period size. Normally, ao.c will allocate a large
    // enough buffer. But in case the period size should be pathologically
    // large, this will help.
    ao->device_buffer = 3 * obtained.samples;

    ao->format = 0;
    for (int n = 0; fmtmap[n][0]; n++) {
        if (obtained.format == fmtmap[n][1]) {
            ao->format = fmtmap[n][0];
            break;
        }
    }
    if (!ao->format) {
        if (!ao->probing)
            MP_ERR(ao, "could not find matching format\n");
        uninit(ao);
        return -1;
    }

    if (!ao_chmap_sel_get_def(ao, &sel, &ao->channels, obtained.channels)) {
        uninit(ao);
        return -1;
    }

    ao->samplerate = obtained.freq;

    priv->paused = 1;

    return 1;
}

static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (!priv->paused)
        SDL_PauseAudio(SDL_TRUE);
    priv->paused = 1;
}

static void start(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (priv->paused)
        SDL_PauseAudio(SDL_FALSE);
    priv->paused = 0;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_sdl = {
    .description = "SDL Audio",
    .name      = "sdl",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .start     = start,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .buflen = 0, // use SDL default
    },
    .options = (const struct m_option[]) {
        {"buflen", OPT_FLOAT(buflen)},
        {0}
    },
    .options_prefix = "sdl",
};
