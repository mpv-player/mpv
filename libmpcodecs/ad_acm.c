#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef USE_WIN32DLL

#include "ad_internal.h"

static ad_info_t info = 
{
	"Win32 ACM audio decoder",
	"acm",
	AFM_ACM,
	"Nick Kurshev",
	"avifile.sf.net",
	""
};

LIBAD_EXTERN(acm)

#include "dll_init.h"

static int init(sh_audio_t *sh_audio)
{
    int ret=acm_decode_audio(sh_audio,sh_audio->a_buffer,4096,sh_audio->a_buffer_size);
    if(ret<0){
        mp_msg(MSGT_DECAUDIO,MSGL_INFO,"ACM decoding error: %d\n",ret);
        return 0;
    }
    sh_audio->a_buffer_len=ret;
  return 1;
}

static int preinit(sh_audio_t *sh_audio)
{
  /* Win32 ACM audio codec: */
  if(init_acm_audio_codec(sh_audio)){
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->channels=sh_audio->o_wf.nChannels;
    sh_audio->samplerate=sh_audio->o_wf.nSamplesPerSec;
  } else {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_ACMiniterror);
    return 0;
  }
  mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32/ACM audio codec init OK!\n");
  return 1;
}

static void uninit(sh_audio_t *sh)
{
    // TODO!
}

static int control(sh_audio_t *sh_audio,int cmd,void* arg, ...)
{
  int skip;
    switch(cmd)
    {
      case ADCTRL_SKIP_FRAME:
		    skip=sh_audio->wf->nBlockAlign;
		    if(skip<16){
		      skip=(sh_audio->wf->nAvgBytesPerSec/16)&(~7);
		      if(skip<16) skip=16;
		    }
		    demux_read_data(sh_audio->ds,NULL,skip);
	  return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  return acm_decode_audio(sh_audio,buf,minlen,maxlen);
}
#endif
