#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"

#include "codec-cfg.h"
#include "stheader.h"

#include "dec_audio.h"
#include "ad.h"
#include "../libao2/afmt.h"

#include "../libaf/af.h"

#ifdef DYNAMIC_PLUGINS
#include <dlfcn.h>
#endif

#ifdef USE_FAKE_MONO
int fakemono=0;
#endif
/* used for ac3surround decoder - set using -channels option */
int audio_output_channels = 2;
af_cfg_t af_cfg; // Configuration for audio filters

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
  sh_audio->channels=0;
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

  mp_msg(MSGT_DECAUDIO,MSGL_INFO,"AUDIO: %d Hz, %d ch, %d bit (0x%X), ratio: %d->%d (%3.1f kbit)\n",
	sh_audio->samplerate,sh_audio->channels,
	sh_audio->samplesize*8,sh_audio->sample_format,
        sh_audio->i_bps,sh_audio->o_bps,sh_audio->i_bps*8*0.001);

  sh_audio->a_out_buffer_size=sh_audio->a_buffer_size;
  sh_audio->a_out_buffer=sh_audio->a_buffer;
  sh_audio->a_out_buffer_len=sh_audio->a_buffer_len;
  
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
#ifdef DYNAMIC_PLUGINS
	if (!mpadec)
	{
	    /* try to open shared decoder plugin */
	    int buf_len;
	    char *buf;
	    ad_functions_t *funcs_sym;
	    ad_info_t *info_sym;
	    
	    buf_len = strlen(LIBDIR)+strlen(sh_audio->codec->drv)+16;
	    buf = malloc(buf_len);
	    if (!buf)
		break;
	    snprintf(buf, buf_len, "%s/mplayer/ad_%s.so", LIBDIR, sh_audio->codec->drv);
	    mp_msg(MSGT_DECAUDIO, MSGL_DBG2, "Trying to open external plugin: %s\n", buf);
	    sh_audio->dec_handle = dlopen(buf, RTLD_LAZY);
	    if (!sh_audio->dec_handle)
		break;
	    snprintf(buf, buf_len, "mpcodecs_ad_%s", sh_audio->codec->drv);
	    funcs_sym = dlsym(sh_audio->dec_handle, buf);
	    if (!funcs_sym || !funcs_sym->info || !funcs_sym->preinit ||
		!funcs_sym->init || !funcs_sym->uninit || !funcs_sym->control ||
		!funcs_sym->decode_audio)
		break;
	    info_sym = funcs_sym->info;
	    if (strcmp(info_sym->short_name, sh_audio->codec->drv))
		break;
	    free(buf);
	    mpadec = funcs_sym;
	    mp_msg(MSGT_DECAUDIO, MSGL_V, "Using external decoder plugin (%s/mplayer/ad_%s.so)!\n",
		LIBDIR, sh_audio->codec->drv);
	}
#endif
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

extern char *get_path(char *filename);

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

void uninit_audio(sh_audio_t *sh_audio)
{
    if(sh_audio->afilter){
	mp_msg(MSGT_DECAUDIO,MSGL_V,"Uninit audio filters...\n");
	af_uninit(sh_audio->afilter);
	sh_audio->afilter=NULL;
    }
    if(sh_audio->inited){
	mp_msg(MSGT_DECAUDIO,MSGL_V,MSGTR_UninitAudioStr,sh_audio->codec->drv);
	mpadec->uninit(sh_audio);
#ifdef DYNAMIC_PLUGINS
	if (sh_audio->dec_handle)
	    dlclose(sh_audio->dec_handle);
#endif
	sh_audio->inited=0;
    }
    if(sh_audio->a_out_buffer!=sh_audio->a_buffer) free(sh_audio->a_out_buffer);
    sh_audio->a_out_buffer=NULL;
    if(sh_audio->a_buffer) free(sh_audio->a_buffer);
    sh_audio->a_buffer=NULL;
    if(sh_audio->a_in_buffer) free(sh_audio->a_in_buffer);
    sh_audio->a_in_buffer=NULL;
}

 /* Init audio filters */
int init_audio_filters(sh_audio_t *sh_audio, 
	int in_samplerate, int in_channels, int in_format, int in_bps,
	int out_samplerate, int out_channels, int out_format, int out_bps,
	int out_minsize, int out_maxsize){
  af_stream_t* afs=malloc(sizeof(af_stream_t));
  memset(afs,0,sizeof(af_stream_t));

  // input format: same as codec's output format:
  afs->input.rate   = in_samplerate;
  afs->input.nch    = in_channels;
  afs->input.format = af_format_decode(in_format);
  afs->input.bps    = in_bps;

  // output format: same as ao driver's input format (if missing, fallback to input)
  afs->output.rate   = out_samplerate ? out_samplerate : afs->input.rate;
  afs->output.nch    = out_channels ? out_channels : afs->input.nch;
  afs->output.format = af_format_decode(out_format ? out_format : afs->input.format);
  afs->output.bps    = out_bps ? out_bps : afs->input.bps;

  // filter config:  
  memcpy(&afs->cfg,&af_cfg,sizeof(af_cfg_t));
  
  mp_msg(MSGT_DECAUDIO, MSGL_INFO, "Building audio filter chain for %dHz/%dch/%dbit -> %dHz/%dch/%dbit...\n",
      afs->input.rate,afs->input.nch,afs->input.bps*8,
      afs->output.rate,afs->output.nch,afs->output.bps*8);
  
  // let's autoprobe it!
  if(0 != af_init(afs)){
    free(afs);
    return 0; // failed :(
  }
  
  // allocate the a_out_* buffers:
  if(out_maxsize<out_minsize) out_maxsize=out_minsize;
  if(out_maxsize<8192) out_maxsize=MAX_OUTBURST; // not sure this is ok

  sh_audio->a_out_buffer_size=out_maxsize;
  sh_audio->a_out_buffer=malloc(sh_audio->a_out_buffer_size);
  memset(sh_audio->a_out_buffer,0,sh_audio->a_out_buffer_size);
  sh_audio->a_out_buffer_len=0;
  
  // ok!
  sh_audio->afilter=(void*)afs;
  return 1;
}

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  int declen;
  af_data_t  afd;  // filter input
  af_data_t* pafd; // filter output

  if(!sh_audio->inited) return -1; // no codec
  if(!sh_audio->afilter){
      // no filter, just decode:
      // FIXME: don't drop initial decoded data in a_buffer!
      return mpadec->decode_audio(sh_audio,buf,minlen,maxlen);
  }
  
//  declen=af_inputlen(sh_audio->afilter,minlen);
  declen=af_calc_insize_constrained(sh_audio->afilter,minlen,maxlen,
      sh_audio->a_buffer_size-sh_audio->audio_out_minsize);

  mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"\ndecaudio: minlen=%d maxlen=%d declen=%d (max=%d)\n",
      minlen, maxlen, declen, sh_audio->a_buffer_size);

  if(declen<=0) return -1; // error!

  // limit declen to buffer size: - DONE by af_calc_insize_constrained
//  if(declen>sh_audio->a_buffer_size) declen=sh_audio->a_buffer_size;

  // decode if needed:
  while(declen>sh_audio->a_buffer_len){
      int len=declen-sh_audio->a_buffer_len;
      int maxlen=sh_audio->a_buffer_size-sh_audio->a_buffer_len;

      mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"decaudio: decoding %d bytes, max: %d (%d)\n",
        len, maxlen, sh_audio->audio_out_minsize);

      if(maxlen<sh_audio->audio_out_minsize) break; // don't overflow buffer!
      // not enough decoded data waiting, decode 'len' bytes more:
      len=mpadec->decode_audio(sh_audio,
          sh_audio->a_buffer+sh_audio->a_buffer_len, len, maxlen);
      if(len<=0) break; // EOF?
      sh_audio->a_buffer_len+=len;
  }
  if(declen>sh_audio->a_buffer_len)
    declen=sh_audio->a_buffer_len; // still no enough data (EOF) :(

  // round to whole samples:
//  declen/=sh_audio->samplesize*sh_audio->channels;
//  declen*=sh_audio->samplesize*sh_audio->channels;

  // run the filters:
  afd.audio=sh_audio->a_buffer;
  afd.len=declen;
  afd.rate=sh_audio->samplerate;
  afd.nch=sh_audio->channels;
  afd.format=af_format_decode(sh_audio->sample_format);
  afd.bps=sh_audio->samplesize;
  //pafd=&afd;
//  printf("\nAF: %d --> ",declen);
  pafd=af_play(sh_audio->afilter,&afd);
//  printf("%d  \n",pafd->len);

  if(!pafd) return -1; // error

  mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"decaudio: declen=%d out=%d (max %d)\n",
      declen, pafd->len, maxlen);
  
  // copy filter==>out:
  if(maxlen < pafd->len)
    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"%i bytes of audio data lost due to buffer overflow, len = %i", pafd->len - maxlen,pafd->len);
  else
    maxlen=pafd->len;
  memmove(buf, pafd->audio, maxlen);
  
  // remove processed data from decoder buffer:
  sh_audio->a_buffer_len-=declen;
  if(sh_audio->a_buffer_len>0)
    memmove(sh_audio->a_buffer, sh_audio->a_buffer+declen, sh_audio->a_buffer_len);
  
  return maxlen;
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
