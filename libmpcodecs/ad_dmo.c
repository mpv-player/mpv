#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "ad_internal.h"
#include "libaf/reorder_ch.h"

static ad_info_t info = 
{
	"Win32/DMO decoders",
	"dmo",
	"A'rpi",
	"avifile.sf.net",
	""
};

LIBAD_EXTERN(dmo)

#include "loader/dmo/DMO_AudioDecoder.h"

static int init(sh_audio_t *sh)
{
  return 1;
}

static int preinit(sh_audio_t *sh_audio)
{
  DMO_AudioDecoder* ds_adec;
  int chans=(audio_output_channels==sh_audio->wf->nChannels) ?
      audio_output_channels : (sh_audio->wf->nChannels>=2 ? 2 : 1);
  if(!(ds_adec=DMO_AudioDecoder_Open(sh_audio->codec->dll,&sh_audio->codec->guid,sh_audio->wf,chans)))
  {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_MissingDLLcodec,sh_audio->codec->dll);
    return 0;
  }
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->channels=chans;
    sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
    sh_audio->samplesize=2;
    sh_audio->audio_in_minsize=4*sh_audio->wf->nBlockAlign;
    if(sh_audio->audio_in_minsize<8192) sh_audio->audio_in_minsize=8192;
    sh_audio->audio_out_minsize=4*16384;
    sh_audio->context = ds_adec;
  mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32/DMO audio codec init OK!\n");
  return 1;
}

static void uninit(sh_audio_t *sh)
{
    DMO_AudioDecoder* ds_adec = sh->context;
    DMO_AudioDecoder_Destroy(ds_adec);
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
	DMO_AudioDecoder* ds_adec = sh_audio->context;
//	int len=-1;
        int size_in=0;
        int size_out=0;
        int srcsize=DMO_AudioDecoder_GetSrcSize(ds_adec, maxlen);
        mp_msg(MSGT_DECAUDIO,MSGL_DBG3,"DMO says: srcsize=%d  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,maxlen);
        if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
        if(sh_audio->a_in_buffer_len<srcsize){
          sh_audio->a_in_buffer_len+=
            demux_read_data(sh_audio->ds,&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len],
            srcsize-sh_audio->a_in_buffer_len);
        }
        DMO_AudioDecoder_Convert(ds_adec, sh_audio->a_in_buffer,sh_audio->a_in_buffer_len,
            buf,maxlen, &size_in,&size_out);
        mp_dbg(MSGT_DECAUDIO,MSGL_DBG2,"DMO: audio %d -> %d converted  (in_buf_len=%d of %d)  %d\n",size_in,size_out,sh_audio->a_in_buffer_len,sh_audio->a_in_buffer_size,ds_tell_pts(sh_audio->ds));
        if(size_in>=sh_audio->a_in_buffer_len){
          sh_audio->a_in_buffer_len=0;
        } else {
          sh_audio->a_in_buffer_len-=size_in;
          memmove(sh_audio->a_in_buffer,&sh_audio->a_in_buffer[size_in],sh_audio->a_in_buffer_len);
        }
        if (size_out > 0 && sh_audio->channels >= 5) {
          reorder_channel_nch(buf, AF_CHANNEL_LAYOUT_WAVEEX_DEFAULT,
                              AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
                              sh_audio->channels,
                              size_out / sh_audio->samplesize,
                              sh_audio->samplesize);
        }
//        len=size_out;
  return size_out;
}
