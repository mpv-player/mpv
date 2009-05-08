/*
 * VDPAU video output driver
 *
 * Copyright (C) 2008 NVIDIA
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

/**
 * \defgroup  VDPAU_Presentation VDPAU Presentation
 * \ingroup Decoder
 *
 * Actual decoding and presentation are implemented here.
 * All necessary frame information is collected through
 * the "vdpau_render_state" structure after parsing all headers
 * etc. in libavcodec for different codecs.
 *
 * @{
 */

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "mp_msg.h"
#include "options.h"
#include "talloc.h"
#include "video_out.h"
#include "x11_common.h"
#include "aspect.h"
#include "sub.h"
#include "subopt-helper.h"
#include "libmpcodecs/vfcap.h"
#include "libmpcodecs/mp_image.h"

#include "libavcodec/vdpau.h"

#include "font_load.h"

#include "libavutil/common.h"
#include "libavutil/mathematics.h"

#include "libass/ass.h"
#include "libass/ass_mp.h"

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
#define NUM_OUTPUT_SURFACES                2
#define MAX_VIDEO_SURFACES                 50

/* number of palette entries */
#define PALETTE_SIZE 256

/* Initial maximum number of EOSD surfaces */
#define EOSD_SURFACES_INITIAL 512

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
    VdpDeviceCreateX11                *vdp_device_create;
    VdpGetProcAddress                 *vdp_get_proc_address;

    VdpPresentationQueueTarget         flip_target;
    VdpPresentationQueue               flip_queue;

    void                              *vdpau_lib_handle;

    /* output_surfaces[NUM_OUTPUT_SURFACES] is misused for OSD. */
#define osd_surface vc->output_surfaces[NUM_OUTPUT_SURFACES]
    VdpOutputSurface                   output_surfaces[NUM_OUTPUT_SURFACES + 1];
    VdpVideoSurface                    deint_surfaces[3];
    mp_image_t                        *deint_mpi[2];
    int                                output_surface_width, output_surface_height;

    VdpVideoMixer                      video_mixer;
    int                                deint;
    int                                deint_type;
    int                                deint_counter;
    int                                deint_buffer_past_frames;
    int                                pullup;
    float                              denoise;
    float                              sharpen;
    int                                chroma_deint;
    int                                top_field_first;

    VdpDecoder                         decoder;
    int                                decoder_max_refs;

    VdpRect                            src_rect_vid;
    VdpRect                            out_rect_vid;
    int                                border_x, border_y;

    struct vdpau_render_state          surface_render[MAX_VIDEO_SURFACES];
    int                                surface_num;
    int                                vid_surface_num;
    uint32_t                           vid_width, vid_height;
    uint32_t                           image_format;
    VdpChromaType                      vdp_chroma_type;
    VdpYCbCrFormat                     vdp_pixel_format;

    /* draw_osd */
    unsigned char                     *index_data;
    int                                index_data_size;
    uint32_t                           palette[PALETTE_SIZE];

    // EOSD
    // Pool of surfaces
    struct {
        VdpBitmapSurface surface;
        int w;
        int h;
        bool in_use;
    } *eosd_surfaces;

    // List of surfaces to be rendered
    struct {
        VdpBitmapSurface surface;
        VdpRect source;
        VdpRect dest;
        VdpColor color;
    } *eosd_targets;

    int eosd_render_count;
    int eosd_surface_count;

    // Video equalizer
    VdpProcamp procamp;

    bool visible_buf;
    bool paused;

    // These tell what's been initialized and uninit() should free/uninitialize
    bool mode_switched;
};


static void push_deint_surface(struct vo *vo, VdpVideoSurface surface)
{
    struct vdpctx *vc = vo->priv;
    vc->deint_surfaces[2] = vc->deint_surfaces[1];
    vc->deint_surfaces[1] = vc->deint_surfaces[0];
    vc->deint_surfaces[0] = surface;
}

static void flip_page(struct vo *vo);
static void video_to_output_surface(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpTime dummy;
    VdpStatus vdp_st;
    int i;
    if (vc->vid_surface_num < 0)
        return;

    if (vc->deint < 2 || vc->deint_surfaces[0] == VDP_INVALID_HANDLE)
        push_deint_surface(vo, vc->surface_render[vc->vid_surface_num].surface);

    for (i = 0; i <= !!(vc->deint > 1); i++) {
        int field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
        VdpOutputSurface output_surface;
        if (i) {
            // draw_eosd(vo);
            //draw_osd(vo, NULL);
            flip_page(vo);
        }
        if (vc->deint)
            field = (vc->top_field_first == i) ^ (vc->deint > 1) ?
                    VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD:
                    VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
        output_surface = vc->output_surfaces[vc->surface_num];
        vdp_st = vdp->presentation_queue_block_until_surface_idle(vc->flip_queue,
                                                                 output_surface,
                                                                 &dummy);
        CHECK_ST_WARNING("Error when calling vdp->presentation_queue_block_until_surface_idle");

        vdp_st = vdp->video_mixer_render(vc->video_mixer, VDP_INVALID_HANDLE, 0,
                                        field, 2, vc->deint_surfaces + 1,
                                        vc->deint_surfaces[0],
                                        1, &vc->surface_render[vc->vid_surface_num].surface,
                                        &vc->src_rect_vid,
                                        output_surface,
                                        NULL, &vc->out_rect_vid, 0, NULL);
        CHECK_ST_WARNING("Error when calling vdp_video_mixer_render");
        push_deint_surface(vo, vc->surface_render[vc->vid_surface_num].surface);
    }
}

static void resize(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    int i;
    struct vo_rect src_rect;
    struct vo_rect dst_rect;
    struct vo_rect borders;
    calc_src_dst_rects(vo, vc->vid_width, vc->vid_height, &src_rect, &dst_rect,
                       &borders, NULL);
    vc->out_rect_vid.x0 = dst_rect.left;
    vc->out_rect_vid.x1 = dst_rect.right;
    vc->out_rect_vid.y0 = dst_rect.top;
    vc->out_rect_vid.y1 = dst_rect.bottom;
    vc->src_rect_vid.x0 = src_rect.left;
    vc->src_rect_vid.x1 = src_rect.right;
    vc->src_rect_vid.y0 = src_rect.top;
    vc->src_rect_vid.y1 = src_rect.bottom;
    vc->border_x        = borders.left;
    vc->border_y        = borders.top;
#ifdef CONFIG_FREETYPE
    // adjust font size to display size
    force_load_font = 1;
#endif
    vo_osd_changed(OSDTYPE_OSD);

    if (vc->output_surface_width < vo->dwidth || vc->output_surface_height < vo->dheight) {
        if (vc->output_surface_width < vo->dwidth) {
            vc->output_surface_width += vc->output_surface_width >> 1;
            vc->output_surface_width = FFMAX(vc->output_surface_width, vo->dwidth);
        }
        if (vc->output_surface_height < vo->dheight) {
            vc->output_surface_height += vc->output_surface_height >> 1;
            vc->output_surface_height = FFMAX(vc->output_surface_height, vo->dheight);
        }
        // Creation of output_surfaces
        for (i = 0; i <= NUM_OUTPUT_SURFACES; i++) {
            if (vc->output_surfaces[i] != VDP_INVALID_HANDLE)
                vdp->output_surface_destroy(vc->output_surfaces[i]);
            vdp_st = vdp->output_surface_create(vc->vdp_device, VDP_RGBA_FORMAT_B8G8R8A8,
                                               vc->output_surface_width, vc->output_surface_height,
                                               &vc->output_surfaces[i]);
            CHECK_ST_WARNING("Error when calling vdp->output_surface_create");
            mp_msg(MSGT_VO, MSGL_DBG2, "OUT CREATE: %u\n", vc->output_surfaces[i]);
        }
    }
    video_to_output_surface(vo);
    if (vc->visible_buf)
        flip_page(vo);
}

/* Initialize vdp_get_proc_address, called from preinit() */
static int win_x11_init_vdpau_procs(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = talloc_zero(vc, struct vdp_functions);
    vc->vdp = vdp;
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

    vdp_st = vc->vdp_device_create(x11->display, x11->screen,
                                   &vc->vdp_device, &vc->vdp_get_proc_address);
    if (vdp_st != VDP_STATUS_OK) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Error when calling vdp_device_create_x11: %i\n", vdp_st);
        return -1;
    }

    vdp->get_error_string = NULL;
    for (dsc = vdp_func; dsc->offset >= 0; dsc++) {
        vdp_st = vc->vdp_get_proc_address(vc->vdp_device, dsc->id,
                                      (void **)((char *)vdp + dsc->offset));
        if (vdp_st != VDP_STATUS_OK) {
            mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Error when calling vdp_get_proc_address(function id %d): %s\n", dsc->id, vdp->get_error_string ? vdp->get_error_string(vdp_st) : "?");
            return -1;
        }
    }
    return 0;
}

/* Initialize vdpau_flip_queue, called from config() */
static int win_x11_init_vdpau_flip_queue(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    struct vo_x11_state *x11 = vo->x11;
    VdpStatus vdp_st;

    vdp_st = vdp->presentation_queue_target_create_x11(vc->vdp_device, x11->window,
                                                      &vc->flip_target);
    CHECK_ST_ERROR("Error when calling vdp->presentation_queue_target_create_x11");

    vdp_st = vdp->presentation_queue_create(vc->vdp_device, vc->flip_target,
                                           &vc->flip_queue);
    CHECK_ST_ERROR("Error when calling vdp->presentation_queue_create");

    return 0;
}

static int create_vdp_mixer(struct vo *vo, VdpChromaType vdp_chroma_type)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
#define VDP_NUM_MIXER_PARAMETER 3
#define MAX_NUM_FEATURES 5
    int i;
    VdpStatus vdp_st;
    int feature_count = 0;
    VdpVideoMixerFeature features[MAX_NUM_FEATURES];
    VdpBool feature_enables[MAX_NUM_FEATURES];
    static const VdpVideoMixerAttribute denoise_attrib[] = {VDP_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION_LEVEL};
    const void * const denoise_value[] = {&vc->denoise};
    static const VdpVideoMixerAttribute sharpen_attrib[] = {VDP_VIDEO_MIXER_ATTRIBUTE_SHARPNESS_LEVEL};
    const void * const sharpen_value[] = {&vc->sharpen};
    static const VdpVideoMixerAttribute skip_chroma_attrib[] = {VDP_VIDEO_MIXER_ATTRIBUTE_SKIP_CHROMA_DEINTERLACE};
    const uint8_t skip_chroma_value = 1;
    const void * const skip_chroma_value_ptr[] = {&skip_chroma_value};
    static const VdpVideoMixerParameter parameters[VDP_NUM_MIXER_PARAMETER] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE
    };
    const void *const parameter_values[VDP_NUM_MIXER_PARAMETER] = {
        &vc->vid_width,
        &vc->vid_height,
        &vdp_chroma_type
    };
    features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL;
    if (vc->deint == 4)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL;
    if (vc->pullup)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_INVERSE_TELECINE;
    if (vc->denoise)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION;
    if (vc->sharpen)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_SHARPNESS;

    vdp_st = vdp->video_mixer_create(vc->vdp_device, feature_count, features,
                                    VDP_NUM_MIXER_PARAMETER,
                                    parameters, parameter_values,
                                    &vc->video_mixer);
    CHECK_ST_ERROR("Error when calling vdp_video_mixer_create");

    for (i = 0; i < feature_count; i++) feature_enables[i] = VDP_TRUE;
    if (vc->deint < 3)
        feature_enables[0] = VDP_FALSE;
    if (feature_count)
        vdp->video_mixer_set_feature_enables(vc->video_mixer, feature_count, features, feature_enables);
    if (vc->denoise)
        vdp->video_mixer_set_attribute_values(vc->video_mixer, 1, denoise_attrib, denoise_value);
    if (vc->sharpen)
        vdp->video_mixer_set_attribute_values(vc->video_mixer, 1, sharpen_attrib, sharpen_value);
    if (!vc->chroma_deint)
        vdp->video_mixer_set_attribute_values(vc->video_mixer, 1, skip_chroma_attrib, skip_chroma_value_ptr);

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

    for (i = 0; i < 3; i++)
        vc->deint_surfaces[i] = VDP_INVALID_HANDLE;

    for (i = 0; i < 2; i++)
        if (vc->deint_mpi[i]) {
            vc->deint_mpi[i]->usage_count--;
            vc->deint_mpi[i] = NULL;
        }

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
            mp_msg(MSGT_VO, MSGL_V, "[vdpau] Creating H264 hardware decoder for %d reference frames.\n", max_refs);
            break;
        case IMGFMT_VDPAU_WMV3:
            vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_MAIN;
            break;
        case IMGFMT_VDPAU_VC1:
            vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
            break;
    }
    vdp_st = vdp->decoder_create(vc->vdp_device, vdp_decoder_profile,
                                vc->vid_width, vc->vid_height, max_refs, &vc->decoder);
    CHECK_ST_WARNING("Failed creating VDPAU decoder");
    if (vdp_st != VDP_STATUS_OK) {
        vc->decoder = VDP_INVALID_HANDLE;
        vc->decoder_max_refs = 0;
        return 0;
    }
    vc->decoder_max_refs = max_refs;
    return 1;
}

/*
 * connect to X server, create and map window, initialize all
 * VDPAU objects, create different surfaces etc.
 */
static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  char *title, uint32_t format)
{
    struct vdpctx *vc = vo->priv;
    struct vo_x11_state *x11 = vo->x11;
    XVisualInfo vinfo;
    XSetWindowAttributes xswa;
    XWindowAttributes attribs;
    unsigned long xswamask;
    int depth;

#ifdef CONFIG_XF86VM
    int vm = flags & VOFLAG_MODESWITCHING;
#endif

    vc->image_format = format;
    vc->vid_width    = width;
    vc->vid_height   = height;
    free_video_specific(vo);
    if (IMGFMT_IS_VDPAU(vc->image_format) && !create_vdp_decoder(vo, 2))
        return -1;

    vc->visible_buf = false;

    {
#ifdef CONFIG_XF86VM
        if (vm) {
            vo_vm_switch(vo);
            vc->mode_switched = true;
        }
#endif
        XGetWindowAttributes(x11->display, DefaultRootWindow(x11->display),
                             &attribs);
        depth = attribs.depth;
        if (depth != 15 && depth != 16 && depth != 24 && depth != 32)
            depth = 24;
        XMatchVisualInfo(x11->display, x11->screen, depth, TrueColor, &vinfo);

        xswa.background_pixel = 0;
        xswa.border_pixel     = 0;
        /* Do not use CWBackPixel: It leads to VDPAU errors after
           aspect ratio changes. */
        xswamask = CWBorderPixel;

        vo_x11_create_vo_window(vo, &vinfo, vo->dx, vo->dy, d_width, d_height,
                                flags, CopyFromParent, "vdpau", title);
        XChangeWindowAttributes(x11->display, x11->window, xswamask, &xswa);

#ifdef CONFIG_XF86VM
        if (vm) {
            /* Grab the mouse pointer in our window */
            if (vo_grabpointer)
                XGrabPointer(x11->display, x11->window, True, 0,
                             GrabModeAsync, GrabModeAsync,
                             x11->window, None, CurrentTime);
            XSetInputFocus(x11->display, x11->window, RevertToNone, CurrentTime);
        }
#endif
    }

    if ((flags & VOFLAG_FULLSCREEN) && WinID <= 0)
        vo_fs = 1;

    /* -----VDPAU related code here -------- */
    if (vc->flip_queue == VDP_INVALID_HANDLE
        && win_x11_init_vdpau_flip_queue(vo))
        return -1;

    vc->vdp_chroma_type = VDP_CHROMA_TYPE_420;
    switch (vc->image_format) {
        case IMGFMT_YV12:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
            vc->vdp_pixel_format = VDP_YCBCR_FORMAT_YV12;
            break;
        case IMGFMT_NV12:
            vc->vdp_pixel_format = VDP_YCBCR_FORMAT_NV12;
            break;
        case IMGFMT_YUY2:
            vc->vdp_pixel_format = VDP_YCBCR_FORMAT_YUYV;
            vc->vdp_chroma_type  = VDP_CHROMA_TYPE_422;
            break;
        case IMGFMT_UYVY:
            vc->vdp_pixel_format = VDP_YCBCR_FORMAT_UYVY;
            vc->vdp_chroma_type  = VDP_CHROMA_TYPE_422;
    }
    if (create_vdp_mixer(vo, vc->vdp_chroma_type))
        return -1;

    vc->surface_num = 0;
    vc->vid_surface_num = -1;
    resize(vo);

    return 0;
}

static void check_events(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;

    int e = vo_x11_check_events(vo);

    if (e & VO_EVENT_RESIZE)
        resize(vo);

    if ((e & VO_EVENT_EXPOSE || e & VO_EVENT_RESIZE) && vc->paused) {
        /* did we already draw a buffer */
        if (vc->visible_buf) {
            /* redraw the last visible buffer */
            VdpStatus vdp_st;
            vdp_st = vdp->presentation_queue_display(vc->flip_queue,
                                                    vc->output_surfaces[vc->surface_num],
                                                    vo->dwidth, vo->dheight,
                                                    0);
            CHECK_ST_WARNING("Error when calling vdp->presentation_queue_display");
        }
    }
}

static void draw_osd_I8A8(void *ctx, int x0, int y0, int w, int h,
                          unsigned char *src, unsigned char *srca, int stride)
{
    struct vo *vo = ctx;
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpOutputSurface output_surface = vc->output_surfaces[vc->surface_num];
    VdpStatus vdp_st;
    int i, j;
    int pitch;
    int index_data_size_required;
    VdpRect output_indexed_rect_vid;
    VdpOutputSurfaceRenderBlendState blend_state;

    if (!w || !h)
        return;

    index_data_size_required = 2*w*h;
    if (vc->index_data_size < index_data_size_required) {
        vc->index_data      = talloc_realloc_size(vc, vc->index_data,
                                                  index_data_size_required);
        vc->index_data_size = index_data_size_required;
    }

    // index_data creation, component order - I, A, I, A, .....
    for (i = 0; i < h; i++)
        for (j = 0; j < w; j++) {
            vc->index_data[i*2*w + j*2]     =  src [i*stride+j];
            vc->index_data[i*2*w + j*2 + 1] = -srca[i*stride+j];
        }

    output_indexed_rect_vid.x0 = x0;
    output_indexed_rect_vid.y0 = y0;
    output_indexed_rect_vid.x1 = x0 + w;
    output_indexed_rect_vid.y1 = y0 + h;

    pitch = w*2;

    // write source_data to osd_surface.
    vdp_st = vdp->output_surface_put_bits_indexed(osd_surface,
                                                 VDP_INDEXED_FORMAT_I8A8,
                                                 (const void *const*)&vc->index_data,
                                                 &pitch,
                                                 &output_indexed_rect_vid,
                                                 VDP_COLOR_TABLE_FORMAT_B8G8R8X8,
                                                 (void *)vc->palette);
    CHECK_ST_WARNING("Error when calling vdp->output_surface_put_bits_indexed");

    blend_state.struct_version                 = VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION;
    blend_state.blend_factor_source_color      = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;
    blend_state.blend_factor_source_alpha      = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;
    blend_state.blend_factor_destination_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.blend_factor_destination_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.blend_equation_color           = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;
    blend_state.blend_equation_alpha           = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;

    vdp_st = vdp->output_surface_render_output_surface(output_surface,
                                                      &output_indexed_rect_vid,
                                                      osd_surface,
                                                      &output_indexed_rect_vid,
                                                      NULL,
                                                      &blend_state,
                                                      VDP_OUTPUT_SURFACE_RENDER_ROTATE_0);
    CHECK_ST_WARNING("Error when calling vdp->output_surface_render_output_surface");
}

static void draw_eosd(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    VdpOutputSurface output_surface = vc->output_surfaces[vc->surface_num];
    VdpOutputSurfaceRenderBlendState blend_state;
    int i;

    blend_state.struct_version                 = VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION;
    blend_state.blend_factor_source_color      = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA;
    blend_state.blend_factor_source_alpha      = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;
    blend_state.blend_factor_destination_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.blend_factor_destination_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA;
    blend_state.blend_equation_color           = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;
    blend_state.blend_equation_alpha           = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;

    for (i=0; i<vc->eosd_render_count; i++) {
        vdp_st = vdp->output_surface_render_bitmap_surface(
            output_surface, &vc->eosd_targets[i].dest,
            vc->eosd_targets[i].surface, &vc->eosd_targets[i].source,
            &vc->eosd_targets[i].color, &blend_state,
            VDP_OUTPUT_SURFACE_RENDER_ROTATE_0);
        CHECK_ST_WARNING("EOSD: Error when rendering");
    }
}

static void generate_eosd(struct vo *vo, mp_eosd_images_t *imgs)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    VdpRect destRect;
    int j, found;
    ass_image_t *img = imgs->imgs;
    ass_image_t *i;

    // Nothing changed, no need to redraw
    if (imgs->changed == 0)
        return;
    vc->eosd_render_count = 0;
    // There's nothing to render!
    if (!img)
        return;

    if (imgs->changed == 1)
        goto eosd_skip_upload;

    for (j=0; j<vc->eosd_surface_count; j++)
        vc->eosd_surfaces[j].in_use = false;

    for (i = img; i; i = i->next) {
        // Try to reuse a suitable surface
        found = -1;
        for (j=0; j<vc->eosd_surface_count; j++) {
            if (vc->eosd_surfaces[j].surface != VDP_INVALID_HANDLE && !vc->eosd_surfaces[j].in_use &&
                vc->eosd_surfaces[j].w >= i->w && vc->eosd_surfaces[j].h >= i->h) {
                found = j;
                break;
            }
        }
        // None found, allocate a new surface
        if (found < 0) {
            for (j = 0; j < vc->eosd_surface_count; j++) {
                if (!vc->eosd_surfaces[j].in_use) {
                    if (vc->eosd_surfaces[j].surface != VDP_INVALID_HANDLE)
                        vdp->bitmap_surface_destroy(vc->eosd_surfaces[j].surface);
                    found = j;
                    break;
                }
            }
            // Allocate new space for surface/target arrays
            if (found < 0) {
                j = found = vc->eosd_surface_count;
                vc->eosd_surface_count = vc->eosd_surface_count ? vc->eosd_surface_count*2 : EOSD_SURFACES_INITIAL;
                vc->eosd_surfaces = talloc_realloc_size(vc, vc->eosd_surfaces,
                                                vc->eosd_surface_count
                                                * sizeof(*vc->eosd_surfaces));
                vc->eosd_targets  = talloc_realloc_size(vc, vc->eosd_targets,
                                                vc->eosd_surface_count 
                                                * sizeof(*vc->eosd_targets));
                for(j = found; j < vc->eosd_surface_count; j++) {
                    vc->eosd_surfaces[j].surface = VDP_INVALID_HANDLE;
                    vc->eosd_surfaces[j].in_use = false;
                }
            }
            vdp_st = vdp->bitmap_surface_create(vc->vdp_device,
                                                VDP_RGBA_FORMAT_A8,
                i->w, i->h, VDP_TRUE, &vc->eosd_surfaces[found].surface);
            CHECK_ST_WARNING("EOSD: error when creating surface");
            vc->eosd_surfaces[found].w = i->w;
            vc->eosd_surfaces[found].h = i->h;
        }
        vc->eosd_surfaces[found].in_use = true;
        vc->eosd_targets[vc->eosd_render_count].surface = vc->eosd_surfaces[found].surface;
        destRect.x0 = 0;
        destRect.y0 = 0;
        destRect.x1 = i->w;
        destRect.y1 = i->h;
        vdp_st = vdp->bitmap_surface_put_bits_native(vc->eosd_targets[vc->eosd_render_count].surface,
            (const void *) &i->bitmap, &i->stride, &destRect);
        CHECK_ST_WARNING("EOSD: putbits failed");
        vc->eosd_render_count++;
    }

eosd_skip_upload:
    vc->eosd_render_count = 0;
    for (i = img; i; i = i->next) {
        // Render dest, color, etc.
        vc->eosd_targets[vc->eosd_render_count].color.alpha = 1.0 - ((i->color >> 0) & 0xff) / 255.0;
        vc->eosd_targets[vc->eosd_render_count].color.blue  = ((i->color >>  8) & 0xff) / 255.0;
        vc->eosd_targets[vc->eosd_render_count].color.green = ((i->color >> 16) & 0xff) / 255.0;
        vc->eosd_targets[vc->eosd_render_count].color.red   = ((i->color >> 24) & 0xff) / 255.0;
        vc->eosd_targets[vc->eosd_render_count].dest.x0 = i->dst_x;
        vc->eosd_targets[vc->eosd_render_count].dest.y0 = i->dst_y;
        vc->eosd_targets[vc->eosd_render_count].dest.x1 = i->w + i->dst_x;
        vc->eosd_targets[vc->eosd_render_count].dest.y1 = i->h + i->dst_y;
        vc->eosd_targets[vc->eosd_render_count].source.x0 = 0;
        vc->eosd_targets[vc->eosd_render_count].source.y0 = 0;
        vc->eosd_targets[vc->eosd_render_count].source.x1 = i->w;
        vc->eosd_targets[vc->eosd_render_count].source.y1 = i->h;
        vc->eosd_render_count++;
    }
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct vdpctx *vc = vo->priv;
    mp_msg(MSGT_VO, MSGL_DBG2, "DRAW_OSD\n");

    osd_draw_text_ext(osd, vo->dwidth, vo->dheight, vc->border_x, vc->border_y,
                      vc->border_x, vc->border_y, vc->vid_width, vc->vid_height,
                      draw_osd_I8A8, vo);
}

static void flip_page(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    mp_msg(MSGT_VO, MSGL_DBG2, "\nFLIP_PAGE VID:%u -> OUT:%u\n",
           vc->surface_render[vc->vid_surface_num].surface, vc->output_surfaces[vc->surface_num]);

    vdp_st = vdp->presentation_queue_display(vc->flip_queue, vc->output_surfaces[vc->surface_num],
                                            vo->dwidth, vo->dheight,
                                            0);
    CHECK_ST_WARNING("Error when calling vdp->presentation_queue_display");

    vc->surface_num = (vc->surface_num + 1) % NUM_OUTPUT_SURFACES;
    vc->visible_buf = true;
}

static int draw_slice(struct vo *vo, uint8_t *image[], int stride[], int w,
                      int h, int x, int y)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    struct vdpau_render_state *rndr = (struct vdpau_render_state *)image[0];
    int max_refs = vc->image_format == IMGFMT_VDPAU_H264 ? rndr->info.h264.num_ref_frames : 2;
    if (!IMGFMT_IS_VDPAU(vc->image_format))
        return VO_FALSE;
    if ((vc->decoder == VDP_INVALID_HANDLE || vc->decoder_max_refs < max_refs)
        && !create_vdp_decoder(vo, max_refs))
        return VO_FALSE;
    
    vdp_st = vdp->decoder_render(vc->decoder, rndr->surface, (void *)&rndr->info, rndr->bitstream_buffers_used, rndr->bitstream_buffers);
    CHECK_ST_WARNING("Failed VDPAU decoder rendering");
    return VO_TRUE;
}


static int draw_frame(struct vo *vo, uint8_t *src[])
{
    return VO_ERROR;
}

static struct vdpau_render_state *get_surface(struct vo *vo, int number)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;

    if (number > MAX_VIDEO_SURFACES)
        return NULL;
    if (vc->surface_render[number].surface == VDP_INVALID_HANDLE) {
        VdpStatus vdp_st;
        vdp_st = vdp->video_surface_create(vc->vdp_device, vc->vdp_chroma_type,
                                          vc->vid_width, vc->vid_height,
                                          &vc->surface_render[number].surface);
        CHECK_ST_WARNING("Error when calling vdp_video_surface_create");
        if (vdp_st != VDP_STATUS_OK)
            return NULL;
    }
    mp_msg(MSGT_VO, MSGL_DBG2, "VID CREATE: %u\n", vc->surface_render[number].surface);
    return &vc->surface_render[number];
}

static uint32_t draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;

    if (IMGFMT_IS_VDPAU(vc->image_format)) {
        struct vdpau_render_state *rndr = mpi->priv;
        vc->vid_surface_num = rndr - vc->surface_render;
        if (vc->deint_buffer_past_frames) {
            mpi->usage_count++;
            if (vc->deint_mpi[1])
                vc->deint_mpi[1]->usage_count--;
            vc->deint_mpi[1] = vc->deint_mpi[0];
            vc->deint_mpi[0] = mpi;
        }
    } else if (!(mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)) {
        VdpStatus vdp_st;
        void *destdata[3] = {mpi->planes[0], mpi->planes[2], mpi->planes[1]};
        struct vdpau_render_state *rndr = get_surface(vo, vc->deint_counter);
        vc->deint_counter = (vc->deint_counter + 1) % 3;
        vc->vid_surface_num = rndr - vc->surface_render;
        if (vc->image_format == IMGFMT_NV12)
            destdata[1] = destdata[2];
        vdp_st = vdp->video_surface_put_bits_y_cb_cr(rndr->surface,
                                                    vc->vdp_pixel_format,
                                                    (const void *const*)destdata,
                                                    mpi->stride); // pitch
        CHECK_ST_ERROR("Error when calling vdp_video_surface_put_bits_y_cb_cr");
    }
    if (mpi->fields & MP_IMGFIELD_ORDERED)
        vc->top_field_first = !!(mpi->fields & MP_IMGFIELD_TOP_FIRST);
    else
        vc->top_field_first = 1;

    video_to_output_surface(vo);
    return VO_TRUE;
}

static uint32_t get_image(struct vo *vo, mp_image_t *mpi)
{
    struct vdpctx *vc = vo->priv;
    struct vdpau_render_state *rndr;

    // no dr for non-decoding for now
    if (!IMGFMT_IS_VDPAU(vc->image_format)) return VO_FALSE;
    if (mpi->type != MP_IMGTYPE_NUMBERED) return VO_FALSE;

    rndr = get_surface(vo, mpi->number);
    if (!rndr) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] no surfaces available in get_image\n");
        // TODO: this probably breaks things forever, provide a dummy buffer?
        return VO_FALSE;
    }
    mpi->flags |= MP_IMGFLAG_DIRECT;
    mpi->stride[0] = mpi->stride[1] = mpi->stride[2] = 0;
    mpi->planes[0] = mpi->planes[1] = mpi->planes[2] = NULL;
    // hack to get around a check and to avoid a special-case in vd_ffmpeg.c
    mpi->planes[0] = (void *)rndr;
    mpi->num_planes = 1;
    mpi->priv = rndr;
    return VO_TRUE;
}

static int query_format(uint32_t format)
{
    int default_flags = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_OSD | VFCAP_EOSD | VFCAP_EOSD_UNSCALED;
    switch (format) {
        case IMGFMT_YV12:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
        case IMGFMT_NV12:
        case IMGFMT_YUY2:
        case IMGFMT_UYVY:
            return default_flags | VOCAP_NOSLICES;
        case IMGFMT_VDPAU_MPEG1:
        case IMGFMT_VDPAU_MPEG2:
        case IMGFMT_VDPAU_H264:
        case IMGFMT_VDPAU_WMV3:
        case IMGFMT_VDPAU_VC1:
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
        CHECK_ST_WARNING("Error when calling vdp->presentation_queue_destroy");
    }

    if (vc->flip_target != VDP_INVALID_HANDLE) {
        vdp_st = vdp->presentation_queue_target_destroy(vc->flip_target);
        CHECK_ST_WARNING("Error when calling vdp->presentation_queue_target_destroy");
    }

    for (i = 0; i <= NUM_OUTPUT_SURFACES; i++) {
        if (vc->output_surfaces[i] == VDP_INVALID_HANDLE)
            continue;
        vdp_st = vdp->output_surface_destroy(vc->output_surfaces[i]);
        CHECK_ST_WARNING("Error when calling vdp->output_surface_destroy");
    }

    for (i = 0; i < vc->eosd_surface_count; i++) {
        if (vc->eosd_surfaces[i].surface == VDP_INVALID_HANDLE)
            continue;
        vdp_st = vdp->bitmap_surface_destroy(vc->eosd_surfaces[i].surface);
        CHECK_ST_WARNING("Error when calling vdp->bitmap_surface_destroy");
    }

    vdp_st = vdp->device_destroy(vc->vdp_device);
    CHECK_ST_WARNING("Error when calling vdp_device_destroy");
}

static void uninit(struct vo *vo)
{
    struct vdpctx *vc = vo->priv;

    /* Destroy all vdpau objects */
    destroy_vdpau_objects(vo);

#ifdef CONFIG_XF86VM
    if (vc->mode_switched)
        vo_vm_close(vo);
#endif
    vo_x11_uninit(vo);

    dlclose(vc->vdpau_lib_handle);
}

static const char help_msg[] =
    "\n-vo vdpau command line help:\n"
    "Example: mplayer -vo vdpau:deint=2\n"
    "\nOptions:\n"
    "  deint (all modes > 0 respect -field-dominance)\n"
    "    0: no deinterlacing\n"
    "    1: only show first field\n"
    "    2: bob deinterlacing\n"
    "    3: temporal deinterlacing (resource-hungry)\n"
    "    4: temporal-spatial deinterlacing (very resource-hungry)\n"
    "  chroma-deint\n"
    "    Operate on luma and chroma when using temporal deinterlacing (default)\n"
    "    Use nochroma-deint to speed up temporal deinterlacing\n"
    "  pullup\n"
    "    Try to apply inverse-telecine (needs temporal deinterlacing)\n"
    "  denoise\n"
    "    Apply denoising, argument is strength from 0.0 to 1.0\n"
    "  sharpen\n"
    "    Apply sharpening or softening, argument is strength from -1.0 to 1.0\n"
    ;

static int preinit(struct vo *vo, const char *arg)
{
    int i;

    struct vdpctx *vc = talloc_zero(vo, struct vdpctx);
    vo->priv = vc;

    // Mark everything as invalid first so uninit() can tell what has been
    // allocated
    vc->decoder = VDP_INVALID_HANDLE;
    for (i = 0; i < MAX_VIDEO_SURFACES; i++)
        vc->surface_render[i].surface = VDP_INVALID_HANDLE;
    vc->video_mixer = VDP_INVALID_HANDLE;
    vc->flip_queue = VDP_INVALID_HANDLE;
    vc->flip_target = VDP_INVALID_HANDLE;
    for (i = 0; i <= NUM_OUTPUT_SURFACES; i++)
        vc->output_surfaces[i] = VDP_INVALID_HANDLE;
    vc->vdp_device = VDP_INVALID_HANDLE;

    vc->deint_type = 3;
    vc->chroma_deint = 1;
    const opt_t subopts[] = {
        {"deint",   OPT_ARG_INT,   &vc->deint,   (opt_test_f)int_non_neg},
        {"chroma-deint", OPT_ARG_BOOL,  &vc->chroma_deint,  NULL},
        {"pullup",  OPT_ARG_BOOL,  &vc->pullup,  NULL},
        {"denoise", OPT_ARG_FLOAT, &vc->denoise, NULL},
        {"sharpen", OPT_ARG_FLOAT, &vc->sharpen, NULL},
        {NULL}
    };
    if (subopt_parse(arg, subopts) != 0) {
        mp_msg(MSGT_VO, MSGL_FATAL, help_msg);
        return -1;
    }
    if (vc->deint)
        vc->deint_type = vc->deint;
    if (vc->deint > 1)
        vc->deint_buffer_past_frames = 1;

    char *vdpaulibrary = "libvdpau.so.1";
    char *vdpau_device_create = "vdp_device_create_x11";
    vc->vdpau_lib_handle = dlopen(vdpaulibrary, RTLD_LAZY);
    if (!vc->vdpau_lib_handle) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Could not open dynamic library %s\n",
               vdpaulibrary);
        return -1;
    }
    vc->vdp_device_create = dlsym(vc->vdpau_lib_handle, vdpau_device_create);
    if (!vc->vdp_device_create) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Could not find function %s in %s\n",
               vdpau_device_create, vdpaulibrary);
        dlclose(vc->vdpau_lib_handle);
        return -1;
    }
    if (!vo_init(vo)) {
        dlclose(vc->vdpau_lib_handle);
        return -1;
    }

    // After this calling uninit() should work to free resources

    if (win_x11_init_vdpau_procs(vo) < 0) {
        if (vc->vdp->device_destroy)
            vc->vdp->device_destroy(vc->vdp_device);
        vo_x11_uninit(vo);
        dlclose(vc->vdpau_lib_handle);
        return -1;
    }

    vc->output_surface_width = vc->output_surface_height = -1;

    // full grayscale palette.
    for (i = 0; i < PALETTE_SIZE; ++i)
        vc->palette[i] = (i << 16) | (i << 8) | i;

    vc->procamp.struct_version = VDP_PROCAMP_VERSION;
    vc->procamp.brightness = 0.0;
    vc->procamp.contrast   = 1.0;
    vc->procamp.saturation = 1.0;
    vc->procamp.hue        = 0.0;

    return 0;
}

static int get_equalizer(struct vo *vo, const char *name, int *value)
{
    struct vdpctx *vc = vo->priv;

    if (!strcasecmp(name, "brightness"))
        *value = vc->procamp.brightness * 100;
    else if (!strcasecmp(name, "contrast"))
        *value = (vc->procamp.contrast-1.0) * 100;
    else if (!strcasecmp(name, "saturation"))
        *value = (vc->procamp.saturation-1.0) * 100;
    else if (!strcasecmp(name, "hue"))
        *value = vc->procamp.hue * 100 / M_PI;
    else
        return VO_NOTIMPL;
    return VO_TRUE;
}

static int set_equalizer(struct vo *vo, const char *name, int value)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;
    VdpStatus vdp_st;
    VdpCSCMatrix matrix;
    static const VdpVideoMixerAttribute attributes[] = {VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX};
    const void *attribute_values[] = {&matrix};

    if (!strcasecmp(name, "brightness"))
        vc->procamp.brightness = value / 100.0;
    else if (!strcasecmp(name, "contrast"))
        vc->procamp.contrast = value / 100.0 + 1.0;
    else if (!strcasecmp(name, "saturation"))
        vc->procamp.saturation = value / 100.0 + 1.0;
    else if (!strcasecmp(name, "hue"))
        vc->procamp.hue = value / 100.0 * M_PI;
    else
        return VO_NOTIMPL;

    vdp_st = vdp->generate_csc_matrix(&vc->procamp, VDP_COLOR_STANDARD_ITUR_BT_601,
                                     &matrix);
    CHECK_ST_WARNING("Error when generating CSC matrix");
    vdp_st = vdp->video_mixer_set_attribute_values(vc->video_mixer, 1, attributes,
                                                  attribute_values);
    CHECK_ST_WARNING("Error when setting CSC matrix");
    return VO_TRUE;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct vdpctx *vc = vo->priv;
    struct vdp_functions *vdp = vc->vdp;

    switch (request) {
        case VOCTRL_GET_DEINTERLACE:
            *(int*)data = vc->deint;
            return VO_TRUE;
        case VOCTRL_SET_DEINTERLACE:
            vc->deint = *(int*)data;
            if (vc->deint)
                vc->deint = vc->deint_type;
            if (vc->deint_type > 2) {
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
                vc->deint_buffer_past_frames = 1;
            }
            return VO_TRUE;
        case VOCTRL_PAUSE:
            return (vc->paused = true);
        case VOCTRL_RESUME:
            return (vc->paused = false);
        case VOCTRL_QUERY_FORMAT:
            return query_format(*(uint32_t *)data);
        case VOCTRL_GET_IMAGE:
            return get_image(vo, data);
        case VOCTRL_DRAW_IMAGE:
            return draw_image(vo, data);
        case VOCTRL_BORDER:
            vo_x11_border(vo);
            resize(vo);
            return VO_TRUE;
        case VOCTRL_FULLSCREEN:
            vo_x11_fullscreen(vo);
            resize(vo);
            return VO_TRUE;
        case VOCTRL_GET_PANSCAN:
            return VO_TRUE;
        case VOCTRL_SET_PANSCAN:
            resize(vo);
            return VO_TRUE;
        case VOCTRL_SET_EQUALIZER: {
            struct voctrl_set_equalizer_args *args = data;
            return set_equalizer(vo, args->name, args->value);
        }
        case VOCTRL_GET_EQUALIZER:
        {
            struct voctrl_get_equalizer_args *args = data;
            return get_equalizer(vo, args->name, args->valueptr);
        }
        case VOCTRL_ONTOP:
            vo_x11_ontop(vo);
            return VO_TRUE;
        case VOCTRL_UPDATE_SCREENINFO:
            update_xinerama_info(vo);
            return VO_TRUE;
        case VOCTRL_DRAW_EOSD:
            if (!data)
                return VO_FALSE;
            generate_eosd(vo, data);
            draw_eosd(vo);
            return VO_TRUE;
        case VOCTRL_GET_EOSD_RES: {
            mp_eosd_res_t *r = data;
            r->mt = r->mb = r->ml = r->mr = 0;
            if (vo_fs) {
                r->w = vo->opts->vo_screenwidth;
                r->h = vo->opts->vo_screenheight;
                r->ml = r->mr = vc->border_x;
                r->mt = r->mb = vc->border_y;
            } else {
                r->w = vo->dwidth;
                r->h = vo->dheight;
            }
            return VO_TRUE;
        }
    }
    return VO_NOTIMPL;
}

const struct vo_driver video_out_vdpau = {
    .is_new = 1,
    .info = &(const struct vo_info_s){
        "VDPAU with X11",
        "vdpau",
        "Rajib Mahapatra <rmahapatra@nvidia.com> and others",
        ""
    },
    .preinit = preinit,
    .config = config,
    .control = control,
    .draw_frame = draw_frame,
    .draw_slice = draw_slice,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
};
