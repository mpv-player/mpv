/*
 * audio output driver for SDL 1.2+
 * Copyright (C) 2012 Rudolf Polzer <divVerent@xonotic.org>
 *
 * This file is part of mpv.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "audio/format.h"
#include "talloc.h"
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

    SDL_AudioSpec desired, obtained;

    switch (ao->format) {
        case AF_FORMAT_U8: desired.format = AUDIO_U8; break;
        case AF_FORMAT_S8: desired.format = AUDIO_S8; break;
        case AF_FORMAT_U16_LE: desired.format = AUDIO_U16LSB; break;
        case AF_FORMAT_U16_BE: desired.format = AUDIO_U16MSB; break;
        default:
        case AF_FORMAT_S16_LE: desired.format = AUDIO_S16LSB; break;
        case AF_FORMAT_S16_BE: desired.format = AUDIO_S16MSB; break;
#ifdef AUDIO_S32LSB
        case AF_FORMAT_S32_LE: desired.format = AUDIO_S32LSB; break;
#endif
#ifdef AUDIO_S32MSB
        case AF_FORMAT_S32_BE: desired.format = AUDIO_S32MSB; break;
#endif
#ifdef AUDIO_F32LSB
        case AF_FORMAT_FLOAT_LE: desired.format = AUDIO_F32LSB; break;
#endif
#ifdef AUDIO_F32MSB
        case AF_FORMAT_FLOAT_BE: desired.format = AUDIO_F32MSB; break;
#endif
    }
    desired.freq = ao->samplerate;
    desired.channels = ao->channels.num;
    desired.samples = MPMIN(32768, ceil_power_of_two(ao->samplerate *
                                                     priv->buflen));
    desired.callback = audio_callback;
    desired.userdata = ao;

    MP_VERBOSE(ao, "requested format: %d Hz, %d channels, %x, "
               "buffer size: %d samples\n",
               (int) desired.freq, (int) desired.channels,
               (int) desired.format, (int) desired.samples);

    obtained = desired;
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

    switch (obtained.format) {
        case AUDIO_U8: ao->format = AF_FORMAT_U8; break;
        case AUDIO_S8: ao->format = AF_FORMAT_S8; break;
        case AUDIO_S16LSB: ao->format = AF_FORMAT_S16_LE; break;
        case AUDIO_S16MSB: ao->format = AF_FORMAT_S16_BE; break;
        case AUDIO_U16LSB: ao->format = AF_FORMAT_U16_LE; break;
        case AUDIO_U16MSB: ao->format = AF_FORMAT_U16_BE; break;
#ifdef AUDIO_S32LSB
        case AUDIO_S32LSB: ao->format = AF_FORMAT_S32_LE; break;
#endif
#ifdef AUDIO_S32MSB
        case AUDIO_S32MSB: ao->format = AF_FORMAT_S32_BE; break;
#endif
#ifdef AUDIO_F32LSB
        case AUDIO_F32LSB: ao->format = AF_FORMAT_FLOAT_LE; break;
#endif
#ifdef AUDIO_F32MSB
        case AUDIO_F32MSB: ao->format = AF_FORMAT_FLOAT_BE; break;
#endif
        default:
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

static void pause(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (!priv->paused)
        SDL_PauseAudio(SDL_TRUE);
    priv->paused = 1;
}

static void resume(struct ao *ao)
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
    .pause     = pause,
    .resume    = resume,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .buflen = 0, // use SDL default
    },
    .options = (const struct m_option[]) {
        OPT_FLOAT("buflen", buflen, 0),
        {0}
    },
};
