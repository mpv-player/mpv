#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef USE_LIBAVCODEC

#include "ad_internal.h"

#include "bswap.h"

static ad_info_t info = 
{
	"FFmpeg audio decoders",
	"ffmpeg",
	"Nick Kurshev",
	"ffmpeg.sf.net",
	""
};

LIBAD_EXTERN(ffmpeg)

#define assert(x)

#ifdef USE_LIBAVCODEC_SO
#include <ffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif

extern int avcodec_inited;

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=AVCODEC_MAX_AUDIO_FRAME_SIZE;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
    int x;
    AVCodecContext *lavc_context;
    AVCodec *lavc_codec;

    mp_msg(MSGT_DECAUDIO,MSGL_V,"FFmpeg's libavcodec audio codec\n");
    if(!avcodec_inited){
      avcodec_init();
      avcodec_register_all();
      avcodec_inited=1;
    }
    
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh_audio->codec->dll);
    if(!lavc_codec){
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_MissingLAVCcodec,sh_audio->codec->dll);
	return 0;
    }
    
    lavc_context = malloc(sizeof(AVCodecContext));
    memset(lavc_context, 0, sizeof(AVCodecContext));
    sh_audio->context=lavc_context;
    
    /* open it */
    if (avcodec_open(lavc_context, lavc_codec) < 0) {
        mp_msg(MSGT_DECAUDIO,MSGL_ERR, MSGTR_CantOpenCodec);
        return 0;
    }
   mp_msg(MSGT_DECAUDIO,MSGL_V,"INFO: libavcodec init OK!\n");

   // Decode at least 1 byte:  (to get header filled)
   x=decode_audio(sh_audio,sh_audio->a_buffer,1,sh_audio->a_buffer_size);
   if(x>0) sh_audio->a_buffer_len=x;

#if 1
  sh_audio->channels=lavc_context->channels;
  sh_audio->samplerate=lavc_context->sample_rate;
  sh_audio->i_bps=lavc_context->bit_rate/8;
#else
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
#endif
  return 1;
}

static void uninit(sh_audio_t *sh)
{
    if (avcodec_close(sh->context) < 0)
	mp_msg(MSGT_DECVIDEO, MSGL_ERR, MSGTR_CantCloseCodec);
    free(sh->context);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    // TODO ???
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
    unsigned char *start=NULL;
    int y,len=-1;
    while(len<minlen){
	int len2=0;
	int x=ds_get_packet(sh_audio->ds,&start);
	if(x<=0) break; // error
	y=avcodec_decode_audio(sh_audio->context,(INT16*)buf,&len2,start,x);
	if(y<0){ mp_msg(MSGT_DECAUDIO,MSGL_V,"lavc_audio: error\n");break; }
	if(y<x) sh_audio->ds->buffer_pos+=y-x;  // put back data (HACK!)
	if(len2>0){
	  //len=len2;break;
	  if(len<0) len=len2; else len+=len2;
	  buf+=len2;
	}
        mp_dbg(MSGT_DECAUDIO,MSGL_DBG2,"Decoded %d -> %d  \n",y,len2);
    }
  return len;
}

#endif
