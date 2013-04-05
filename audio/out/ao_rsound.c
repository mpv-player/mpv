/*
 * RSound audio output driver
 *
 * Copyright (C) 2011 Hans-Kristian Arntzen
 *
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rsound.h>

#include "talloc.h"

#include "core/subopt-helper.h"
#include "osdep/timer.h"
#include "audio/format.h"
#include "ao.h"

struct priv {
    rsound_t *rd;
};

static int set_format(struct ao *ao)
{
    int rsd_format;

    switch (ao->format) {
    case AF_FORMAT_U8:
        rsd_format = RSD_U8;
        break;
    case AF_FORMAT_S8:
        rsd_format = RSD_S8;
        break;
    case AF_FORMAT_S16_LE:
        rsd_format = RSD_S16_LE;
        break;
    case AF_FORMAT_S16_BE:
        rsd_format = RSD_S16_BE;
        break;
    case AF_FORMAT_U16_LE:
        rsd_format = RSD_U16_LE;
        break;
    case AF_FORMAT_U16_BE:
        rsd_format = RSD_U16_BE;
        break;
    case AF_FORMAT_S24_LE:
    case AF_FORMAT_S24_BE:
    case AF_FORMAT_U24_LE:
    case AF_FORMAT_U24_BE:
        rsd_format = RSD_S32_LE;
        ao->format = AF_FORMAT_S32_LE;
        break;
    case AF_FORMAT_S32_LE:
        rsd_format = RSD_S32_LE;
        break;
    case AF_FORMAT_S32_BE:
        rsd_format = RSD_S32_BE;
        break;
    case AF_FORMAT_U32_LE:
        rsd_format = RSD_U32_LE;
        break;
    case AF_FORMAT_U32_BE:
        rsd_format = RSD_U32_BE;
        break;
    default:
        rsd_format = RSD_S16_LE;
        ao->format = AF_FORMAT_S16_LE;
    }

    return rsd_format;
}

static int init(struct ao *ao, char *params)
{
    struct priv *priv = talloc_zero(ao, struct priv);
    ao->priv = priv;

    char *host = NULL;
    char *port = NULL;

    const opt_t subopts[] = {
        {"host", OPT_ARG_MSTRZ, &host, NULL},
        {"port", OPT_ARG_MSTRZ, &port, NULL},
        {NULL}
    };

    if (subopt_parse(params, subopts) != 0)
        return -1;

    if (rsd_init(&priv->rd) < 0) {
        free(host);
        free(port);
        return -1;
    }

    if (host) {
        rsd_set_param(priv->rd, RSD_HOST, host);
        free(host);
    }

    if (port) {
        rsd_set_param(priv->rd, RSD_PORT, port);
        free(port);
    }

    mp_chmap_reorder_to_alsa(&ao->channels);

    rsd_set_param(priv->rd, RSD_SAMPLERATE, &ao->samplerate);
    rsd_set_param(priv->rd, RSD_CHANNELS, &ao->channels.num);

    int rsd_format = set_format(ao);
    rsd_set_param(priv->rd, RSD_FORMAT, &rsd_format);

    if (rsd_start(priv->rd) < 0) {
        rsd_free(priv->rd);
        return -1;
    }

    ao->bps = ao->channels.num * ao->samplerate * af_fmt2bits(ao->format) / 8;

    return 0;
}

static void uninit(struct ao *ao, bool cut_audio)
{
    struct priv *priv = ao->priv;
    /* The API does not provide a direct way to explicitly wait until
     * the last byte has been played server-side as this cannot be
     * guaranteed by backend drivers, so we approximate this behavior.
     */
    if (!cut_audio)
        usec_sleep(rsd_delay_ms(priv->rd) * 1000);

    rsd_stop(priv->rd);
    rsd_free(priv->rd);
}

static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;
    rsd_stop(priv->rd);
    rsd_start(priv->rd);
}

static void audio_pause(struct ao *ao)
{
    struct priv *priv = ao->priv;
    rsd_pause(priv->rd, 1);
}

static void audio_resume(struct ao *ao)
{
    struct priv *priv = ao->priv;
    rsd_pause(priv->rd, 0);
}

static int get_space(struct ao *ao)
{
    struct priv *priv = ao->priv;
    return rsd_get_avail(priv->rd);
}

static int play(struct ao *ao, void *data, int len, int flags)
{
    struct priv *priv = ao->priv;
    return rsd_write(priv->rd, data, len);
}

static float get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;
    return rsd_delay_ms(priv->rd) / 1000.0;
}

const struct ao_driver audio_out_rsound = {
    .is_new    = true,
    .info      = &(const struct ao_info) {
        .name       = "RSound output driver",
        .short_name = "rsound",
        .author     = "Hans-Kristian Arntzen",
        .comment    = "",
    },
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = audio_pause,
    .resume    = audio_resume,
};

