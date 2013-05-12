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

#include "core/m_config.h"
#include "core/m_option.h"

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

    struct mp_audio data;
    struct mp_audio temp;
};

static const struct priv defaults = {
    .in_format = AF_FORMAT_UNKNOWN,
    .out_format = AF_FORMAT_UNKNOWN,
};

#define OPT_BASE_STRUCT struct priv

static const struct m_option options[] = {
    OPT_AUDIOFORMAT("format", in_format, 0),
    OPT_INTRANGE("srate", in_srate, 0, 1000, 8*48000),
    OPT_CHMAP("channels", in_channels, CONF_MIN, .min = 0),
    OPT_AUDIOFORMAT("out-format", out_format, 0),
    OPT_INTRANGE("out-srate", out_srate, 0, 1000, 8*48000),
    OPT_CHMAP("out-channels", out_channels, CONF_MIN, .min = 0),
    {0}
};

static int control(struct af_instance *af, int cmd, void *arg)
{
    struct priv *priv = af->setup;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio orig_in = *in;
        struct mp_audio *out = af->data;

        if (priv->in_format != AF_FORMAT_UNKNOWN)
            mp_audio_set_format(in, priv->in_format);

        if (priv->in_channels.num)
            mp_audio_set_channels(in, &priv->in_channels);

        if (priv->in_srate)
            in->rate = priv->in_srate;

        mp_audio_copy_config(out, in);

        if (priv->out_format != AF_FORMAT_UNKNOWN)
            mp_audio_set_format(out, priv->out_format);

        if (priv->out_channels.num)
            mp_audio_set_channels(out, &priv->out_channels);

        if (priv->out_srate)
            out->rate = priv->out_srate;

        if (in->nch != out->nch || in->bps != out->bps) {
            mp_msg(MSGT_AFILTER, MSGL_ERR,
                   "[af_force] Forced input/output format are incompatible.\n");
            return AF_ERROR;
        }

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    case AF_CONTROL_COMMAND_LINE: {
        if (m_config_parse_suboptions(priv->config, "af_force", (char *)arg) < 0)
            return AF_ERROR;
        return AF_OK;
    }
    }
    return AF_UNKNOWN;
}

static struct mp_audio *play(struct af_instance *af, struct mp_audio *data)
{
    struct priv *priv = af->setup;
    struct mp_audio *r = &priv->temp;

    *r = *af->data;
    r->audio = data->audio;
    r->len = data->len;

    return r;
}

static void uninit(struct af_instance *af)
{
    talloc_free(af->setup);
}

static int af_open(struct af_instance *af)
{
    af->control = control;
    af->uninit = uninit;
    af->play = play;
    af->mul = 1;
    struct priv *priv = talloc(NULL, struct priv);
    af->setup = priv;
    *priv = defaults;
    priv->config = m_config_simple(priv);
    talloc_steal(priv, priv->config);
    m_config_register_options(priv->config, options);
    af->data = &priv->data;
    return AF_OK;
}

struct af_info af_info_force = {
    "Force audio format",
    "force",
    "",
    "",
    0,
    af_open
};
