//#define ANGELPOTION

char* get_vids_codec_name(){
//  unsigned long fccHandler=avi_header.video.fccHandler;
  unsigned long fccHandler=avi_header.bih.biCompression;
  avi_header.yuv_supported=0;
  avi_header.yuv_hack_needed=0;
  avi_header.flipped=0;
  switch(fccHandler){
	case mmioFOURCC('M', 'P', 'G', '4'):
	case mmioFOURCC('m', 'p', 'g', '4'):
	case mmioFOURCC('M', 'P', '4', '2'):
	case mmioFOURCC('m', 'p', '4', '2'):
//	case mmioFOURCC('M', 'P', '4', '3'):
//      case mmioFOURCC('m', 'p', '4', '3'):
	  printf("Video in Microsoft MPEG-4 format\n");
          avi_header.yuv_supported=1;
          avi_header.yuv_hack_needed=1;
#ifdef ANGELPOTION
          return "APmpg4v1.dll";
#endif
          return "mpg4c32.dll";
	case mmioFOURCC('M', 'P', '4', '3'):
	case mmioFOURCC('m', 'p', '4', '3'):
	  printf("Video in MPEG-4 v3 (really DivX) format\n");
          avi_header.bih.biCompression=mmioFOURCC('d', 'i', 'v', '3'); // hack
          avi_header.yuv_supported=1;
          avi_header.yuv_hack_needed=1;
#ifdef ANGELPOTION
          return "APmpg4v1.dll";
#endif
          return "divxc32.dll";

	case mmioFOURCC('D', 'I', 'V', 'X'):
	case mmioFOURCC('d', 'i', 'v', 'x'):
          return "DivX.dll";

	case mmioFOURCC('D', 'I', 'V', '3'):
	case mmioFOURCC('d', 'i', 'v', '3'):
	case mmioFOURCC('D', 'I', 'V', '4'):
        case mmioFOURCC('d', 'i', 'v', '4'):
	case mmioFOURCC('D', 'I', 'V', '5'):
        case mmioFOURCC('d', 'i', 'v', '5'):
	case mmioFOURCC('M', 'P', '4', '1'):
	case mmioFOURCC('m', 'p', '4', '1'):
	  printf("Video in DivX ;-) format\n");
          avi_header.yuv_supported=1;
          avi_header.yuv_hack_needed=1;
#ifdef ANGELPOTION
          return "APmpg4v1.dll";
#endif
          return "divxc32.dll";

	case mmioFOURCC('I', 'V', '5', '0'):	    
	case mmioFOURCC('i', 'v', '5', '0'):	 
	  printf("Video in Indeo Video 5 format\n");
          avi_header.yuv_supported=1;   // YUV pic is upside-down :(
          return "ir50_32.dll";

	case mmioFOURCC('I', 'V', '4', '1'):	    
	case mmioFOURCC('i', 'v', '4', '1'):	    
	  printf("Video in Indeo Video 4.1 format\n");   
          avi_header.flipped=1;
		  avi_header.no_32bpp_support=1;
          return "ir41_32.dll";

	case mmioFOURCC('I', 'V', '3', '2'):	    
	case mmioFOURCC('i', 'v', '3', '2'):
	  printf("Video in Indeo Video 3.2 format\n");   
          avi_header.flipped=1;
		  avi_header.no_32bpp_support=1;
          return "ir32_32.dll";

	case mmioFOURCC('c', 'v', 'i', 'd'):
	  printf("Video in Cinepak format\n");
          avi_header.yuv_supported=1;
          return "iccvid.dll";

	case mmioFOURCC('c', 'r', 'a', 'm'):
	case mmioFOURCC('C', 'R', 'A', 'M'):
	  printf("Video in CRAM format\n");
	  avi_header.no_32bpp_support=1;
          //avi_header.yuv_supported=1;
          return "msvidc32.dll";

//*** Only 16bit .DLL available (can't load under linux) ***
//	case mmioFOURCC('V', 'C', 'R', '1'):
//	  printf("Video in ATI VCR1 format\n");
//          return "ativcr1.dll";

	case mmioFOURCC('V', 'C', 'R', '2'):
	  printf("Video in ATI VCR2 format\n");
          avi_header.yuv_supported=1;
          return "ativcr2.dll";

	case mmioFOURCC('A', 'S', 'V', '1'):
	  printf("Asus ASV-1 format\n");
//          avi_header.yuv_supported=1;
          return "asusasvd.dll";

	case mmioFOURCC('A', 'S', 'V', '2'):
	  printf("Asus ASV-2 format\n");
//          avi_header.yuv_supported=1;
          avi_header.flipped=1;
//          avi_header.bih.biCompression=mmioFOURCC('A', 'S', 'V', '1');
//          return "asusasvd.dll";
          return "asusasv2.dll";

	case mmioFOURCC('I', '2', '6', '3'):
	case mmioFOURCC('i', '2', '6', '3'):
	  printf("Video in I263 format\n");
          return "i263_32.drv";

	case mmioFOURCC('M', 'J', 'P', 'G'):
	  printf("Video in MJPEG format\n");
          avi_header.yuv_supported=1;
		    return "M3JPEG32.dll";
//          return "mcmjpg32.dll";
//          return "m3jpeg32.dll";
//          return "libavi_mjpeg.so";
  }
  printf("UNKNOWN video codec: %.4s (0x%0X)\n",&fccHandler,fccHandler);
  printf("If you know this video format and codec, you can edit codecs.c in the source!\n");
  printf("Please contact the author, send this movie to be supported by future version.\n");
  return NULL;
}

char* get_auds_codec_name(){
  int id=((WAVEFORMATEX*)avi_header.wf_ext)->wFormatTag;
  switch (id){
    	case 0x161://DivX audio
//            ((WAVEFORMATEX*)avi_header.wf_ext)->wFormatTag=0x160; //hack
    	case 0x160://DivX audio
            avi_header.audio_seekable=0;
            return "divxa32.acm";   // tested, OK.
	case 0x2://MS ADPCM
            avi_header.audio_seekable=0;
            return "msadp32.acm";   // tested, OK.
	case 0x55://MPEG l3
            avi_header.audio_seekable=1;
            return "l3codeca.acm";  // tested, OK. faster than mp3lib on intel
	case 0x11://IMA ADPCM
            return "imaadp32.acm";  // segfault :(
	case 0x31://MS GSM
	case 0x32://MS GSM
            return "msgsm32.acm";   // segfault :( - not req. has internal now!
        case 0x75://VoxWare
            return "voxmsdec.ax";   // directshow, not yet supported just a try
//	case 0x06://???
//            return "lhacm.acm";
//            return "msg711.acm";
//            return "tssoft32.acm";
  }
  printf("UNKNOWN audio codec: 0x%0X\n",id);
  printf("If you know this audio format and codec, you can edit codecs.c in the source!\n");
  printf("Please contact the author, send this movie to be supported by future version.\n");
  return NULL;
}

