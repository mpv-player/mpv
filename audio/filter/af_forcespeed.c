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

#include "af.h"

struct priv {
    double speed;
};

static int control(struct af_instance *af, int cmd, void *arg)
{
    struct priv *priv = af->priv;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio orig_in = *in;
        struct mp_audio *out = af->data;

        mp_audio_copy_config(out, in);
        out->rate = in->rate * priv->speed;

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    case AF_CONTROL_SET_PLAYBACK_SPEED_RESAMPLE: {
        priv->speed = *(double *)arg;
        return AF_OK;
    }
    }
    return AF_UNKNOWN;
}

static int filter(struct af_instance *af, struct mp_audio *data)
{
    if (data)
        mp_audio_copy_config(data, af->data);
    af_add_output_frame(af, data);
    return 0;
}

static int af_open(struct af_instance *af)
{
    struct priv *priv = af->priv;
    af->control = control;
    af->filter_frame = filter;
    priv->speed = 1.0;
    return AF_OK;
}

#define OPT_BASE_STRUCT struct priv

const struct af_info af_info_forcespeed = {
    .info = "Force audio speed",
    .name = "forcespeed",
    .open = af_open,
    .priv_size = sizeof(struct priv),
};
