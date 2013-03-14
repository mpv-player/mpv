/*
 * VDPAU video output driver
 *
 * Copyright (C) 2008 NVIDIA
 * Copyright (C) 2009 Uoti Urpala
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Actual decoding and presentation are implemented here.
 * All necessary frame information is collected through
 * the "vdpau_render_state" structure after parsing all headers
 * etc. in libavcodec for different codecs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>

#include <libavutil/common.h>
#include <libavcodec/vdpau.h>

#include "config.h"
#include "core/mp_msg.h"
#include "core/options.h"
#include "talloc.h"
#include "vo.h"
#include "x11_common.h"
#include "video/csputils.h"
#include "sub/sub.h"
#include "core/m_option.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "osdep/timer.h"
#include "bitmap_packer.h"

#define WRAP_ADD(x, a, m) ((a) < 0 \
                           ? ((x)+(a)+(m) < (m) ? (x)+(a)+(m) : (x)+(a)) \
                           : ((x)+(a) < (m) ? (x)+(a) : (x)+(a)-(m)))

#define CHECK_ST_ERROR(message) \
    do { \
        if (vdp_st != VDP_STATUS_OK) { \
            mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] %s: %s\n", \
                   message, vdp->get_error_string(vdp_st)); \
            return -1; \
        } \
    } while (0)

#define CHECK_ST_WARNING(message) \
    do { \
        if (vdp_st != VDP_STATUS_OK) \
            mp_msg(MSGT_VO, MSGL_WARN, "[   vdpau] %s: %s\n", \
                   message, vdp->get_error_string(vdp_st)); \
    } while (0)

/* number of video and output surfaces */
#define MAX_OUTPUT_SURFACES                15
#define MAX_VIDEO_SURFACES                 50
#define NUM_BUFFERED_VIDEO                 5

/* Pixelformat used for output surfaces */
#define OUTPUT_RGBA_FORMAT VDP_RGBA_FORMAT_B8G8R8A8

/*
 * Global variable declaration - VDPAU specific
 */

struct vdp_functions {
#define VDP_FUNCTION(vdp_type, _, mp_name) vdp_type *mp_name;
#include "vdpau_template.c"
#undef VDP_FUNCTION
};

struct vdpctx {
    struct vdp_functions *vdp;

    VdpDevice                          vdp_device;
    bool                               is_preempted;
    bool                               preemption_acked;
    bool                               preemption_user_notified;
    unsigned int                       last_preemption_retry_fail;
    VdpGetProcAddress                 *vdp_get_proc_address;

    VdpPresentationQueueTarget         flip_target;
    VdpPresentationQueue               flip_queue;
    uint64_t                           last_vdp_time;
    unsigned int                       last_sync_update;

    VdpOutputSurface                   output_surfaces[MAX_OUTPUT_SURFACES];
    VdpOutputSurface                   screenshot_surface;
    int                                num_output_surfaces;
    struct buffered_video_surface {
        VdpVideoSurface surface;
        double pts;
        mp_image_t *mpi;
    } buffered_video[NUM_BUFFERED_VIDEO];
    int                                deint_queue_pos;
    int                                output_surface_width, output_surface_height;

    VdpVideoMixer                      video_mixer;
    struct mp_csp_details              colorspace;
    int                                deint;
    int                                deint_type;
    int                                deint_counter;
    int                                pullup;
    float                              denoise;
    float                              sharpen;
    int                                hqscaling;
    int                                chroma_deint;
    int                                flip_offset_window;
    int                                flip_offset_fs;
    int                                top_field_first;
    bool                               flip;

    VdpDecoder                         decoder;
    int                                decoder_max_refs;

    VdpRect                            src_rect_vid;
    VdpRect                            out_rect_vid;
    struct mp_osd_res                  osd_rect;

    struct vdpau_render_state          surface_render[MAX_VIDEO_SURFACES];
    bool                               surface_in_use[MAX_VIDEO_SURFACES];
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
        int bitmap_id;
        int bitmap_pos_id;
    } osd_surfaces[MAX_OSD_PARTS];

    // Video equalizer
    struct mp_csp_equalizer video_eq;
};

static bool status_ok(struct vo *vo);

static int change_vdptime_sync(struct vdpctx *vc, unsigned int *t)
{
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    VdpTime vdp_time;
    vdp_st = vdp->presentation_queue_get_time(vc->flip_queue, &vdp_time);
    CHECK_ST_ERROR("Error when calling vdp_presentation_queue_get_time");
    unsigned int t1 = *t;
    unsigned int t2 = GetTimer();
    uint64_t old = vc->last_vdp_time + (t1 - vc->last_sync_update) * 1000ULL;
    if (vdp_time > old) {
        if (vdp_time > old + (t2 - t1) * 1000ULL)
            vdp_time -= (t2 - t1) * 1000ULL;
        else
            vdp_time = old;
    }
    mp_msg(MSGT_VO, MSGL_DBG2, "[vdpau] adjusting VdpTime offset by %f µs\n",
           (int64_t)(vdp_time - old) / 1000.);
    vc->last_vdp_time = vdp_time;
    vc->last_sync_update = t1;
    *t = t2;
    return 0;
}

static uint64_t sync_vdptime(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    unsigned int t = GetTimer();
    if (t - vc->last_sync_update > 5000000)
        change_vdptime_sync(vc, &t);
    uint64_t now = (t - vc->last_sync_update) * 1000ULL + vc->last_vdp_time;
    // Make sure nanosecond inaccuracies don't make things inconsistent
    now = FFMAX(now, vc->recent_vsync_time);
    return now;
}

static uint64_t convert_to_vdptime(struct vo *vo, unsigned int t)
{
    struct vdpctx *vc = vo->priv;
    return (int)(t - vc->last_sync_update) * 1000LL + vc->last_vdp_time;
}

static int render_video_to_output_surface(struct vo *vo,
                                          VdpOutputSurface output_surface,
                                          VdpRect *output_rect)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpTime dummy;
    VdpStatus vdp_st;
    if (vc->deint_queue_pos < 0)
        return -1;

    struct buffered_video_surface *bv = vc->buffered_video;
    int field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
    unsigned int dp = vc->deint_queue_pos;
    // dp==0 means last field of latest frame, 1 earlier field of latest frame,
    // 2 last field of previous frame and so on
    if (vc->deint) {
        field = vc->top_field_first ^ (dp & 1) ?
            VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD:
            VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    }
    const VdpVideoSurface *past_fields = (const VdpVideoSurface []){
        bv[(dp+1)/2].surface, bv[(dp+2)/2].surface};
    const VdpVideoSurface *future_fields = (const VdpVideoSurface []){
        dp >= 1 ? bv[(dp-1)/2].surface : VDP_INVALID_HANDLE};
    vdp_st = vdp->presentation_queue_block_until_surface_idle(vc->flip_queue,
                                                              output_surface,
                                                              &dummy);
    CHECK_ST_WARNING("Error when calling "
                     "vdp_presentation_queue_block_until_surface_idle");

    vdp_st = vdp->video_mixer_render(vc->video_mixer, VDP_INVALID_HANDLE,
                                     0, field, 2, past_fields,
                                     bv[dp/2].surface, 1, future_fields,
                                     &vc->src_rect_vid, output_surface,
                                     NULL, output_rect, 0, NULL);
    CHECK_ST_WARNING("Error when calling vdp_video_mixer_render");
    return 0;
}

static int video_to_output_surface(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    return render_video_to_output_surface(vo,
                                          vc->output_surfaces[vc->surface_num],
                                          &vc->out_rect_vid);
}

static int next_deint_queue_pos(struct vo *vo, bool eof)
{
    struct vdpctx *vc = vo->priv;

    int dqp = vc->deint_queue_pos;
    if (dqp < 0)
        dqp += 1000;
    else
        dqp = vc->deint >= 2 ? dqp - 1 : dqp - 2 | 1;
    if (dqp < (eof ? 0 : 3))
        return -1;
    return dqp;
}

static void set_next_frame_info(struct vo *vo, bool eof)
{
    struct vdpctx *vc = vo->priv;

    vo->frame_loaded = false;
    int dqp = next_deint_queue_pos(vo, eof);
    if (dqp < 0)
        return;
    vo->frame_loaded = true;

    // Set pts values
    struct buffered_video_surface *bv = vc->buffered_video;
    int idx = dqp >> 1;
    if (idx == 0) {  // no future frame/pts available
        vo->next_pts = bv[0].pts;
        vo->next_pts2 = MP_NOPTS_VALUE;
    } else if (!(vc->deint >= 2)) {    // no field-splitting deinterlace
        vo->next_pts = bv[idx].pts;
        vo->next_pts2 = bv[idx - 1].pts;
    } else {  // deinterlace with separate fields
        double intermediate_pts;
        double diff = bv[idx - 1].pts - bv[idx].pts;
        if (diff > 0 && diff < 0.5)
            intermediate_pts = (bv[idx].pts + bv[idx - 1].pts) / 2;
        else
            intermediate_pts =  bv[idx].pts;
        if (dqp & 1) { // first field
            vo->next_pts = bv[idx].pts;
            vo->next_pts2 = intermediate_pts;
        } else {
            vo->next_pts = intermediate_pts;
            vo->next_pts2 = bv[idx - 1].pts;
        }
    }
}

static void add_new_video_surface(struct vo *vo, VdpVideoSurface surface,
                                  struct mp_image *reserved_mpi, double pts)
{
    struct vdpctx *vc = vo->priv;
    struct buffered_video_surface *bv = vc->buffered_video;

    mp_image_unrefp(&bv[NUM_BUFFERED_VIDEO - 1].mpi);

    for (int i = NUM_BUFFERED_VIDEO - 1; i > 0; i--)
        bv[i] = bv[i - 1];
    bv[0] = (struct buffered_video_surface){
        .mpi = reserved_mpi ? mp_image_new_ref(reserved_mpi) : NULL,
        .surface = surface,
        .pts = pts,
    };

    vc->deint_queue_pos = FFMIN(vc->deint_queue_pos + 2,
                                NUM_BUFFERED_VIDEO * 2 - 3);
    set_next_frame_info(vo, false);
}

static void forget_frames(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    vc->deint_queue_pos = -1001;
    vc->dropped_frame = false;
    for (int i = 0; i < NUM_BUFFERED_VIDEO; i++) {
        struct buffered_video_surface *p = vc->buffered_video + i;
        mp_image_unrefp(&p->mpi);
        *p = (struct buffered_video_surface){
            .surface = VDP_INVALID_HANDLE,
        };
    }
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
    vc->src_rect_vid.y0 = vc->flip ? src_rect.y1 : src_rect.y0;
    vc->src_rect_vid.y1 = vc->flip ? src_rect.y0 : src_rect.y1;

    int flip_offset_ms = vo->opts->fs ?
                         vc->flip_offset_fs :
                         vc->flip_offset_window;

    vo->flip_queue_offset = flip_offset_ms / 1000.;

    if (vc->output_surface_width < vo->dwidth
        || vc->output_surface_height < vo->dheight) {
        if (vc->output_surface_width < vo->dwidth) {
            vc->output_surface_width += vc->output_surface_width >> 1;
            vc->output_surface_width = FFMAX(vc->output_surface_width,
                                             vo->dwidth);
        }
        if (vc->output_surface_height < vo->dheight) {
            vc->output_surface_height += vc->output_surface_height >> 1;
            vc->output_surface_height = FFMAX(vc->output_surface_height,
                                              vo->dheight);
        }
        // Creation of output_surfaces
        for (int i = 0; i < vc->num_output_surfaces; i++)
            if (vc->output_surfaces[i] != VDP_INVALID_HANDLE) {
                vdp_st = vdp->output_surface_destroy(vc->output_surfaces[i]);
                CHECK_ST_WARNING("Error when calling "
                                 "vdp_output_surface_destroy");
            }
        for (int i = 0; i < vc->num_output_surfaces; i++) {
            vdp_st = vdp->output_surface_create(vc->vdp_device,
                                                OUTPUT_RGBA_FORMAT,
                                                vc->output_surface_width,
                                                vc->output_surface_height,
                                                &vc->output_surfaces[i]);
            CHECK_ST_WARNING("Error when calling vdp_output_surface_create");
            mp_msg(MSGT_VO, MSGL_DBG2, "vdpau out create: %u\n",
                   vc->output_surfaces[i]);
        }
    }
    vo->want_redraw = true;
}

static void preemption_callback(VdpDevice device, void *context)
{
    struct vdpctx *vc = context;
    vc->is_preempted = true;
    vc->preemption_acked = false;
}

/* Initialize vdp_get_proc_address, called from preinit() */
static int win_x11_init_vdpau_procs(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    struct vdpctx *vc = vo->priv;
    if (vc->vdp)  // reinitialization after preemption
        memset(vc->vdp, 0, sizeof(*vc->vdp));
    else
        vc->vdp = talloc_zero(vc, struct vdp_functions);
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;

    struct vdp_function {
        const int id;
        int offset;
    };

    const struct vdp_function *dsc;

    static const struct vdp_function vdp_func[] = {
#define VDP_FUNCTION(_, macro_name, mp_name) {macro_name, offsetof(struct vdp_functions, mp_name)},
#include "vdpau_template.c"
#undef VDP_FUNCTION
        {0, -1}
    };

    vdp_st = vdp_device_create_x11(x11->display, x11->screen, &vc->vdp_device,
                                   &vc->vdp_get_proc_address);
    if (vdp_st != VDP_STATUS_OK) {
        if (vc->is_preempted)
            mp_msg(MSGT_VO, MSGL_DBG2, "[vdpau] Error calling "
                   "vdp_device_create_x11 while preempted: %d\n", vdp_st);
        else
            mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Error when calling "
                   "vdp_device_create_x11: %d\n", vdp_st);
        return -1;
    }

    vdp->get_error_string = NULL;
    for (dsc = vdp_func; dsc->offset >= 0; dsc++) {
        vdp_st = vc->vdp_get_proc_address(vc->vdp_device, dsc->id,
                                      (void **)((char *)vdp + dsc->offset));
        if (vdp_st != VDP_STATUS_OK) {
            mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Error when calling "
                   "vdp_get_proc_address(function id %d): %s\n", dsc->id,
                   vdp->get_error_string ? vdp->get_error_string(vdp_st) : "?");
            return -1;
        }
    }
    vdp_st = vdp->preemption_callback_register(vc->vdp_device,
                                               preemption_callback, vc);
    return 0;
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
        CHECK_ST_ERROR("Error when calling "
                       "vdp_presentation_queue_target_create_x11");
    }

    /* Emperically this seems to be the first call which fails when we
     * try to reinit after preemption while the user is still switched
     * from X to a virtual terminal (creating the vdp_device initially
     * succeeds, as does creating the flip_target above). This is
     * probably not guaranteed behavior, but we'll assume it as a simple
     * way to reduce warnings while trying to recover from preemption.
     */
    if (vc->flip_queue == VDP_INVALID_HANDLE) {
        vdp_st = vdp->presentation_queue_create(vc->vdp_device, vc->flip_target,
                                                &vc->flip_queue);
        if (vc->is_preempted && vdp_st != VDP_STATUS_OK) {
            mp_msg(MSGT_VO, MSGL_DBG2, "[vdpau] Failed to create flip queue "
                   "while preempted: %s\n", vdp->get_error_string(vdp_st));
            return -1;
        } else
            CHECK_ST_ERROR("Error when calling vdp_presentation_queue_create");
    }

    VdpTime vdp_time;
    vdp_st = vdp->presentation_queue_get_time(vc->flip_queue, &vdp_time);
    CHECK_ST_ERROR("Error when calling vdp_presentation_queue_get_time");
    vc->last_vdp_time = vdp_time;
    vc->last_sync_update = GetTimer();

    vc->vsync_interval = 1;
    if (vc->composite_detect && vo_x11_screen_is_composited(vo)) {
        mp_msg(MSGT_VO, MSGL_INFO, "[vdpau] Compositing window manager "
               "detected. Assuming timing info is inaccurate.\n");
    } else if (vc->user_fps > 0) {
        vc->vsync_interval = 1e9 / vc->user_fps;
        mp_msg(MSGT_VO, MSGL_INFO, "[vdpau] Assuming user-specified display "
               "refresh rate of %.3f Hz.\n", vc->user_fps);
    } else if (vc->user_fps == 0) {
#ifdef CONFIG_XF86VM
        double fps = vo_x11_vm_get_fps(vo);
        if (!fps)
            mp_msg(MSGT_VO, MSGL_WARN, "[vdpau] Failed to get display FPS\n");
        else {
            vc->vsync_interval = 1e9 / fps;
            // This is verbose, but I'm not yet sure how common wrong values are
            mp_msg(MSGT_VO, MSGL_INFO,
                   "[vdpau] Got display refresh rate %.3f Hz.\n"
                   "[vdpau] If that value looks wrong give the "
                   "-vo vdpau:fps=X suboption manually.\n", fps);
        }
#else
        mp_msg(MSGT_VO, MSGL_INFO, "[vdpau] This binary has been compiled "
               "without XF86VidMode support.\n");
        mp_msg(MSGT_VO, MSGL_INFO, "[vdpau] Can't use vsync-aware timing "
               "without manually provided -vo vdpau:fps=X suboption.\n");
#endif
    } else
        mp_msg(MSGT_VO, MSGL_V, "[vdpau] framedrop/timing logic disabled by "
               "user.\n");

    return 0;
}

static int set_video_attribute(struct vdpctx *vc, VdpVideoMixerAttribute attr,
                               const void *value, char *attr_name)
{
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;

    vdp_st = vdp->video_mixer_set_attribute_values(vc->video_mixer, 1, &attr,
                                                   &value);
    if (vdp_st != VDP_STATUS_OK) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Error setting video mixer "
               "attribute %s: %s\n", attr_name, vdp->get_error_string(vdp_st));
        return -1;
    }
    return 0;
}

static void update_csc_matrix(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    mp_msg(MSGT_VO, MSGL_V, "[vdpau] Updating CSC matrix\n");

    // VdpCSCMatrix happens to be compatible with mplayer's CSC matrix type
    // both are float[3][4]
    VdpCSCMatrix matrix;

    struct mp_csp_params cparams = {
        .colorspace = vc->colorspace, .input_bits = 8, .texture_bits = 8 };
    mp_csp_copy_equalizer_values(&cparams, &vc->video_eq);
    mp_get_yuv2rgb_coeffs(&cparams, matrix);

    set_video_attribute(vc, VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX,
                        &matrix, "CSC matrix");
}

#define SET_VIDEO_ATTR(attr_name, attr_type, value) set_video_attribute(vc, \
                 VDP_VIDEO_MIXER_ATTRIBUTE_ ## attr_name, &(attr_type){value},\
                 # attr_name)
static int create_vdp_mixer(struct vo *vo, VdpChromaType vdp_chroma_type)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
#define VDP_NUM_MIXER_PARAMETER 3
#define MAX_NUM_FEATURES 6
    int i;
    VdpStatus vdp_st;

    if (vc->video_mixer != VDP_INVALID_HANDLE)
        return 0;

    int feature_count = 0;
    VdpVideoMixerFeature features[MAX_NUM_FEATURES];
    VdpBool feature_enables[MAX_NUM_FEATURES];
    static const VdpVideoMixerParameter parameters[VDP_NUM_MIXER_PARAMETER] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
    };
    const void *const parameter_values[VDP_NUM_MIXER_PARAMETER] = {
        &vc->vid_width,
        &vc->vid_height,
        &vdp_chroma_type,
    };
    features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL;
    if (vc->deint_type == 4)
        features[feature_count++] =
            VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL;
    if (vc->pullup)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_INVERSE_TELECINE;
    if (vc->denoise)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION;
    if (vc->sharpen)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_SHARPNESS;
    if (vc->hqscaling) {
        VdpVideoMixerFeature hqscaling_feature =
            VDP_VIDEO_MIXER_FEATURE_HIGH_QUALITY_SCALING_L1 + vc->hqscaling-1;
        VdpBool hqscaling_available;
        vdp_st = vdp->video_mixer_query_feature_support(vc->vdp_device,
                                                        hqscaling_feature,
                                                        &hqscaling_available);
        CHECK_ST_ERROR("Error when calling video_mixer_query_feature_support");
        if (hqscaling_available)
            features[feature_count++] = hqscaling_feature;
        else
            mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Your hardware or VDPAU "
                   "library does not support requested hqscaling.\n");
    }

    vdp_st = vdp->video_mixer_create(vc->vdp_device, feature_count, features,
                                     VDP_NUM_MIXER_PARAMETER,
                                     parameters, parameter_values,
                                     &vc->video_mixer);
    CHECK_ST_ERROR("Error when calling vdp_video_mixer_create");

    for (i = 0; i < feature_count; i++)
        feature_enables[i] = VDP_TRUE;
    if (vc->deint < 3)
        feature_enables[0] = VDP_FALSE;
    if (vc->deint_type == 4 && vc->deint < 4)
        feature_enables[1] = VDP_FALSE;
    if (feature_count) {
        vdp_st = vdp->video_mixer_set_feature_enables(vc->video_mixer,
                                                      feature_count, features,
                                                      feature_enables);
        CHECK_ST_WARNING("Error calling vdp_video_mixer_set_feature_enables");
    }
    if (vc->denoise)
        SET_VIDEO_ATTR(NOISE_REDUCTION_LEVEL, float, vc->denoise);
    if (vc->sharpen)
        SET_VIDEO_ATTR(SHARPNESS_LEVEL, float, vc->sharpen);
    if (!vc->chroma_deint)
        SET_VIDEO_ATTR(SKIP_CHROMA_DEINTERLACE, uint8_t, 1);

    update_csc_matrix(vo);
    return 0;
}

// Free everything specific to a certain video file
static void free_video_specific(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    int i;
    VdpStatus vdp_st;

    if (vc->decoder != VDP_INVALID_HANDLE)
        vdp->decoder_destroy(vc->decoder);
    vc->decoder = VDP_INVALID_HANDLE;
    vc->decoder_max_refs = -1;

    forget_frames(vo);

    for (i = 0; i < MAX_VIDEO_SURFACES; i++) {
        if (vc->surface_render[i].surface != VDP_INVALID_HANDLE) {
            vdp_st = vdp->video_surface_destroy(vc->surface_render[i].surface);
            CHECK_ST_WARNING("Error when calling vdp_video_surface_destroy");
        }
        vc->surface_render[i].surface = VDP_INVALID_HANDLE;
    }

    if (vc->video_mixer != VDP_INVALID_HANDLE) {
        vdp_st = vdp->video_mixer_destroy(vc->video_mixer);
        CHECK_ST_WARNING("Error when calling vdp_video_mixer_destroy");
    }
    vc->video_mixer = VDP_INVALID_HANDLE;

    if (vc->screenshot_surface != VDP_INVALID_HANDLE) {
        vdp_st = vdp->output_surface_destroy(vc->screenshot_surface);
        CHECK_ST_WARNING("Error when calling vdp_output_surface_destroy");
    }
    vc->screenshot_surface = VDP_INVALID_HANDLE;
}

static int create_vdp_decoder(struct vo *vo, int max_refs)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    VdpDecoderProfile vdp_decoder_profile;
    if (vc->decoder != VDP_INVALID_HANDLE)
        vdp->decoder_destroy(vc->decoder);
    switch (vc->image_format) {
    case IMGFMT_VDPAU_MPEG1:
        vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG1;
        break;
    case IMGFMT_VDPAU_MPEG2:
        vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG2_MAIN;
        break;
    case IMGFMT_VDPAU_H264:
        vdp_decoder_profile = VDP_DECODER_PROFILE_H264_HIGH;
        mp_msg(MSGT_VO, MSGL_V, "[vdpau] Creating H264 hardware decoder "
               "for %d reference frames.\n", max_refs);
        break;
    case IMGFMT_VDPAU_WMV3:
        vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_MAIN;
        break;
    case IMGFMT_VDPAU_VC1:
        vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
        break;
    case IMGFMT_VDPAU_MPEG4:
        vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG4_PART2_ASP;
        break;
    default:
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Unknown image format!\n");
        goto fail;
    }
    vdp_st = vdp->decoder_create(vc->vdp_device, vdp_decoder_profile,
                                 vc->vid_width, vc->vid_height, max_refs,
                                 &vc->decoder);
    CHECK_ST_WARNING("Failed creating VDPAU decoder");
    if (vdp_st != VDP_STATUS_OK) {
    fail:
        vc->decoder = VDP_INVALID_HANDLE;
        vc->decoder_max_refs = 0;
        return 0;
    }
    vc->decoder_max_refs = max_refs;
    return 1;
}

static int initialize_vdpau_objects(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    vc->vdp_chroma_type = VDP_CHROMA_TYPE_420;
    switch (vc->image_format) {
    case IMGFMT_420P:
        vc->vdp_pixel_format = VDP_YCBCR_FORMAT_YV12;
        break;
    case IMGFMT_NV12:
        vc->vdp_pixel_format = VDP_YCBCR_FORMAT_NV12;
        break;
    case IMGFMT_YUYV:
        vc->vdp_pixel_format = VDP_YCBCR_FORMAT_YUYV;
        vc->vdp_chroma_type  = VDP_CHROMA_TYPE_422;
        break;
    case IMGFMT_UYVY:
        vc->vdp_pixel_format = VDP_YCBCR_FORMAT_UYVY;
        vc->vdp_chroma_type  = VDP_CHROMA_TYPE_422;
    }
    if (win_x11_init_vdpau_flip_queue(vo) < 0)
        return -1;

    if (create_vdp_mixer(vo, vc->vdp_chroma_type) < 0)
        return -1;

    forget_frames(vo);
    resize(vo);
    return 0;
}

static void mark_vdpau_objects_uninitialized(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    vc->decoder = VDP_INVALID_HANDLE;
    for (int i = 0; i < MAX_VIDEO_SURFACES; i++)
        vc->surface_render[i].surface = VDP_INVALID_HANDLE;
    forget_frames(vo);
    vc->video_mixer = VDP_INVALID_HANDLE;
    vc->flip_queue = VDP_INVALID_HANDLE;
    vc->flip_target = VDP_INVALID_HANDLE;
    for (int i = 0; i < MAX_OUTPUT_SURFACES; i++)
        vc->output_surfaces[i] = VDP_INVALID_HANDLE;
    vc->screenshot_surface = VDP_INVALID_HANDLE;
    vc->vdp_device = VDP_INVALID_HANDLE;
    for (int i = 0; i < MAX_OSD_PARTS; i++) {
        struct osd_bitmap_surface *sfc = &vc->osd_surfaces[i];
        talloc_free(sfc->packer);
        sfc->bitmap_id = sfc->bitmap_pos_id = 0;
        *sfc = (struct osd_bitmap_surface){
            .surface = VDP_INVALID_HANDLE,
        };
    }
    vc->output_surface_width = vc->output_surface_height = -1;
}

static int handle_preemption(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    if (!vc->is_preempted)
        return 0;
    if (!vc->preemption_acked)
        mark_vdpau_objects_uninitialized(vo);
    vc->preemption_acked = true;
    if (!vc->preemption_user_notified) {
        mp_tmsg(MSGT_VO, MSGL_ERR, "[vdpau] Got display preemption notice! "
                "Will attempt to recover.\n");
        vc->preemption_user_notified = true;
    }
    /* Trying to initialize seems to be quite slow, so only try once a
     * second to avoid using 100% CPU. */
    if (vc->last_preemption_retry_fail
        && GetTimerMS() - vc->last_preemption_retry_fail < 1000)
        return -1;
    if (win_x11_init_vdpau_procs(vo) < 0 || initialize_vdpau_objects(vo) < 0) {
        vc->last_preemption_retry_fail = GetTimerMS() | 1;
        return -1;
    }
    vc->last_preemption_retry_fail = 0;
    vc->is_preempted = false;
    vc->preemption_user_notified = false;
    mp_tmsg(MSGT_VO, MSGL_INFO, "[vdpau] Recovered from display preemption.\n");
    return 1;
}

/*
 * connect to X server, create and map window, initialize all
 * VDPAU objects, create different surfaces etc.
 */
static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct vdpctx *vc = vo->priv;

    if (handle_preemption(vo) < 0)
        return -1;

    vc->flip = flags & VOFLAG_FLIPPING;
    vc->image_format = format;
    vc->vid_width    = width;
    vc->vid_height   = height;

    free_video_specific(vo);
    if (IMGFMT_IS_VDPAU(vc->image_format) && !create_vdp_decoder(vo, 2))
        return -1;

    vo_x11_config_vo_window(vo, NULL, vo->dx, vo->dy, d_width, d_height,
                            flags, "vdpau");

    if (initialize_vdpau_objects(vo) < 0)
        return -1;

    return 0;
}

static void check_events(struct vo *vo)
{
    if (handle_preemption(vo) < 0)
        return;

    int e = vo_x11_check_events(vo);

    if (e & VO_EVENT_RESIZE)
        resize(vo);
    else if (e & VO_EVENT_EXPOSE) {
        vo->want_redraw = true;
    }
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
    CHECK_ST_WARNING("Query to get max OSD surface size failed");
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
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE,
        .blend_factor_destination_color =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .blend_factor_destination_alpha =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA,
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
        CHECK_ST_WARNING("OSD: Error when rendering");
    }
}

static void generate_osd_part(struct vo *vo, struct sub_bitmaps *imgs)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    struct osd_bitmap_surface *sfc = &vc->osd_surfaces[imgs->render_index];
    bool need_upload = false;

    if (imgs->bitmap_pos_id == sfc->bitmap_pos_id)
        return; // Nothing changed and we still have the old data

    sfc->render_count = 0;

    if (imgs->format == SUBBITMAP_EMPTY || imgs->num_parts == 0)
        return;

    if (imgs->bitmap_id == sfc->bitmap_id)
        goto osd_skip_upload;

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
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] OSD bitmaps do not fit on "
               "a surface with the maximum supported size\n");
        return;
    } else if (r == 1) {
        if (sfc->surface != VDP_INVALID_HANDLE) {
            vdp_st = vdp->bitmap_surface_destroy(sfc->surface);
            CHECK_ST_WARNING("Error when calling vdp_bitmap_surface_destroy");
        }
        mp_msg(MSGT_VO, MSGL_V, "[vdpau] Allocating a %dx%d surface for "
               "OSD bitmaps.\n", sfc->packer->w, sfc->packer->h);
        vdp_st = vdp->bitmap_surface_create(vc->vdp_device, format,
                                            sfc->packer->w, sfc->packer->h,
                                            true, &sfc->surface);
        if (vdp_st != VDP_STATUS_OK)
            sfc->surface = VDP_INVALID_HANDLE;
        CHECK_ST_WARNING("OSD: error when creating surface");
    }
    if (imgs->scaled) {
        char zeros[sfc->packer->used_width * format_size];
        memset(zeros, 0, sizeof(zeros));
        vdp_st = vdp->bitmap_surface_put_bits_native(sfc->surface,
                &(const void *){zeros}, &(uint32_t){0},
                &(VdpRect){0, 0, sfc->packer->used_width,
                                 sfc->packer->used_height});
    }

osd_skip_upload:
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
                CHECK_ST_WARNING("OSD: putbits failed");
        }
        sfc->render_count++;
    }

    sfc->bitmap_id = imgs->bitmap_id;
    sfc->bitmap_pos_id = imgs->bitmap_pos_id;
}

static void draw_osd_cb(void *ctx, struct sub_bitmaps *imgs)
{
    struct vo *vo = ctx;
    generate_osd_part(vo, imgs);
    draw_osd_part(vo, imgs->render_index);
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct vdpctx *vc = vo->priv;

    if (!status_ok(vo))
        return;

    static const bool formats[SUBBITMAP_COUNT] = {
        [SUBBITMAP_LIBASS] = true,
        [SUBBITMAP_RGBA] = true,
    };

    osd_draw(osd, vc->osd_rect, osd->vo_pts, 0, formats, draw_osd_cb, vo);
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
        CHECK_ST_WARNING("Error calling "
                         "presentation_queue_query_surface_status");
        if (status == VDP_PRESENTATION_QUEUE_STATUS_QUEUED)
            break;
        if (vc->vsync_interval > 1) {
            uint64_t qtime = vc->queue_time[vc->query_surface_num];
            if (vtime < qtime + vc->vsync_interval / 2)
                mp_msg(MSGT_VO, MSGL_V, "[vdpau] Frame shown too early\n");
            if (vtime > qtime + vc->vsync_interval)
                mp_msg(MSGT_VO, MSGL_V, "[vdpau] Frame shown late\n");
        }
        vc->query_surface_num = WRAP_ADD(vc->query_surface_num, 1,
                                         vc->num_output_surfaces);
        vc->recent_vsync_time = vtime;
    }
    int num_queued = WRAP_ADD(vc->surface_num, -vc->query_surface_num,
                              vc->num_output_surfaces);
    mp_msg(MSGT_VO, MSGL_DBG3, "[vdpau] Queued surface count (before add): "
           "%d\n", num_queued);
    return num_queued;
}

static inline uint64_t prev_vs2(struct vdpctx *vc, uint64_t ts, int shift)
{
    uint64_t offset = ts - vc->recent_vsync_time;
    // Fix negative values for 1<<shift vsyncs before vc->recent_vsync_time
    offset += (uint64_t)vc->vsync_interval << shift;
    offset %= vc->vsync_interval;
    return ts - offset;
}

static void flip_page_timed(struct vo *vo, unsigned int pts_us, int duration)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    uint32_t vsync_interval = vc->vsync_interval;

    if (handle_preemption(vo) < 0)
        return;

    if (duration > INT_MAX / 1000)
        duration = -1;
    else
        duration *= 1000;

    if (vc->vsync_interval == 1)
        duration = -1;  // Make sure drop logic is disabled

    uint64_t now = sync_vdptime(vo);
    uint64_t pts = pts_us ? convert_to_vdptime(vo, pts_us) : now;
    uint64_t ideal_pts = pts;
    uint64_t npts = duration >= 0 ? pts + duration : UINT64_MAX;

#define PREV_VS2(ts, shift) prev_vs2(vc, ts, shift)
    // Only gives accurate results for ts >= vc->recent_vsync_time
#define PREV_VSYNC(ts) PREV_VS2(ts, 0)

    /* We hope to be here at least one vsync before the frame should be shown.
     * If we are running late then don't drop the frame unless there is
     * already one queued for the next vsync; even if we _hope_ to show the
     * next frame soon enough to mean this one should be dropped we might
     * not make the target time in reality. Without this check we could drop
     * every frame, freezing the display completely if video lags behind.
     */
    if (now > PREV_VSYNC(FFMAX(pts, vc->last_queue_time + vsync_interval)))
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
    if (pts < vsync + vsync_interval / 4
        && (vsync - PREV_VS2(vc->last_queue_time, 16)
            > pts - vc->last_ideal_time + vsync_interval / 2
            || vc->dropped_frame && vsync > vc->dropped_time))
        pts -= vsync_interval / 2;

    vc->dropped_frame = true; // changed at end if false
    vc->dropped_time = ideal_pts;

    pts = FFMAX(pts, vc->last_queue_time + vsync_interval);
    pts = FFMAX(pts, now);
    if (npts < PREV_VSYNC(pts) + vsync_interval)
        return;

    int num_flips = update_presentation_queue_status(vo);
    vsync = vc->recent_vsync_time + num_flips * vc->vsync_interval;
    now = sync_vdptime(vo);
    pts = FFMAX(pts, now);
    pts = FFMAX(pts, vsync + (vsync_interval >> 2));
    vsync = PREV_VSYNC(pts);
    if (npts < vsync + vsync_interval)
        return;
    pts = vsync + (vsync_interval >> 2);
    vdp_st =
        vdp->presentation_queue_display(vc->flip_queue,
                                        vc->output_surfaces[vc->surface_num],
                                        vo->dwidth, vo->dheight, pts);
    CHECK_ST_WARNING("Error when calling vdp_presentation_queue_display");

    vc->last_queue_time = pts;
    vc->queue_time[vc->surface_num] = pts;
    vc->last_ideal_time = ideal_pts;
    vc->dropped_frame = false;
    vc->surface_num = WRAP_ADD(vc->surface_num, 1, vc->num_output_surfaces);
}

static int decoder_render(struct vo *vo, void *state_ptr)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    struct vdpau_render_state *rndr = (struct vdpau_render_state *)state_ptr;

    if (handle_preemption(vo) < 0)
        return VO_TRUE;

    int max_refs = vc->image_format == IMGFMT_VDPAU_H264 ?
        rndr->info.h264.num_ref_frames : 2;
    if (!IMGFMT_IS_VDPAU(vc->image_format))
        return VO_FALSE;
    if ((vc->decoder == VDP_INVALID_HANDLE || vc->decoder_max_refs < max_refs)
        && !create_vdp_decoder(vo, max_refs))
        return VO_FALSE;

    vdp_st = vdp->decoder_render(vc->decoder, rndr->surface,
                                 (void *)&rndr->info,
                                 rndr->bitstream_buffers_used,
                                 rndr->bitstream_buffers);
    CHECK_ST_WARNING("Failed VDPAU decoder rendering");
    return VO_TRUE;
}


static struct vdpau_render_state *get_surface(struct vo *vo, int number)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;

    if (number >= MAX_VIDEO_SURFACES)
        return NULL;
    if (vc->surface_render[number].surface == VDP_INVALID_HANDLE
        && !vc->is_preempted) {
        VdpStatus vdp_st;
        vdp_st = vdp->video_surface_create(vc->vdp_device, vc->vdp_chroma_type,
                                           vc->vid_width, vc->vid_height,
                                           &vc->surface_render[number].surface);
        CHECK_ST_WARNING("Error when calling vdp_video_surface_create");
    }
    mp_msg(MSGT_VO, MSGL_DBG3, "vdpau vid create: %u\n",
           vc->surface_render[number].surface);
    return &vc->surface_render[number];
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    struct mp_image *reserved_mpi = NULL;
    struct vdpau_render_state *rndr;

    if (IMGFMT_IS_VDPAU(vc->image_format)) {
        rndr = (struct vdpau_render_state *)mpi->planes[0];
        reserved_mpi = mpi;
    } else {
        rndr = get_surface(vo, vc->deint_counter);
        vc->deint_counter = WRAP_ADD(vc->deint_counter, 1, NUM_BUFFERED_VIDEO);
        if (handle_preemption(vo) >= 0) {
            VdpStatus vdp_st;
            const void *destdata[3] = {mpi->planes[0], mpi->planes[2],
                                       mpi->planes[1]};
            if (vc->image_format == IMGFMT_NV12)
                destdata[1] = destdata[2];
            vdp_st = vdp->video_surface_put_bits_y_cb_cr(rndr->surface,
                    vc->vdp_pixel_format, destdata, mpi->stride);
            CHECK_ST_WARNING("Error when calling "
                             "vdp_video_surface_put_bits_y_cb_cr");
        }
    }
    if (mpi->fields & MP_IMGFIELD_ORDERED)
        vc->top_field_first = !!(mpi->fields & MP_IMGFIELD_TOP_FIRST);
    else
        vc->top_field_first = 1;

    add_new_video_surface(vo, rndr->surface, reserved_mpi, mpi->pts);

    return;
}

// warning: the size and pixel format of surface must match that of the
//          surfaces in vc->output_surfaces
static struct mp_image *read_output_surface(struct vdpctx *vc,
                                            VdpOutputSurface surface,
                                            int width, int height)
{
    VdpStatus vdp_st;
    struct vdp_functions *vdp = vc->vdp;
    struct mp_image *image = mp_image_alloc(IMGFMT_BGR32, width, height);
    image->colorspace = MP_CSP_RGB;
    image->levels = vc->colorspace.levels_out; // hardcoded with conv. matrix

    void *dst_planes[] = { image->planes[0] };
    uint32_t dst_pitches[] = { image->stride[0] };
    vdp_st = vdp->output_surface_get_bits_native(surface, NULL, dst_planes,
                                                 dst_pitches);
    CHECK_ST_WARNING("Error when calling vdp_output_surface_get_bits_native");

    return image;
}

static struct mp_image *get_screenshot(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    VdpStatus vdp_st;
    struct vdp_functions *vdp = vc->vdp;

    if (vc->screenshot_surface == VDP_INVALID_HANDLE) {
        vdp_st = vdp->output_surface_create(vc->vdp_device,
                                            OUTPUT_RGBA_FORMAT,
                                            vc->vid_width, vc->vid_height,
                                            &vc->screenshot_surface);
        CHECK_ST_WARNING("Error when calling vdp_output_surface_create");
    }

    VdpRect rc = { .x1 = vc->vid_width, .y1 = vc->vid_height };
    render_video_to_output_surface(vo, vc->screenshot_surface, &rc);

    struct mp_image *image = read_output_surface(vc, vc->screenshot_surface,
                                                 vc->vid_width, vc->vid_height);

    mp_image_set_display_size(image, vo->aspdat.prew, vo->aspdat.preh);

    return image;
}

static struct mp_image *get_window_screenshot(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    int last_surface = WRAP_ADD(vc->surface_num, -1, vc->num_output_surfaces);
    VdpOutputSurface screen = vc->output_surfaces[last_surface];
    struct mp_image *image = read_output_surface(vo->priv, screen,
                                                 vc->output_surface_width,
                                                 vc->output_surface_height);
    mp_image_set_size(image, vo->dwidth, vo->dheight);
    return image;
}

static void release_decoder_surface(void *ptr)
{
    bool *in_use_ptr = ptr;
    *in_use_ptr = false;
}

static struct mp_image *get_decoder_surface(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    if (!IMGFMT_IS_VDPAU(vc->image_format))
        return NULL;

    for (int n = 0; n < MAX_VIDEO_SURFACES; n++) {
        if (!vc->surface_in_use[n]) {
            vc->surface_in_use[n] = true;
            struct mp_image *res =
                mp_image_new_custom_ref(&(struct mp_image){0},
                                        &vc->surface_in_use[n],
                                        release_decoder_surface);
            mp_image_setfmt(res, vc->image_format);
            mp_image_set_size(res, vc->vid_width, vc->vid_height);
            struct vdpau_render_state *rndr = get_surface(vo, n);
            res->planes[0] = (void *)rndr;
            return res;
        }
    }

    mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] no surfaces available in "
           "get_decoder_surface\n");
    // TODO: this probably breaks things forever, provide a dummy buffer?
    return NULL;
}

static int query_format(struct vo *vo, uint32_t format)
{
    int default_flags = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW
        | VFCAP_FLIP;
    switch (format) {
    case IMGFMT_420P:
    case IMGFMT_NV12:
    case IMGFMT_YUYV:
    case IMGFMT_UYVY:
    case IMGFMT_VDPAU_MPEG1:
    case IMGFMT_VDPAU_MPEG2:
    case IMGFMT_VDPAU_H264:
    case IMGFMT_VDPAU_WMV3:
    case IMGFMT_VDPAU_VC1:
    case IMGFMT_VDPAU_MPEG4:
        return default_flags;
    }
    return 0;
}

static void destroy_vdpau_objects(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;

    int i;
    VdpStatus vdp_st;

    free_video_specific(vo);

    if (vc->flip_queue != VDP_INVALID_HANDLE) {
        vdp_st = vdp->presentation_queue_destroy(vc->flip_queue);
        CHECK_ST_WARNING("Error when calling vdp_presentation_queue_destroy");
    }

    if (vc->flip_target != VDP_INVALID_HANDLE) {
        vdp_st = vdp->presentation_queue_target_destroy(vc->flip_target);
        CHECK_ST_WARNING("Error when calling "
                         "vdp_presentation_queue_target_destroy");
    }

    for (i = 0; i < vc->num_output_surfaces; i++) {
        if (vc->output_surfaces[i] == VDP_INVALID_HANDLE)
            continue;
        vdp_st = vdp->output_surface_destroy(vc->output_surfaces[i]);
        CHECK_ST_WARNING("Error when calling vdp_output_surface_destroy");
    }

    for (int i = 0; i < MAX_OSD_PARTS; i++) {
        struct osd_bitmap_surface *sfc = &vc->osd_surfaces[i];
        if (sfc->surface != VDP_INVALID_HANDLE) {
            vdp_st = vdp->bitmap_surface_destroy(sfc->surface);
            CHECK_ST_WARNING("Error when calling vdp_bitmap_surface_destroy");
        }
    }

    vdp_st = vdp->device_destroy(vc->vdp_device);
    CHECK_ST_WARNING("Error when calling vdp_device_destroy");
}

static void uninit(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    /* Destroy all vdpau objects */
    destroy_vdpau_objects(vo);

    vo_x11_uninit(vo);

    // Free bitstream buffers allocated by FFmpeg
    for (int i = 0; i < MAX_VIDEO_SURFACES; i++)
        av_freep(&vc->surface_render[i].bitstream_buffers);
}

static int preinit(struct vo *vo, const char *arg)
{
    struct vdpctx *vc = vo->priv;

    // Mark everything as invalid first so uninit() can tell what has been
    // allocated
    mark_vdpau_objects_uninitialized(vo);

    vc->colorspace = (struct mp_csp_details) MP_CSP_DETAILS_DEFAULTS;
    vc->video_eq.capabilities = MP_CSP_EQ_CAPS_COLORMATRIX;

    vc->deint_type = vc->deint ? FFABS(vc->deint) : 3;
    if (vc->deint < 0)
        vc->deint = 0;

    if (!vo_x11_init(vo))
        return -1;

    // After this calling uninit() should work to free resources

    if (win_x11_init_vdpau_procs(vo) < 0) {
        if (vc->vdp && vc->vdp->device_destroy)
            vc->vdp->device_destroy(vc->vdp_device);
        vo_x11_uninit(vo);
        return -1;
    }

    return 0;
}

static int get_equalizer(struct vo *vo, const char *name, int *value)
{
    struct vdpctx *vc = vo->priv;
    return mp_csp_equalizer_get(&vc->video_eq, name, value) >= 0 ?
           VO_TRUE : VO_NOTIMPL;
}

static bool status_ok(struct vo *vo)
{
    if (!vo->config_ok || handle_preemption(vo) < 0)
        return false;
    return true;
}

static int set_equalizer(struct vo *vo, const char *name, int value)
{
    struct vdpctx *vc = vo->priv;

    if (mp_csp_equalizer_set(&vc->video_eq, name, value) < 0)
        return VO_NOTIMPL;

    if (status_ok(vo))
        update_csc_matrix(vo);
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
    struct vdp_functions *vdp = vc->vdp;

    handle_preemption(vo);

    switch (request) {
    case VOCTRL_GET_DEINTERLACE:
        *(int *)data = vc->deint;
        return VO_TRUE;
    case VOCTRL_SET_DEINTERLACE:
        vc->deint = *(int *)data;
        if (vc->deint)
            vc->deint = vc->deint_type;
        if (vc->deint_type > 2 && status_ok(vo)) {
            VdpStatus vdp_st;
            VdpVideoMixerFeature features[1] =
                {vc->deint_type == 3 ?
                 VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL :
                 VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL};
            VdpBool feature_enables[1] = {vc->deint ? VDP_TRUE : VDP_FALSE};
            vdp_st = vdp->video_mixer_set_feature_enables(vc->video_mixer,
                                                          1, features,
                                                          feature_enables);
            CHECK_ST_WARNING("Error changing deinterlacing settings");
        }
        vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_PAUSE:
        if (vc->dropped_frame)
            vo->want_redraw = true;
        return true;
    case VOCTRL_HWDEC_ALLOC_SURFACE:
        *(struct mp_image **)data = get_decoder_surface(vo);
        return true;
    case VOCTRL_HWDEC_DECODER_RENDER:
        return decoder_render(vo, data);
    case VOCTRL_BORDER:
        vo_x11_border(vo);
        checked_resize(vo);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        vo_x11_fullscreen(vo);
        checked_resize(vo);
        return VO_TRUE;
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
    case VOCTRL_SET_YUV_COLORSPACE:
        vc->colorspace = *(struct mp_csp_details *)data;
        if (status_ok(vo))
            update_csc_matrix(vo);
        vo->want_redraw = true;
        return true;
    case VOCTRL_GET_YUV_COLORSPACE:
        *(struct mp_csp_details *)data = vc->colorspace;
        return true;
    case VOCTRL_ONTOP:
        vo_x11_ontop(vo);
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        vo_x11_update_screeninfo(vo);
        return VO_TRUE;
    case VOCTRL_NEWFRAME:
        vc->deint_queue_pos = next_deint_queue_pos(vo, true);
        if (status_ok(vo))
            video_to_output_surface(vo);
        return true;
    case VOCTRL_SKIPFRAME:
        vc->deint_queue_pos = next_deint_queue_pos(vo, true);
        return true;
    case VOCTRL_REDRAW_FRAME:
        if (status_ok(vo))
            video_to_output_surface(vo);
        return true;
    case VOCTRL_RESET:
        forget_frames(vo);
        return true;
    case VOCTRL_SCREENSHOT: {
        if (!status_ok(vo))
            return false;
        struct voctrl_screenshot_args *args = data;
        if (args->full_window)
            args->out_image = get_window_screenshot(vo);
        else
            args->out_image = get_screenshot(vo);
        return true;
    }
    }
    return VO_NOTIMPL;
}

#define OPT_BASE_STRUCT struct vdpctx

const struct vo_driver video_out_vdpau = {
    .buffer_frames = true,
    .info = &(const struct vo_info_s){
        "VDPAU with X11",
        "vdpau",
        "Rajib Mahapatra <rmahapatra@nvidia.com> and others",
        ""
    },
    .preinit = preinit,
    .query_format = query_format,
    .config = config,
    .control = control,
    .draw_image = draw_image,
    .get_buffered_frame = set_next_frame_info,
    .draw_osd = draw_osd,
    .flip_page_timed = flip_page_timed,
    .check_events = check_events,
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
        {NULL},
    }
};
