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
    void *buf;
    int bytes_per_enqueue;
    pthread_mutex_t buffer_lock;
    double audio_latency;

    int frames_per_enqueue;
    int buffer_size_in_ms;
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

    free(p->buf);
    p->buf = NULL;
}

#undef DESTROY

static void buffer_callback(SLBufferQueueItf buffer_queue, void *context)
{
    struct ao *ao = context;
    struct priv *p = ao->priv;
    SLresult res;
    double delay;

    pthread_mutex_lock(&p->buffer_lock);

    delay = p->frames_per_enqueue / (double)ao->samplerate;
    delay += p->audio_latency;
    ao_read_data(ao, &p->buf, p->frames_per_enqueue,
        mp_time_us() + 1000000LL * delay);

    res = (*buffer_queue)->Enqueue(buffer_queue, p->buf, p->bytes_per_enqueue);
    if (res != SL_RESULT_SUCCESS)
        MP_ERR(ao, "Failed to Enqueue: %d\n", res);

    pthread_mutex_unlock(&p->buffer_lock);
}

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
    SLAndroidDataFormat_PCM_EX pcm;
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
    locator_buffer_queue.numBuffers = 8;

    if (af_fmt_is_int(ao->format)) {
        // Be future-proof
        if (af_fmt_to_bytes(ao->format) > 2)
            ao->format = AF_FORMAT_S32;
        else
            ao->format = af_fmt_from_planar(ao->format);
        pcm.formatType = SL_DATAFORMAT_PCM;
    } else {
        ao->format = AF_FORMAT_FLOAT;
        pcm.formatType = SL_ANDROID_DATAFORMAT_PCM_EX;
        pcm.representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
    }
    pcm.numChannels = ao->channels.num;
    pcm.containerSize = pcm.bitsPerSample = 8 * af_fmt_to_bytes(ao->format);
    pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    pcm.sampleRate = ao->samplerate * 1000;

    if (p->buffer_size_in_ms) {
        ao->device_buffer = ao->samplerate * p->buffer_size_in_ms / 1000;
        // As the purpose of buffer_size_in_ms is to request a specific
        // soft buffer size:
        ao->def_buffer = 0;
    }

    // But it does not make sense if it is smaller than the enqueue size:
    if (p->frames_per_enqueue) {
        ao->device_buffer = MPMAX(ao->device_buffer, p->frames_per_enqueue);
    } else {
        if (ao->device_buffer) {
            p->frames_per_enqueue = ao->device_buffer;
        } else if (ao->def_buffer) {
            p->frames_per_enqueue = ao->def_buffer * ao->samplerate;
        } else {
            MP_ERR(ao, "Enqueue size is not set and can neither be derived\n");
            goto error;
        }
    }

    p->bytes_per_enqueue = p->frames_per_enqueue * ao->channels.num *
        af_fmt_to_bytes(ao->format);
    p->buf = calloc(1, p->bytes_per_enqueue);
    if (!p->buf) {
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

    SLInterfaceID iid_array[] = { SL_IID_BUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
    SLboolean required[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_FALSE };
    CHK((*p->engine)->CreateAudioPlayer(p->engine, &p->player, &audio_source,
        &audio_sink, 2, iid_array, required));

    CHK((*p->player)->Realize(p->player, SL_BOOLEAN_FALSE));
    CHK((*p->player)->GetInterface(p->player, SL_IID_PLAY, (void*)&p->play));
    CHK((*p->player)->GetInterface(p->player, SL_IID_BUFFERQUEUE,
        (void*)&p->buffer_queue));
    CHK((*p->buffer_queue)->RegisterCallback(p->buffer_queue,
        buffer_callback, ao));
    CHK((*p->play)->SetPlayState(p->play, SL_PLAYSTATE_PLAYING));

    SLAndroidConfigurationItf android_config;
    SLuint32 audio_latency = 0, value_size = sizeof(SLuint32);

    SLint32 get_interface_result = (*p->player)->GetInterface(
        p->player,
        SL_IID_ANDROIDCONFIGURATION,
        &android_config
    );

    if (get_interface_result == SL_RESULT_SUCCESS) {
        SLint32 get_configuration_result = (*android_config)->GetConfiguration(
            android_config,
            (const SLchar *)"androidGetAudioLatency",
            &value_size,
            &audio_latency
        );

        if (get_configuration_result == SL_RESULT_SUCCESS) {
            p->audio_latency = (double)audio_latency / 1000.0;
            MP_INFO(ao, "Device latency is %f\n", p->audio_latency);
        }
    }

    return 1;
error:
    uninit(ao);
    return -1;
}

#undef CHK

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    (*p->buffer_queue)->Clear(p->buffer_queue);
}

static void resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    buffer_callback(p->buffer_queue, ao);
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_opensles = {
    .description = "OpenSL ES audio output",
    .name      = "opensles",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .start     = resume,

    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .buffer_size_in_ms = 250,
    },
    .options = (const struct m_option[]) {
        {"frames-per-enqueue", OPT_INT(frames_per_enqueue),
            M_RANGE(1, 96000)},
        {"buffer-size-in-ms", OPT_INT(buffer_size_in_ms),
            M_RANGE(0, 500)},
        {0}
    },
    .options_prefix = "opensles",
};
