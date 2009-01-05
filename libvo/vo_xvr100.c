/*
 * SUN XVR-100 (ATI Radeon 7000) VO driver for SPARC Solaris(at least)
 *
 * Copyright (C) 2000-2004 Robin Kay <komadori [at] gekkou [dot] co [dot] uk>
 * Copyright (C) 2004 Jake Goerzen
 * Copyright (C) 2007 Denes Balatoni
 *
 * written for xine by
 * Robin Kay <komadori [at] gekkou [dot] co [dot] uk>
 *
 * Sun XVR-100 framebuffer graciously donated by Jake Goerzen.
 *
 * Ported to mplayer by Denes Balatoni
 * Contains portions from the mga and tdfix_vid vo drivers
 *
 * no double-buffering, as it would slow down playback (waiting for vertical retraces)
 * FIXME: only YV12 supported for now
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/fbio.h>
#include <sys/visual_io.h>
#include <strings.h>
#include <sys/mman.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"
#include "geometry.h"
#include "fastmemcpy.h"
#include "sub.h"
#include "mp_msg.h"


static const vo_info_t info = {
    "sun xvr-100",
    "xvr100",
    "Denes Balatoni",
    ""
};

const LIBVO_EXTERN(xvr100)

#define PFB_VRAM_MMAPBASE 0x08000000
#define PFB_VRAM_MMAPLEN 0x04000000 /* depends on memory size */
#define PFB_REGS_MMAPBASE 0x10000000
#define PFB_REGS_MMAPLEN 0x00040000

#define PFB_CLOCK_CNTL_INDEX 0x002
#define PFB_CLOCK_CNTL_DATA 0x003

#define PFB_MC_FB_LOCATION 0x052

#define PFB_OV0_Y_X_START 0x100
#define PFB_OV0_Y_X_END 0x101
#define PFB_OV0_REG_LOAD_CNTL 0x104
#define PFB_OV0_REG_LOAD_LOCK 0x00000001
#define PFB_OV0_REG_LOAD_LOCK_READBACK 0x00000008
#define PFB_OV0_SCALE_CNTL 0x108
#define PFB_OV0_SCALE_EN 0x417f0000/*417f0000*/
#define PFB_OV0_SCALE_YUV12 0x00000A00
#define PFB_OV0_SCALE_VYUY422 0x00000B00
#define PFB_OV0_V_INC 0x109
#define PFB_OV0_P1_V_ACCUM_INIT 0x10A
#define PFB_OV0_P23_V_ACCUM_INIT 0x10B
#define PFB_OV0_P1_BLANK_LINES_AT_TOP 0x10C
#define PFB_OV0_P23_BLANK_LINES_AT_TOP 0x10D
#define PFB_OV0_BASE_ADDR 0x10F
#define PFB_OV0_BUF0_BASE_ADRS 0x110
#define PFB_OV0_BUF1_BASE_ADRS 0x111
#define PFB_OV0_BUF2_BASE_ADRS 0x112
#define PFB_OV0_BUF3_BASE_ADRS 0x113
#define PFB_OV0_BUF4_BASE_ADRS 0x114
#define PFB_OV0_BUF5_BASE_ADRS 0x115
#define PFB_OV0_VID_BUF_PITCH0_VALUE 0x118
#define PFB_OV0_VID_BUF_PITCH1_VALUE 0x119
#define PFB_OV0_AUTO_FLIP_CNTL 0x11C
#define PFB_OV0_AUTO_FLIP_BUF0 0x00000200
#define PFB_OV0_AUTO_FLIP_BUF3 0x00000243
#define PFB_OV0_DEINTERLACE_PATTERN 0x11D
#define PFB_OV0_H_INC 0x120
#define PFB_OV0_STEP_BY 0x121
#define PFB_OV0_P1_H_ACCUM_INIT 0x122
#define PFB_OV0_P23_H_ACCUM_INIT 0x123
#define PFB_OV0_P1_X_START_END 0x125
#define PFB_OV0_P2_X_START_END 0x126
#define PFB_OV0_P3_X_START_END 0x127
#define PFB_OV0_FILTER_CNTL 0x128
#define PFB_OV0_FILTER_EN 0x0000000f
#define PFB_OV0_GRPH_KEY_CLR_LOW 0x13B
#define PFB_OV0_GRPH_KEY_CLR_HIGH 0x13C
#define PFB_OV0_KEY_CNTL 0x13D
#define PFB_OV0_KEY_EN 0x00000121

#define PFB_DISP_MERGE_CNTL 0x358
#define PFB_DISP_MERGE_EN 0xffff0000



static char pfb_devname[]="/dev/fbs/pfb0";
static int pfb_devfd;
static uint8_t *pfb_vbase;
static volatile uint32_t *pfb_vregs;
static int pfb_buffer[3];
static int pfb_stride[3];
static int pfb_srcwidth, pfb_srcheight, pfb_dstwidth, pfb_dstheight;
static int pfb_native_format=PFB_OV0_SCALE_YUV12;
static int pfb_deinterlace_en=0;
static short int pfb_wx0, pfb_wy0, pfb_wx1, pfb_wy1;
static int pfb_xres,pfb_yres;
static int pfb_free_top;
static int pfb_fs;
static int pfb_usecolorkey=0;
static uint32_t pfb_colorkey;



void pfb_overlay_on(void) {
    int h_inc, h_step, ecp_div;

    pfb_vregs[PFB_CLOCK_CNTL_INDEX] = (pfb_vregs[PFB_CLOCK_CNTL_INDEX] & ~0x0000003f) | 0x00000008;
    ecp_div = (pfb_vregs[PFB_CLOCK_CNTL_DATA] >> 8) & 0x3;
    h_inc = (pfb_srcwidth << (12 + ecp_div)) / pfb_dstwidth;
    h_step = 1;

    while (h_inc > 0x1fff) {
        h_inc >>= 1;
        h_step++;
    }

    pfb_vregs[PFB_OV0_REG_LOAD_CNTL] = PFB_OV0_REG_LOAD_LOCK;
    while (!(pfb_vregs[PFB_OV0_REG_LOAD_CNTL] & PFB_OV0_REG_LOAD_LOCK_READBACK))
        usleep(100);

    pfb_vregs[PFB_DISP_MERGE_CNTL] = PFB_DISP_MERGE_EN;
    pfb_vregs[PFB_OV0_Y_X_START] = (pfb_wy0 << 16) | pfb_wx0;
    pfb_vregs[PFB_OV0_Y_X_END] = ((pfb_wy1 - 1) << 16) | (pfb_wx1 - 1);
    pfb_vregs[PFB_OV0_V_INC] = ((pfb_deinterlace_en ? pfb_srcheight/2 : pfb_srcheight) << 20) / pfb_dstheight;
    pfb_vregs[PFB_OV0_P1_V_ACCUM_INIT] = 0x00180001;
    pfb_vregs[PFB_OV0_P23_V_ACCUM_INIT] = 0x00180001;
    pfb_vregs[PFB_OV0_P1_BLANK_LINES_AT_TOP] = (((pfb_deinterlace_en ? pfb_srcheight/2 : pfb_srcheight) - 1) << 16) | 0xfff;
    pfb_vregs[PFB_OV0_P23_BLANK_LINES_AT_TOP] = (((pfb_deinterlace_en ? pfb_srcheight/2 : pfb_srcheight) / 2 - 1) << 16) | 0x7ff;
    pfb_vregs[PFB_OV0_BASE_ADDR] = (pfb_vregs[PFB_MC_FB_LOCATION] & 0xffff) << 16;
    pfb_vregs[PFB_OV0_VID_BUF_PITCH0_VALUE] = pfb_deinterlace_en ? pfb_stride[0]*2 : pfb_stride[0];
    pfb_vregs[PFB_OV0_VID_BUF_PITCH1_VALUE] = pfb_deinterlace_en ? pfb_stride[1]*2 : pfb_stride[1];
    pfb_vregs[PFB_OV0_DEINTERLACE_PATTERN] = 0x000aaaaa;
    pfb_vregs[PFB_OV0_H_INC] = ((h_inc / 2) << 16) | h_inc;
    pfb_vregs[PFB_OV0_STEP_BY] = (h_step << 8) | h_step;
    pfb_vregs[PFB_OV0_P1_H_ACCUM_INIT] = (((0x00005000 + h_inc) << 7) & 0x000f8000) | (((0x00005000 + h_inc) << 15) & 0xf0000000);
    pfb_vregs[PFB_OV0_P23_H_ACCUM_INIT] = (((0x0000A000 + h_inc) << 6) & 0x000f8000) | (((0x0000A000 + h_inc) << 14) & 0x70000000);
    pfb_vregs[PFB_OV0_P1_X_START_END] = pfb_srcwidth - 1;
    pfb_vregs[PFB_OV0_P2_X_START_END] = (pfb_srcwidth / 2) - 1;
    pfb_vregs[PFB_OV0_P3_X_START_END] = (pfb_srcwidth / 2) - 1;
    pfb_vregs[PFB_OV0_FILTER_CNTL] = PFB_OV0_FILTER_EN;

    if (pfb_usecolorkey) {
        pfb_vregs[PFB_OV0_GRPH_KEY_CLR_LOW] = pfb_colorkey;
        pfb_vregs[PFB_OV0_GRPH_KEY_CLR_HIGH] = pfb_colorkey | 0xff000000;
        pfb_vregs[PFB_OV0_KEY_CNTL] = PFB_OV0_KEY_EN;
    } else {
        pfb_vregs[PFB_OV0_KEY_CNTL] = 0x010;
    }

    pfb_vregs[PFB_OV0_SCALE_CNTL] = PFB_OV0_SCALE_EN | pfb_native_format;

    pfb_vregs[PFB_OV0_REG_LOAD_CNTL] = 0;

    pfb_vregs[PFB_OV0_BUF0_BASE_ADRS] = pfb_buffer[0];
    pfb_vregs[PFB_OV0_BUF1_BASE_ADRS] = pfb_buffer[1] | 0x00000001;
    pfb_vregs[PFB_OV0_BUF2_BASE_ADRS] = pfb_buffer[2] | 0x00000001;

    pfb_vregs[PFB_OV0_AUTO_FLIP_CNTL] = PFB_OV0_AUTO_FLIP_BUF0;
}

void pfb_overlay_off(void) {
    pfb_vregs[PFB_OV0_SCALE_CNTL] = 0;
}

void center_overlay(void) {
    if (pfb_xres > pfb_dstwidth) {
        pfb_wx0 = (pfb_xres - pfb_dstwidth) / 2;
        pfb_wx1 = pfb_wx0 + pfb_dstwidth;
    }
    else {
        pfb_wx0 = 0;
        pfb_wx1 = pfb_xres;
    }

    if (pfb_yres > pfb_dstheight) {
        pfb_wy0 = (pfb_yres - pfb_dstheight) / 2;
        pfb_wy1 = pfb_wy0 + pfb_dstheight;
    }
    else {
        pfb_wy0 = 0;
        pfb_wy1 = pfb_yres;
    }
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,
                  uint32_t d_height, uint32_t flags, char *title,
                  uint32_t format) {
    int memsize;

    pfb_srcwidth=width;
    pfb_srcheight=height;

    if (pfb_srcwidth>1536)
        mp_msg(MSGT_VO, MSGL_WARN, "vo_xvr100: XVR-100 can not handle width greater than 1536 pixels!\n");

    if (!(flags & VOFLAG_XOVERLAY_SUB_VO)) {
        aspect_save_orig(width,height);
        aspect_save_prescale(d_width,d_height);
        aspect_save_screenres(pfb_xres,pfb_yres);
        if( flags&VOFLAG_FULLSCREEN) { /* -fs */
            aspect(&pfb_dstwidth,&pfb_dstheight, A_ZOOM);
            pfb_fs = 1;
        } else {
            aspect(&pfb_dstwidth,&pfb_dstheight, A_NOZOOM);
            pfb_fs = 0;
        }
    } else {
        pfb_dstwidth=d_width;
        pfb_dstheight=d_height;
    }

    center_overlay();

    pfb_stride[0]=(pfb_srcwidth+15) & ~15;
    pfb_stride[1]=pfb_stride[2]=(((pfb_srcwidth+1)>>1)+15) & ~15;
    memsize = (pfb_stride[0]*pfb_srcheight+pfb_stride[1]*((pfb_srcheight+1) & ~1));
    if (memsize > pfb_free_top) {
        mp_msg(MSGT_VO, MSGL_FATAL, "vo_xvr100: out of VRAM! \n");
        return 1;
    }
    pfb_buffer[0] = pfb_free_top - memsize;
    pfb_buffer[1] = pfb_buffer[0] + pfb_stride[0]*pfb_srcheight;
    pfb_buffer[2] = pfb_buffer[1] + pfb_stride[1]*((pfb_srcheight+1)>>1);

    pfb_overlay_on();

    return 0;
}

static int preinit(const char *arg) {
    struct vis_identifier ident;
    struct fbgattr attr;

    if ((pfb_devfd = open(pfb_devname, O_RDWR)) < 0) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_xvr100: Error: can't open framebuffer device '%s'\n", pfb_devname);
        return 1;
    }

    if (ioctl(pfb_devfd, VIS_GETIDENTIFIER, &ident) < 0) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_xvr100: Error: ioctl failed (VIS_GETIDENTIFIER), bad device (%s)\n", pfb_devname);
        return 1;
    }

    if (strcmp("SUNWpfb", ident.name) == 0) {
        mp_msg(MSGT_VO, MSGL_INFO, "vo_xvr100: SUNWpfb (XVR-100/ATI Radeon 7000) detected \n");
    }
    else {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_xvr100: Error: '%s' is not a SUN XVR-100 framebuffer device\n", pfb_devname);
        return 1;
    }

    if (ioctl(pfb_devfd, FBIOGATTR, &attr) < 0) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_xvr100: Error: ioctl failed (FBIOGATTR)\n");
        close(pfb_devfd);
        return 1;
    }

    pfb_free_top = attr.fbtype.fb_size - 0x2000;
    pfb_xres = attr.fbtype.fb_width;
    pfb_yres = attr.fbtype.fb_height;

    if ((pfb_vbase = mmap(NULL, PFB_VRAM_MMAPLEN, PROT_READ | PROT_WRITE,
                          MAP_SHARED, pfb_devfd, PFB_VRAM_MMAPBASE)) == MAP_FAILED) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_xvr100: Error: unable to memory map framebuffer\n");
        close(pfb_devfd);
        return 1;
    }

    if ((pfb_vregs = (uint32_t *)(void *)mmap(NULL, PFB_REGS_MMAPLEN, PROT_READ | PROT_WRITE,
                                              MAP_SHARED, pfb_devfd, PFB_REGS_MMAPBASE)) == MAP_FAILED) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_xvr100: Error: unable to memory map framebuffer\n");
        munmap(pfb_vbase, PFB_VRAM_MMAPLEN);
        close(pfb_devfd);
        return 1;
    }

  return 0;
}

static void uninit(void)
{
    if (!vo_config_count)
        return;

    pfb_overlay_off();
    munmap(pfb_vbase, PFB_VRAM_MMAPLEN);
    munmap(pfb_vregs, PFB_REGS_MMAPLEN);
}

static uint32_t pfb_fullscreen(void) {
    if (!pfb_fs) {
        aspect(&pfb_dstwidth,&pfb_dstheight, A_ZOOM);
        pfb_fs = 1;
    } else {
        aspect(&pfb_dstwidth,&pfb_dstheight, A_NOZOOM);
        pfb_fs = 0;
    }

    center_overlay();

    pfb_overlay_on();

    return VO_TRUE;
}

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
    vo_draw_alpha_yv12(w,h,src,srca,stride,pfb_vbase+pfb_buffer[0]+pfb_stride[0]*y0+x0,pfb_stride[0]);
}

static void draw_osd(void)
{
    vo_draw_text(pfb_srcwidth, pfb_srcheight,draw_alpha);
}

static void check_events(void)
{
}

static void flip_page(void)
{
}

static uint32_t get_image(mp_image_t *mpi){
    if (mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE;
    if (!(mpi->flags&MP_IMGFLAG_PLANAR)) return VO_FALSE; // FIXME: impossible for YV12, right?
    if (!(mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE)) return VO_FALSE;

    mpi->planes[0]=pfb_vbase + pfb_buffer[0];
    mpi->planes[1]=pfb_vbase + pfb_buffer[1];
    mpi->planes[2]=pfb_vbase + pfb_buffer[2];
    mpi->stride[0]=pfb_stride[0];
    mpi->stride[1]=mpi->stride[2]=pfb_stride[1];
    mpi->flags|=MP_IMGFLAG_DIRECT;

    return VO_TRUE;
}

static int draw_frame(uint8_t *src[])
{
    mp_msg(MSGT_VO,MSGL_WARN,"!!! vo_xvr100::draw_frame() called !!!\n");
    return 0;
}


static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
    mem2agpcpy_pic(pfb_vbase + pfb_buffer[0] + pfb_stride[0] * y + x, src[0], w, h, pfb_stride[0], stride[0]);
    w>>=1; h>>=1; x>>=1; y>>=1;
    mem2agpcpy_pic(pfb_vbase + pfb_buffer[1] + pfb_stride[1] * y + x, src[1], w, h, pfb_stride[1], stride[1]);
    mem2agpcpy_pic(pfb_vbase + pfb_buffer[2] + pfb_stride[1] * y + x, src[2], w, h, pfb_stride[1], stride[2]);

    return 0;
}


static uint32_t draw_image(mp_image_t *mpi){
    // if -dr or -slices then do nothing:
    if (mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK)) return VO_TRUE;

    draw_slice(mpi->planes,mpi->stride,mpi->w,mpi->h,0,0);

    return VO_TRUE;
}

static uint32_t pfb_set_colorkey(mp_colorkey_t* colork) {
    pfb_colorkey = colork->x11;
    pfb_usecolorkey = 1;

    pfb_overlay_on();

    return VO_TRUE;
}

static uint32_t pfb_set_window(mp_win_t* w) {
    pfb_dstwidth = w->w;
    pfb_dstheight = w->h;
    pfb_wx0 = w->x;
    pfb_wy0 = w->y;
    pfb_wx1 = w->x + pfb_dstwidth;
    pfb_wy1 = w->y + pfb_dstheight;

    pfb_overlay_on();

    return VO_TRUE;
}

static int query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
    case IMGFMT_I420:
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |VFCAP_HWSCALE_UP|VFCAP_HWSCALE_DOWN|VFCAP_ACCEPT_STRIDE;
    }
    return 0;
}

static int control(uint32_t request, void *data, ...)
{
    switch (request) {
        case VOCTRL_GET_IMAGE:
            return get_image(data);
        case VOCTRL_QUERY_FORMAT:
            return query_format(*((uint32_t*)data));
        case VOCTRL_DRAW_IMAGE:
            return draw_image(data);
        case VOCTRL_FULLSCREEN:
            return pfb_fullscreen();
        case VOCTRL_XOVERLAY_SUPPORT:
            return VO_TRUE;
        case VOCTRL_XOVERLAY_SET_COLORKEY:
            return pfb_set_colorkey(data);
        case VOCTRL_XOVERLAY_SET_WIN:
            return pfb_set_window(data);
    }

    return VO_NOTIMPL;
}
