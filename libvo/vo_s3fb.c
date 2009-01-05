/*
 * copyright (C) 2006 Mark Sanderson <mmp@kiora.ath.cx>
 *
 * 30-Mar-2006 Modified from tdfxfb.c by Mark Zealey
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

/* Hints and tricks:
 * - Use -dr to get direct rendering
 * - Use -vf yuy2 to get yuy2 rendering, *MUCH* faster than yv12
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/io.h>

#include "config.h"
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include "mp_msg.h"
#include "fastmemcpy.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"
#include "sub.h"

static const vo_info_t info =
  {
    "S3 Virge over fbdev",
    "s3fb",
    "Mark Sanderson <mmp@kiora.ath.cx>",
    ""
  };

const LIBVO_EXTERN(s3fb)

typedef struct vga_type {
  int cr38, cr39, cr53;
  unsigned char *mmio;
} vga_t;

static vga_t *v = NULL;
static int fd = -1;
static struct fb_fix_screeninfo fb_finfo;
static struct fb_var_screeninfo fb_vinfo;
static uint32_t in_width, in_height, in_format, in_depth, in_s3_format,
  screenwidth, screenheight, screendepth, screenstride,
  vidwidth, vidheight, vidx, vidy, page, offset, sreg;
static char *inpage, *inpage0, *smem = NULL;
static void (*alpha_func)();

static void clear_screen(void);

/* streams registers */
#define PSTREAM_CONTROL_REG 0x8180
#define COL_CHROMA_KEY_CONTROL_REG 0x8184
#define SSTREAM_CONTROL_REG 0x8190
#define CHROMA_KEY_UPPER_BOUND_REG 0x8194
#define SSTREAM_STRETCH_REG 0x8198
#define BLEND_CONTROL_REG 0x81A0
#define PSTREAM_FBADDR0_REG 0x81C0
#define PSTREAM_FBADDR1_REG 0x81C4
#define PSTREAM_STRIDE_REG 0x81C8
#define DOUBLE_BUFFER_REG 0x81CC
#define SSTREAM_FBADDR0_REG 0x81D0
#define SSTREAM_FBADDR1_REG 0x81D4
#define SSTREAM_STRIDE_REG 0x81D8
#define OPAQUE_OVERLAY_CONTROL_REG 0x81DC
#define K1_VSCALE_REG 0x81E0
#define K2_VSCALE_REG 0x81E4
#define DDA_VERT_REG 0x81E8
#define STREAMS_FIFO_REG 0x81EC
#define PSTREAM_START_REG 0x81F0
#define PSTREAM_WINDOW_SIZE_REG 0x81F4
#define SSTREAM_START_REG 0x81F8
#define SSTREAM_WINDOW_SIZE_REG 0x81FC

#define S3_MEMBASE      sreg
#define S3_NEWMMIO_REGBASE      0x1000000  /* 16MB */
#define S3_NEWMMIO_REGSIZE        0x10000  /* 64KB */
#define S3V_MMIO_REGSIZE           0x8000  /* 32KB */
#define S3_NEWMMIO_VGABASE      (S3_NEWMMIO_REGBASE + 0x8000)

#define OUTREG(mmreg, value) *(unsigned int *)(&v->mmio[mmreg]) = value

int readcrtc(int reg) {
  outb(reg, 0x3d4);
  return inb(0x3d5);
}

void writecrtc(int reg, int value) {
  outb(reg, 0x3d4);
  outb(value, 0x3d5);   
}

// enable S3 registers
int enable(void) {
  int fd;

  if (v)
    return 1;
  errno = 0;
  v = malloc(sizeof(vga_t));
  if (v) {
    if (ioperm(0x3d4, 2, 1) == 0) {
      fd = open("/dev/mem", O_RDWR);
      if (fd != -1) {
        v->mmio = mmap(0, S3_NEWMMIO_REGSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd,
                 S3_MEMBASE + S3_NEWMMIO_REGBASE);
        close(fd);
        if (v->mmio != MAP_FAILED) {
          v->cr38 = readcrtc(0x38);
          v->cr39 = readcrtc(0x39);
          v->cr53 = readcrtc(0x53);
          writecrtc(0x38, 0x48);
          writecrtc(0x39, 0xa5);
          writecrtc(0x53, 0x08);
          return 1;
	}
      }
      iopl(0);
    }
    free(v);
    v = NULL;
  }
  return 0;
}

void disable(void) {
  if (v) {
    writecrtc(0x53, v->cr53);
    writecrtc(0x39, v->cr39);
    writecrtc(0x38, v->cr38);
    ioperm(0x3d4, 2, 0);
    munmap(v->mmio, S3_NEWMMIO_REGSIZE);
    free(v);
    v = NULL;
  }
}

int yuv_on(int format, int src_w, int src_h, int dst_x, int dst_y, int dst_w, int dst_h, int crop, int xres, int yres, int line_length, int offset) {
  int tmp, pitch, start, src_wc, src_hc, bpp;

  if (format == 0 || format == 7)
    bpp = 4;
  else if (format == 6)
    bpp = 3;
  else
    bpp = 2;

  src_wc = src_w - crop * 2;
  src_hc = src_h - crop * 2;
  pitch = src_w * bpp;
   
  // video card memory layout:
  // 0-n: visible screen memory, n = width * height * bytes per pixel
  // n-m: scaler source memory, n is aligned to a page boundary
  // m+: scaler source memory for multiple buffers

  // offset is the first aligned byte after the screen memory, where the scaler input buffer is
  tmp = (yres * line_length + 4095) & ~4095;
  offset += tmp;
   
  // start is the top left viewable scaler input pixel
  start = offset + crop * pitch + crop * bpp;
   
  OUTREG(COL_CHROMA_KEY_CONTROL_REG, 0x47000000);
  OUTREG(CHROMA_KEY_UPPER_BOUND_REG, 0x0);
  OUTREG(BLEND_CONTROL_REG, 0x00000020);
  OUTREG(DOUBLE_BUFFER_REG, 0x0); /* Choose fbaddr0 as stream source. */
  OUTREG(OPAQUE_OVERLAY_CONTROL_REG, 0x0);
   
  OUTREG(PSTREAM_CONTROL_REG, 0x06000000);
  OUTREG(PSTREAM_FBADDR0_REG, 0x0);
  OUTREG(PSTREAM_FBADDR1_REG, 0x0);
  OUTREG(PSTREAM_STRIDE_REG, line_length);
  OUTREG(PSTREAM_START_REG, 0x00010001);
  OUTREG(PSTREAM_WINDOW_SIZE_REG, 0x00010001);
  //OUTREG(SSTREAM_WINDOW_SIZE_REG, ( ((xres-1) << 16) | yres) & 0x7ff07ff);
   
  if (dst_w == src_w)
    tmp = 0;
  else
    tmp = 2;
  /* format 1=YCbCr-16 2=YUV-16 3=BGR15 4=YUV-16/32(mixed 2/4byte stride) 5=BGR16 6=BGR24 0,7=BGR32 */
  /* The YUV format pixel has a range of value from 0 to 255, while the YCbCr format pixel values are in the range of 16 to 240. */
  OUTREG(SSTREAM_CONTROL_REG, tmp << 28 | (format << 24) |
         ((((src_wc-1)<<1)-(dst_w-1)) & 0xfff));
  OUTREG(SSTREAM_STRETCH_REG,
         ((src_wc - 1) & 0x7ff) | (((src_wc - dst_w-1) & 0x7ff) << 16));
  OUTREG(SSTREAM_FBADDR0_REG, start & 0x3fffff );
  OUTREG(SSTREAM_STRIDE_REG, pitch & 0xfff );
  OUTREG(SSTREAM_START_REG, ((dst_x + 1) << 16) | (dst_y + 1));
  OUTREG(SSTREAM_WINDOW_SIZE_REG, ( ((dst_w-1) << 16) | (dst_h ) ) & 0x7ff07ff);
  OUTREG(K1_VSCALE_REG, src_hc - 1 );
  OUTREG(K2_VSCALE_REG, (src_hc - dst_h) & 0x7ff );
  /* 0xc000 = bw & vert interp */
  /* 0x8000 = no bw save */
  OUTREG(DDA_VERT_REG, (((~dst_h)-1) & 0xfff ) | 0xc000);
  writecrtc(0x92, (((pitch + 7) / 8) >> 8) | 0x80);
  writecrtc(0x93, (pitch + 7) / 8);
   
  writecrtc(0x67, readcrtc(0x67) | 0x4);
   
  return offset;
}

void yuv_off(void) {
  writecrtc(0x67, readcrtc(0x67) & ~0xc);
  memset(v->mmio + 0x8180, 0, 0x80);
  OUTREG(0x81b8, 0x900);
  OUTREG(0x81bc, 0x900);
  OUTREG(0x81c8, 0x900);
  OUTREG(0x81cc, 0x900);
  OUTREG(0x81d8, 0x1);
  OUTREG(0x81f8, 0x07ff07ff);
  OUTREG(0x81fc, 0x00010001);
  writecrtc(0x92, 0);
  writecrtc(0x93, 0);
}

static int preinit(const char *arg)
{
  char *name;

  if(arg)
    name = (char*)arg;
  else if(!(name = getenv("FRAMEBUFFER")))
    name = "/dev/fb0";

  if((fd = open(name, O_RDWR)) == -1) {
    mp_msg(MSGT_VO, MSGL_FATAL, "s3fb: can't open %s: %s\n", name, strerror(errno));
    return -1;
  }

  if(ioctl(fd, FBIOGET_FSCREENINFO, &fb_finfo)) {
    mp_msg(MSGT_VO, MSGL_FATAL, "s3fb: problem with FBITGET_FSCREENINFO ioctl: %s\n",
           strerror(errno));
    close(fd);
    fd = -1;
    return -1;
  }

  if(ioctl(fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
    mp_msg(MSGT_VO, MSGL_FATAL, "s3fb: problem with FBITGET_VSCREENINFO ioctl: %s\n",
           strerror(errno));
    close(fd);
    fd = -1;
    return -1;
  }

  // Check the depth now as config() musn't fail
  switch(fb_vinfo.bits_per_pixel) {
  case 16:
  case 24:
  case 32:
    break; // Ok
  default:
    mp_msg(MSGT_VO, MSGL_FATAL, "s3fb: %d bpp output is not supported\n", fb_vinfo.bits_per_pixel);
    close(fd);
    fd = -1;
    return -1;
  }

  /* Open up a window to the hardware */
  smem = mmap(0, fb_finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  sreg = fb_finfo.smem_start;

  if(smem == (void *)-1) {
    mp_msg(MSGT_VO, MSGL_FATAL, "s3fb: Couldn't map memory areas: %s\n", strerror(errno));
    smem = NULL;
    close(fd);
    fd = -1;
    return -1;
  }

  if (!enable()) {
    mp_msg(MSGT_VO, MSGL_FATAL, "s3fb: Couldn't map S3 registers: %s\n", strerror(errno));
    close(fd);
    fd = -1;
    return -1;
  }

  return 0; // Success
}

/* And close our mess */
static void uninit(void)
{
  if (inpage0) {
    clear_screen();
    yuv_off();
    inpage0 = NULL;
  }
   
  if(smem) {
    munmap(smem, fb_finfo.smem_len);
    smem = NULL;
  }
   
  disable();

  if(fd != -1) {
    close(fd);
    fd = -1;
  }
}

static void clear_screen(void)
{
  if (inpage0) {
    int n;
           
    memset(smem, 0, screenheight * screenstride);
           
    if (in_format == IMGFMT_YUY2) {
      unsigned short *ptr;
      int i;
                 
      ptr = (unsigned short *)inpage0;
      n = in_width * in_height;
      if (vo_doublebuffering)
        n *= 2;
      for(i=0; i<n; i++)
        *ptr++ = 0x8000;
                 
    } else {
      n = in_depth * in_width * in_height;
      if (vo_doublebuffering)
        n *= 2;
      memset(inpage0, 0, n);
    }
  }
}

/* Setup output screen dimensions etc */
static void setup_screen(uint32_t full)
{
  int inpageoffset;
   
  aspect(&vidwidth, &vidheight, full ? A_ZOOM : A_NOZOOM);

  // center picture
  vidx = (screenwidth - vidwidth) / 2;
  vidy = (screenheight - vidheight) / 2;

  geometry(&vidx, &vidy, &vidwidth, &vidheight, screenwidth, screenheight);
  vo_fs = full;
   
  inpageoffset = yuv_on(in_s3_format, in_width, in_height, vidx, vidy, vidwidth, vidheight, 0, screenwidth, screenheight, screenstride, 0);
  inpage0 = smem + inpageoffset;
  inpage = inpage0;
  mp_msg(MSGT_VO, MSGL_INFO, "s3fb: output is at %dx%d +%dx%d\n", vidx, vidy, vidwidth, vidheight);
   
  clear_screen();
}

static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height,
                  uint32_t flags, char *title, uint32_t format)
{
  screenwidth = fb_vinfo.xres;
  screenheight = fb_vinfo.yres;
  screenstride = fb_finfo.line_length;
  aspect_save_screenres(fb_vinfo.xres,fb_vinfo.yres);

  in_width = width;
  in_height = height;
  in_format = format;
  aspect_save_orig(width,height);

  aspect_save_prescale(d_width,d_height);

  /* Setup the screen for rendering to */
  screendepth = fb_vinfo.bits_per_pixel / 8;

  switch(in_format) {
        
  case IMGFMT_YUY2:
    in_depth = 2;
    in_s3_format = 1;
    alpha_func = vo_draw_alpha_yuy2;
    break;
           
  case IMGFMT_BGR15:
    in_depth = 2;
    in_s3_format = 3;
    alpha_func = vo_draw_alpha_rgb16;
    break;
           
  case IMGFMT_BGR16:
    in_depth = 2;
    in_s3_format = 5;
    alpha_func = vo_draw_alpha_rgb16;
    break;

  case IMGFMT_BGR24:
    in_depth = 3;
    in_s3_format = 6;
    alpha_func = vo_draw_alpha_rgb24;
    break;

  case IMGFMT_BGR32:
    in_depth = 4;
    in_s3_format = 7;
    alpha_func = vo_draw_alpha_rgb32;
    break;

  default:
    mp_msg(MSGT_VO, MSGL_FATAL, "s3fb: Eik! Something's wrong with control().\n");
    return -1;
  }
   
  offset = in_width * in_depth * in_height;
  if (vo_doublebuffering)
    page = offset;
  else
    page = 0;
   
  if(screenheight * screenstride + page + offset > fb_finfo.smem_len) {
    mp_msg(MSGT_VO, MSGL_FATAL, "s3fb: Not enough video memory to play this movie. Try at a lower resolution\n");
    return -1;
  }

  setup_screen(flags & VOFLAG_FULLSCREEN);
  if (vo_doublebuffering)
    inpage = inpage0 + page;
      
  mp_msg(MSGT_VO, MSGL_INFO, "s3fb: screen is %dx%d at %d bpp, in is %dx%d at %d bpp, norm is %dx%d\n",
         screenwidth, screenheight, screendepth * 8,
         in_width, in_height, in_depth * 8,
         d_width, d_height);

  return 0;
}

static void draw_alpha(int x, int y, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride)
{
  char *dst = inpage + (y * in_width + x) * in_depth;
  alpha_func(w, h, src, srca, stride, dst, in_width * in_depth);
}

static void draw_osd(void)
{
  if (!vo_doublebuffering)
    vo_draw_text(in_width, in_height, draw_alpha);
}

/* Render onto the screen */
static void flip_page(void)
{
  if(vo_doublebuffering) {
    vo_draw_text(in_width, in_height, draw_alpha);
    yuv_on(in_s3_format, in_width, in_height, vidx, vidy, vidwidth, vidheight, 0, screenwidth, screenheight, screenstride, page);
    page ^= offset;
    inpage = inpage0 + page;
  }
}

static int draw_frame(uint8_t *src[])
{
  mem2agpcpy(inpage, src[0], in_width * in_depth * in_height);
  return 0;
}

static int draw_slice(uint8_t *i[], int s[], int w, int h, int x, int y)
{
  return 1;
}

/* Attempt to start doing DR */
static uint32_t get_image(mp_image_t *mpi)
{

  if(mpi->flags & MP_IMGFLAG_READABLE)
    return VO_FALSE;
  if(mpi->type == MP_IMGTYPE_STATIC && vo_doublebuffering)
    return VO_FALSE;
  if(mpi->type > MP_IMGTYPE_TEMP)
    return VO_FALSE; // TODO ??

  switch(in_format) {
  case IMGFMT_BGR15:
  case IMGFMT_BGR16:
  case IMGFMT_BGR24:
  case IMGFMT_BGR32:
  case IMGFMT_YUY2:
    mpi->planes[0] = inpage;
    mpi->stride[0] = in_width * in_depth;
    break;

  default:
    return VO_FALSE;
  }

  mpi->width = in_width;
  mpi->flags |= MP_IMGFLAG_DIRECT;

  return VO_TRUE;
}

static int control(uint32_t request, void *data, ...)
{
  switch(request) {
  case VOCTRL_GET_IMAGE:
    return get_image(data);

  case VOCTRL_QUERY_FORMAT:
    switch(*((uint32_t*)data)) {
    case IMGFMT_BGR15:
    case IMGFMT_BGR16:
    case IMGFMT_BGR24:
    case IMGFMT_BGR32:
    case IMGFMT_YUY2:
      return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |
        VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
    }

    return 0;           /* Not supported */

  case VOCTRL_FULLSCREEN:
    setup_screen(!vo_fs);
    return 0;
  }

  return VO_NOTIMPL;
}

/* Dummy funcs */
static void check_events(void) {}
