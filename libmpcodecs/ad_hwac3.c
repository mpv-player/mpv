
// Reference: DOCS/tech/hwac3.txt !!!!!

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "ad_internal.h"

#include "../liba52/a52.h"

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
  sh->audio_in_minsize=3840;
  sh->channels=2;
  sh->sample_format=AFMT_AC3;
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
    if((len=a52_fillbuff(sh_audio))<=0) return len; /*EOF*/
  sh_audio->a_in_buffer_len=0;

//    int ac3_iec958_build_burst(int length, int data_type, int big_endian, unsigned char * data, unsigned char * out)
//  len = ac3_iec958_build_burst(len, 0x01, 1, sh_audio->a_in_buffer, buf);

	buf[0] = 0x72;
	buf[1] = 0xF8;
	buf[2] = 0x1F;
	buf[3] = 0x4E;
	buf[4] = 0x01; //(length) ? data_type : 0; /* & 0x1F; */
	buf[5] = 0x00;
	buf[6] = (len << 3) & 0xFF;
	buf[7] = (len >> 5) & 0xFF;
	swab(sh_audio->a_in_buffer, buf + 8, len);
	//memcpy(buf + 8, sh_audio->a_in_buffer, len);
	memset(buf + 8 + len, 0, 6144 - 8 - len);

	return 6144;
}
