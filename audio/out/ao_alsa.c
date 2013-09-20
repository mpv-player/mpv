/*
 * ALSA 0.9.x-1.x audio output driver
 *
 * Copyright (C) 2004 Alex Beregszaszi
 *
 * modified for real ALSA 0.9.0 support by Zsolt Barat <joy@streamminister.de>
 * additional AC-3 passthrough support by Andy Lo A Foe <andy@alsaplayer.org>
 * 08/22/2002 iec958-init rewritten and merged with common init, zsolt
 * 04/13/2004 merged with ao_alsa1.x, fixes provided by Jindrich Makovicka
 * 04/25/2004 printfs converted to mp_msg, Zsolt.
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
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <alloca.h>

#include "config.h"
#include "mpvcore/options.h"
#include "mpvcore/m_option.h"
#include "mpvcore/mp_msg.h"

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

#include <alsa/asoundlib.h>

#include "ao.h"
#include "audio/format.h"
#include "audio/reorder_ch.h"

struct priv {
    snd_pcm_t *alsa;
    snd_pcm_format_t alsa_fmt;
    size_t bytes_per_sample;
    int can_pause;
    snd_pcm_sframes_t prepause_frames;
    float delay_before_pause;
    int buffersize;
    int outburst;

    int cfg_block;
    char *cfg_device;
    char *cfg_mixer_device;
    char *cfg_mixer_name;
    int cfg_mixer_index;
};

#define BUFFER_TIME 500000  // 0.5 s
#define FRAGCOUNT 16

#define CHECK_ALSA_ERROR(message) \
    do { \
        if (err < 0) { \
            MP_ERR(ao, "%s: %s\n", (message), snd_strerror(err)); \
            goto alsa_error; \
        } \
    } while (0)

static float get_delay(struct ao *ao);
static int play(struct ao *ao, void *data, int len, int flags);
static void uninit(struct ao *ao, bool immed);

static void alsa_error_handler(const char *file, int line, const char *function,
                               int err, const char *format, ...)
{
    char tmp[0xc00];
    va_list va;

    va_start(va, format);
    vsnprintf(tmp, sizeof tmp, format, va);
    va_end(va);

    if (err) {
        mp_msg(MSGT_AO, MSGL_ERR, "alsa-lib: %s:%i:(%s) %s: %s\n",
               file, line, function, tmp, snd_strerror(err));
    } else {
        mp_msg(MSGT_AO, MSGL_ERR, "alsa-lib: %s:%i:(%s) %s\n",
               file, line, function, tmp);
    }
}

/* to set/get/query special features/parameters */
static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    snd_mixer_t *handle = NULL;
    switch (cmd) {
    case AOCONTROL_GET_MUTE:
    case AOCONTROL_SET_MUTE:
    case AOCONTROL_GET_VOLUME:
    case AOCONTROL_SET_VOLUME:
    {
        int err;
        snd_mixer_elem_t *elem;
        snd_mixer_selem_id_t *sid;

        long pmin, pmax;
        long get_vol, set_vol;
        float f_multi;

        if (AF_FORMAT_IS_IEC61937(ao->format))
            return CONTROL_TRUE;

        //allocate simple id
        snd_mixer_selem_id_alloca(&sid);

        //sets simple-mixer index and name
        snd_mixer_selem_id_set_index(sid, p->cfg_mixer_index);
        snd_mixer_selem_id_set_name(sid, p->cfg_mixer_name);

        err = snd_mixer_open(&handle, 0);
        CHECK_ALSA_ERROR("Mixer open error");

        err = snd_mixer_attach(handle, p->cfg_mixer_device);
        CHECK_ALSA_ERROR("Mixer attach error");

        err = snd_mixer_selem_register(handle, NULL, NULL);
        CHECK_ALSA_ERROR("Mixer register error");

        err = snd_mixer_load(handle);
        CHECK_ALSA_ERROR("Mixer load error");

        elem = snd_mixer_find_selem(handle, sid);
        if (!elem) {
            MP_VERBOSE(ao, "Unable to find simple control '%s',%i.\n",
                       snd_mixer_selem_id_get_name(sid),
                       snd_mixer_selem_id_get_index(sid));
            goto alsa_error;
        }

        snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
        f_multi = (100 / (float)(pmax - pmin));

        switch (cmd) {
        case AOCONTROL_SET_VOLUME: {
            ao_control_vol_t *vol = arg;
            set_vol = vol->left / f_multi + pmin + 0.5;

            //setting channels
            err = snd_mixer_selem_set_playback_volume
                    (elem, SND_MIXER_SCHN_FRONT_LEFT, set_vol);
            CHECK_ALSA_ERROR("Error setting left channel");
            MP_DBG(ao, "left=%li, ", set_vol);

            set_vol = vol->right / f_multi + pmin + 0.5;

            err = snd_mixer_selem_set_playback_volume
                    (elem, SND_MIXER_SCHN_FRONT_RIGHT, set_vol);
            CHECK_ALSA_ERROR("Error setting right channel");
            MP_DBG(ao, "right=%li, pmin=%li, pmax=%li, mult=%f\n",
                   set_vol, pmin, pmax,
                   f_multi);
            break;
        }
        case AOCONTROL_GET_VOLUME: {
            ao_control_vol_t *vol = arg;
            snd_mixer_selem_get_playback_volume
                (elem, SND_MIXER_SCHN_FRONT_LEFT, &get_vol);
            vol->left = (get_vol - pmin) * f_multi;
            snd_mixer_selem_get_playback_volume
                (elem, SND_MIXER_SCHN_FRONT_RIGHT, &get_vol);
            vol->right = (get_vol - pmin) * f_multi;
            MP_DBG(ao, "left=%f, right=%f\n", vol->left, vol->right);
            break;
        }
        case AOCONTROL_SET_MUTE: {
            bool *mute = arg;
            if (!snd_mixer_selem_has_playback_switch(elem))
                goto alsa_error;
            if (!snd_mixer_selem_has_playback_switch_joined(elem)) {
                snd_mixer_selem_set_playback_switch
                    (elem, SND_MIXER_SCHN_FRONT_RIGHT, !*mute);
            }
            snd_mixer_selem_set_playback_switch
                (elem, SND_MIXER_SCHN_FRONT_LEFT, !*mute);
            break;
        }
        case AOCONTROL_GET_MUTE: {
            bool *mute = arg;
            if (!snd_mixer_selem_has_playback_switch(elem))
                goto alsa_error;
            int tmp = 1;
            snd_mixer_selem_get_playback_switch
                (elem, SND_MIXER_SCHN_FRONT_LEFT, &tmp);
            *mute = !tmp;
            if (!snd_mixer_selem_has_playback_switch_joined(elem)) {
                snd_mixer_selem_get_playback_switch
                    (elem, SND_MIXER_SCHN_FRONT_RIGHT, &tmp);
                *mute &= !tmp;
            }
            break;
        }
        }
        snd_mixer_close(handle);
        return CONTROL_OK;
    }

    } //end switch
    return CONTROL_UNKNOWN;

alsa_error:
    if (handle)
        snd_mixer_close(handle);
    return CONTROL_ERROR;
}

static const int mp_to_alsa_format[][2] = {
    {AF_FORMAT_S8,          SND_PCM_FORMAT_S8},
    {AF_FORMAT_U8,          SND_PCM_FORMAT_U8},
    {AF_FORMAT_U16_LE,      SND_PCM_FORMAT_U16_LE},
    {AF_FORMAT_U16_BE,      SND_PCM_FORMAT_U16_BE},
    {AF_FORMAT_S16_LE,      SND_PCM_FORMAT_S16_LE},
    {AF_FORMAT_S16_BE,      SND_PCM_FORMAT_S16_BE},
    {AF_FORMAT_U32_LE,      SND_PCM_FORMAT_U32_LE},
    {AF_FORMAT_U32_BE,      SND_PCM_FORMAT_U32_BE},
    {AF_FORMAT_S32_LE,      SND_PCM_FORMAT_S32_LE},
    {AF_FORMAT_S32_BE,      SND_PCM_FORMAT_S32_BE},
    {AF_FORMAT_U24_LE,      SND_PCM_FORMAT_U24_3LE},
    {AF_FORMAT_U24_BE,      SND_PCM_FORMAT_U24_3BE},
    {AF_FORMAT_S24_LE,      SND_PCM_FORMAT_S24_3LE},
    {AF_FORMAT_S24_BE,      SND_PCM_FORMAT_S24_3BE},
    {AF_FORMAT_FLOAT_LE,    SND_PCM_FORMAT_FLOAT_LE},
    {AF_FORMAT_FLOAT_BE,    SND_PCM_FORMAT_FLOAT_BE},
    {AF_FORMAT_AC3_LE,      SND_PCM_FORMAT_S16_LE},
    {AF_FORMAT_AC3_BE,      SND_PCM_FORMAT_S16_BE},
    {AF_FORMAT_IEC61937_LE, SND_PCM_FORMAT_S16_LE},
    {AF_FORMAT_IEC61937_BE, SND_PCM_FORMAT_S16_BE},
    {AF_FORMAT_MPEG2,       SND_PCM_FORMAT_MPEG},
    {AF_FORMAT_UNKNOWN,     SND_PCM_FORMAT_UNKNOWN},
};

static int find_alsa_format(int af_format)
{
    for (int n = 0; mp_to_alsa_format[n][0] != AF_FORMAT_UNKNOWN; n++) {
        if (mp_to_alsa_format[n][0] == af_format)
            return mp_to_alsa_format[n][1];
    }
    return SND_PCM_FORMAT_UNKNOWN;
}

// Lists device names and their implied channel map.
// The second item must be resolvable with mp_chmap_from_str().
// Source: http://www.alsa-project.org/main/index.php/DeviceNames
// (Speaker names are slightly different from mpv's.)
static const char *device_channel_layouts[][2] = {
    {"default",         "fc"},
    {"default",         "fl-fr"},
    {"rear",            "bl-br"},
    {"center_lfe",      "fc-lfe"},
    {"side",            "sl-sr"},
    {"surround40",      "fl-fr-bl-br"},
    {"surround50",      "fl-fr-bl-br-fc"},
    {"surround41",      "fl-fr-bl-br-lfe"},
    {"surround51",      "fl-fr-bl-br-fc-lfe"},
    {"surround71",      "fl-fr-bl-br-fc-lfe-sl-sr"},
};

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

#define NUM_ALSA_CHMAPS ARRAY_LEN(device_channel_layouts)

static const char *select_chmap(struct ao *ao)
{
    struct mp_chmap_sel sel = {0};
    struct mp_chmap maps[NUM_ALSA_CHMAPS];
    for (int n = 0; n < NUM_ALSA_CHMAPS; n++) {
        mp_chmap_from_str(&maps[n], bstr0(device_channel_layouts[n][1]));
        mp_chmap_sel_add_map(&sel, &maps[n]);
    };

    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        return NULL;

    for (int n = 0; n < NUM_ALSA_CHMAPS; n++) {
        if (mp_chmap_equals(&ao->channels, &maps[n]))
            return device_channel_layouts[n][0];
    }

    char *name = mp_chmap_to_str(&ao->channels);
    MP_ERR(ao, "channel layout %s (%d ch) not supported.\n",
           name, ao->channels.num);
    talloc_free(name);
    return "default";
}

static int try_open_device(struct ao *ao, const char *device, int open_mode,
                           int try_ac3)
{
    struct priv *p = ao->priv;
    int err, len;
    char *ac3_device, *args;

    if (try_ac3) {
        /* to set the non-audio bit, use AES0=6 */
        len = strlen(device);
        ac3_device = malloc(len + 7 + 1);
        if (!ac3_device)
            return -ENOMEM;
        strcpy(ac3_device, device);
        args = strchr(ac3_device, ':');
        if (!args) {
            /* no existing parameters: add it behind device name */
            strcat(ac3_device, ":AES0=6");
        } else {
            do {
                ++args;
            } while (isspace(*args));
            if (*args == '\0') {
                /* ":" but no parameters */
                strcat(ac3_device, "AES0=6");
            } else if (*args != '{') {
                /* a simple list of parameters: add it at the end of the list */
                strcat(ac3_device, ",AES0=6");
            } else {
                /* parameters in config syntax: add it inside the { } block */
                do {
                    --len;
                } while (len > 0 && isspace(ac3_device[len]));
                if (ac3_device[len] == '}')
                    strcpy(ac3_device + len, " AES0=6}");
            }
        }
        err = snd_pcm_open
                (&p->alsa, ac3_device, SND_PCM_STREAM_PLAYBACK, open_mode);
        free(ac3_device);
        if (!err)
            return 0;
    }
    return snd_pcm_open
            (&p->alsa, device, SND_PCM_STREAM_PLAYBACK, open_mode);
}

/*
    open & setup audio device
    return: 0=success -1=fail
 */
static int init(struct ao *ao)
{
    int err;
    snd_pcm_uframes_t chunk_size;
    snd_pcm_uframes_t bufsize;
    snd_pcm_uframes_t boundary;

    struct priv *p = ao->priv;

    MP_VERBOSE(ao, "requested format: %d Hz, %d channels, %x\n",
               ao->samplerate, ao->channels.num, ao->format);

    p->prepause_frames = 0;
    p->delay_before_pause = 0;

    /* switch for spdif
     * sets opening sequence for SPDIF
     * sets also the playback and other switches 'on the fly'
     * while opening the abstract alias for the spdif subdevice
     * 'iec958'
     */
    const char *device;
    if (AF_FORMAT_IS_IEC61937(ao->format)) {
        device = "iec958";
        MP_VERBOSE(ao, "playing AC3/iec61937/iec958, %i channels\n",
                   ao->channels.num);
    } else {
        device = select_chmap(ao);
        if (strcmp(device, "default") != 0 && ao->format == AF_FORMAT_FLOAT_NE)
        {
            // hack - use the converter plugin (why the heck?)
            device = talloc_asprintf(ao, "plug:%s", device);
        }
    }
    if (p->cfg_device && p->cfg_device[0])
        device = p->cfg_device;

    MP_VERBOSE(ao, "using device: %s\n", device);

    p->can_pause = 1;

    MP_VERBOSE(ao, "using ALSA version: %s\n", snd_asoundlib_version());
    snd_lib_error_set_handler(alsa_error_handler);

    int open_mode = p->cfg_block ? 0 : SND_PCM_NONBLOCK;
    int isac3 =  AF_FORMAT_IS_IEC61937(ao->format);
    //modes = 0, SND_PCM_NONBLOCK, SND_PCM_ASYNC
    err = try_open_device(ao, device, open_mode, isac3);
    if (err < 0) {
        if (err != -EBUSY && !p->cfg_block) {
            MP_WARN(ao, "Open in nonblock-mode "
                    "failed, trying to open in block-mode.\n");
            err = try_open_device(ao, device, 0, isac3);
        }
        CHECK_ALSA_ERROR("Playback open error");
    }

    err = snd_pcm_nonblock(p->alsa, 0);
    if (err < 0) {
        MP_ERR(ao, "Error setting block-mode: %s.\n", snd_strerror(err));
    } else {
        MP_VERBOSE(ao, "pcm opened in blocking mode\n");
    }

    snd_pcm_hw_params_t *alsa_hwparams;
    snd_pcm_sw_params_t *alsa_swparams;

    snd_pcm_hw_params_alloca(&alsa_hwparams);
    snd_pcm_sw_params_alloca(&alsa_swparams);

    // setting hw-parameters
    err = snd_pcm_hw_params_any(p->alsa, alsa_hwparams);
    CHECK_ALSA_ERROR("Unable to get initial parameters");

    err = snd_pcm_hw_params_set_access
            (p->alsa, alsa_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    CHECK_ALSA_ERROR("Unable to set access type");

    p->alsa_fmt = find_alsa_format(ao->format);
    if (p->alsa_fmt == SND_PCM_FORMAT_UNKNOWN) {
        p->alsa_fmt = SND_PCM_FORMAT_S16;
        ao->format = AF_FORMAT_S16_NE;
    }

    err = snd_pcm_hw_params_test_format(p->alsa, alsa_hwparams, p->alsa_fmt);
    if (err < 0) {
        MP_INFO(ao, "Format %s is not supported by hardware, trying default.\n",
                af_fmt2str_short(ao->format));
        p->alsa_fmt = SND_PCM_FORMAT_S16_LE;
        if (AF_FORMAT_IS_AC3(ao->format))
            ao->format = AF_FORMAT_AC3_LE;
        else if (AF_FORMAT_IS_IEC61937(ao->format))
            ao->format = AF_FORMAT_IEC61937_LE;
        else
            ao->format = AF_FORMAT_S16_LE;
    }

    err = snd_pcm_hw_params_set_format(p->alsa, alsa_hwparams, p->alsa_fmt);
    CHECK_ALSA_ERROR("Unable to set format");

    int num_channels = ao->channels.num;
    err = snd_pcm_hw_params_set_channels_near
            (p->alsa, alsa_hwparams, &num_channels);
    CHECK_ALSA_ERROR("Unable to set channels");

    if (num_channels != ao->channels.num) {
        MP_ERR(ao, "Couldn't get requested number of channels.\n");
        mp_chmap_from_channels_alsa(&ao->channels, num_channels);
    }

    /* workaround for buggy rate plugin (should be fixed in ALSA 1.0.11)
        prefer our own resampler, since that allows users to choose the resampler,
        even per file if desired */
    err = snd_pcm_hw_params_set_rate_resample(p->alsa, alsa_hwparams, 0);
    CHECK_ALSA_ERROR("Unable to disable resampling");

    err = snd_pcm_hw_params_set_rate_near
            (p->alsa, alsa_hwparams, &ao->samplerate, NULL);
    CHECK_ALSA_ERROR("Unable to set samplerate-2");

    p->bytes_per_sample = af_fmt2bits(ao->format) / 8;
    p->bytes_per_sample *= ao->channels.num;

    err = snd_pcm_hw_params_set_buffer_time_near
            (p->alsa, alsa_hwparams, &(unsigned int){BUFFER_TIME}, NULL);
    CHECK_ALSA_ERROR("Unable to set buffer time near");

    err = snd_pcm_hw_params_set_periods_near
            (p->alsa, alsa_hwparams, &(unsigned int){FRAGCOUNT}, NULL);
    CHECK_ALSA_ERROR("Unable to set periods");

    /* finally install hardware parameters */
    err = snd_pcm_hw_params(p->alsa, alsa_hwparams);
    CHECK_ALSA_ERROR("Unable to set hw-parameters");

    // end setting hw-params

    // gets buffersize for control
    err = snd_pcm_hw_params_get_buffer_size(alsa_hwparams, &bufsize);
    CHECK_ALSA_ERROR("Unable to get buffersize");

    p->buffersize = bufsize * p->bytes_per_sample;
    MP_VERBOSE(ao, "got buffersize=%i\n", p->buffersize);

    err = snd_pcm_hw_params_get_period_size(alsa_hwparams, &chunk_size, NULL);
    CHECK_ALSA_ERROR("Unable to get period size");

    MP_VERBOSE(ao, "got period size %li\n", chunk_size);
    p->outburst = chunk_size * p->bytes_per_sample;

    /* setting software parameters */
    err = snd_pcm_sw_params_current(p->alsa, alsa_swparams);
    CHECK_ALSA_ERROR("Unable to get sw-parameters");

    err = snd_pcm_sw_params_get_boundary(alsa_swparams, &boundary);
    CHECK_ALSA_ERROR("Unable to get boundary");

    /* start playing when one period has been written */
    err = snd_pcm_sw_params_set_start_threshold
            (p->alsa, alsa_swparams, chunk_size);
    CHECK_ALSA_ERROR("Unable to set start threshold");

    /* disable underrun reporting */
    err = snd_pcm_sw_params_set_stop_threshold
            (p->alsa, alsa_swparams, boundary);
    CHECK_ALSA_ERROR("Unable to set stop threshold");

    /* play silence when there is an underrun */
    err = snd_pcm_sw_params_set_silence_size
            (p->alsa, alsa_swparams, boundary);
    CHECK_ALSA_ERROR("Unable to set silence size");

    err = snd_pcm_sw_params(p->alsa, alsa_swparams);
    CHECK_ALSA_ERROR("Unable to get sw-parameters");

    /* end setting sw-params */

    p->can_pause = snd_pcm_hw_params_can_pause(alsa_hwparams);

    MP_VERBOSE(ao, "opened: %d Hz/%d channels/%d bpf/%d bytes buffer/%s\n",
               ao->samplerate, ao->channels.num, (int)p->bytes_per_sample,
               p->buffersize, snd_pcm_format_description(p->alsa_fmt));

    return 0;

alsa_error:
    uninit(ao, true);
    return -1;
} // end init


/* close audio device */
static void uninit(struct ao *ao, bool immed)
{
    struct priv *p = ao->priv;

    if (p->alsa) {
        int err;

        if (!immed)
            snd_pcm_drain(p->alsa);

        err = snd_pcm_close(p->alsa);
        CHECK_ALSA_ERROR("pcm close error");

        MP_VERBOSE(ao, "uninit: pcm closed\n");
    }

alsa_error:
    p->alsa = NULL;
    snd_lib_error_set_handler(NULL);
}

static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;
    int err;

    if (p->can_pause) {
        p->delay_before_pause = get_delay(ao);
        err = snd_pcm_pause(p->alsa, 1);
        CHECK_ALSA_ERROR("pcm pause error");
    } else {
        MP_VERBOSE(ao, "pause not supported by hardware\n");
        if (snd_pcm_delay(p->alsa, &p->prepause_frames) < 0
            || p->prepause_frames < 0)
            p->prepause_frames = 0;
        p->delay_before_pause = p->prepause_frames / (float)ao->samplerate;

        err = snd_pcm_drop(p->alsa);
        CHECK_ALSA_ERROR("pcm drop error");
    }

alsa_error: ;
}

static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    int err;

    if (snd_pcm_state(p->alsa) == SND_PCM_STATE_SUSPENDED) {
        MP_INFO(ao, "PCM in suspend mode, trying to resume.\n");
        while ((err = snd_pcm_resume(p->alsa)) == -EAGAIN)
            sleep(1);
    }
    if (p->can_pause) {
        err = snd_pcm_pause(p->alsa, 0);
        CHECK_ALSA_ERROR("pcm resume error");
    } else {
        MP_VERBOSE(ao, "resume not supported by hardware\n");
        err = snd_pcm_prepare(p->alsa);
        CHECK_ALSA_ERROR("pcm prepare error");
        if (p->prepause_frames) {
            void *silence = calloc(p->prepause_frames, p->bytes_per_sample);
            play(ao, silence, p->prepause_frames * p->bytes_per_sample, 0);
            free(silence);
        }
    }

alsa_error: ;
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    int err;

    p->prepause_frames = 0;
    p->delay_before_pause = 0;
    err = snd_pcm_drop(p->alsa);
    CHECK_ALSA_ERROR("pcm prepare error");
    err = snd_pcm_prepare(p->alsa);
    CHECK_ALSA_ERROR("pcm prepare error");

alsa_error: ;
}

/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
    modified last at 29.06.02 by jp
    thanxs for marius <marius@rospot.com> for giving us the light ;)
 */

static int play(struct ao *ao, void *data, int len, int flags)
{
    struct priv *p = ao->priv;
    int num_frames;
    snd_pcm_sframes_t res = 0;
    if (!(flags & AOPLAY_FINAL_CHUNK))
        len = len / p->outburst * p->outburst;
    num_frames = len / p->bytes_per_sample;

    if (!p->alsa) {
        MP_ERR(ao, "Device configuration error.");
        return 0;
    }

    if (num_frames == 0)
        return 0;

    do {
        res = snd_pcm_writei(p->alsa, data, num_frames);

        if (res == -EINTR) {
            /* nothing to do */
            res = 0;
        } else if (res == -ESTRPIPE) {  /* suspend */
            MP_INFO(ao, "PCM in suspend mode, trying to resume.\n");
            while ((res = snd_pcm_resume(p->alsa)) == -EAGAIN)
                sleep(1);
        }
        if (res < 0) {
            MP_ERR(ao, "Write error: %s\n", snd_strerror(res));
            res = snd_pcm_prepare(p->alsa);
            int err = res;
            CHECK_ALSA_ERROR("pcm prepare error");
            res = 0;
        }
    } while (res == 0);

    return res < 0 ? 0 : res * p->bytes_per_sample;

alsa_error:
    return 0;
}

/* how many byes are free in the buffer */
static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;
    snd_pcm_status_t *status;
    int err;

    snd_pcm_status_alloca(&status);

    err = snd_pcm_status(p->alsa, status);
    CHECK_ALSA_ERROR("cannot get pcm status");

    unsigned space = snd_pcm_status_get_avail(status) * p->bytes_per_sample;
    if (space > p->buffersize) // Buffer underrun?
        space = p->buffersize;
    return space;

alsa_error:
    return 0;
}

/* delay in seconds between first and last sample in buffer */
static float get_delay(struct ao *ao)
{
    struct priv *p = ao->priv;
    if (p->alsa) {
        snd_pcm_sframes_t delay;

        if (snd_pcm_state(p->alsa) == SND_PCM_STATE_PAUSED)
            return p->delay_before_pause;

        if (snd_pcm_delay(p->alsa, &delay) < 0)
            return 0;

        if (delay < 0) {
            /* underrun - move the application pointer forward to catch up */
            snd_pcm_forward(p->alsa, -delay);
            delay = 0;
        }
        return (float)delay / (float)ao->samplerate;
    } else
        return 0;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_alsa = {
    .info = &(const struct ao_info) {
        "ALSA-0.9.x-1.x audio output",
        "alsa",
        "Alex Beregszaszi, Zsolt Barat <joy@streamminister.de>",
        "under development"
    },
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
    .priv_defaults = &(const struct priv) {
        .cfg_block = 1,
        .cfg_mixer_device = "default",
        .cfg_mixer_name = "Master",
        .cfg_mixer_index = 0,
    },
    .options = (const struct m_option[]) {
        OPT_STRING("device", cfg_device, 0),
        OPT_FLAG("block", cfg_block, 0),
        OPT_STRING("mixer-device", cfg_mixer_device, 0),
        OPT_STRING("mixer-name", cfg_mixer_name, 0),
        OPT_INTRANGE("mixer-index", cfg_mixer_index, 0, 0, 99),
        {0}
    },
};
