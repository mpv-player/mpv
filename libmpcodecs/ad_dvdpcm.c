#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"Uncompressed DVD PCM audio decoder",
	"dvdpcm",
	AFM_DVDPCM,
	"Nick Kurshev",
	"A'rpi",
	""
};

LIBAD_EXTERN(dvdpcm)

static int init(sh_audio_t *sh)
{
/* DVD PCM Audio:*/
    sh->channels=2;
    sh->samplerate=48000;
    sh->i_bps=2*2*48000;
/*    sh_audio->pcm_bswap=1;*/
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
  int j,len;
  len=demux_read_data(sh_audio->ds,buf,(minlen+3)&(~3));
#ifndef WORDS_BIGENDIAN
  for(j=0;j<len;j+=2){
    char x=buf[j];
    buf[j]=buf[j+1];
    buf[j+1]=x;
  }
#endif
  return len;
}
