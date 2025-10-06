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
#include <inttypes.h>
#include <math.h>

#include <libavutil/rational.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libplacebo/utils/libav.h>

#include "common/msg.h"
#include "common/common.h"
#include "filters/f_autoconvert.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "video/img_format.h"
#include "video/mp_image.h"

#include "options/m_option.h"

struct priv {
    struct vf_format_opts *opts;
    struct mp_autoconvert *conv;
};

struct vf_format_opts {
    int fmt;
    int colormatrix;
    int colorlevels;
    int primaries;
    int gamma;
    float sig_peak;
    int light;
    int chroma_location;
    int stereo_in;
    int rotate;
    int alpha;
    int w, h;
    int dw, dh;
    double dar;
    bool convert;
    int force_scaler;
    bool dovi;
    bool hdr10plus;
    bool film_grain;
};

static void set_params(struct vf_format_opts *p, struct mp_image_params *out,
                       bool set_size)
{
    if (p->colormatrix)
        out->repr.sys = p->colormatrix;
    if (p->colorlevels)
        out->repr.levels = p->colorlevels;
    if (p->primaries)
        out->color.primaries = p->primaries;
    if (p->gamma) {
        enum pl_color_transfer in_gamma = p->gamma;
        out->color.transfer = p->gamma;
        if (in_gamma != out->color.transfer) {
            // When changing the gamma function explicitly, also reset stuff
            // related to the gamma function since that information will almost
            // surely be false now and have to be re-inferred
            out->color.hdr = (struct pl_hdr_metadata){0};
            out->light = MP_CSP_LIGHT_AUTO;
        }
    }
    if (out->repr.sys != PL_COLOR_SYSTEM_DOLBYVISION) {
        out->primaries_orig = out->color.primaries;
        out->transfer_orig = out->color.transfer;
        out->sys_orig = out->repr.sys;
    }
    if (p->sig_peak)
        out->color.hdr = (struct pl_hdr_metadata){ .max_luma = p->sig_peak * MP_REF_WHITE };
    if (p->light)
        out->light = p->light;
    if (p->chroma_location)
        out->chroma_location = p->chroma_location;
    if (p->stereo_in)
        out->stereo3d = p->stereo_in;
    if (p->rotate >= 0)
        out->rotate = p->rotate;
    if (p->alpha)
        out->repr.alpha = p->alpha;

    if (p->w > 0 && set_size)
        out->w = p->w;
    if (p->h > 0 && set_size)
        out->h = p->h;
    AVRational dsize;
    mp_image_params_get_dsize(out, &dsize.num, &dsize.den);
    if (p->dw > 0)
        dsize.num = p->dw;
    if (p->dh > 0)
        dsize.den = p->dh;
    if (p->dar > 0)
        dsize = av_d2q(p->dar, INT_MAX);
    mp_image_params_set_dsize(out, dsize.num, dsize.den);
}

static inline void *get_side_data(const struct mp_image *mpi,
                                  enum AVFrameSideDataType type)
{
    for (int i = 0; i < mpi->num_ff_side_data; i++) {
        if (mpi->ff_side_data[i].type == type)
            return (void *)mpi->ff_side_data[i].buf->data;
    }
    return NULL;
}

static void vf_format_process(struct mp_filter *f)
{
    struct priv *priv = f->priv;

    if (mp_pin_can_transfer_data(priv->conv->f->pins[0], f->ppins[0])) {
        struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

        if (priv->opts->convert && frame.type == MP_FRAME_VIDEO) {
            struct mp_image *img = frame.data;
            struct mp_image_params par = img->params;
            int outfmt = priv->opts->fmt;

            // If we convert from RGB to YUV, default to limited range.
            if (mp_imgfmt_get_forced_csp(img->imgfmt) == PL_COLOR_SYSTEM_RGB &&
                outfmt && mp_imgfmt_get_forced_csp(outfmt) == PL_COLOR_SYSTEM_UNKNOWN)
            {
                par.repr.levels = PL_COLOR_LEVELS_LIMITED;
            }

            set_params(priv->opts, &par, true);

            if (outfmt && par.imgfmt != outfmt) {
                par.imgfmt = outfmt;
                par.hw_subfmt = 0;
            }
            mp_image_params_guess_csp(&par);

            mp_autoconvert_set_target_image_params(priv->conv, &par);
        }

        mp_pin_in_write(priv->conv->f->pins[0], frame);
    }

    if (mp_pin_can_transfer_data(f->ppins[1], priv->conv->f->pins[1])) {
        struct mp_frame frame = mp_pin_out_read(priv->conv->f->pins[1]);
        struct mp_image *img = frame.data;

        if (frame.type != MP_FRAME_VIDEO)
            goto write_out;

        if (!priv->opts->convert) {
            set_params(priv->opts, &img->params, false);
            mp_image_params_guess_csp(&img->params);
        }

        if (!priv->opts->dovi) {
            mp_image_params_restore_dovi_mapping(&img->params);
            // Map again to strip any DV metadata set to common fields.
            img->params.color.hdr = (struct pl_hdr_metadata){0};
            pl_map_hdr_metadata(&img->params.color.hdr, &(struct pl_av_hdr_metadata) {
                .mdm = get_side_data(img, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA),
                .clm = get_side_data(img, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL),
                .dhp = get_side_data(img, AV_FRAME_DATA_DYNAMIC_HDR_PLUS),
            });
        }

        if (!priv->opts->hdr10plus) {
            memset(img->params.color.hdr.scene_max, 0,
                   sizeof(img->params.color.hdr.scene_max));
            img->params.color.hdr.scene_avg = 0;
            img->params.color.hdr.ootf = (struct pl_hdr_bezier){0};
        }

        if (!priv->opts->film_grain)
            av_buffer_unref(&img->film_grain);

write_out:
        mp_pin_in_write(f->ppins[1], frame);
    }
}

static const struct mp_filter_info vf_format_filter = {
    .name = "format",
    .process = vf_format_process,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_format_create(struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &vf_format_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    struct priv *priv = f->priv;
    priv->opts = talloc_steal(priv, options);

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    priv->conv = mp_autoconvert_create(f);
    if (!priv->conv) {
        talloc_free(f);
        return NULL;
    }

    priv->conv->force_scaler = priv->opts->force_scaler;

    if (priv->opts->fmt)
        mp_autoconvert_add_imgfmt(priv->conv, priv->opts->fmt, 0);

    return f;
}

#define OPT_BASE_STRUCT struct vf_format_opts
static const m_option_t vf_opts_fields[] = {
    {"fmt", OPT_IMAGEFORMAT(fmt)},
    {"colormatrix", OPT_CHOICE_C(colormatrix, pl_csp_names)},
    {"colorlevels", OPT_CHOICE_C(colorlevels, pl_csp_levels_names)},
    {"primaries", OPT_CHOICE_C(primaries, pl_csp_prim_names)},
    {"gamma", OPT_CHOICE_C(gamma, pl_csp_trc_names)},
    {"transfer", OPT_ALIAS("gamma")},
    {"sig-peak", OPT_FLOAT(sig_peak)},
    {"light", OPT_CHOICE_C(light, mp_csp_light_names)},
    {"chroma-location", OPT_CHOICE_C(chroma_location, pl_chroma_names)},
    {"stereo-in", OPT_CHOICE_C(stereo_in, mp_stereo3d_names)},
    {"rotate", OPT_INT(rotate), M_RANGE(-1, 359)},
    {"alpha", OPT_CHOICE_C(alpha, pl_alpha_names)},
    {"w", OPT_INT(w)},
    {"h", OPT_INT(h)},
    {"dw", OPT_INT(dw)},
    {"dh", OPT_INT(dh)},
    {"dar", OPT_DOUBLE(dar)},
    {"convert", OPT_BOOL(convert)},
    {"dolbyvision", OPT_BOOL(dovi)},
    {"hdr10plus", OPT_BOOL(hdr10plus)},
    {"film-grain", OPT_BOOL(film_grain)},
    {"force-scaler", OPT_CHOICE(force_scaler,
                                {"auto", MP_SWS_AUTO},
                                {"sws", MP_SWS_SWS},
                                {"zimg", MP_SWS_ZIMG})},
    {0}
};

const struct mp_user_filter_entry vf_format = {
    .desc = {
        .description = "force output format",
        .name = "format",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT){
            .rotate = -1,
            .dovi = true,
            .hdr10plus = true,
            .film_grain = true,
        },
        .options = vf_opts_fields,
    },
    .create = vf_format_create,
};
