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
#include "common/msg.h"
#include "options/m_option.h"
#include "osdep/timer.h"

#include <libavutil/fifo.h>
#include <libavutil/common.h>
#include <SDL.h>

// hack because SDL can't be asked about the current delay
#define ESTIMATE_DELAY

struct priv
{
    AVFifoBuffer *buffer;
    SDL_mutex *buffer_mutex;
    SDL_cond *underrun_cond;
    bool unpause;
    bool paused;
#ifdef ESTIMATE_DELAY
    int64_t callback_time0;
    int64_t callback_time1;
#endif
    float buflen;
    float bufcnt;
};

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;

    SDL_LockMutex(priv->buffer_mutex);

#ifdef ESTIMATE_DELAY
    priv->callback_time1 = priv->callback_time0;
    priv->callback_time0 = mp_time_us();
#endif

    while (len > 0 && !priv->paused) {
        int got = av_fifo_size(priv->buffer);
        if (got > len)
            got = len;
        if (got > 0) {
            av_fifo_generic_read(priv->buffer, stream, got, NULL);
            len -= got;
            stream += got;
        }
        if (len > 0)
            SDL_CondWait(priv->underrun_cond, priv->buffer_mutex);
    }

    SDL_UnlockMutex(priv->buffer_mutex);
}

static void uninit(struct ao *ao, bool cut_audio)
{
    struct priv *priv = ao->priv;
    if (!priv)
        return;

    // abort the callback
    priv->paused = 1;

    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        if (priv->buffer_mutex)
            SDL_LockMutex(priv->buffer_mutex);
        if (priv->underrun_cond)
            SDL_CondSignal(priv->underrun_cond);
        if (priv->buffer_mutex)
            SDL_UnlockMutex(priv->buffer_mutex);

        // make sure the callback exits
        SDL_LockAudio();

        // close audio device
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    // get rid of the mutex
    if (priv->underrun_cond)
        SDL_DestroyCond(priv->underrun_cond);
    if (priv->buffer_mutex)
        SDL_DestroyMutex(priv->buffer_mutex);
    if (priv->buffer)
        av_fifo_free(priv->buffer);

    talloc_free(ao->priv);
    ao->priv = NULL;
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
        uninit(ao, true);
        return -1;
    }

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext_def(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels)) {
        uninit(ao, true);
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
    desired.samples = FFMIN(32768, ceil_power_of_two(ao->samplerate *
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
        uninit(ao, true);
        return -1;
    }

    MP_VERBOSE(ao, "obtained format: %d Hz, %d channels, %x, "
               "buffer size: %d samples\n",
               (int) obtained.freq, (int) obtained.channels,
               (int) obtained.format, (int) obtained.samples);

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
            uninit(ao, true);
            return -1;
    }

    if (!ao_chmap_sel_get_def(ao, &sel, &ao->channels, obtained.channels)) {
        uninit(ao, true);
        return -1;
    }

    ao->samplerate = obtained.freq;
    priv->buffer = av_fifo_alloc(obtained.size * priv->bufcnt);
    priv->buffer_mutex = SDL_CreateMutex();
    if (!priv->buffer_mutex) {
        MP_ERR(ao, "SDL_CreateMutex failed\n");
        uninit(ao, true);
        return -1;
    }
    priv->underrun_cond = SDL_CreateCond();
    if (!priv->underrun_cond) {
        MP_ERR(ao, "SDL_CreateCond failed\n");
        uninit(ao, true);
        return -1;
    }

    priv->unpause = 1;
    priv->paused = 1;
    priv->callback_time0 = priv->callback_time1 = mp_time_us();

    return 1;
}

static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;
    SDL_LockMutex(priv->buffer_mutex);
    av_fifo_reset(priv->buffer);
    SDL_UnlockMutex(priv->buffer_mutex);
}

static int get_space(struct ao *ao)
{
    struct priv *priv = ao->priv;
    SDL_LockMutex(priv->buffer_mutex);
    int space = av_fifo_space(priv->buffer);
    SDL_UnlockMutex(priv->buffer_mutex);
    return space / ao->sstride;
}

static void pause(struct ao *ao)
{
    struct priv *priv = ao->priv;
    SDL_PauseAudio(SDL_TRUE);
    priv->unpause = 0;
    priv->paused = 1;
    SDL_CondSignal(priv->underrun_cond);
}

static void do_resume(struct ao *ao)
{
    struct priv *priv = ao->priv;
    priv->paused = 0;
    SDL_PauseAudio(SDL_FALSE);
}

static void resume(struct ao *ao)
{
    struct priv *priv = ao->priv;
    SDL_LockMutex(priv->buffer_mutex);
    int free = av_fifo_space(priv->buffer);
    SDL_UnlockMutex(priv->buffer_mutex);
    if (free)
        priv->unpause = 1;
    else
        do_resume(ao);
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *priv = ao->priv;
    int len = samples * ao->sstride;
    SDL_LockMutex(priv->buffer_mutex);
    int free = av_fifo_space(priv->buffer);
    if (len > free) len = free;
    av_fifo_generic_write(priv->buffer, data[0], len, NULL);
    SDL_CondSignal(priv->underrun_cond);
    SDL_UnlockMutex(priv->buffer_mutex);
    if (priv->unpause) {
        priv->unpause = 0;
        do_resume(ao);
    }
    return len / ao->sstride;
}

static float get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;
    SDL_LockMutex(priv->buffer_mutex);
    int sz = av_fifo_size(priv->buffer);
#ifdef ESTIMATE_DELAY
    int64_t callback_time0 = priv->callback_time0;
    int64_t callback_time1 = priv->callback_time1;
#endif
    SDL_UnlockMutex(priv->buffer_mutex);

    // delay component: our FIFO's length
    float delay = sz / (float) ao->bps;

#ifdef ESTIMATE_DELAY
    // delay component: outstanding audio living in SDL

    int64_t current_time = mp_time_us();

    // interval between callbacks
    int64_t callback_interval = callback_time0 - callback_time1;
    int64_t elapsed_interval = current_time - callback_time0;
    if (elapsed_interval > callback_interval)
        elapsed_interval = callback_interval;

    // delay subcomponent: remaining audio from the currently played buffer
    int64_t buffer_interval = callback_interval - elapsed_interval;

    // delay subcomponent: remaining audio from the next played buffer, as
    // provided by the callback
    buffer_interval += callback_interval;

    delay += buffer_interval / 1000000.0;
#endif

    return delay;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_sdl = {
    .description = "SDL Audio",
    .name      = "sdl",
    .init      = init,
    .uninit    = uninit,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = pause,
    .resume    = resume,
    .reset     = reset,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .buflen = 0, // use SDL default
        .bufcnt = 2,
    },
    .options = (const struct m_option[]) {
        OPT_FLOAT("buflen", buflen, 0),
        OPT_FLOAT("bufcnt", bufcnt, 0),
        {0}
    },
};
