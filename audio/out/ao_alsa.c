/*
 * ALSA 0.9.x-1.x audio output driver
 *
 * Copyright (C) 2004 Alex Beregszaszi
 * Zsolt Barat <joy@streamminister.de>
 *
 * modified for real ALSA 0.9.0 support by Zsolt Barat <joy@streamminister.de>
 * additional AC-3 passthrough support by Andy Lo A Foe <andy@alsaplayer.org>
 * 08/22/2002 iec958-init rewritten and merged with common init, zsolt
 * 04/13/2004 merged with ao_alsa1.x, fixes provided by Jindrich Makovicka
 * 04/25/2004 printfs converted to mp_msg, Zsolt.
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

#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "options/options.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "common/msg.h"
#include "osdep/endian.h"

#include <alsa/asoundlib.h>

#if defined(SND_CHMAP_API_VERSION) && SND_CHMAP_API_VERSION >= (1 << 16)
#define HAVE_CHMAP_API 1
#else
#define HAVE_CHMAP_API 0
#endif

#include "ao.h"
#include "internal.h"
#include "audio/format.h"

struct ao_alsa_opts {
    char *mixer_device;
    char *mixer_name;
    int mixer_index;
    int resample;
    int ni;
    int ignore_chmap;
    int buffer_time;
    int frags;
};

#define OPT_BASE_STRUCT struct ao_alsa_opts
static const struct m_sub_options ao_alsa_conf = {
    .opts = (const struct m_option[]) {
        {"alsa-resample", OPT_FLAG(resample)},
        {"alsa-mixer-device", OPT_STRING(mixer_device)},
        {"alsa-mixer-name", OPT_STRING(mixer_name)},
        {"alsa-mixer-index", OPT_INT(mixer_index), M_RANGE(0, 99)},
        {"alsa-non-interleaved", OPT_FLAG(ni)},
        {"alsa-ignore-chmap", OPT_FLAG(ignore_chmap)},
        {"alsa-buffer-time", OPT_INT(buffer_time), M_RANGE(0, INT_MAX)},
        {"alsa-periods", OPT_INT(frags), M_RANGE(0, INT_MAX)},
        {0}
    },
    .defaults = &(const struct ao_alsa_opts) {
        .mixer_device = "default",
        .mixer_name = "Master",
        .mixer_index = 0,
        .ni = 0,
        .buffer_time = 100000,
        .frags = 4,
    },
    .size = sizeof(struct ao_alsa_opts),
};

struct priv {
    snd_pcm_t *alsa;
    bool device_lost;
    snd_pcm_format_t alsa_fmt;
    bool can_pause;
    snd_pcm_uframes_t buffersize;
    snd_pcm_uframes_t outburst;

    snd_output_t *output;

    struct ao_convert_fmt convert;

    struct ao_alsa_opts *opts;
};

#define CHECK_ALSA_ERROR(message) \
    do { \
        if (err < 0) { \
            MP_ERR(ao, "%s: %s\n", (message), snd_strerror(err)); \
            goto alsa_error; \
        } \
    } while (0)

#define CHECK_ALSA_WARN(message) \
    do { \
        if (err < 0) \
            MP_WARN(ao, "%s: %s\n", (message), snd_strerror(err)); \
    } while (0)

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

        if (!af_fmt_is_pcm(ao->format))
            return CONTROL_FALSE;

        snd_mixer_selem_id_alloca(&sid);

        snd_mixer_selem_id_set_index(sid, p->opts->mixer_index);
        snd_mixer_selem_id_set_name(sid, p->opts->mixer_name);

        err = snd_mixer_open(&handle, 0);
        CHECK_ALSA_ERROR("Mixer open error");

        err = snd_mixer_attach(handle, p->opts->mixer_device);
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

            err = snd_mixer_selem_set_playback_volume(elem, 0, set_vol);
            CHECK_ALSA_ERROR("Error setting left channel");
            MP_DBG(ao, "left=%li, ", set_vol);

            set_vol = vol->right / f_multi + pmin + 0.5;

            err = snd_mixer_selem_set_playback_volume(elem, 1, set_vol);
            CHECK_ALSA_ERROR("Error setting right channel");
            MP_DBG(ao, "right=%li, pmin=%li, pmax=%li, mult=%f\n",
                   set_vol, pmin, pmax, f_multi);
            break;
        }
        case AOCONTROL_GET_VOLUME: {
            ao_control_vol_t *vol = arg;
            snd_mixer_selem_get_playback_volume(elem, 0, &get_vol);
            vol->left = (get_vol - pmin) * f_multi;
            snd_mixer_selem_get_playback_volume(elem, 1, &get_vol);
            vol->right = (get_vol - pmin) * f_multi;
            MP_DBG(ao, "left=%f, right=%f\n", vol->left, vol->right);
            break;
        }
        case AOCONTROL_SET_MUTE: {
            bool *mute = arg;
            if (!snd_mixer_selem_has_playback_switch(elem))
                goto alsa_error;
            if (!snd_mixer_selem_has_playback_switch_joined(elem)) {
                snd_mixer_selem_set_playback_switch(elem, 1, !*mute);
            }
            snd_mixer_selem_set_playback_switch(elem, 0, !*mute);
            break;
        }
        case AOCONTROL_GET_MUTE: {
            bool *mute = arg;
            if (!snd_mixer_selem_has_playback_switch(elem))
                goto alsa_error;
            int tmp = 1;
            snd_mixer_selem_get_playback_switch(elem, 0, &tmp);
            *mute = !tmp;
            if (!snd_mixer_selem_has_playback_switch_joined(elem)) {
                snd_mixer_selem_get_playback_switch(elem, 1, &tmp);
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

struct alsa_fmt {
    int mp_format;
    int alsa_format;
    int bits;           // alsa format full sample size (optional)
    int pad_msb;        // how many MSB bits are 0 (optional)
};

// Entries that have the same mp_format must be:
//  1. consecutive
//  2. sorted by preferred format (worst comes last)
static const struct alsa_fmt mp_alsa_formats[] = {
    {AF_FORMAT_U8,          SND_PCM_FORMAT_U8},
    {AF_FORMAT_S16,         SND_PCM_FORMAT_S16},
    {AF_FORMAT_S32,         SND_PCM_FORMAT_S32},
    {AF_FORMAT_S32,         SND_PCM_FORMAT_S24, .bits = 32, .pad_msb = 8},
    {AF_FORMAT_S32,
            MP_SELECT_LE_BE(SND_PCM_FORMAT_S24_3LE, SND_PCM_FORMAT_S24_3BE),
            .bits = 24, .pad_msb = 0},
    {AF_FORMAT_FLOAT,       SND_PCM_FORMAT_FLOAT},
    {AF_FORMAT_DOUBLE,      SND_PCM_FORMAT_FLOAT64},
    {0},
};

static const struct alsa_fmt *find_alsa_format(int mp_format)
{
    for (int n = 0; mp_alsa_formats[n].mp_format; n++) {
        if (mp_alsa_formats[n].mp_format == mp_format)
            return &mp_alsa_formats[n];
    }
    return NULL;
}

#if HAVE_CHMAP_API

static const int alsa_to_mp_channels[][2] = {
    {SND_CHMAP_FL,      MP_SP(FL)},
    {SND_CHMAP_FR,      MP_SP(FR)},
    {SND_CHMAP_RL,      MP_SP(BL)},
    {SND_CHMAP_RR,      MP_SP(BR)},
    {SND_CHMAP_FC,      MP_SP(FC)},
    {SND_CHMAP_LFE,     MP_SP(LFE)},
    {SND_CHMAP_SL,      MP_SP(SL)},
    {SND_CHMAP_SR,      MP_SP(SR)},
    {SND_CHMAP_RC,      MP_SP(BC)},
    {SND_CHMAP_FLC,     MP_SP(FLC)},
    {SND_CHMAP_FRC,     MP_SP(FRC)},
    {SND_CHMAP_FLW,     MP_SP(WL)},
    {SND_CHMAP_FRW,     MP_SP(WR)},
    {SND_CHMAP_TC,      MP_SP(TC)},
    {SND_CHMAP_TFL,     MP_SP(TFL)},
    {SND_CHMAP_TFR,     MP_SP(TFR)},
    {SND_CHMAP_TFC,     MP_SP(TFC)},
    {SND_CHMAP_TRL,     MP_SP(TBL)},
    {SND_CHMAP_TRR,     MP_SP(TBR)},
    {SND_CHMAP_TRC,     MP_SP(TBC)},
    {SND_CHMAP_RRC,     MP_SP(SDR)},
    {SND_CHMAP_RLC,     MP_SP(SDL)},
    {SND_CHMAP_MONO,    MP_SP(FC)},
    {SND_CHMAP_NA,      MP_SPEAKER_ID_NA},
    {SND_CHMAP_UNKNOWN, MP_SPEAKER_ID_NA},
    {SND_CHMAP_LAST,    MP_SPEAKER_ID_COUNT}
};

static int find_mp_channel(int alsa_channel)
{
    for (int i = 0; alsa_to_mp_channels[i][1] != MP_SPEAKER_ID_COUNT; i++) {
        if (alsa_to_mp_channels[i][0] == alsa_channel)
            return alsa_to_mp_channels[i][1];
    }

    return MP_SPEAKER_ID_COUNT;
}

#define CHMAP(n, ...) &(struct mp_chmap) MP_CONCAT(MP_CHMAP, n) (__VA_ARGS__)

// Replace each channel in a with b (a->num == b->num)
static void replace_submap(struct mp_chmap *dst, struct mp_chmap *a,
                           struct mp_chmap *b)
{
    struct mp_chmap t = *dst;
    if (!mp_chmap_is_valid(&t) || mp_chmap_diffn(a, &t) != 0)
        return;
    assert(a->num == b->num);
    for (int n = 0; n < t.num; n++) {
        for (int i = 0; i < a->num; i++) {
            if (t.speaker[n] == a->speaker[i]) {
                t.speaker[n] = b->speaker[i];
                break;
            }
        }
    }
    if (mp_chmap_is_valid(&t))
        *dst = t;
}

static bool mp_chmap_from_alsa(struct mp_chmap *dst, snd_pcm_chmap_t *src)
{
    *dst = (struct mp_chmap) {0};

    if (src->channels > MP_NUM_CHANNELS)
        return false;

    dst->num = src->channels;
    for (int c = 0; c < dst->num; c++)
        dst->speaker[c] = find_mp_channel(src->pos[c]);

    // Assume anything with 1 channel is mono.
    if (dst->num == 1)
        dst->speaker[0] = MP_SP(FC);

    // Remap weird Intel HDA HDMI 7.1 layouts correctly.
    replace_submap(dst, CHMAP(6, FL, FR, BL, BR, SDL, SDR),
                        CHMAP(6, FL, FR, SL, SR, BL,  BR));

    return mp_chmap_is_valid(dst);
}

static bool query_chmaps(struct ao *ao, struct mp_chmap *chmap)
{
    struct priv *p = ao->priv;
    struct mp_chmap_sel chmap_sel = {.tmp = p};

    snd_pcm_chmap_query_t **maps = snd_pcm_query_chmaps(p->alsa);
    if (!maps) {
        MP_VERBOSE(ao, "snd_pcm_query_chmaps() returned NULL\n");
        return false;
    }

    for (int i = 0; maps[i] != NULL; i++) {
        char aname[128];
        if (snd_pcm_chmap_print(&maps[i]->map, sizeof(aname), aname) <= 0)
            aname[0] = '\0';

        struct mp_chmap entry;
        if (mp_chmap_from_alsa(&entry, &maps[i]->map)) {
            struct mp_chmap reorder = entry;
            mp_chmap_reorder_norm(&reorder);

            MP_DBG(ao, "got ALSA chmap: %s (%s) -> %s", aname,
                   snd_pcm_chmap_type_name(maps[i]->type),
                   mp_chmap_to_str(&entry));
            if (!mp_chmap_equals(&entry, &reorder))
                MP_DBG(ao, " -> %s", mp_chmap_to_str(&reorder));
            MP_DBG(ao, "\n");

            struct mp_chmap final =
                maps[i]->type == SND_CHMAP_TYPE_VAR ? reorder : entry;
            mp_chmap_sel_add_map(&chmap_sel, &final);
        } else {
            MP_VERBOSE(ao, "skipping unknown ALSA channel map: %s\n", aname);
        }
    }

    snd_pcm_free_chmaps(maps);

    return ao_chmap_sel_adjust2(ao, &chmap_sel, chmap, false);
}

// Map back our selected channel layout to an ALSA one. This is done this way so
// that our ALSA->mp_chmap mapping function only has to go one way.
// The return value is to be freed with free().
static snd_pcm_chmap_t *map_back_chmap(struct ao *ao, struct mp_chmap *chmap)
{
    struct priv *p = ao->priv;
    if (!mp_chmap_is_valid(chmap))
        return NULL;

    snd_pcm_chmap_query_t **maps = snd_pcm_query_chmaps(p->alsa);
    if (!maps)
        return NULL;

    snd_pcm_chmap_t *alsa_chmap = NULL;

    for (int i = 0; maps[i] != NULL; i++) {
        struct mp_chmap entry;
        if (!mp_chmap_from_alsa(&entry, &maps[i]->map))
            continue;

        if (mp_chmap_equals(chmap, &entry) ||
            (mp_chmap_equals_reordered(chmap, &entry) &&
                maps[i]->type == SND_CHMAP_TYPE_VAR))
        {
            alsa_chmap = calloc(1, sizeof(*alsa_chmap) +
                                   sizeof(alsa_chmap->pos[0]) * entry.num);
            if (!alsa_chmap)
                break;
            alsa_chmap->channels = entry.num;

            // Undo if mp_chmap_reorder() was called on the result.
            int reorder[MP_NUM_CHANNELS];
            mp_chmap_get_reorder(reorder, chmap, &entry);
            for (int n = 0; n < entry.num; n++)
                alsa_chmap->pos[n] = maps[i]->map.pos[reorder[n]];
            break;
        }
    }

    snd_pcm_free_chmaps(maps);
    return alsa_chmap;
}


static int set_chmap(struct ao *ao, struct mp_chmap *dev_chmap, int num_channels)
{
    struct priv *p = ao->priv;
    int err;

    snd_pcm_chmap_t *alsa_chmap = map_back_chmap(ao, dev_chmap);
    if (alsa_chmap) {
        char tmp[128];
        if (snd_pcm_chmap_print(alsa_chmap, sizeof(tmp), tmp) > 0)
            MP_VERBOSE(ao, "trying to set ALSA channel map: %s\n", tmp);

        err = snd_pcm_set_chmap(p->alsa, alsa_chmap);
        if (err == -ENXIO) {
            // A device my not be able to set any channel map, even channel maps
            // that were reported as supported. This is either because the ALSA
            // device is broken (dmix), or because the driver has only 1
            // channel map per channel count, and setting the map is not needed.
            MP_VERBOSE(ao, "device returned ENXIO when setting channel map %s\n",
                       mp_chmap_to_str(dev_chmap));
        } else {
            CHECK_ALSA_WARN("Channel map setup failed");
        }

        free(alsa_chmap);
    }

    alsa_chmap = snd_pcm_get_chmap(p->alsa);
    if (alsa_chmap) {
        char tmp[128];
        if (snd_pcm_chmap_print(alsa_chmap, sizeof(tmp), tmp) > 0)
            MP_VERBOSE(ao, "channel map reported by ALSA: %s\n", tmp);

        struct mp_chmap chmap;
        mp_chmap_from_alsa(&chmap, alsa_chmap);

        MP_VERBOSE(ao, "which we understand as: %s\n", mp_chmap_to_str(&chmap));

        if (p->opts->ignore_chmap) {
            MP_VERBOSE(ao, "user set ignore-chmap; ignoring the channel map.\n");
        } else if (af_fmt_is_spdif(ao->format)) {
            MP_VERBOSE(ao, "using spdif passthrough; ignoring the channel map.\n");
        } else if (!mp_chmap_is_valid(&chmap)) {
            MP_WARN(ao, "Got unknown channel map from ALSA.\n");
        } else if (chmap.num != num_channels) {
            MP_WARN(ao, "ALSA channel map conflicts with channel count!\n");
        } else {
            if (mp_chmap_equals(&chmap, &ao->channels)) {
                MP_VERBOSE(ao, "which is what we requested.\n");
            } else if (!mp_chmap_is_valid(dev_chmap)) {
                MP_VERBOSE(ao, "ignoring the ALSA channel map.\n");
            } else {
                MP_VERBOSE(ao, "using the ALSA channel map.\n");
                ao->channels = chmap;
            }
        }

        free(alsa_chmap);
    }

    return 0;
}

#else /* HAVE_CHMAP_API */

static bool query_chmaps(struct ao *ao, struct mp_chmap *chmap)
{
    return false;
}

static int set_chmap(struct ao *ao, struct mp_chmap *dev_chmap, int num_channels)
{
    return 0;
}

#endif /* else HAVE_CHMAP_API */

static void dump_hw_params(struct ao *ao, const char *msg,
                           snd_pcm_hw_params_t *hw_params)
{
    struct priv *p = ao->priv;
    int err;

    err = snd_pcm_hw_params_dump(hw_params, p->output);
    CHECK_ALSA_WARN("Dump hwparams error");

    char *tmp = NULL;
    size_t tmp_s = snd_output_buffer_string(p->output, &tmp);
    if (tmp)
        mp_msg(ao->log, MSGL_DEBUG, "%s---\n%.*s---\n", msg, (int)tmp_s, tmp);
    snd_output_flush(p->output);
}

static int map_iec958_srate(int srate)
{
    switch (srate) {
    case 44100:     return IEC958_AES3_CON_FS_44100;
    case 48000:     return IEC958_AES3_CON_FS_48000;
    case 32000:     return IEC958_AES3_CON_FS_32000;
    case 22050:     return IEC958_AES3_CON_FS_22050;
    case 24000:     return IEC958_AES3_CON_FS_24000;
    case 88200:     return IEC958_AES3_CON_FS_88200;
    case 768000:    return IEC958_AES3_CON_FS_768000;
    case 96000:     return IEC958_AES3_CON_FS_96000;
    case 176400:    return IEC958_AES3_CON_FS_176400;
    case 192000:    return IEC958_AES3_CON_FS_192000;
    default:        return IEC958_AES3_CON_FS_NOTID;
    }
}

// ALSA device strings can have parameters. They are usually appended to the
// device name. There can be various forms, and we (sometimes) want to append
// them to unknown device strings, which possibly already include params.
static char *append_params(void *ta_parent, const char *device, const char *p)
{
    if (!p || !p[0])
        return talloc_strdup(ta_parent, device);

    int len = strlen(device);
    char *end = strchr(device, ':');
    if (!end) {
        /* no existing parameters: add it behind device name */
        return talloc_asprintf(ta_parent, "%s:%s", device, p);
    } else if (end[1] == '\0') {
        /* ":" but no parameters */
        return talloc_asprintf(ta_parent, "%s%s", device, p);
    } else if (end[1] == '{' && device[len - 1] == '}') {
        /* parameters in config syntax: add it inside the { } block */
        return talloc_asprintf(ta_parent, "%.*s %s}", len - 1, device, p);
    } else {
        /* a simple list of parameters: add it at the end of the list */
        return talloc_asprintf(ta_parent, "%s,%s", device, p);
    }
    abort();
}

static int try_open_device(struct ao *ao, const char *device, int mode)
{
    struct priv *p = ao->priv;
    int err;

    if (af_fmt_is_spdif(ao->format)) {
        void *tmp = talloc_new(NULL);
        char *params = talloc_asprintf(tmp,
                        "AES0=%d,AES1=%d,AES2=0,AES3=%d",
                        IEC958_AES0_NONAUDIO | IEC958_AES0_PRO_EMPHASIS_NONE,
                        IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER,
                        map_iec958_srate(ao->samplerate));
        const char *ac3_device = append_params(tmp, device, params);
        MP_VERBOSE(ao, "opening device '%s' => '%s'\n", device, ac3_device);
        err = snd_pcm_open(&p->alsa, ac3_device, SND_PCM_STREAM_PLAYBACK, mode);
        if (err < 0) {
            // Some spdif-capable devices do not accept the AES0 parameter,
            // and instead require the iec958 pseudo-device (they will play
            // noise otherwise). Unfortunately, ALSA gives us no way to map
            // these devices, so try it for the default device only.
            bstr dev;
            bstr_split_tok(bstr0(device), ":", &dev, &(bstr){0});
            if (bstr_equals0(dev, "default")) {
                const char *const fallbacks[] = {"hdmi", "iec958", NULL};
                for (int n = 0; fallbacks[n]; n++) {
                    char *ndev = append_params(tmp, fallbacks[n], params);
                    MP_VERBOSE(ao, "got error '%s'; opening iec fallback "
                               "device '%s'\n", snd_strerror(err), ndev);
                    err = snd_pcm_open
                                (&p->alsa, ndev, SND_PCM_STREAM_PLAYBACK, mode);
                    if (err >= 0)
                        break;
                }
            }
        }
        talloc_free(tmp);
    } else {
        MP_VERBOSE(ao, "opening device '%s'\n", device);
        err = snd_pcm_open(&p->alsa, device, SND_PCM_STREAM_PLAYBACK, mode);
    }

    return err;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->output)
        snd_output_close(p->output);
    p->output = NULL;

    if (p->alsa) {
        int err;

        err = snd_pcm_close(p->alsa);
        p->alsa = NULL;
        CHECK_ALSA_ERROR("pcm close error");
    }

alsa_error: ;
}

#define INIT_DEVICE_ERR_GENERIC -1
#define INIT_DEVICE_ERR_HWPARAMS -2
static int init_device(struct ao *ao, int mode)
{
    struct priv *p = ao->priv;
    struct ao_alsa_opts *opts = p->opts;
    int ret = INIT_DEVICE_ERR_GENERIC;
    char *tmp;
    size_t tmp_s;
    int err;

    p->alsa_fmt = SND_PCM_FORMAT_UNKNOWN;

    err = snd_output_buffer_open(&p->output);
    CHECK_ALSA_ERROR("Unable to create output buffer");

    const char *device = "default";
    if (ao->device)
        device = ao->device;

    err = try_open_device(ao, device, mode);
    CHECK_ALSA_ERROR("Playback open error");

    err = snd_pcm_dump(p->alsa, p->output);
    CHECK_ALSA_WARN("Dump PCM error");
    tmp_s = snd_output_buffer_string(p->output, &tmp);
    if (tmp)
        MP_DBG(ao, "PCM setup:\n---\n%.*s---\n", (int)tmp_s, tmp);
    snd_output_flush(p->output);

    err = snd_pcm_nonblock(p->alsa, 0);
    CHECK_ALSA_WARN("Unable to set blocking mode");

    snd_pcm_hw_params_t *alsa_hwparams;
    snd_pcm_hw_params_alloca(&alsa_hwparams);

    err = snd_pcm_hw_params_any(p->alsa, alsa_hwparams);
    CHECK_ALSA_ERROR("Unable to get initial parameters");

    dump_hw_params(ao, "Start HW params:\n", alsa_hwparams);

    // Some ALSA drivers have broken delay reporting, so disable the ALSA
    // resampling plugin by default.
    if (!p->opts->resample) {
        err = snd_pcm_hw_params_set_rate_resample(p->alsa, alsa_hwparams, 0);
        CHECK_ALSA_ERROR("Unable to disable resampling");
    }
    dump_hw_params(ao, "HW params after rate:\n", alsa_hwparams);

    snd_pcm_access_t access = af_fmt_is_planar(ao->format)
                                    ? SND_PCM_ACCESS_RW_NONINTERLEAVED
                                    : SND_PCM_ACCESS_RW_INTERLEAVED;
    err = snd_pcm_hw_params_set_access(p->alsa, alsa_hwparams, access);
    if (err < 0 && af_fmt_is_planar(ao->format)) {
        ao->format = af_fmt_from_planar(ao->format);
        access = SND_PCM_ACCESS_RW_INTERLEAVED;
        err = snd_pcm_hw_params_set_access(p->alsa, alsa_hwparams, access);
    }
    CHECK_ALSA_ERROR("Unable to set access type");
    dump_hw_params(ao, "HW params after access:\n", alsa_hwparams);

    bool found_format = false;
    int try_formats[AF_FORMAT_COUNT + 1];
    af_get_best_sample_formats(ao->format, try_formats);
    for (int n = 0; try_formats[n] && !found_format; n++) {
        int mp_format = try_formats[n];
        if (af_fmt_is_planar(ao->format) != af_fmt_is_planar(mp_format))
            continue; // implied SND_PCM_ACCESS mismatches
        int mp_pformat = af_fmt_from_planar(mp_format);
        if (af_fmt_is_spdif(mp_pformat))
            mp_pformat = AF_FORMAT_S16;
        const struct alsa_fmt *fmt = find_alsa_format(mp_pformat);
        if (!fmt)
            continue;
        for (; fmt->mp_format == mp_pformat; fmt++) {
            p->alsa_fmt = fmt->alsa_format;
            p->convert = (struct ao_convert_fmt){
                .src_fmt = mp_format,
                .dst_bits = fmt->bits ? fmt->bits : af_fmt_to_bytes(mp_format) * 8,
                .pad_msb = fmt->pad_msb,
            };
            if (!ao_can_convert_inplace(&p->convert))
                continue;
            MP_VERBOSE(ao, "trying format %s/%d\n", af_fmt_to_str(mp_pformat),
                       p->alsa_fmt);
            if (snd_pcm_hw_params_test_format(p->alsa, alsa_hwparams,
                                              p->alsa_fmt) >= 0)
            {
                ao->format = mp_format;
                found_format = true;
                break;
            }
        }
    }

    if (!found_format) {
        MP_ERR(ao, "Can't find appropriate sample format.\n");
        goto alsa_error;
    }

    err = snd_pcm_hw_params_set_format(p->alsa, alsa_hwparams, p->alsa_fmt);
    CHECK_ALSA_ERROR("Unable to set format");
    dump_hw_params(ao, "HW params after format:\n", alsa_hwparams);

    // Stereo, or mono if input is 1 channel.
    struct mp_chmap reduced;
    mp_chmap_from_channels(&reduced, MPMIN(2, ao->channels.num));

    struct mp_chmap dev_chmap = {0};
    if (!af_fmt_is_spdif(ao->format) && !p->opts->ignore_chmap &&
        !mp_chmap_equals(&ao->channels, &reduced))
    {
        struct mp_chmap res = ao->channels;
        if (query_chmaps(ao, &res))
            dev_chmap = res;

        // Whatever it is, we dumb it down to mono or stereo. Some drivers may
        // return things like bl-br, but the user (probably) still wants stereo.
        // This also handles the failure case (dev_chmap.num==0).
        if (dev_chmap.num <= 2) {
            dev_chmap.num = 0;
            ao->channels = reduced;
        } else if (dev_chmap.num) {
            ao->channels = dev_chmap;
        }
    }

    int num_channels = ao->channels.num;
    err = snd_pcm_hw_params_set_channels_near
            (p->alsa, alsa_hwparams, &num_channels);
    CHECK_ALSA_ERROR("Unable to set channels");
    dump_hw_params(ao, "HW params after channels:\n", alsa_hwparams);

    if (num_channels > MP_NUM_CHANNELS) {
        MP_FATAL(ao, "Too many audio channels (%d).\n", num_channels);
        goto alsa_error;
    }

    err = snd_pcm_hw_params_set_rate_near
            (p->alsa, alsa_hwparams, &ao->samplerate, NULL);
    CHECK_ALSA_ERROR("Unable to set samplerate-2");
    dump_hw_params(ao, "HW params after rate-2:\n", alsa_hwparams);

    snd_pcm_hw_params_t *hwparams_backup;
    snd_pcm_hw_params_alloca(&hwparams_backup);
    snd_pcm_hw_params_copy(hwparams_backup, alsa_hwparams);

    // Cargo-culted buffer settings; might still be useful for PulseAudio.
    err = 0;
    if (opts->buffer_time) {
        err = snd_pcm_hw_params_set_buffer_time_near
                (p->alsa, alsa_hwparams, &(unsigned int){opts->buffer_time}, NULL);
        CHECK_ALSA_WARN("Unable to set buffer time near");
    }
    if (err >= 0 && opts->frags) {
        err = snd_pcm_hw_params_set_periods_near
                    (p->alsa, alsa_hwparams, &(unsigned int){opts->frags}, NULL);
        CHECK_ALSA_WARN("Unable to set periods");
    }
    if (err < 0)
        snd_pcm_hw_params_copy(alsa_hwparams, hwparams_backup);

    dump_hw_params(ao, "Going to set final HW params:\n", alsa_hwparams);

    /* finally install hardware parameters */
    err = snd_pcm_hw_params(p->alsa, alsa_hwparams);
    ret = INIT_DEVICE_ERR_HWPARAMS;
    CHECK_ALSA_ERROR("Unable to set hw-parameters");
    ret = INIT_DEVICE_ERR_GENERIC;
    dump_hw_params(ao, "Final HW params:\n", alsa_hwparams);

    if (set_chmap(ao, &dev_chmap, num_channels) < 0)
        goto alsa_error;

    if (num_channels != ao->channels.num) {
        int req = ao->channels.num;
        mp_chmap_from_channels(&ao->channels, MPMIN(2, num_channels));
        mp_chmap_fill_na(&ao->channels, num_channels);
        MP_ERR(ao, "Asked for %d channels, got %d - fallback to %s.\n", req,
               num_channels, mp_chmap_to_str(&ao->channels));
        if (num_channels != ao->channels.num) {
            MP_FATAL(ao, "mismatching channel counts.\n");
            goto alsa_error;
        }
    }

    err = snd_pcm_hw_params_get_buffer_size(alsa_hwparams, &p->buffersize);
    CHECK_ALSA_ERROR("Unable to get buffersize");

    err = snd_pcm_hw_params_get_period_size(alsa_hwparams, &p->outburst, NULL);
    CHECK_ALSA_ERROR("Unable to get period size");

    p->can_pause = snd_pcm_hw_params_can_pause(alsa_hwparams);

    snd_pcm_sw_params_t *alsa_swparams;
    snd_pcm_sw_params_alloca(&alsa_swparams);

    err = snd_pcm_sw_params_current(p->alsa, alsa_swparams);
    CHECK_ALSA_ERROR("Unable to get sw-parameters");

    snd_pcm_uframes_t boundary;
    err = snd_pcm_sw_params_get_boundary(alsa_swparams, &boundary);
    CHECK_ALSA_ERROR("Unable to get boundary");

    // Manual trigger; INT_MAX as suggested by ALSA doxygen (they call it MAXINT).
    err = snd_pcm_sw_params_set_start_threshold(p->alsa, alsa_swparams, INT_MAX);
    CHECK_ALSA_ERROR("Unable to set start threshold");

    /* play silence when there is an underrun */
    err = snd_pcm_sw_params_set_silence_size
            (p->alsa, alsa_swparams, boundary);
    CHECK_ALSA_ERROR("Unable to set silence size");

    err = snd_pcm_sw_params(p->alsa, alsa_swparams);
    CHECK_ALSA_ERROR("Unable to set sw-parameters");

    MP_VERBOSE(ao, "hw pausing supported: %s\n", p->can_pause ? "yes" : "no");
    MP_VERBOSE(ao, "buffersize: %d samples\n", (int)p->buffersize);
    MP_VERBOSE(ao, "period size: %d samples\n", (int)p->outburst);

    ao->device_buffer = p->buffersize;

    p->convert.channels = ao->channels.num;

    err = snd_pcm_prepare(p->alsa);
    CHECK_ALSA_ERROR("pcm prepare error");

    return 0;

alsa_error:
    uninit(ao);
    return ret;
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    p->opts = mp_get_config_group(ao, ao->global, &ao_alsa_conf);

    if (!p->opts->ni)
        ao->format = af_fmt_from_planar(ao->format);

    MP_VERBOSE(ao, "using ALSA version: %s\n", snd_asoundlib_version());

    int mode = 0;
    int r = init_device(ao, mode);
    if (r == INIT_DEVICE_ERR_HWPARAMS) {
        // With some drivers, ALSA appears to be unable to set valid hwparams,
        // but they work if at least SND_PCM_NO_AUTO_FORMAT is set. Also, it
        // appears you can set this flag only on opening a device, thus there
        // is the need to retry opening the device.
        MP_WARN(ao, "Attempting to work around even more ALSA bugs...\n");
        mode |= SND_PCM_NO_AUTO_CHANNELS | SND_PCM_NO_AUTO_FORMAT |
                SND_PCM_NO_AUTO_RESAMPLE;
        r = init_device(ao, mode);
    }

    // Sometimes, ALSA will advertise certain chmaps, but it's not possible to
    // set them. This can happen with dmix: as of alsa 1.0.29, dmix can do
    // stereo only, but advertises the surround chmaps of the underlying device.
    // In this case, e.g. setting 6 channels will succeed, but requesting  5.1
    // afterwards will fail. Then it will return something like "FL FR NA NA NA NA"
    // as channel map. This means we would have to pad stereo output to 6
    // channels with silence, which would require lots of extra processing. You
    // can't change the number of channels to 2 either, because the hw params
    // are already set! So just fuck it and reopen the device with the chmap
    // "cleaned out" of NA entries.
    if (r >= 0) {
        struct mp_chmap without_na = ao->channels;
        mp_chmap_remove_na(&without_na);

        if (mp_chmap_is_valid(&without_na) && without_na.num <= 2 &&
            ao->channels.num > 2)
        {
            MP_VERBOSE(ao, "Working around braindead dmix multichannel behavior.\n");
            uninit(ao);
            ao->channels = without_na;
            r = init_device(ao, mode);
        }
    }

    return r;
}

// Function for dealing with playback state. This attempts to recover the ALSA
// state (bring it into SND_PCM_STATE_{PREPARED,RUNNING,PAUSED,UNDERRUN}). If
// state!=NULL, fill it after recovery is attempted.
// Returns true if PCM is in one the expected states.
static bool recover_and_get_state(struct ao *ao, struct mp_pcm_state *state)
{
    struct priv *p = ao->priv;
    int err;

    snd_pcm_status_t *st;
    snd_pcm_status_alloca(&st);

    bool state_ok = false;
    snd_pcm_state_t pcmst = SND_PCM_STATE_DISCONNECTED;

    // Give it a number of chances to recover. This tries to deal with the fact
    // that the API is asynchronous, and to account for some past cargo-cult
    // (where things were retried in a loop).
    for (int n = 0; n < 10; n++) {
        err = snd_pcm_status(p->alsa, st);
        if (err == -EPIPE) {
            // ALSA APIs can return -EPIPE when an XRUN happens,
            // we skip right to handling it by setting pcmst
            // manually.
            pcmst = SND_PCM_STATE_XRUN;
        } else {
            // Otherwise do error checking and query the PCM state properly.
            CHECK_ALSA_ERROR("snd_pcm_status");

            pcmst = snd_pcm_status_get_state(st);
        }

        if (pcmst == SND_PCM_STATE_PREPARED ||
            pcmst == SND_PCM_STATE_RUNNING ||
            pcmst == SND_PCM_STATE_PAUSED)
        {
            state_ok = true;
            break;
        }

        MP_VERBOSE(ao, "attempt %d to recover from state '%s'...\n",
                   n + 1, snd_pcm_state_name(pcmst));

        switch (pcmst) {
        // Underrun; recover. (We never use draining.)
        case SND_PCM_STATE_XRUN:
        case SND_PCM_STATE_DRAINING:
            err = snd_pcm_prepare(p->alsa);
            CHECK_ALSA_ERROR("pcm prepare error");
            continue;
        // Hardware suspend.
        case SND_PCM_STATE_SUSPENDED:
            MP_INFO(ao, "PCM in suspend mode, trying to resume.\n");
            err = snd_pcm_resume(p->alsa);
            if (err == -EAGAIN) {
                // Cargo-cult from decades ago, with a cargo cult timeout.
                MP_INFO(ao, "PCM resume EAGAIN - retrying.\n");
                sleep(1);
                continue;
            }
            if (err == -ENOSYS) {
                // As suggested by ALSA doxygen.
                MP_VERBOSE(ao, "ENOSYS, retrying with snd_pcm_prepare().\n");
                err = snd_pcm_prepare(p->alsa);
            }
            if (err < 0)
                MP_ERR(ao, "resuming from SUSPENDED: %s\n", snd_strerror(err));
            continue;
        // Device lost. OPEN/SETUP are states we never enter after init, so
        // treat them like DISCONNECTED.
        case SND_PCM_STATE_DISCONNECTED:
        case SND_PCM_STATE_OPEN:
        case SND_PCM_STATE_SETUP:
        default:
            if (!p->device_lost) {
                MP_WARN(ao, "Device lost, trying to recover...\n");
                ao_request_reload(ao);
                p->device_lost = true;
            }
            goto alsa_error;
        }
    }

    if (!state_ok) {
        MP_ERR(ao, "could not recover\n");
    }

alsa_error:

    if (state) {
        snd_pcm_sframes_t del = state_ok ? snd_pcm_status_get_delay(st) : 0;
        state->delay = MPMAX(del, 0) / (double)ao->samplerate;
        state->free_samples = state_ok ? snd_pcm_status_get_avail(st) : 0;
        state->free_samples = MPCLAMP(state->free_samples, 0, ao->device_buffer);
        // Align to period size.
        state->free_samples = state->free_samples / p->outburst * p->outburst;
        state->queued_samples = ao->device_buffer - state->free_samples;
        state->playing = pcmst == SND_PCM_STATE_RUNNING ||
                         pcmst == SND_PCM_STATE_PAUSED;
    }

    return state_ok;
}

static void audio_get_state(struct ao *ao, struct mp_pcm_state *state)
{
    recover_and_get_state(ao, state);
}

static void audio_start(struct ao *ao)
{
    struct priv *p = ao->priv;
    int err;

    recover_and_get_state(ao, NULL);

    err = snd_pcm_start(p->alsa);
    CHECK_ALSA_ERROR("pcm start error");

alsa_error: ;
}

static void audio_reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    int err;

    err = snd_pcm_drop(p->alsa);
    CHECK_ALSA_ERROR("pcm drop error");
    err = snd_pcm_prepare(p->alsa);
    CHECK_ALSA_ERROR("pcm prepare error");

    recover_and_get_state(ao, NULL);

alsa_error: ;
}

static bool audio_set_paused(struct ao *ao, bool paused)
{
    struct priv *p = ao->priv;
    int err;

    recover_and_get_state(ao, NULL);

    if (!p->can_pause)
        return false;

    snd_pcm_state_t pcmst = snd_pcm_state(p->alsa);
    if (pcmst == SND_PCM_STATE_RUNNING && paused) {
        err = snd_pcm_pause(p->alsa, 1);
        CHECK_ALSA_ERROR("pcm pause error");
    } else if (pcmst == SND_PCM_STATE_PAUSED && !paused) {
        err = snd_pcm_pause(p->alsa, 0);
        CHECK_ALSA_ERROR("pcm resume error");
    }

    return true;

alsa_error:
    return false;
}

static bool audio_write(struct ao *ao, void **data, int samples)
{
    struct priv *p = ao->priv;

    ao_convert_inplace(&p->convert, data, samples);

    if (!recover_and_get_state(ao, NULL))
        return false;

    snd_pcm_sframes_t err = 0;
    if (af_fmt_is_planar(ao->format)) {
        err = snd_pcm_writen(p->alsa, data, samples);
    } else {
        err = snd_pcm_writei(p->alsa, data[0], samples);
    }

    CHECK_ALSA_ERROR("pcm write error");
    if (err >= 0 && err != samples) {
        MP_ERR(ao, "unexpected partial write (%d of %d frames), dropping audio\n",
               (int)err, samples);
    }

    return true;

alsa_error:
    return false;
}

static bool is_useless_device(char *name)
{
    char *crap[] = {"rear", "center_lfe", "side", "pulse", "null", "dsnoop", "hw"};
    for (int i = 0; i < MP_ARRAY_SIZE(crap); i++) {
        int l = strlen(crap[i]);
        if (name && strncmp(name, crap[i], l) == 0 &&
            (!name[l] || name[l] == ':'))
            return true;
    }
    // The standard default entry will achieve exactly the same.
    if (name && strcmp(name, "default") == 0)
        return true;
    return false;
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    void **hints;
    if (snd_device_name_hint(-1, "pcm", &hints) < 0)
        return;

    ao_device_list_add(list, ao, &(struct ao_device_desc){"", ""});

    for (int n = 0; hints[n]; n++) {
        char *name = snd_device_name_get_hint(hints[n], "NAME");
        char *desc = snd_device_name_get_hint(hints[n], "DESC");
        char *io = snd_device_name_get_hint(hints[n], "IOID");
        if (!is_useless_device(name) && (!io || strcmp(io, "Output") == 0)) {
            char desc2[1024];
            snprintf(desc2, sizeof(desc2), "%s", desc ? desc : "");
            for (int i = 0; desc2[i]; i++) {
                if (desc2[i] == '\n')
                    desc2[i] = '/';
            }
            ao_device_list_add(list, ao, &(struct ao_device_desc){name, desc2});
        }
        free(name);
        free(desc);
        free(io);
    }

    snd_device_name_free_hint(hints);
}

const struct ao_driver audio_out_alsa = {
    .description = "ALSA audio output",
    .name      = "alsa",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .get_state = audio_get_state,
    .write     = audio_write,
    .start     = audio_start,
    .set_pause = audio_set_paused,
    .reset     = audio_reset,
    .list_devs = list_devs,
    .priv_size = sizeof(struct priv),
    .global_opts = &ao_alsa_conf,
};
