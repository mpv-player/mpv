#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"Uncompressed PCM audio decoder",
	"pcm",
	AFM_PCM,
	"Nick Kurshev",
	"A'rpi",
	""
};

LIBAD_EXTERN(pcm)

static int init(sh_audio_t *sh_audio)
{
  WAVEFORMATEX *h=sh_audio->wf;
  sh_audio->i_bps=h->nAvgBytesPerSec;
  sh_audio->channels=h->nChannels;
  sh_audio->samplerate=h->nSamplesPerSec;
  sh_audio->samplesize=(h->wBitsPerSample+7)/8;
  switch(sh_audio->format){ /* hardware formats: */
    case 0x6:  sh_audio->sample_format=AFMT_A_LAW;break;
    case 0x7:  sh_audio->sample_format=AFMT_MU_LAW;break;
    case 0x11: sh_audio->sample_format=AFMT_IMA_ADPCM;break;
    case 0x50: sh_audio->sample_format=AFMT_MPEG;break;
    //  format 0x736f7774  ; "twos" (MOV files)
    case 0x736F7774: sh_audio->sample_format=AFMT_S16_LE;/*sh_audio->codec->driver=AFM_DVDPCM;*/ break;
/*    case 0x2000: sh_audio->sample_format=AFMT_AC3; */
    default: sh_audio->sample_format=(sh_audio->samplesize==2)?AFMT_S16_LE:AFMT_U8;
  }
  return 1;
}

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=2048;
  return 1;
}

static void uninit(sh_audio_t *sh)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
  int skip;
    switch(cmd)
    {
      case ADCTRL_SKIP_FRAME:
	skip=sh->i_bps/16;
	skip=skip&(~3);
	demux_read_data(sh->ds,NULL,skip);
	return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  if(sh_audio->format==0x736F7774){ // "twos" is swapped byteorder
    int j,len;
    len=demux_read_data(sh_audio->ds,buf,(minlen+1)&(~1));
    for(j=0;j<len;j+=2){
      char x=buf[j];
      buf[j]=buf[j+1];
      buf[j+1]=x;
    }
    return len;
  }
  return demux_read_data(sh_audio->ds,buf,minlen);
}
