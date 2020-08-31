/*
 * OpenAL audio output driver for MPlayer
 *
 * Copyleft 2006 by Reimar Döffinger (Reimar.Doeffinger@stud.uni-karlsruhe.de)
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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#ifdef OPENAL_AL_H
#include <OpenAL/alc.h>
#include <OpenAL/al.h>
#include <OpenAL/alext.h>
#else
#include <AL/alc.h>
#include <AL/al.h>
#include <AL/alext.h>
#endif

#include "common/msg.h"

#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "options/m_option.h"

#define MAX_CHANS MP_NUM_CHANNELS
#define MAX_BUF 128
#define MAX_SAMPLES 32768
static ALuint buffers[MAX_BUF];
static ALuint buffer_size[MAX_BUF];
static ALuint source;

static int cur_buf;
static int unqueue_buf;

static struct ao *ao_data;

struct priv {
    ALenum al_format;
    int num_buffers;
    int num_samples;
    int direct_channels;
};

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
    case AOCONTROL_SET_VOLUME: {
        ALfloat volume;
        ao_control_vol_t *vol = (ao_control_vol_t *)arg;
        if (cmd == AOCONTROL_SET_VOLUME) {
            volume = (vol->left + vol->right) / 200.0;
            alListenerf(AL_GAIN, volume);
        }
        alGetListenerf(AL_GAIN, &volume);
        vol->left = vol->right = volume * 100;
        return CONTROL_TRUE;
    }
    case AOCONTROL_GET_MUTE:
    case AOCONTROL_SET_MUTE: {
        bool mute = *(bool *)arg;

        // openal has no mute control, only gain.
        // Thus reverse the muted state to get required gain
        ALfloat al_mute = (ALfloat)(!mute);
        if (cmd == AOCONTROL_SET_MUTE) {
            alSourcef(source, AL_GAIN, al_mute);
        }
        alGetSourcef(source, AL_GAIN, &al_mute);
        *(bool *)arg = !((bool)al_mute);
        return CONTROL_TRUE;
    }

    }
    return CONTROL_UNKNOWN;
}

static enum af_format get_supported_format(int format)
{
    switch (format) {
    case AF_FORMAT_U8:
        if (alGetEnumValue((ALchar*)"AL_FORMAT_MONO8"))
            return AF_FORMAT_U8;
        break;

    case AF_FORMAT_S16:
        if (alGetEnumValue((ALchar*)"AL_FORMAT_MONO16"))
            return AF_FORMAT_S16;
        break;

    case AF_FORMAT_S32:
        if (strstr(alGetString(AL_RENDERER), "X-Fi") != NULL)
            return AF_FORMAT_S32;
        break;

    case AF_FORMAT_FLOAT:
        if (alIsExtensionPresent((ALchar*)"AL_EXT_float32") == AL_TRUE)
            return AF_FORMAT_FLOAT;
        break;
    }
    return AL_FALSE;
}

static ALenum get_supported_layout(int format, int channels)
{
    const char *channel_str[] = {
        [1] = "MONO",
        [2] = "STEREO",
        [4] = "QUAD",
        [6] = "51CHN",
        [7] = "61CHN",
        [8] = "71CHN",
    };
    const char *format_str[] = {
        [AF_FORMAT_U8] = "8",
        [AF_FORMAT_S16] = "16",
        [AF_FORMAT_S32] = "32",
        [AF_FORMAT_FLOAT] = "_FLOAT32",
    };
    if (channel_str[channels] == NULL || format_str[format] == NULL)
        return AL_FALSE;

    char enum_name[32];
    // AF_FORMAT_FLOAT uses same enum name as AF_FORMAT_S32 for multichannel
    // playback, while it is different for mono and stereo.
    // OpenAL Soft does not support AF_FORMAT_S32 and seems to reuse the names.
    if (channels > 2 && format == AF_FORMAT_FLOAT)
        format = AF_FORMAT_S32;
    snprintf(enum_name, sizeof(enum_name), "AL_FORMAT_%s%s", channel_str[channels],
             format_str[format]);

    if (alGetEnumValue((ALchar*)enum_name)) {
        return alGetEnumValue((ALchar*)enum_name);
    }
    return AL_FALSE;
}

// close audio device
static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    alSourceStop(source);
    alSourcei(source, AL_BUFFER, 0);

    alDeleteBuffers(p->num_buffers, buffers);
    alDeleteSources(1, &source);

    ALCcontext *ctx = alcGetCurrentContext();
    ALCdevice *dev = alcGetContextsDevice(ctx);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
    ao_data = NULL;
}

static int init(struct ao *ao)
{
    float position[3] = {0, 0, 0};
    float direction[6] = {0, 0, -1, 0, 1, 0};
    ALCdevice *dev = NULL;
    ALCcontext *ctx = NULL;
    ALCint freq = 0;
    ALCint attribs[] = {ALC_FREQUENCY, ao->samplerate, 0, 0};
    struct priv *p = ao->priv;
    if (ao_data) {
        MP_FATAL(ao, "Not reentrant!\n");
        return -1;
    }
    ao_data = ao;
    char *dev_name = ao->device;
    dev = alcOpenDevice(dev_name && dev_name[0] ? dev_name : NULL);
    if (!dev) {
        MP_FATAL(ao, "could not open device\n");
        goto err_out;
    }
    ctx = alcCreateContext(dev, attribs);
    alcMakeContextCurrent(ctx);
    alListenerfv(AL_POSITION, position);
    alListenerfv(AL_ORIENTATION, direction);

    alGenSources(1, &source);
    if (p->direct_channels && alGetEnumValue((ALchar*)"AL_DIRECT_CHANNELS_SOFT")) {
        alSourcei(source, alGetEnumValue((ALchar*)"AL_DIRECT_CHANNELS_SOFT"), AL_TRUE);
    }

    cur_buf = 0;
    unqueue_buf = 0;
    for (int i = 0; i < p->num_buffers; ++i) {
        buffer_size[i] = 0;
    }

    alGenBuffers(p->num_buffers, buffers);

    alcGetIntegerv(dev, ALC_FREQUENCY, 1, &freq);
    if (alcGetError(dev) == ALC_NO_ERROR && freq)
        ao->samplerate = freq;

    // Check sample format
    int try_formats[AF_FORMAT_COUNT + 1];
    enum af_format sample_format = 0;
    af_get_best_sample_formats(ao->format, try_formats);
    for (int n = 0; try_formats[n]; n++) {
        sample_format = get_supported_format(try_formats[n]);
        if (sample_format != AF_FORMAT_UNKNOWN) {
            ao->format = try_formats[n];
            break;
        }
    }

    if (sample_format == AF_FORMAT_UNKNOWN) {
        MP_FATAL(ao, "Can't find appropriate sample format.\n");
        uninit(ao);
        goto err_out;
    }

    // Check if OpenAL driver supports the desired number of channels.
    int num_channels = ao->channels.num;
    do {
        p->al_format = get_supported_layout(sample_format, num_channels);
        if (p->al_format == AL_FALSE) {
            num_channels = num_channels - 1;
        }
    } while (p->al_format == AL_FALSE && num_channels > 1);

    // Request number of speakers for output from ao.
    const struct mp_chmap possible_layouts[] = {
        {0},                                        // empty
        MP_CHMAP_INIT_MONO,                         // mono
        MP_CHMAP_INIT_STEREO,                       // stereo
        {0},                                        // 2.1
        MP_CHMAP4(FL, FR, BL, BR),                  // 4.0
        {0},                                        // 5.0
        MP_CHMAP6(FL, FR, FC, LFE, BL, BR),         // 5.1
        MP_CHMAP7(FL, FR, FC, LFE, SL, SR, BC),     // 6.1
        MP_CHMAP8(FL, FR, FC, LFE, BL, BR, SL, SR), // 7.1
    };
    ao->channels = possible_layouts[num_channels];
    if (!ao->channels.num)
        mp_chmap_set_unknown(&ao->channels, num_channels);

    if (p->al_format == AL_FALSE || !mp_chmap_is_valid(&ao->channels)) {
        MP_FATAL(ao, "Can't find appropriate channel layout.\n");
        uninit(ao);
        goto err_out;
    }

    ao->device_buffer = p->num_buffers * p->num_samples;
    return 0;

err_out:
    ao_data = NULL;
    return -1;
}

static void unqueue_buffers(struct ao *ao)
{
    struct priv *q = ao->priv;
    ALint p;
    int till_wrap = q->num_buffers - unqueue_buf;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &p);
    if (p >= till_wrap) {
        alSourceUnqueueBuffers(source, till_wrap, &buffers[unqueue_buf]);
        unqueue_buf = 0;
        p -= till_wrap;
    }
    if (p) {
        alSourceUnqueueBuffers(source, p, &buffers[unqueue_buf]);
        unqueue_buf += p;
    }
}

static void reset(struct ao *ao)
{
    alSourceStop(source);
    unqueue_buffers(ao);
}

static bool audio_set_pause(struct ao *ao, bool pause)
{
    if (pause) {
        alSourcePause(source);
    } else {
        alSourcePlay(source);
    }
    return true;
}

static bool audio_write(struct ao *ao, void **data, int samples)
{
    struct priv *p = ao->priv;

    int num = (samples + p->num_samples - 1) / p->num_samples;

    for (int i = 0; i < num; i++) {
        char *d = *data;
        buffer_size[cur_buf] =
            MPMIN(samples - i * p->num_samples, p->num_samples);
        d += i * buffer_size[cur_buf] * ao->sstride;
        alBufferData(buffers[cur_buf], p->al_format, d,
            buffer_size[cur_buf] * ao->sstride, ao->samplerate);
        alSourceQueueBuffers(source, 1, &buffers[cur_buf]);
        cur_buf = (cur_buf + 1) % p->num_buffers;
    }

    return true;
}

static void audio_start(struct ao *ao)
{
    alSourcePlay(source);
}

static void get_state(struct ao *ao, struct mp_pcm_state *state)
{
    struct priv *p = ao->priv;

    ALint queued;
    unqueue_buffers(ao);
    alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);

    double source_offset = 0;
    if(alIsExtensionPresent("AL_SOFT_source_latency")) {
        ALdouble offsets[2];
        LPALGETSOURCEDVSOFT alGetSourcedvSOFT = alGetProcAddress("alGetSourcedvSOFT");
        alGetSourcedvSOFT(source, AL_SEC_OFFSET_LATENCY_SOFT, offsets);
        // Additional latency to the play buffer, the remaining seconds to be
        // played minus the offset (seconds already played)
        source_offset = offsets[1] - offsets[0];
    } else {
        float offset = 0;
        alGetSourcef(source, AL_SEC_OFFSET, &offset);
        source_offset = -offset;
    }

    int queued_samples = 0;
    for (int i = 0, index = cur_buf; i < queued; ++i) {
        queued_samples += buffer_size[index];
        index = (index + 1) % p->num_buffers;
    }

    state->delay = queued_samples / (double)ao->samplerate + source_offset;

    state->queued_samples = queued_samples;
    state->free_samples = MPMAX(p->num_buffers - queued, 0) * p->num_samples;

    ALint source_state = 0;
    alGetSourcei(source, AL_SOURCE_STATE, &source_state);
    state->playing = source_state == AL_PLAYING;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_openal = {
    .description = "OpenAL audio output",
    .name      = "openal",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .get_state = get_state,
    .write     = audio_write,
    .start     = audio_start,
    .set_pause = audio_set_pause,
    .reset     = reset,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .num_buffers = 4,
        .num_samples = 8192,
        .direct_channels = 0,
    },
    .options = (const struct m_option[]) {
        {"num-buffers", OPT_INT(num_buffers), M_RANGE(2, MAX_BUF)},
        {"num-samples", OPT_INT(num_samples), M_RANGE(256, MAX_SAMPLES)},
        {"direct-channels", OPT_FLAG(direct_channels)},
        {0}
    },
    .options_prefix = "openal",
};
