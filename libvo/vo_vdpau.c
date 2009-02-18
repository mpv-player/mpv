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

#include "config.h"
#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "x11_common.h"
#include "aspect.h"
#include "sub.h"

#include "libavcodec/vdpau.h"

#include "gui/interface.h"

#include "libavutil/common.h"

static vo_info_t info = {
    "VDPAU with X11",
    "vdpau",
    "Rajib Mahapatra <rmahapatra@nvidia.com> and others",
    ""
};

LIBVO_EXTERN(vdpau)

#define CHECK_ST_ERROR(message) \
    if (vdp_st != VDP_STATUS_OK) { \
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] %s: %s\n", \
               message, vdp_get_error_string(vdp_st)); \
        return -1; \
    }

#define CHECK_ST_WARNING(message) \
    if (vdp_st != VDP_STATUS_OK) \
        mp_msg(MSGT_VO, MSGL_WARN, "[vdpau] %s: %s\n", \
               message, vdp_get_error_string(vdp_st));

/* number of video and output surfaces */
#define NUM_OUTPUT_SURFACES                3
#define MAX_VIDEO_SURFACES                 50

/* number of palette entries */
#define PALETTE_SIZE 256

/*
 * Global variable declaration - VDPAU specific
 */

/* Declaration for all variables of win_x11_init_vdpau_procs() and
 * win_x11_init_vdpau_flip_queue() functions
 */
static VdpDevice                          vdp_device;
static VdpDeviceCreateX11                *vdp_device_create;
static VdpGetProcAddress                 *vdp_get_proc_address;

static VdpPresentationQueueTarget         vdp_flip_target;
static VdpPresentationQueue               vdp_flip_queue;

static VdpDeviceDestroy                  *vdp_device_destroy;
static VdpVideoSurfaceCreate             *vdp_video_surface_create;
static VdpVideoSurfaceDestroy            *vdp_video_surface_destroy;

static VdpGetErrorString                 *vdp_get_error_string;

/* May be used in software filtering/postprocessing options
 * in MPlayer (./mplayer -vf ..) if we copy video_surface data to
 * system memory.
 */
static VdpVideoSurfacePutBitsYCbCr       *vdp_video_surface_put_bits_y_cb_cr;
static VdpOutputSurfacePutBitsNative     *vdp_output_surface_put_bits_native;

static VdpOutputSurfaceCreate            *vdp_output_surface_create;
static VdpOutputSurfaceDestroy           *vdp_output_surface_destroy;

/* VideoMixer puts video_surface data on displayable output_surface. */
static VdpVideoMixerCreate               *vdp_video_mixer_create;
static VdpVideoMixerDestroy              *vdp_video_mixer_destroy;
static VdpVideoMixerRender               *vdp_video_mixer_render;

static VdpPresentationQueueTargetDestroy *vdp_presentation_queue_target_destroy;
static VdpPresentationQueueCreate        *vdp_presentation_queue_create;
static VdpPresentationQueueDestroy       *vdp_presentation_queue_destroy;
static VdpPresentationQueueDisplay       *vdp_presentation_queue_display;
static VdpPresentationQueueBlockUntilSurfaceIdle *vdp_presentation_queue_block_until_surface_idle;
static VdpPresentationQueueTargetCreateX11       *vdp_presentation_queue_target_create_x11;

/* output_surfaces[2] is used in composite-picture. */
static VdpOutputSurfaceRenderOutputSurface       *vdp_output_surface_render_output_surface;
static VdpOutputSurfacePutBitsIndexed            *vdp_output_surface_put_bits_indexed;

static VdpDecoderCreate                          *vdp_decoder_create;
static VdpDecoderDestroy                         *vdp_decoder_destroy;
static VdpDecoderRender                          *vdp_decoder_render;

static void                              *vdpau_lib_handle;
static VdpOutputSurface                   output_surfaces[NUM_OUTPUT_SURFACES];
static int                                output_surface_width, output_surface_height;

static VdpVideoMixer                      video_mixer;

static VdpDecoder                         decoder;
static int                                decoder_max_refs;

static VdpRect                            src_rect_vid;
static VdpRect                            out_rect_vid;
static int                                border_x, border_y;

static struct vdpau_render_state          surface_render[MAX_VIDEO_SURFACES];
static int                                surface_num;
static int                                vid_surface_num;
static uint32_t                           vid_width, vid_height;
static uint32_t                           image_format;
static const VdpChromaType                vdp_chroma_type = VDP_CHROMA_TYPE_420;

/* draw_osd */
static unsigned char                     *index_data;
static int                                index_data_size;
static uint32_t                           palette[PALETTE_SIZE];

/*
 * X11 specific
 */
static int                                visible_buf;
static int                                int_pause;

static void video_to_output_surface(void)
{
    VdpTime dummy;
    VdpStatus vdp_st;
    VdpOutputSurface output_surface = output_surfaces[surface_num];
    if (vid_surface_num < 0)
        return;

    vdp_st = vdp_presentation_queue_block_until_surface_idle(vdp_flip_queue,
                                                             output_surface,
                                                             &dummy);
    CHECK_ST_WARNING("Error when calling vdp_presentation_queue_block_until_surface_idle")

    vdp_st = vdp_video_mixer_render(video_mixer, VDP_INVALID_HANDLE, 0,
                                    VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
                                    0, NULL, surface_render[vid_surface_num].surface, 0, NULL, &src_rect_vid,
                                    output_surface,
                                    NULL, &out_rect_vid, 0, NULL);
    CHECK_ST_WARNING("Error when calling vdp_video_mixer_render")
}

static void resize(void)
{
    VdpStatus vdp_st;
    int i;
    struct vo_rect src_rect;
    struct vo_rect dst_rect;
    struct vo_rect borders;
    calc_src_dst_rects(vid_width, vid_height, &src_rect, &dst_rect, &borders, NULL);
    out_rect_vid.x0 = dst_rect.left;
    out_rect_vid.x1 = dst_rect.right;
    out_rect_vid.y0 = dst_rect.top;
    out_rect_vid.y1 = dst_rect.bottom;
    src_rect_vid.x0 = src_rect.left;
    src_rect_vid.x1 = src_rect.right;
    src_rect_vid.y0 = src_rect.top;
    src_rect_vid.y1 = src_rect.bottom;
    border_x        = borders.left;
    border_y        = borders.top;
#ifdef CONFIG_FREETYPE
    // adjust font size to display size
    force_load_font = 1;
#endif
    vo_osd_changed(OSDTYPE_OSD);

    if (output_surface_width < vo_dwidth || output_surface_height < vo_dheight) {
        if (output_surface_width < vo_dwidth) {
            output_surface_width += output_surface_width >> 1;
            output_surface_width = FFMAX(output_surface_width, vo_dwidth);
        }
        if (output_surface_height < vo_dheight) {
            output_surface_height += output_surface_height >> 1;
            output_surface_height = FFMAX(output_surface_height, vo_dheight);
        }
        // Creation of output_surfaces
        for (i = 0; i < NUM_OUTPUT_SURFACES; i++) {
            if (output_surfaces[i] != VDP_INVALID_HANDLE)
                vdp_output_surface_destroy(output_surfaces[i]);
            vdp_st = vdp_output_surface_create(vdp_device, VDP_RGBA_FORMAT_B8G8R8A8,
                                               output_surface_width, output_surface_height,
                                               &output_surfaces[i]);
            CHECK_ST_WARNING("Error when calling vdp_output_surface_create")
            mp_msg(MSGT_VO, MSGL_DBG2, "OUT CREATE: %u\n", output_surfaces[i]);
        }
    }
    video_to_output_surface();
    if (visible_buf)
        flip_page();
}

/* Initialize vdp_get_proc_address, called from preinit() */
static int win_x11_init_vdpau_procs(void)
{
    VdpStatus vdp_st;

    struct vdp_function {
        const int id;
        void *pointer;
    };

    const struct vdp_function *dsc;

    static const struct vdp_function vdp_func[] = {
        {VDP_FUNC_ID_GET_ERROR_STRING,          &vdp_get_error_string},
        {VDP_FUNC_ID_DEVICE_DESTROY,            &vdp_device_destroy},
        {VDP_FUNC_ID_VIDEO_SURFACE_CREATE,      &vdp_video_surface_create},
        {VDP_FUNC_ID_VIDEO_SURFACE_DESTROY,     &vdp_video_surface_destroy},
        {VDP_FUNC_ID_VIDEO_SURFACE_PUT_BITS_Y_CB_CR,
                        &vdp_video_surface_put_bits_y_cb_cr},
        {VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_NATIVE,
                        &vdp_output_surface_put_bits_native},
        {VDP_FUNC_ID_OUTPUT_SURFACE_CREATE,     &vdp_output_surface_create},
        {VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY,    &vdp_output_surface_destroy},
        {VDP_FUNC_ID_VIDEO_MIXER_CREATE, &vdp_video_mixer_create},
        {VDP_FUNC_ID_VIDEO_MIXER_DESTROY,       &vdp_video_mixer_destroy},
        {VDP_FUNC_ID_VIDEO_MIXER_RENDER,        &vdp_video_mixer_render},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_DESTROY,
                        &vdp_presentation_queue_target_destroy},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE, &vdp_presentation_queue_create},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY,
                        &vdp_presentation_queue_destroy},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY,
                        &vdp_presentation_queue_display},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE,
                        &vdp_presentation_queue_block_until_surface_idle},
        {VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11,
                        &vdp_presentation_queue_target_create_x11},
        {VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_OUTPUT_SURFACE,
                        &vdp_output_surface_render_output_surface},
        {VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_INDEXED,
                        &vdp_output_surface_put_bits_indexed},
        {VDP_FUNC_ID_DECODER_CREATE,            &vdp_decoder_create},
        {VDP_FUNC_ID_DECODER_RENDER,            &vdp_decoder_render},
        {VDP_FUNC_ID_DECODER_DESTROY,           &vdp_decoder_destroy},
        {0, NULL}
    };

    vdp_st = vdp_device_create(mDisplay, mScreen,
                               &vdp_device, &vdp_get_proc_address);
    if (vdp_st != VDP_STATUS_OK) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Error when calling vdp_device_create_x11: %i\n", vdp_st);
        return -1;
    }

    for (dsc = vdp_func; dsc->pointer; dsc++) {
        vdp_st = vdp_get_proc_address(vdp_device, dsc->id, dsc->pointer);
        if (vdp_st != VDP_STATUS_OK) {
            mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Error when calling vdp_get_proc_address(function id %d): %s\n", dsc->id, vdp_get_error_string ? vdp_get_error_string(vdp_st) : "?");
            return -1;
        }
    }
    return 0;
}

/* Initialize vdpau_flip_queue, called from config() */
static int win_x11_init_vdpau_flip_queue(void)
{
    VdpStatus vdp_st;

    vdp_st = vdp_presentation_queue_target_create_x11(vdp_device, vo_window,
                                                      &vdp_flip_target);
    CHECK_ST_ERROR("Error when calling vdp_presentation_queue_target_create_x11")

    vdp_st = vdp_presentation_queue_create(vdp_device, vdp_flip_target,
                                           &vdp_flip_queue);
    CHECK_ST_ERROR("Error when calling vdp_presentation_queue_create")

    return 0;
}

static int create_vdp_mixer(VdpChromaType vdp_chroma_type) {
#define VDP_NUM_MIXER_PARAMETER 3
    VdpStatus vdp_st;
    static const VdpVideoMixerParameter parameters[VDP_NUM_MIXER_PARAMETER] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE
    };
    const void *const parameter_values[VDP_NUM_MIXER_PARAMETER] = {
        &vid_width,
        &vid_height,
        &vdp_chroma_type
    };

    vdp_st = vdp_video_mixer_create(vdp_device, 0, 0,
                                    VDP_NUM_MIXER_PARAMETER,
                                    parameters, parameter_values,
                                    &video_mixer);
    CHECK_ST_ERROR("Error when calling vdp_video_mixer_create")

    return 0;
}

// Free everything specific to a certain video file
static void free_video_specific(void) {
    int i;
    VdpStatus vdp_st;

    if (decoder != VDP_INVALID_HANDLE)
        vdp_decoder_destroy(decoder);
    decoder = VDP_INVALID_HANDLE;
    decoder_max_refs = -1;

    for (i = 0; i < MAX_VIDEO_SURFACES; i++) {
        if (surface_render[i].surface != VDP_INVALID_HANDLE) {
          vdp_st = vdp_video_surface_destroy(surface_render[i].surface);
          CHECK_ST_WARNING("Error when calling vdp_video_surface_destroy")
        }
        surface_render[i].surface = VDP_INVALID_HANDLE;
    }

    if (video_mixer != VDP_INVALID_HANDLE)
        vdp_st = vdp_video_mixer_destroy(video_mixer);
    CHECK_ST_WARNING("Error when calling vdp_video_mixer_destroy")
    video_mixer = VDP_INVALID_HANDLE;
}

/*
 * connect to X server, create and map window, initialize all
 * VDPAU objects, create different surfaces etc.
 */
static int config(uint32_t width, uint32_t height, uint32_t d_width,
                  uint32_t d_height, uint32_t flags, char *title,
                  uint32_t format)
{
    XVisualInfo vinfo;
    XSetWindowAttributes xswa;
    XWindowAttributes attribs;
    unsigned long xswamask;
    int depth;

#ifdef CONFIG_XF86VM
    int vm = flags & VOFLAG_MODESWITCHING;
#endif

    image_format = format;

    int_pause = 0;
    visible_buf = 0;

#ifdef CONFIG_GUI
    if (use_gui)
        guiGetEvent(guiSetShVideo, 0);  // the GUI will set up / resize our window
    else
#endif
    {
#ifdef CONFIG_XF86VM
        if (vm)
            vo_vm_switch();
        else
#endif
        XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);
        depth = attribs.depth;
        if (depth != 15 && depth != 16 && depth != 24 && depth != 32)
            depth = 24;
        XMatchVisualInfo(mDisplay, mScreen, depth, TrueColor, &vinfo);

        xswa.background_pixel = 0;
        xswa.border_pixel     = 0;
        xswamask = CWBackPixel | CWBorderPixel;

        vo_x11_create_vo_window(&vinfo, vo_dx, vo_dy, d_width, d_height,
                                flags, CopyFromParent, "vdpau", title);
        XChangeWindowAttributes(mDisplay, vo_window, xswamask, &xswa);

#ifdef CONFIG_XF86VM
        if (vm) {
            /* Grab the mouse pointer in our window */
            if (vo_grabpointer)
                XGrabPointer(mDisplay, vo_window, True, 0,
                             GrabModeAsync, GrabModeAsync,
                             vo_window, None, CurrentTime);
            XSetInputFocus(mDisplay, vo_window, RevertToNone, CurrentTime);
        }
#endif
    }

    if ((flags & VOFLAG_FULLSCREEN) && WinID <= 0)
        vo_fs = 1;

    /* -----VDPAU related code here -------- */

    free_video_specific();

    if (vdp_flip_queue == VDP_INVALID_HANDLE && win_x11_init_vdpau_flip_queue())
        return -1;

    // video width and height
    vid_width  = width;
    vid_height = height;

    if (create_vdp_mixer(vdp_chroma_type))
        return -1;

    surface_num = 0;
    vid_surface_num = -1;
    resize();

    return 0;
}

static void check_events(void)
{
    int e = vo_x11_check_events(mDisplay);

    if (e & VO_EVENT_RESIZE)
        resize();

    if ((e & VO_EVENT_EXPOSE || e & VO_EVENT_RESIZE) && int_pause) {
        /* did we already draw a buffer */
        if (visible_buf) {
            /* redraw the last visible buffer */
            VdpStatus vdp_st;
            vdp_st = vdp_presentation_queue_display(vdp_flip_queue,
                                                    output_surfaces[surface_num],
                                                    vo_dwidth, vo_dheight,
                                                    0);
            CHECK_ST_WARNING("Error when calling vdp_presentation_queue_display")
        }
    }
}

static void draw_osd_I8A8(int x0,int y0, int w,int h, unsigned char *src,
                          unsigned char *srca, int stride)
{
    VdpOutputSurface output_surface = output_surfaces[surface_num];
    VdpStatus vdp_st;
    int i, j;
    int pitch;
    int index_data_size_required;
    VdpRect output_indexed_rect_vid;
    VdpOutputSurfaceRenderBlendState blend_state;

    if (!w || !h)
        return;

    index_data_size_required = 2*w*h;
    if (index_data_size < index_data_size_required) {
        index_data      = realloc(index_data, index_data_size_required);
        index_data_size = index_data_size_required;
    }

    // index_data creation, component order - I, A, I, A, .....
    for (i = 0; i < h; i++)
        for (j = 0; j < w; j++) {
            index_data[i*2*w + j*2]     =  src [i*stride+j];
            index_data[i*2*w + j*2 + 1] = -srca[i*stride+j];
        }

    output_indexed_rect_vid.x0 = x0;
    output_indexed_rect_vid.y0 = y0;
    output_indexed_rect_vid.x1 = x0 + w;
    output_indexed_rect_vid.y1 = y0 + h;

    pitch = w*2;

    // write source_data to output_surfaces[2].
    vdp_st = vdp_output_surface_put_bits_indexed(output_surfaces[2],
                                                 VDP_INDEXED_FORMAT_I8A8,
                                                 (const void *const*)&index_data,
                                                 &pitch,
                                                 &output_indexed_rect_vid,
                                                 VDP_COLOR_TABLE_FORMAT_B8G8R8X8,
                                                 (void *)palette);
    CHECK_ST_WARNING("Error when calling vdp_output_surface_put_bits_indexed")

    blend_state.struct_version                 = VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION;
    blend_state.blend_factor_source_color      = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;
    blend_state.blend_factor_source_alpha      = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE;
    blend_state.blend_factor_destination_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.blend_factor_destination_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.blend_equation_color           = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;
    blend_state.blend_equation_alpha           = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD;

    vdp_st = vdp_output_surface_render_output_surface(output_surface,
                                                      &output_indexed_rect_vid,
                                                      output_surfaces[2],
                                                      &output_indexed_rect_vid,
                                                      NULL,
                                                      &blend_state,
                                                      VDP_OUTPUT_SURFACE_RENDER_ROTATE_0);
    CHECK_ST_WARNING("Error when calling vdp_output_surface_render_output_surface")
}

static void draw_osd(void)
{
    mp_msg(MSGT_VO, MSGL_DBG2, "DRAW_OSD\n");

    vo_draw_text_ext(vo_dwidth, vo_dheight, border_x, border_y, border_x, border_y,
                     vid_width, vid_height, draw_osd_I8A8);
}

static void flip_page(void)
{
    VdpStatus vdp_st;
    mp_msg(MSGT_VO, MSGL_DBG2, "\nFLIP_PAGE VID:%u -> OUT:%u\n",
           surface_render[vid_surface_num].surface, output_surfaces[surface_num]);

    vdp_st = vdp_presentation_queue_display(vdp_flip_queue, output_surfaces[surface_num],
                                            vo_dwidth, vo_dheight,
                                            0);
    CHECK_ST_WARNING("Error when calling vdp_presentation_queue_display")

    surface_num = !surface_num;
    visible_buf = 1;
}

static int draw_slice(uint8_t *image[], int stride[], int w, int h,
                      int x, int y)
{
    VdpStatus vdp_st;
    struct vdpau_render_state *rndr = (struct vdpau_render_state *)image[0];
    int max_refs = image_format == IMGFMT_VDPAU_H264 ? rndr->info.h264.num_ref_frames : 2;
    if (!IMGFMT_IS_VDPAU(image_format))
        return VO_FALSE;
    if (decoder == VDP_INVALID_HANDLE || decoder_max_refs < max_refs) {
        VdpDecoderProfile vdp_decoder_profile;
        if (decoder != VDP_INVALID_HANDLE)
            vdp_decoder_destroy(decoder);
        decoder = VDP_INVALID_HANDLE;
        switch (image_format) {
            case IMGFMT_VDPAU_MPEG1:
                vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG1;
                break;
            case IMGFMT_VDPAU_MPEG2:
                vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG2_MAIN;
                break;
            case IMGFMT_VDPAU_H264:
                vdp_decoder_profile = VDP_DECODER_PROFILE_H264_HIGH;
                break;
            case IMGFMT_VDPAU_WMV3:
                vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_MAIN;
                break;
            case IMGFMT_VDPAU_VC1:
                vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
                break;
        }
        vdp_st = vdp_decoder_create(vdp_device, vdp_decoder_profile, vid_width, vid_height, max_refs, &decoder);
        CHECK_ST_WARNING("Failed creating VDPAU decoder");
        decoder_max_refs = max_refs;
    }
    vdp_st = vdp_decoder_render(decoder, rndr->surface, (void *)&rndr->info, rndr->bitstream_buffers_used, rndr->bitstream_buffers);
    CHECK_ST_WARNING("Failed VDPAU decoder rendering");
    return VO_TRUE;
}


static int draw_frame(uint8_t *src[])
{
    return VO_ERROR;
}

static struct vdpau_render_state *get_surface(int number)
{
    if (number > MAX_VIDEO_SURFACES)
        return NULL;
    if (surface_render[number].surface == VDP_INVALID_HANDLE) {
        VdpStatus vdp_st;
        vdp_st = vdp_video_surface_create(vdp_device, vdp_chroma_type,
                                          vid_width, vid_height,
                                          &surface_render[number].surface);
        CHECK_ST_WARNING("Error when calling vdp_video_surface_create")
        if (vdp_st != VDP_STATUS_OK)
            return NULL;
    }
    mp_msg(MSGT_VO, MSGL_DBG2, "VID CREATE: %u\n", surface_render[number].surface);
    return &surface_render[number];
}

static uint32_t draw_image(mp_image_t *mpi)
{
    if (IMGFMT_IS_VDPAU(image_format)) {
        struct vdpau_render_state *rndr = mpi->priv;
        vid_surface_num = rndr - surface_render;
    } else if (!(mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)) {
        VdpStatus vdp_st;
        void *destdata[3] = {mpi->planes[0], mpi->planes[2], mpi->planes[1]};
        struct vdpau_render_state *rndr = get_surface(0);
        vid_surface_num = rndr - surface_render;
        vdp_st = vdp_video_surface_put_bits_y_cb_cr(rndr->surface,
                                                    VDP_YCBCR_FORMAT_YV12,
                                                    (const void *const*)destdata,
                                                    mpi->stride); // pitch
        CHECK_ST_ERROR("Error when calling vdp_video_surface_put_bits_y_cb_cr")
    }

    video_to_output_surface();
    return VO_TRUE;
}

static uint32_t get_image(mp_image_t *mpi)
{
    struct vdpau_render_state *rndr;

    // no dr for non-decoding for now
    if (!IMGFMT_IS_VDPAU(image_format)) return VO_FALSE;
    if (mpi->type != MP_IMGTYPE_NUMBERED) return VO_FALSE;

    rndr = get_surface(mpi->number);
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
    int default_flags = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_OSD;
    switch (format) {
        case IMGFMT_YV12:
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

static void DestroyVdpauObjects(void)
{
    int i;
    VdpStatus vdp_st;

    free_video_specific();

    vdp_st = vdp_presentation_queue_destroy(vdp_flip_queue);
    CHECK_ST_WARNING("Error when calling vdp_presentation_queue_destroy")

    vdp_st = vdp_presentation_queue_target_destroy(vdp_flip_target);
    CHECK_ST_WARNING("Error when calling vdp_presentation_queue_target_destroy")

    for (i = 0; i < NUM_OUTPUT_SURFACES; i++) {
        vdp_st = vdp_output_surface_destroy(output_surfaces[i]);
        output_surfaces[i] = VDP_INVALID_HANDLE;
        CHECK_ST_WARNING("Error when calling vdp_output_surface_destroy")
    }

    vdp_st = vdp_device_destroy(vdp_device);
    CHECK_ST_WARNING("Error when calling vdp_device_destroy")
}

static void uninit(void)
{
    if (!vo_config_count)
        return;
    visible_buf = 0;

    /* Destroy all vdpau objects */
    DestroyVdpauObjects();

    free(index_data);
    index_data = NULL;

#ifdef CONFIG_XF86VM
    vo_vm_close();
#endif
    vo_x11_uninit();

    dlclose(vdpau_lib_handle);
}

static int preinit(const char *arg)
{
    int i;
    static const char *vdpaulibrary = "libvdpau.so.1";
    static const char *vdpau_device_create = "vdp_device_create_x11";

    if (arg) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Unknown subdevice: %s\n", arg);
        return ENOSYS;
    }

    vdpau_lib_handle = dlopen(vdpaulibrary, RTLD_LAZY);
    if (!vdpau_lib_handle) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Could not open dynamic library %s\n",
               vdpaulibrary);
        return -1;
    }
    vdp_device_create = dlsym(vdpau_lib_handle, vdpau_device_create);
    if (!vdp_device_create) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] Could not find function %s in %s\n",
               vdpau_device_create, vdpaulibrary);
        return -1;
    }
    if (!vo_init() || win_x11_init_vdpau_procs())
        return -1;

    decoder = VDP_INVALID_HANDLE;
    for (i = 0; i < MAX_VIDEO_SURFACES; i++)
        surface_render[i].surface = VDP_INVALID_HANDLE;
    video_mixer = VDP_INVALID_HANDLE;
    for (i = 0; i < NUM_OUTPUT_SURFACES; i++)
        output_surfaces[i] = VDP_INVALID_HANDLE;
    vdp_flip_queue = VDP_INVALID_HANDLE;
    output_surface_width = output_surface_height = -1;

    // full grayscale palette.
    for (i = 0; i < PALETTE_SIZE; ++i)
        palette[i] = (i << 16) | (i << 8) | i;
    index_data = NULL;
    index_data_size = 0;

    return 0;
}

static int control(uint32_t request, void *data, ...)
{
    switch (request) {
        case VOCTRL_PAUSE:
            return (int_pause = 1);
        case VOCTRL_RESUME:
            return (int_pause = 0);
        case VOCTRL_QUERY_FORMAT:
            return query_format(*(uint32_t *)data);
        case VOCTRL_GET_IMAGE:
            return get_image(data);
        case VOCTRL_DRAW_IMAGE:
            return draw_image(data);
        case VOCTRL_GUISUPPORT:
            return VO_TRUE;
        case VOCTRL_BORDER:
            vo_x11_border();
            resize();
	    return VO_TRUE;
        case VOCTRL_FULLSCREEN:
            vo_x11_fullscreen();
            resize();
            return VO_TRUE;
        case VOCTRL_GET_PANSCAN:
            return VO_TRUE;
        case VOCTRL_SET_PANSCAN:
            resize();
            return VO_TRUE;
        case VOCTRL_SET_EQUALIZER: {
            va_list ap;
            int value;

            va_start(ap, data);
            value = va_arg(ap, int);

            va_end(ap);
            return vo_x11_set_equalizer(data, value);
        }
        case VOCTRL_GET_EQUALIZER: {
            va_list ap;
            int *value;

            va_start(ap, data);
            value = va_arg(ap, int *);

            va_end(ap);
            return vo_x11_get_equalizer(data, value);
        }
        case VOCTRL_ONTOP:
            vo_x11_ontop();
            return VO_TRUE;
        case VOCTRL_UPDATE_SCREENINFO:
            update_xinerama_info();
            return VO_TRUE;
    }
    return VO_NOTIMPL;
}

/* @} */
