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
#include "core/mp_msg.h"
#include "core/subopt-helper.h"
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
    unsigned int callback_time0;
    unsigned int callback_time1;
#endif
};

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;

    SDL_LockMutex(priv->buffer_mutex);

#ifdef ESTIMATE_DELAY
    priv->callback_time1 = priv->callback_time0;
    priv->callback_time0 = GetTimer();
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

static void print_help(void) {
  mp_msg(MSGT_AO, MSGL_FATAL,
          "\n-ao sdl commandline help:\n"
          "Example: mpv -ao sdl:buflen=len\n"
          "\nOptions:\n"
          "   buflen=len\n"
          "      Length of audio buffer in seconds\n"
          "   bufcnt=cnt\n"
          "      Count of extra audio buffers\n"
        );
}

static int init(struct ao *ao, char *params)
{
    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        mp_msg(MSGT_AO, MSGL_ERR, "[sdl] already initialized\n");
        return -1;
    }

    float buflen = 0; // use SDL default
    float bufcnt = 2;
    const opt_t subopts[] = {
        {"buflen", OPT_ARG_FLOAT, &buflen, NULL},
        {"bufcnt", OPT_ARG_FLOAT, &bufcnt, NULL},
        {NULL}
    };
    if (subopt_parse(params, subopts) != 0) {
        print_help();
        return -1;
    }

    struct priv *priv = talloc_zero(ao, struct priv);
    ao->priv = priv;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        if (!ao->probing)
            mp_msg(MSGT_AO, MSGL_ERR, "[sdl] SDL_Init failed\n");
        uninit(ao, true);
        return -1;
    }

    SDL_AudioSpec desired, obtained;

    int bytes = 0;
    switch (ao->format) {
        case AF_FORMAT_U8: desired.format = AUDIO_U8; bytes = 1; break;
        case AF_FORMAT_S8: desired.format = AUDIO_S8; bytes = 1; break;
        case AF_FORMAT_U16_LE: desired.format = AUDIO_U16LSB; bytes = 2; break;
        case AF_FORMAT_U16_BE: desired.format = AUDIO_U16MSB; bytes = 2; break;
        default:
        case AF_FORMAT_S16_LE: desired.format = AUDIO_S16LSB; bytes = 2; break;
        case AF_FORMAT_S16_BE: desired.format = AUDIO_S16MSB; bytes = 2; break;
#ifdef AUDIO_S32LSB
        case AF_FORMAT_S32_LE: desired.format = AUDIO_S32LSB; bytes = 4; break;
#endif
#ifdef AUDIO_S32MSB
        case AF_FORMAT_S32_BE: desired.format = AUDIO_S32MSB; bytes = 4; break;
#endif
#ifdef AUDIO_F32LSB
        case AF_FORMAT_FLOAT_LE: desired.format = AUDIO_F32LSB; bytes = 4; break;
#endif
#ifdef AUDIO_F32MSB
        case AF_FORMAT_FLOAT_BE: desired.format = AUDIO_F32MSB; bytes = 4; break;
#endif
    }
    desired.freq = ao->samplerate;
    desired.channels = ao->channels.num;
    desired.samples = FFMIN(32768, ceil_power_of_two(ao->samplerate * buflen));
    desired.callback = audio_callback;
    desired.userdata = ao;

    mp_msg(MSGT_AO, MSGL_V, "[sdl] requested format: %d Hz, %d channels, %x, "
           "buffer size: %d samples\n",
           (int) desired.freq, (int) desired.channels,
           (int) desired.format, (int) desired.samples);

    obtained = desired;
    if (SDL_OpenAudio(&desired, &obtained)) {
        if (!ao->probing)
            mp_msg(MSGT_AO, MSGL_ERR, "[sdl] could not open audio: %s\n",
                SDL_GetError());
        uninit(ao, true);
        return -1;
    }

    mp_msg(MSGT_AO, MSGL_V, "[sdl] obtained format: %d Hz, %d channels, %x, "
           "buffer size: %d samples\n",
           (int) obtained.freq, (int) obtained.channels,
           (int) obtained.format, (int) obtained.samples);

    switch (obtained.format) {
        case AUDIO_U8: ao->format = AF_FORMAT_U8; bytes = 1; break;
        case AUDIO_S8: ao->format = AF_FORMAT_S8; bytes = 1; break;
        case AUDIO_S16LSB: ao->format = AF_FORMAT_S16_LE; bytes = 2; break;
        case AUDIO_S16MSB: ao->format = AF_FORMAT_S16_BE; bytes = 2; break;
        case AUDIO_U16LSB: ao->format = AF_FORMAT_U16_LE; bytes = 2; break;
        case AUDIO_U16MSB: ao->format = AF_FORMAT_U16_BE; bytes = 2; break;
#ifdef AUDIO_S32LSB
        case AUDIO_S32LSB: ao->format = AF_FORMAT_S32_LE; bytes = 4; break;
#endif
#ifdef AUDIO_S32MSB
        case AUDIO_S32MSB: ao->format = AF_FORMAT_S32_BE; bytes = 4; break;
#endif
#ifdef AUDIO_F32LSB
        case AUDIO_F32LSB: ao->format = AF_FORMAT_FLOAT_LE; bytes = 4; break;
#endif
#ifdef AUDIO_F32MSB
        case AUDIO_F32MSB: ao->format = AF_FORMAT_FLOAT_BE; bytes = 4; break;
#endif
        default:
            if (!ao->probing)
                mp_msg(MSGT_AO, MSGL_ERR,
                       "[sdl] could not find matching format\n");
            uninit(ao, true);
            return -1;
    }

    ao->samplerate = obtained.freq;
    mp_chmap_from_channels(&ao->channels, obtained.channels);
    mp_chmap_reorder_to_alsa(&ao->channels);
    ao->bps = ao->channels.num * ao->samplerate * bytes;
    ao->buffersize = obtained.size * bufcnt;
    ao->outburst = obtained.size;
    priv->buffer = av_fifo_alloc(ao->buffersize);
    priv->buffer_mutex = SDL_CreateMutex();
    if (!priv->buffer_mutex) {
        mp_msg(MSGT_AO, MSGL_ERR, "[sdl] SDL_CreateMutex failed\n");
        uninit(ao, true);
        return -1;
    }
    priv->underrun_cond = SDL_CreateCond();
    if (!priv->underrun_cond) {
        mp_msg(MSGT_AO, MSGL_ERR, "[sdl] SDL_CreateCond failed\n");
        uninit(ao, true);
        return -1;
    }

    priv->unpause = 1;
    priv->paused = 1;
    priv->callback_time0 = priv->callback_time1 = GetTimer();

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
    return space;
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

static int play(struct ao *ao, void *data, int len, int flags)
{
    struct priv *priv = ao->priv;
    SDL_LockMutex(priv->buffer_mutex);
    int free = av_fifo_space(priv->buffer);
    if (len > free) len = free;
    av_fifo_generic_write(priv->buffer, data, len, NULL);
    SDL_CondSignal(priv->underrun_cond);
    SDL_UnlockMutex(priv->buffer_mutex);
    if (priv->unpause) {
        priv->unpause = 0;
        do_resume(ao);
    }
    return len;
}

static float get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;
    SDL_LockMutex(priv->buffer_mutex);
    int sz = av_fifo_size(priv->buffer);
#ifdef ESTIMATE_DELAY
    unsigned int callback_time0 = priv->callback_time0;
    unsigned int callback_time1 = priv->callback_time1;
#endif
    SDL_UnlockMutex(priv->buffer_mutex);

    // delay component: our FIFO's length
    float delay = sz / (float) ao->bps;

#ifdef ESTIMATE_DELAY
    // delay component: outstanding audio living in SDL

    unsigned int current_time = GetTimer();

    // interval between callbacks
    unsigned int callback_interval = callback_time0 - callback_time1;
    unsigned int elapsed_interval = current_time - callback_time0;
    if (elapsed_interval > callback_interval)
        elapsed_interval = callback_interval;

    // delay subcomponent: remaining audio from the currently played buffer
    unsigned int buffer_interval = callback_interval - elapsed_interval;

    // delay subcomponent: remaining audio from the next played buffer, as
    // provided by the callback
    buffer_interval += callback_interval;

    delay += buffer_interval / 1000000.0;
#endif

    return delay;
}

const struct ao_driver audio_out_sdl = {
    .info = &(const struct ao_info) {
        "SDL Audio",
        "sdl",
        "Rudolf Polzer <divVerent@xonotic.org>",
        ""
    },
    .init      = init,
    .uninit    = uninit,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = pause,
    .resume    = resume,
    .reset     = reset,
};
