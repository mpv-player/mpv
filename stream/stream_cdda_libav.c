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
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include "stream.h"
#include "options/m_option.h"
#include "options/m_config.h"
#include "options/options.h"

typedef struct cdda_params {
    char *device;
} cdda_priv;

#define OPT_BASE_STRUCT struct cdda_params

static const m_option_t cdda_params_fields[] = {
    OPT_STRING("device", device, 0),
    {0}
};

const struct m_sub_options stream_cdda_conf = {
    .opts = (const m_option_t[]) {
        OPT_STRING("device", device, 0),
        {0}
    },
    .size = sizeof(struct cdda_params),
};

static int open_f(stream_t *stream)
{
    cdda_priv *priv = stream->priv;
    cdda_priv *p = priv;

    stream->type = STREAMTYPE_AVDEVICE;
    stream->demuxer = "lavf";

    if (!p->device || !p->device[0]) {
        talloc_free(p->device);
        if (stream->opts->cdrom_device && stream->opts->cdrom_device[0])
            p->device = talloc_strdup(stream, stream->opts->cdrom_device);
        else
            p->device = talloc_strdup(stream, DEFAULT_CDROM_DEVICE);
    }

    stream->url = talloc_asprintf(stream, "libcdio:%s", p->device);

    return STREAM_OK;
}

const stream_info_t stream_info_cdda = {
    .name = "cdda",
    .open = open_f,
    .protocols = (const char*const[]){"cdda", NULL },
    .priv_size = sizeof(cdda_priv),
    .options = cdda_params_fields,
    .url_options = (const char*const[]){
        "port=speed",
        "filename=device",
        NULL
    },
};
