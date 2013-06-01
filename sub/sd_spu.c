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

static int init(struct sh_sub *sh, struct osd_state *osd)
{
    if (sh->initialized)
        return 0;
    void *spudec = spudec_new_scaled(osd->sub_video_w, osd->sub_video_h,
                                     sh->extradata, sh->extradata_len);
    if (!spudec)
        return -1;
    struct sd_spu_priv *priv = talloc_zero(NULL, struct sd_spu_priv);
    priv->spudec = spudec;
    sh->context = priv;
    return 0;
}

static void decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                   int data_len, double pts, double duration)
{
    struct sd_spu_priv *priv = sh->context;

    if (pts < 0 || data_len == 0)
        return;

    spudec_assemble(priv->spudec, data, data_len, pts * 90000);
}

static void get_bitmaps(struct sh_sub *sh, struct osd_state *osd,
                        struct mp_osd_res d, double pts,
                        struct sub_bitmaps *res)
{
    struct MPOpts *opts = sh->opts;
    struct sd_spu_priv *priv = sh->context;

    spudec_set_forced_subs_only(priv->spudec, opts->forced_subs_only);
    spudec_heartbeat(priv->spudec, pts * 90000);

    if (spudec_visible(priv->spudec))
        spudec_get_indexed(priv->spudec, &d, res);
}

static void reset(struct sh_sub *sh, struct osd_state *osd)
{
    struct sd_spu_priv *priv = sh->context;

    spudec_reset(priv->spudec);
}

static void uninit(struct sh_sub *sh)
{
    struct sd_spu_priv *priv = sh->context;

    spudec_free(priv->spudec);
    talloc_free(priv);
}

const struct sd_functions sd_spu = {
    .supports_format = supports_format,
    .init = init,
    .decode = decode,
    .get_bitmaps = get_bitmaps,
    .reset = reset,
    .switch_off = reset,
    .uninit = uninit,
};
