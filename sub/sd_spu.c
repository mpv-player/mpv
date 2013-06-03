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
#include <assert.h>

#include "talloc.h"
#include "core/options.h"
#include "demux/stheader.h"
#include "sd.h"
#include "sub.h"
#include "spudec.h"

struct sd_spu_priv {
    void *spudec;
};

static bool is_dvd_sub(const char *t)
{
    return t && (strcmp(t, "dvd_subtitle") == 0 ||
                 strcmp(t, "dvd_subtitle_mpg") == 0);
}

static bool supports_format(const char *format)
{
    return is_dvd_sub(format);
}

static int init(struct sd *sd)
{
    void *spudec = spudec_new_scaled(sd->sub_video_w, sd->sub_video_h,
                                     sd->extradata, sd->extradata_len);
    if (!spudec)
        return -1;
    struct sd_spu_priv *priv = talloc_zero(NULL, struct sd_spu_priv);
    priv->spudec = spudec;
    sd->priv = priv;
    return 0;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    struct sd_spu_priv *priv = sd->priv;

    if (packet->pts < 0 || packet->len == 0)
        return;

    spudec_assemble(priv->spudec, packet->buffer, packet->len,
                    packet->pts * 90000);
}

static void get_bitmaps(struct sd *sd, struct mp_osd_res d, double pts,
                        struct sub_bitmaps *res)
{
    struct MPOpts *opts = sd->opts;
    struct sd_spu_priv *priv = sd->priv;

    spudec_set_forced_subs_only(priv->spudec, opts->forced_subs_only);
    spudec_heartbeat(priv->spudec, pts * 90000);

    if (spudec_visible(priv->spudec))
        spudec_get_indexed(priv->spudec, &d, res);
}

static void reset(struct sd *sd)
{
    struct sd_spu_priv *priv = sd->priv;

    spudec_reset(priv->spudec);
}

static void uninit(struct sd *sd)
{
    struct sd_spu_priv *priv = sd->priv;

    spudec_free(priv->spudec);
    talloc_free(priv);
}

const struct sd_functions sd_spu = {
    .name = "spu",
    .supports_format = supports_format,
    .init = init,
    .decode = decode,
    .get_bitmaps = get_bitmaps,
    .reset = reset,
    .uninit = uninit,
};
