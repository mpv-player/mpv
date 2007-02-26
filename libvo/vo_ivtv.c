/*
 *  Copyright (C) 2006 Benjamin Zores
 *   Video output for WinTV PVR-150/250/350 (a.k.a IVTV) cards.
 *   TV-Out through hardware MPEG decoder.
 *   Based on some old code from ivtv driver authors.
 *   See http://ivtvdriver.org/index.php/Main_Page for more details on the
 *    cards supported by the ivtv driver.
 *
 *   This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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
#include <linux/ivtv.h>
#include <linux/ioctl.h>

#include "mp_msg.h"
#include "subopt-helper.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "libmpdemux/mpeg_packetizer.h"

#define DEFAULT_MPEG_DECODER "/dev/video16"
#define IVTV_VO_HDR "VO: [ivtv]"

/* ivtv private */
int ivtv_fd = -1;
static vo_mpegpes_t *pes;

/* suboptions */
static int output = -1;
static char *device = NULL;

static opt_t subopts[] = {
  {"output",   OPT_ARG_INT,       &output,       (opt_test_f)int_non_neg},
  {"device",   OPT_ARG_MSTRZ,     &device,       NULL},
  {NULL}
};

static vo_info_t info = 
{
  "IVTV MPEG Video Decoder TV-Out",
  "ivtv",
  "Benjamin Zores",
  ""
};
LIBVO_EXTERN (ivtv)

/* ivtv internals */

static uint32_t
ivtv_reset (int blank_screen)
{
  struct ivtv_cfg_stop_decode sd;
  struct ivtv_cfg_start_decode sd1;
  int flags = 0;

  if (blank_screen)
    flags |= IVTV_STOP_FL_HIDE_FRAME;
  sd.flags = flags;
 
  if (ioctl (ivtv_fd, IVTV_IOC_STOP_DECODE, &sd) < 0)
  {
    mp_msg (MSGT_VO, MSGL_ERR,
            "IVTV_IOC_STOP_DECODE: %s\n", strerror (errno));
    return 1;
  }
  
  sd1.gop_offset = 0;
  sd1.muted_audio_frames = 0;
  
  if (ioctl (ivtv_fd, IVTV_IOC_START_DECODE, &sd1) < 0)
  {
    mp_msg (MSGT_VO, MSGL_ERR,
            "IVTV_IOC_START_DECODE: %s\n", strerror (errno));
    return 1;
  }

  return 0;
}

int
ivtv_write (unsigned char *data, int len)
{
  if (ivtv_fd < 0)
    return 0;
  
  return write (ivtv_fd, data, len);
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
  int err;

  if (subopt_parse (arg, subopts) != 0)
  {
    mp_msg (MSGT_VO, MSGL_FATAL,
            "\n-vo ivtv command line help:\n"
            "Example: mplayer -vo ivtv:device=/dev/video16:output=2\n"
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
  
  ivtv_fd = open (device, O_RDWR);
  if (ivtv_fd < 0)
  {  
    free (device);
    mp_msg (MSGT_VO, MSGL_FATAL, "%s %s\n", IVTV_VO_HDR, strerror (errno));
    return -1;
  }

  /* list available outputs */
  vout.index = 0;
  err = 1;
  mp_msg (MSGT_VO, MSGL_INFO, "%s Available video outputs: ", IVTV_VO_HDR);
  while (ioctl (ivtv_fd, VIDIOC_ENUMOUTPUT, &vout) >= 0)
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
    if (ioctl (ivtv_fd, VIDIOC_S_OUTPUT, &output) < 0)
    {
      mp_msg (MSGT_VO, MSGL_ERR,
              "%s can't set output (%s)\n", IVTV_VO_HDR, strerror (errno));
      free (device);
      return -1;
    }
  }

  /* display device name */
  mp_msg (MSGT_VO, MSGL_INFO, "%s using %s\n", IVTV_VO_HDR, device);
  free (device);

  /* display current video output */
  if (ioctl (ivtv_fd, VIDIOC_G_OUTPUT, &output) == 0)
  {
    vout.index = output;
    if (ioctl (ivtv_fd, VIDIOC_ENUMOUTPUT, &vout) < 0)
    {
      mp_msg (MSGT_VO, MSGL_ERR,
              "%s can't get output (%s).\n", IVTV_VO_HDR, strerror (errno));
      return -1;
    }
    else
      mp_msg (MSGT_VO, MSGL_INFO,
              "%s video output: %s\n", IVTV_VO_HDR, vout.name);
  }
  else
  {
    mp_msg (MSGT_VO, MSGL_ERR,
            "%s can't get output (%s).\n", IVTV_VO_HDR, strerror (errno));
    return -1;
  }
  
  /* clear output */
  ivtv_reset (1);

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
  if (ivtv_fd < 0)
    return;

  if (!pes)
    return;

  send_mpeg_pes_packet (pes->data, pes->size, pes->id,
                         pes->timestamp ? pes->timestamp : vo_pts, 2,
                         ivtv_write);

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
  if (ivtv_fd < 0)
    return;

  /* clear output */
  ivtv_reset (1);

  /* close device */
  close (ivtv_fd);
  ivtv_fd = -1;
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
  case VOCTRL_PAUSE: 
  case VOCTRL_RESUME: 
    return ivtv_reset (0);

  case VOCTRL_RESET: 
    return ivtv_reset (1);

  case VOCTRL_QUERY_FORMAT:
    return query_format (*((uint32_t*) data));
  }
  
  return VO_NOTIMPL;
}
