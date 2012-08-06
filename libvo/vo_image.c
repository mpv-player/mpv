/*
 * This file is part of mplayer.
 *
 * mplayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include <libswscale/swscale.h>

#include "config.h"
#include "talloc.h"
#include "mp_msg.h"
#include "libvo/video_out.h"
#include "libvo/csputils.h"
#include "libmpcodecs/vfcap.h"
#include "libmpcodecs/mp_image.h"
#include "fmt-conversion.h"
#include "image_writer.h"
#include "m_config.h"
#include "m_option.h"

struct priv {
    struct image_writer_opts *opts;

    int frame;

    uint32_t d_width;
    uint32_t d_height;

    struct mp_csp_details colorspace;
};

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct priv *p = vo->priv;

    p->d_width = d_width;
    p->d_height = d_height;

    return 0;
}

static void check_events(struct vo *vo)
{
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
}

static void flip_page(struct vo *vo)
{
}

static uint32_t draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;

    mp_image_t tmp = *mpi;
    tmp.width = p->d_width;
    tmp.height = p->d_height;

    char filename[80];
    snprintf(filename, sizeof(filename), "%08d.%s", p->frame,
             image_writer_file_ext(p->opts));
    mp_msg(MSGT_VO, MSGL_STATUS, "Save %s!\n", filename);
    write_image(&tmp, &p->colorspace, p->opts, filename);

    (p->frame)++;

    return VO_TRUE;
}

static int query_format(struct vo *vo, uint32_t fmt)
{
    enum PixelFormat av_format = imgfmt2pixfmt(fmt);

    // NOTE: accept everything that can be converted by swscale. screenshot.c
    // always wants RGB (at least for now), but it probably doesn't matter
    // whether we or screenshot.c do the conversion.
    if (av_format != PIX_FMT_NONE && sws_isSupportedInput(av_format))
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |
               VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
    return 0;
}

static void uninit(struct vo *vo)
{
}

static int preinit(struct vo *vo, const char *arg)
{
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;

    switch (request) {
    case VOCTRL_QUERY_FORMAT:
        return query_format(vo, *(uint32_t *)data);
    case VOCTRL_DRAW_IMAGE:
        return draw_image(vo, data);
    case VOCTRL_SET_YUV_COLORSPACE:
        p->colorspace = *(struct mp_csp_details *)data;
        return true;
    case VOCTRL_GET_YUV_COLORSPACE:
        *(struct mp_csp_details *)data = p->colorspace;
        return true;
    // prevent random frame stepping by frontend
    case VOCTRL_REDRAW_FRAME:
        return true;
    }
    return VO_NOTIMPL;
}

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_image =
{
    .is_new = true,
    .info = &(const vo_info_t) {
        "Write video frames to image files",
        "image",
        "wm4",
        ""
    },
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .colorspace = MP_CSP_DETAILS_DEFAULTS,
    },
    .options = (const struct m_option[]) {
        OPT_SUBSTRUCT(opts, image_writer_conf, M_OPT_MERGE),
        {0},
    },
    .preinit = preinit,
    .config = config,
    .control = control,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
};
