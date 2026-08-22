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

#include <libplacebo/gpu.h>
#include <libplacebo/options.h>
#include <libplacebo/renderer.h>
#include <libplacebo/shaders/lut.h>
#include <libplacebo/log.h>
#include <libplacebo/swapchain.h>
#include <libplacebo/utils/libav.h>
#include <libplacebo/utils/frame_queue.h>

#include "osdep/threads.h"
#include "misc/mp_assert.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "video/out/libmpv.h"
#include "video/out/gpu/video.h"
#include "video/out/gpu/video_shaders.h"
#include "video/out/placebo/utils.h"
#include "video/mp_image.h"

#include "sub/osd.h"
#include "sub/draw_bmp.h"

#include "mpv/render_placebo.h"



struct osd_entry {
    pl_tex tex;
    struct pl_overlay_part *parts;
    int num_parts;
};

struct overlay_state {
    struct osd_entry entries[MAX_OSD_PARTS];
    struct pl_overlay overlays[MAX_OSD_PARTS];
};

struct frame_priv {
    struct priv *p;  // Back-reference to main priv
    struct overlay_state subs;
    uint64_t osd_sync;
};

struct priv {
    struct mp_log *log;
    struct mpv_global *global;

    // libplacebo objects
    pl_log pllog;
    pl_gpu gpu;
    pl_renderer rr;
    pl_queue queue;
    pl_swapchain swapchain;
    bool external_pllog;

    // Allocated DR buffers
    mp_mutex dr_lock;
    pl_buf *dr_buffers;
    int num_dr_buffers;

    // OSD state
    pl_fmt osd_fmt[SUBBITMAP_COUNT];
    pl_tex *sub_tex;
    int num_sub_tex;
    struct overlay_state osd_state;
    uint64_t osd_sync;

    // Frame state
    struct mp_rect src, dst;
    struct mp_osd_res osd_res;
    struct mp_image_params img_params;
    struct osd_state *osd;  // OSD source from vo
    uint64_t last_id;
    double last_pts;
    bool want_reset;

    // Rendering options
    pl_options pars;
    struct m_config_cache *opts_cache;

    int last_w;
    int last_h;
};

static void update_options(struct priv *p);

static pl_buf get_dr_buf(struct priv *p, const uint8_t *ptr)
{
    mp_mutex_lock(&p->dr_lock);

    for (int i = 0; i < p->num_dr_buffers; i++) {
        pl_buf buf = p->dr_buffers[i];
        if (ptr >= buf->data && ptr < buf->data + buf->params.size) {
            mp_mutex_unlock(&p->dr_lock);
            return buf;
        }
    }

    mp_mutex_unlock(&p->dr_lock);
    return NULL;
}

static void free_dr_buf(void *opaque, uint8_t *data)
{
    struct priv *p = opaque;
    mp_mutex_lock(&p->dr_lock);

    for (int i = 0; i < p->num_dr_buffers; i++) {
        if (p->dr_buffers[i]->data == data) {
            pl_buf_destroy(p->gpu, &p->dr_buffers[i]);
            MP_TARRAY_REMOVE_AT(p->dr_buffers, p->num_dr_buffers, i);
            mp_mutex_unlock(&p->dr_lock);
            return;
        }
    }

    MP_ASSERT_UNREACHABLE();
}

static int plane_data_from_imgfmt(struct pl_plane_data out_data[4],
                                  struct pl_bit_encoding *out_bits,
                                  enum mp_imgfmt imgfmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(imgfmt);
    if (!desc.num_planes || !(desc.flags & MP_IMGFLAG_HAS_COMPS))
        return 0;
    if (desc.flags & MP_IMGFLAG_HWACCEL)
        return 0;

    if (!(desc.flags & MP_IMGFLAG_NE))
        return 0;

    if (desc.flags & MP_IMGFLAG_PAL)
        return 0;

    if ((desc.flags & MP_IMGFLAG_TYPE_FLOAT) && (desc.flags & MP_IMGFLAG_YUV))
        return 0;

    bool has_bits = false;

    for (int p = 0; p < desc.num_planes; p++) {
        struct pl_plane_data *data = &out_data[p];
        struct mp_imgfmt_comp_desc sorted[MP_NUM_COMPONENTS];
        int num_comps = 0;
        if (desc.bpp[p] % 8)
            return 0;

        for (int c = 0; c < mp_imgfmt_desc_get_num_comps(&desc); c++) {
            if (desc.comps[c].plane != p)
                continue;

            data->component_map[num_comps] = c;
            sorted[num_comps] = desc.comps[c];
            num_comps++;

            for (int i = num_comps - 1; i > 0; i--) {
                if (sorted[i].offset >= sorted[i - 1].offset)
                    break;
                MPSWAP(struct mp_imgfmt_comp_desc, sorted[i], sorted[i - 1]);
                MPSWAP(int, data->component_map[i], data->component_map[i - 1]);
            }
        }

        uint64_t total_bits = 0;
        memset(data->component_size, 0, sizeof(data->component_size));
        for (int c = 0; c < num_comps; c++) {
            data->component_size[c] = sorted[c].size;
            data->component_pad[c] = sorted[c].offset - total_bits;
            total_bits += data->component_pad[c] + data->component_size[c];

            if (!out_bits || data->component_map[c] == PL_CHANNEL_A)
                continue;

            struct pl_bit_encoding bits = {
                .sample_depth = data->component_size[c],
                .color_depth = sorted[c].size - abs(sorted[c].pad),
                .bit_shift = MPMAX(sorted[c].pad, 0),
            };

            if (!has_bits) {
                *out_bits = bits;
                has_bits = true;
            } else {
                if (!pl_bit_encoding_equal(out_bits, &bits)) {
                    *out_bits = (struct pl_bit_encoding) {0};
                    out_bits = NULL;
                }
            }
        }

        data->pixel_stride = desc.bpp[p] / 8;
        data->type = (desc.flags & MP_IMGFLAG_TYPE_FLOAT)
                            ? PL_FMT_FLOAT
                            : PL_FMT_UNORM;
    }

    return desc.num_planes;
}

static bool format_supported(struct priv *p, int format)
{
    struct pl_bit_encoding bits;
    struct pl_plane_data data[4] = {0};
    int planes = plane_data_from_imgfmt(data, &bits, format);
    if (!planes)
        return false;

    for (int i = 0; i < planes; i++) {
        if (!pl_plane_find_fmt(p->gpu, NULL, &data[i]))
            return false;
    }

    return true;
}

static int init(struct render_backend *ctx, mpv_render_param *params)
{
    ctx->priv = talloc_zero(NULL, struct priv);
    struct priv *p = ctx->priv;
    p->log = ctx->log;
    p->global = ctx->global;

    char *api = get_mpv_render_param(params, MPV_RENDER_PARAM_API_TYPE, NULL);
    if (!api || strcmp(api, MPV_RENDER_API_TYPE_LIBPLACEBO) != 0)
        return MPV_ERROR_NOT_IMPLEMENTED;

    // Get optional pl_log
    pl_log external_log = get_mpv_render_param(params, (mpv_render_param_type) MPV_RENDER_PARAM_LIBPLACEBO_PL_LOG, NULL);

    // Get required swapchain
    p->swapchain = get_mpv_render_param(params, (mpv_render_param_type) MPV_RENDER_PARAM_LIBPLACEBO_SWAPCHAIN, NULL);
    if (!p->swapchain) {
        MP_ERR(p, "MPV_RENDER_PARAM_LIBPLACEBO_SWAPCHAIN is required\n");
        return MPV_ERROR_INVALID_PARAMETER;
    }

    // Create or use pl_log
    if (external_log) {
        p->pllog = external_log;
        p->external_pllog = true;
    } else {
        p->pllog = mppl_log_create(p, p->log);
        if (!p->pllog) {
            MP_ERR(p, "Failed to create libplacebo log\n");
            return MPV_ERROR_GENERIC;
        }
        p->external_pllog = false;
    }

    p->gpu = p->swapchain->gpu;
    if (!p->gpu) {
        MP_ERR(p, "Failed to obtain GPU context\n");
        return MPV_ERROR_GENERIC;
    }

    // Create renderer and queue
    p->rr = pl_renderer_create(p->pllog, p->gpu);
    if (!p->rr) {
        MP_ERR(p, "Failed to create libplacebo renderer\n");
        return MPV_ERROR_GENERIC;
    }

    p->queue = pl_queue_create(p->gpu);
    if (!p->queue) {
        MP_ERR(p, "Failed to create frame queue\n");
        return MPV_ERROR_GENERIC;
    }

    // Initialize OSD formats
    p->osd_fmt[SUBBITMAP_LIBASS] = pl_find_named_fmt(p->gpu, "r8");
    p->osd_fmt[SUBBITMAP_BGRA] = pl_find_named_fmt(p->gpu, "bgra8");
    p->osd_sync = 1;

    // Initialize render options
    p->pars = pl_options_alloc(p->pllog);
    p->opts_cache = m_config_cache_alloc(p, p->global, &gl_video_conf);

    // Note: Hardware decoding not supported by this backend.
    // libmpv render API with external swapchain doesn't have ra_ctx,
    // required by libmpv's hwdec structure.
    // TODO: Implement an alternative to ra_hwdec_driver for this backend.
    // Or find some other way.
    ctx->hwdec_devs = NULL;

    // Set capabilities
    ctx->driver_caps = VO_CAP_ROTATE90 | VO_CAP_FILM_GRAIN | VO_CAP_VFLIP;

    // Initialize DR buffer mutex
    mp_mutex_init(&p->dr_lock);

    p->last_h = p->last_w = 0;
    return 0;
}

static bool check_format(struct render_backend *ctx, int imgfmt)
{
    struct priv *p = ctx->priv;

    return format_supported(p, imgfmt);
}

static int set_parameter(struct render_backend *ctx, mpv_render_param param)
{
    // struct priv *p = ctx->priv;
    (void)ctx;

    switch (param.type) {
    case MPV_RENDER_PARAM_ICC_PROFILE: {
        // ICC profile handling would go here
        // For now, we don't support dynamic ICC profile changes
        return MPV_ERROR_NOT_IMPLEMENTED;
    }
    default:
        return MPV_ERROR_NOT_IMPLEMENTED;
    }
}

static void reconfig(struct render_backend *ctx, struct mp_image_params *params)
{
    struct priv *p = ctx->priv;
    p->img_params = *params;
    p->want_reset = true;
}

static void reset(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;
    p->want_reset = true;
}

static void update_external(struct render_backend *ctx, struct vo *vo)
{
    struct priv *p = ctx->priv;
    p->osd = vo ? vo->osd : NULL;
}

static void resize(struct render_backend *ctx, struct mp_rect *src,
                   struct mp_rect *dst, struct mp_osd_res *osd)
{
    struct priv *p = ctx->priv;

    // Store the positioning info - dst contains the centered position
    // calculated by mp_get_src_dst_rects based on video alignment options
    p->src = *src;
    p->dst = *dst;
    p->osd_res = *osd;
}

static int get_target_size(struct render_backend *ctx, mpv_render_param *params,
                           int *out_w, int *out_h)
{
    struct priv *p = ctx->priv;

    *out_w = p->last_w;
    *out_h = p->last_h;
    return 0;
}

static void update_overlays(struct priv *p, struct mp_osd_res res,
                            struct pl_frame *frame, struct overlay_state *state,
                            struct osd_state *osd, double pts)
{
    if (!osd)
        return;

    struct sub_bitmap_list *subs = osd_render(osd, res, pts, 0, mp_draw_sub_formats);

    frame->overlays = state->overlays;
    frame->num_overlays = 0;

    for (int n = 0; n < subs->num_items; n++) {
        const struct sub_bitmaps *item = subs->items[n];
        if (!item->num_parts || !item->packed)
            continue;

        struct osd_entry *entry = &state->entries[item->render_index];
        pl_fmt tex_fmt = p->osd_fmt[item->format];

        if (!entry->tex)
            MP_TARRAY_POP(p->sub_tex, p->num_sub_tex, &entry->tex);

        bool ok = pl_tex_recreate(p->gpu, &entry->tex, &(struct pl_tex_params) {
            .format = tex_fmt,
            .w = item->packed_w,
            .h = item->packed_h,
            .host_writable = true,
            .sampleable = true,
        });

        if (!ok) {
            MP_ERR(p, "Failed recreating OSD texture!\n");
            break;
        }

        ok = pl_tex_upload(p->gpu, &(struct pl_tex_transfer_params) {
            .tex        = entry->tex,
            .rc         = { .x1 = item->packed_w, .y1 = item->packed_h, },
            .row_pitch  = item->packed->stride[0],
            .ptr        = item->packed->planes[0],
        });

        if (!ok) {
            MP_ERR(p, "Failed uploading OSD texture!\n");
            break;
        }

        entry->num_parts = 0;
        for (int i = 0; i < item->num_parts; i++) {
            const struct sub_bitmap *b = &item->parts[i];
            if (b->dw == 0 || b->dh == 0)
                continue;

            uint32_t c = b->libass.color;
            struct pl_overlay_part part = {
                .src = { b->src_x, b->src_y, b->src_x + b->w, b->src_y + b->h },
                .dst = { b->x, b->y, b->x + b->dw, b->y + b->dh },
                .color = {
                    (c >> 24) / 255.0f,
                    ((c >> 16) & 0xFF) / 255.0f,
                    ((c >> 8) & 0xFF) / 255.0f,
                    (255 - (c & 0xFF)) / 255.0f,
                }
            };
            MP_TARRAY_APPEND(p, entry->parts, entry->num_parts, part);
        }

        struct pl_overlay *ol = &state->overlays[frame->num_overlays++];
        *ol = (struct pl_overlay) {
            .tex = entry->tex,
            .parts = entry->parts,
            .num_parts = entry->num_parts,
            .color = {
                .primaries = PL_COLOR_PRIM_BT_709,
                .transfer = PL_COLOR_TRC_SRGB,
            },
            .coords = PL_OVERLAY_COORDS_DST_FRAME,
        };

        switch (item->format) {
        case SUBBITMAP_BGRA:
            ol->mode = PL_OVERLAY_NORMAL;
            ol->repr.alpha = PL_ALPHA_PREMULTIPLIED;
            break;
        case SUBBITMAP_LIBASS:
            ol->mode = PL_OVERLAY_MONOCHROME;
            ol->repr.alpha = PL_ALPHA_INDEPENDENT;
            break;
        }
    }

    talloc_free(subs);
}

static bool map_frame(pl_gpu gpu, pl_tex *tex, const struct pl_source_frame *src,
                      struct pl_frame *frame)
{
    struct mp_image *mpi = src->frame_data;
    if (!mpi)
        return false;

    struct frame_priv *fp = mpi->priv;
    struct priv *p = fp->p;

    struct mp_image_params par = mpi->params;
    mp_image_params_guess_csp(&par);

    *frame = (struct pl_frame) {
        .color = par.color,
        .repr = par.repr,
        .profile = {
            .data = mpi->icc_profile ? mpi->icc_profile->data : NULL,
            .len = mpi->icc_profile ? mpi->icc_profile->size : 0,
        },
        .rotation = par.rotate / 90,
        .user_data = mpi,
    };

    // Software decoding path (hwdec not supported in this backend)
    struct pl_plane_data data[4] = {0};
    frame->num_planes = plane_data_from_imgfmt(data, &frame->repr.bits, mpi->imgfmt);

    if (!frame->num_planes) {
        // Unsupported format (likely hwdec frame - use --hwdec=no)
        MP_ERR(p, "Unsupported frame format: %s (use --hwdec=no)\n", mp_imgfmt_to_name(mpi->imgfmt));
        talloc_free(mpi);
        return false;
    }

    for (int n = 0; n < frame->num_planes; n++) {
        struct pl_plane *plane = &frame->planes[n];
        data[n].width = mp_image_plane_w(mpi, n);
        data[n].height = mp_image_plane_h(mpi, n);

        if (mpi->stride[n] < 0) {
            data[n].pixels = mpi->planes[n] + (data[n].height - 1) * mpi->stride[n];
            data[n].row_stride = -mpi->stride[n];
            plane->flipped = true;
        } else {
            data[n].pixels = mpi->planes[n];
            data[n].row_stride = mpi->stride[n];
        }

        // Check if frame is in a DR buffer (zero-copy path)
        pl_buf buf = get_dr_buf(p, data[n].pixels);
        if (buf) {
            data[n].buf = buf;
            data[n].buf_offset = (uint8_t *) data[n].pixels - buf->data;
            data[n].pixels = NULL;
        } else if (gpu->limits.callbacks) {
            data[n].callback = talloc_free;
            data[n].priv = mp_image_new_ref(mpi);
        }

        if (!pl_upload_plane(gpu, plane, &tex[n], &data[n])) {
            MP_ERR(p, "Failed uploading frame!\n");
            talloc_free(mpi);
            return false;
        }
    }

    pl_frame_set_chroma_location(frame, par.chroma_location);

    if (mpi->film_grain)
        pl_film_grain_from_av(&frame->film_grain, (AVFilmGrainParams *) mpi->film_grain->data);

    return true;
}

static void unmap_frame(pl_gpu gpu, struct pl_frame *frame,
                        const struct pl_source_frame *src)
{
    struct mp_image *mpi = src->frame_data;
    struct frame_priv *fp = mpi->priv;
    struct priv *p = fp->p;

    // Save OSD textures for reuse
    for (int i = 0; i < MP_ARRAY_SIZE(fp->subs.entries); i++) {
        pl_tex tex = fp->subs.entries[i].tex;
        if (tex)
            MP_TARRAY_APPEND(p, p->sub_tex, p->num_sub_tex, tex);
    }
    talloc_free(mpi);
}

static void discard_frame(const struct pl_source_frame *src)
{
    struct mp_image *mpi = src->frame_data;
    talloc_free(mpi);
}

static int render(struct render_backend *ctx, mpv_render_param *params,
                  struct vo_frame *frame)
{
    struct priv *p = ctx->priv;
    mpv_render_rect viewport = get_mpv_render_param(
        params, (mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_VIEWPORT,
        NULL);
    update_options(p);

    const struct gl_video_opts *opts = p->opts_cache->opts;
    bool can_interpolate = opts->interpolation && frame->display_synced &&
                           !frame->still && frame->num_frames > 1;
    double pts_offset = can_interpolate ? frame->ideal_frame_vsync : 0;

    struct pl_source_frame vpts;
    if (frame->current && !p->want_reset) {
        // Check for backward PTS jump
        if (pl_queue_peek(p->queue, 0, &vpts) &&
            frame->current->pts + MPMAX(0, pts_offset) < vpts.pts)
        {
            MP_VERBOSE(p, "PTS discontinuity (backward): current=%f queue=%f\n",
                       frame->current->pts, vpts.pts);
            p->want_reset = true;
        }
        // Check for large forward PTS jump (common in streams)
        if (p->last_pts > 0 && frame->current->pts > p->last_pts + 5.0) {
            MP_VERBOSE(p, "PTS discontinuity (forward): current=%f last=%f\n",
                       frame->current->pts, p->last_pts);
            p->want_reset = true;
        }
    }

    // Push incoming frames to queue
    for (int n = 0; n < frame->num_frames; n++) {
        int id = frame->frame_id + n;

        // Handle reset inside the loop to properly handle discontinuities
        if (p->want_reset) {
            pl_renderer_flush_cache(p->rr);
            pl_queue_reset(p->queue);
            p->last_pts = 0.0;
            p->last_id = 0;
            p->want_reset = false;
        }

        if (id <= p->last_id)
            continue;

        struct mp_image *mpi = mp_image_new_ref(frame->frames[n]);
        if (!mpi) {
            MP_ERR(p, "Failed to ref frame\n");
            continue;
        }
        struct frame_priv *fp = talloc_zero(mpi, struct frame_priv);
        fp->p = p;
        mpi->priv = fp;

        pl_queue_push(p->queue, &(struct pl_source_frame) {
            .pts = mpi->pts,
            .duration = can_interpolate ? frame->approx_duration : 0,
            .frame_data = mpi,
            .map = map_frame,
            .unmap = unmap_frame,
            .discard = discard_frame,
        });

        p->last_id = id;
    }

    struct pl_swapchain_frame* swframe = NULL;
    struct pl_swapchain_frame internal_swframe;
    bool use_application_swframe = false;
    {
        struct pl_swapchain_frame *application_swframe = get_mpv_render_param(
            params, (mpv_render_param_type)MPV_RENDER_PARAM_LIBPLACEBO_FRAME,
            NULL);
        if (!application_swframe) {
            if (!pl_swapchain_start_frame(p->swapchain, &internal_swframe)) {
                if (frame->current) {
                    struct pl_queue_params qparams = *pl_queue_params(
                        .pts = frame->current->pts + pts_offset,
                        .radius = pl_frame_mix_radius(&p->pars->params),
                    );
                    pl_queue_update(p->queue, NULL, &qparams);
                }
                return MPV_ERROR_GENERIC;
            }
            swframe = &internal_swframe;
        } else {
            swframe = application_swframe;
            use_application_swframe = true;
        }
    }

    p->last_w = swframe->fbo->params.w;
    p->last_h = swframe->fbo->params.h;

    if (viewport) {
        // Get video dimensions
        int video_w = p->src.x1 - p->src.x0;
        int video_h = p->src.y1 - p->src.y0;
        if (video_w <= 0 || video_h <= 0) {
            // Fallback to image params if src not set
            video_w = swframe->fbo->params.w;
            video_h = swframe->fbo->params.h;
        }

        // Get viewport dimensions
        double vp_w = viewport->x1 - viewport->x0;
        double vp_h = viewport->y1 - viewport->y0;

        // Calculate aspect ratios
        double video_aspect = (double)video_w / video_h;
        double viewport_aspect = vp_w / vp_h;

        // Calculate destination rect that fits video in viewport with aspect ratio
        double dst_w, dst_h;
        if (video_aspect > viewport_aspect) {
            // Video is wider - fit to width
            dst_w = vp_w;
            dst_h = vp_w / video_aspect;
        } else {
            // Video is taller - fit to height
            dst_h = vp_h;
            dst_w = vp_h * video_aspect;
        }

        // Center in viewport
        double offset_x = (vp_w - dst_w) / 2.0;
        double offset_y = (vp_h - dst_h) / 2.0;

        // Set destination crop
        p->dst.x0 = viewport->x0 + offset_x;
        p->dst.y0 = viewport->y0 + offset_y;
        p->dst.x1 = viewport->x0 + offset_x + dst_w;
        p->dst.y1 = viewport->y0 + offset_y + dst_h;
    }


    struct pl_frame target;
    pl_frame_from_swapchain(&target, swframe);
    target.overlays = NULL;
    target.num_overlays = 0;

    // Set up target crop
    if (p->dst.x1 > p->dst.x0 && p->dst.y1 > p->dst.y0) {
        target.crop = (struct pl_rect2df) {
            .x0 = p->dst.x0,
            .y0 = p->dst.y0,
            .x1 = p->dst.x1,
            .y1 = p->dst.y1,
        };
    } else {
        target.crop = (struct pl_rect2df) {
            .x0 = 0,
            .y0 = 0,
            .x1 = swframe->fbo->params.w,
            .y1 = swframe->fbo->params.h,
        };
    }

    // Get frame mix from queue
    struct pl_frame_mix mix = {0};
    if (frame->current) {
        struct pl_queue_params qparams = *pl_queue_params(
            .pts = frame->current->pts + pts_offset,
            .radius = pl_frame_mix_radius(&p->pars->params),
            .vsync_duration = can_interpolate ? frame->ideal_frame_vsync_duration : 0,
            .interpolation_threshold = opts->interpolation_threshold,
        );
#if PL_API_VER >= 340
        qparams.drift_compensation = 0;
#endif

        struct pl_source_frame first;
        if (pl_queue_peek(p->queue, 0, &first) && qparams.pts < first.pts) {
            qparams.pts = first.pts;
        }
        p->last_pts = qparams.pts;

        switch (pl_queue_update(p->queue, &mix, &qparams)) {
        case PL_QUEUE_ERR:
            MP_ERR(p, "Failed updating frame queue!\n");
            pl_tex_clear(p->gpu, swframe->fbo, (float[4]){ 0.5, 0.0, 1.0, 1.0 });
            goto submit;
        case PL_QUEUE_EOF:
        case PL_QUEUE_MORE:
        case PL_QUEUE_OK:
            break;
        }

        // Apply source crop to all frames
        for (int i = 0; i < mix.num_frames; i++) {
            struct pl_frame *image = (struct pl_frame *) mix.frames[i];
            struct mp_image *mpi = image->user_data;
            if (p->src.x1 > p->src.x0 && p->src.y1 > p->src.y0) {
                image->crop = (struct pl_rect2df) {
                    .x0 = p->src.x0,
                    .y0 = p->src.y0,
                    .x1 = p->src.x1,
                    .y1 = p->src.y1,
                };
            } else if (mpi) {
                image->crop = (struct pl_rect2df) {
                    .x0 = 0,
                    .y0 = 0,
                    .x1 = mpi->params.w,
                    .y1 = mpi->params.h,
                };
            }
        }
    }
    // Update OSD overlays on target frame
    double pts = frame->current ? frame->current->pts : 0;
    update_overlays(p, p->osd_res, &target, &p->osd_state, p->osd, pts);

    // Render
    struct pl_render_params render_params = p->pars->params;

    // Only enable frame mixing when we can actually interpolate
    if (!can_interpolate || mix.num_frames < 2)
        render_params.frame_mixer = NULL;

    if (mix.num_frames > 0) {
        if (!pl_render_image_mix(p->rr, &mix, &target, &render_params)) {
            MP_ERR(p, "Failed rendering frame!\n");
            pl_tex_clear(p->gpu, swframe->fbo, (float[4]){ 0.5, 0.0, 1.0, 1.0 });
        }
    } else {
        pl_tex_clear(p->gpu, swframe->fbo, (float[4]){ 0.0, 0.0, 0.0, 1.0 });
    }

    // Flush GPU commands
    pl_gpu_flush(p->gpu);

submit:
    if (!use_application_swframe) {
        if (!pl_swapchain_submit_frame(p->swapchain)) {
            MP_ERR(p, "Failed submitting frame to swapchain!\n");
            return MPV_ERROR_GENERIC;
        }

        pl_swapchain_swap_buffers(p->swapchain);
    }

    return 0;
}

static struct mp_image *get_image(struct render_backend *ctx, int imgfmt,
                                  int w, int h, int stride_align, int flags)
{
    struct priv *p = ctx->priv;
    pl_gpu gpu = p->gpu;

    if (!gpu->limits.thread_safe || !gpu->limits.max_mapped_size)
        return NULL;

    if ((flags & VO_DR_FLAG_HOST_CACHED) && !gpu->limits.host_cached)
        return NULL;

    stride_align = mp_lcm(stride_align, gpu->limits.align_tex_xfer_pitch);
    stride_align = mp_lcm(stride_align, gpu->limits.align_tex_xfer_offset);
    int size = mp_image_get_alloc_size(imgfmt, w, h, stride_align);
    if (size < 0)
        return NULL;

    pl_buf buf = pl_buf_create(gpu, &(struct pl_buf_params) {
        .memory_type = PL_BUF_MEM_HOST,
        .host_mapped = true,
        .size = size + stride_align,
    });

    if (!buf)
        return NULL;

    struct mp_image *mpi = mp_image_from_buffer(imgfmt, w, h, stride_align,
                                                buf->data, buf->params.size,
                                                p, free_dr_buf);
    if (!mpi) {
        pl_buf_destroy(gpu, &buf);
        return NULL;
    }

    mp_mutex_lock(&p->dr_lock);
    MP_TARRAY_APPEND(p, p->dr_buffers, p->num_dr_buffers, buf);
    mp_mutex_unlock(&p->dr_lock);

    return mpi;
}

static void screenshot(struct render_backend *ctx, struct vo_frame *frame,
                       struct voctrl_screenshot *args)
{
    // Not implemented
    args->res = NULL;
}

static void perfdata(struct render_backend *ctx,
                     struct voctrl_performance_data *out)
{
    // Not implemented
    memset(out, 0, sizeof(*out));
}

static void destroy(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;

    if (!p)
        return;

    // Destroy queue first
    pl_queue_destroy(&p->queue);

    // Destroy OSD textures
    for (int i = 0; i < MP_ARRAY_SIZE(p->osd_state.entries); i++)
        pl_tex_destroy(p->gpu, &p->osd_state.entries[i].tex);
    for (int i = 0; i < p->num_sub_tex; i++)
        pl_tex_destroy(p->gpu, &p->sub_tex[i]);

    // Destroy hwdec
    mp_assert(p->num_dr_buffers == 0);
    mp_mutex_destroy(&p->dr_lock);

    // Destroy renderer
    pl_renderer_destroy(&p->rr);

    // Destroy options
    pl_options_free(&p->pars);

    // Destroy pl_log if we created it
    if (!p->external_pllog && p->pllog)
        pl_log_destroy(&p->pllog);

    talloc_free(p);
    ctx->priv = NULL;
}

static void update_options(struct priv *p)
{
    m_config_cache_update(p->opts_cache);

    // Apply basic render options from config
    pl_options pars = p->pars;
    const struct gl_video_opts *opts = p->opts_cache->opts;

    pars->params.background_color[0] = opts->background_color.r / 255.0;
    pars->params.background_color[1] = opts->background_color.g / 255.0;
    pars->params.background_color[2] = opts->background_color.b / 255.0;
    pars->params.background_transparency = 1 - opts->background_color.a / 255.0;

    // Deband params
    pars->params.deband_params = opts->deband ? &pars->deband_params : NULL;
    if (opts->deband && opts->deband_opts) {
        pars->deband_params.iterations = opts->deband_opts->iterations;
        pars->deband_params.radius = opts->deband_opts->range;
        pars->deband_params.threshold = opts->deband_opts->threshold / 16.384;
        pars->deband_params.grain = opts->deband_opts->grain / 8.192;
    }

    // Sigmoid params
    pars->params.sigmoid_params = opts->sigmoid_upscaling ? &pars->sigmoid_params : NULL;
    if (opts->sigmoid_upscaling) {
        pars->sigmoid_params.center = opts->sigmoid_center;
        pars->sigmoid_params.slope = opts->sigmoid_slope;
    }
}

const struct render_backend_fns render_backend_libplacebo = {
    .init = init,
    .check_format = check_format,
    .set_parameter = set_parameter,
    .reconfig = reconfig,
    .reset = reset,
    .update_external = update_external,
    .resize = resize,
    .get_target_size = get_target_size,
    .render = render,
    .get_image = get_image,
    .screenshot = screenshot,
    .perfdata = perfdata,
    .destroy = destroy,
};
