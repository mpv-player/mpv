/*
 * MJPEG API extensions for the Video4Linux API, first introduced by the
 * Iomega Buz driver by Rainer Johanni <rainer@johanni.de>.
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

#ifndef MPLAYER_VIDEODEV_MJPEG_H
#define MPLAYER_VIDEODEV_MJPEG_H

#include <stdlib.h>

/* This is identical with the mgavideo internal params struct,
   please tell me if you change this struct here ! <gz@lysator.liu.se) */
struct mjpeg_params
{

   /* The following parameters can only be queried */

   int major_version;            /* Major version number of driver */
   int minor_version;            /* Minor version number of driver */

   /* Main control parameters */

   int input;                    /* Input channel: 0 = Composite, 1 = S-VHS */
   int norm;                     /* Norm: VIDEO_MODE_PAL or VIDEO_MODE_NTSC */
   int decimation;               /* decimation of captured video,
                                    enlargement of video played back.
                                    Valid values are 1, 2, 4 or 0.
                                    0 is a special value where the user
                                    has full control over video scaling */

   /* The following parameters only have to be set if decimation==0,
      for other values of decimation they provide the data how the image is captured */

   int HorDcm;                    /* Horizontal decimation: 1, 2 or 4 */
   int VerDcm;                    /* Vertical decimation: 1 or 2 */
   int TmpDcm;                    /* Temporal decimation: 1 or 2,
                                     if TmpDcm==2 in capture every second frame is dropped,
                                     in playback every frame is played twice */
   int field_per_buff;            /* Number of fields per buffer: 1 or 2 */
   int img_x;                     /* start of image in x direction */
   int img_y;                     /* start of image in y direction */
   int img_width;                 /* image width BEFORE decimation,
                                     must be a multiple of HorDcm*16 */
   int img_height;                /* image height BEFORE decimation,
                                     must be a multiple of VerDcm*8 */

   /* --- End of parameters for decimation==0 only --- */

   /* JPEG control parameters */

   int  quality;                  /* Measure for quality of compressed images.
                                     Scales linearly with the size of the compressed images.
                                     Must be beetween 0 and 100, 100 is a compression
                                     ratio of 1:4 */

   int  odd_even;                 /* Which field should come first ???
                                     This is more aptly named "top_first",
                                     i.e. (odd_even==1) --> top-field-first */

   int  APPn;                     /* Number of APP segment to be written, must be 0..15 */
   int  APP_len;                  /* Length of data in JPEG APPn segment */
   char APP_data[60];             /* Data in the JPEG APPn segment. */

   int  COM_len;                  /* Length of data in JPEG COM segment */
   char COM_data[60];             /* Data in JPEG COM segment */

   unsigned long jpeg_markers;    /* Which markers should go into the JPEG output.
                                     Unless you exactly know what you do, leave them untouched.
                                     Inluding less markers will make the resulting code
                                     smaller, but there will be fewer aplications
                                     which can read it.
                                     The presence of the APP and COM marker is
                                     influenced by APP0_len and COM_len ONLY! */
#define JPEG_MARKER_DHT (1<<3)    /* Define Huffman Tables */
#define JPEG_MARKER_DQT (1<<4)    /* Define Quantization Tables */
#define JPEG_MARKER_DRI (1<<5)    /* Define Restart Interval */
#define JPEG_MARKER_COM (1<<6)    /* Comment segment */
#define JPEG_MARKER_APP (1<<7)    /* App segment, driver will allways use APP0 */

   int  VFIFO_FB;                 /* Flag for enabling Video Fifo Feedback.
                                     If this flag is turned on and JPEG decompressing
                                     is going to the screen, the decompress process
                                     is stopped every time the Video Fifo is full.
                                     This enables a smooth decompress to the screen
                                     but the video output signal will get scrambled */

   /* Misc */

	char reserved[312];  /* Makes 512 bytes for this structure */
};

struct mjpeg_requestbuffers
{
   unsigned long count;      /* Number of buffers for MJPEG grabbing */
   unsigned long size;       /* Size PER BUFFER in bytes */
};

struct mjpeg_sync
{
   unsigned long frame;      /* Frame (0 - n) for double buffer */
   unsigned long length;     /* number of code bytes in buffer (capture only) */
   unsigned long seq;        /* frame sequence number */
   struct timeval timestamp; /* timestamp */
};

struct mjpeg_status
{
   int input;                /* Input channel, has to be set prior to BUZIOC_G_STATUS */
   int signal;               /* Returned: 1 if valid video signal detected */
   int norm;                 /* Returned: VIDEO_MODE_PAL or VIDEO_MODE_NTSC */
   int color;                /* Returned: 1 if color signal detected */
};

/*
Private IOCTL to set up for displaying MJPEG
*/
#define MJPIOC_G_PARAMS       _IOR ('v', BASE_VIDIOCPRIVATE+0,  struct mjpeg_params)
#define MJPIOC_S_PARAMS       _IOWR('v', BASE_VIDIOCPRIVATE+1,  struct mjpeg_params)
#define MJPIOC_REQBUFS        _IOWR('v', BASE_VIDIOCPRIVATE+2,  struct mjpeg_requestbuffers)
#define MJPIOC_QBUF_CAPT      _IOW ('v', BASE_VIDIOCPRIVATE+3,  int)
#define MJPIOC_QBUF_PLAY      _IOW ('v', BASE_VIDIOCPRIVATE+4,  int)
#define MJPIOC_SYNC           _IOR ('v', BASE_VIDIOCPRIVATE+5,  struct mjpeg_sync)
#define MJPIOC_G_STATUS       _IOWR('v', BASE_VIDIOCPRIVATE+6,  struct mjpeg_status)

#endif /* MPLAYER_VIDEODEV_MJPEG_H */
