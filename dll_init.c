
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

extern int verbose; // defined in mplayer.c

#include "stream.h"
#include "demuxer.h"

#include "loader.h"
//#include "wine/mmreg.h"
//#include "wine/vfw.h"
#include "wine/avifmt.h"

#include "codec-cfg.h"
#include "stheader.h"

#include "libvo/img_format.h"
#include "linux/shmem.h"

extern char* win32_codec_name;  // must be set before calling DrvOpen() !!!

// ACM audio and VfW video codecs initialization
// based on the avifile library [http://divx.euro.ru]

int init_acm_audio_codec(sh_audio_t *sh_audio){
    HRESULT ret;
    WAVEFORMATEX *in_fmt=sh_audio->wf;
    unsigned long srcsize=0;

  if(verbose) printf("======= Win32 (ACM) AUDIO Codec init =======\n");

    sh_audio->srcstream=NULL;

//    if(in_fmt->nSamplesPerSec==0){  printf("Bad WAVE header!\n");exit(1);  }
//    MSACM_RegisterAllDrivers();

    sh_audio->o_wf.nChannels=in_fmt->nChannels;
    sh_audio->o_wf.nSamplesPerSec=in_fmt->nSamplesPerSec;
    sh_audio->o_wf.nAvgBytesPerSec=2*sh_audio->o_wf.nSamplesPerSec*sh_audio->o_wf.nChannels;
    sh_audio->o_wf.wFormatTag=WAVE_FORMAT_PCM;
    sh_audio->o_wf.nBlockAlign=2*in_fmt->nChannels;
    sh_audio->o_wf.wBitsPerSample=16;
    sh_audio->o_wf.cbSize=0;

    win32_codec_name = sh_audio->codec->dll;
    ret=acmStreamOpen(&sh_audio->srcstream,(HACMDRIVER)NULL,
                    in_fmt,&sh_audio->o_wf,
		    NULL,0,0,0);
    if(ret){
        if(ret==ACMERR_NOTPOSSIBLE)
            printf("ACM_Decoder: Unappropriate audio format\n");
        else
            printf("ACM_Decoder: acmStreamOpen error %d", (int)ret);
        sh_audio->srcstream=NULL;
        return 0;
    }
    if(verbose) printf("Audio codec opened OK! ;-)\n");

    acmStreamSize(sh_audio->srcstream, in_fmt->nBlockAlign, &srcsize, ACM_STREAMSIZEF_SOURCE);
    srcsize*=2;
    if(srcsize<MAX_OUTBURST) srcsize=MAX_OUTBURST;
    sh_audio->audio_out_minsize=srcsize; // audio output min. size
    if(verbose) printf("Audio ACM output buffer min. size: %ld\n",srcsize);

    acmStreamSize(sh_audio->srcstream, srcsize, &srcsize, ACM_STREAMSIZEF_DESTINATION);
    sh_audio->audio_in_minsize=srcsize; // audio input min. size
    if(verbose) printf("Audio ACM input buffer min. size: %ld\n",srcsize);
    
    if(srcsize<in_fmt->nBlockAlign) srcsize=in_fmt->nBlockAlign;

    sh_audio->a_in_buffer_size=2*sh_audio->audio_in_minsize;
    sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
    sh_audio->a_in_buffer_len=0;

    return 1;
}

int acm_decode_audio(sh_audio_t *sh_audio, void* a_buffer,int len){
        ACMSTREAMHEADER ash;
        HRESULT hr;
        DWORD srcsize=0;
        acmStreamSize(sh_audio->srcstream,len , &srcsize, ACM_STREAMSIZEF_DESTINATION);
        if(verbose>=3)printf("acm says: srcsize=%ld  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,len);
//        if(srcsize==0) srcsize=((WAVEFORMATEX *)&sh_audio->o_wf_ext)->nBlockAlign;
        if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
        if(sh_audio->a_in_buffer_len<srcsize){
          sh_audio->a_in_buffer_len+=
            demux_read_data(sh_audio->ds,&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len],
            srcsize-sh_audio->a_in_buffer_len);
        }
        if(verbose>=3)printf("acm convert %d -> %d bytes\n",sh_audio->a_in_buffer_len,len);
        memset(&ash, 0, sizeof(ash));
        ash.cbStruct=sizeof(ash);
        ash.fdwStatus=0;
        ash.dwUser=0; 
        ash.pbSrc=sh_audio->a_in_buffer;
        ash.cbSrcLength=sh_audio->a_in_buffer_len;
        ash.pbDst=a_buffer;
        ash.cbDstLength=len;
        hr=acmStreamPrepareHeader(sh_audio->srcstream,&ash,0);
        if(hr){
          printf("ACM_Decoder: acmStreamPrepareHeader error %d\n",(int)hr);
					return -1;
        }
        hr=acmStreamConvert(sh_audio->srcstream,&ash,0);
        if(hr){
          if(verbose>=2) printf("ACM_Decoder: acmStreamConvert error %d\n",(int)hr);
          
//					return -1;
        }
        if(verbose)
          printf("acm converted %d -> %d\n",ash.cbSrcLengthUsed,ash.cbDstLengthUsed);
        if(ash.cbSrcLengthUsed>=sh_audio->a_in_buffer_len){
          sh_audio->a_in_buffer_len=0;
        } else {
          sh_audio->a_in_buffer_len-=ash.cbSrcLengthUsed;
          memcpy(sh_audio->a_in_buffer,&sh_audio->a_in_buffer[ash.cbSrcLengthUsed],sh_audio->a_in_buffer_len);
        }
        len=ash.cbDstLengthUsed;
        hr=acmStreamUnprepareHeader(sh_audio->srcstream,&ash,0);
        if(hr){
          printf("ACM_Decoder: acmStreamUnprepareHeader error %d\n",(int)hr);
        }
        return len;
}



int init_video_codec(sh_video_t *sh_video){
  HRESULT ret;
  int yuv=0;
  unsigned int outfmt=sh_video->codec->outfmt[sh_video->outfmtidx];

  if(verbose) printf("======= Win32 (VFW) VIDEO Codec init =======\n");

  memset(&sh_video->o_bih, 0, sizeof(BITMAPINFOHEADER));
  sh_video->o_bih.biSize = sizeof(BITMAPINFOHEADER);

  win32_codec_name = sh_video->codec->dll;
//  sh_video->hic = ICOpen( 0x63646976, sh_video->bih->biCompression, ICMODE_FASTDECOMPRESS);
  sh_video->hic = ICOpen( 0x63646976, sh_video->bih->biCompression, ICMODE_DECOMPRESS);
  if(!sh_video->hic){
    printf("ICOpen failed! unknown codec / wrong parameters?\n");
    return 0;
  }

//  sh_video->bih.biBitCount=32;

  ret = ICDecompressGetFormat(sh_video->hic, sh_video->bih, &sh_video->o_bih);
  if(ret){
    printf("ICDecompressGetFormat failed: Error %d\n", (int)ret);
    return 0;
  }
  if(verbose) printf("ICDecompressGetFormat OK\n");
  
//  printf("ICM_DECOMPRESS_QUERY=0x%X",ICM_DECOMPRESS_QUERY);

//  sh_video->o_bih.biWidth=sh_video->bih.biWidth;
//  sh_video->o_bih.biCompression = 0x32315659; //  mmioFOURCC('U','Y','V','Y');
//  ret=ICDecompressGetFormatSize(sh_video->hic,&sh_video->o_bih);
//  sh_video->o_bih.biCompression = 3; //0x32315659;
//  sh_video->o_bih.biCompression = mmioFOURCC('U','Y','V','Y');
//  sh_video->o_bih.biCompression = mmioFOURCC('U','Y','V','Y');
//  sh_video->o_bih.biCompression = mmioFOURCC('Y','U','Y','2');
//  sh_video->o_bih.biPlanes=3;
//  sh_video->o_bih.biBitCount=16;

#if 0
  // workaround for pegasus MJPEG:
  if(!sh_video->o_bih.biWidth) sh_video->o_bih.biWidth=sh_video->bih->biWidth;
  if(!sh_video->o_bih.biHeight) sh_video->o_bih.biHeight=sh_video->bih->biHeight;
  if(!sh_video->o_bih.biPlanes) sh_video->o_bih.biPlanes=sh_video->bih->biPlanes;
#endif

  switch (outfmt) {

/* planar format */
  case IMGFMT_YV12:
  case IMGFMT_I420:
  case IMGFMT_IYUV:
      sh_video->o_bih.biBitCount=12;
      yuv=1;
      break;

/* packed format */
  case IMGFMT_YUY2:
  case IMGFMT_UYVY:
  case IMGFMT_YVYU:
      sh_video->o_bih.biBitCount=16;
      yuv=1;
      break;

/* rgb/bgr format */
  case IMGFMT_RGB8:
  case IMGFMT_BGR8:
      sh_video->o_bih.biBitCount=8;
      break;

  case IMGFMT_RGB15:
  case IMGFMT_RGB16:
  case IMGFMT_BGR15:
  case IMGFMT_BGR16:
      sh_video->o_bih.biBitCount=16;
      break;

  case IMGFMT_RGB24:
  case IMGFMT_BGR24:
      sh_video->o_bih.biBitCount=24;
      break;

  case IMGFMT_RGB32:
  case IMGFMT_BGR32:
      sh_video->o_bih.biBitCount=32;
      break;

  default:
      printf("unsupported image format: 0x%x\n", outfmt);
      return 0;
  }

  sh_video->o_bih.biSizeImage = sh_video->o_bih.biWidth * sh_video->o_bih.biHeight * (sh_video->o_bih.biBitCount/8);
  
  if(!(sh_video->codec->outflags[sh_video->outfmtidx]&CODECS_FLAG_FLIP)) {
      sh_video->o_bih.biHeight=-sh_video->bih->biHeight; // flip image!
  }

  if(yuv && !(sh_video->codec->outflags[sh_video->outfmtidx] & CODECS_FLAG_YUVHACK))
	 sh_video->o_bih.biCompression = outfmt;

  if(verbose) {
    printf("Starting decompression, format:\n");
	printf("  biSize %ld\n", sh_video->bih->biSize);
	printf("  biWidth %ld\n", sh_video->bih->biWidth);
	printf("  biHeight %ld\n", sh_video->bih->biHeight);
	printf("  biPlanes %d\n", sh_video->bih->biPlanes);
	printf("  biBitCount %d\n", sh_video->bih->biBitCount);
	printf("  biCompression 0x%lx ('%.4s')\n", sh_video->bih->biCompression, (char *)&sh_video->bih->biCompression);
	printf("  biSizeImage %ld\n", sh_video->bih->biSizeImage);
    printf("Dest fmt:\n");
	printf("  biSize %ld\n", sh_video->o_bih.biSize);
	printf("  biWidth %ld\n", sh_video->o_bih.biWidth);
	printf("  biHeight %ld\n", sh_video->o_bih.biHeight);
	printf("  biPlanes %d\n", sh_video->o_bih.biPlanes);
	printf("  biBitCount %d\n", sh_video->o_bih.biBitCount);
	printf("  biCompression 0x%lx ('%.4s')\n", sh_video->o_bih.biCompression, (char *)&sh_video->o_bih.biCompression);
	printf("  biSizeImage %ld\n", sh_video->o_bih.biSizeImage);
  }

  ret = ICDecompressQuery(sh_video->hic, sh_video->bih, &sh_video->o_bih);
  if(ret){
    printf("ICDecompressQuery failed: Error %d\n", (int)ret);
    return 0;
  }
  if(verbose) printf("ICDecompressQuery OK\n");

  ret = ICDecompressBegin(sh_video->hic, sh_video->bih, &sh_video->o_bih);
  if(ret){
    printf("ICDecompressBegin failed: Error %d\n", (int)ret);
    return 0;
  }

  sh_video->our_out_buffer = shmem_alloc(sh_video->o_bih.biSizeImage);
  if(!sh_video->our_out_buffer){
    printf("not enough memory for decoded picture buffer (%ld bytes)\n", sh_video->o_bih.biSizeImage);
    return 0;
  }

  if(yuv && sh_video->codec->outflags[sh_video->outfmtidx] & CODECS_FLAG_YUVHACK)
    sh_video->o_bih.biCompression = outfmt;

//  avi_header.our_in_buffer=malloc(avi_header.video.dwSuggestedBufferSize); // FIXME!!!!
  
  if(verbose) printf("VIDEO CODEC Init OK!!! ;-)\n");
  return 1;
}
