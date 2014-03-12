/*
 * Copyright (C)2002 Anders Johansson ajh@atri.curtin.edu.au
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "common/common.h"
#include "af.h"
#include "demux/demux.h"

struct priv {
    float level;                // Gain level for each channel
    int rgain_track;            // Enable/disable track based replaygain
    int rgain_album;            // Enable/disable album based replaygain
    float rgain_preamp;         // Set replaygain pre-amplification
    int rgain_noclip;           // Enable/disable clipping prevention
    int soft;                   // Enable/disable soft clipping
    int fast;                   // Use fix-point volume control
    float cfg_volume;
};

static int decode_float(char *str, float *out)
{
    char *rest;
    float dec_val;

    dec_val = strtod(str, &rest);
    if (!rest || (rest == str) || !isfinite(dec_val))
        return -1;

    *out = dec_val;
    return 0;
}

static int decode_gain(struct af_instance *af, const char *tag, float *out)
{
    char *tag_val = NULL;
    float dec_val;

    tag_val = mp_tags_get_str(af->metadata, tag);
    if (!tag_val) {
        mp_msg(af->log, MSGL_ERR, "Replaygain tags not found\n");
        return -1;
    }

    if (decode_float(tag_val, &dec_val)) {
        mp_msg(af->log, MSGL_ERR, "Invalid replaygain value\n");
        return -1;
    }

    *out = dec_val;
    return 0;
}

static int decode_peak(struct af_instance *af, const char *tag, float *out)
{
    char *tag_val = NULL;
    float dec_val;

    *out = 1.0;

    tag_val = mp_tags_get_str(af->metadata, tag);
    if (!tag_val)
        return 0;

    if (decode_float(tag_val, &dec_val))
        return 0;

    if (dec_val == 0.0)
        return 0;

    *out = dec_val;
    return 0;
}

static int control(struct af_instance *af, int cmd, void *arg)
{
    struct priv *s = af->priv;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;

        mp_audio_copy_config(af->data, in);
        mp_audio_force_interleaved_format(af->data);

        if (s->fast && af_fmt_from_planar(in->format) != AF_FORMAT_FLOAT) {
            mp_audio_set_format(af->data, AF_FORMAT_S16);
        } else {
            mp_audio_set_format(af->data, AF_FORMAT_FLOAT);
        }
        if (af_fmt_is_planar(in->format))
            mp_audio_set_format(af->data, af_fmt_to_planar(af->data->format));
        if ((s->rgain_track || s->rgain_album) && af->metadata) {
            float gain, peak;
            char *gain_tag = NULL, *peak_tag = NULL;

            if (s->rgain_track) {
                gain_tag = "REPLAYGAIN_TRACK_GAIN";
                peak_tag = "REPLAYGAIN_TRACK_PEAK";
            } else if (s->rgain_album) {
                gain_tag = "REPLAYGAIN_ALBUM_GAIN";
                peak_tag = "REPLAYGAIN_ALBUM_PEAK";
            }

            if (!decode_gain(af, gain_tag, &gain) &&
                !decode_peak(af, peak_tag, &peak))
            {
                gain += s->rgain_preamp;
                af_from_dB(1, &gain, &s->level, 20.0, -200.0, 60.0);

                if (s->rgain_noclip) // clipping prevention
                    s->level = MPMIN(s->level, 1.0 / peak);
            }
        }
        return af_test_output(af, in);
    }
    case AF_CONTROL_SET_VOLUME:
        s->level = *(float *)arg;
        return AF_OK;
    case AF_CONTROL_GET_VOLUME:
        *(float *)arg = s->level;
        return AF_OK;
    }
    return AF_UNKNOWN;
}

static void filter_plane(struct af_instance *af, void *ptr, int num_samples)
{
    struct priv *s = af->priv;

    if (af_fmt_from_planar(af->data->format) == AF_FORMAT_S16) {
        int16_t *a = ptr;
        int vol = 256.0 * s->level;
        if (vol != 256) {
            for (int i = 0; i < num_samples; i++) {
                int x = (a[i] * vol) >> 8;
                a[i] = MPCLAMP(x, SHRT_MIN, SHRT_MAX);
            }
        }
    } else if (af_fmt_from_planar(af->data->format) == AF_FORMAT_FLOAT) {
        float *a = ptr;
        float vol = s->level;
        if (vol != 1.0) {
            for (int i = 0; i < num_samples; i++) {
                float x = a[i] * vol;
                a[i] = s->soft ? af_softclip(x) : MPCLAMP(x, -1.0, 1.0);
            }
        }
    }
}

static int filter(struct af_instance *af, struct mp_audio *data, int f)
{
    for (int n = 0; n < data->num_planes; n++)
        filter_plane(af, data->planes[n], data->samples * data->spf);

    return 0;
}

static int af_open(struct af_instance *af)
{
    struct priv *s = af->priv;
    af->control = control;
    af->filter = filter;
    af_from_dB(1, &s->cfg_volume, &s->level, 20.0, -200.0, 60.0);
    return AF_OK;
}

#define OPT_BASE_STRUCT struct priv

// Description of this filter
struct af_info af_info_volume = {
    .info = "Volume control audio filter",
    .name = "volume",
    .flags = AF_FLAGS_NOT_REENTRANT,
    .open = af_open,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_FLOATRANGE("volumedb", cfg_volume, 0, -200, 60),
        OPT_FLAG("replaygain-track", rgain_track, 0),
        OPT_FLAG("replaygain-album", rgain_album, 0),
        OPT_FLOATRANGE("replaygain-preamp", rgain_preamp, 0, -15, 15),
        OPT_FLAG("replaygain-noclip", rgain_noclip, 0, OPTDEF_INT(1)),
        OPT_FLAG("softclip", soft, 0),
        OPT_FLAG("s16", fast, 0),
        {0}
    },
};
