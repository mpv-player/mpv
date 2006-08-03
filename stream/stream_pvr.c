/*
 *  Copyright (C) 2006 Benjamin Zores
 *   Stream layer for WinTV PVR-150/250/350 (a.k.a IVTV) PVR cards.
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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <inttypes.h>
#include <sys/poll.h>
#include <linux/videodev2.h>
#include <linux/ivtv.h>

#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "tv.h"

#define PVR_DEFAULT_DEVICE "/dev/video0"

/* logging mechanisms */
#define LOG_LEVEL_PVR  "[pvr]"
#define LOG_LEVEL_V4L2 "[v4l2]"
#define LOG_LEVEL_IVTV "[ivtv]"

/* IVTV driver settings (see http://ivtvdriver.org/index.php/Ivtvctl ) */

/* codec aspect ratio (1:1, 4:3, 16:9, 2.21:1) */
#define PVR_ASPECT_RATIO_1_1                                   1
#define PVR_ASPECT_RATIO_4_3                                   2
#define PVR_ASPECT_RATIO_16_9                                  3
#define PVR_ASPECT_RATIO_2_21_1                                4

/* audio codec sample rate (32KHz, CD 44.1 KHz, AC97 48 KHz) */
#define PVR_AUDIO_SAMPLE_RATE_44_1_KHZ                         0x0000
#define PVR_AUDIO_SAMPLE_RATE_48_KHZ                           0x0001
#define PVR_AUDIO_SAMPLE_RATE_32_KHZ                           0x0002

/* audio codec layer (1 or 2) */
#define PVR_AUDIO_LAYER_1                                      0x0004
#define PVR_AUDIO_LAYER_2                                      0x0008

/* audio codec bitrate */
#define PVR_AUDIO_BITRATE_32                                   0x0010
#define PVR_AUDIO_BITRATE_L1_64                                0x0020
#define PVR_AUDIO_BITRATE_L1_96                                0x0030
#define PVR_AUDIO_BITRATE_L1_128                               0x0040
#define PVR_AUDIO_BITRATE_L1_160                               0x0050
#define PVR_AUDIO_BITRATE_L1_192                               0x0060
#define PVR_AUDIO_BITRATE_L1_224                               0x0070
#define PVR_AUDIO_BITRATE_L1_256                               0x0080
#define PVR_AUDIO_BITRATE_L1_288                               0x0090
#define PVR_AUDIO_BITRATE_L1_320                               0x00A0
#define PVR_AUDIO_BITRATE_L1_352                               0x00B0
#define PVR_AUDIO_BITRATE_L1_384                               0x00C0
#define PVR_AUDIO_BITRATE_L1_416                               0x00D0
#define PVR_AUDIO_BITRATE_L1_448                               0x00E0
#define PVR_AUDIO_BITRATE_L2_48                                0x0020
#define PVR_AUDIO_BITRATE_L2_56                                0x0030
#define PVR_AUDIO_BITRATE_L2_64                                0x0040
#define PVR_AUDIO_BITRATE_L2_80                                0x0050
#define PVR_AUDIO_BITRATE_L2_96                                0x0060
#define PVR_AUDIO_BITRATE_L2_112                               0x0070
#define PVR_AUDIO_BITRATE_L2_128                               0x0080
#define PVR_AUDIO_BITRATE_L2_160                               0x0090
#define PVR_AUDIO_BITRATE_L2_192                               0x00A0
#define PVR_AUDIO_BITRATE_L2_224                               0x00B0
#define PVR_AUDIO_BITRATE_L2_256                               0x00C0
#define PVR_AUDIO_BITRATE_L2_320                               0x00D0
#define PVR_AUDIO_BITRATE_L2_384                               0x00E0

/* audio codec mode */
#define PVR_AUDIO_MODE_ARG_STEREO                              "stereo"
#define PVR_AUDIO_MODE_ARG_JOINT_STEREO                        "joint_stereo"
#define PVR_AUDIO_MODE_ARG_DUAL                                "dual"
#define PVR_AUDIO_MODE_ARG_MONO                                "mono"
#define PVR_AUDIO_MODE_STEREO                                  0x0000
#define PVR_AUDIO_MODE_JOINT_STEREO                            0x0100
#define PVR_AUDIO_MODE_DUAL                                    0x0200
#define PVR_AUDIO_MODE_MONO                                    0x0300

/* video codec bitrate mode */
#define PVR_VIDEO_BITRATE_MODE_ARG_VBR                         "vbr"
#define PVR_VIDEO_BITRATE_MODE_ARG_CBR                         "cbr"
#define PVR_VIDEO_BITRATE_MODE_VBR                             0
#define PVR_VIDEO_BITRATE_MODE_CBR                             1

/* video codec stream type */
#define PVR_VIDEO_STREAM_TYPE_PS                               "ps"
#define PVR_VIDEO_STREAM_TYPE_TS                               "ts"
#define PVR_VIDEO_STREAM_TYPE_MPEG1                            "mpeg1"
#define PVR_VIDEO_STREAM_TYPE_DVD                              "dvd"
#define PVR_VIDEO_STREAM_TYPE_VCD                              "vcd"
#define PVR_VIDEO_STREAM_TYPE_SVCD                             "svcd"
#define PVR_VIDEO_STREAM_TYPE_DVD_S1                           "dvds1"
#define PVR_VIDEO_STREAM_TYPE_DVD_S2                           "dvds2"

/* command line arguments */
int pvr_param_aspect_ratio = 0;
int pvr_param_sample_rate = 0;
int pvr_param_audio_layer = 0;
int pvr_param_audio_bitrate = 0;
char *pvr_param_audio_mode = NULL;
int pvr_param_bitrate = 0;
char *pvr_param_bitrate_mode = NULL;
int pvr_param_bitrate_peak = 0;
char *pvr_param_stream_type = NULL;

struct pvr_t {
  int dev_fd;
  char *video_dev;

  /* v4l2 params */
  int mute;
  int input;
  int normid;
  int brightness;
  int contrast;
  int hue;
  int saturation;
  int width;
  int height;
  char *freq;

  /* ivtv params */
  int aspect;
  int samplerate;
  int layer;
  int audio_rate;
  int audio_mode;
  int bitrate;
  int bitrate_mode;
  int bitrate_peak;
  int stream_type;
};

static struct pvr_t *
pvr_init (void)
{
  struct pvr_t *pvr = NULL;

  pvr = malloc (sizeof (struct pvr_t)); 
  pvr->dev_fd = -1;
  pvr->video_dev = strdup (PVR_DEFAULT_DEVICE);

  /* v4l2 params */
  pvr->mute = 0;
  pvr->input = 0;
  pvr->normid = -1;
  pvr->brightness = 0;
  pvr->contrast = 0;
  pvr->hue = 0;
  pvr->saturation = 0;
  pvr->width = -1;
  pvr->height = -1;
  pvr->freq = NULL;

  /* ivtv params */
  pvr->aspect = -1;
  pvr->samplerate = -1;
  pvr->layer = -1;
  pvr->audio_rate = -1;
  pvr->audio_mode = -1;
  pvr->bitrate = -1;
  pvr->bitrate_mode = -1;
  pvr->bitrate_peak = -1;
  pvr->stream_type = -1;
  
  return pvr;
}

static void
pvr_uninit (struct pvr_t *pvr)
{
  if (!pvr)
    return;

  /* close device */
  if (pvr->dev_fd)
    close (pvr->dev_fd);
  
  if (pvr->video_dev)
    free (pvr->video_dev);
  if (pvr->freq)
    free (pvr->freq);
  free (pvr);
}

/* IVTV layer */

static void
parse_ivtv_options (struct pvr_t *pvr)
{
  if (!pvr)
    return;

  /* -pvr aspect=digit */
  if (pvr_param_aspect_ratio >= 1 && pvr_param_aspect_ratio <= 4)
    pvr->aspect = pvr_param_aspect_ratio;

  /* -pvr arate=x */
  if (pvr_param_sample_rate != 0)
  {
    switch (pvr_param_sample_rate)
    {
    case 32000:
      pvr->samplerate = PVR_AUDIO_SAMPLE_RATE_32_KHZ;
      break;
    case 44100:
      pvr->samplerate = PVR_AUDIO_SAMPLE_RATE_44_1_KHZ;
      break;
    case 48000:
      pvr->samplerate = PVR_AUDIO_SAMPLE_RATE_48_KHZ;
      break;
    default:
      break;
    }
  }

  /* -pvr alayer=x */
  if (pvr_param_audio_layer == 1)
    pvr->layer = PVR_AUDIO_LAYER_1;
  else if (pvr_param_audio_layer == 2)
    pvr->layer = PVR_AUDIO_LAYER_2;

  /* -pvr abitrate=x */
  if (pvr_param_audio_bitrate != 0)
  {
    /* set according to layer or use layer 1 by default if not specified */
    switch (pvr_param_audio_bitrate)
    {
    case 32:
      pvr->audio_rate = PVR_AUDIO_BITRATE_32;
      break;
    case 48:
      pvr->audio_rate = PVR_AUDIO_BITRATE_L2_48;
      break;
    case 56:
      pvr->audio_rate = PVR_AUDIO_BITRATE_L2_56;
      break;
    case 64:
      pvr->audio_rate = (pvr_param_audio_layer == 2) ?
        PVR_AUDIO_BITRATE_L2_64 : PVR_AUDIO_BITRATE_L1_64;
      break;
    case 80:
      pvr->audio_rate = PVR_AUDIO_BITRATE_L2_80;
      break;
    case 96:
      pvr->audio_rate = (pvr_param_audio_layer == 2) ?
        PVR_AUDIO_BITRATE_L2_96 : PVR_AUDIO_BITRATE_L1_96;
      break;
    case 112:
      pvr->audio_rate = PVR_AUDIO_BITRATE_L2_112;
      break;
    case 128:
      pvr->audio_rate = (pvr_param_audio_layer == 2) ?
        PVR_AUDIO_BITRATE_L2_128 : PVR_AUDIO_BITRATE_L1_128;
      break;
    case 160:
      pvr->audio_rate = (pvr_param_audio_layer == 2) ?
        PVR_AUDIO_BITRATE_L2_160 : PVR_AUDIO_BITRATE_L1_160;
      break;
    case 192:
      pvr->audio_rate = (pvr_param_audio_layer == 2) ?
        PVR_AUDIO_BITRATE_L2_192 : PVR_AUDIO_BITRATE_L1_192;
      break;
    case 224:
      pvr->audio_rate = (pvr_param_audio_layer == 2) ?
        PVR_AUDIO_BITRATE_L2_224 : PVR_AUDIO_BITRATE_L1_224;
      break;
    case 256:
      pvr->audio_rate = (pvr_param_audio_layer == 2) ?
        PVR_AUDIO_BITRATE_L2_256 : PVR_AUDIO_BITRATE_L1_256;
      break;
    case 288:
      pvr->audio_rate = PVR_AUDIO_BITRATE_L1_288;
      break;
    case 320:
      pvr->audio_rate = (pvr_param_audio_layer == 2) ?
        PVR_AUDIO_BITRATE_L2_320 : PVR_AUDIO_BITRATE_L1_320;
      break;
    case 352:
      pvr->audio_rate = PVR_AUDIO_BITRATE_L1_352;
      break;
    case 384:
      pvr->audio_rate = (pvr_param_audio_layer == 2) ?
        PVR_AUDIO_BITRATE_L2_384 : PVR_AUDIO_BITRATE_L1_384;
      break;
    case 416:
      pvr->audio_rate = PVR_AUDIO_BITRATE_L1_416;
      break;
    case 448:
      pvr->audio_rate = PVR_AUDIO_BITRATE_L1_448;
      break;
    default:
      break;
    }
  }
  
  /* -pvr amode=x */
  if (pvr_param_audio_mode)
  {
    if (!strcmp (pvr_param_audio_mode, PVR_AUDIO_MODE_ARG_STEREO))
      pvr->audio_mode = PVR_AUDIO_MODE_STEREO;
    else if (!strcmp (pvr_param_audio_mode, PVR_AUDIO_MODE_ARG_JOINT_STEREO))
      pvr->audio_mode = PVR_AUDIO_MODE_JOINT_STEREO;
    else if (!strcmp (pvr_param_audio_mode, PVR_AUDIO_MODE_ARG_DUAL))
      pvr->audio_mode = PVR_AUDIO_MODE_DUAL;
    else if (!strcmp (pvr_param_audio_mode, PVR_AUDIO_MODE_ARG_MONO))
      pvr->audio_mode = PVR_AUDIO_MODE_MONO;
    else /* for anything else, set to stereo */
      pvr->audio_mode = PVR_AUDIO_MODE_STEREO;
  }

  /* -pvr vbitrate=x */
  if (pvr_param_bitrate)
    pvr->bitrate = pvr_param_bitrate;

  /* -pvr vmode=x */
  if (pvr_param_bitrate_mode)
  {
    if (!strcmp (pvr_param_bitrate_mode, PVR_VIDEO_BITRATE_MODE_ARG_VBR))
      pvr->bitrate_mode = PVR_VIDEO_BITRATE_MODE_VBR;
    else if (!strcmp (pvr_param_bitrate_mode, PVR_VIDEO_BITRATE_MODE_ARG_CBR))
      pvr->bitrate_mode = PVR_VIDEO_BITRATE_MODE_CBR;
    else /* for anything else, set to VBR */
      pvr->bitrate_mode = PVR_VIDEO_BITRATE_MODE_VBR;
  }

  /* -pvr vpeak=x */
  if (pvr_param_bitrate_peak)
    pvr->bitrate_peak = pvr_param_bitrate_peak;

  /* -pvr fmt=x */
  if (pvr_param_stream_type)
  {
    if (!strcmp (pvr_param_stream_type, PVR_VIDEO_STREAM_TYPE_PS))
      pvr->stream_type = IVTV_STREAM_PS;
    else if (!strcmp (pvr_param_stream_type, PVR_VIDEO_STREAM_TYPE_TS))
      pvr->stream_type = IVTV_STREAM_TS;
    else if (!strcmp (pvr_param_stream_type, PVR_VIDEO_STREAM_TYPE_MPEG1))
      pvr->stream_type = IVTV_STREAM_MPEG1;
    else if (!strcmp (pvr_param_stream_type, PVR_VIDEO_STREAM_TYPE_DVD))
      pvr->stream_type = IVTV_STREAM_DVD;
    else if (!strcmp (pvr_param_stream_type, PVR_VIDEO_STREAM_TYPE_VCD))
      pvr->stream_type = IVTV_STREAM_VCD;
    else if (!strcmp (pvr_param_stream_type, PVR_VIDEO_STREAM_TYPE_SVCD))
      pvr->stream_type = IVTV_STREAM_SVCD;
    else if (!strcmp (pvr_param_stream_type, PVR_VIDEO_STREAM_TYPE_DVD_S1))
      pvr->stream_type = IVTV_STREAM_DVD_S1;
    else if (!strcmp (pvr_param_stream_type, PVR_VIDEO_STREAM_TYPE_DVD_S2))
      pvr->stream_type = IVTV_STREAM_DVD_S2;
    else /* for anything else, set to MPEG PS */
      pvr->stream_type = IVTV_STREAM_PS;
  }
}

static int
set_ivtv_settings (struct pvr_t *pvr)
{
  struct ivtv_ioctl_codec codec;

  if (!pvr)
    return -1;
  
  if (pvr->dev_fd < 0)
    return -1;

  /* get current settings */
  if (ioctl (pvr->dev_fd, IVTV_IOC_G_CODEC, &codec) < 0)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s can't get codec (%s).\n", LOG_LEVEL_IVTV, strerror (errno));
    return -1;
  }
  
  /* set default encoding settings
   * may be overlapped by user parameters
   * Use VBR MPEG_PS encoding at 6 Mbps (peak at 9.6 Mbps)
   * with 48 KHz L2 384 kbps audio.
   */
  codec.aspect = PVR_ASPECT_RATIO_4_3;
  codec.bitrate_mode = PVR_VIDEO_BITRATE_MODE_VBR;
  codec.bitrate = 6000000;
  codec.bitrate_peak = 9600000;
  codec.stream_type = IVTV_STREAM_PS;
  codec.audio_bitmask = PVR_AUDIO_LAYER_2
    | PVR_AUDIO_BITRATE_L2_384 | PVR_AUDIO_SAMPLE_RATE_48_KHZ;

  /* set aspect ratio */
  if (pvr->aspect != -1)
    codec.aspect = pvr->aspect;

  /* if user value is given, we need to reset audio bitmask */
  if ((pvr->samplerate != -1) || (pvr->layer != -1)
      || (pvr->audio_rate != -1) || (pvr->audio_mode != -1))
    codec.audio_bitmask = 0;
  
  /* set audio samplerate */
  if (pvr->samplerate != -1)
    codec.audio_bitmask |= pvr->samplerate;

  /* set audio layer */
  if (pvr->layer != -1)
    codec.audio_bitmask |= pvr->layer;

  /* set audio bitrate */
  if (pvr->audio_rate != -1)
    codec.audio_bitmask |= pvr->audio_rate;

  /* set audio mode */
  if (pvr->audio_mode != -1)
    codec.audio_bitmask |= pvr->audio_mode;

  /* set video bitrate */
  if (pvr->bitrate != -1)
    codec.bitrate = pvr->bitrate;

  /* set video bitrate mode */
  if (pvr->bitrate_mode != -1)
    codec.bitrate_mode = pvr->bitrate_mode;

  /* set video bitrate peak */
  if (pvr->bitrate != -1)
    codec.bitrate_peak = pvr->bitrate_peak;

  /* set video stream type */
  if (pvr->stream_type != -1)
    codec.stream_type = pvr->stream_type;

  /* set new encoding settings */
  if (ioctl (pvr->dev_fd, IVTV_IOC_S_CODEC, &codec) < 0)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s can't set codec (%s).\n", LOG_LEVEL_IVTV, strerror (errno));
    return -1;
  }

  return 0;
}

/* V4L2 layer */

static void
parse_v4l2_tv_options (struct pvr_t *pvr)
{
  if (!pvr)
    return;
  
  if (tv_param_device)
  {
    if (pvr->video_dev)
      free (pvr->video_dev);
    pvr->video_dev = strdup (tv_param_device);
  }
  
  if (tv_param_noaudio)
    pvr->mute = tv_param_noaudio;

  if (tv_param_input)
    pvr->input = tv_param_input;
  
  if (tv_param_normid)
    pvr->normid = tv_param_normid;
  
  if (tv_param_brightness)
    pvr->brightness = tv_param_brightness;
  
  if (tv_param_contrast)
    pvr->contrast = tv_param_contrast;
  
  if (tv_param_hue)
    pvr->hue = tv_param_hue;
  
  if (tv_param_saturation)
    pvr->saturation = tv_param_saturation;

  if (tv_param_width)
    pvr->width = tv_param_width;

  if (tv_param_height)
    pvr->height = tv_param_height;

  if (tv_param_freq)
    pvr->freq = strdup (tv_param_freq);
}

static int
set_v4l2_settings (struct pvr_t *pvr)
{
  if (!pvr)
    return -1;
  
  if (pvr->dev_fd < 0)
    return -1;

  /* -tv noaudio */
  if (pvr->mute)
  {
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_AUDIO_MUTE;
    ctrl.value = 1;
    if (ioctl (pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't mute (%s).\n", LOG_LEVEL_V4L2, strerror (errno));
      return -1;
    }
  }

  /* -tv input=x */
  if (pvr->input != 0)
  {
    if (ioctl (pvr->dev_fd, VIDIOC_S_INPUT, &pvr->input) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't set input (%s)\n", LOG_LEVEL_V4L2, strerror (errno));
      return -1;
    }
  }
  
  /* -tv normid=x */
  if (pvr->normid != -1)
  {
    struct v4l2_standard std;
    std.index = pvr->normid;

    if (ioctl (pvr->dev_fd, VIDIOC_ENUMSTD, &std) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't set norm (%s)\n", LOG_LEVEL_V4L2, strerror (errno));
      return -1;
    }

    mp_msg (MSGT_OPEN, MSGL_V,
            "%s set norm to %s\n", LOG_LEVEL_V4L2, std.name);

    if (ioctl (pvr->dev_fd, VIDIOC_S_STD, &std.id) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't set norm (%s)\n", LOG_LEVEL_V4L2, strerror (errno));
      return -1;
    }
  }
  
  /* -tv brightness=x */
  if (pvr->brightness != 0)
  {
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_BRIGHTNESS;
    ctrl.value = pvr->brightness;

    if (ctrl.value < 0)
      ctrl.value = 0;
    if (ctrl.value > 255)
      ctrl.value = 255;
    
    if (ioctl (pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't set brightness to %d (%s).\n",
              LOG_LEVEL_V4L2, ctrl.value, strerror (errno));
      return -1;
    }
  }

  /* -tv contrast=x */
  if (pvr->contrast != 0)
  {
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_CONTRAST;
    ctrl.value = pvr->contrast;

    if (ctrl.value < 0)
      ctrl.value = 0;
    if (ctrl.value > 127)
      ctrl.value = 127;
    
    if (ioctl (pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't set contrast to %d (%s).\n",
              LOG_LEVEL_V4L2, ctrl.value, strerror (errno));
      return -1;
    }
  }

  /* -tv hue=x */
  if (pvr->hue != 0)
  {
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_HUE;
    ctrl.value = pvr->hue;

    if (ctrl.value < -128)
      ctrl.value = -128;
    if (ctrl.value > 127)
      ctrl.value = 127;
    
    if (ioctl (pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't set hue to %d (%s).\n",
              LOG_LEVEL_V4L2, ctrl.value, strerror (errno));
      return -1;
    }
  }
  
  /* -tv saturation=x */
  if (pvr->saturation != 0)
  {
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_SATURATION;
    ctrl.value = pvr->saturation;

    if (ctrl.value < 0)
      ctrl.value = 0;
    if (ctrl.value > 127)
      ctrl.value = 127;
    
    if (ioctl (pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't set saturation to %d (%s).\n",
              LOG_LEVEL_V4L2, ctrl.value, strerror (errno));
      return -1;
    }
  }
  
  /* -tv width=x:height=y */
  if (pvr->width && pvr->height)
  {
    struct v4l2_format vfmt;
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vfmt.fmt.pix.width = pvr->width;
    vfmt.fmt.pix.height = pvr->height;

    if (ioctl (pvr->dev_fd, VIDIOC_S_FMT, &vfmt) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't set resolution to %dx%d (%s).\n",
              LOG_LEVEL_V4L2, pvr->width, pvr->height, strerror (errno));
      return -1;
    }
  }

  /* -tv freq=x */
  if (pvr->freq)
  {
    struct v4l2_frequency vf;
    vf.tuner = 0;
    vf.type = 0;
    vf.frequency = strtol (pvr->freq, 0L, 0);
    mp_msg (MSGT_OPEN, MSGL_INFO,
            "%s setting frequency to %d\n", LOG_LEVEL_V4L2, vf.frequency);
    
    if (ioctl (pvr->dev_fd, VIDIOC_S_FREQUENCY, &vf) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR, "%s can't set frequency (%s).\n",
              LOG_LEVEL_V4L2, strerror (errno));
      return -1;
    }
  }

  return 0;
}

static int
v4l2_list_capabilities (struct pvr_t *pvr)
{
  struct v4l2_audio vaudio;
  struct v4l2_standard vs;
  struct v4l2_input vin;
  int err = 0;
  
  if (!pvr)
    return -1;

  if (pvr->dev_fd < 0)
    return -1;
  
  /* list available video inputs */
  vin.index = 0;
  err = 1;
  mp_msg (MSGT_OPEN, MSGL_INFO,
          "%s Available video inputs: ", LOG_LEVEL_V4L2);
  while (ioctl (pvr->dev_fd, VIDIOC_ENUMINPUT, &vin) >= 0)
  {
    err = 0;
    mp_msg (MSGT_OPEN, MSGL_INFO, "'#%d, %s' ", vin.index, vin.name);
    vin.index++;
  }
  if (err)
  {
    mp_msg (MSGT_OPEN, MSGL_INFO, "none\n");
    return -1;
  }
  else
    mp_msg (MSGT_OPEN, MSGL_INFO, "\n");

  /* list available audio inputs */
  vaudio.index = 0;
  err = 1;
  mp_msg (MSGT_OPEN, MSGL_INFO,
          "%s Available audio inputs: ", LOG_LEVEL_V4L2);
  while (ioctl (pvr->dev_fd, VIDIOC_ENUMAUDIO, &vaudio) >= 0)
  {
    err = 0;
    mp_msg (MSGT_OPEN, MSGL_INFO, "'#%d, %s' ", vaudio.index, vaudio.name);
    vaudio.index++;
  }
  if (err)
  {
    mp_msg (MSGT_OPEN, MSGL_INFO, "none\n");
    return -1;
  }
  else
    mp_msg (MSGT_OPEN, MSGL_INFO, "\n");

  /* list available norms */
  vs.index = 0;
  mp_msg (MSGT_OPEN, MSGL_INFO, "%s Available norms: ", LOG_LEVEL_V4L2);
  while (ioctl (pvr->dev_fd, VIDIOC_ENUMSTD, &vs) >= 0)
  {
    err = 0;
    mp_msg (MSGT_OPEN, MSGL_INFO, "'#%d, %s' ", vs.index, vs.name);
    vs.index++;
  }
  if (err)
  {
    mp_msg (MSGT_OPEN, MSGL_INFO, "none\n");
    return -1;
  }
  else
    mp_msg (MSGT_OPEN, MSGL_INFO, "\n");

  return 0;
}

static int
v4l2_display_settings (struct pvr_t *pvr)
{
  struct v4l2_audio vaudio;
  struct v4l2_standard vs;
  struct v4l2_input vin;
  v4l2_std_id std;
  int input;
  
  if (!pvr)
    return -1;

  if (pvr->dev_fd < 0)
    return -1;

  /* get current video input */
  if (ioctl (pvr->dev_fd, VIDIOC_G_INPUT, &input) == 0)
  {
    vin.index = input;
    if (ioctl (pvr->dev_fd, VIDIOC_ENUMINPUT, &vin) < 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s can't get input (%s).\n", LOG_LEVEL_V4L2, strerror (errno));
      return -1;
    }
    else
      mp_msg (MSGT_OPEN, MSGL_INFO,
              "%s Video input: %s\n", LOG_LEVEL_V4L2, vin.name);
  }
  else
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s can't get input (%s).\n", LOG_LEVEL_V4L2, strerror (errno));
    return -1;
  }

  /* get current audio input */
  if (ioctl (pvr->dev_fd, VIDIOC_G_AUDIO, &vaudio) == 0)
  {
    mp_msg (MSGT_OPEN, MSGL_INFO,
            "%s Audio input: %s\n", LOG_LEVEL_V4L2, vaudio.name);
  }
  else
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s can't get input (%s).\n", LOG_LEVEL_V4L2, strerror (errno));
    return -1;
  }

  /* get current video format */
  if (ioctl (pvr->dev_fd, VIDIOC_G_STD, &std) == 0)
  {
    vs.index = 0;

    while (ioctl (pvr->dev_fd, VIDIOC_ENUMSTD, &vs) >= 0)
    {
      if (vs.id == std)
      {
        mp_msg (MSGT_OPEN, MSGL_INFO,
                "%s Norm: %s.\n", LOG_LEVEL_V4L2, vs.name);
        break;
      }
      vs.index++;
    }
  }
  else
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s can't get norm (%s)\n", LOG_LEVEL_V4L2, strerror (errno));
    return -1;
  }

  return 0;
}

/* stream layer */

static void
pvr_stream_close (stream_t *stream)
{
  struct pvr_t *pvr;

  if (!stream)
    return;
  
  pvr = (struct pvr_t *) stream->priv;
  pvr_uninit (pvr);
}

static int
pvr_stream_read (stream_t *stream, char *buffer, int size)
{
  struct pollfd pfds[1];
  struct pvr_t *pvr;
  int rk, fd, pos;

  if (!stream || !buffer)
    return 0;
  
  pvr = (struct pvr_t *) stream->priv;
  fd = pvr->dev_fd;
  pos = 0;

  if (fd < 0)
    return 0;
  
  while (pos < size)
  {
    pfds[0].fd = fd;
    pfds[0].events = POLLIN | POLLPRI;

    rk = size - pos;

    if (poll (pfds, 1, 500) <= 0)
    {
      mp_msg (MSGT_OPEN, MSGL_ERR,
              "%s failed with errno %d when reading %d bytes\n",
              LOG_LEVEL_PVR, errno, size-pos);
      break;
    }

    rk = read (fd, &buffer[pos], rk);
    if (rk > 0)
    {
      pos += rk;
      mp_msg (MSGT_OPEN, MSGL_DBG3,
              "%s read (%d) bytes\n", LOG_LEVEL_PVR, pos);
    }
  }
		
  if (!pos)
    mp_msg (MSGT_OPEN, MSGL_ERR, "%s read %d bytes\n", LOG_LEVEL_PVR, pos);

  return pos;
}

static int
pvr_stream_open (stream_t *stream, int mode, void *opts, int *file_format)
{
  struct ivtv_ioctl_codec codec;
  struct ivtv_driver_info info;
  struct v4l2_capability vcap;
  struct pvr_t *pvr = NULL;
  
  if (mode != STREAM_READ)
    return STREAM_UNSUPORTED;
  
  pvr = pvr_init ();

  parse_v4l2_tv_options (pvr);
  parse_ivtv_options (pvr);
  
  /* open device */
  pvr->dev_fd = open (pvr->video_dev, O_RDWR);
  mp_msg (MSGT_OPEN, MSGL_INFO,
          "%s Using device %s\n", LOG_LEVEL_PVR, pvr->video_dev);
  if (pvr->dev_fd == -1)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s error opening device %s\n", LOG_LEVEL_PVR, pvr->video_dev);
    pvr_uninit (pvr);
    return STREAM_ERROR;
  }
  
  /* query capabilities (i.e test V4L2 support) */
  if (ioctl (pvr->dev_fd, VIDIOC_QUERYCAP, &vcap) < 0)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s device is not V4L2 compliant (%s).\n",
            LOG_LEVEL_PVR, strerror (errno));
    pvr_uninit (pvr);
    return STREAM_ERROR;
  }
  else
    mp_msg (MSGT_OPEN, MSGL_INFO,
            "%s Detected %s\n", LOG_LEVEL_PVR, vcap.card);

  /* get codec and initialize card (i.e test IVTV support) */
  if (ioctl (pvr->dev_fd, IVTV_IOC_G_CODEC, &codec) < 0)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s device is not IVTV compliant (%s).\n",
            LOG_LEVEL_PVR, strerror (errno));
    pvr_uninit (pvr);
    return STREAM_ERROR;
  }
  
  /* get ivtv driver info */
  if (ioctl (pvr->dev_fd, IVTV_IOC_G_DRIVER_INFO, &info) < 0)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s device is not IVTV compliant (%s).\n",
            LOG_LEVEL_PVR, strerror (errno));
    pvr_uninit (pvr);
    return STREAM_ERROR;
  }
  else
    mp_msg (MSGT_OPEN, MSGL_INFO,
            "%s Detected ivtv driver: %s\n", LOG_LEVEL_PVR, info.comment);

  /* list V4L2 capabilities */
  if (v4l2_list_capabilities (pvr) == -1)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s can't get v4l2 capabilities\n", LOG_LEVEL_PVR);
    pvr_uninit (pvr);
    return STREAM_ERROR;
  }
  
  /* apply V4L2 settings */
  if (set_v4l2_settings (pvr) == -1)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s can't set v4l2 settings\n", LOG_LEVEL_PVR);
    pvr_uninit (pvr);
    return STREAM_ERROR;
  }

  /* apply IVTV settings */
  if (set_ivtv_settings (pvr) == -1)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s can't set ivtv settings\n", LOG_LEVEL_PVR);
    pvr_uninit (pvr);
    return STREAM_ERROR;
  }
  
  /* display current V4L2 settings */
  if (v4l2_display_settings (pvr) == -1)
  {
    mp_msg (MSGT_OPEN, MSGL_ERR,
            "%s can't get v4l2 settings\n", LOG_LEVEL_PVR);
    pvr_uninit (pvr);
    return STREAM_ERROR;
  }

  stream->priv = pvr;
  stream->type = STREAMTYPE_PVR;
  stream->fill_buffer = pvr_stream_read;
  stream->close = pvr_stream_close;
  
  return STREAM_OK;
}

stream_info_t stream_info_pvr = {
  "PVR (V4L2/IVTV) Input",
  "pvr",
  "Benjamin Zores",
  "",
  pvr_stream_open, 			
  { "pvr", NULL },
  NULL,
  1
};
