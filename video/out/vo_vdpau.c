/*
 * VDPAU video output driver
 *
 * Copyright (C) 2008 NVIDIA (Rajib Mahapatra <rmahapatra@nvidia.com>)
 * Copyright (C) 2009 Uoti Urpala
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

/*
 * Actual decoding is done in video/decode/vdpau.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>

#include <libavutil/common.h>

#include "config.h"
#include "video/vdpau.h"
#include "video/vdpau_mixer.h"
#include "video/hwdec.h"
#include "common/msg.h"
#include "options/options.h"
#include "talloc.h"
#include "vo.h"
#include "x11_common.h"
#include "video/csputils.h"
#include "sub/osd.h"
#include "options/m_option.h"
#include "video/mp_image.h"
#include "osdep/timer.h"
#include "bitmap_packer.h"

// Returns x + a, but wrapped around to the range [0, m)
// a must be within [-m, m], x within [0, m)
#define WRAP_ADD(x, a, m) ((a) < 0 \
                           ? ((x)+(a)+(m) < (m) ? (x)+(a)+(m) : (x)+(a)) \
                           : ((x)+(a) < (m) ? (x)+(a) : (x)+(a)-(m)))


/* number of video and output surfaces */
#define MAX_OUTPUT_SURFACES                15

/* Pixelformat used for output surfaces */
#define OUTPUT_RGBA_FORMAT VDP_RGBA_FORMAT_B8G8R8A8

/*
 * Global variable declaration - VDPAU specific
 */

struct vdpctx {
    struct mp_vdpau_ctx               *mpvdp;
    struct vdp_functions              *vdp;
    VdpDevice                          vdp_device;
    uint64_t                           preemption_counter;
    struct mp_hwdec_info               hwdec_info;

    struct m_color                     colorkey;

    VdpPresentationQueueTarget         flip_target;
    VdpPresentationQueue               flip_queue;

    VdpOutputSurface                   output_surfaces[MAX_OUTPUT_SURFACES];
    int                                num_output_surfaces;
    VdpOutputSurface                   black_pixel;

    struct mp_image                   *current_image;

    int                                output_surface_width, output_surface_height;

    int                                force_yuv;
    struct mp_vdpau_mixer             *video_mixer;
    int                                deint;
    int                                pullup;
    float                              denoise;
    float                              sharpen;
    int                                hqscaling;
    int                                chroma_deint;
    int                                flip_offset_window;
    int                                flip_offset_fs;
    int64_t                            flip_offset_us;

    VdpRect                            src_rect_vid;
    VdpRect                            out_rect_vid;
    struct mp_osd_res                  osd_rect;

    int                                surface_num; // indexes output_surfaces
    int                                query_surface_num;
    VdpTime                            recent_vsync_time;
    float                              user_fps;
    int                                composite_detect;
    unsigned int                       vsync_interval;
    uint64_t                           last_queue_time;
    uint64_t                           queue_time[MAX_OUTPUT_SURFACES];
    uint64_t                           last_ideal_time;
    bool                               dropped_frame;
    uint64_t                           dropped_time;
    uint32_t                           vid_width, vid_height;
    uint32_t                           image_format;
    VdpChromaType                      vdp_chroma_type;
    VdpYCbCrFormat                     vdp_pixel_format;
    bool                               rgb_mode;

    // OSD
    struct osd_bitmap_surface {
        VdpRGBAFormat format;
        VdpBitmapSurface surface;
        uint32_t max_width;
        uint32_t max_height;
        struct bitmap_packer *packer;
        // List of surfaces to be rendered
        struct osd_target {
            VdpRect source;
            VdpRect dest;
            VdpColor color;
        } *targets;
        int targets_size;
        int render_count;
        int change_id;
    } osd_surfaces[MAX_OSD_PARTS];

    // Video equalizer
    struct mp_csp_equalizer video_eq;
};

static bool status_ok(struct vo *vo);
static void draw_osd(struct vo *vo);

static int render_video_to_output_surface(struct vo *vo,
                                          VdpOutputSurface output_surface,
                                          VdpRect *output_rect,
                                          VdpRect *video_rect)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpTime dummy;
    VdpStatus vdp_st;
    struct mp_image *mpi = vc->current_image;

    vdp_st = vdp->presentation_queue_block_until_surface_idle(vc->flip_queue,
                                                              output_surface,
                                                              &dummy);
    CHECK_VDP_WARNING(vo, "Error when calling "
                      "vdp_presentation_queue_block_until_surface_idle");

    // Clear the borders between video and window (if there are any).
    // For some reason, video_mixer_render doesn't need it for YUV.
    // Also, if there is nothing to render, at least clear the screen.
    if (vc->rgb_mode || !mpi) {
        int flags = VDP_OUTPUT_SURFACE_RENDER_ROTATE_0;
        vdp_st = vdp->output_surface_render_output_surface(output_surface,
                                                           NULL, vc->black_pixel,
                                                           NULL, NULL, NULL,
                                                           flags);
        CHECK_VDP_WARNING(vo, "Error clearing screen");
    }

    if (!mpi)
        return -1;

    struct mp_vdpau_mixer_frame *frame = mp_vdpau_mixed_frame_get(mpi);
    struct mp_vdpau_mixer_opts opts = {0};
    if (frame)
        opts = frame->opts;

    // Apply custom vo_vdpau suboptions.
    opts.chroma_deint |= vc->chroma_deint;
    opts.pullup |= vc->pullup;
    opts.denoise = MPCLAMP(opts.denoise + vc->denoise, 0, 1);
    opts.sharpen = MPCLAMP(opts.sharpen + vc->sharpen, -1, 1);
    if (vc->hqscaling)
        opts.hqscaling = vc->hqscaling;

    mp_vdpau_mixer_render(vc->video_mixer, &opts, output_surface, output_rect,
                          mpi, video_rect);
    return 0;
}

static int video_to_output_surface(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    int r = render_video_to_output_surface(vo,
                                           vc->output_surfaces[vc->surface_num],
                                           &vc->out_rect_vid, &vc->src_rect_vid);
    draw_osd(vo);
    return r;
}

static void forget_frames(struct vo *vo, bool seek_reset)
{
    struct vdpctx *vc = vo->priv;

    if (!seek_reset)
        mp_image_unrefp(&vc->current_image);

    vc->dropped_frame = false;
}

static int s_size(int s, int disp)
{
    disp = MPMAX(1, disp);
    s += s / 2;
    return s >= disp ? s : disp;
}

static void resize(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    struct mp_rect src_rect;
    struct mp_rect dst_rect;
    vo_get_src_dst_rects(vo, &src_rect, &dst_rect, &vc->osd_rect);
    vc->out_rect_vid.x0 = dst_rect.x0;
    vc->out_rect_vid.x1 = dst_rect.x1;
    vc->out_rect_vid.y0 = dst_rect.y0;
    vc->out_rect_vid.y1 = dst_rect.y1;
    vc->src_rect_vid.x0 = src_rect.x0;
    vc->src_rect_vid.x1 = src_rect.x1;
    vc->src_rect_vid.y0 = src_rect.y0;
    vc->src_rect_vid.y1 = src_rect.y1;

    vc->flip_offset_us = vo->opts->fullscreen ?
                         1000LL * vc->flip_offset_fs :
                         1000LL * vc->flip_offset_window;
    vo_set_flip_queue_params(vo, vc->flip_offset_us, false);

    if (vc->output_surface_width < vo->dwidth
        || vc->output_surface_height < vo->dheight) {
        vc->output_surface_width = s_size(vc->output_surface_width, vo->dwidth);
        vc->output_surface_height = s_size(vc->output_surface_height, vo->dheight);
        // Creation of output_surfaces
        for (int i = 0; i < vc->num_output_surfaces; i++)
            if (vc->output_surfaces[i] != VDP_INVALID_HANDLE) {
                vdp_st = vdp->output_surface_destroy(vc->output_surfaces[i]);
                CHECK_VDP_WARNING(vo, "Error when calling "
                                  "vdp_output_surface_destroy");
            }
        for (int i = 0; i < vc->num_output_surfaces; i++) {
            vdp_st = vdp->output_surface_create(vc->vdp_device,
                                                OUTPUT_RGBA_FORMAT,
                                                vc->output_surface_width,
                                                vc->output_surface_height,
                                                &vc->output_surfaces[i]);
            CHECK_VDP_WARNING(vo, "Error when calling vdp_output_surface_create");
            MP_DBG(vo, "vdpau out create: %u\n",
                   vc->output_surfaces[i]);
        }
    }
    vo->want_redraw = true;
}

static int win_x11_init_vdpau_flip_queue(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    struct vo_x11_state *x11 = vo->x11;
    VdpStatus vdp_st;

    if (vc->flip_target == VDP_INVALID_HANDLE) {
        vdp_st = vdp->presentation_queue_target_create_x11(vc->vdp_device,
                                                           x11->window,
                                                           &vc->flip_target);
        CHECK_VDP_ERROR(vo, "Error when calling "
                        "vdp_presentation_queue_target_create_x11");
    }

    /* Emperically this seems to be the first call which fails when we
     * try to reinit after preemption while the user is still switched
     * from X to a virtual terminal (creating the vdp_device initially
     * succeeds, as does creating the flip_target above). This is
     * probably not guaranteed behavior.
     */
    if (vc->flip_queue == VDP_INVALID_HANDLE) {
        vdp_st = vdp->presentation_queue_create(vc->vdp_device, vc->flip_target,
                                                &vc->flip_queue);
        CHECK_VDP_ERROR(vo, "Error when calling vdp_presentation_queue_create");
    }

    if (vc->colorkey.a > 0) {
        VdpColor color = {
            .red = vc->colorkey.r / 255.0,
            .green = vc->colorkey.g / 255.0,
            .blue = vc->colorkey.b / 255.0,
            .alpha = 0,
        };
        vdp_st = vdp->presentation_queue_set_background_color(vc->flip_queue,
                                                              &color);
        CHECK_VDP_WARNING(vo, "Error setting colorkey");
    }

    if (vc->composite_detect && vo_x11_screen_is_composited(vo)) {
        MP_INFO(vo, "Compositing window manager detected. Assuming timing info "
                "is inaccurate.\n");
        vc->user_fps = -1;
    }

    return 0;
}

// Free everything specific to a certain video file
static void free_video_specific(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;

    forget_frames(vo, false);

    if (vc->black_pixel != VDP_INVALID_HANDLE) {
        vdp_st = vdp->output_surface_destroy(vc->black_pixel);
        CHECK_VDP_WARNING(vo, "Error when calling vdp_output_surface_destroy");
    }
    vc->black_pixel = VDP_INVALID_HANDLE;
}

static int initialize_vdpau_objects(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;

    mp_vdpau_get_format(vc->image_format, &vc->vdp_chroma_type,
                        &vc->vdp_pixel_format);

    vc->video_mixer->chroma_type = vc->vdp_chroma_type;
    vc->video_mixer->initialized = false;

    if (win_x11_init_vdpau_flip_queue(vo) < 0)
        return -1;

    if (vc->black_pixel == VDP_INVALID_HANDLE) {
        vdp_st = vdp->output_surface_create(vc->vdp_device, OUTPUT_RGBA_FORMAT,
                                            1, 1, &vc->black_pixel);
        CHECK_VDP_ERROR(vo, "Allocating clearing surface");
        const char data[4] = {0};
        vdp_st = vdp->output_surface_put_bits_native(vc->black_pixel,
                                                     (const void*[]){data},
                                                     (uint32_t[]){4}, NULL);
        CHECK_VDP_ERROR(vo, "Initializing clearing surface");
    }

    forget_frames(vo, false);
    resize(vo);
    return 0;
}

static void mark_vdpau_objects_uninitialized(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    forget_frames(vo, false);
    vc->black_pixel = VDP_INVALID_HANDLE;
    vc->video_mixer->video_mixer = VDP_INVALID_HANDLE;
    vc->flip_queue = VDP_INVALID_HANDLE;
    vc->flip_target = VDP_INVALID_HANDLE;
    for (int i = 0; i < MAX_OUTPUT_SURFACES; i++)
        vc->output_surfaces[i] = VDP_INVALID_HANDLE;
    vc->vdp_device = VDP_INVALID_HANDLE;
    for (int i = 0; i < MAX_OSD_PARTS; i++) {
        struct osd_bitmap_surface *sfc = &vc->osd_surfaces[i];
        talloc_free(sfc->packer);
        sfc->change_id = 0;
        *sfc = (struct osd_bitmap_surface){
            .surface = VDP_INVALID_HANDLE,
        };
    }
    vc->output_surface_width = vc->output_surface_height = -1;
}

static bool check_preemption(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    int r = mp_vdpau_handle_preemption(vc->mpvdp, &vc->preemption_counter);
    if (r < 1) {
        mark_vdpau_objects_uninitialized(vo);
        if (r < 0)
            return false;
        vc->vdp_device = vc->mpvdp->vdp_device;
        if (initialize_vdpau_objects(vo) < 0)
            return false;
    }
    return true;
}

static bool status_ok(struct vo *vo)
{
    return vo->config_ok && check_preemption(vo);
}

/*
 * connect to X server, create and map window, initialize all
 * VDPAU objects, create different surfaces etc.
 */
static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct vdpctx *vc = vo->priv;

    if (!check_preemption(vo))
        return -1;

    vc->image_format = params->imgfmt;
    vc->vid_width    = params->w;
    vc->vid_height   = params->h;

    vc->rgb_mode = mp_vdpau_get_rgb_format(params->imgfmt, NULL);

    free_video_specific(vo);

    vo_x11_config_vo_window(vo, NULL, flags, "vdpau");

    if (initialize_vdpau_objects(vo) < 0)
        return -1;

    return 0;
}

static struct bitmap_packer *make_packer(struct vo *vo, VdpRGBAFormat format)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;

    struct bitmap_packer *packer = talloc_zero(vo, struct bitmap_packer);
    uint32_t w_max = 0, h_max = 0;
    VdpStatus vdp_st = vdp->
        bitmap_surface_query_capabilities(vc->vdp_device, format,
                                          &(VdpBool){0}, &w_max, &h_max);
    CHECK_VDP_WARNING(vo, "Query to get max OSD surface size failed");
    packer->w_max = w_max;
    packer->h_max = h_max;
    return packer;
}

static void draw_osd_part(struct vo *vo, int index)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    struct osd_bitmap_surface *sfc = &vc->osd_surfaces[index];
    VdpOutputSurface output_surface = vc->output_surfaces[vc->surface_num];
    int i;

    VdpOutputSurfaceRenderBlendState blend_state = {
        .struct_version = VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION,
        .blend_factor_source_color =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA,
        .blend_factor_source_alpha =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO,
        .blend_factor_destination_color =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .blend_factor_destination_alpha =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO,
        .blend_equation_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
        .blend_equation_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
    };

    VdpOutputSurfaceRenderBlendState blend_state_premultiplied = blend_state;
    blend_state_premultiplied.blend_factor_source_color =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;

    for (i = 0; i < sfc->render_count; i++) {
        VdpOutputSurfaceRenderBlendState *blend = &blend_state;
        if (sfc->format == VDP_RGBA_FORMAT_B8G8R8A8)
            blend = &blend_state_premultiplied;
        vdp_st = vdp->
            output_surface_render_bitmap_surface(output_surface,
                                                 &sfc->targets[i].dest,
                                                 sfc->surface,
                                                 &sfc->targets[i].source,
                                                 &sfc->targets[i].color,
                                                 blend,
                                                 VDP_OUTPUT_SURFACE_RENDER_ROTATE_0);
        CHECK_VDP_WARNING(vo, "OSD: Error when rendering");
    }
}

static void generate_osd_part(struct vo *vo, struct sub_bitmaps *imgs)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    struct osd_bitmap_surface *sfc = &vc->osd_surfaces[imgs->render_index];
    bool need_upload = false;

    if (imgs->change_id == sfc->change_id)
        return; // Nothing changed and we still have the old data

    sfc->render_count = 0;

    if (imgs->format == SUBBITMAP_EMPTY || imgs->num_parts == 0)
        return;

    need_upload = true;
    VdpRGBAFormat format;
    int format_size;
    switch (imgs->format) {
    case SUBBITMAP_LIBASS:
        format = VDP_RGBA_FORMAT_A8;
        format_size = 1;
        break;
    case SUBBITMAP_RGBA:
        format = VDP_RGBA_FORMAT_B8G8R8A8;
        format_size = 4;
        break;
    default:
        abort();
    };
    if (sfc->format != format) {
        talloc_free(sfc->packer);
        sfc->packer = NULL;
    };
    sfc->format = format;
    if (!sfc->packer)
        sfc->packer = make_packer(vo, format);
    sfc->packer->padding = imgs->scaled; // assume 2x2 filter on scaling
    int r = packer_pack_from_subbitmaps(sfc->packer, imgs);
    if (r < 0) {
        MP_ERR(vo, "OSD bitmaps do not fit on a surface with the maximum "
               "supported size\n");
        return;
    } else if (r == 1) {
        if (sfc->surface != VDP_INVALID_HANDLE) {
            vdp_st = vdp->bitmap_surface_destroy(sfc->surface);
            CHECK_VDP_WARNING(vo, "Error when calling vdp_bitmap_surface_destroy");
        }
        MP_VERBOSE(vo, "Allocating a %dx%d surface for OSD bitmaps.\n",
                   sfc->packer->w, sfc->packer->h);
        vdp_st = vdp->bitmap_surface_create(vc->vdp_device, format,
                                            sfc->packer->w, sfc->packer->h,
                                            true, &sfc->surface);
        if (vdp_st != VDP_STATUS_OK)
            sfc->surface = VDP_INVALID_HANDLE;
        CHECK_VDP_WARNING(vo, "OSD: error when creating surface");
    }
    if (imgs->scaled) {
        char zeros[sfc->packer->used_width * format_size];
        memset(zeros, 0, sizeof(zeros));
        vdp_st = vdp->bitmap_surface_put_bits_native(sfc->surface,
                &(const void *){zeros}, &(uint32_t){0},
                &(VdpRect){0, 0, sfc->packer->used_width,
                                 sfc->packer->used_height});
        CHECK_VDP_WARNING(vo, "OSD: error uploading OSD bitmap");
    }

    if (sfc->surface == VDP_INVALID_HANDLE)
        return;
    if (sfc->packer->count > sfc->targets_size) {
        talloc_free(sfc->targets);
        sfc->targets_size = sfc->packer->count;
        sfc->targets = talloc_size(vc, sfc->targets_size
                                       * sizeof(*sfc->targets));
    }

    for (int i = 0 ;i < sfc->packer->count; i++) {
        struct sub_bitmap *b = &imgs->parts[i];
        struct osd_target *target = sfc->targets + sfc->render_count;
        int x = sfc->packer->result[i].x;
        int y = sfc->packer->result[i].y;
        target->source = (VdpRect){x, y, x + b->w, y + b->h};
        target->dest = (VdpRect){b->x, b->y, b->x + b->dw, b->y + b->dh};
        target->color = (VdpColor){1, 1, 1, 1};
        if (imgs->format == SUBBITMAP_LIBASS) {
            uint32_t color = b->libass.color;
            target->color.alpha = 1.0 - ((color >> 0) & 0xff) / 255.0;
            target->color.blue  = ((color >>  8) & 0xff) / 255.0;
            target->color.green = ((color >> 16) & 0xff) / 255.0;
            target->color.red   = ((color >> 24) & 0xff) / 255.0;
        }
        if (need_upload) {
            vdp_st = vdp->
                bitmap_surface_put_bits_native(sfc->surface,
                                               &(const void *){b->bitmap},
                                               &(uint32_t){b->stride},
                                               &target->source);
                CHECK_VDP_WARNING(vo, "OSD: putbits failed");
        }
        sfc->render_count++;
    }

    sfc->change_id = imgs->change_id;
}

static void draw_osd_cb(void *ctx, struct sub_bitmaps *imgs)
{
    struct vo *vo = ctx;
    generate_osd_part(vo, imgs);
    draw_osd_part(vo, imgs->render_index);
}

static void draw_osd(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    if (!status_ok(vo))
        return;

    static const bool formats[SUBBITMAP_COUNT] = {
        [SUBBITMAP_LIBASS] = true,
        [SUBBITMAP_RGBA] = true,
    };

    double pts = vc->current_image ? vc->current_image->pts : 0;
    osd_draw(vo->osd, vc->osd_rect, pts, 0, formats, draw_osd_cb, vo);
}

static int update_presentation_queue_status(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;

    while (vc->query_surface_num != vc->surface_num) {
        VdpTime vtime;
        VdpPresentationQueueStatus status;
        VdpOutputSurface surface = vc->output_surfaces[vc->query_surface_num];
        vdp_st = vdp->presentation_queue_query_surface_status(vc->flip_queue,
                                                              surface,
                                                              &status, &vtime);
        CHECK_VDP_WARNING(vo, "Error calling "
                         "presentation_queue_query_surface_status");
        if (mp_msg_test(vo->log, MSGL_TRACE)) {
            VdpTime current;
            vdp_st = vdp->presentation_queue_get_time(vc->flip_queue, &current);
            CHECK_VDP_WARNING(vo, "Error when calling "
                              "vdp_presentation_queue_get_time");
            MP_TRACE(vo, "Vdpau time: %"PRIu64"\n", (uint64_t)current);
            MP_TRACE(vo, "Surface %d status: %d time: %"PRIu64"\n",
                     (int)surface, (int)status, (uint64_t)vtime);
        }
        if (status == VDP_PRESENTATION_QUEUE_STATUS_QUEUED)
            break;
        if (vc->vsync_interval > 1) {
            uint64_t qtime = vc->queue_time[vc->query_surface_num];
            int diff = ((int64_t)vtime - (int64_t)qtime) / 1e6;
            MP_TRACE(vo, "Queue time difference: %d ms\n", diff);
            if (vtime < qtime + vc->vsync_interval / 2)
                MP_VERBOSE(vo, "Frame shown too early (%d ms)\n", diff);
            if (vtime > qtime + vc->vsync_interval)
                MP_VERBOSE(vo, "Frame shown late (%d ms)\n", diff);
        }
        vc->query_surface_num = WRAP_ADD(vc->query_surface_num, 1,
                                         vc->num_output_surfaces);
        vc->recent_vsync_time = vtime;
    }
    int num_queued = WRAP_ADD(vc->surface_num, -vc->query_surface_num,
                              vc->num_output_surfaces);
    MP_DBG(vo, "Queued surface count (before add): %d\n", num_queued);
    return num_queued;
}

// Return the timestamp of the vsync that must have happened before ts.
static inline uint64_t prev_vsync(struct vdpctx *vc, uint64_t ts)
{
    int64_t diff = (int64_t)(ts - vc->recent_vsync_time);
    int64_t offset = diff % vc->vsync_interval;
    if (offset < 0)
        offset += vc->vsync_interval;
    return ts - offset;
}

static int flip_page_timed(struct vo *vo, int64_t pts_us, int duration)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;

    vc->dropped_frame = true; // changed at end if false

    if (!check_preemption(vo))
        return 0;

    vc->vsync_interval = 1;
    if (vc->user_fps > 0) {
        vc->vsync_interval = 1e9 / vc->user_fps;
    } else if (vc->user_fps == 0) {
        vc->vsync_interval = vo_get_vsync_interval(vo) * 1000;
    }

    if (duration > INT_MAX / 1000)
        duration = -1;
    else
        duration *= 1000;

    if (vc->vsync_interval == 1)
        duration = -1;  // Make sure drop logic is disabled

    VdpTime vdp_time = 0;
    vdp_st = vdp->presentation_queue_get_time(vc->flip_queue, &vdp_time);
    CHECK_VDP_WARNING(vo, "Error when calling vdp_presentation_queue_get_time");

    int64_t rel_pts_ns = (pts_us - mp_time_us()) * 1000;
    if (!pts_us || rel_pts_ns < 0)
        rel_pts_ns = 0;

    uint64_t now = vdp_time;
    uint64_t pts = now + rel_pts_ns;
    uint64_t ideal_pts = pts;
    uint64_t npts = duration >= 0 ? pts + duration : UINT64_MAX;

    /* This should normally never happen.
     * - The last queued frame can't have a PTS that goes more than 50ms in the
     *   future. This is guaranteed by vo.c, which currently actually queues
     *   ahead by roughly the flip queue offset. Just to be sure
     *   give some additional room by doubling the time.
     * - The last vsync can never be in the future.
     */
    int64_t max_pts_ahead = vc->flip_offset_us * 1000 * 2;
    if (vc->last_queue_time > now + max_pts_ahead ||
        vc->recent_vsync_time > now)
    {
        vc->last_queue_time = 0;
        vc->recent_vsync_time = 0;
        MP_WARN(vo, "Inconsistent timing detected.\n");
    }

#define PREV_VSYNC(ts) prev_vsync(vc, ts)

    /* We hope to be here at least one vsync before the frame should be shown.
     * If we are running late then don't drop the frame unless there is
     * already one queued for the next vsync; even if we _hope_ to show the
     * next frame soon enough to mean this one should be dropped we might
     * not make the target time in reality. Without this check we could drop
     * every frame, freezing the display completely if video lags behind.
     */
    if (now > PREV_VSYNC(FFMAX(pts, vc->last_queue_time + vc->vsync_interval)))
        npts = UINT64_MAX;

    /* Allow flipping a frame at a vsync if its presentation time is a
     * bit after that vsync and the change makes the flip time delta
     * from previous frame better match the target timestamp delta.
     * This avoids instability with frame timestamps falling near vsyncs.
     * For example if the frame timestamps were (with vsyncs at
     * integer values) 0.01, 1.99, 4.01, 5.99, 8.01, ... then
     * straightforward timing at next vsync would flip the frames at
     * 1, 2, 5, 6, 9; this changes it to 1, 2, 4, 6, 8 and so on with
     * regular 2-vsync intervals.
     *
     * Also allow moving the frame forward if it looks like we dropped
     * the previous frame incorrectly (now that we know better after
     * having final exact timestamp information for this frame) and
     * there would unnecessarily be a vsync without a frame change.
     */
    uint64_t vsync = PREV_VSYNC(pts);
    if (pts < vsync + vc->vsync_interval / 4
        && (vsync - PREV_VSYNC(vc->last_queue_time)
            > pts - vc->last_ideal_time + vc->vsync_interval / 2
            || (vc->dropped_frame && vsync > vc->dropped_time)))
        pts -= vc->vsync_interval / 2;

    vc->dropped_time = ideal_pts;

    pts = FFMAX(pts, vc->last_queue_time + vc->vsync_interval);
    pts = FFMAX(pts, now);
    if (npts < PREV_VSYNC(pts) + vc->vsync_interval)
        return 0;

    int num_flips = update_presentation_queue_status(vo);
    vsync = vc->recent_vsync_time + num_flips * vc->vsync_interval;
    pts = FFMAX(pts, now);
    pts = FFMAX(pts, vsync + (vc->vsync_interval >> 2));
    vsync = PREV_VSYNC(pts);
    if (npts < vsync + vc->vsync_interval)
        return 0;
    pts = vsync + (vc->vsync_interval >> 2);
    VdpOutputSurface frame = vc->output_surfaces[vc->surface_num];
    vdp_st = vdp->presentation_queue_display(vc->flip_queue, frame,
                                             vo->dwidth, vo->dheight, pts);
    CHECK_VDP_WARNING(vo, "Error when calling vdp_presentation_queue_display");

    MP_TRACE(vo, "Queue new surface %d: Vdpau time: %"PRIu64" "
             "pts: %"PRIu64"\n", (int)frame, now, pts);

    vc->last_queue_time = pts;
    vc->queue_time[vc->surface_num] = pts;
    vc->last_ideal_time = ideal_pts;
    vc->dropped_frame = false;
    vc->surface_num = WRAP_ADD(vc->surface_num, 1, vc->num_output_surfaces);
    return 1;
}

static void draw_image(struct vo *vo, struct mp_image *mpi)
{
    struct vdpctx *vc = vo->priv;

    check_preemption(vo);

    struct mp_image *vdp_mpi = mp_vdpau_upload_video_surface(vc->mpvdp, mpi);
    if (!vdp_mpi)
        MP_ERR(vo, "Could not upload image.\n");
    talloc_free(mpi);

    talloc_free(vc->current_image);
    vc->current_image = vdp_mpi;

    if (status_ok(vo))
        video_to_output_surface(vo);
}

// warning: the size and pixel format of surface must match that of the
//          surfaces in vc->output_surfaces
static struct mp_image *read_output_surface(struct vo *vo,
                                            VdpOutputSurface surface,
                                            int width, int height)
{
    struct vdpctx *vc = vo->priv;
    VdpStatus vdp_st;
    struct vdp_functions *vdp = vc->vdp;
    if (!vo->params)
        return NULL;

    struct mp_image *image = mp_image_alloc(IMGFMT_BGR0, width, height);
    if (!image)
        return NULL;

    image->params.colorspace = MP_CSP_RGB;
    // hardcoded with conv. matrix
    image->params.colorlevels = vo->params->outputlevels;

    void *dst_planes[] = { image->planes[0] };
    uint32_t dst_pitches[] = { image->stride[0] };
    vdp_st = vdp->output_surface_get_bits_native(surface, NULL, dst_planes,
                                                 dst_pitches);
    CHECK_VDP_WARNING(vo, "Error when calling vdp_output_surface_get_bits_native");

    return image;
}

static struct mp_image *get_window_screenshot(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    int last_surface = WRAP_ADD(vc->surface_num, -1, vc->num_output_surfaces);
    VdpOutputSurface screen = vc->output_surfaces[last_surface];
    struct mp_image *image = read_output_surface(vo, screen,
                                                 vc->output_surface_width,
                                                 vc->output_surface_height);
    mp_image_set_size(image, vo->dwidth, vo->dheight);
    return image;
}

static int query_format(struct vo *vo, int format)
{
    struct vdpctx *vc = vo->priv;

    if (mp_vdpau_get_format(format, NULL, NULL))
        return 1;
    if (!vc->force_yuv && mp_vdpau_get_rgb_format(format, NULL))
        return 1;
    return 0;
}

static void destroy_vdpau_objects(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;

    VdpStatus vdp_st;

    free_video_specific(vo);

    if (vc->flip_queue != VDP_INVALID_HANDLE) {
        vdp_st = vdp->presentation_queue_destroy(vc->flip_queue);
        CHECK_VDP_WARNING(vo, "Error when calling vdp_presentation_queue_destroy");
    }

    if (vc->flip_target != VDP_INVALID_HANDLE) {
        vdp_st = vdp->presentation_queue_target_destroy(vc->flip_target);
        CHECK_VDP_WARNING(vo, "Error when calling "
                         "vdp_presentation_queue_target_destroy");
    }

    for (int i = 0; i < vc->num_output_surfaces; i++) {
        if (vc->output_surfaces[i] == VDP_INVALID_HANDLE)
            continue;
        vdp_st = vdp->output_surface_destroy(vc->output_surfaces[i]);
        CHECK_VDP_WARNING(vo, "Error when calling vdp_output_surface_destroy");
    }

    for (int i = 0; i < MAX_OSD_PARTS; i++) {
        struct osd_bitmap_surface *sfc = &vc->osd_surfaces[i];
        if (sfc->surface != VDP_INVALID_HANDLE) {
            vdp_st = vdp->bitmap_surface_destroy(sfc->surface);
            CHECK_VDP_WARNING(vo, "Error when calling vdp_bitmap_surface_destroy");
        }
    }

    mp_vdpau_destroy(vc->mpvdp);
    vc->mpvdp = NULL;
}

static void uninit(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    /* Destroy all vdpau objects */
    mp_vdpau_mixer_destroy(vc->video_mixer);
    destroy_vdpau_objects(vo);

    vo_x11_uninit(vo);
}

static int preinit(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    if (!vo_x11_init(vo))
        return -1;

    vc->mpvdp = mp_vdpau_create_device_x11(vo->log, vo->x11->display);
    if (!vc->mpvdp) {
        vo_x11_uninit(vo);
        return -1;
    }

    vc->hwdec_info.hwctx = &vc->mpvdp->hwctx;

    vc->video_mixer = mp_vdpau_mixer_create(vc->mpvdp, vo->log);

    if (mp_vdpau_guess_if_emulated(vc->mpvdp)) {
        MP_WARN(vo, "VDPAU is most likely emulated via VA-API.\n"
                    "This is inefficient. Use --vo=opengl instead.\n");
    }

    // Mark everything as invalid first so uninit() can tell what has been
    // allocated
    mark_vdpau_objects_uninitialized(vo);

    mp_vdpau_handle_preemption(vc->mpvdp, &vc->preemption_counter);

    vc->vdp_device = vc->mpvdp->vdp_device;
    vc->vdp = &vc->mpvdp->vdp;

    vc->video_eq.capabilities = MP_CSP_EQ_CAPS_COLORMATRIX;

    return 0;
}

static int get_equalizer(struct vo *vo, const char *name, int *value)
{
    struct vdpctx *vc = vo->priv;

    if (vc->rgb_mode)
        return false;

    return mp_csp_equalizer_get(&vc->video_mixer->video_eq, name, value) >= 0 ?
           VO_TRUE : VO_NOTIMPL;
}

static int set_equalizer(struct vo *vo, const char *name, int value)
{
    struct vdpctx *vc = vo->priv;

    if (vc->rgb_mode)
        return false;

    if (mp_csp_equalizer_set(&vc->video_mixer->video_eq, name, value) < 0)
        return VO_NOTIMPL;
    vc->video_mixer->initialized = false;
    return true;
}

static void checked_resize(struct vo *vo)
{
    if (!status_ok(vo))
        return;
    resize(vo);
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct vdpctx *vc = vo->priv;

    check_preemption(vo);

    switch (request) {
    case VOCTRL_GET_HWDEC_INFO: {
        struct mp_hwdec_info **arg = data;
        *arg = &vc->hwdec_info;
        return true;
    }
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        checked_resize(vo);
        return VO_TRUE;
    case VOCTRL_SET_EQUALIZER: {
        vo->want_redraw = true;
        struct voctrl_set_equalizer_args *args = data;
        return set_equalizer(vo, args->name, args->value);
    }
    case VOCTRL_GET_EQUALIZER: {
        struct voctrl_get_equalizer_args *args = data;
        return get_equalizer(vo, args->name, args->valueptr);
    }
    case VOCTRL_REDRAW_FRAME:
        if (status_ok(vo))
            video_to_output_surface(vo);
        return true;
    case VOCTRL_RESET:
        forget_frames(vo, true);
        return true;
    case VOCTRL_SCREENSHOT_WIN:
        if (!status_ok(vo))
            return false;
        *(struct mp_image **)data = get_window_screenshot(vo);
        return true;
    case VOCTRL_GET_PREF_DEINT:
        *(int *)data = vc->deint;
        return true;
    }

    int events = 0;
    int r = vo_x11_control(vo, &events, request, data);

    if (events & VO_EVENT_RESIZE) {
        checked_resize(vo);
    } else if (events & VO_EVENT_EXPOSE) {
        vo->want_redraw = true;
    }
    vo_event(vo, events);

    return r;
}

#define OPT_BASE_STRUCT struct vdpctx

const struct vo_driver video_out_vdpau = {
    .description = "VDPAU with X11",
    .name = "vdpau",
    .caps = VO_CAP_FRAMEDROP,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page_timed = flip_page_timed,
    .uninit = uninit,
    .priv_size = sizeof(struct vdpctx),
    .options = (const struct m_option []){
        OPT_INTRANGE("deint", deint, 0, -4, 4),
        OPT_FLAG("chroma-deint", chroma_deint, 0, OPTDEF_INT(1)),
        OPT_FLAG("pullup", pullup, 0),
        OPT_FLOATRANGE("denoise", denoise, 0, 0, 1),
        OPT_FLOATRANGE("sharpen", sharpen, 0, -1, 1),
        OPT_INTRANGE("hqscaling", hqscaling, 0, 0, 9),
        OPT_FLOAT("fps", user_fps, 0),
        OPT_FLAG("composite-detect", composite_detect, 0, OPTDEF_INT(1)),
        OPT_INT("queuetime_windowed", flip_offset_window, 0, OPTDEF_INT(50)),
        OPT_INT("queuetime_fs", flip_offset_fs, 0, OPTDEF_INT(50)),
        OPT_INTRANGE("output_surfaces", num_output_surfaces, 0,
                     2, MAX_OUTPUT_SURFACES, OPTDEF_INT(3)),
        OPT_COLOR("colorkey", colorkey, 0,
                  .defval = &(const struct m_color) {
                      .r = 2, .g = 5, .b = 7, .a = 255,
                  }),
        OPT_FLAG("force-yuv", force_yuv, 0),
        {NULL},
    }
};
