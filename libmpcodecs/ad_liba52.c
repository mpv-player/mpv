/*
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

#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"
#include "mpbswap.h"

#include "ad_internal.h"

#include "cpudetect.h"

#include "libaf/af_format.h"

#include <a52dec/a52.h>
#include <a52dec/mm_accel.h>
int (* a52_resample) (float * _f, int16_t * s16);

static a52_state_t *a52_state;
static uint32_t a52_flags=0;
/** Used by a52_resample_float, it defines the mapping between liba52
 * channels and output channels.  The ith nibble from the right in the
 * hex representation of channel_map is the index of the source
 * channel corresponding to the ith output channel.  Source channels are
 * indexed 1-6.  Silent output channels are marked by 0xf. */
static uint32_t channel_map;

#define DRC_NO_ACTION      0
#define DRC_NO_COMPRESSION 1
#define DRC_CALLBACK       2

/** The output is multiplied by this var.  Used for volume control */
static sample_t a52_level = 1;
static int a52_drc_action = DRC_NO_ACTION;

static const ad_info_t info =
{
	"AC3 decoding with liba52",
	"liba52",
	"Nick Kurshev",
	"Michel LESPINASSE",
	""
};

LIBAD_EXTERN(liba52)

static int a52_fillbuff(sh_audio_t *sh_audio)
{
int length=0;
int flags=0;
int sample_rate=0;
int bit_rate=0;

    sh_audio->a_in_buffer_len=0;
    /* sync frame:*/
while(1){
    while(sh_audio->a_in_buffer_len<8){
	int c=demux_getc(sh_audio->ds);
	if(c<0) return -1; /* EOF*/
        sh_audio->a_in_buffer[sh_audio->a_in_buffer_len++]=c;
    }
    if(sh_audio->format!=0x2000) swab(sh_audio->a_in_buffer,sh_audio->a_in_buffer,8);
    length = a52_syncinfo (sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
    if(length>=7 && length<=3840) break; /* we're done.*/
    /* bad file => resync*/
    if(sh_audio->format!=0x2000) swab(sh_audio->a_in_buffer,sh_audio->a_in_buffer,8);
    memmove(sh_audio->a_in_buffer,sh_audio->a_in_buffer+1,7);
    --sh_audio->a_in_buffer_len;
}
    mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"a52: len=%d  flags=0x%X  %d Hz %d bit/s\n",length,flags,sample_rate,bit_rate);
    sh_audio->samplerate=sample_rate;
    sh_audio->i_bps=bit_rate/8;
    sh_audio->samplesize=sh_audio->sample_format==AF_FORMAT_FLOAT_NE ? 4 : 2;
    demux_read_data(sh_audio->ds,sh_audio->a_in_buffer+8,length-8);
    if(sh_audio->format!=0x2000)
	swab(sh_audio->a_in_buffer+8,sh_audio->a_in_buffer+8,length-8);

#ifdef CONFIG_LIBA52_INTERNAL
    if(crc16_block(sh_audio->a_in_buffer+2,length-2)!=0)
	mp_msg(MSGT_DECAUDIO,MSGL_STATUS,"a52: CRC check failed!  \n");
#endif

    return length;
}

/* returns: number of available channels*/
static int a52_printinfo(sh_audio_t *sh_audio){
int flags, sample_rate, bit_rate;
char* mode="unknown";
int channels=0;
  a52_syncinfo (sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
  switch(flags&A52_CHANNEL_MASK){
    case A52_CHANNEL: mode="channel"; channels=2; break;
    case A52_MONO: mode="mono"; channels=1; break;
    case A52_STEREO: mode="stereo"; channels=2; break;
    case A52_3F: mode="3f";channels=3;break;
    case A52_2F1R: mode="2f+1r";channels=3;break;
    case A52_3F1R: mode="3f+1r";channels=4;break;
    case A52_2F2R: mode="2f+2r";channels=4;break;
    case A52_3F2R: mode="3f+2r";channels=5;break;
    case A52_CHANNEL1: mode="channel1"; channels=2; break;
    case A52_CHANNEL2: mode="channel2"; channels=2; break;
    case A52_DOLBY: mode="dolby"; channels=2; break;
  }
  mp_msg(MSGT_DECAUDIO,MSGL_V,"AC3: %d.%d (%s%s)  %d Hz  %3.1f kbit/s\n",
	channels, (flags&A52_LFE)?1:0,
	mode, (flags&A52_LFE)?"+lfe":"",
	sample_rate, bit_rate*0.001f);
  return (flags&A52_LFE) ? (channels+1) : channels;
}

static sample_t dynrng_call (sample_t c, void *data)
{
//	fprintf(stderr, "(%lf, %lf): %lf\n", (double)c, (double)drc_level, (double)pow((double)c, drc_level));
	return pow((double)c, drc_level);
}


static int preinit(sh_audio_t *sh)
{
  /* Dolby AC3 audio: */
  /* however many channels, 2 bytes in a word, 256 samples in a block, 6 blocks in a frame */
  if (sh->samplesize < 4) sh->samplesize = 4;
  sh->audio_out_minsize=audio_output_channels*sh->samplesize*256*6;
  sh->audio_in_minsize=3840;
  a52_level = 1.0;
  return 1;
}

/**
 * \brief Function to convert the "planar" float format used by liba52
 * into the interleaved float format used by libaf/libao2.
 * \param in the input buffer containing the planar samples.
 * \param out the output buffer where the interleaved result is stored.
 */
static int a52_resample_float(float *in, int16_t *out)
{
    unsigned long i;
    float *p = (float*) out;
    for (i = 0; i != 256; i++) {
	unsigned long map = channel_map;
	do {
	    unsigned long ch = map & 15;
	    if (ch == 15)
		*p = 0;
	    else
		*p = in[i + ((ch-1)<<8)];
	    p++;
	} while ((map >>= 4));
    }
    return (int16_t*) p - out;
}

static int init(sh_audio_t *sh_audio)
{
  uint32_t a52_accel=0;
  sample_t level=a52_level, bias=384;
  int flags=0;
  /* Dolby AC3 audio:*/
#ifdef MM_ACCEL_X86_SSE
  if(gCpuCaps.hasSSE) a52_accel|=MM_ACCEL_X86_SSE;
#endif
  if(gCpuCaps.hasMMX) a52_accel|=MM_ACCEL_X86_MMX;
  if(gCpuCaps.hasMMX2) a52_accel|=MM_ACCEL_X86_MMXEXT;
  if(gCpuCaps.has3DNow) a52_accel|=MM_ACCEL_X86_3DNOW;
#ifdef MM_ACCEL_X86_3DNOWEXT
  if(gCpuCaps.has3DNowExt) a52_accel|=MM_ACCEL_X86_3DNOWEXT;
#endif
#ifdef MM_ACCEL_PPC_ALTIVEC
  if(gCpuCaps.hasAltiVec) a52_accel|=MM_ACCEL_PPC_ALTIVEC;
#endif
  a52_state=a52_init (a52_accel);
  if (a52_state == NULL) {
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 init failed\n");
	return 0;
  }
  sh_audio->sample_format = AF_FORMAT_FLOAT_NE;
  if(a52_fillbuff(sh_audio)<0){
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 sync failed\n");
	return 0;
  }


  /* Init a52 dynrng */
  if (drc_level < 0.001) {
	  /* level == 0 --> no compression, init library without callback */
	  a52_drc_action = DRC_NO_COMPRESSION;
  } else if (drc_level > 0.999) {
	  /* level == 1 --> full compression, do nothing at all (library default = full compression) */
	  a52_drc_action = DRC_NO_ACTION;
  } else {
	  a52_drc_action = DRC_CALLBACK;
  }
  /* Library init for dynrng has to be done for each frame, see decode_audio() */


  /* 'a52 cannot upmix' hotfix:*/
  a52_printinfo(sh_audio);
  sh_audio->channels=audio_output_channels;
while(sh_audio->channels>0){
  switch(sh_audio->channels){
	    case 1: a52_flags=A52_MONO; break;
/*	    case 2: a52_flags=A52_STEREO; break;*/
	    case 2: a52_flags=A52_DOLBY; break;
/*	    case 3: a52_flags=A52_3F; break;*/
	    case 3: a52_flags=A52_2F1R; break;
	    case 4: a52_flags=A52_2F2R; break; /* 2+2*/
	    case 5: a52_flags=A52_3F2R; break;
	    case 6: a52_flags=A52_3F2R|A52_LFE; break; /* 5.1*/
  }
  /* test:*/
  flags=a52_flags|A52_ADJUST_LEVEL;
  mp_msg(MSGT_DECAUDIO,MSGL_V,"A52 flags before a52_frame: 0x%X\n",flags);
  if (a52_frame (a52_state, sh_audio->a_in_buffer, &flags, &level, bias)){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"a52: error decoding frame -> nosound\n");
    return 0;
  }
  mp_msg(MSGT_DECAUDIO,MSGL_V,"A52 flags after a52_frame: 0x%X\n",flags);
  /* frame decoded, let's init resampler:*/
  channel_map = 0;
  if (sh_audio->sample_format == AF_FORMAT_FLOAT_NE) {
      if (!(flags & A52_LFE)) {
	  switch ((flags<<3) | sh_audio->channels) {
	    case (A52_MONO    << 3) | 1: channel_map = 0x1; break;
	    case (A52_CHANNEL << 3) | 2:
	    case (A52_STEREO  << 3) | 2:
	    case (A52_DOLBY   << 3) | 2: channel_map =    0x21; break;
	    case (A52_2F1R    << 3) | 3: channel_map =   0x321; break;
	    case (A52_2F2R    << 3) | 4: channel_map =  0x4321; break;
	    case (A52_3F      << 3) | 5: channel_map = 0x2ff31; break;
	    case (A52_3F2R    << 3) | 5: channel_map = 0x25431; break;
	  }
      } else if (sh_audio->channels == 6) {
	  switch (flags & ~A52_LFE) {
	    case A52_MONO   : channel_map = 0x12ffff; break;
	    case A52_CHANNEL:
	    case A52_STEREO :
	    case A52_DOLBY  : channel_map = 0x1fff32; break;
	    case A52_3F     : channel_map = 0x13ff42; break;
	    case A52_2F1R   : channel_map = 0x1f4432; break;
	    case A52_2F2R   : channel_map = 0x1f5432; break;
	    case A52_3F2R   : channel_map = 0x136542; break;
	  }
      }
      if (channel_map) {
	  a52_resample = a52_resample_float;
	  break;
      }
  } else
  break;
}
  if(sh_audio->channels<=0){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"a52: no resampler. try different channel setup!\n");
    return 0;
  }
  return 1;
}

static void uninit(sh_audio_t *sh)
{
  a52_free(a52_state);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    switch(cmd)
    {
      case ADCTRL_RESYNC_STREAM:
      case ADCTRL_SKIP_FRAME:
	  a52_fillbuff(sh);
	  return CONTROL_TRUE;
      case ADCTRL_SET_VOLUME: {
	  float vol = *(float*)arg;
	  if (vol > 60.0) vol = 60.0;
	  a52_level = vol <= -200.0 ? 0 : pow(10.0,vol/20.0);
	  return CONTROL_TRUE;
      }
      case ADCTRL_QUERY_FORMAT:
	  if (*(int*)arg == AF_FORMAT_S16_NE ||
	      *(int*)arg == AF_FORMAT_FLOAT_NE)
	      return CONTROL_TRUE;
	  return CONTROL_FALSE;
    }
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
    sample_t level=a52_level, bias=384;
    int flags=a52_flags|A52_ADJUST_LEVEL;
    int i,len=-1;
	if (sh_audio->sample_format == AF_FORMAT_FLOAT_NE)
	    bias = 0;
	if(!sh_audio->a_in_buffer_len)
	    if(a52_fillbuff(sh_audio)<0) return len; /* EOF */
	sh_audio->a_in_buffer_len=0;
	if (a52_frame (a52_state, sh_audio->a_in_buffer, &flags, &level, bias)){
	    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"a52: error decoding frame\n");
	    return len;
	}

	/* handle dynrng */
	if (a52_drc_action != DRC_NO_ACTION) {
	    if (a52_drc_action == DRC_NO_COMPRESSION)
		a52_dynrng(a52_state, NULL, NULL);
	    else
		a52_dynrng(a52_state, dynrng_call, NULL);
	}

	len=0;
	for (i = 0; i < 6; i++) {
	    if (a52_block (a52_state)){
		mp_msg(MSGT_DECAUDIO,MSGL_WARN,"a52: error at resampling\n");
		break;
	    }
	    len+=2*a52_resample(a52_samples(a52_state),(int16_t *)&buf[len]);
	}
	assert(len <= maxlen);
  return len;
}
