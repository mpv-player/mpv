#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"Uncompressed DVD/VOB LPCM audio decoder",
	"dvdpcm",
	"Nick Kurshev",
	"A'rpi",
	""
};

LIBAD_EXTERN(dvdpcm)

static int init(sh_audio_t *sh)
{
/* DVD PCM Audio:*/
    if(sh->codecdata_len==3){
	// we have LPCM header:
	unsigned char h=sh->codecdata[1];
	sh->channels=1+(h&7);
	switch((h>>4)&3){
	case 0: sh->samplerate=48000;break;
	case 1: sh->samplerate=96000;break;
	case 2: sh->samplerate=44100;break;
	case 3: sh->samplerate=32000;break;
	}
    } else {
	// use defaults:
	sh->channels=2;
	sh->samplerate=48000;
    }
    sh->i_bps=2*sh->channels*sh->samplerate;
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
