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

#include <stdlib.h>

#include <libavutil/common.h>

#include "options/m_option.h"

#include "audio/format.h"
#include "af.h"

struct priv {
    struct m_config *config;

    int in_format;
    int in_srate;
    struct mp_chmap in_channels;
    int out_format;
    int out_srate;
    struct mp_chmap out_channels;

    int fail;
};

static void force_in_params(struct af_instance *af, struct mp_audio *in)
{
    struct priv *priv = af->priv;

    if (priv->in_format != AF_FORMAT_UNKNOWN)
        mp_audio_set_format(in, priv->in_format);

    if (priv->in_channels.num)
        mp_audio_set_channels(in, &priv->in_channels);

    if (priv->in_srate)
        in->rate = priv->in_srate;
}

static void force_out_params(struct af_instance *af, struct mp_audio *out)
{
    struct priv *priv = af->priv;

    if (priv->out_format != AF_FORMAT_UNKNOWN)
        mp_audio_set_format(out, priv->out_format);

    if (priv->out_channels.num)
        mp_audio_set_channels(out, &priv->out_channels);

    if (priv->out_srate)
        out->rate = priv->out_srate;
}

static int control(struct af_instance *af, int cmd, void *arg)
{
    struct priv *priv = af->priv;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio orig_in = *in;
        struct mp_audio *out = af->data;

        force_in_params(af, in);
        mp_audio_copy_config(out, in);
        force_out_params(af, out);

        if (in->nch != out->nch || in->bps != out->bps) {
            MP_ERR(af, "Forced input/output formats are incompatible.\n");
            return AF_ERROR;
        }

        if (priv->fail) {
            MP_ERR(af, "Failing on purpose.\n");
            return AF_ERROR;
        }

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
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
    af->control = control;
    af->filter_frame = filter;

    force_in_params(af, af->data);
    force_out_params(af, af->data);

    return AF_OK;
}

#define OPT_BASE_STRUCT struct priv

const struct af_info af_info_format = {
    .info = "Force audio format",
    .name = "format",
    .open = af_open,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_AUDIOFORMAT("format", in_format, 0),
        OPT_INTRANGE("srate", in_srate, 0, 1000, 8*48000),
        OPT_CHMAP("channels", in_channels, CONF_MIN, .min = 0),
        OPT_AUDIOFORMAT("out-format", out_format, 0),
        OPT_INTRANGE("out-srate", out_srate, 0, 1000, 8*48000),
        OPT_CHMAP("out-channels", out_channels, CONF_MIN, .min = 0),
        OPT_FLAG("fail", fail, 0),
        {0}
    },
};
