/*
 * Video driver for Nintendo Wii/GameCube Framebuffer device
 *
 * Copyright (C) 2008 Jing Liu <fatersh-1@yahoo.com>
 *
 * Maintainer: Benjamin Zores <ben@geexbox.org>
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
 * This driver handles dedicated ATI GPU, which can be found in:
 *  - Nintendo GameCube (ATI LSI Flipper @ 162 MHz)
 *  - Nintendo Wii (ATI Hollywood @ 243 MHz)
 *
 * Flipper and Hollywood chipsets are pretty similar, except from clock speed:
 *  - Embedded framebuffer is 2MB.
 *  - Texture cache is 1MB.
 *  - Vertex cache is 0.1 MB.
 *  - Framebuffer is YUY2, not RGB.
 *  - Best resolution is 480p (854x480)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <linux/fb.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "sub.h"
#include "mp_msg.h"

static const vo_info_t info = {
  "Nintendo Wii/GameCube Framebuffer Device",
  "wii",
  "Jing Liu <fartersh-1@yahoo.com>",
  ""
};

LIBVO_EXTERN(wii)

static signed int pre_init_err = -2;

static char *fb_dev_name = NULL;

static FILE *vt_fp = NULL;
static int vt_doit = 1;

static int fb_dev_fd;
static int fb_tty_fd = -1;
static size_t fb_size;
static uint8_t *frame_buffer;
static uint8_t *center;
static struct fb_var_screeninfo fb_orig_vinfo;
static struct fb_var_screeninfo fb_vinfo;
static int fb_pixel_size;       // 32:  4  24:  3  16:  2  15:  2
static int fb_line_len;
static int in_width;
static int in_height;
static int out_width;
static int out_height;
static int fs;

static int fb_preinit(int reset)
{
  static int fb_preinit_done = 0;
  static int fb_works = 0;

  if (reset) {
    fb_preinit_done = 0;
    return 0;
  }

  if (fb_preinit_done)
    return fb_works;

  if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER")))
    fb_dev_name = strdup("/dev/fb0");
  mp_msg(MSGT_VO, MSGL_V, "using %s\n", fb_dev_name);

  if ((fb_dev_fd = open(fb_dev_name, O_RDWR)) == -1) {
    mp_msg(MSGT_VO, MSGL_ERR, "Can't open %s: %s\n", fb_dev_name, strerror(errno));
    goto err_out;
  }
  if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
    mp_msg(MSGT_VO, MSGL_ERR, "Can't get VSCREENINFO: %s\n", strerror(errno));
    goto err_out_fd;
  }
  fb_orig_vinfo = fb_vinfo;

  if ((fb_tty_fd = open("/dev/tty", O_RDWR)) < 0) {
    mp_msg(MSGT_VO, MSGL_ERR, "notice: Can't open /dev/tty: %s\n", strerror(errno));
    goto err_out_fd;
  }

  fb_preinit_done = 1;
  fb_works = 1;
  return 1;

 err_out_fd:
  close(fb_dev_fd);
  fb_dev_fd = -1;
 err_out:
  fb_preinit_done = 1;
  fb_works = 0;

  return 0;
}

static void vt_set_textarea(int u, int l)
{
  /* how can I determine the font height?
   * just use 16 for now
   */
  int urow = ((u + 15) / 16) + 1;
  int lrow = l / 16;

  mp_msg(MSGT_VO, MSGL_DBG2, "vt_set_textarea(%d, %d): %d,%d\n", u, l, urow, lrow);

  if (vt_fp) {
    fprintf(vt_fp, "\33[%d;%dr\33[%d;%dH", urow, lrow, lrow, 0);
    fflush(vt_fp);
  }
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,
                  uint32_t d_height, uint32_t flags, char *title,
                  uint32_t format)
{
  struct fb_fix_screeninfo fb_finfo;
  uint32_t black = 0x00800080;
  long temp;
  int vt_fd;

  fs = flags & VOFLAG_FULLSCREEN;

  if (pre_init_err == -2) {
    mp_msg(MSGT_VO, MSGL_ERR, "Internal fatal error: config() was called before preinit()\n");
    return -1;
  }

  if (pre_init_err)
    return 1;

  in_width  = width;
  in_height = height;

  out_width  = (d_width && fs) ? d_width  : width;
  out_height = (d_width && fs) ? d_height : height;

  fb_vinfo.xres_virtual = fb_vinfo.xres;
  fb_vinfo.yres_virtual = fb_vinfo.yres;

  if (fb_tty_fd >= 0 && ioctl(fb_tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
    mp_msg(MSGT_VO, MSGL_V, "Can't set graphics mode: %s\n", strerror(errno));
    close(fb_tty_fd);
    fb_tty_fd = -1;
  }

  if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_vinfo)) {
    mp_msg(MSGT_VO, MSGL_ERR, "Can't put VSCREENINFO: %s\n", strerror(errno));
    if (fb_tty_fd >= 0 && ioctl(fb_tty_fd, KDSETMODE, KD_TEXT) < 0) {
      mp_msg(MSGT_VO, MSGL_ERR, "Can't restore text mode: %s\n", strerror(errno));
    }
    return 1;
  }

  fb_pixel_size = 2;

  if (fs) {
    out_width  = fb_vinfo.xres;
    out_height = fb_vinfo.yres;
  }
  if (out_width < in_width || out_height < in_height) {
    mp_msg(MSGT_VO, MSGL_ERR, "screensize is smaller than video size\n");
    return 1;
  }

  if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fb_finfo)) {
    mp_msg(MSGT_VO, MSGL_ERR, "Can't get FSCREENINFO: %s\n", strerror(errno));
    return 1;
  }

  if (fb_finfo.type != FB_TYPE_PACKED_PIXELS) {
    mp_msg(MSGT_VO, MSGL_ERR, "type %d not supported\n", fb_finfo.type);
    return 1;
  }

  fb_line_len = fb_finfo.line_length;
  fb_size     = fb_finfo.smem_len;

  frame_buffer = (uint8_t *) mmap(0, fb_size, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fb_dev_fd, 0);
  if (frame_buffer == (uint8_t *) -1) {
    mp_msg(MSGT_VO, MSGL_ERR, "Can't mmap %s: %s\n", fb_dev_name, strerror(errno));
    return 1;
  }

  center = frame_buffer +
    ((out_width - in_width) / 2) * fb_pixel_size +
    ((out_height - in_height) / 2) * fb_line_len;

  mp_msg(MSGT_VO, MSGL_DBG2, "frame_buffer @ %p\n", frame_buffer);
  mp_msg(MSGT_VO, MSGL_DBG2, "center @ %p\n", center);
  mp_msg(MSGT_VO, MSGL_V, "pixel per line: %d\n", fb_line_len / fb_pixel_size);

  /* blanking screen */
  for (temp = 0; temp < fb_size; temp += 4)
    memcpy(frame_buffer + temp, (void *) &black, 4);

  if (vt_doit && (vt_fd = open("/dev/tty", O_WRONLY)) == -1) {
    mp_msg(MSGT_VO, MSGL_ERR, "can't open /dev/tty: %s\n", strerror(errno));
    vt_doit = 0;
  }
  if (vt_doit && !(vt_fp = fdopen(vt_fd, "w"))) {
    mp_msg(MSGT_VO, MSGL_ERR, "can't fdopen /dev/tty: %s\n", strerror(errno));
    vt_doit = 0;
  }

  if (vt_doit)
    vt_set_textarea((out_height + in_height) / 2, fb_vinfo.yres);

  return 0;
}

static int query_format(uint32_t format)
{
  if (!fb_preinit(0))
    return 0;

  if (format != IMGFMT_YUY2)
    return 0;

  return VFCAP_ACCEPT_STRIDE | VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride)
{
  unsigned char *dst;

  dst = center + fb_line_len * y0 + fb_pixel_size * x0;

  vo_draw_alpha_yuy2(w, h, src, srca, stride, dst, fb_line_len);
}

static int draw_frame(uint8_t *src[])
{
  return 1;
}

static int draw_slice(uint8_t *src[], int stride[], int w, int h, int x, int y)
{
  uint8_t *d, *s;

  d = center + fb_line_len * y + fb_pixel_size * x;

  s = src[0];
  while (h) {
    memcpy(d, s, w * fb_pixel_size);
    d += fb_line_len;
    s += stride[0];
    h--;
  }

  return 0;
}

static void check_events(void)
{
}

static void flip_page(void)
{
}

static void draw_osd(void)
{
  vo_draw_text(in_width, in_height, draw_alpha);
}

static void uninit(void)
{
  if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &fb_vinfo))
    mp_msg(MSGT_VO, MSGL_WARN, "ioctl FBIOGET_VSCREENINFO: %s\n", strerror(errno));
  fb_orig_vinfo.xoffset = fb_vinfo.xoffset;
  fb_orig_vinfo.yoffset = fb_vinfo.yoffset;
  if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &fb_orig_vinfo))
    mp_msg(MSGT_VO, MSGL_WARN, "Can't reset original fb_var_screeninfo: %s\n", strerror(errno));
  if (fb_tty_fd >= 0) {
    if (ioctl(fb_tty_fd, KDSETMODE, KD_TEXT) < 0)
      mp_msg(MSGT_VO, MSGL_WARN, "Can't restore text mode: %s\n", strerror(errno));
  }
  if (vt_doit)
    vt_set_textarea(0, fb_orig_vinfo.yres);
  close(fb_tty_fd);
  close(fb_dev_fd);
  if (frame_buffer)
    munmap(frame_buffer, fb_size);
  frame_buffer = NULL;
  fb_preinit(1);
}

static int preinit(const char *vo_subdevice)
{
  pre_init_err = 0;

  if (!pre_init_err)
    return pre_init_err = (fb_preinit(0) ? 0 : -1);
  return -1;
}

static uint32_t get_image(mp_image_t *mpi)
{
  if (((mpi->type != MP_IMGTYPE_STATIC) && (mpi->type != MP_IMGTYPE_TEMP)) ||
      (mpi->flags & MP_IMGFLAG_PLANAR) ||
      (mpi->flags & MP_IMGFLAG_YUV) ||
      (mpi->width != in_width) ||
      (mpi->height != in_height)
     )
    return VO_FALSE;

  mpi->planes[0] = center;
  mpi->stride[0] = fb_line_len;
  mpi->flags |= MP_IMGFLAG_DIRECT;

  return VO_TRUE;
}

static int control(uint32_t request, void *data, ...)
{
  if (request == VOCTRL_GET_IMAGE)
    return get_image(data);
  else if (request == VOCTRL_QUERY_FORMAT)
    return query_format(*((uint32_t*) data));

  return VO_NOTIMPL;
}
