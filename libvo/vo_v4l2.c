/*
 * video output for V4L2 hardware MPEG decoders
 *
 * Copyright (C) 2007 Benjamin Zores
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/ioctl.h>

#include "mp_msg.h"
#include "subopt-helper.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "libmpdemux/mpeg_packetizer.h"

#define DEFAULT_MPEG_DECODER "/dev/video16"
#define V4L2_VO_HDR "VO: [v4l2]"

int v4l2_fd = -1;
static vo_mpegpes_t *pes;

/* suboptions */
static int output = -1;
static char *device = NULL;

static opt_t subopts[] = {
  {"output",   OPT_ARG_INT,       &output,       (opt_test_f)int_non_neg},
  {"device",   OPT_ARG_MSTRZ,     &device,       NULL},
  {NULL}
};

static const vo_info_t info = 
{
  "V4L2 MPEG Video Decoder Output",
  "v4l2",
  "Benjamin Zores",
  ""
};
const LIBVO_EXTERN (v4l2)

int
v4l2_write (unsigned char *data, int len)
{
  if (v4l2_fd < 0)
    return 0;
  
  return write (v4l2_fd, data, len);
}

/* video out functions */

static int
config (uint32_t width, uint32_t height,
        uint32_t d_width, uint32_t d_height,
        uint32_t fullscreen, char *title, uint32_t format)
{
  return 0;
}

static int
preinit (const char *arg)
{
  struct v4l2_output vout;
  struct v4l2_ext_controls ctrls;
  int err;

  if (subopt_parse (arg, subopts) != 0)
  {
    mp_msg (MSGT_VO, MSGL_FATAL,
            "\n-vo v4l2 command line help:\n"
            "Example: mplayer -vo v4l2:device=/dev/video16:output=2\n"
            "\nOptions:\n"
            "  device=/dev/videoX\n"
            "    Name of the MPEG decoder device file.\n"
            "  output=<0-...>\n"
            "    V4L2 id of the TV output.\n"
            "\n" );
    return -1;
  }
  
  if (!device)
    device = strdup (DEFAULT_MPEG_DECODER);    
  
  v4l2_fd = open (device, O_RDWR);
  if (v4l2_fd < 0)
  {  
    free (device);
    mp_msg (MSGT_VO, MSGL_FATAL, "%s %s\n", V4L2_VO_HDR, strerror (errno));
    return -1;
  }

  /* check for device hardware MPEG decoding capability */
  ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG; 
  ctrls.count = 0; 
  ctrls.controls = NULL;
  
  if (ioctl (v4l2_fd, VIDIOC_G_EXT_CTRLS, &ctrls) < 0)
  {
    free (device);
    mp_msg (MSGT_OPEN, MSGL_FATAL, "%s %s\n", V4L2_VO_HDR, strerror (errno));
    return -1;
  }
  
  /* list available outputs */
  vout.index = 0;
  err = 1;
  mp_msg (MSGT_VO, MSGL_INFO, "%s Available video outputs: ", V4L2_VO_HDR);
  while (ioctl (v4l2_fd, VIDIOC_ENUMOUTPUT, &vout) >= 0)
  {
    err = 0;
    mp_msg (MSGT_VO, MSGL_INFO, "'#%d, %s' ", vout.index, vout.name);
    vout.index++;
  }
  if (err)
  {
    mp_msg (MSGT_VO, MSGL_INFO, "none\n");
    free (device);
    return -1;
  }
  else
    mp_msg (MSGT_VO, MSGL_INFO, "\n");

  /* set user specified output */
  if (output != -1)
  {
    if (ioctl (v4l2_fd, VIDIOC_S_OUTPUT, &output) < 0)
    {
      mp_msg (MSGT_VO, MSGL_ERR,
              "%s can't set output (%s)\n", V4L2_VO_HDR, strerror (errno));
      free (device);
      return -1;
    }
  }

  /* display device name */
  mp_msg (MSGT_VO, MSGL_INFO, "%s using %s\n", V4L2_VO_HDR, device);
  free (device);

  /* display current video output */
  if (ioctl (v4l2_fd, VIDIOC_G_OUTPUT, &output) == 0)
  {
    vout.index = output;
    if (ioctl (v4l2_fd, VIDIOC_ENUMOUTPUT, &vout) < 0)
    {
      mp_msg (MSGT_VO, MSGL_ERR,
              "%s can't get output (%s).\n", V4L2_VO_HDR, strerror (errno));
      return -1;
    }
    else
      mp_msg (MSGT_VO, MSGL_INFO,
              "%s video output: %s\n", V4L2_VO_HDR, vout.name);
  }
  else
  {
    mp_msg (MSGT_VO, MSGL_ERR,
            "%s can't get output (%s).\n", V4L2_VO_HDR, strerror (errno));
    return -1;
  }
  
  return 0;
}

static void
draw_osd (void)
{
  /* do nothing */
}

static int
draw_frame (uint8_t * src[])
{
  pes = (vo_mpegpes_t *) src[0];
  return 0;
}

static void
flip_page (void)
{
  if (v4l2_fd < 0)
    return;

  if (!pes)
    return;

  send_mpeg_pes_packet (pes->data, pes->size, pes->id,
                        pes->timestamp ? pes->timestamp : vo_pts, 2,
                        v4l2_write);

  /* ensure flip_page() won't be called twice */
  pes = NULL;
}

static int
draw_slice (uint8_t *image[], int stride[], int w, int h, int x, int y)
{
  return 0;
}

static void
uninit (void)
{
  if (v4l2_fd < 0)
    return;

  /* close device */
  close (v4l2_fd);
  v4l2_fd = -1;
}

static void
check_events (void)
{
  /* do nothing */
}

static int
query_format (uint32_t format)
{
  if (format != IMGFMT_MPEGPES)
    return 0;
    
  return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_TIMER;
}

static int
control (uint32_t request, void *data, ...)
{
  switch (request)
  {
  case VOCTRL_QUERY_FORMAT:
    return query_format (*((uint32_t*) data));
  }
  
  return VO_NOTIMPL;
}
