#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

extern int verbose; // defined in mplayer.c

#include "stream.h"
#include "demuxer.h"

#include "codec-cfg.h"
#include "stheader.h"

#include "dec_audio.h"
#include "ad.h"
#include "../libao2/afmt.h"

#ifdef USE_FAKE_MONO
int fakemono=0;
#endif
/* used for ac3surround decoder - set using -channels option */
int audio_output_channels = 2;

static ad_functions_t* mpadec;

void afm_help(){
    int i;
    mp_msg(MSGT_DECAUDIO,MSGL_INFO,MSGTR_AvailableAudioFm);
    mp_msg(MSGT_DECAUDIO,MSGL_INFO,"    afm:    info:  (comment)\n");
    for (i=0; mpcodecs_ad_drivers[i] != NULL; i++)
      if(mpcodecs_ad_drivers[i]->info->comment && mpcodecs_ad_drivers[i]->info->comment[0])
	mp_msg(MSGT_DECAUDIO,MSGL_INFO,"%9s  %s (%s)\n",
	    mpcodecs_ad_drivers[i]->info->short_name,
	    mpcodecs_ad_drivers[i]->info->name,
	    mpcodecs_ad_drivers[i]->info->comment);
      else
	mp_msg(MSGT_DECAUDIO,MSGL_INFO,"%9s  %s\n",
	    mpcodecs_ad_drivers[i]->info->short_name,
	    mpcodecs_ad_drivers[i]->info->name);
}

int init_audio(sh_audio_t *sh_audio)
{
  unsigned i;
  for (i=0; mpcodecs_ad_drivers[i] != NULL; i++)
//    if(mpcodecs_ad_drivers[i]->info->id==sh_audio->codec->driver){
    if(!strcmp(mpcodecs_ad_drivers[i]->info->short_name,sh_audio->codec->drv)){
	mpadec=mpcodecs_ad_drivers[i]; break;
    }
  if(!mpadec){
      mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_AudioCodecFamilyNotAvailableStr,
          sh_audio->codec->name, sh_audio->codec->drv);
      return 0; // no such driver
  }
  
  mp_msg(MSGT_DECAUDIO,MSGL_INFO,MSGTR_OpeningAudioDecoder,mpadec->info->short_name,mpadec->info->name);

  // reset in/out buffer size/pointer:
  sh_audio->a_buffer_size=0;
  sh_audio->a_buffer=NULL;
  sh_audio->a_in_buffer_size=0;
  sh_audio->a_in_buffer=NULL;

  // Set up some common usefull defaults. ad->preinit() can override these:
  
  sh_audio->samplesize=2;
#ifdef WORDS_BIGENDIAN
  sh_audio->sample_format=AFMT_S16_BE;
#else
  sh_audio->sample_format=AFMT_S16_LE;
#endif
  sh_audio->samplerate=0;
  sh_audio->i_bps=0;  // input rate (bytes/sec)
  sh_audio->o_bps=0;  // output rate (bytes/sec)

  sh_audio->audio_out_minsize=8192;/* default size, maybe not enough for Win32/ACM*/
  sh_audio->audio_in_minsize=0;
  
  if(!mpadec->preinit(sh_audio))
  {
      mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_ADecoderPreinitFailed);
      return 0;
  }

/* allocate audio in buffer: */
  if(sh_audio->audio_in_minsize>0){
      sh_audio->a_in_buffer_size=sh_audio->audio_in_minsize;
      mp_msg(MSGT_DECAUDIO,MSGL_V,MSGTR_AllocatingBytesForInputBuffer,
          sh_audio->a_in_buffer_size);
      sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
      memset(sh_audio->a_in_buffer,0,sh_audio->a_in_buffer_size);
      sh_audio->a_in_buffer_len=0;
  }

/* allocate audio out buffer: */
  sh_audio->a_buffer_size=sh_audio->audio_out_minsize+MAX_OUTBURST; /* worst case calc.*/

  mp_msg(MSGT_DECAUDIO,MSGL_V,MSGTR_AllocatingBytesForOutputBuffer,
      sh_audio->audio_out_minsize,MAX_OUTBURST,sh_audio->a_buffer_size);

  sh_audio->a_buffer=malloc(sh_audio->a_buffer_size);
  if(!sh_audio->a_buffer){
      mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_CantAllocAudioBuf);
      return 0;
  }
  memset(sh_audio->a_buffer,0,sh_audio->a_buffer_size);
  sh_audio->a_buffer_len=0;

  if(!mpadec->init(sh_audio)){
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,MSGTR_ADecoderInitFailed);
      uninit_audio(sh_audio); // free buffers
      return 0;
  }

  sh_audio->inited=1;
  
  if(!sh_audio->channels || !sh_audio->samplerate){
    mp_msg(MSGT_DECAUDIO,MSGL_WARN,MSGTR_UnknownAudio);
    uninit_audio(sh_audio); // free buffers
    return 0;
  }

  if(!sh_audio->o_bps)
  sh_audio->o_bps=sh_audio->channels*sh_audio->samplerate*sh_audio->samplesize;
  
  return 1;
}

void uninit_audio(sh_audio_t *sh_audio)
{
    if(sh_audio->a_buffer) free(sh_audio->a_buffer);
    sh_audio->a_buffer=NULL;
    if(sh_audio->a_in_buffer) free(sh_audio->a_in_buffer);
    sh_audio->a_in_buffer=NULL;
    if(!sh_audio->inited) return;
    mp_msg(MSGT_DECAUDIO,MSGL_V,MSGTR_UninitAudioStr,sh_audio->codec->drv);
    mpadec->uninit(sh_audio);
    sh_audio->inited=0;
}

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  if(sh_audio->inited) 
    return mpadec->decode_audio(sh_audio,buf,minlen,maxlen);
  else
    return -1;
}

void resync_audio_stream(sh_audio_t *sh_audio)
{
  sh_audio->a_in_buffer_len=0;        // clear audio input buffer
  if(!sh_audio->inited) return;
  mpadec->control(sh_audio,ADCTRL_RESYNC_STREAM,NULL);
}

void skip_audio_frame(sh_audio_t *sh_audio)
{
  if(!sh_audio->inited) return;
  if(mpadec->control(sh_audio,ADCTRL_SKIP_FRAME,NULL)==CONTROL_TRUE) return;
  // default skip code:
  ds_fill_buffer(sh_audio->ds);  // skip block
}
