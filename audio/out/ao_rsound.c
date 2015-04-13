/*
 * RSound audio output driver
 *
 * Copyright (C) 2011 Hans-Kristian Arntzen
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rsound.h>

#include "talloc.h"

#include "options/m_option.h"
#include "osdep/timer.h"
#include "audio/format.h"
#include "ao.h"
#include "internal.h"

struct priv {
    rsound_t *rd;
    char *host;
    char *port;
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
    case AF_FORMAT_S16:
        rsd_format = RSD_S16_NE;
        break;
    case AF_FORMAT_U16:
        rsd_format = RSD_U16_NE;
        break;
    case AF_FORMAT_S24:
    case AF_FORMAT_U24:
        rsd_format = RSD_S32_NE;
        ao->format = AF_FORMAT_S32;
        break;
    case AF_FORMAT_S32:
        rsd_format = RSD_S32_NE;
        break;
    case AF_FORMAT_U32:
        rsd_format = RSD_U32_NE;
        break;
    default:
        rsd_format = RSD_S16_NE;
        ao->format = AF_FORMAT_S16;
    }

    return rsd_format;
}

static int init(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (rsd_init(&priv->rd) < 0)
        return -1;

    if (priv->host && priv->host[0])
        rsd_set_param(priv->rd, RSD_HOST, priv->host);

    if (priv->port && priv->port[0])
        rsd_set_param(priv->rd, RSD_PORT, priv->port);

    // Actual channel layout unknown.
    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext_def(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels)) {
        rsd_free(priv->rd);
        return -1;
    }

    rsd_set_param(priv->rd, RSD_SAMPLERATE, (int[]) { ao->samplerate });
    rsd_set_param(priv->rd, RSD_CHANNELS, (int[]) { ao->channels.num });

    ao->format = af_fmt_from_planar(ao->format);

    int rsd_format = set_format(ao);
    rsd_set_param(priv->rd, RSD_FORMAT, &rsd_format);

    if (rsd_start(priv->rd) < 0) {
        rsd_free(priv->rd);
        return -1;
    }

    return 0;
}

static void uninit(struct ao *ao)
{
    struct priv *priv = ao->priv;

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
    return rsd_get_avail(priv->rd) / ao->sstride;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *priv = ao->priv;
    return rsd_write(priv->rd, data[0], samples * ao->sstride) / ao->sstride;
}

static double get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;
    return rsd_delay_ms(priv->rd) / 1000.0;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_rsound = {
    .description = "RSound output driver",
    .name      = "rsound",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .priv_size = sizeof(struct priv),
    .options   = (const struct m_option[]) {
        OPT_STRING("host", host, 0),
        OPT_STRING("port", port, 0),
        {0}
    },
};

