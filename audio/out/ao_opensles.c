/*
 * OpenSL ES audio output driver.
 * Copyright (C) 2016 Ilya Zhuravlev <whatever@xyz.is>
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
#include "internal.h"
#include "common/msg.h"
#include "audio/format.h"
#include "options/m_option.h"
#include "osdep/timer.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <pthread.h>

struct priv {
    SLObjectItf sl, output_mix, player;
    SLBufferQueueItf buffer_queue;
    SLEngineItf engine;
    SLPlayItf play;
    char *curbuf, *buf1, *buf2;
    size_t buffer_size;
    pthread_mutex_t buffer_lock;

    int cfg_frames_per_buffer;
    int cfg_sample_rate;
};

static const int fmtmap[][2] = {
    { AF_FORMAT_U8, SL_PCMSAMPLEFORMAT_FIXED_8 },
    { AF_FORMAT_S16, SL_PCMSAMPLEFORMAT_FIXED_16 },
    { 0 }
};

#define DESTROY(thing) \
    if (p->thing) { \
        (*p->thing)->Destroy(p->thing); \
        p->thing = NULL; \
    }

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    DESTROY(player);
    DESTROY(output_mix);
    DESTROY(sl);

    p->buffer_queue = NULL;
    p->engine = NULL;
    p->play = NULL;

    pthread_mutex_destroy(&p->buffer_lock);

    free(p->buf1);
    free(p->buf2);
    p->curbuf = p->buf1 = p->buf2 = NULL;
    p->buffer_size = 0;
}

#undef DESTROY

static void buffer_callback(SLBufferQueueItf buffer_queue, void *context)
{
    struct ao *ao = context;
    struct priv *p = ao->priv;
    SLresult res;
    void *data[1];
    double delay;

    pthread_mutex_lock(&p->buffer_lock);

    data[0] = p->curbuf;
    delay = 2 * p->buffer_size / (double)ao->bps;
    ao_read_data(ao, data, p->buffer_size / ao->sstride,
        mp_time_us() + 1000000LL * delay);

    res = (*buffer_queue)->Enqueue(buffer_queue, p->curbuf, p->buffer_size);
    if (res != SL_RESULT_SUCCESS)
        MP_ERR(ao, "Failed to Enqueue: %d\n", res);
    else
        p->curbuf = (p->curbuf == p->buf1) ? p->buf2 : p->buf1;

    pthread_mutex_unlock(&p->buffer_lock);
}

#define DEFAULT_BUFFER_SIZE_MS 50

#define CHK(stmt) \
    { \
        SLresult res = stmt; \
        if (res != SL_RESULT_SUCCESS) { \
            MP_ERR(ao, "%s: %d\n", #stmt, res); \
            goto error; \
        } \
    }

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    SLDataLocator_BufferQueue locator_buffer_queue;
    SLDataLocator_OutputMix locator_output_mix;
    SLDataFormat_PCM pcm;
    SLDataSource audio_source;
    SLDataSink audio_sink;

    // This AO only supports two channels at the moment
    mp_chmap_from_channels(&ao->channels, 2);

    CHK(slCreateEngine(&p->sl, 0, NULL, 0, NULL, NULL));
    CHK((*p->sl)->Realize(p->sl, SL_BOOLEAN_FALSE));
    CHK((*p->sl)->GetInterface(p->sl, SL_IID_ENGINE, (void*)&p->engine));
    CHK((*p->engine)->CreateOutputMix(p->engine, &p->output_mix, 0, NULL, NULL));
    CHK((*p->output_mix)->Realize(p->output_mix, SL_BOOLEAN_FALSE));

    locator_buffer_queue.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
    locator_buffer_queue.numBuffers = 2;

    pcm.formatType = SL_DATAFORMAT_PCM;
    pcm.numChannels = 2;

    int compatible_formats[AF_FORMAT_COUNT];
    af_get_best_sample_formats(ao->format, compatible_formats);
    pcm.bitsPerSample = 0;
    for (int i = 0; compatible_formats[i] && !pcm.bitsPerSample; ++i)
        for (int j = 0; fmtmap[j][0]; ++j)
            if (compatible_formats[i] == fmtmap[j][0]) {
                ao->format = fmtmap[j][0];
                pcm.bitsPerSample = fmtmap[j][1];
                break;
            }
    if (!pcm.bitsPerSample) {
        MP_ERR(ao, "Cannot find compatible audio format\n");
        goto error;
    }
    pcm.containerSize = 8 * af_fmt_to_bytes(ao->format);
    pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

    if (p->cfg_sample_rate)
        ao->samplerate = p->cfg_sample_rate;

    // samplesPerSec is misnamed, actually it's samples per ms
    pcm.samplesPerSec = ao->samplerate * 1000;

    if (p->cfg_frames_per_buffer)
        ao->device_buffer = p->cfg_frames_per_buffer;
    else
        ao->device_buffer = ao->samplerate * DEFAULT_BUFFER_SIZE_MS / 1000;
    p->buffer_size = ao->device_buffer * ao->channels.num *
        af_fmt_to_bytes(ao->format);
    p->buf1 = calloc(1, p->buffer_size);
    p->buf2 = calloc(1, p->buffer_size);
    p->curbuf = p->buf1;
    if (!p->buf1 || !p->buf2) {
        MP_ERR(ao, "Failed to allocate device buffer\n");
        goto error;
    }
    int r = pthread_mutex_init(&p->buffer_lock, NULL);
    if (r) {
        MP_ERR(ao, "Failed to initialize the mutex: %d\n", r);
        goto error;
    }

    audio_source.pFormat = (void*)&pcm;
    audio_source.pLocator = (void*)&locator_buffer_queue;

    locator_output_mix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    locator_output_mix.outputMix = p->output_mix;

    audio_sink.pLocator = (void*)&locator_output_mix;
    audio_sink.pFormat = NULL;

    SLboolean required[] = { SL_BOOLEAN_TRUE };
    SLInterfaceID iid_array[] = { SL_IID_BUFFERQUEUE };
    CHK((*p->engine)->CreateAudioPlayer(p->engine, &p->player, &audio_source,
        &audio_sink, 1, iid_array, required));
    CHK((*p->player)->Realize(p->player, SL_BOOLEAN_FALSE));
    CHK((*p->player)->GetInterface(p->player, SL_IID_PLAY, (void*)&p->play));
    CHK((*p->player)->GetInterface(p->player, SL_IID_BUFFERQUEUE,
        (void*)&p->buffer_queue));
    CHK((*p->buffer_queue)->RegisterCallback(p->buffer_queue,
        buffer_callback, ao));

    return 1;
error:
    uninit(ao);
    return -1;
}

#undef CHK

static void set_play_state(struct ao *ao, SLuint32 state)
{
    struct priv *p = ao->priv;
    SLresult res = (*p->play)->SetPlayState(p->play, state);
    if (res != SL_RESULT_SUCCESS)
        MP_ERR(ao, "Failed to SetPlayState(%d): %d\n", state, res);
}

static void reset(struct ao *ao)
{
    set_play_state(ao, SL_PLAYSTATE_STOPPED);
}

static void resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    set_play_state(ao, SL_PLAYSTATE_PLAYING);

    // enqueue two buffers
    buffer_callback(p->buffer_queue, ao);
    buffer_callback(p->buffer_queue, ao);
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_opensles = {
    .description = "OpenSL ES audio output",
    .name      = "opensles",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .resume    = resume,

    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_INTRANGE("frames-per-buffer", cfg_frames_per_buffer, 0, 1, 10000),
        OPT_INTRANGE("sample-rate", cfg_sample_rate, 0, 1000, 100000),
        {0}
    },
    .legacy_prefix = "opensles",
};
