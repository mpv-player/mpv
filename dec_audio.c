
// FIXME: use codec.conf struct here!!!
int detect_audio_format(sh_audio_t *sh_audio){
    int has_audio=0;
// Decide audio format:
    switch(sh_audio->wf.wFormatTag){
      case 0:
        has_audio=0;break; // disable/no audio
      case 6:
        avi_header.audio_seekable=1;
        has_audio=5;break; // aLaw
      case 0x31:
      case 0x32:
        has_audio=6;break; // MS-GSM
      case 0x50:
#ifdef DEFAULT_MPG123
      case 0x55:
#endif
        avi_header.audio_seekable=1;
        has_audio=1;break; // MPEG
      case 0x01:
        avi_header.audio_seekable=1;
        has_audio=2;break; // PCM
      case 0x2000:
        avi_header.audio_seekable=1;
        has_audio=3;break; // AC3
      default:
        avi_header.audio_seekable=0;
        has_audio=4;       // Win32/ACM
    }
  if(has_audio==4){
    if(!avi_header.audio_codec) avi_header.audio_codec=get_auds_codec_name(sh_audio);
    if(avi_header.auds_guid) has_audio=7; // force DShow
    if(!avi_header.audio_codec) has_audio=0; // unknown win32 codec
    if(verbose) printf("win32 audio codec: '%s'\n",avi_header.audio_codec);
  }
  if(verbose) printf("detected audio format: %d\n",has_audio);
  return has_audio;
}

int init_audio(sh_audio_t *sh_audio){
int has_audio=sh_audio->codec.driver;

sh_audio->samplesize=2;
sh_audio->pcm_bswap=0;
sh_audio->a_buffer_size=16384;  // default size, maybe not enough for Win32/ACM

if(has_audio==4){
  // Win32 ACM audio codec:
  if(init_acm_audio_codec(sh_audio)){
    sh_audio->channels=sh_audio->o_wf.nChannels;
    sh_audio->samplerate=sh_audio->o_wf.nSamplesPerSec;
    if(sh_audio->a_buffer_size<sh_audio->audio_out_minsize+OUTBURST)
        sh_audio->a_buffer_size=sh_audio->audio_out_minsize+OUTBURST;
  } else {
    printf("Could not load/initialize Win32/ACM AUDIO codec (missing DLL file?)\n");
    if((sh_audio->wf.wFormatTag)==0x55){
      printf("Audio format is MP3 -> fallback to internal mp3lib/mpg123\n");
      has_audio=1;  // fallback to mp3lib
    } else
      has_audio=0;  // nosound
  }
}

if(has_audio==7){
#ifndef USE_DIRECTSHOW
  printf("Compiled without DirectShow support -> force nosound :(\n");
  has_audio=0;
#else
  // Win32 DShow audio codec:
    WAVEFORMATEX *in_fmt=&sh_audio->wf;
    sh_audio->o_wf.nChannels=in_fmt->nChannels;
    sh_audio->o_wf.nSamplesPerSec=in_fmt->nSamplesPerSec;
    sh_audio->o_wf.nAvgBytesPerSec=2*sh_audio->o_wf.nSamplesPerSec*sh_audio->o_wf.nChannels;
    sh_audio->o_wf.wFormatTag=WAVE_FORMAT_PCM;
    sh_audio->o_wf.nBlockAlign=2*in_fmt->nChannels;
    sh_audio->o_wf.wBitsPerSample=16;
    sh_audio->o_wf.cbSize=0;

  if(!DS_AudioDecoder_Open(avi_header.audio_codec,avi_header.auds_guid,in_fmt)){
    sh_audio->channels=sh_audio->o_wf.nChannels;
    sh_audio->samplerate=sh_audio->o_wf.nSamplesPerSec;

    sh_audio->audio_in_minsize=2*sh_audio->o_wf.nBlockAlign;
    if(sh_audio->audio_in_minsize<8192) sh_audio->audio_in_minsize=8192;
    sh_audio->a_in_buffer_size=sh_audio->audio_in_minsize;
    sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
    sh_audio->a_in_buffer_len=0;

  } else {
    printf("ERROR: Could not load/initialize Win32/DirctShow AUDIO codec: %s\n",avi_header.audio_codec);
    if((in_fmt->wFormatTag)==0x55){
      printf("Audio format is MP3 -> fallback to internal mp3lib/mpg123\n");
      has_audio=1;  // fallback to mp3lib
    } else
      printf("Audio disabled! Try to upgrade your w32codec.zip package!!!\n");
      has_audio=0;  // nosound
  }
#endif
}


// allocate audio out buffer:
sh_audio->a_buffer=malloc(sh_audio->a_buffer_size);
memset(sh_audio->a_buffer,0,sh_audio->a_buffer_size);
sh_audio->a_buffer_len=0;

if(has_audio==4){
    int ret=acm_decode_audio(sh_audio,sh_audio->a_buffer,sh_audio->a_buffer_size);
    if(ret<0){
        printf("ACM error %d -> switching to nosound...\n",ret);
        has_audio=0;
    } else {
        sh_audio->a_buffer_len=ret;
        printf("ACM decoding test: %d bytes\n",ret);
    }
}

if(has_audio==2){
//  if(file_format==DEMUXER_TYPE_AVI){    // FIXME!!!!!!!
    // AVI PCM Audio:
    WAVEFORMATEX *h=&sh_audio->wf;
    sh_audio->channels=h->nChannels;
    sh_audio->samplerate=h->nSamplesPerSec;
    sh_audio->samplesize=(h->wBitsPerSample+7)/8;
//  } else {
//    // DVD PCM audio:
//    sh_audio->channels=2;
//    sh_audio->samplerate=48000;
//    sh_audio->pcm_bswap=1;
//  }
} else
if(has_audio==3){
  // Dolby AC3 audio:
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
    sh_audio->samplerate=sh_audio->ac3_frame->sampling_rate;
    sh_audio->channels=2;
  } else has_audio=0; // bad frame -> disable audio
} else
if(has_audio==5){
  // aLaw audio codec:
  Gen_aLaw_2_Signed(); // init table
  sh_audio->channels=sh_audio->wf.nChannels;
  sh_audio->samplerate=sh_audio->wf.nSamplesPerSec;
} else
if(has_audio==6){
  // MS-GSM audio codec:
  GSM_Init();
  sh_audio->channels=sh_audio->wf.nChannels;
  sh_audio->samplerate=sh_audio->wf.nSamplesPerSec;
}
// must be here for Win32->mp3lib fallbacks
if(has_audio==1){
  // MPEG Audio:
  MP3_Init();
  MP3_samplerate=MP3_channels=0;
//  printf("[\n");
  sh_audio->a_buffer_len=MP3_DecodeFrame(sh_audio->a_buffer,-1);
//  printf("]\n");
  sh_audio->channels=2; // hack
  sh_audio->samplerate=MP3_samplerate;
}

if(!sh_audio->channels || !sh_audio->samplerate){
  printf("Unknown/missing audio format, using nosound\n");
  has_audio=0;
}

  sh_audio->o_bps=sh_audio->channels*sh_audio->samplerate*sh_audio->samplesize;

  return has_audio;
}

// Audio decoding

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int maxlen){
    int len=-1;
    switch(sh_audio->codec.driver){
      case 1: // MPEG layer 2 or 3
        len=MP3_DecodeFrame(buf,-1);
        sh_audio->channels=2; // hack
        break;
      case 2: // PCM
      { len=demux_read_data(sh_audio->ds,buf,OUTBURST);
        if(sh_audio->pcm_bswap){
          int j;
          //if(i&1){ printf("Warning! pcm_audio_size&1 !=0  (%d)\n",i);i&=~1; }
          for(j=0;j<len;j+=2){
            char x=buf[j];
            buf[j]=buf[j+1];
            buf[j+1]=x;
          }
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
                if(demux_read_data(d_audio,buf,65)!=65) break; // EOF
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
          memcpy(buf,sh_audio->ac3_frame->audio_data,len);
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


