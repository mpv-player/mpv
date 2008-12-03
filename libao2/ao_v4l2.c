/*
 * audio output for V4L2 hardware MPEG decoders
 *
 * WARNING: You need to force -ac hwmpa for audio output to work.
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

#include <inttypes.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include "libmpdemux/mpeg_packetizer.h"

#define MPEG_AUDIO_ID 0x1C0

static int freq = 0;

static ao_info_t info = 
{
  "V4L2 MPEG Audio Decoder output",
  "v4l2",
  "Benjamin Zores",
  ""
};

LIBAO_EXTERN (v4l2)

/* to set/get/query special features/parameters */
static int
control (int cmd,void *arg)
{
  return CONTROL_UNKNOWN;
}

/* open & setup audio device */
static int
init (int rate, int channels, int format, int flags)
{
  extern int v4l2_fd;

  if (v4l2_fd < 0)
    return 0;

  if (format != AF_FORMAT_MPEG2)
  {
    mp_msg (MSGT_AO, MSGL_FATAL,
            "AO: [v4l2] can only handle MPEG audio streams.\n");
    return 0;
  }
  
  ao_data.outburst = 2048;
  ao_data.samplerate = rate;
  ao_data.channels = channels;
  ao_data.format = AF_FORMAT_MPEG2;
  ao_data.buffersize = 2048;
  ao_data.bps = rate * 2 * 2;
  ao_data.pts = 0;
  freq = rate;

  /* check for supported audio rate */
  if (rate != 32000 || rate != 41000 || rate != 48000)
  {
    mp_msg (MSGT_AO, MSGL_ERR, MSGTR_AO_MPEGPES_UnsupSamplerate, rate);
    rate = 48000;
  }

  return 1;
}

/* close audio device */
static void
uninit (int immed)
{
  /* nothing to do */
}

/* stop playing and empty buffers (for seeking/pause) */
static void
reset (void)
{
  /* nothing to do */
}

/* stop playing, keep buffers (for pause) */
static void
audio_pause (void)
{
  reset ();
}

/* resume playing, after audio_pause() */
static void
audio_resume (void)
{
  /* nothing to do */
}

/* how many bytes can be played without blocking */
static int
get_space (void)
{
  extern int vo_pts;
  float x;
  int y;

  x = (float) (vo_pts - ao_data.pts) / 90000.0;
  if (x <= 0)
    return 0;
  
  y  = freq * 4 * x;
  y /= ao_data.outburst;
  y *= ao_data.outburst;
  
  if (y > 32000)
    y = 32000;

  return y;
}

/* number of bytes played */
static int
play (void *data, int len, int flags)
{
  int v4l2_write (unsigned char *data, int len);
  
  if (ao_data.format != AF_FORMAT_MPEG2)
    return 0;

  send_mpeg_pes_packet (data, len, MPEG_AUDIO_ID, ao_data.pts, 2, v4l2_write);

  return len;
}

/* delay in seconds between first and last sample in buffer */
static float
get_delay (void)
{
  return 0.0;
}
