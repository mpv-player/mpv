#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"native GSM/MSGSM audio decoder",
	"msgsm",
	"A'rpi",
	"XAnim",
	""
};

LIBAD_EXTERN(msgsm)

#include "native/xa_gsm.h"

static int init(sh_audio_t *sh_audio)
{
  if(!sh_audio->wf) return 0;
  // MS-GSM audio codec:
  GSM_Init();
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->samplesize=2;
  // decodes 65 byte -> 320 short
  // 1 sec: sh_audio->channels*sh_audio->samplerate  samples
  // 1 frame: 320 samples
  if(sh_audio->format==0x31 || sh_audio->format==0x32){
    sh_audio->ds->ss_mul=65; sh_audio->ds->ss_div=320;
  } else {
    sh_audio->ds->ss_mul=33; sh_audio->ds->ss_div=160;
  }
  sh_audio->i_bps=sh_audio->ds->ss_mul*(sh_audio->channels*sh_audio->samplerate)/sh_audio->ds->ss_div;  // 1:10
  return 1;
}

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize=4*320;
  return 1;
}

static void uninit(sh_audio_t *sh)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  if(sh_audio->format==0x31 || sh_audio->format==0x32){
    unsigned char ibuf[65]; // 65 bytes / frame
    if(demux_read_data(sh_audio->ds,ibuf,65)!=65) return -1; // EOF
    XA_MSGSM_Decoder(ibuf,(unsigned short *) buf); // decodes 65 byte -> 320 short
    return 2*320;
  } else {
    unsigned char ibuf[33]; // 33 bytes / frame
    if(demux_read_data(sh_audio->ds,ibuf,33)!=33) return -1; // EOF
    XA_GSM_Decoder(ibuf,(unsigned short *) buf); // decodes 33 byte -> 160 short
    return 2*160;
  }
}
