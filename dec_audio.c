
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

extern int verbose; // defined in mplayer.c

#ifdef USE_FAKE_MONO
int fakemono=0;
#endif

#include "stream.h"
#include "demuxer.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "codec-cfg.h"
#include "stheader.h"

#include "mp3lib/mp3.h"
#include "libac3/ac3.h"

#include "alaw.c"
#include "xa/xa_gsm.h"

#include "loader/DirectShow/DS_AudioDec.h"

static sh_audio_t* ac3_audio_sh=NULL;

// AC3 decoder buffer callback:
static void ac3_fill_buffer(uint8_t **start,uint8_t **end){
    int len=ds_get_packet(ac3_audio_sh->ds,start);
    //printf("<ac3:%d>\n",len);
    if(len<0)
          *start = *end = NULL;
    else
          *end = *start + len;
}


int init_audio(sh_audio_t *sh_audio){

int driver=sh_audio->codec->driver;

extern int init_acm_audio_codec(sh_audio_t *sh_audio);
extern int acm_decode_audio(sh_audio_t *sh_audio, void* a_buffer,int len);

sh_audio->samplesize=2;
sh_audio->samplerate=0;
//sh_audio->pcm_bswap=0;

sh_audio->a_buffer_size=16384;  // default size, maybe not enough for Win32/ACM
sh_audio->a_buffer=NULL;

if(driver==4){
  // Win32 ACM audio codec:
  if(init_acm_audio_codec(sh_audio)){
    sh_audio->channels=sh_audio->o_wf.nChannels;
    sh_audio->samplerate=sh_audio->o_wf.nSamplesPerSec;
    if(sh_audio->a_buffer_size<sh_audio->audio_out_minsize+OUTBURST)
        sh_audio->a_buffer_size=sh_audio->audio_out_minsize+OUTBURST;
  } else {
    printf("Could not load/initialize Win32/ACM AUDIO codec (missing DLL file?)\n");
    driver=0;
  }
}

if(driver==7){
#ifndef USE_DIRECTSHOW
  printf("Compiled without DirectShow support -> force nosound :(\n");
  driver=0;
#else
  // Win32 DShow audio codec:
//  printf("DShow_audio: channs=%d  rate=%d\n",sh_audio->channels,sh_audio->samplerate);

  if(DS_AudioDecoder_Open(sh_audio->codec->dll,&sh_audio->codec->guid,sh_audio->wf)){
    printf("ERROR: Could not load/initialize Win32/DirctShow AUDIO codec: %s\n",sh_audio->codec->dll);
    driver=0;
  } else {
    sh_audio->channels=sh_audio->wf->nChannels;
    sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
    sh_audio->audio_in_minsize=2*sh_audio->wf->nBlockAlign;
    if(sh_audio->audio_in_minsize<8192) sh_audio->audio_in_minsize=8192;
    sh_audio->a_in_buffer_size=sh_audio->audio_in_minsize;
    sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
    sh_audio->a_in_buffer_len=0;
  }
#endif
}

if(!driver) return 0;

// allocate audio out buffer:
sh_audio->a_buffer=malloc(sh_audio->a_buffer_size);
memset(sh_audio->a_buffer,0,sh_audio->a_buffer_size);
sh_audio->a_buffer_len=0;

switch(driver){
case 4: {
    int ret=acm_decode_audio(sh_audio,sh_audio->a_buffer,sh_audio->a_buffer_size);
    if(ret<0){
        printf("ACM decoding error: %d\n",ret);
        driver=0;
    }
    sh_audio->a_buffer_len=ret;
    break;
}
case 2: {
    // AVI PCM Audio:
    WAVEFORMATEX *h=sh_audio->wf;
    sh_audio->channels=h->nChannels;
    sh_audio->samplerate=h->nSamplesPerSec;
    sh_audio->samplesize=(h->wBitsPerSample+7)/8;
    break;
}
case 8: {
    // DVD PCM Audio:
    sh_audio->channels=2;
    sh_audio->samplerate=48000;
//    sh_audio->pcm_bswap=1;
    break;
}
case 3: {
  // Dolby AC3 audio:
  ac3_audio_sh=sh_audio; // save sh_audio for the callback:
  ac3_config.fill_buffer_callback = ac3_fill_buffer;
  ac3_config.num_output_ch = 2;
  ac3_config.flags = 0;
#ifdef HAVE_MMX
  ac3_config.flags |= AC3_MMX_ENABLE;
#endif
#ifdef HAVE_3DNOW
  ac3_config.flags |= AC3_3DNOW_ENABLE;
#endif
  ac3_init();
  sh_audio->ac3_frame = ac3_decode_frame();
  if(sh_audio->ac3_frame){
    sh_audio->samplerate=((ac3_frame_t*)sh_audio->ac3_frame)->sampling_rate;
    sh_audio->channels=2;
  } else {
    driver=0; // bad frame -> disable audio
  }
  break;
}
case 5: {
  // aLaw audio codec:
  Gen_aLaw_2_Signed(); // init table
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  break;
}
case 6: {
  // MS-GSM audio codec:
  GSM_Init();
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  break;
}
case 1: {
  // MPEG Audio:
#ifdef USE_FAKE_MONO
  MP3_Init(fakemono);
#else
  MP3_Init();
#endif
  MP3_samplerate=MP3_channels=0;
//  printf("[\n");
  sh_audio->a_buffer_len=MP3_DecodeFrame(sh_audio->a_buffer,-1);
//  printf("]\n");
  sh_audio->channels=2; // hack
  sh_audio->samplerate=MP3_samplerate;
  break;
}
}

if(!sh_audio->channels || !sh_audio->samplerate){
  printf("Unknown/missing audio format, using nosound\n");
  driver=0;
}

  if(!driver){
      if(sh_audio->a_buffer) free(sh_audio->a_buffer);
      sh_audio->o_bps=0;
      return 0;
  }

  sh_audio->o_bps=sh_audio->channels*sh_audio->samplerate*sh_audio->samplesize;
  return driver;
}

// Audio decoding

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int maxlen){
    int len=-1;
    switch(sh_audio->codec->driver){
      case 1: // MPEG layer 2 or 3
        len=MP3_DecodeFrame(buf,-1);
        sh_audio->channels=2; // hack
        break;
      case 2: // AVI PCM
      { len=demux_read_data(sh_audio->ds,buf,OUTBURST);
        break;
      }
      case 8: // DVD PCM
      { int j;
        len=demux_read_data(sh_audio->ds,buf,OUTBURST);
          //if(i&1){ printf("Warning! pcm_audio_size&1 !=0  (%d)\n",i);i&=~1; }
          for(j=0;j<len;j+=2){
            char x=buf[j];
            buf[j]=buf[j+1];
            buf[j+1]=x;
          }
        break;
      }
      case 5:  // aLaw decoder
      { int l=demux_read_data(sh_audio->ds,buf,OUTBURST/2);
        unsigned short *d=(unsigned short *) buf;
        unsigned char *s=buf;
        len=2*l;
        while(l>0){
          --l;
          d[l]=xa_alaw_2_sign[s[l]];
        }
        break;
      }
      case 6:  // MS-GSM decoder
      { unsigned char buf[65]; // 65 bytes / frame
            len=0;
            while(len<OUTBURST){
                if(demux_read_data(sh_audio->ds,buf,65)!=65) break; // EOF
                XA_MSGSM_Decoder(buf,(unsigned short *) buf); // decodes 65 byte -> 320 short
//  		XA_GSM_Decoder(buf,(unsigned short *) &sh_audio->a_buffer[sh_audio->a_buffer_len]); // decodes 33 byte -> 160 short
                len+=2*320;
            }
        break;
      }
      case 3: // AC3 decoder
        //printf("{1:%d}",avi_header.idx_pos);fflush(stdout);
        if(!sh_audio->ac3_frame) sh_audio->ac3_frame=ac3_decode_frame();
        //printf("{2:%d}",avi_header.idx_pos);fflush(stdout);
        if(sh_audio->ac3_frame){
          len = 256 * 6 *sh_audio->channels*sh_audio->samplesize;
          memcpy(buf,((ac3_frame_t*)sh_audio->ac3_frame)->audio_data,len);
          sh_audio->ac3_frame=NULL;
        }
        //printf("{3:%d}",avi_header.idx_pos);fflush(stdout);
        break;
      case 4:
      { len=acm_decode_audio(sh_audio,buf,maxlen);
        break;
      }
#ifdef USE_DIRECTSHOW
      case 7: // DirectShow
      { int ret;
        int size_in=0;
        int size_out=0;
        int srcsize=DS_AudioDecoder_GetSrcSize(maxlen);
        if(verbose>2)printf("DShow says: srcsize=%d  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,maxlen);
        if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
        if(sh_audio->a_in_buffer_len<srcsize){
          sh_audio->a_in_buffer_len+=
            demux_read_data(sh_audio->ds,&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len],
            srcsize-sh_audio->a_in_buffer_len);
        }
        DS_AudioDecoder_Convert(sh_audio->a_in_buffer,sh_audio->a_in_buffer_len,
            buf,maxlen, &size_in,&size_out);
        if(verbose>2)printf("DShow: audio %d -> %d converted  (in_buf_len=%d of %d)\n",size_in,size_out,sh_audio->a_in_buffer_len,sh_audio->a_in_buffer_size);
        if(size_in>=sh_audio->a_in_buffer_len){
          sh_audio->a_in_buffer_len=0;
        } else {
          sh_audio->a_in_buffer_len-=size_in;
          memcpy(sh_audio->a_in_buffer,&sh_audio->a_in_buffer[size_in],sh_audio->a_in_buffer_len);
        }
        len=size_out;
        break;
      }
#endif
    }
    return len;
}


