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

#include "mpvcore/mp_common.h"
#include "af.h"

struct priv {
    float level[AF_NCH];        // Gain level for each channel
    int soft;                   // Enable/disable soft clipping
    int fast;                   // Use fix-point volume control
    float cfg_volume;
};

static int control(struct af_instance *af, int cmd, void *arg)
{
    struct priv *s = af->priv;

    switch (cmd) {
    case AF_CONTROL_REINIT:
        mp_audio_copy_config(af->data, (struct mp_audio *)arg);

        if (s->fast && (((struct mp_audio *)arg)->format != AF_FORMAT_FLOAT_NE))
            mp_audio_set_format(af->data, AF_FORMAT_S16_NE);
        else {
            mp_audio_set_format(af->data, AF_FORMAT_FLOAT_NE);
        }
        return af_test_output(af, (struct mp_audio *)arg);
    case AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET:
        memcpy(s->level, arg, sizeof(float) * AF_NCH);
        return AF_OK;
    case AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_GET:
        memcpy(arg, s->level, sizeof(float) * AF_NCH);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

static struct mp_audio *play(struct af_instance *af, struct mp_audio *data)
{
    struct mp_audio *c = data;
    struct priv *s = af->priv;
    int nch = c->nch;

    if (af->data->format == AF_FORMAT_S16_NE) {
        int16_t *a = c->audio;
        int len = c->len / 2;
        for (int ch = 0; ch < nch; ch++) {
            int vol = 256.0 * s->level[ch];
            if (vol != 256) {
                for (int i = ch; i < len; i += nch) {
                    int x = (a[i] * vol) >> 8;
                    a[i] = MPCLAMP(x, SHRT_MIN, SHRT_MAX);
                }
            }
        }
    } else if (af->data->format == AF_FORMAT_FLOAT_NE) {
        float *a = c->audio;
        int len = c->len / 4;
        for (int ch = 0; ch < nch; ch++) {
            if (s->level[ch] != 1.0) {
                for (int i = ch; i < len; i += nch) {
                    float x = a[i];
                    x *= s->level[ch];
                    a[i] = s->soft ? af_softclip(x) : MPCLAMP(x, -1.0, 1.0);
                }
            }
        }
    }
    return c;
}

static int af_open(struct af_instance *af)
{
    struct priv *s = af->priv;
    af->control = control;
    af->play = play;
    af->mul = 1;
    af->data = talloc_zero(af, struct mp_audio);
    float level;
    af_from_dB(1, &s->cfg_volume, &level, 20.0, -200.0, 60.0);
    for (int i = 0; i < AF_NCH; i++)
        s->level[i] = level;
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
        OPT_FLAG("softclip", soft, 0),
        OPT_FLAG("s16", fast, 0),
        {0}
    },
};
