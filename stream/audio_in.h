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

#ifndef MPLAYER_AUDIO_IN_H
#define MPLAYER_AUDIO_IN_H

#define AUDIO_IN_ALSA 1
#define AUDIO_IN_OSS 2
#define AUDIO_IN_SNDIO 3

#include "config.h"

struct mp_log;

#if HAVE_ALSA
#include <alsa/asoundlib.h>

typedef struct {
    char *device;

    snd_pcm_t *handle;
    snd_output_t *log;
    int buffer_time, period_time, chunk_size;
    size_t bits_per_sample, bits_per_frame;
} ai_alsa_t;
#endif

#if HAVE_OSS_AUDIO
typedef struct {
    char *device;

    int audio_fd;
} ai_oss_t;
#endif

#if HAVE_SNDIO
#include <sndio.h>

typedef struct {
    char *device;

    struct sio_hdl *hdl;
} ai_sndio_t;
#endif

typedef struct
{
    struct mp_log *log;
    int type;
    int setup;

    /* requested values */
    int req_channels;
    int req_samplerate;

    /* real values read-only */
    int channels;
    int samplerate;
    int blocksize;
    int bytes_per_sample;
    int samplesize;

#if HAVE_ALSA
    ai_alsa_t alsa;
#endif
#if HAVE_OSS_AUDIO
    ai_oss_t oss;
#endif
#if HAVE_SNDIO
    ai_sndio_t sndio;
#endif
} audio_in_t;

int audio_in_init(audio_in_t *ai, struct mp_log *log, int type);
int audio_in_setup(audio_in_t *ai);
int audio_in_set_device(audio_in_t *ai, char *device);
int audio_in_set_samplerate(audio_in_t *ai, int rate);
int audio_in_set_channels(audio_in_t *ai, int channels);
int audio_in_uninit(audio_in_t *ai);
int audio_in_start_capture(audio_in_t *ai);
int audio_in_read_chunk(audio_in_t *ai, unsigned char *buffer);

#if HAVE_ALSA
int ai_alsa_setup(audio_in_t *ai);
int ai_alsa_init(audio_in_t *ai);
int ai_alsa_xrun(audio_in_t *ai);
#endif

#if HAVE_OSS_AUDIO
int ai_oss_set_samplerate(audio_in_t *ai);
int ai_oss_set_channels(audio_in_t *ai);
int ai_oss_init(audio_in_t *ai);
#endif

#if HAVE_SNDIO
int ai_sndio_setup(audio_in_t *ai);
int ai_sndio_init(audio_in_t *ai);
#endif

#endif /* MPLAYER_AUDIO_IN_H */
