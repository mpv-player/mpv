#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "ad_internal.h"

#include "../liba52/a52.h"
#include "../ac3-iec958.h"

extern int a52_fillbuff(sh_audio_t *sh_audio);

static ad_info_t info = 
{
	"AC3 through SPDIF",
	"hwac3",
	AFM_HWAC3,
	"Nick Kurshev",
	"???",
	""
};

LIBAD_EXTERN(hwac3)

static int preinit(sh_audio_t *sh)
{
  /* Dolby AC3 audio: */
  sh->audio_out_minsize=4*256*6;
  sh->channels=2;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
  /* Dolby AC3 passthrough:*/
  sample_t *a52_samples=a52_init(0);
  if (a52_samples == NULL) {
       mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 init failed\n");
       return 0;
  }
  sh_audio->a_in_buffer_size=3840;
  sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
  sh_audio->a_in_buffer_len=0;
  if(a52_fillbuff(sh_audio)<0) {
       mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 sync failed\n");
       return 0;
  }
 /* 
  sh_audio->samplerate=ai.samplerate;   // SET by a52_fillbuff()
  sh_audio->samplesize=ai.framesize;
  sh_audio->i_bps=ai.bitrate*(1000/8);  // SET by a52_fillbuff()
  sh_audio->ac3_frame=malloc(6144);
  sh_audio->o_bps=sh_audio->i_bps;  // XXX FIXME!!! XXX

   o_bps is calculated from samplesize*channels*samplerate
   a single ac3 frame is always translated to 6144 byte packet. (zero padding)*/
  sh_audio->channels=2;
  sh_audio->samplesize=2;   /* 2*2*(6*256) = 6144 (very TRICKY!)*/
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
  int len=-1;
  if(!sh_audio->a_in_buffer_len)
    if((len=a52_fillbuff(sh_audio))<0) return len; /*EOF*/
  sh_audio->a_in_buffer_len=0;
  len = ac3_iec958_build_burst(len, 0x01, 1, sh_audio->a_in_buffer, buf);
  return len;
}
