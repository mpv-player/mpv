/*
 * audio output driver for SDL3
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

#include <stdio.h>

#include <SDL3/SDL.h>

#include "audio/format.h"
#include "mpv_talloc.h"
#include "ao.h"
#include "internal.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "osdep/timer.h"

struct priv
{
    bool paused;

    float buflen;

    SDL_AudioStream *stream;
    uint8_t *callback_buf;
    size_t callback_buf_size;
};

static const int fmtmap[][2] = {
    {AF_FORMAT_U8,      SDL_AUDIO_U8},
    {AF_FORMAT_S16,     SDL_AUDIO_S16},
    {AF_FORMAT_S32,     SDL_AUDIO_S32},
    {AF_FORMAT_FLOAT,   SDL_AUDIO_F32},
    {0}
};

static void audio_callback(void *userdata, SDL_AudioStream *stream,
                           int additional_amount, int total_amount)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;

    if (additional_amount <= 0)
        return;

    // SDL can ask for more than callback_buf holds after an underrun
    for (int done = 0; done < additional_amount; ) {
        int n = MPMIN(additional_amount - done, (int)priv->callback_buf_size);
        n -= n % ao->sstride;
        if (n <= 0)
            break;
        done += n;

        // 1 period of fixed latency, plus the time this chunk takes
        int64_t out_time = mp_time_ns() + MP_TIME_S_TO_NS(
            (double)(additional_amount + done) / ao->bps);

        void *data[1] = {priv->callback_buf};
        ao_read_data(ao, data, n / ao->sstride, out_time, NULL, true, true);
        SDL_PutAudioStreamData(stream, priv->callback_buf, n);
    }
}

static void uninit(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (!priv)
        return;

    if (priv->stream) {
        // also closes the device opened by SDL_OpenAudioDeviceStream
        SDL_DestroyAudioStream(priv->stream);
        priv->stream = NULL;
    }

    talloc_free(priv->callback_buf);
    priv->callback_buf = NULL;

    if (SDL_WasInit(SDL_INIT_AUDIO))
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
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

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        if (!ao->probing)
            MP_ERR(ao, "SDL_Init failed: %s\n", SDL_GetError());
        uninit(ao);
        return -1;
    }

    SDL_AudioSpec device_spec = {0};
    bool have_device = SDL_GetAudioDeviceFormat(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &device_spec, NULL);

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext_def(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels) ||
        (have_device && device_spec.channels &&
         !ao_chmap_sel_get_def(ao, &sel, &ao->channels,
                               device_spec.channels)))
    {
        uninit(ao);
        return -1;
    }

    int af = af_fmt_from_planar(ao->format);

    SDL_AudioSpec desired = {0};
    desired.format = SDL_AUDIO_S16;
    ao->format = AF_FORMAT_S16;
    for (int n = 0; fmtmap[n][0]; n++) {
        if (af == fmtmap[n][0]) {
            desired.format = fmtmap[n][1];
            ao->format = fmtmap[n][0];
            break;
        }
    }
    desired.freq = ao->samplerate;
    desired.channels = ao->channels.num;

    if (priv->buflen) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u",
                 MPMIN(32768, ceil_power_of_two(ao->samplerate *
                                                priv->buflen)));
        SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, buf,
                                SDL_HINT_OVERRIDE);
    } else {
        SDL_ResetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);
    }

    MP_VERBOSE(ao, "requested format: %d Hz, %d channels, %x\n",
               (int) desired.freq, (int) desired.channels,
               (int) desired.format);

    // opens paused; the callback can't run before ao->sstride is set
    priv->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                             &desired, audio_callback, ao);
    if (!priv->stream) {
        if (!ao->probing)
            MP_ERR(ao, "could not open audio: %s\n", SDL_GetError());
        uninit(ao);
        return -1;
    }

    // the period is only final once the device is open
    SDL_AudioSpec got = {0};
    int device_frames = 0;
    if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(priv->stream), &got,
                                 &device_frames))
    {
        MP_VERBOSE(ao, "device format: %d Hz, %d channels, %x, "
                   "%d samples per period\n", (int) got.freq,
                   (int) got.channels, (int) got.format, device_frames);
    }

    if (device_frames <= 0 || got.freq <= 0) {
        device_frames = 1024;
    } else {
        // device_buffer is in samples at ao->samplerate
        device_frames = (int64_t)device_frames * ao->samplerate / got.freq;
    }

    ao->device_buffer = 3 * device_frames;

    int sstride = af_fmt_to_bytes(ao->format) * ao->channels.num;
    size_t buf_size = (size_t)ao->device_buffer * sstride;
    buf_size = MPCLAMP(buf_size, 4096, 1 << 20);
    buf_size -= buf_size % sstride;
    priv->callback_buf = talloc_size(ao, buf_size);
    priv->callback_buf_size = buf_size;

    priv->paused = 1;

    return 1;
}

static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (priv->stream && !priv->paused)
        SDL_PauseAudioStreamDevice(priv->stream);
    priv->paused = 1;
}

static void start(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (priv->stream && priv->paused)
        SDL_ResumeAudioStreamDevice(priv->stream);
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
