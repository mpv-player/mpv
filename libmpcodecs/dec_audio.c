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

int init_audio_codec(sh_audio_t *sh_audio)
{
  unsigned i;

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

  mp_msg(MSGT_DECAUDIO,MSGL_INFO,"AUDIO: %d Hz, %d ch, sfmt: 0x%X (%d bps), ratio: %d->%d (%3.1f kbit)\n",
	sh_audio->samplerate,sh_audio->channels,
	sh_audio->sample_format,sh_audio->samplesize,
        sh_audio->i_bps,sh_audio->o_bps,sh_audio->i_bps*8*0.001);
  
  return 1;
}

int init_audio(sh_audio_t *sh_audio,char* codecname,char* afm,int status){
    unsigned int orig_fourcc=sh_audio->wf?sh_audio->wf->wFormatTag:0;
    sh_audio->codec=NULL;
    while(1){
	int i;
	// restore original fourcc:
	if(sh_audio->wf) sh_audio->wf->wFormatTag=i=orig_fourcc;
	if(!(sh_audio->codec=find_codec(sh_audio->format,
          sh_audio->wf?(&i):NULL, sh_audio->codec,1) )) break;
	if(sh_audio->wf) sh_audio->wf->wFormatTag=i;
	// ok we found one codec
	if(sh_audio->codec->flags&CODECS_FLAG_SELECTED) continue; // already tried & failed
	if(codecname && strcmp(sh_audio->codec->name,codecname)) continue; // -ac
	if(afm && strcmp(sh_audio->codec->drv,afm)) continue; // afm doesn't match
	if(sh_audio->codec->status<status) continue; // too unstable
	sh_audio->codec->flags|=CODECS_FLAG_SELECTED; // tagging it
	// ok, it matches all rules, let's find the driver!
	for (i=0; mpcodecs_ad_drivers[i] != NULL; i++)
	    if(!strcmp(mpcodecs_ad_drivers[i]->info->short_name,sh_audio->codec->drv)) break;
	mpadec=mpcodecs_ad_drivers[i];
	if(!mpadec){ // driver not available (==compiled in)
            mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_AudioCodecFamilyNotAvailableStr,
        	sh_audio->codec->name, sh_audio->codec->drv);
	    continue;
	}
	// it's available, let's try to init!
	// init()
	mp_msg(MSGT_DECAUDIO,MSGL_INFO,MSGTR_OpeningAudioDecoder,mpadec->info->short_name,mpadec->info->name);
	if(!init_audio_codec(sh_audio)){
	    mp_msg(MSGT_DECAUDIO,MSGL_INFO,MSGTR_ADecoderInitFailed);
	    continue; // try next...
	}
	// Yeah! We got it!
	return 1;
    }
    return 0;
}

int init_best_audio_codec(sh_audio_t *sh_audio,char** audio_codec_list,char** audio_fm_list){
char* ac_l_default[2]={"",(char*)NULL};
// hack:
if(!audio_codec_list) audio_codec_list=ac_l_default;
// Go through the codec.conf and find the best codec...
sh_audio->inited=0;
codecs_reset_selection(1);
while(!sh_audio->inited && *audio_codec_list){
  char* audio_codec=*(audio_codec_list++);
  if(audio_codec[0]){
    if(audio_codec[0]=='-'){
      // disable this codec:
      select_codec(audio_codec+1,1);
    } else {
      // forced codec by name:
      mp_msg(MSGT_DECAUDIO,MSGL_INFO,MSGTR_ForcedAudioCodec,audio_codec);
      init_audio(sh_audio,audio_codec,NULL,-1);
    }
  } else {
    int status;
    // try in stability order: UNTESTED, WORKING, BUGGY. never try CRASHING.
    if(audio_fm_list){
      char** fmlist=audio_fm_list;
      // try first the preferred codec families:
      while(!sh_audio->inited && *fmlist){
        char* audio_fm=*(fmlist++);
	mp_msg(MSGT_DECAUDIO,MSGL_INFO,MSGTR_TryForceAudioFmtStr,audio_fm);
	for(status=CODECS_STATUS__MAX;status>=CODECS_STATUS__MIN;--status)
	    if(init_audio(sh_audio,NULL,audio_fm,status)) break;
      }
    }
    if(!sh_audio->inited)
	for(status=CODECS_STATUS__MAX;status>=CODECS_STATUS__MIN;--status)
	    if(init_audio(sh_audio,NULL,NULL,status)) break;
  }
}

if(!sh_audio->inited){
    mp_msg(MSGT_DECAUDIO,MSGL_HINT, MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_CantFindAudioCodec,sh_audio->format);
    return 0; // failed
}

mp_msg(MSGT_DECAUDIO,MSGL_INFO,"Selected audio codec: [%s] afm:%s (%s)\n",
    sh_audio->codec->name,sh_audio->codec->drv,sh_audio->codec->info);
return 1; // success
}


int init_best_audio_codec_old(sh_audio_t *sh_audio,char* audio_codec,char* audio_fm){
  // Go through the codec.conf and find the best codec...
  sh_audio->codec=NULL;
  if(audio_fm) mp_msg(MSGT_DECAUDIO,MSGL_INFO,MSGTR_TryForceAudioFmtStr,audio_fm);
  while(1){
    sh_audio->codec=find_codec(sh_audio->format,NULL,sh_audio->codec,1);
    if(!sh_audio->codec){
      if(audio_fm) {
        sh_audio->codec=NULL; /* re-search */
        mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_CantFindAfmtFallback);
        audio_fm=NULL;
        continue;
      }
      mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_CantFindAudioCodec,sh_audio->format);
      mp_msg(MSGT_DECAUDIO,MSGL_HINT, MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
      return 0;
    }
    if(audio_codec && strcmp(sh_audio->codec->name,audio_codec)) continue;
    if(audio_fm && strcmp(sh_audio->codec->drv,audio_fm)) continue;
    mp_msg(MSGT_DECAUDIO,MSGL_INFO,"%s audio codec: [%s] afm:%s (%s)\n",
	audio_codec?mp_gettext("Forcing"):mp_gettext("Detected"),sh_audio->codec->name,sh_audio->codec->drv,sh_audio->codec->info);
    break;
  }
  // found it...
  if(!init_audio_codec(sh_audio)){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_CouldntInitAudioCodec);
    return 0;
  }
  mp_msg(MSGT_DECAUDIO,MSGL_INFO,"AUDIO: %d Hz, %d ch, sfmt: 0x%X (%d bps), ratio: %d->%d (%3.1f kbit)\n",
	sh_audio->samplerate,sh_audio->channels,
	sh_audio->sample_format,sh_audio->samplesize,
        sh_audio->i_bps,sh_audio->o_bps,sh_audio->i_bps*8*0.001);
  return 1; // success!
}

void uninit_audio(sh_audio_t *sh_audio)
{
    if(sh_audio->inited){
	mp_msg(MSGT_DECAUDIO,MSGL_V,MSGTR_UninitAudioStr,sh_audio->codec->drv);
	mpadec->uninit(sh_audio);
	sh_audio->inited=0;
    }
    if(sh_audio->a_buffer) free(sh_audio->a_buffer);
    sh_audio->a_buffer=NULL;
    if(sh_audio->a_in_buffer) free(sh_audio->a_in_buffer);
    sh_audio->a_in_buffer=NULL;
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
