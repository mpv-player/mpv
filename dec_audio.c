
// Audio decoding

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int maxlen){
    int len=-1;
    switch(sh_audio->codec.driver){
      case 1: // MPEG layer 2 or 3
        len=MP3_DecodeFrame(buf,-1);
        MP3_channels=2; // hack
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
//  		XA_GSM_Decoder(buf,(unsigned short *) &a_buffer[a_buffer_len]); // decodes 33 byte -> 160 short
                len+=2*320;
            }
        break;
      }
      case 3: // AC3 decoder
        //printf("{1:%d}",avi_header.idx_pos);fflush(stdout);
        if(!sh_audio->ac3_frame) sh_audio->ac3_frame=ac3_decode_frame();
        //printf("{2:%d}",avi_header.idx_pos);fflush(stdout);
        if(sh_audio->ac3_frame){
          len = 256 * 6 *MP3_channels*MP3_bps;
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


