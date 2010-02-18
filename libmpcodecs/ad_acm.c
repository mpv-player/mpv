/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "libmpdemux/aviprint.h"
#include "loader/wineacm.h"

#include "ad_internal.h"
#include "osdep/timer.h"

static const ad_info_t info =
{
	"Win32/ACM decoders",
	"acm",
	"A'rpi",
	"A'rpi & Alex",
	""
};

LIBAD_EXTERN(acm)

typedef struct {
    WAVEFORMATEX *o_wf;
    HACMSTREAM handle;
} acm_context_t;

static int init(sh_audio_t *sh_audio)
{
    int ret=decode_audio(sh_audio,sh_audio->a_buffer,4096,sh_audio->a_buffer_size);
    if(ret<0){
        mp_msg(MSGT_DECAUDIO,MSGL_INFO,"ACM decoding error: %d\n",ret);
        return 0;
    }
    sh_audio->a_buffer_len=ret;
  return 1;
}

static int preinit(sh_audio_t *sh_audio)
{
    HRESULT ret;
    WAVEFORMATEX *in_fmt = sh_audio->wf;
    DWORD srcsize = 0;
    acm_context_t *priv;

    priv = malloc(sizeof(acm_context_t));
    if (!priv)
	return 0;
    sh_audio->context = priv;

    mp_msg(MSGT_WIN32, MSGL_V, "======= Win32 (ACM) AUDIO Codec init =======\n");

//    priv->handle = NULL;

    priv->o_wf = malloc(sizeof(WAVEFORMATEX));
    if (!priv->o_wf)
    {
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_ACMiniterror);
	return 0;
    }

    priv->o_wf->nChannels = in_fmt->nChannels;
    priv->o_wf->nSamplesPerSec = in_fmt->nSamplesPerSec;
    priv->o_wf->nAvgBytesPerSec = 2*in_fmt->nSamplesPerSec*in_fmt->nChannels;
    priv->o_wf->wFormatTag = WAVE_FORMAT_PCM;
    priv->o_wf->nBlockAlign = 2*in_fmt->nChannels;
    priv->o_wf->wBitsPerSample = 16;
//    priv->o_wf->wBitsPerSample = inf_fmt->wBitsPerSample;
    priv->o_wf->cbSize = 0;

    if ( mp_msg_test(MSGT_DECAUDIO,MSGL_V) )
    {
	mp_msg(MSGT_DECAUDIO, MSGL_V, "Input format:\n");
	print_wave_header(in_fmt, MSGL_V);
	mp_msg(MSGT_DECAUDIO, MSGL_V, "Output format:\n");
	print_wave_header(priv->o_wf, MSGL_V);
    }

    MSACM_RegisterDriver((const char *)sh_audio->codec->dll, in_fmt->wFormatTag, 0);
    ret = acmStreamOpen(&priv->handle, (HACMDRIVER)NULL, in_fmt,
			priv->o_wf, NULL, 0, 0, 0);
    if (ret)
    {
	if (ret == ACMERR_NOTPOSSIBLE)
	    mp_msg(MSGT_WIN32, MSGL_ERR, "ACM_Decoder: Unappropriate audio format\n");
	else
	    mp_msg(MSGT_WIN32, MSGL_ERR, "ACM_Decoder: acmStreamOpen error: %d\n",
		(int)ret);
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_ACMiniterror);
	return 0;
    }
    mp_msg(MSGT_WIN32, MSGL_V, "Audio codec opened OK! ;-)\n");

    acmStreamSize(priv->handle, in_fmt->nBlockAlign, &srcsize, ACM_STREAMSIZEF_SOURCE);
    //if ( mp_msg_test(MSGT_DECAUDIO,MSGL_V) ) printf("Audio ACM output buffer min. size: %ld (reported by codec)\n", srcsize);
    srcsize *= 2;
    //if (srcsize < MAX_OUTBURST) srcsize = MAX_OUTBURST;
    if (!srcsize)
    {
	mp_msg(MSGT_WIN32, MSGL_WARN, "Warning! ACM codec reports srcsize=0\n");
	srcsize = 16384;
    }
    // limit srcsize to 4-16kb
    //while(srcsize && srcsize<4096) srcsize*=2;
    //while(srcsize>16384) srcsize/=2;
    sh_audio->audio_out_minsize=srcsize; // audio output min. size
    mp_msg(MSGT_WIN32,MSGL_V,"Audio ACM output buffer min. size: %ld\n",srcsize);

    acmStreamSize(priv->handle, srcsize, &srcsize, ACM_STREAMSIZEF_DESTINATION);
//    if(srcsize<in_fmt->nBlockAlign) srcsize=in_fmt->nBlockAlign;

    if (!srcsize)
    {
	mp_msg(MSGT_WIN32, MSGL_WARN, "Warning! ACM codec reports srcsize=0\n");
	srcsize = 2*in_fmt->nBlockAlign;
    }

    mp_msg(MSGT_WIN32,MSGL_V,"Audio ACM input buffer min. size: %ld\n",srcsize);

    sh_audio->audio_in_minsize=2*srcsize; // audio input min. size

    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->channels=priv->o_wf->nChannels;
    sh_audio->samplerate=priv->o_wf->nSamplesPerSec;
    sh_audio->samplesize=2;

    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32/ACM audio codec init OK!\n");
    return 1;
}

static void uninit(sh_audio_t *sh)
{
    HRESULT ret;
    acm_context_t *priv = sh->context;

retry:
    ret = acmStreamClose(priv->handle, 0);

    if (ret)
    switch(ret)
    {
	case ACMERR_BUSY:
	case ACMERR_CANCELED:
	    mp_msg(MSGT_WIN32, MSGL_DBG2, "ACM_Decoder: stream busy, waiting..\n");
	    usec_sleep(100000000);
	    goto retry;
	case ACMERR_UNPREPARED:
	case ACMERR_NOTPOSSIBLE:
	    return;
	default:
	    mp_msg(MSGT_WIN32, MSGL_WARN, "ACM_Decoder: unknown error occurred: %ld\n", ret);
	    return;
    }

    MSACM_UnregisterAllDrivers();

    free(priv->o_wf);
    free(priv);
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
    ACMSTREAMHEADER ash;
    HRESULT hr;
    DWORD srcsize=0;
    DWORD len=minlen;
    acm_context_t *priv = sh_audio->context;

    acmStreamSize(priv->handle, len, &srcsize, ACM_STREAMSIZEF_DESTINATION);
    mp_msg(MSGT_WIN32,MSGL_DBG3,"acm says: srcsize=%ld  (buffsize=%d)  out_size=%ld\n",srcsize,sh_audio->a_in_buffer_size,len);

    if(srcsize<sh_audio->wf->nBlockAlign){
       srcsize=sh_audio->wf->nBlockAlign;
       acmStreamSize(priv->handle, srcsize, &len, ACM_STREAMSIZEF_SOURCE);
       if(len>maxlen) len=maxlen;
    }

//    if(srcsize==0) srcsize=((WAVEFORMATEX *)&sh_audio->o_wf_ext)->nBlockAlign;
    if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
    if(sh_audio->a_in_buffer_len<srcsize){
      sh_audio->a_in_buffer_len+=
        demux_read_data(sh_audio->ds,&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len],
        srcsize-sh_audio->a_in_buffer_len);
    }
    mp_msg(MSGT_WIN32,MSGL_DBG3,"acm convert %d -> %ld bytes\n",sh_audio->a_in_buffer_len,len);
    memset(&ash, 0, sizeof(ash));
    ash.cbStruct=sizeof(ash);
    ash.fdwStatus=0;
    ash.dwUser=0;
    ash.pbSrc=sh_audio->a_in_buffer;
    ash.cbSrcLength=sh_audio->a_in_buffer_len;
    ash.pbDst=buf;
    ash.cbDstLength=len;
    hr=acmStreamPrepareHeader(priv->handle,&ash,0);
    if(hr){
      mp_msg(MSGT_WIN32,MSGL_V,"ACM_Decoder: acmStreamPrepareHeader error %d\n",(int)hr);
      return -1;
    }
    hr=acmStreamConvert(priv->handle,&ash,0);
    if(hr){
      mp_msg(MSGT_WIN32,MSGL_DBG2,"ACM_Decoder: acmStreamConvert error %d\n",(int)hr);
      switch(hr)
      {
	case ACMERR_NOTPOSSIBLE:
	case ACMERR_UNPREPARED:
	    mp_msg(MSGT_WIN32, MSGL_DBG2, "ACM_Decoder: acmStreamConvert error: probarly not initialized!\n");
      }
//      return -1;
    }
    mp_msg(MSGT_WIN32,MSGL_DBG2,"acm converted %ld -> %ld\n",ash.cbSrcLengthUsed,ash.cbDstLengthUsed);
    if(ash.cbSrcLengthUsed>=sh_audio->a_in_buffer_len){
      sh_audio->a_in_buffer_len=0;
    } else {
      sh_audio->a_in_buffer_len-=ash.cbSrcLengthUsed;
      memcpy(sh_audio->a_in_buffer,&sh_audio->a_in_buffer[ash.cbSrcLengthUsed],sh_audio->a_in_buffer_len);
    }
    len=ash.cbDstLengthUsed;
    hr=acmStreamUnprepareHeader(priv->handle,&ash,0);
    if(hr){
      mp_msg(MSGT_WIN32,MSGL_V,"ACM_Decoder: acmStreamUnprepareHeader error %d\n",(int)hr);
    }
    return len;
}
