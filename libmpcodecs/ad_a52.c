#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "ad_internal.h"

#include "cpudetect.h"

#include "../liba52/a52.h"
#include "../liba52/mm_accel.h"

static sample_t * a52_samples;
static a52_state_t a52_state;
static uint32_t a52_flags=0;

#include "bswap.h"

static ad_info_t info = 
{
	"AC3-liba52",
	"liba52",
	AFM_A52,
	"Nick Kurshev",
	"Michel LESPINASSE",
	""
};

LIBAD_EXTERN(liba52)

extern int audio_output_channels;

int a52_fillbuff(sh_audio_t *sh_audio){
int length=0;
int flags=0;
int sample_rate=0;
int bit_rate=0;

    sh_audio->a_in_buffer_len=0;
    /* sync frame:*/
while(1){
    while(sh_audio->a_in_buffer_len<7){
	int c=demux_getc(sh_audio->ds);
	if(c<0) return -1; /* EOF*/
        sh_audio->a_in_buffer[sh_audio->a_in_buffer_len++]=c;
    }
    length = a52_syncinfo (sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
    if(length>=7 && length<=3840) break; /* we're done.*/
    /* bad file => resync*/
    memcpy(sh_audio->a_in_buffer,sh_audio->a_in_buffer+1,6);
    --sh_audio->a_in_buffer_len;
}
    mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"a52: len=%d  flags=0x%X  %d Hz %d bit/s\n",length,flags,sample_rate,bit_rate);
    sh_audio->samplerate=sample_rate;
    sh_audio->i_bps=bit_rate/8;
    demux_read_data(sh_audio->ds,sh_audio->a_in_buffer+7,length-7);
    
    if(crc16_block(sh_audio->a_in_buffer+2,length-2)!=0)
	mp_msg(MSGT_DECAUDIO,MSGL_STATUS,"a52: CRC check failed!  \n");
    
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
  mp_msg(MSGT_DECAUDIO,MSGL_INFO,"AC3: %d.%d (%s%s)  %d Hz  %3.1f kbit/s\n",
	channels, (flags&A52_LFE)?1:0,
	mode, (flags&A52_LFE)?"+lfe":"",
	sample_rate, bit_rate*0.001f);
  return (flags&A52_LFE) ? (channels+1) : channels;
}


static int preinit(sh_audio_t *sh)
{
  /* Dolby AC3 audio: */
  /* however many channels, 2 bytes in a word, 256 samples in a block, 6 blocks in a frame */
  sh->audio_out_minsize=audio_output_channels*2*256*6;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
  uint32_t a52_accel=0;
  sample_t level=1, bias=384;
  int flags=0;
  /* Dolby AC3 audio:*/
  if(gCpuCaps.hasSSE) a52_accel|=MM_ACCEL_X86_SSE;
  if(gCpuCaps.hasMMX) a52_accel|=MM_ACCEL_X86_MMX;
  if(gCpuCaps.hasMMX2) a52_accel|=MM_ACCEL_X86_MMXEXT;
  if(gCpuCaps.has3DNow) a52_accel|=MM_ACCEL_X86_3DNOW;
  if(gCpuCaps.has3DNowExt) a52_accel|=MM_ACCEL_X86_3DNOWEXT;
  a52_samples=a52_init (a52_accel);
  if (a52_samples == NULL) {
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 init failed\n");
	return 0;
  }
   sh_audio->a_in_buffer_size=3840;
   sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
   sh_audio->a_in_buffer_len=0;
  if(a52_fillbuff(sh_audio)<0){
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 sync failed\n");
	return 0;
  }
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
  if (a52_frame (&a52_state, sh_audio->a_in_buffer, &flags, &level, bias)){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"a52: error decoding frame -> nosound\n");
    return 0;
  }
  mp_msg(MSGT_DECAUDIO,MSGL_V,"A52 flags after a52_frame: 0x%X\n",flags);
  /* frame decoded, let's init resampler:*/
  if(a52_resample_init(a52_accel,flags,sh_audio->channels)) break;
  --sh_audio->channels; /* try to decrease no. of channels*/
}
  if(sh_audio->channels<=0){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"a52: no resampler. try different channel setup!\n");
    return 0;
  }
  return 1;
}

static void uninit(sh_audio_t *sh)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    switch(cmd)
    {
      case ADCTRL_SKIP_FRAME:
	  a52_fillbuff(sh); break; // skip AC3 frame
	  return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
    sample_t level=1, bias=384;
    int flags=a52_flags|A52_ADJUST_LEVEL;
    int i,len=-1;
	if(!sh_audio->a_in_buffer_len) 
	    if(a52_fillbuff(sh_audio)<0) return len; /* EOF */
	sh_audio->a_in_buffer_len=0;
	if (a52_frame (&a52_state, sh_audio->a_in_buffer, &flags, &level, bias)){
	    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"a52: error decoding frame\n");
	    return len;
	}
	len=0;
	for (i = 0; i < 6; i++) {
	    if (a52_block (&a52_state, a52_samples)){
		mp_msg(MSGT_DECAUDIO,MSGL_WARN,"a52: error at resampling\n");
		break;
	    }
	    len+=2*a52_resample(a52_samples,&buf[len]);
	}
  return len;
}
