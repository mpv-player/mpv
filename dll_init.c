// ACM audio and VfW video codecs initialization
// based on the avifile library [http://divx.euro.ru]

static char* a_in_buffer=NULL;
static int a_in_buffer_len=0;
static int a_in_buffer_size=0;

int init_audio_codec(){
    HRESULT ret;
    WAVEFORMATEX *in_fmt=(WAVEFORMATEX*)&avi_header.wf_ext;
    unsigned long srcsize=0;

  if(verbose) printf("======= Win32 (ACM) AUDIO Codec init =======\n");

    avi_header.srcstream=NULL;

//    if(in_fmt->nSamplesPerSec==0){  printf("Bad WAVE header!\n");exit(1);  }
//    MSACM_RegisterAllDrivers();

    avi_header.wf.nChannels=in_fmt->nChannels;
    avi_header.wf.nSamplesPerSec=in_fmt->nSamplesPerSec;
    avi_header.wf.nAvgBytesPerSec=2*avi_header.wf.nSamplesPerSec*avi_header.wf.nChannels;
    avi_header.wf.wFormatTag=WAVE_FORMAT_PCM;
    avi_header.wf.nBlockAlign=2*in_fmt->nChannels;
    avi_header.wf.wBitsPerSample=16;
    avi_header.wf.cbSize=0;

    win32_codec_name = avi_header.audio_codec;
    ret=acmStreamOpen(&avi_header.srcstream,(HACMDRIVER)NULL,
                    in_fmt,&avi_header.wf,
		    NULL,0,0,0);
    if(ret){
        if(ret==ACMERR_NOTPOSSIBLE)
            printf("ACM_Decoder: Unappropriate audio format\n");
        else
            printf("ACM_Decoder: acmStreamOpen error %d", ret);
        avi_header.srcstream=NULL;
        return 0;
    }
    if(verbose) printf("Audio codec opened OK! ;-)\n");

    srcsize=in_fmt->nBlockAlign;
    acmStreamSize(avi_header.srcstream, srcsize, &srcsize, ACM_STREAMSIZEF_SOURCE);
    if(srcsize<OUTBURST) srcsize=OUTBURST;
    avi_header.audio_out_minsize=srcsize; // audio output min. size
    if(verbose) printf("Audio ACM output buffer min. size: %d\n",srcsize);

    acmStreamSize(avi_header.srcstream, srcsize, &srcsize, ACM_STREAMSIZEF_DESTINATION);
    avi_header.audio_in_minsize=srcsize; // audio input min. size
    if(verbose) printf("Audio ACM input buffer min. size: %d\n",srcsize);

    a_in_buffer_size=avi_header.audio_in_minsize;
    a_in_buffer=malloc(a_in_buffer_size);
    a_in_buffer_len=0;

    return 1;
}

int acm_decode_audio(void* a_buffer,int len){
        ACMSTREAMHEADER ash;
        HRESULT hr;
        DWORD srcsize=0;
        acmStreamSize(avi_header.srcstream,len , &srcsize, ACM_STREAMSIZEF_DESTINATION);
        if(verbose>=3)printf("acm says: srcsize=%d  (buffsize=%d)  out_size=%d\n",srcsize,a_in_buffer_size,len);
//        if(srcsize==0) srcsize=((WAVEFORMATEX *)&avi_header.wf_ext)->nBlockAlign;
        if(srcsize>a_in_buffer_size) srcsize=a_in_buffer_size; // !!!!!!
        if(a_in_buffer_len<srcsize){
          a_in_buffer_len+=
            demux_read_data(d_audio,&a_in_buffer[a_in_buffer_len],
            srcsize-a_in_buffer_len);
        }
        memset(&ash, 0, sizeof(ash));
        ash.cbStruct=sizeof(ash);
        ash.fdwStatus=0;
        ash.dwUser=0; 
        ash.pbSrc=a_in_buffer;
        ash.cbSrcLength=a_in_buffer_len;
        ash.pbDst=a_buffer;
        ash.cbDstLength=len;
        hr=acmStreamPrepareHeader(avi_header.srcstream,&ash,0);
        if(hr){
          printf("ACM_Decoder: acmStreamPrepareHeader error %d\n",hr);
					return -1;
        }
        hr=acmStreamConvert(avi_header.srcstream,&ash,0);
        if(hr){
          printf("ACM_Decoder: acmStreamConvert error %d\n",hr);
					return -1;
        }
        //printf("ACM convert %d -> %d  (buf=%d)\n",ash.cbSrcLengthUsed,ash.cbDstLengthUsed,a_in_buffer_len);
        if(ash.cbSrcLengthUsed>=a_in_buffer_len){
          a_in_buffer_len=0;
        } else {
          a_in_buffer_len-=ash.cbSrcLengthUsed;
          memcpy(a_in_buffer,&a_in_buffer[ash.cbSrcLengthUsed],a_in_buffer_len);
        }
        len=ash.cbDstLengthUsed;
        hr=acmStreamUnprepareHeader(avi_header.srcstream,&ash,0);
        if(hr){
          printf("ACM_Decoder: acmStreamUnprepareHeader error %d\n",hr);
        }
        return len;
}



int init_video_codec(int outfmt){
  HRESULT ret;

  if(verbose) printf("======= Win32 (VFW) VIDEO Codec init =======\n");

  memset(&avi_header.o_bih, 0, sizeof(BITMAPINFOHEADER));
  avi_header.o_bih.biSize = sizeof(BITMAPINFOHEADER);

  win32_codec_name = avi_header.video_codec;
  avi_header.hic = ICOpen( 0x63646976, avi_header.bih.biCompression, ICMODE_FASTDECOMPRESS);
//  avi_header.hic = ICOpen( 0x63646976, avi_header.bih.biCompression, ICMODE_DECOMPRESS);
  if(!avi_header.hic){
    printf("ICOpen failed! unknown codec / wrong parameters?\n");
    return 0;
  }

//  avi_header.bih.biBitCount=32;

  ret = ICDecompressGetFormat(avi_header.hic, &avi_header.bih, &avi_header.o_bih);
  if(ret){
    printf("ICDecompressGetFormat failed: Error %d\n", ret);
    return 0;
  }
  if(verbose) printf("ICDecompressGetFormat OK\n");
  
//  printf("ICM_DECOMPRESS_QUERY=0x%X",ICM_DECOMPRESS_QUERY);

//  avi_header.o_bih.biWidth=avi_header.bih.biWidth;
//  avi_header.o_bih.biCompression = 0x32315659; //  mmioFOURCC('U','Y','V','Y');
//  ret=ICDecompressGetFormatSize(avi_header.hic,&avi_header.o_bih);
//  avi_header.o_bih.biCompression = 3; //0x32315659;
//  avi_header.o_bih.biCompression = mmioFOURCC('U','Y','V','Y');
//  avi_header.o_bih.biCompression = mmioFOURCC('U','Y','V','Y');
//  avi_header.o_bih.biCompression = mmioFOURCC('Y','U','Y','2');
//  avi_header.o_bih.biPlanes=3;
//  avi_header.o_bih.biBitCount=16;

  if(outfmt==IMGFMT_YUY2)
    avi_header.o_bih.biBitCount=16;
  else
    avi_header.o_bih.biBitCount=outfmt&0xFF;//   //24;

  avi_header.o_bih.biSizeImage=avi_header.o_bih.biWidth*avi_header.o_bih.biHeight*(avi_header.o_bih.biBitCount/8);

  if(!avi_header.flipped)
    avi_header.o_bih.biHeight=-avi_header.bih.biHeight; // flip image!

  if(outfmt==IMGFMT_YUY2 && !avi_header.yuv_hack_needed)
    avi_header.o_bih.biCompression = mmioFOURCC('Y','U','Y','2');

//  avi_header.o_bih.biCompression = mmioFOURCC('U','Y','V','Y');


  if(verbose) {
    printf("Starting decompression, format:\n");
	printf("  biSize %d\n", avi_header.bih.biSize);
	printf("  biWidth %d\n", avi_header.bih.biWidth);
	printf("  biHeight %d\n", avi_header.bih.biHeight);
	printf("  biPlanes %d\n", avi_header.bih.biPlanes);
	printf("  biBitCount %d\n", avi_header.bih.biBitCount);
	printf("  biCompression %d='%.4s'\n", avi_header.bih.biCompression, &avi_header.bih.biCompression);
	printf("  biSizeImage %d\n", avi_header.bih.biSizeImage);
    printf("Dest fmt:\n");
	printf("  biSize %d\n", avi_header.o_bih.biSize);
	printf("  biWidth %d\n", avi_header.o_bih.biWidth);
	printf("  biHeight %d\n", avi_header.o_bih.biHeight);
	printf("  biPlanes %d\n", avi_header.o_bih.biPlanes);
	printf("  biBitCount %d\n", avi_header.o_bih.biBitCount);
	printf("  biCompression %d='%.4s'\n", avi_header.o_bih.biCompression, &avi_header.o_bih.biCompression);
	printf("  biSizeImage %d\n", avi_header.o_bih.biSizeImage);
  }

  ret = ICDecompressQuery(avi_header.hic, &avi_header.bih, &avi_header.o_bih);
  if(ret){
    printf("ICDecompressQuery failed: Error %d\n", ret);
    return 0;
  }
  if(verbose) printf("ICDecompressQuery OK\n");

  
  ret = ICDecompressBegin(avi_header.hic, &avi_header.bih, &avi_header.o_bih);
  if(ret){
    printf("ICDecompressBegin failed: Error %d\n", ret);
    return 0;
  }

#if 0

//avi_header.hic
//ICSendMessage(HIC hic,unsigned int msg,long lParam1,long lParam2)
{ int i;
  for(i=73;i<256;i++){
    printf("Calling ICM_USER+%d function...",i);fflush(stdout);
    ret = ICSendMessage(avi_header.hic,ICM_USER+i,NULL,NULL);
    printf(" ret=%d\n",ret);
  }
}
#endif

  avi_header.our_out_buffer = malloc(avi_header.o_bih.biSizeImage);
  if(!avi_header.our_out_buffer){
    printf("not enough memory for decoded picture buffer (%d bytes)\n", avi_header.o_bih.biSizeImage);
    return 0;
  }

  if(outfmt==IMGFMT_YUY2 && avi_header.yuv_hack_needed)
    avi_header.o_bih.biCompression = mmioFOURCC('Y','U','Y','2');

//  avi_header.our_in_buffer=malloc(avi_header.video.dwSuggestedBufferSize); // FIXME!!!!
  
  if(verbose) printf("VIDEO CODEC Init OK!!! ;-)\n");
  return 1;
}
