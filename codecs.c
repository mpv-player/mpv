//#define ANGELPOTION
//#define USE_DIRECTSHOW

static    GUID wmv1_clsid={0x4facbba1, 0xffd8, 0x4cd7,
    {0x82, 0x28, 0x61, 0xe2, 0xf6, 0x5c, 0xb1, 0xae}};
static    GUID wmv2_clsid={0x521fb373, 0x7654, 0x49f2, 
    {0xbd, 0xb1, 0x0c, 0x6e, 0x66, 0x60, 0x71, 0x4f}};    
static    GUID CLSID_MorganMjpeg={0x6988b440, 0x8352, 0x11d3, 
    {0x9b, 0xda, 0xca, 0x86, 0x73, 0x7c, 0x71, 0x68}};
static    GUID CLSID_Acelp={0x4009f700, 0xaeba, 0x11d1,
    {0x83, 0x44, 0x00, 0xc0, 0x4f, 0xb9, 0x2e, 0xb7}};
static    GUID CLSID_Voxware={0x73f7a062, 0x8829, 0x11d1,
    {0xb5, 0x50, 0x00, 0x60, 0x97, 0x24, 0x2d, 0x8d}};
static    GUID CLSID_DivxDecompressorCF={0x82CCd3E0, 0xF71A, 0x11D0,
    { 0x9f, 0xe5, 0x00, 0x60, 0x97, 0x78, 0xaa, 0xaa}};
static    GUID CLSID_IV50_Decoder={0x30355649, 0x0000, 0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static    GUID dvsd_clsid={0xB1B77C00, 0xC3E4, 0x11CF,
    {0xAF, 0x79, 0x00, 0xAA, 0x00, 0xB6, 0x7A, 0x42}};


char* get_vids_codec_name(){
//  unsigned long fccHandler=avi_header.video.fccHandler;
  unsigned long fccHandler=avi_header.bih.biCompression;
  avi_header.yuv_supported=0;
  avi_header.yuv_hack_needed=0;
  avi_header.flipped=0;
  avi_header.vids_guid=NULL;

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

	case mmioFOURCC('D', 'I', 'V', '5'):
        case mmioFOURCC('d', 'i', 'v', '5'):
	case mmioFOURCC('D', 'I', 'V', '6'):
        case mmioFOURCC('d', 'i', 'v', '6'):
          avi_header.bih.biCompression-=0x02000000; // div5->div3, div6->div4
	case mmioFOURCC('D', 'I', 'V', '3'):
	case mmioFOURCC('d', 'i', 'v', '3'):
	case mmioFOURCC('D', 'I', 'V', '4'):
        case mmioFOURCC('d', 'i', 'v', '4'):
	case mmioFOURCC('M', 'P', '4', '1'):
	case mmioFOURCC('m', 'p', '4', '1'):
	  printf("Video in DivX ;-) format\n");
          avi_header.yuv_supported=1;
#ifdef USE_DIRECTSHOW
          avi_header.vids_guid=&CLSID_DivxDecompressorCF;
          return "divx_c32.ax";
#else
          avi_header.yuv_hack_needed=1;
#ifdef ANGELPOTION
          return "APmpg4v1.dll";
#else
          return "divxc32.dll";
#endif
#endif

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
//		    return "M3JPEG32.dll";
          return "mcmjpg32.dll";
//          return "m3jpeg32.dll";
//          return "libavi_mjpeg.so";

	case mmioFOURCC('W', 'M', 'V', '1'):
	  printf("Video in Windows Media Video 1 format\n");
          avi_header.yuv_supported=1;
          avi_header.vids_guid=&wmv1_clsid;
          return "wmvds32.ax";


  }
  printf("UNKNOWN video codec: %.4s (0x%0X)\n",&fccHandler,fccHandler);
  printf("If you know this video format and codec, you can edit codecs.c in the source!\n");
  printf("Please contact the author, send this movie to be supported by future version.\n");
  return NULL;
}

char* get_auds_codec_name(){
  int id=((WAVEFORMATEX*)avi_header.wf_ext)->wFormatTag;
  avi_header.auds_guid=NULL;
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
            avi_header.auds_guid=&CLSID_Voxware;
            return "voxmsdec.ax";
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

