/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <sys/stat.h>

#include <libswscale/swscale.h>

#include "config.h"
#include "misc/bstr.h"
#include "osdep/io.h"
#include "options/m_config.h"
#include "options/path.h"
#include "mpv_talloc.h"
#include "common/common.h"
#include "common/msg.h"
#include "video/out/vo.h"
#include "video/csputils.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"
#include "video/image_writer.h"
#include "video/sws_utils.h"
#include "sub/osd.h"
#include "options/m_option.h"

static const struct m_sub_options image_writer_conf = {
    .opts = image_writer_opts,
    .size = sizeof(struct image_writer_opts),
    .defaults = &image_writer_opts_defaults,
};

struct vo_image_opts {
    struct image_writer_opts *opts;
    char *outdir;
};

#define OPT_BASE_STRUCT struct vo_image_opts

static const struct m_sub_options vo_image_conf = {
    .opts = (const struct m_option[]) {
        OPT_SUBSTRUCT("vo-image", opts, image_writer_conf, 0),
        OPT_STRING("vo-image-outdir", outdir, M_OPT_FILE),
        {0},
    },
    .size = sizeof(struct vo_image_opts),
};

struct priv {
    struct vo_image_opts *opts;

    struct mp_image *current;
    int frame;
};

static bool checked_mkdir(struct vo *vo, const char *buf)
{
    MP_INFO(vo, "Creating output directory '%s'...\n", buf);
    if (mkdir(buf, 0755) < 0) {
        char *errstr = mp_strerror(errno);
        if (errno == EEXIST) {
            struct stat stat_p;
            if (stat(buf, &stat_p ) == 0 && S_ISDIR(stat_p.st_mode))
                return true;
        }
        MP_ERR(vo, "Error creating output directory: %s\n", errstr);
        return false;
    }
    return true;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;
    mp_image_unrefp(&p->current);

    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;

    p->current = mpi;

    struct mp_osd_res dim = osd_res_from_image_params(vo->params);
    osd_draw_on_image(vo->osd, dim, mpi->pts, OSD_DRAW_SUB_ONLY, p->current);
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (!p->current)
        return;

    (p->frame)++;

    void *t = talloc_new(NULL);
    char *filename = talloc_asprintf(t, "%08d.%s", p->frame,
                                     image_writer_file_ext(p->opts->opts));

    if (p->opts->outdir && strlen(p->opts->outdir))
        filename = mp_path_join(t, p->opts->outdir, filename);

    MP_INFO(vo, "Saving %s\n", filename);
    write_image(p->current, p->opts->opts, filename, vo->log);

    talloc_free(t);
    mp_image_unrefp(&p->current);
}

static int query_format(struct vo *vo, int fmt)
{
    if (mp_sws_supported_format(fmt))
        return 1;
    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    mp_image_unrefp(&p->current);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    p->opts = mp_get_config_group(vo, vo->global, &vo_image_conf);
    if (p->opts->outdir && !checked_mkdir(vo, p->opts->outdir))
        return -1;
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

const struct vo_driver video_out_image =
{
    .description = "Write video frames to image files",
    .name = "image",
    .untimed = true,
    .priv_size = sizeof(struct priv),
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .global_opts = &vo_image_conf,
};
