
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"

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

#ifdef HAVE_OGGVORBIS
/* XXX is math.h really needed? - atmos */
#include <math.h>
#include <vorbis/codec.h>
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
sh_audio->o_bps=0;

sh_audio->a_buffer_size=0;
sh_audio->a_buffer=NULL;

sh_audio->a_in_buffer_len=0;

// setup required min. in/out buffer size:
sh_audio->audio_out_minsize=8192;// default size, maybe not enough for Win32/ACM

switch(driver){
case AFM_ACM:
#ifndef	USE_WIN32DLL
  mp_msg(MSGT_DECAUDIO,MSGL_ERR,"Win32/ACM audio codec disabled, or unavailable on non-x86 CPU -> force nosound :(\n");
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
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"Could not load/initialize Win32/ACM AUDIO codec (missing DLL file?)\n");
    driver=0;
  }
#endif
  break;
case AFM_DSHOW:
#ifndef USE_DIRECTSHOW
  mp_msg(MSGT_DECAUDIO,MSGL_ERR,"Compiled without DirectShow support -> force nosound :(\n");
  driver=0;
#else
  // Win32 DShow audio codec:
//  printf("DShow_audio: channs=%d  rate=%d\n",sh_audio->channels,sh_audio->samplerate);
  if(DS_AudioDecoder_Open(sh_audio->codec->dll,&sh_audio->codec->guid,sh_audio->wf)){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"ERROR: Could not load/initialize Win32/DirectShow AUDIO codec: %s\n",sh_audio->codec->dll);
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
case AFM_VORBIS:
#ifndef	HAVE_OGGVORBIS
  mp_msg(MSGT_DECAUDIO,MSGL_ERR,"OggVorbis audio codec disabled -> force nosound :(\n");
  driver=0;
#else
  /* OggVorbis audio via libvorbis, compatible with files created by nandub and zorannt codec */
  sh_audio->audio_out_minsize=4096;
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

mp_msg(MSGT_DECAUDIO,MSGL_V,"dec_audio: Allocating %d + %d = %d bytes for output buffer\n",
    sh_audio->audio_out_minsize,MAX_OUTBURST,sh_audio->a_buffer_size);

sh_audio->a_buffer=malloc(sh_audio->a_buffer_size);
if(!sh_audio->a_buffer){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"Cannot allocate audio out buffer\n");
    return 0;
}
memset(sh_audio->a_buffer,0,sh_audio->a_buffer_size);
sh_audio->a_buffer_len=0;

switch(driver){
#ifdef USE_WIN32DLL
case AFM_ACM: {
    int ret=acm_decode_audio(sh_audio,sh_audio->a_buffer,4096,sh_audio->a_buffer_size);
    if(ret<0){
        mp_msg(MSGT_DECAUDIO,MSGL_WARN,"ACM decoding error: %d\n",ret);
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
  len = ds_get_packet(sh_audio->ds, &buffer); // maybe 1 packet is not enough,
    // at least for mpeg, PS packets contain about max. 2000 bytes of data.
  if(ac3_iec958_parse_syncinfo(buffer, len, &ai, &skipped) < 0) {
      mp_msg(MSGT_DECAUDIO,MSGL_ERR, "AC3 stream not valid.\n");
      driver = 0;
      break;
  }
  if(ai.samplerate != 48000) {
      mp_msg(MSGT_DECAUDIO,MSGL_ERR,"Only 48000 Hz streams supported.\n");
      driver = 0;
      break;
  }
  sh_audio->samplerate=ai.samplerate;
  sh_audio->samplesize=ai.framesize;
  sh_audio->channels=1;
  sh_audio->i_bps=ai.bitrate*(1000/8);
  sh_audio->ac3_frame=malloc(6144);
  sh_audio->o_bps=sh_audio->i_bps;  // XXX FIXME!!! XXX
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
#ifdef HAVE_OGGVORBIS
case AFM_VORBIS: {
  // OggVorbis Audio:
#if 0 /* just here for reference - atmos */ 
  ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
  ogg_stream_state os; /* take physical pages, weld into a logical
			  stream of packets */
  ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */
  
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			  settings */
  vorbis_comment   vc; /* struct that stores all the bitstream user comments */
  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */
#else
  /* nix, nada, rien, nothing, nem, nüx */
#endif

  uint32_t hdrsizes[3];/* stores vorbis header sizes from AVI audio header,
			  maybe use ogg_uint32_t */
  //int i;
  int ret;
  char *buffer;
  ogg_packet hdr;
  //ov_struct_t *s=&sh_audio->ov;
  sh_audio->ov=malloc(sizeof(ov_struct_t));
  //s=&sh_audio->ov;

  vorbis_info_init(&sh_audio->ov->vi);
  vorbis_comment_init(&sh_audio->ov->vc);

  printf("OggVorbis: cbsize: %i\n", sh_audio->wf->cbSize);
  memcpy(hdrsizes, ((unsigned char*)sh_audio->wf)+2*sizeof(WAVEFORMATEX), 3*sizeof(uint32_t));
  printf("OggVorbis: Read header sizes: initial: %i comment: %i codebook: %i\n", hdrsizes[0], hdrsizes[1], hdrsizes[2]);
  /*for(i=12; i <= 40; i+=2) { // header bruteforce :)
    memcpy(hdrsizes, ((unsigned char*)sh_audio->wf)+i, 3*sizeof(uint32_t));
    printf("OggVorbis: Read header sizes (%i): %ld %ld %ld\n", i, hdrsizes[0], hdrsizes[1], hdrsizes[2]);
  }*/

  /* read headers */ // FIXME disable sound on errors here, we absolutely need this headers! - atmos
  hdr.packet=NULL;
  hdr.b_o_s  = 1; /* beginning of stream for first packet */  
  hdr.bytes  = hdrsizes[0];
  hdr.packet = realloc(hdr.packet,hdr.bytes);
  memcpy(hdr.packet,((unsigned char*)sh_audio->wf)+2*sizeof(WAVEFORMATEX)+3*sizeof(uint32_t),hdr.bytes);
  if(vorbis_synthesis_headerin(&sh_audio->ov->vi,&sh_audio->ov->vc,&hdr)<0)
    printf("OggVorbis: initial (identification) header broken!\n");
  hdr.b_o_s  = 0;
  hdr.bytes  = hdrsizes[1];
  hdr.packet = realloc(hdr.packet,hdr.bytes);
  memcpy(hdr.packet,((unsigned char*)sh_audio->wf)+2*sizeof(WAVEFORMATEX)+3*sizeof(uint32_t)+hdrsizes[0],hdr.bytes);
  if(vorbis_synthesis_headerin(&sh_audio->ov->vi,&sh_audio->ov->vc,&hdr)<0)
    printf("OggVorbis: comment header broken!\n");
  hdr.bytes  = hdrsizes[2];
  hdr.packet = realloc(hdr.packet,hdr.bytes);
  memcpy(hdr.packet,((unsigned char*)sh_audio->wf)+2*sizeof(WAVEFORMATEX)+3*sizeof(uint32_t)+hdrsizes[0]+hdrsizes[1],hdr.bytes);
  if(vorbis_synthesis_headerin(&sh_audio->ov->vi,&sh_audio->ov->vc,&hdr)<0)
    printf("OggVorbis: codebook header broken!\n");
  hdr.bytes=0;
  hdr.packet = realloc(hdr.packet,hdr.bytes); /* free */
  /* done with the headers */


  /* Throw the comments plus a few lines about the bitstream we're
     decoding */
  {
    char **ptr=sh_audio->ov->vc.user_comments;
    while(*ptr){
      printf("OggVorbisComment: %s\n",*ptr);
      ++ptr;
    }
      printf("OggVorbis: Bitstream is %d channel, %ldHz, %ldkbit/s %cBR\n",sh_audio->ov->vi.channels,sh_audio->ov->vi.rate,sh_audio->ov->vi.bitrate_nominal/1000, (sh_audio->ov->vi.bitrate_lower!=sh_audio->ov->vi.bitrate_nominal)||(sh_audio->ov->vi.bitrate_upper!=sh_audio->ov->vi.bitrate_nominal)?'V':'C');
      printf("OggVorbis: Encoded by: %s\n",sh_audio->ov->vc.vendor);
  }
  sh_audio->channels=sh_audio->ov->vi.channels; 
  sh_audio->samplerate=sh_audio->ov->vi.rate;
  sh_audio->i_bps=sh_audio->ov->vi.bitrate_nominal/8;
    
//  printf("[\n");
  sh_audio->a_buffer_len=sh_audio->audio_out_minsize;///ov->vi.channels;
//  printf("]\n");

  /* OK, got and parsed all three headers. Initialize the Vorbis
     packet->PCM decoder. */
  vorbis_synthesis_init(&sh_audio->ov->vd,&sh_audio->ov->vi); /* central decode state */
  vorbis_block_init(&sh_audio->ov->vd,&sh_audio->ov->vb);     /* local state for most of the decode
								 so multiple block decodes can
								 proceed in parallel.  We could init
								 multiple vorbis_block structures
								 for vd here */
  //printf("OggVorbis: synthesis and block init done.\n"); 
  ogg_sync_init(&sh_audio->ov->oy); /* Now we can read pages */

  while((ret = ogg_sync_pageout(&sh_audio->ov->oy,&sh_audio->ov->og))!=1) {
    if(ret == -1)
      printf("OggVorbis: Pageout: not properly synced, had to skip some bytes.\n");
    else
    if(ret == 0) {
      printf("OggVorbis: Pageout: need more data to verify page, reading more data.\n");
      /* submit a a_buffer_len  block to libvorbis' Ogg layer */
      buffer=ogg_sync_buffer(&sh_audio->ov->oy,sh_audio->a_buffer_len);
      ogg_sync_wrote(&sh_audio->ov->oy,demux_read_data(sh_audio->ds,buffer,sh_audio->a_buffer_len));
    }
  }
  printf("OggVorbis: Pageout: successfull.\n");
  /* commenting out pagein to leave data (hopefully) to the decoder - atmos */
  //ogg_stream_pagein(&sh_audio->ov->os,&sh_audio->ov->og); /* we can ignore any errors here
  //					 as they'll also become apparent
  //					 at packetout */

  /* Get the serial number and set up the rest of decode. */
  /* serialno first; use it to set up a logical stream */
  ogg_stream_init(&sh_audio->ov->os,ogg_page_serialno(&sh_audio->ov->og));
  
  printf("OggVorbis: Init OK!\n");

  break;
}
#endif
}

if(!sh_audio->channels || !sh_audio->samplerate){
  mp_msg(MSGT_DECAUDIO,MSGL_WARN,"Unknown/missing audio format, using nosound\n");
  driver=0;
}

  if(!driver){
      if(sh_audio->a_buffer) free(sh_audio->a_buffer);
      sh_audio->a_buffer=NULL;
      return 0;
  }

  if(!sh_audio->o_bps)
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
#ifdef HAVE_OGGVORBIS
      case AFM_VORBIS: { // OggVorbis
        /* note: good minlen would be 4k or 8k IMHO - atmos */
        int ret;
        char *buffer;
        int bytes;
	int samples;
	float **pcm;
        ogg_int16_t convbuffer[4096];
        int convsize;
        minlen=4096; // XXX hack, not neccessarily needed - atmos
        convsize=minlen/sh_audio->ov->vi.channels;
        while((ret = ogg_sync_pageout(&sh_audio->ov->oy,&sh_audio->ov->og))!=1) {
          if(ret == -1)
            printf("OggVorbis: Pageout: not properly synced, had to skip some bytes.\n");
          else
          if(ret == 0) {
            //printf("OggVorbis: Pageout: need more data to verify page, reading more data.\n");
            /* submit a minlen k block to libvorbis' Ogg layer */
            buffer=ogg_sync_buffer(&sh_audio->ov->oy,minlen);
            bytes=demux_read_data(sh_audio->ds,buffer,minlen);
            ogg_sync_wrote(&sh_audio->ov->oy,bytes);
            if(bytes==0)
              printf("OggVorbis: 0Bytes written, possible End of Stream\n");
          }
        }
        //printf("OggVorbis: Pageout: successfull, pagin in.\n");
        if(ogg_stream_pagein(&sh_audio->ov->os,&sh_audio->ov->og)<0)
          printf("OggVorbis: Pagein failed!\n");

        ret=ogg_stream_packetout(&sh_audio->ov->os,&sh_audio->ov->op);
        if(ret==0)
          printf("OggVorbis: Packetout: need more data, FIXME!\n");
        else
        if(ret<0)
          printf("OggVorbis: Packetout: missing or corrupt data, skipping packet!\n");
        else {

        /* we have a packet.  Decode it */
	      
	if(vorbis_synthesis(&sh_audio->ov->vb,&sh_audio->ov->op)==0) /* test for success! */
	  vorbis_synthesis_blockin(&sh_audio->ov->vd,&sh_audio->ov->vb);
	
        /* **pcm is a multichannel float vector.  In stereo, for
	   example, pcm[0] is left, and pcm[1] is right.  samples is
	   the size of each channel.  Convert the float values
	   (-1.<=range<=1.) to whatever PCM format and write it out */
	      
        while((samples=vorbis_synthesis_pcmout(&sh_audio->ov->vd,&pcm))>0){
	  int i,j;
	  int clipflag=0;
	  int bout=(samples<convsize?samples:convsize);
		
	  /* convert floats to 16 bit signed ints (host order) and
	     interleave */
	  for(i=0;i<sh_audio->ov->vi.channels;i++){
	    ogg_int16_t *ptr=convbuffer+i;
	    float  *mono=pcm[i];
	    for(j=0;j<bout;j++){
#if 1
	      int val=mono[j]*32767.f;
#else /* optional dither */
	      int val=mono[j]*32767.f+drand48()-0.5f;
#endif
	      /* might as well guard against clipping */
	      if(val>32767){
	        val=32767;
	        clipflag=1;
	      }
	      if(val<-32768){
	        val=-32768;
	        clipflag=1;
	      }
	      *ptr=val;
	      ptr+=sh_audio->ov->vi.channels;
	    }
	  }
		
	  if(clipflag)
	    printf("Clipping in frame %ld\n",(long)(sh_audio->ov->vd.sequence));
	
	  //fwrite(convbuffer,2*sh_audio->ov->vi.channels,bout,stderr); //dump pcm to file for debugging
          len=2*sh_audio->ov->vi.channels*bout;
	  memcpy(buf,convbuffer,len);
		
	  vorbis_synthesis_read(&sh_audio->ov->vd,bout); /* tell libvorbis how
							    many samples we
							    actually consumed */
        }
	if(ogg_page_eos(&sh_audio->ov->og))
          printf("OggVorbis: End of Stream reached!\n"); // FIXME clearup decoder, notify mplayer - atmos

        } // from else, packetout ok

        break;
      }
#endif
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
        mp_msg(MSGT_DECAUDIO,MSGL_DBG3,"DShow says: srcsize=%d  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,maxlen);
        if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
        if(sh_audio->a_in_buffer_len<srcsize){
          sh_audio->a_in_buffer_len+=
            demux_read_data(sh_audio->ds,&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len],
            srcsize-sh_audio->a_in_buffer_len);
        }
        DS_AudioDecoder_Convert(sh_audio->a_in_buffer,sh_audio->a_in_buffer_len,
            buf,maxlen, &size_in,&size_out);
        mp_dbg(MSGT_DECAUDIO,MSGL_DBG2,"DShow: audio %d -> %d converted  (in_buf_len=%d of %d)  %d\n",size_in,size_out,sh_audio->a_in_buffer_len,sh_audio->a_in_buffer_size,ds_tell_pts(sh_audio->ds));
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
