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

#include <zimg.h>

#include "common/common.h"
#include "common/tags.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_option.h"
#include "video/img_format.h"
#include "video/sws_utils.h"

#include "osdep/timer.h"

#define ZIMG_ALIGN 32

#define PRINT_ENTRY_NUM 10

struct f_opts {
    int type;
    int clear;
    int print;
};

const struct m_opt_choice_alternatives type_names[] = {
    {"gray-hex-8x8",    8},
    {"gray-hex-16x16",  16},
    {0}
};

#define OPT_BASE_STRUCT struct f_opts
static const struct m_option f_opts_list[] = {
    OPT_CHOICE_C("type", type, 0, type_names),
    OPT_FLAG("clear-on-query", clear, 0),
    OPT_FLAG("print", print, 0),
    {0}
};

static const struct f_opts f_opts_def = {
    .type = 16,
    .clear = 1,
};

struct print_entry {
    double pts;
    char *print;
};

struct priv {
    struct f_opts *opts;
    struct mp_image *scaled;
    struct mp_sws_context *sws;
    struct print_entry entries[PRINT_ENTRY_NUM];
    int num_entries;
    int last_imgfmt, last_w, last_h;
    int gray_plane_imgfmt;
    zimg_filter_graph *zimg_graph;
    void *zimg_tmp;
};

// (Other code internal to this filter also calls this to reset the frame list.)
static void f_reset(struct mp_filter *f)
{
    struct priv *p = f->priv;

    for (int n = 0; n < p->num_entries; n++)
        talloc_free(p->entries[n].print);
    p->num_entries = 0;
}

static void reinit_fmt(struct mp_filter *f, struct mp_image *mpi)
{
    struct priv *p = f->priv;

    if (mpi->imgfmt == p->last_imgfmt &&
        mpi->w == p->last_w &&
        mpi->h == p->last_h)
        return;

    p->last_imgfmt = mpi->imgfmt;
    p->last_w = mpi->w;
    p->last_h = mpi->h;

    free(p->zimg_tmp);
    p->zimg_tmp = NULL;
    zimg_filter_graph_free(p->zimg_graph);
    p->zimg_graph = NULL;

    if (!(mpi->fmt.flags & (MP_IMGFLAG_YUV_NV | MP_IMGFLAG_YUV_P)))
        return;

    zimg_image_format src_fmt, dst_fmt;

    // Note: we try to pass only the first plane. Formats which do not have
    // such a luma plane are excluded above.
    zimg_image_format_default(&src_fmt, ZIMG_API_VERSION);
    src_fmt.width = mpi->w;
    src_fmt.height = mpi->h;
    src_fmt.color_family = ZIMG_COLOR_GREY;
    src_fmt.pixel_type = ZIMG_PIXEL_BYTE;
    src_fmt.depth = mpi->fmt.component_bits;
    src_fmt.pixel_range = mpi->params.color.levels == MP_CSP_LEVELS_PC ?
                          ZIMG_RANGE_FULL : ZIMG_RANGE_LIMITED;

    zimg_image_format_default(&dst_fmt, ZIMG_API_VERSION);
    dst_fmt.width = p->scaled->w;
    dst_fmt.height = p->scaled->h;
    dst_fmt.color_family = ZIMG_COLOR_GREY;
    dst_fmt.pixel_type = ZIMG_PIXEL_BYTE;
    dst_fmt.depth = 8;
    dst_fmt.pixel_range = ZIMG_RANGE_FULL;

    zimg_graph_builder_params params;
    zimg_graph_builder_params_default(&params, ZIMG_API_VERSION);
    params.resample_filter = ZIMG_RESIZE_BILINEAR;

    p->zimg_graph = zimg_filter_graph_build(&src_fmt, &dst_fmt, &params);
    if (!p->zimg_graph)
        return;

    size_t tmp_size;
    if (!zimg_filter_graph_get_tmp_size(p->zimg_graph, &tmp_size)) {
        if (posix_memalign(&p->zimg_tmp, ZIMG_ALIGN, tmp_size))
            p->zimg_tmp = NULL;
    }

    if (!p->zimg_tmp) {
        zimg_filter_graph_free(p->zimg_graph);
        p->zimg_graph = NULL;
    }
}

static void f_process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_pin_can_transfer_data(f->ppins[1], f->ppins[0]))
        return;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

    if (mp_frame_is_signaling(frame)) {
        mp_pin_in_write(f->ppins[1], frame);
        return;
    }

    if (frame.type != MP_FRAME_VIDEO)
        goto error;

    struct mp_image *mpi = frame.data;

    reinit_fmt(f, mpi);

    if (p->zimg_graph &&
        !((uintptr_t)mpi->planes[0] % ZIMG_ALIGN) &&
        !(mpi->stride[0] % ZIMG_ALIGN))
    {
        zimg_image_buffer_const src_buf = {ZIMG_API_VERSION};
        src_buf.plane[0].data = mpi->planes[0];
        src_buf.plane[0].stride = mpi->stride[0];
        src_buf.plane[0].mask = ZIMG_BUFFER_MAX;
        zimg_image_buffer dst_buf = {ZIMG_API_VERSION};
        dst_buf.plane[0].data = p->scaled->planes[0];
        dst_buf.plane[0].stride = p->scaled->stride[0];
        dst_buf.plane[0].mask = ZIMG_BUFFER_MAX;
        // (The API promises to succeed if no user callbacks fail, so no need
        // to check the return value.)
        zimg_filter_graph_process(p->zimg_graph, &src_buf, &dst_buf,
                                  p->zimg_tmp, NULL, NULL, NULL, NULL);
    } else {
        if (mp_sws_scale(p->sws, p->scaled, mpi) < 0)
            goto error;
    }

    if (p->num_entries >= PRINT_ENTRY_NUM) {
        talloc_free(p->entries[0].print);
        MP_TARRAY_REMOVE_AT(p->entries, p->num_entries, 0);
    }

    int size = p->scaled->w;

    struct print_entry *e = &p->entries[p->num_entries++];
    e->pts = mpi->pts;
    e->print = talloc_array(p, char, size * size * 2 + 1);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            char *offs = &e->print[(y * size + x) * 2];
            uint8_t v = p->scaled->planes[0][y * p->scaled->stride[0] + x];
            snprintf(offs, 3, "%02x", v);
        }
    }

    if (p->opts->print)
        MP_INFO(f, "%f: %s\n", e->pts, e->print);

    mp_pin_in_write(f->ppins[1], frame);
    return;

error:
    MP_ERR(f, "unsupported video format\n");
    mp_pin_in_write(f->ppins[1], frame);
    mp_filter_internal_mark_failed(f);
}

static bool f_command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct priv *p = f->priv;

    switch (cmd->type) {
    case MP_FILTER_COMMAND_GET_META: {
        struct mp_tags *t = talloc_zero(NULL, struct mp_tags);

        for (int n = 0; n < p->num_entries; n++) {
            struct print_entry *e = &p->entries[n];

            if (e->pts != MP_NOPTS_VALUE) {
                mp_tags_set_str(t, mp_tprintf(80, "fp%d.pts", n),
                                   mp_tprintf(80, "%f", e->pts));
            }
            mp_tags_set_str(t, mp_tprintf(80, "fp%d.hex", n), e->print);
        }

        mp_tags_set_str(t, "type", m_opt_choice_str(type_names, p->opts->type));

        if (p->opts->clear)
            f_reset(f);

        *(struct mp_tags **)cmd->res = t;
        return true;
    }
    default:
        return false;
    }
}

static const struct mp_filter_info filter = {
    .name = "fingerprint",
    .process = f_process,
    .command = f_command,
    .reset = f_reset,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *f_create(struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);
    int size = p->opts->type;
    p->scaled = mp_image_alloc(IMGFMT_Y8, size, size);
    MP_HANDLE_OOM(p->scaled);
    talloc_steal(p, p->scaled);
    p->scaled->params.color.levels = MP_CSP_LEVELS_PC;
    p->sws = mp_sws_alloc(p);
    MP_HANDLE_OOM(p->sws);
    return f;
}

const struct mp_user_filter_entry vf_fingerprint = {
    .desc = {
        .description = "'Compute video frame fingerprints",
        .name = "fingerprint",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &f_opts_def,
        .options = f_opts_list,
    },
    .create = f_create,
};
