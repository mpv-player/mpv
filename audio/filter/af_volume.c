/*
 * Copyright (C)2002 Anders Johansson ajh@atri.curtin.edu.au
 *
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
    float level;                // User-specified gain level for each channel
    float rgain;                // Replaygain level
    int rgain_track;            // Enable/disable track based replaygain
    int rgain_album;            // Enable/disable album based replaygain
    float rgain_preamp;         // Set replaygain pre-amplification
    int rgain_clip;             // Enable/disable clipping prevention
    int soft;                   // Enable/disable soft clipping
    int fast;                   // Use fix-point volume control
    int detach;                 // Detach if gain volume is neutral
    float cfg_volume;
};

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
        if (AF_FORMAT_IS_PLANAR(in->format))
            mp_audio_set_format(af->data, af_fmt_to_planar(af->data->format));
        s->rgain = 1.0;
        if ((s->rgain_track || s->rgain_album) && af->replaygain_data) {
            float gain, peak;

            if (s->rgain_track) {
                gain = af->replaygain_data->track_gain;
                peak = af->replaygain_data->track_peak;
            } else {
                gain = af->replaygain_data->album_gain;
                peak = af->replaygain_data->album_peak;
            }

            gain += s->rgain_preamp;
            af_from_dB(1, &gain, &s->rgain, 20.0, -200.0, 60.0);

            MP_VERBOSE(af, "Applying replay-gain: %f\n", s->rgain);

            if (!s->rgain_clip) { // clipping prevention
                s->rgain = MPMIN(s->rgain, 1.0 / peak);
                MP_VERBOSE(af, "...with clipping prevention: %f\n", s->rgain);
            }
        }
        if (s->detach && fabs(s->level * s->rgain - 1.0) < 0.00001)
            return AF_DETACH;
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

static void filter_plane(struct af_instance *af, struct mp_audio *data, int p)
{
    struct priv *s = af->priv;

    float level = s->level * s->rgain;
    int num_samples = data->samples * data->spf;

    if (af_fmt_from_planar(af->data->format) == AF_FORMAT_S16) {
        int vol = 256.0 * level;
        if (vol != 256) {
            if (af_make_writeable(af, data) < 0)
                return; // oom
            int16_t *a = data->planes[p];
            for (int i = 0; i < num_samples; i++) {
                int x = (a[i] * vol) >> 8;
                a[i] = MPCLAMP(x, SHRT_MIN, SHRT_MAX);
            }
        }
    } else if (af_fmt_from_planar(af->data->format) == AF_FORMAT_FLOAT) {
        float vol = level;
        if (vol != 1.0) {
            if (af_make_writeable(af, data) < 0)
                return; // oom
            float *a = data->planes[p];
            for (int i = 0; i < num_samples; i++) {
                float x = a[i] * vol;
                a[i] = s->soft ? af_softclip(x) : MPCLAMP(x, -1.0, 1.0);
            }
        }
    }
}

static int filter(struct af_instance *af, struct mp_audio *data)
{
    if (data) {
        for (int n = 0; n < data->num_planes; n++)
            filter_plane(af, data, n);
        af_add_output_frame(af, data);
    }
    return 0;
}

static int af_open(struct af_instance *af)
{
    struct priv *s = af->priv;
    af->control = control;
    af->filter_frame = filter;
    af_from_dB(1, &s->cfg_volume, &s->level, 20.0, -200.0, 60.0);
    return AF_OK;
}

#define OPT_BASE_STRUCT struct priv

// Description of this filter
const struct af_info af_info_volume = {
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
        OPT_FLAG("replaygain-clip", rgain_clip, 0),
        OPT_FLAG("softclip", soft, 0),
        OPT_FLAG("s16", fast, 0),
        OPT_FLAG("detach", detach, 0),
        {0}
    },
};
