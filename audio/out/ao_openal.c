/*
 * OpenAL audio output driver for MPlayer
 *
 * Copyleft 2006 by Reimar Döffinger (Reimar.Doeffinger@stud.uni-karlsruhe.de)
 *
 * This file is part of MPlayer.
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
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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

#include "mpvcore/mp_msg.h"

#include "ao.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "mpvcore/m_option.h"

#define MAX_CHANS MP_NUM_CHANNELS
#define NUM_BUF 128
#define CHUNK_SIZE 512
static ALuint buffers[MAX_CHANS][NUM_BUF];
static ALuint sources[MAX_CHANS];

static int cur_buf[MAX_CHANS];
static int unqueue_buf[MAX_CHANS];
static int16_t *tmpbuf;

static struct ao *ao_data;

struct priv {
    char *cfg_device;
};

static void reset(struct ao *ao);

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
    }
    return CONTROL_UNKNOWN;
}

static int validate_device_opt(const m_option_t *opt, struct bstr name,
                               struct bstr param)
{
    if (bstr_equals0(param, "help")) {
        if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT") != AL_TRUE) {
            mp_msg(MSGT_AO, MSGL_FATAL, "Device listing not supported.\n");
            return M_OPT_EXIT;
        }
        const char *list = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
        mp_msg(MSGT_AO, MSGL_INFO, "OpenAL devices:\n");
        while (list && *list) {
            mp_msg(MSGT_AO, MSGL_INFO, "  '%s'\n", list);
            list = list + strlen(list) + 1;
        }
        return M_OPT_EXIT - 1;
    }
    return 0;
}

struct speaker {
    int id;
    float pos[3];
};

static const struct speaker speaker_pos[] = {
    {MP_SPEAKER_ID_FL,   {-1, 0, 0.5}},
    {MP_SPEAKER_ID_FR,   { 1, 0, 0.5}},
    {MP_SPEAKER_ID_FC,   { 0, 0,   1}},
    {MP_SPEAKER_ID_LFE,  { 0, 0, 0.1}},
    {MP_SPEAKER_ID_BL,   {-1, 0,  -1}},
    {MP_SPEAKER_ID_BR,   { 1, 0,  -1}},
    {MP_SPEAKER_ID_BC,   { 0, 0,  -1}},
    {MP_SPEAKER_ID_SL,   {-1, 0,   0}},
    {MP_SPEAKER_ID_SR,   { 1, 0,   0}},
    {-1},
};

static int init(struct ao *ao)
{
    float position[3] = {0, 0, 0};
    float direction[6] = {0, 0, 1, 0, -1, 0};
    ALCdevice *dev = NULL;
    ALCcontext *ctx = NULL;
    ALCint freq = 0;
    ALCint attribs[] = {ALC_FREQUENCY, ao->samplerate, 0, 0};
    int i;
    struct priv *p = ao->priv;
    if (ao_data) {
        MP_FATAL(ao, "Not reentrant!\n");
        return -1;
    }
    ao_data = ao;
    ao->no_persistent_volume = true;
    struct mp_chmap_sel sel = {0};
    for (i = 0; speaker_pos[i].id != -1; i++)
        mp_chmap_sel_add_speaker(&sel, speaker_pos[i].id);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        goto err_out;
    struct speaker speakers[MAX_CHANS];
    for (i = 0; i < ao->channels.num; i++) {
        speakers[i].id = -1;
        for (int n = 0; speaker_pos[n].id >= 0; n++) {
            if (speaker_pos[n].id == ao->channels.speaker[i])
                speakers[i] = speaker_pos[n];
        }
        if (speakers[i].id < 0) {
            MP_FATAL(ao, "Unknown channel layout\n");
            goto err_out;
        }
    }
    dev = alcOpenDevice(p->cfg_device && p->cfg_device[0] ? p->cfg_device : NULL);
    if (!dev) {
        MP_FATAL(ao, "could not open device\n");
        goto err_out;
    }
    ctx = alcCreateContext(dev, attribs);
    alcMakeContextCurrent(ctx);
    alListenerfv(AL_POSITION, position);
    alListenerfv(AL_ORIENTATION, direction);
    alGenSources(ao->channels.num, sources);
    for (i = 0; i < ao->channels.num; i++) {
        cur_buf[i] = 0;
        unqueue_buf[i] = 0;
        alGenBuffers(NUM_BUF, buffers[i]);
        alSourcefv(sources[i], AL_POSITION, speakers[i].pos);
        alSource3f(sources[i], AL_VELOCITY, 0, 0, 0);
    }
    alcGetIntegerv(dev, ALC_FREQUENCY, 1, &freq);
    if (alcGetError(dev) == ALC_NO_ERROR && freq)
        ao->samplerate = freq;
    ao->format = AF_FORMAT_S16_NE;
    tmpbuf = malloc(CHUNK_SIZE);
    return 0;

err_out:
    return -1;
}

// close audio device
static void uninit(struct ao *ao, bool immed)
{
    ALCcontext *ctx = alcGetCurrentContext();
    ALCdevice *dev = alcGetContextsDevice(ctx);
    free(tmpbuf);
    if (!immed) {
        ALint state;
        alGetSourcei(sources[0], AL_SOURCE_STATE, &state);
        while (state == AL_PLAYING) {
            mp_sleep_us(10000);
            alGetSourcei(sources[0], AL_SOURCE_STATE, &state);
        }
    }
    reset(ao);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
    ao_data = NULL;
}

static void unqueue_buffers(void)
{
    ALint p;
    int s;
    for (s = 0; s < ao_data->channels.num; s++) {
        int till_wrap = NUM_BUF - unqueue_buf[s];
        alGetSourcei(sources[s], AL_BUFFERS_PROCESSED, &p);
        if (p >= till_wrap) {
            alSourceUnqueueBuffers(sources[s], till_wrap,
                                   &buffers[s][unqueue_buf[s]]);
            unqueue_buf[s] = 0;
            p -= till_wrap;
        }
        if (p) {
            alSourceUnqueueBuffers(sources[s], p, &buffers[s][unqueue_buf[s]]);
            unqueue_buf[s] += p;
        }
    }
}

/**
 * \brief stop playing and empty buffers (for seeking/pause)
 */
static void reset(struct ao *ao)
{
    alSourceStopv(ao->channels.num, sources);
    unqueue_buffers();
}

/**
 * \brief stop playing, keep buffers (for pause)
 */
static void audio_pause(struct ao *ao)
{
    alSourcePausev(ao->channels.num, sources);
}

/**
 * \brief resume playing, after audio_pause()
 */
static void audio_resume(struct ao *ao)
{
    alSourcePlayv(ao->channels.num, sources);
}

static int get_space(struct ao *ao)
{
    ALint queued;
    unqueue_buffers();
    alGetSourcei(sources[0], AL_BUFFERS_QUEUED, &queued);
    queued = NUM_BUF - queued - 3;
    if (queued < 0)
        return 0;
    return (queued * CHUNK_SIZE * ao->channels.num) / ao->sstride;
}

/**
 * \brief write data into buffer and reset underrun flag
 */
static int play(struct ao *ao, void **data, int samples, int flags)
{
    ALint state;
    int i, j, k;
    int ch;
    int16_t *d = data[0];
    int len = samples * ao->sstride;
    len /= ao->channels.num * CHUNK_SIZE;
    for (i = 0; i < len; i++) {
        for (ch = 0; ch < ao->channels.num; ch++) {
            for (j = 0, k = ch; j < CHUNK_SIZE / 2; j++, k += ao->channels.num)
                tmpbuf[j] = d[k];
            alBufferData(buffers[ch][cur_buf[ch]], AL_FORMAT_MONO16, tmpbuf,
                         CHUNK_SIZE, ao->samplerate);
            alSourceQueueBuffers(sources[ch], 1, &buffers[ch][cur_buf[ch]]);
            cur_buf[ch] = (cur_buf[ch] + 1) % NUM_BUF;
        }
        d += ao->channels.num * CHUNK_SIZE / 2;
    }
    alGetSourcei(sources[0], AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) // checked here in case of an underrun
        alSourcePlayv(ao->channels.num, sources);
    return len * ao->channels.num * CHUNK_SIZE / ao->sstride;
}

static float get_delay(struct ao *ao)
{
    ALint queued;
    unqueue_buffers();
    alGetSourcei(sources[0], AL_BUFFERS_QUEUED, &queued);
    return queued * CHUNK_SIZE / 2 / (float)ao->samplerate;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_openal = {
    .description = "OpenAL audio output",
    .name      = "openal",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .reset     = reset,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_STRING_VALIDATE("device", cfg_device, 0, validate_device_opt),
        {0}
    },
};
