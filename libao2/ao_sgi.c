/*
 * SGI/IRIX audio output driver
 *
 * copyright (c) 2001 oliver.schoenbrunner@jku.at
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <dmedia/audio.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "libaf/af_format.h"

static ao_info_t info = 
{
	"sgi audio output",
	"sgi",
	"Oliver Schoenbrunner",
	""
};

LIBAO_EXTERN(sgi)


static ALconfig	ao_config;
static ALport	ao_port;
static int sample_rate;
static int queue_size;
static int bytes_per_frame;

/**
 * \param   [in/out]  format
 * \param   [out]     width
 *
 * \return  the closest matching SGI AL sample format
 *
 * \note    width is set to required per-channel sample width
 *          format is updated to match the SGI AL sample format
 */
static int fmt2sgial(int *format, int *width) {
  int smpfmt = AL_SAMPFMT_TWOSCOMP;

  /* SGI AL only supports float and signed integers in native
   * endianness. If this is something else, we must rely on the audio
   * filter to convert it to a compatible format. */

  /* 24-bit audio is supported, but only with 32-bit alignment.
   * mplayer's 24-bit format is packed, unfortunately.
   * So we must upgrade 24-bit requests to 32 bits. Then we drop the
   * lowest 8 bits during playback. */

  switch(*format) {
  case AF_FORMAT_U8:
  case AF_FORMAT_S8:
    *width = AL_SAMPLE_8;
    *format = AF_FORMAT_S8;
    break;

  case AF_FORMAT_U16_LE:
  case AF_FORMAT_U16_BE:
  case AF_FORMAT_S16_LE:
  case AF_FORMAT_S16_BE:
    *width = AL_SAMPLE_16;
    *format = AF_FORMAT_S16_NE;
    break;

  case AF_FORMAT_U24_LE:
  case AF_FORMAT_U24_BE:
  case AF_FORMAT_S24_LE:
  case AF_FORMAT_S24_BE:
  case AF_FORMAT_U32_LE:
  case AF_FORMAT_U32_BE:
  case AF_FORMAT_S32_LE:
  case AF_FORMAT_S32_BE:
    *width = AL_SAMPLE_24;
    *format = AF_FORMAT_S32_NE;
    break;

  case AF_FORMAT_FLOAT_LE:
  case AF_FORMAT_FLOAT_BE:
    *width = 4;
    *format = AF_FORMAT_FLOAT_NE;
    smpfmt = AL_SAMPFMT_FLOAT;
    break;

  default:
    *width = AL_SAMPLE_16;
    *format = AF_FORMAT_S16_NE;
    break;

  }

  return smpfmt;
}

// to set/get/query special features/parameters
static int control(int cmd, void *arg){
  
  mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_SGI_INFO);
  
  switch(cmd) {
  case AOCONTROL_QUERY_FORMAT:
    /* Do not reject any format: return the closest matching
     * format if the request is not supported natively. */
    return CONTROL_TRUE;
  }

  return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate, int channels, int format, int flags) {

  int smpwidth, smpfmt;
  int rv = AL_DEFAULT_OUTPUT;

  smpfmt = fmt2sgial(&format, &smpwidth);

  mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_SGI_InitInfo, rate, (channels > 1) ? "Stereo" : "Mono", af_fmt2str_short(format));
  
  { /* from /usr/share/src/dmedia/audio/setrate.c */
  
    double frate, realrate;
    ALpv x[2];

    if(ao_subdevice) {
      rv = alGetResourceByName(AL_SYSTEM, ao_subdevice, AL_OUTPUT_DEVICE_TYPE);
      if (!rv) {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_SGI_InvalidDevice);
	return 0;
      }
    }
    
    frate = rate;
   
    x[0].param = AL_RATE;
    x[0].value.ll = alDoubleToFixed(rate);
    x[1].param = AL_MASTER_CLOCK;
    x[1].value.i = AL_CRYSTAL_MCLK_TYPE;

    if (alSetParams(rv,x, 2)<0) {
      mp_msg(MSGT_AO, MSGL_WARN, MSGTR_AO_SGI_CantSetParms_Samplerate, alGetErrorString(oserror()));
    }
    
    if (x[0].sizeOut < 0) {
      mp_msg(MSGT_AO, MSGL_WARN, MSGTR_AO_SGI_CantSetAlRate);
    }

    if (alGetParams(rv,x, 1)<0) {
      mp_msg(MSGT_AO, MSGL_WARN, MSGTR_AO_SGI_CantGetParms, alGetErrorString(oserror()));
    }
    
    realrate = alFixedToDouble(x[0].value.ll);
    if (frate != realrate) {
      mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_SGI_SampleRateInfo, realrate, frate);
    } 
    sample_rate = (int)realrate;
  }
  
  bytes_per_frame = channels * smpwidth;

  ao_data.samplerate = sample_rate;
  ao_data.channels = channels;
  ao_data.format = format;
  ao_data.bps = sample_rate * bytes_per_frame;
  ao_data.buffersize=131072;
  ao_data.outburst = ao_data.buffersize/16;
  
  ao_config = alNewConfig();
  
  if (!ao_config) {
    mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_SGI_InitConfigError, alGetErrorString(oserror()));
    return 0;
  }
  
  if(alSetChannels(ao_config, channels) < 0 ||
     alSetWidth(ao_config, smpwidth) < 0 ||
     alSetSampFmt(ao_config, smpfmt) < 0 ||
     alSetQueueSize(ao_config, sample_rate) < 0 ||
     alSetDevice(ao_config, rv) < 0) {
    mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_SGI_InitConfigError, alGetErrorString(oserror()));
    return 0;
  }
  
  ao_port = alOpenPort("mplayer", "w", ao_config);
  
  if (!ao_port) {
    mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_SGI_InitOpenAudioFailed, alGetErrorString(oserror()));
    return 0;
  }
  
  // printf("ao_sgi, init: port %d config %d\n", ao_port, ao_config);
  queue_size = alGetQueueSize(ao_config);
  return 1;  

}

// close audio device
static void uninit(int immed) {

  /* TODO: samplerate should be set back to the value before mplayer was started! */

  mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_SGI_Uninit);

  if (ao_config) {
    alFreeConfig(ao_config);
    ao_config = NULL;
  }

  if (ao_port) {
    if (!immed)
    while(alGetFilled(ao_port) > 0) sginap(1);  
    alClosePort(ao_port);
    ao_port = NULL;
  }
	
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void) {
  
  mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_SGI_Reset);
  
  alDiscardFrames(ao_port, queue_size);
}

// stop playing, keep buffers (for pause)
static void audio_pause(void) {
    
  mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_SGI_PauseInfo);
    
}

// resume playing, after audio_pause()
static void audio_resume(void) {

  mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_SGI_ResumeInfo);

}

// return: how many bytes can be played without blocking
static int get_space(void) {
  
  // printf("ao_sgi, get_space: (ao_outburst %d)\n", ao_data.outburst);
  // printf("ao_sgi, get_space: alGetFillable [%d] \n", alGetFillable(ao_port));
  
  return alGetFillable(ao_port) * bytes_per_frame;
    
}


// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data, int len, int flags) {
    
  /* Always process data in quadword-aligned chunks (64-bits). */
  const int plen = len / (sizeof(uint64_t) * bytes_per_frame);
  const int framecount = plen * sizeof(uint64_t);

  // printf("ao_sgi, play: len %d flags %d (%d %d)\n", len, flags, ao_port, ao_config);
  // printf("channels %d\n", ao_data.channels);

  if(ao_data.format == AF_FORMAT_S32_NE) {
    /* The zen of this is explained in fmt2sgial() */
    int32_t *smpls = data;
    const int32_t *smple = smpls + (framecount * ao_data.channels);
    while(smpls < smple)
      *smpls++ >>= 8;
  }

  alWriteFrames(ao_port, data, framecount);

  return framecount * bytes_per_frame;
  
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void){
  
  // printf("ao_sgi, get_delay: (ao_buffersize %d)\n", ao_buffersize);
  
  // return  (float)queue_size/((float)sample_rate);
  const int outstanding = alGetFilled(ao_port);
  return (float)((outstanding < 0) ? queue_size : outstanding) /
    ((float)sample_rate);
}






