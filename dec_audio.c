
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "libao2/afmt.h"

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

#include "alaw.h"

#include "xa/xa_gsm.h"

#include "ac3-iec958.h"

#ifdef USE_DIRECTSHOW
#include "loader/DirectShow/DS_AudioDec.h"
#endif

extern int init_acm_audio_codec(sh_audio_t *sh_audio);
extern int acm_decode_audio(sh_audio_t *sh_audio, void* a_buffer,int minlen,int maxlen);


static sh_audio_t* dec_audio_sh=NULL;

// AC3 decoder buffer callback:
static void ac3_fill_buffer(uint8_t **start,uint8_t **end){
    int len=ds_get_packet(dec_audio_sh->ds,start);
    //printf("<ac3:%d>\n",len);
    if(len<0)
          *start = *end = NULL;
    else
          *end = *start + len;
}

// MP3 decoder buffer callback:
int mplayer_audio_read(char *buf,int size){
  int len;
  len=demux_read_data(dec_audio_sh->ds,buf,size);
  return len;
}

int init_audio(sh_audio_t *sh_audio){
int driver=sh_audio->codec->driver;

sh_audio->samplesize=2;
#if WORDS_BIGENDIAN
sh_audio->sample_format=AFMT_S16_BE;
#else
sh_audio->sample_format=AFMT_S16_LE;
#endif
sh_audio->samplerate=0;
//sh_audio->pcm_bswap=0;

sh_audio->a_buffer_size=0;
sh_audio->a_buffer=NULL;

sh_audio->a_in_buffer_len=0;

// setup required min. in/out buffer size:
sh_audio->audio_out_minsize=8192;// default size, maybe not enough for Win32/ACM

switch(driver){
case AFM_ACM:
#ifndef	USE_WIN32DLL
  printf("Win32/ACM audio codec disabled, or unavailable on non-x86 CPU -> force nosound :(\n");
  driver=0;
#else
  // Win32 ACM audio codec:
  if(init_acm_audio_codec(sh_audio)){
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->channels=sh_audio->o_wf.nChannels;
    sh_audio->samplerate=sh_audio->o_wf.nSamplesPerSec;
//    if(sh_audio->audio_out_minsize>16384) sh_audio->audio_out_minsize=16384;
//    sh_audio->a_buffer_size=sh_audio->audio_out_minsize;
//    if(sh_audio->a_buffer_size<sh_audio->audio_out_minsize+MAX_OUTBURST)
//        sh_audio->a_buffer_size=sh_audio->audio_out_minsize+MAX_OUTBURST;
  } else {
    printf("Could not load/initialize Win32/ACM AUDIO codec (missing DLL file?)\n");
    driver=0;
  }
#endif
  break;
case AFM_DSHOW:
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
#endif
  break;
case AFM_PCM:
case AFM_DVDPCM:
case AFM_ALAW:
  // PCM, aLaw
  sh_audio->audio_out_minsize=2048;
  break;
case AFM_AC3:
  // Dolby AC3 audio:
  sh_audio->audio_out_minsize=4*256*6;
  break;
case AFM_HWAC3:
  // Dolby AC3 audio:
  sh_audio->audio_out_minsize=4*256*6;
  sh_audio->sample_format = AFMT_AC3;
  break;
case AFM_GSM:
  // MS-GSM audio codec:
  sh_audio->audio_out_minsize=4*320;
  break;
case AFM_MPEG:
  // MPEG Audio:
  sh_audio->audio_out_minsize=4608;
  break;
}

if(!driver) return 0;

// allocate audio out buffer:
sh_audio->a_buffer_size=sh_audio->audio_out_minsize+MAX_OUTBURST; // worst case calc.

if(verbose) printf("dec_audio: Allocating %d + %d = %d bytes for output buffer\n",
    sh_audio->audio_out_minsize,MAX_OUTBURST,sh_audio->a_buffer_size);

sh_audio->a_buffer=malloc(sh_audio->a_buffer_size);
if(!sh_audio->a_buffer){
    fprintf(stderr,"Cannot allocate audio out buffer\n");
    return 0;
}
memset(sh_audio->a_buffer,0,sh_audio->a_buffer_size);
sh_audio->a_buffer_len=0;

switch(driver){
#ifdef USE_WIN32DLL
case AFM_ACM: {
    int ret=acm_decode_audio(sh_audio,sh_audio->a_buffer,4096,sh_audio->a_buffer_size);
    if(ret<0){
        printf("ACM decoding error: %d\n",ret);
        driver=0;
    }
    sh_audio->a_buffer_len=ret;
    break;
}
#endif
case AFM_PCM: {
    // AVI PCM Audio:
    WAVEFORMATEX *h=sh_audio->wf;
    sh_audio->i_bps=h->nAvgBytesPerSec;
    sh_audio->channels=h->nChannels;
    sh_audio->samplerate=h->nSamplesPerSec;
    sh_audio->samplesize=(h->wBitsPerSample+7)/8;
    switch(sh_audio->format){ // hardware formats:
    case 0x6:  sh_audio->sample_format=AFMT_A_LAW;break;
    case 0x7:  sh_audio->sample_format=AFMT_MU_LAW;break;
    case 0x11: sh_audio->sample_format=AFMT_IMA_ADPCM;break;
    case 0x50: sh_audio->sample_format=AFMT_MPEG;break;
//    case 0x2000: sh_audio->sample_format=AFMT_AC3;
    default: sh_audio->sample_format=(sh_audio->samplesize==2)?AFMT_S16_LE:AFMT_U8;
    }
    break;
}
case AFM_DVDPCM: {
    // DVD PCM Audio:
    sh_audio->channels=2;
    sh_audio->samplerate=48000;
    sh_audio->i_bps=2*2*48000;
//    sh_audio->pcm_bswap=1;
    break;
}
case AFM_AC3: {
  // Dolby AC3 audio:
  dec_audio_sh=sh_audio; // save sh_audio for the callback:
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
    ac3_frame_t* fr=(ac3_frame_t*)sh_audio->ac3_frame;
    sh_audio->samplerate=fr->sampling_rate;
    sh_audio->channels=2;
    // 1 frame: 6*256 samples     1 sec: sh_audio->samplerate samples
    //sh_audio->i_bps=fr->frame_size*fr->sampling_rate/(6*256);
    sh_audio->i_bps=fr->bit_rate*(1000/8);
  } else {
    driver=0; // bad frame -> disable audio
  }
  break;
}
case AFM_HWAC3: {
  unsigned char *buffer;		    
  struct hwac3info ai;
  int len, skipped;
  len = ds_get_packet(sh_audio->ds, &buffer);
  if(ac3_iec958_parse_syncinfo(buffer, len, &ai, &skipped) < 0) {
      fprintf(stderr, "AC3 stream not valid.\n");
      driver = 0;
      break;
  }
  if(ai.samplerate != 48000) {
      fprintf(stderr, "Only 48000 Hz streams supported.\n");
      driver = 0;
      break;
  }
  sh_audio->samplerate=ai.samplerate;
//  sh_audio->samplesize=ai.framesize;
  sh_audio->channels=1;
  sh_audio->i_bps=ai.bitrate*(1000/8);
  sh_audio->ac3_frame=malloc(6144);
  break;
}
case AFM_ALAW: {
  // aLaw audio codec:
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps=sh_audio->channels*sh_audio->samplerate;
  break;
}
case AFM_GSM: {
  // MS-GSM audio codec:
  GSM_Init();
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  // decodes 65 byte -> 320 short
  // 1 sec: sh_audio->channels*sh_audio->samplerate  samples
  // 1 frame: 320 samples
  sh_audio->i_bps=65*(sh_audio->channels*sh_audio->samplerate)/320;  // 1:10
  break;
}
case AFM_MPEG: {
  // MPEG Audio:
  dec_audio_sh=sh_audio; // save sh_audio for the callback:
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
  sh_audio->i_bps=MP3_bitrate*(1000/8);
  break;
}
}

if(!sh_audio->channels || !sh_audio->samplerate){
  printf("Unknown/missing audio format, using nosound\n");
  driver=0;
}

  if(!driver){
      if(sh_audio->a_buffer) free(sh_audio->a_buffer);
      sh_audio->a_buffer=NULL;
      sh_audio->o_bps=0;
      return 0;
  }

  sh_audio->o_bps=sh_audio->channels*sh_audio->samplerate*sh_audio->samplesize;
  return driver;
}

// Audio decoding:

// Decode a single frame (mp3,acm etc) or 'minlen' bytes (pcm/alaw etc)
// buffer length is 'maxlen' bytes, it shouldn't be exceeded...

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen){
    int len=-1;
    switch(sh_audio->codec->driver){
      case AFM_MPEG: // MPEG layer 2 or 3
        len=MP3_DecodeFrame(buf,-1);
//        len=MP3_DecodeFrame(buf,3);
        break;
      case AFM_PCM: // AVI PCM
        len=demux_read_data(sh_audio->ds,buf,minlen);
        break;
      case AFM_DVDPCM: // DVD PCM
      { int j;
        len=demux_read_data(sh_audio->ds,buf,minlen);
          //if(i&1){ printf("Warning! pcm_audio_size&1 !=0  (%d)\n",i);i&=~1; }
          // swap endian:
          for(j=0;j<len;j+=2){
            char x=buf[j];
            buf[j]=buf[j+1];
            buf[j+1]=x;
          }
        break;
      }
      case AFM_ALAW:  // aLaw decoder
      { int l=demux_read_data(sh_audio->ds,buf,minlen/2);
        unsigned short *d=(unsigned short *) buf;
        unsigned char *s=buf;
        len=2*l;
        if(sh_audio->format==6){
        // aLaw
          while(l>0){ --l; d[l]=alaw2short[s[l]]; }
        } else {
        // uLaw
          while(l>0){ --l; d[l]=ulaw2short[s[l]]; }
        }
        break;
      }
      case AFM_GSM:  // MS-GSM decoder
      { unsigned char buf[65]; // 65 bytes / frame
        if(demux_read_data(sh_audio->ds,buf,65)!=65) break; // EOF
        XA_MSGSM_Decoder(buf,(unsigned short *) buf); // decodes 65 byte -> 320 short
//  	    XA_GSM_Decoder(buf,(unsigned short *) &sh_audio->a_buffer[sh_audio->a_buffer_len]); // decodes 33 byte -> 160 short
        len=2*320;
        break;
      }
      case AFM_AC3: // AC3 decoder
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
      case AFM_HWAC3: // AC3 through SPDIF
	if(demux_read_data(sh_audio->ds,sh_audio->ac3_frame, 6144) != 6144) 
	    break; //EOF 
	ac3_iec958_build_burst(1536, 0x1F, 1, buf, sh_audio->ac3_frame);
	len = 6144;
	break;
#ifdef USE_WIN32DLL
      case AFM_ACM:
//        len=sh_audio->audio_out_minsize; // optimal decoded fragment size
//        if(len<minlen) len=minlen; else
//        if(len>maxlen) len=maxlen;
//        len=acm_decode_audio(sh_audio,buf,len);
        len=acm_decode_audio(sh_audio,buf,minlen,maxlen);
        break;
#endif

#ifdef USE_DIRECTSHOW
      case AFM_DSHOW: // DirectShow
      { int size_in=0;
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
        if(verbose)
          printf("DShow: audio %d -> %d converted  (in_buf_len=%d of %d)  %d\n",size_in,size_out,sh_audio->a_in_buffer_len,sh_audio->a_in_buffer_size,ds_tell_pts(sh_audio->ds));
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

void resync_audio_stream(sh_audio_t *sh_audio){
        switch(sh_audio->codec->driver){
        case AFM_MPEG:
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
          break;
        case AFM_AC3:
          ac3_bitstream_reset();    // reset AC3 bitstream buffer
    //      if(verbose){ printf("Resyncing AC3 audio...");fflush(stdout);}
          sh_audio->ac3_frame=ac3_decode_frame(); // resync
    //      if(verbose) printf(" OK!\n");
          break;
        case AFM_ACM:
        case AFM_DSHOW:
	case AFM_HWAC3:
          sh_audio->a_in_buffer_len=0;        // reset ACM/DShow audio buffer
          break;
        }
	
}

void skip_audio_frame(sh_audio_t *sh_audio){
              switch(sh_audio->codec->driver){
                case AFM_MPEG: MP3_DecodeFrame(NULL,-2);break; // skip MPEG frame
                case AFM_AC3: sh_audio->ac3_frame=ac3_decode_frame();break; // skip AC3 frame
		case AFM_ACM:
		case AFM_DSHOW: {
		    int skip=sh_audio->wf->nBlockAlign;
		    if(skip<16){
		      skip=(sh_audio->wf->nAvgBytesPerSec/16)&(~7);
		      if(skip<16) skip=16;
		    }
		    demux_read_data(sh_audio->ds,NULL,skip);
		    break;
		}
		case AFM_PCM:
		case AFM_DVDPCM:
		case AFM_ALAW: {
		    int skip=sh_audio->i_bps/16;
		    skip=skip&(~3);
		    demux_read_data(sh_audio->ds,NULL,skip);
		    break;
		}
                default: ds_fill_buffer(sh_audio->ds);  // skip PCM frame
              }
}
