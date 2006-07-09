/*
 *
 * X11 DGA Interface
 *
 * Copyright ( C ) 2001, Andreas Ackermann. All Rights Reserved.
 *
 * <acki@acki-netz.de>
 *
 * Sourceforge username: acki2
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"
#include "x11_common.h"
#include "fastmemcpy.h"
#include "mp_msg.h"

#include <X11/Xlib.h>
#include <X11/extensions/xf86dga.h>

#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif

static vo_info_t info = {
#ifdef HAVE_DGA2
    "DGA ( Direct Graphic Access V2.0 )",
#else
#ifdef HAVE_XF86VM
    "DGA ( Direct Graphic Access V1.0+XF86VidModeExt. )",
#else
    "DGA ( Direct Graphic Access V1.0 )",
#endif
#endif
    "dga",
    "Andreas Ackermann <acki@acki-netz.de>",
    ""
};

LIBVO_EXTERN(dga)
//------------------------------------------------------------------
//#define BITSPP (vo_dga_modes[vo_dga_active_mode].vdm_bitspp)
//#define BYTESPP (vo_dga_modes[vo_dga_active_mode].vdm_bytespp)
#define VO_DGA_INVALID_RES 100000
#define HW_MODE (vo_dga_modes[vo_dga_hw_mode])
#define SRC_MODE (vo_dga_modes[vo_dga_src_mode])
struct vd_modes
{
    int vdm_mplayer_depth;
    int vdm_supported;
    int vdm_depth;
    int vdm_bitspp;
    int vdm_bytespp;
    int vdm_rmask;
    int vdm_gmask;
    int vdm_bmask;
    int vdm_hw_mode;
};

//------------------------------------------------------------------

static struct vd_modes vo_dga_modes[] = {
    // these entries describe HW modes
    // however, we use the same entries to tell mplayer what we support
    // so the last two values describe, which HW mode to use and which conversion 
    // function to use for a mode that is not supported by HW

    {0, 0, 0, 0, 0, 0, 0, 0, 0,},
    {15, 0, 15, 16, 2, 0x7c00, 0x03e0, 0x001f, 2,},
    {16, 0, 16, 16, 2, 0xf800, 0x07e0, 0x001f, 2,},
    {24, 0, 24, 24, 3, 0xff0000, 0x00ff00, 0x0000ff, 4},
    {32, 0, 24, 32, 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 4}
};

static int vo_dga_mode_num =
    sizeof(vo_dga_modes) / sizeof(struct vd_modes);

// enable a HW mode (by description)
static int vd_EnableMode(int depth, int bitspp,
                         int rmask, int gmask, int bmask)
{
    int i;

    for (i = 1; i < vo_dga_mode_num; i++)
    {
        if (vo_dga_modes[i].vdm_depth == depth &&
            vo_dga_modes[i].vdm_bitspp == bitspp &&
            vo_dga_modes[i].vdm_rmask == rmask &&
            vo_dga_modes[i].vdm_gmask == gmask &&
            vo_dga_modes[i].vdm_bmask == bmask)
        {
            vo_dga_modes[i].vdm_supported = 1;
            vo_dga_modes[i].vdm_hw_mode = i;
            return i;
        }
    }
    return 0;
}

static int vd_ModeEqual(int depth, int bitspp,
                        int rmask, int gmask, int bmask, int index)
{
    return ((vo_dga_modes[index].vdm_depth == depth &&
             vo_dga_modes[index].vdm_bitspp == bitspp &&
             vo_dga_modes[index].vdm_rmask == rmask &&
             vo_dga_modes[index].vdm_gmask == gmask &&
             vo_dga_modes[index].vdm_bmask == bmask) ? 1 : 0);
}


// enable a HW mode (mplayer_depth decides which)
static int vd_ValidateMode(int mplayer_depth)
{
    int i;

    if (mplayer_depth == 0)
        return 0;
    for (i = 1; i < vo_dga_mode_num; i++)
    {
        if (vo_dga_modes[i].vdm_mplayer_depth == mplayer_depth)
        {
            vo_dga_modes[i].vdm_supported = 1;
            vo_dga_modes[i].vdm_hw_mode = i;
            return i;
        }
    }
    return 0;
}

// do we support this mode? (not important whether native or conversion)
static int vd_ModeValid(int mplayer_depth)
{
    int i;

    if (mplayer_depth == 0)
        return 0;
    for (i = 1; i < vo_dga_mode_num; i++)
    {
        if (vo_dga_modes[i].vdm_mplayer_depth == mplayer_depth &&
            vo_dga_modes[i].vdm_supported != 0)
        {
            return i;
        }
    }
    return 0;
}

static char *vd_GetModeString(int index)
{

#define VO_DGA_MAX_STRING_LEN 100
    static char stringbuf[VO_DGA_MAX_STRING_LEN];

    stringbuf[VO_DGA_MAX_STRING_LEN - 1] = 0;
    snprintf(stringbuf, VO_DGA_MAX_STRING_LEN - 2,
             "depth=%d, bpp=%d, r=%06x, g=%06x, b=%06x, %s (-bpp %d)",
             vo_dga_modes[index].vdm_depth,
             vo_dga_modes[index].vdm_bitspp,
             vo_dga_modes[index].vdm_rmask,
             vo_dga_modes[index].vdm_gmask,
             vo_dga_modes[index].vdm_bmask,
             vo_dga_modes[index].
             vdm_supported ? "native" : "not supported",
             vo_dga_modes[index].vdm_mplayer_depth);
    return stringbuf;
}

//-----------------------------------------------------------------

#if defined(HAVE_XF86VM) && !defined(HAVE_DGA2)
static XF86VidModeModeInfo **vo_dga_vidmodes = NULL;
#endif


static int vo_dga_src_format;
static int vo_dga_width;        // bytes per line in framebuffer
static int vo_dga_vp_width;     // visible pixels per line in 

                                         // framebuffer
static int vo_dga_vp_height;    // visible lines in framebuffer
static int vo_dga_is_running = 0;
static int vo_dga_src_width;    // width of video in pixels
static int vo_dga_src_height;   // height of video in pixels
static int vo_dga_src_offset = 0;       // offset in src
static int vo_dga_vp_offset = 0;        // offset in dest
static int vo_dga_bytes_per_line;       // bytes per line to copy
static int vo_dga_vp_skip;      // dto. for dest 
static int vo_dga_lines;        // num of lines to copy                                
static int vo_dga_hw_mode = 0;  // index in mode list that is actually

                                         // used by framebuffer
static int vo_dga_src_mode = 0; // index in mode list that is used by 

                                         // codec
static int vo_dga_XServer_mode = 0;     // index in mode list for resolution

#ifdef HAVE_DGA2
static XDGAMode *vo_modelines;
static int vo_modecount;
#endif

#define MAX_NR_VIDEO_BUFFERS 3

#define CURRENT_VIDEO_BUFFER \
        (vo_dga_video_buffer[vo_dga_current_video_buffer])

static int vo_dga_nr_video_buffers;     // Total number of frame buffers.
static int vo_dga_current_video_buffer; // Buffer available for rendering.

static struct video_buffer
{
    int y;
    uint8_t *data;
} vo_dga_video_buffer[MAX_NR_VIDEO_BUFFERS];

/* saved src and dst dimensions for SwScaler */
static unsigned int scale_srcW = 0,
    scale_dstW = 0, scale_srcH = 0, scale_dstH = 0;

//---------------------------------------------------------

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride)
{

    char *d;
    unsigned int offset;
    int buffer_stride;

    offset = vo_dga_width * y0 + x0;
    buffer_stride = vo_dga_width;
    d = CURRENT_VIDEO_BUFFER.data + vo_dga_vp_offset;

    switch (HW_MODE.vdm_mplayer_depth)
    {

        case 32:
            vo_draw_alpha_rgb32(w, h, src, srca, stride, d + 4 * offset,
                                4 * buffer_stride);
            break;
        case 24:
            vo_draw_alpha_rgb24(w, h, src, srca, stride, d + 3 * offset,
                                3 * buffer_stride);
            break;
        case 15:
            vo_draw_alpha_rgb15(w, h, src, srca, stride, d + 2 * offset,
                                2 * buffer_stride);
            break;
        case 16:
            vo_draw_alpha_rgb16(w, h, src, srca, stride, d + 2 * offset,
                                2 * buffer_stride);
            break;
    }
}


//---------------------------------------------------------




// quick & dirty - for debugging only 
#if 0
static void fillblock(char *strt, int yoff, int lines, int val)
{
    char *i;

    for (i = strt + yoff * vo_dga_width * HW_MODE.vdm_bytespp;
         i < strt + (lines + yoff) * vo_dga_width * HW_MODE.vdm_bytespp;)
    {
        *i++ = val;
    }
}
#endif

//---------------------------------------------------------

static int draw_frame(uint8_t * src[])
{

    int numlines = vo_dga_lines;

    char *s, *d;

    s = *src;
    d = CURRENT_VIDEO_BUFFER.data + vo_dga_vp_offset;

    mem2agpcpy_pic(d, s,
                   vo_dga_bytes_per_line,
                   numlines,
                   vo_dga_bytes_per_line + vo_dga_vp_skip,
                   vo_dga_bytes_per_line);

    // DBG-COde

#if 0
    d = CURRENT_VIDEO_BUFFER.data + vo_dga_vp_offset;
    fillblock(d, 0, 10, 0x800000ff);
    fillblock(d, 10, 10, 0x8000ff00);
    fillblock(d, 20, 10, 0x80ff0000);
    fillblock(d, 30, 10, 0xff0000ff);
    fillblock(d, 40, 10, 0x800000ff);
    fillblock(d, 50, 10, 0x0f0000ff);
#endif
    return 0;
}

//---------------------------------------------------------

static void check_events(void)
{
    vo_x11_check_events(mDisplay);
}

//---------------------------------------------------------

#include "sub.h"

static void draw_osd(void)
{
    vo_draw_text(vo_dga_src_width, vo_dga_src_height, draw_alpha);
}

static void switch_video_buffers(void)
{
    vo_dga_current_video_buffer =
        (vo_dga_current_video_buffer + 1) % vo_dga_nr_video_buffers;
}

static void flip_page(void)
{
    if (1 < vo_dga_nr_video_buffers)
    {
#ifdef HAVE_DGA2
        XDGASetViewport(mDisplay, mScreen,
                        0, CURRENT_VIDEO_BUFFER.y, XDGAFlipRetrace);
#else
        XF86DGASetViewPort(mDisplay, mScreen, 0, CURRENT_VIDEO_BUFFER.y);
#endif
        switch_video_buffers();
    }
}

//---------------------------------------------------------

static int draw_slice(uint8_t * src[], int stride[],
                           int w, int h, int x, int y)
{
    return 0;
};

//---------------------------------------------------------

static int query_format(uint32_t format)
{

    if ((format & IMGFMT_BGR_MASK) == IMGFMT_BGR &&
        vd_ModeValid(format & 0xff))
    {
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD;
    }

    return 0;
}

//---------------------------------------------------------

static void uninit(void)
{

#ifdef HAVE_DGA2
    XDGADevice *dgadevice;
#endif

    if (!vo_config_count)
        return;

    if (vo_dga_is_running)
    {
        vo_dga_is_running = 0;
        mp_msg(MSGT_VO, MSGL_V, "vo_dga: in uninit\n");
        if (vo_grabpointer)
            XUngrabPointer(mDisplay, CurrentTime);
        XUngrabKeyboard(mDisplay, CurrentTime);
#ifdef HAVE_DGA2
        XDGACloseFramebuffer(mDisplay, mScreen);
        dgadevice = XDGASetMode(mDisplay, mScreen, 0);
        if (dgadevice != NULL)
        {
            XFree(dgadevice);
        }
#else
        XF86DGADirectVideo(mDisplay, mScreen, 0);
        // first disable DirectVideo and then switch mode back!     
#ifdef HAVE_XF86VM
        if (vo_dga_vidmodes != NULL)
        {
            int screen;

            screen = XDefaultScreen(mDisplay);
            mp_msg(MSGT_VO, MSGL_V,
                   "vo_dga: VidModeExt: Switching back..\n");
            // seems some graphics adaptors need this more than once ...
            XF86VidModeSwitchToMode(mDisplay, screen, vo_dga_vidmodes[0]);
            XF86VidModeSwitchToMode(mDisplay, screen, vo_dga_vidmodes[0]);
            XF86VidModeSwitchToMode(mDisplay, screen, vo_dga_vidmodes[0]);
            XF86VidModeSwitchToMode(mDisplay, screen, vo_dga_vidmodes[0]);
            XFree(vo_dga_vidmodes);
        }
#endif
#endif
    }
    vo_x11_uninit();
}


//----------------------------------------------------------
// TODO: check for larger maxy value 
// (useful for double buffering!!!)

static int check_res(int num, int x, int y, int bpp,
                     int new_x, int new_y, int new_vbi, int new_maxy,
                     int *old_x, int *old_y, int *old_vbi, int *old_maxy)
{

    mp_msg(MSGT_VO, MSGL_V,
           "vo_dga: (%3d) Trying %4d x %4d @ %3d Hz @ depth %2d ..", num,
           new_x, new_y, new_vbi, bpp);
    mp_msg(MSGT_VO, MSGL_V, "(old: %dx%d@%d).", *old_x, *old_y, *old_vbi);
    if ((new_x >= x) && (new_y >= y) && (
                                            // prefer a better resolution either in X or in Y
                                            // as long as the other dimension is at least the same
                                            // 
                                            // hmm ... MAYBE it would be more clever to focus on the 
                                            // x-resolution; I had 712x400 and 640x480 and the movie 
                                            // was 640x360; 640x480 would be the 'right thing' here
                                            // but since 712x400 was queried first I got this one. 
                                            // I think there should be a cmd-line switch to let the
                                            // user choose the mode he likes ...   (acki2)
                                            (((new_x < *old_x) &&
                                              !(new_y > *old_y)) ||
                                             ((new_y < *old_y) &&
                                              !(new_x > *old_x)))
                                            // but if we get an identical resolution choose
                                            // the one with the lower refreshrate (saves bandwidth !!!)
                                            // as long as it's above 50 Hz (acki2 on 30/3/2001)
                                            ||
                                            (((new_x == *old_x) &&
                                              (new_y == *old_y) &&
                                              ((new_vbi >= *old_vbi
                                                && *old_vbi < 50)
                                               || (*old_vbi >= 50
                                                   && new_vbi < *old_vbi
                                                   && new_vbi >= 50))) ||
                                             // if everything is equal, then use the mode with the lower 
                                             // stride 
                                             ((new_x == *old_x) &&
                                              (new_y == *old_y) &&
                                              (new_vbi == *old_vbi) &&
                                              (new_maxy > *old_maxy)))))
    {
        *old_x = new_x;
        *old_y = new_y;
        *old_maxy = new_maxy;
        *old_vbi = new_vbi;
        mp_msg(MSGT_VO, MSGL_V, ".ok!!\n");
        return 1;
    } else
    {
        mp_msg(MSGT_VO, MSGL_V, ".no\n");
        return 0;
    }
}



//---------------------------------------------------------

static void init_video_buffers(uint8_t * buffer_base,
                               int view_port_height,
                               int bytes_per_scanline,
                               int max_view_port_y,
                               int use_multiple_buffers)
{
    int bytes_per_buffer = view_port_height * bytes_per_scanline;
    int i;

    if (use_multiple_buffers)
        vo_dga_nr_video_buffers = max_view_port_y / view_port_height;
    else
        vo_dga_nr_video_buffers = 1;

    if (vo_dga_nr_video_buffers > MAX_NR_VIDEO_BUFFERS)
        vo_dga_nr_video_buffers = MAX_NR_VIDEO_BUFFERS;
    if (vo_dga_nr_video_buffers <= 0)
        vo_dga_nr_video_buffers = 1;

    vo_dga_current_video_buffer = 0;

    for (i = 0; i < vo_dga_nr_video_buffers; i++)
    {
        vo_dga_video_buffer[i].y = i * view_port_height;
        vo_dga_video_buffer[i].data = buffer_base + i * bytes_per_buffer;

        // Clear video buffer.
        memset(vo_dga_video_buffer[i].data, 0, bytes_per_buffer);
    }
}

static int config(uint32_t width, uint32_t height,
                       uint32_t d_width, uint32_t d_height,
                       uint32_t flags, char *title, uint32_t format)
{

    int x_off, y_off;
    int wanted_width, wanted_height;

    static unsigned char *vo_dga_base;
    static int prev_width, prev_height;

#ifdef HAVE_DGA2
    // needed to change DGA video mode
    int mX = VO_DGA_INVALID_RES, mY = VO_DGA_INVALID_RES, mVBI =
        100000, mMaxY = 0, i, j = 0;
    int dga_modenum;
    XDGAMode *modeline;
    XDGADevice *dgadevice;
#else
#ifdef HAVE_XF86VM
    unsigned int vm_event, vm_error;
    unsigned int vm_ver, vm_rev;
    int i, j = 0, have_vm = 0;
    int mX = VO_DGA_INVALID_RES, mY = VO_DGA_INVALID_RES, mVBI =
        100000, mMaxY = 0, dga_modenum;
#endif
    int bank, ram;
#endif

    vo_dga_src_format = format;

    wanted_width = d_width;
    wanted_height = d_height;

    if (!wanted_height)
        wanted_height = height;
    if (!wanted_width)
        wanted_width = width;

    if (!vo_dbpp)
    {
        if ((format & IMGFMT_BGR_MASK) == IMGFMT_BGR)
        {
            vo_dga_src_mode = vd_ModeValid(format & 0xff);
        }
    } else
    {
        vo_dga_src_mode = vd_ModeValid(vo_dbpp);
    }
    vo_dga_hw_mode = SRC_MODE.vdm_hw_mode;

    if (!vo_dga_src_mode)
    {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_dga: unsupported video format!\n");
        return 1;
    }

    vo_dga_vp_width = vo_screenwidth;
    vo_dga_vp_height = vo_screenheight;

    mp_msg(MSGT_VO, MSGL_V, "vo_dga: XServer res: %dx%d\n",
           vo_dga_vp_width, vo_dga_vp_height);

// choose a suitable mode ...

#ifdef HAVE_DGA2
// Code to change the video mode added by Michael Graffam
// mgraffam@idsi.net

    mp_msg(MSGT_VO, MSGL_V, "vo_dga: vo_modelines=%p, vo_modecount=%d\n",
           vo_modelines, vo_modecount);

    if (vo_modelines == NULL)
    {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_dga: can't get modelines\n");
        return 1;
    }

    mp_msg(MSGT_VO, MSGL_INFO,
           "vo_dga: DGA 2.0 available :-) Can switch resolution AND depth!\n");
    for (i = 0; i < vo_modecount; i++)
    {
        if (vd_ModeEqual(vo_modelines[i].depth,
                         vo_modelines[i].bitsPerPixel,
                         vo_modelines[i].redMask,
                         vo_modelines[i].greenMask,
                         vo_modelines[i].blueMask, vo_dga_hw_mode))
        {

            mp_msg(MSGT_VO, MSGL_V, "maxy: %4d, depth: %2d, %4dx%4d, ",
                   vo_modelines[i].maxViewportY, vo_modelines[i].depth,
                   vo_modelines[i].imageWidth,
                   vo_modelines[i].imageHeight);
            if (check_res
                (i, wanted_width, wanted_height, vo_modelines[i].depth,
                 vo_modelines[i].viewportWidth,
                 vo_modelines[i].viewportHeight,
                 (unsigned) vo_modelines[i].verticalRefresh,
                 vo_modelines[i].maxViewportY, &mX, &mY, &mVBI, &mMaxY))
                j = i;
        }
    }
    mp_msg(MSGT_VO, MSGL_INFO,
           "vo_dga: Selected hardware mode %4d x %4d @ %3d Hz @ depth %2d, bitspp %2d.\n",
           mX, mY, mVBI, HW_MODE.vdm_depth, HW_MODE.vdm_bitspp);
    mp_msg(MSGT_VO, MSGL_INFO,
           "vo_dga: Video parameters by codec: %3d x %3d, depth %2d, bitspp %2d.\n",
           width, height, SRC_MODE.vdm_depth, SRC_MODE.vdm_bitspp);
    vo_dga_vp_width = mX;
    vo_dga_vp_height = mY;

    if ((flags & VOFLAG_SWSCALE) || (flags & VOFLAG_FULLSCREEN))
    {                           /* -zoom or -fs */
        scale_dstW = (d_width + 7) & ~7;
        scale_dstH = d_height;
        scale_srcW = width;
        scale_srcH = height;
        aspect_save_screenres(mX, mY);
        aspect_save_orig(scale_srcW, scale_srcH);
        aspect_save_prescale(scale_dstW, scale_dstH);
        if (flags & VOFLAG_FULLSCREEN)       /* -fs */
            aspect(&scale_dstW, &scale_dstH, A_ZOOM);
        else if (flags & VOFLAG_SWSCALE)  /* -fs */
            aspect(&scale_dstW, &scale_dstH, A_NOZOOM);
        mp_msg(MSGT_VO, MSGL_INFO,
               "vo_dga: Aspect corrected size for SwScaler: %4d x %4d.\n",
               scale_dstW, scale_dstH);
        /* XXX this is a hack, but I'm lazy ;-) :: atmos */
        width = scale_dstW;
        height = scale_dstH;
    }

    vo_dga_width = vo_modelines[j].bytesPerScanline / HW_MODE.vdm_bytespp;
    dga_modenum = vo_modelines[j].num;
    modeline = vo_modelines + j;

#else

#ifdef HAVE_XF86VM

    mp_msg(MSGT_VO, MSGL_INFO,
           "vo_dga: DGA 1.0 compatibility code: Using XF86VidMode for mode switching!\n");

    if (XF86VidModeQueryExtension(mDisplay, &vm_event, &vm_error))
    {
        XF86VidModeQueryVersion(mDisplay, &vm_ver, &vm_rev);
        mp_msg(MSGT_VO, MSGL_INFO,
               "vo_dga: XF86VidMode Extension v%i.%i\n", vm_ver, vm_rev);
        have_vm = 1;
    } else
    {
        mp_msg(MSGT_VO, MSGL_ERR,
               "vo_dga: XF86VidMode Extension not available.\n");
    }

#define GET_VREFRESH(dotclk, x, y)( (((dotclk)/(x))*1000)/(y) )

    if (have_vm)
    {
        int modecount;

        XF86VidModeGetAllModeLines(mDisplay, mScreen, &modecount,
                                   &vo_dga_vidmodes);

        if (vo_dga_vidmodes != NULL)
        {
            for (i = 0; i < modecount; i++)
            {
                if (check_res(i, wanted_width, wanted_height,
                              vo_dga_modes[vo_dga_hw_mode].vdm_depth,
                              vo_dga_vidmodes[i]->hdisplay,
                              vo_dga_vidmodes[i]->vdisplay,
                              GET_VREFRESH(vo_dga_vidmodes[i]->dotclock,
                                           vo_dga_vidmodes[i]->htotal,
                                           vo_dga_vidmodes[i]->vtotal),
                              0, &mX, &mY, &mVBI, &mMaxY))
                    j = i;
            }

            mp_msg(MSGT_VO, MSGL_INFO,
                   "vo_dga: Selected video mode %4d x %4d @ %3d Hz @ depth %2d, bitspp %2d, video %3d x %3d.\n",
                   mX, mY, mVBI,
                   vo_dga_modes[vo_dga_hw_mode].vdm_depth,
                   vo_dga_modes[vo_dga_hw_mode].vdm_bitspp, width, height);
        } else
        {
            mp_msg(MSGT_VO, MSGL_INFO,
                   "vo_dga: XF86VidMode returned no screens - using current resolution.\n");
        }
        dga_modenum = j;
        vo_dga_vp_width = mX;
        vo_dga_vp_height = mY;
    }

#else
    mp_msg(MSGT_VO, MSGL_INFO,
           "vo_dga: Only have DGA 1.0 extension and no XF86VidMode :-(\n");
    mp_msg(MSGT_VO, MSGL_INFO,
           "        Thus, resolution switching is NOT possible.\n");

#endif
#endif

    vo_dga_src_width = width;
    vo_dga_src_height = height;

    if (vo_dga_src_width > vo_dga_vp_width ||
        vo_dga_src_height > vo_dga_vp_height)
    {
        mp_msg(MSGT_VO, MSGL_ERR,
               "vo_dga: Sorry, video larger than viewport is not yet supported!\n");
        // ugly, do something nicer in the future ...
#ifndef HAVE_DGA2
#ifdef HAVE_XF86VM
        if (vo_dga_vidmodes)
        {
            XFree(vo_dga_vidmodes);
            vo_dga_vidmodes = NULL;
        }
#endif
#endif
        return 1;
    }

    if (vo_dga_vp_width == VO_DGA_INVALID_RES)
    {
        mp_msg(MSGT_VO, MSGL_ERR,
               "vo_dga: Something is wrong with your DGA. There doesn't seem to be a\n"
               "         single suitable mode!\n"
               "         Please file a bug report (see DOCS/HTML/en/bugreports.html)\n");
#ifndef HAVE_DGA2
#ifdef HAVE_XF86VM
        if (vo_dga_vidmodes)
        {
            XFree(vo_dga_vidmodes);
            vo_dga_vidmodes = NULL;
        }
#endif
#endif
        return 1;
    }
// now let's start the DGA thing 

    if (!vo_config_count || width != prev_width || height != prev_height)
    {
#ifdef HAVE_DGA2

        if (!XDGAOpenFramebuffer(mDisplay, mScreen))
        {
            mp_msg(MSGT_VO, MSGL_ERR,
                   "vo_dga: Framebuffer mapping failed!!!\n");
            return 1;
        }

        dgadevice = XDGASetMode(mDisplay, mScreen, dga_modenum);
        XDGASync(mDisplay, mScreen);

        vo_dga_base = dgadevice->data;
        XFree(dgadevice);

        XDGASetViewport(mDisplay, mScreen, 0, 0, XDGAFlipRetrace);

#else

#ifdef HAVE_XF86VM
        if (have_vm)
        {
            XF86VidModeLockModeSwitch(mDisplay, mScreen, 0);
            // Two calls are needed to switch modes on my ATI Rage 128. Why?
            // for riva128 one call is enough!
            XF86VidModeSwitchToMode(mDisplay, mScreen,
                                    vo_dga_vidmodes[dga_modenum]);
            XF86VidModeSwitchToMode(mDisplay, mScreen,
                                    vo_dga_vidmodes[dga_modenum]);
        }
#endif

        XF86DGAGetViewPortSize(mDisplay, mScreen,
                               &vo_dga_vp_width, &vo_dga_vp_height);

        XF86DGAGetVideo(mDisplay, mScreen,
                        (char **) &vo_dga_base, &vo_dga_width, &bank,
                        &ram);

        XF86DGADirectVideo(mDisplay, mScreen,
                           XF86DGADirectGraphics | XF86DGADirectMouse |
                           XF86DGADirectKeyb);

        XF86DGASetViewPort(mDisplay, mScreen, 0, 0);

#endif
    }
    // do some more checkings here ...

    mp_msg(MSGT_VO, MSGL_V,
           "vo_dga: bytes/line: %d, screen res: %dx%d, depth: %d, base: %p, bpp: %d\n",
           vo_dga_width, vo_dga_vp_width, vo_dga_vp_height,
           HW_MODE.vdm_bytespp, vo_dga_base, HW_MODE.vdm_bitspp);

    x_off = (vo_dga_vp_width - vo_dga_src_width) >> 1;
    y_off = (vo_dga_vp_height - vo_dga_src_height) >> 1;

    vo_dga_bytes_per_line = vo_dga_src_width * HW_MODE.vdm_bytespp;
    vo_dga_lines = vo_dga_src_height;

    vo_dga_src_offset = 0;
    vo_dga_vp_offset =
        (y_off * vo_dga_width + x_off) * HW_MODE.vdm_bytespp;

    vo_dga_vp_skip = (vo_dga_width - vo_dga_src_width) * HW_MODE.vdm_bytespp;   // todo

    mp_msg(MSGT_VO, MSGL_V, "vo_dga: vp_off=%d, vp_skip=%d, bpl=%d\n",
           vo_dga_vp_offset, vo_dga_vp_skip, vo_dga_bytes_per_line);


    XGrabKeyboard(mDisplay, DefaultRootWindow(mDisplay), True,
                  GrabModeAsync, GrabModeAsync, CurrentTime);
    if (vo_grabpointer)
        XGrabPointer(mDisplay, DefaultRootWindow(mDisplay), True,
                     ButtonPressMask, GrabModeAsync, GrabModeAsync,
                     None, None, CurrentTime);

    if (!vo_config_count || width != prev_width || height != prev_height)
    {
        init_video_buffers(vo_dga_base,
                           vo_dga_vp_height,
                           vo_dga_width * HW_MODE.vdm_bytespp,
#ifdef HAVE_DGA2
                           modeline->maxViewportY,
#else
                           vo_dga_vp_height,
#endif
                           vo_doublebuffering);
        prev_width = width;
        prev_height = height;
    }

    mp_msg(MSGT_VO, MSGL_V, "vo_dga: Using %d frame buffer%s.\n",
           vo_dga_nr_video_buffers,
           vo_dga_nr_video_buffers == 1 ? "" : "s");

    vo_dga_is_running = 1;
    return 0;
}

static int dga_depths_init = 0;

static int preinit(const char *arg)
{
    if (arg)
    {
        mp_msg(MSGT_VO, MSGL_INFO, "vo_dga: Unknown subdevice: %s\n", arg);
        return ENOSYS;
    }

    if (!vo_init())
        return -1;              // Can't open X11

    if (dga_depths_init == 0)
    {                           // FIXME!?
        int i;

        vo_dga_XServer_mode = vd_ValidateMode(vo_depthonscreen);

        if (vo_dga_XServer_mode == 0)
        {
#ifndef HAVE_DGA2
            mp_msg(MSGT_VO, MSGL_ERR,
                   "vo_dga: Your X-Server is not running in a ");
            mp_msg(MSGT_VO, MSGL_ERR,
                   "resolution supported by DGA driver!\n");
#endif
        }                       //else{
        //  mp_msg(MSGT_VO, MSGL_V, "vo_dga: X running at: %s\n", 
        //            vd_GetModeString(vo_dga_XServer_mode));
        //}                                

#ifdef HAVE_DGA2
        vo_modelines = XDGAQueryModes(mDisplay, mScreen, &vo_modecount);
        if (vo_modelines)
        {
            for (i = 0; i < vo_modecount; i++)
            {
                mp_msg(MSGT_VO, MSGL_V,
                       "vo_dga: (%03d) depth=%d, bpp=%d, r=%08lx, g=%08lx, b=%08lx, %d x %d\n",
                       i, vo_modelines[i].depth,
                       vo_modelines[i].bitsPerPixel,
                       vo_modelines[i].redMask, vo_modelines[i].greenMask,
                       vo_modelines[i].blueMask,
                       vo_modelines[i].viewportWidth,
                       vo_modelines[i].viewportHeight);
                vd_EnableMode(vo_modelines[i].depth,
                              vo_modelines[i].bitsPerPixel,
                              vo_modelines[i].redMask,
                              vo_modelines[i].greenMask,
                              vo_modelines[i].blueMask);
            }
        }
#endif
        dga_depths_init = 1;

        if (!vo_dga_modes[1].vdm_supported
            && vo_dga_modes[2].vdm_supported)
        {
            vo_dga_modes[1].vdm_supported = 1;
        }

        if (!vo_dga_modes[3].vdm_supported
            && vo_dga_modes[4].vdm_supported)
        {
            vo_dga_modes[3].vdm_supported = 1;
        }

        for (i = 1; i < vo_dga_mode_num; i++)
        {
            mp_msg(MSGT_VO, MSGL_INFO, "vo_dga: Mode: %s",
                   vd_GetModeString(i));
            if (vo_dbpp && vo_dbpp != vo_dga_modes[i].vdm_mplayer_depth)
            {
                vo_dga_modes[i].vdm_supported = 0;
                mp_msg(MSGT_VO, MSGL_INFO, " ...disabled by -bpp %d",
                       vo_dbpp);
            }
            mp_msg(MSGT_VO, MSGL_INFO, "\n");
        }
    }

    return 0;
}

static uint32_t get_image(mp_image_t * mpi)
{
    if (!IMGFMT_IS_BGR(mpi->imgfmt) ||
        (IMGFMT_BGR_DEPTH(mpi->imgfmt) !=
         vo_dga_modes[vo_dga_hw_mode].vdm_mplayer_depth)
        || (mpi->type == MP_IMGTYPE_STATIC && vo_dga_nr_video_buffers > 1)
        || (mpi->type == MP_IMGTYPE_IP && vo_dga_nr_video_buffers < 2)
        || (mpi->type == MP_IMGTYPE_IPB))
        return (VO_FALSE);

    if ((mpi->flags & MP_IMGFLAG_ACCEPT_STRIDE) ||
        (mpi->flags & MP_IMGFLAG_ACCEPT_WIDTH &&
         ((vo_dga_bytes_per_line + vo_dga_vp_skip) % (mpi->bpp / 8)) == 0)
        || (mpi->width * (mpi->bpp / 8) ==
            (vo_dga_bytes_per_line + vo_dga_vp_skip)))
    {

        mpi->planes[0] = CURRENT_VIDEO_BUFFER.data + vo_dga_vp_offset;
        mpi->stride[0] = vo_dga_bytes_per_line + vo_dga_vp_skip;
        mpi->width =
            (vo_dga_bytes_per_line + vo_dga_vp_skip) / (mpi->bpp / 8);
        mpi->flags |= MP_IMGFLAG_DIRECT;
        return (VO_TRUE);
    }

    return (VO_FALSE);
}

static int control(uint32_t request, void *data, ...)
{
    switch (request)
    {
        case VOCTRL_GET_IMAGE:
            return get_image(data);
        case VOCTRL_QUERY_FORMAT:
            return query_format(*((uint32_t *) data));
    }
    return VO_NOTIMPL;
}

//---------------------------------------------------------
