#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef USE_DIRECTSHOW

#include "ad_internal.h"

static ad_info_t info = 
{
	"Win32/DirectShow decoders",
	"dshow",
	AFM_DSHOW,
	"Nick Kurshev",
	"avifile.sf.net",
	""
};

LIBAD_EXTERN(dshow)

#include "dshow/DS_AudioDecoder.h"

static DS_AudioDecoder* ds_adec=NULL;

static int init(sh_audio_t *sh)
{
  return 1;
}

static int preinit(sh_audio_t *sh_audio)
{
  if(!(ds_adec=DS_AudioDecoder_Open(sh_audio->codec->dll,&sh_audio->codec->guid,sh_audio->wf)))
  {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_MissingDLLcodec,sh_audio->codec->dll);
    return 0;
  } else {
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->channels=sh_audio->wf->nChannels;
    sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
    sh_audio->audio_in_minsize=2*sh_audio->wf->nBlockAlign;
    if(sh_audio->audio_in_minsize<8192) sh_audio->audio_in_minsize=8192;
    sh_audio->a_in_buffer_size=sh_audio->audio_in_minsize;
    sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
    sh_audio->a_in_buffer_len=0;
    sh_audio->audio_out_minsize=16384;
  }
  mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32/DShow audio codec init OK!\n");
  return 1;
}

static void uninit(sh_audio_t *sh)
{
    // TODO!!!
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
//	int len=-1;
        int size_in=0;
        int size_out=0;
        int srcsize=DS_AudioDecoder_GetSrcSize(ds_adec, maxlen);
        mp_msg(MSGT_DECAUDIO,MSGL_DBG3,"DShow says: srcsize=%d  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,maxlen);
        if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
        if(sh_audio->a_in_buffer_len<srcsize){
          sh_audio->a_in_buffer_len+=
            demux_read_data(sh_audio->ds,&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len],
            srcsize-sh_audio->a_in_buffer_len);
        }
        DS_AudioDecoder_Convert(ds_adec, sh_audio->a_in_buffer,sh_audio->a_in_buffer_len,
            buf,maxlen, &size_in,&size_out);
        mp_dbg(MSGT_DECAUDIO,MSGL_DBG2,"DShow: audio %d -> %d converted  (in_buf_len=%d of %d)  %d\n",size_in,size_out,sh_audio->a_in_buffer_len,sh_audio->a_in_buffer_size,ds_tell_pts(sh_audio->ds));
        if(size_in>=sh_audio->a_in_buffer_len){
          sh_audio->a_in_buffer_len=0;
        } else {
          sh_audio->a_in_buffer_len-=size_in;
          memcpy(sh_audio->a_in_buffer,&sh_audio->a_in_buffer[size_in],sh_audio->a_in_buffer_len);
        }
//        len=size_out;
  return size_out;
}
#endif
