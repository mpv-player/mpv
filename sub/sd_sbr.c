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

#include <math.h>
#include <limits.h>

#include <subrandr/subrandr.h>

#include "mpv_talloc.h"

#include "options/m_config.h"
#include "options/options.h"
#include "common/common.h"
#include "demux/packet_pool.h"
#include "demux/stheader.h"
#include "sub/osd.h"
#include "video/mp_image.h"
#include "sd.h"

struct sd_sbr_priv {
    struct sbr_library *sbr_library;
    struct sbr_renderer *sbr_renderer;
    struct sbr_subtitles *sbr_subtitles;
    struct mp_osd_res osd;
    struct sub_bitmaps *bitmaps;
};

static void enable_output(struct sd *sd, bool enable)
{
    struct sd_sbr_priv *ctx = sd->priv;
    if (enable == !!ctx->sbr_renderer)
        return;
    if (ctx->sbr_renderer) {
        sbr_renderer_destroy(ctx->sbr_renderer);
        ctx->sbr_renderer = NULL;
    } else {
        ctx->sbr_renderer = sbr_renderer_create(ctx->sbr_library);
        mp_require(ctx->sbr_renderer);
    }
}

static int init(struct sd *sd)
{
    if (!sd->codec->codec)
        return -1;
    if (strcmp(sd->codec->codec, "subrandr/srv3") && strcmp(sd->codec->codec, "subrandr/vtt"))
        return -1;

    struct sd_sbr_priv *ctx = talloc_zero(sd, struct sd_sbr_priv);
    sd->priv = ctx;

    ctx->sbr_library = sbr_library_init();
    mp_require(ctx->sbr_library);
    ctx->bitmaps = talloc_zero(ctx, struct sub_bitmaps);
    ctx->bitmaps->format = SUBBITMAP_BGRA;
    ctx->bitmaps->num_parts = 1;
    ctx->bitmaps->parts = talloc_zero(ctx->bitmaps, struct sub_bitmap);

    enable_output(sd, true);

    return 0;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    struct sd_sbr_priv *ctx = sd->priv;

    if (ctx->sbr_subtitles)
        sbr_subtitles_destroy(ctx->sbr_subtitles);
    if (ctx->sbr_renderer)
        sbr_renderer_set_subtitles(ctx->sbr_renderer, NULL);

    sbr_subtitle_format fmt = SBR_SUBTITLE_FORMAT_UNKNOWN;
    if (!strcmp(sd->codec->codec, "subrandr/srv3") )
        fmt = SBR_SUBTITLE_FORMAT_SRV3;
    else if (!strcmp(sd->codec->codec, "subrandr/vtt"))
        fmt = SBR_SUBTITLE_FORMAT_WEBVTT;

    ctx->sbr_subtitles = sbr_load_text(
        ctx->sbr_library,
        packet->buffer,
        packet->len,
        fmt,
        NULL
    );
    packet->sub_duration = packet->duration;

    if (!ctx->sbr_subtitles) {
        const char *error = sbr_get_last_error_string();
        mp_err(sd->log, "Failed to load subtitles: %s\n", error);
    }
}

static struct sub_bitmaps *get_bitmaps(struct sd *sd, struct mp_osd_res dim,
                                       int format, double pts)
{
    struct sd_sbr_priv *ctx = sd->priv;
    struct mp_subtitle_opts *opts = sd->opts;

    ctx->osd = dim;

    if (pts == MP_NOPTS_VALUE || !ctx->sbr_renderer || !ctx->sbr_subtitles)
        return NULL;

    if (opts->sub_forced_events_only)
        return NULL;

    struct sbr_subtitle_context context = (sbr_subtitle_context) {
        .dpi = 72,
        .padding_top = (int32_t)dim.mt << 6,
        .padding_bottom = (int32_t)dim.mb << 6,
        .padding_left = (int32_t)dim.ml << 6,
        .padding_right = (int32_t)dim.mr << 6,
        .video_height = (int32_t)(dim.h - dim.mt - dim.mb) << 6,
        .video_width = (int32_t)(dim.w - dim.ml - dim.mr) << 6,
    };

    unsigned t = lrint(pts * 1000);

    struct sub_bitmaps *bitmaps = ctx->bitmaps;
    struct sub_bitmap *bitmap = bitmaps->parts;

    bool size_did_change = bitmap->w != dim.w || bitmap->h != dim.h;
    if (size_did_change || sbr_renderer_did_change(ctx->sbr_renderer, &context, t)) {
        talloc_free(bitmaps->packed);

        bitmaps->packed = mp_image_alloc(IMGFMT_BGRA, dim.w, dim.h);
        mp_require(bitmaps->packed);
        bitmaps->packed_h = dim.h;
        bitmaps->packed_w = dim.w;
        bitmaps->packed->params.repr.alpha = PL_ALPHA_PREMULTIPLIED;
        ++bitmaps->change_id;

        bitmap->bitmap = bitmaps->packed->planes[0];
        bitmap->w = dim.w;
        bitmap->h = dim.h;
        bitmap->dw = dim.w;
        bitmap->dh = dim.h;
        bitmap->stride = (*bitmaps->packed).stride[0];

        sbr_renderer_set_subtitles(ctx->sbr_renderer, ctx->sbr_subtitles);
        if (sbr_renderer_render(ctx->sbr_renderer, &context, t, bitmap->bitmap,
                                dim.w, dim.h, bitmap->stride >> 2) < 0) {
            const char *error = sbr_get_last_error_string();
            mp_err(sd->log, "Failed to render frame: %s\n", error);
            return NULL;
        }
    }

    return sub_bitmaps_copy(NULL, bitmaps);
}

static void uninit(struct sd *sd)
{
    struct sd_sbr_priv *ctx = sd->priv;

    talloc_free(ctx->bitmaps->packed);
    enable_output(sd, false);
    if (ctx->sbr_subtitles)
        sbr_subtitles_destroy(ctx->sbr_subtitles);
    sbr_library_fini(ctx->sbr_library);
}

static int control(struct sd *sd, enum sd_ctrl cmd, void *arg)
{
    switch (cmd) {
    default:
        return CONTROL_UNKNOWN;
    }
}

const struct sd_functions sd_sbr = {
    .name = "subrandr",
    .accept_packets_in_advance = true,
    .init = init,
    .decode = decode,
    .get_bitmaps = get_bitmaps,
    .control = control,
    .select = enable_output,
    .uninit = uninit,
};
