#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"native MSGSM audio decoder",
	"msgsm",
	"A'rpi",
	"XAnim",
	""
};

LIBAD_EXTERN(msgsm)

#include "xa_gsm.h"

static int init(sh_audio_t *sh_audio)
{
  if(!sh_audio->wf) return 0;
  // MS-GSM audio codec:
  GSM_Init();
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  // decodes 65 byte -> 320 short
  // 1 sec: sh_audio->channels*sh_audio->samplerate  samples
  // 1 frame: 320 samples
  sh_audio->i_bps=65*(sh_audio->channels*sh_audio->samplerate)/320;  // 1:10
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
    unsigned char ibuf[65]; // 65 bytes / frame
    if(demux_read_data(sh_audio->ds,ibuf,65)!=65) return -1; // EOF
    XA_MSGSM_Decoder(ibuf,(unsigned short *) buf); // decodes 65 byte -> 320 short
    return 2*320;
}
