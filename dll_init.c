
#include "config.h"

#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <unistd.h>

#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"

#include "loader.h"
//#include "wine/mmreg.h"
#include "wine/vfw.h"
#include "wine/avifmt.h"

#include "codec-cfg.h"
#include "stheader.h"

#include "dll_init.h"

#ifdef USE_LIBVO2
#include "libvo2/img_format.h"
#else
#include "libvo/img_format.h"
#endif
#include "linux/shmem.h"

extern int divx_quality;

// ACM audio and VfW video codecs initialization
// based on the avifile library [http://divx.euro.ru]

int init_acm_audio_codec(sh_audio_t *sh_audio){
    HRESULT ret;
    WAVEFORMATEX *in_fmt=sh_audio->wf;
    unsigned int srcsize=0;

    mp_msg(MSGT_WIN32,MSGL_V,"======= Win32 (ACM) AUDIO Codec init =======\n");

    sh_audio->srcstream=NULL;

    sh_audio->o_wf.nChannels=in_fmt->nChannels;
    sh_audio->o_wf.nSamplesPerSec=in_fmt->nSamplesPerSec;
    sh_audio->o_wf.nAvgBytesPerSec=2*sh_audio->o_wf.nSamplesPerSec*sh_audio->o_wf.nChannels;
    sh_audio->o_wf.wFormatTag=WAVE_FORMAT_PCM;
    sh_audio->o_wf.nBlockAlign=2*in_fmt->nChannels;
    sh_audio->o_wf.wBitsPerSample=16;
//    sh_audio->o_wf.wBitsPerSample=in_fmt->wBitsPerSample;
    sh_audio->o_wf.cbSize=0;

    if(verbose) {
#if 0
	printf("Input format:\n");
	    printf("  wFormatTag %d\n", in_fmt->wFormatTag);
	    printf("  nChannels %d\n", in_fmt->nChannels);
	    printf("  nSamplesPerSec %ld\n", in_fmt->nSamplesPerSec);
	    printf("  nAvgBytesPerSec %d\n", in_fmt->nAvgBytesPerSec);
	    printf("  nBlockAlign %d\n", in_fmt->nBlockAlign);
	    printf("  wBitsPerSample %d\n", in_fmt->wBitsPerSample);
	    printf("  cbSize %d\n", in_fmt->cbSize);
	printf("Output fmt:\n");
	    printf("  wFormatTag %d\n", sh_audio->o_wf.wFormatTag);
	    printf("  nChannels %d\n", sh_audio->o_wf.nChannels);
	    printf("  nSamplesPerSec %ld\n", sh_audio->o_wf.nSamplesPerSec);
	    printf("  nAvgBytesPerSec %d\n", sh_audio->o_wf.nAvgBytesPerSec);
	    printf("  nBlockAlign %d\n", sh_audio->o_wf.nBlockAlign);
	    printf("  wBitsPerSample %d\n", sh_audio->o_wf.wBitsPerSample);
	    printf("  cbSize %d\n", sh_audio->o_wf.cbSize);
#else
	printf("Input format:\n");
	print_wave_header(in_fmt);
	printf("Output fmt:\n");
	print_wave_header(&sh_audio->o_wf);
	printf("\n");
#endif
    }


    win32_codec_name = sh_audio->codec->dll;
    ret=acmStreamOpen(&sh_audio->srcstream,(HACMDRIVER)NULL,
                    in_fmt,&sh_audio->o_wf,
		    NULL,0,0,0);
    if(ret){
        if(ret==ACMERR_NOTPOSSIBLE)
            mp_msg(MSGT_WIN32,MSGL_ERR,"ACM_Decoder: Unappropriate audio format\n");
        else
            mp_msg(MSGT_WIN32,MSGL_ERR,"ACM_Decoder: acmStreamOpen error: %d", (int)ret);
        sh_audio->srcstream=NULL;
        return 0;
    }
    mp_msg(MSGT_WIN32,MSGL_V,"Audio codec opened OK! ;-)\n");

    acmStreamSize(sh_audio->srcstream, in_fmt->nBlockAlign, &srcsize, ACM_STREAMSIZEF_SOURCE);
    //if(verbose) printf("Audio ACM output buffer min. size: %ld (reported by codec)\n",srcsize);
    srcsize*=2;
    //if(srcsize<MAX_OUTBURST) srcsize=MAX_OUTBURST;
    if(!srcsize){
        mp_msg(MSGT_WIN32,MSGL_WARN,"Warning! ACM codec reports srcsize=0\n");
        srcsize=16384;
    }
    // limit srcsize to 4-16kb
    //while(srcsize && srcsize<4096) srcsize*=2;
    //while(srcsize>16384) srcsize/=2;
    sh_audio->audio_out_minsize=srcsize; // audio output min. size
    mp_msg(MSGT_WIN32,MSGL_V,"Audio ACM output buffer min. size: %ld\n",srcsize);

    acmStreamSize(sh_audio->srcstream, srcsize, &srcsize, ACM_STREAMSIZEF_DESTINATION);
//    if(srcsize<in_fmt->nBlockAlign) srcsize=in_fmt->nBlockAlign;

    mp_msg(MSGT_WIN32,MSGL_V,"Audio ACM input buffer min. size: %ld\n",srcsize);

    sh_audio->audio_in_minsize=2*srcsize; // audio input min. size
    
    return 1;
}

int acm_decode_audio(sh_audio_t *sh_audio, void* a_buffer,int minlen,int maxlen){
        ACMSTREAMHEADER ash;
        HRESULT hr;
        DWORD srcsize=0;
        DWORD len=minlen;
        acmStreamSize(sh_audio->srcstream,len , &srcsize, ACM_STREAMSIZEF_DESTINATION);
        mp_msg(MSGT_WIN32,MSGL_DBG3,"acm says: srcsize=%ld  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,len);

        if(srcsize<sh_audio->wf->nBlockAlign){
           srcsize=sh_audio->wf->nBlockAlign;
           acmStreamSize(sh_audio->srcstream, srcsize, &len, ACM_STREAMSIZEF_SOURCE);
           if(len>maxlen) len=maxlen;
        }

//        if(srcsize==0) srcsize=((WAVEFORMATEX *)&sh_audio->o_wf_ext)->nBlockAlign;
        if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
        if(sh_audio->a_in_buffer_len<srcsize){
          sh_audio->a_in_buffer_len+=
            demux_read_data(sh_audio->ds,&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len],
            srcsize-sh_audio->a_in_buffer_len);
        }
        mp_msg(MSGT_WIN32,MSGL_DBG3,"acm convert %d -> %d bytes\n",sh_audio->a_in_buffer_len,len);
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
          mp_msg(MSGT_WIN32,MSGL_V,"ACM_Decoder: acmStreamPrepareHeader error %d\n",(int)hr);
					return -1;
        }
        hr=acmStreamConvert(sh_audio->srcstream,&ash,0);
        if(hr){
          mp_msg(MSGT_WIN32,MSGL_DBG2,"ACM_Decoder: acmStreamConvert error %d\n",(int)hr);
          switch(hr)
	  {
	    case ACMERR_NOTPOSSIBLE:
	    case ACMERR_UNPREPARED:
		mp_msg(MSGT_WIN32, MSGL_DBG2, "ACM_Decoder: acmStreamConvert error: probarly not initialized!\n");
	  }  
//					return -1;
        }
        if(verbose>1)
          mp_msg(MSGT_WIN32,MSGL_DBG2,"acm converted %d -> %d\n",ash.cbSrcLengthUsed,ash.cbDstLengthUsed);
        if(ash.cbSrcLengthUsed>=sh_audio->a_in_buffer_len){
          sh_audio->a_in_buffer_len=0;
        } else {
          sh_audio->a_in_buffer_len-=ash.cbSrcLengthUsed;
          memcpy(sh_audio->a_in_buffer,&sh_audio->a_in_buffer[ash.cbSrcLengthUsed],sh_audio->a_in_buffer_len);
        }
        len=ash.cbDstLengthUsed;
        hr=acmStreamUnprepareHeader(sh_audio->srcstream,&ash,0);
        if(hr){
          mp_msg(MSGT_WIN32,MSGL_V,"ACM_Decoder: acmStreamUnprepareHeader error %d\n",(int)hr);
        }
        return len;
}

int close_acm_audio_codec(sh_audio_t *sh_audio)
{
    HRESULT ret;
    
    ret = acmStreamClose(sh_audio->srcstream, 0);
    
    if (ret)
    switch(ret)
    {
	case ACMERR_BUSY:
	case ACMERR_CANCELED:
	    mp_msg(MSGT_WIN32, MSGL_DBG2, "ACM_Decoder: stream busy, waiting..\n");
	    sleep(100);
	    return(close_acm_audio_codec(sh_audio));
	case ACMERR_UNPREPARED:
	case ACMERR_NOTPOSSIBLE:
	    return(0);
	default:
	    mp_msg(MSGT_WIN32, MSGL_WARN, "ACM_Decoder: unknown error occured: %d\n", ret);
	    return(0);
    }
    
//    MSACM_UnregisterAllDrivers();
    return(1);
}

int init_vfw_video_codec(sh_video_t *sh_video,int ex){
  HRESULT ret;
  int yuv=0;
  unsigned int outfmt=sh_video->codec->outfmt[sh_video->outfmtidx];
  char *temp;
  int temp_len;
  int i;

  mp_msg(MSGT_WIN32,MSGL_V,"======= Win32 (VFW) VIDEO Codec init =======\n");

  memset(&sh_video->o_bih, 0, sizeof(BITMAPINFOHEADER));
  sh_video->o_bih.biSize = sizeof(BITMAPINFOHEADER);

  win32_codec_name = sh_video->codec->dll;
//  sh_video->hic = ICOpen( 0x63646976, sh_video->bih->biCompression, ICMODE_FASTDECOMPRESS);
  sh_video->hic = ICOpen( 0x63646976, sh_video->bih->biCompression, ICMODE_DECOMPRESS);
  if(!sh_video->hic){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICOpen failed! unknown codec / wrong parameters?\n");
    return 0;
  }

//  sh_video->bih->biBitCount=32;

  temp_len = ICDecompressGetFormatSize(sh_video->hic, sh_video->bih);

  if (temp_len < sh_video->o_bih.biSize)
    temp_len = sh_video->o_bih.biSize;

  temp = malloc(temp_len);
  printf("ICDecompressGetFormatSize ret: %d\n", temp_len);

#if 0
{
  ICINFO icinfo;
  ret = ICGetInfo(sh_video->hic, &icinfo, sizeof(ICINFO));
  printf("%d - %d - %d\n", ret, icinfo.dwSize, sizeof(ICINFO));
printf("Compressor type: %.4x\n", icinfo.fccType);
printf("Compressor subtype: %.4x\n", icinfo.fccHandler);
printf("Compressor flags: %lu, version %lu, ICM version: %lu\n",
    icinfo.dwFlags, icinfo.dwVersion, icinfo.dwVersionICM);
printf("Compressor name: %s\n", icinfo.szName);
printf("Compressor description: %s\n", icinfo.szDescription);

printf("Flags:");
if (icinfo.dwFlags & VIDCF_QUALITY)
    printf(" quality");
if (icinfo.dwFlags & VIDCF_FASTTEMPORALD)
    printf(" fast-decompr");
if (icinfo.dwFlags & VIDCF_QUALITYTIME)
    printf(" temp-quality");
printf("\n");
}
#endif

  // Note: DivX.DLL overwrites 4 bytes _AFTER_ the o_bih header, so it corrupts
  // the sh_video struct content. We call it with an 1024-byte temp space and
  // then copy out the data we need:
  memset(temp,0x77,temp_len);
//  memcpy(temp,sh_video->bih,sizeof(BITMAPINFOHEADER));
//  sh_video->o_bih.biSize = temp_len;

  ret = ICDecompressGetFormat(sh_video->hic, sh_video->bih, temp);
  if(ret < 0){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICDecompressGetFormat failed: Error %d\n", (int)ret);
    for (i=0; i < temp_len; i++) mp_msg(MSGT_WIN32, MSGL_DBG2, "%02x ", temp[i]);
    return 0;
  }
  mp_msg(MSGT_WIN32,MSGL_V,"ICDecompressGetFormat OK\n");
  
  memcpy(&sh_video->o_bih,temp,sizeof(sh_video->o_bih));

  if (temp_len > sizeof(sh_video->o_bih))
  {
    mp_msg(MSGT_WIN32, MSGL_V, "Extra info in o_bih (%d bytes)!\n",
	temp_len-sizeof(sh_video->o_bih));
    for(i=sizeof(sh_video->o_bih);i<temp_len;i++) mp_msg(MSGT_WIN32, MSGL_DBG2, "%02X ",temp[i]);
  }
  free(temp);
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
      mp_msg(MSGT_WIN32,MSGL_ERR,"unsupported image format: 0x%x\n", outfmt);
      return 0;
  }

  sh_video->o_bih.biSizeImage = sh_video->o_bih.biWidth * sh_video->o_bih.biHeight * (sh_video->o_bih.biBitCount/8);
  
  if(!(sh_video->codec->outflags[sh_video->outfmtidx]&CODECS_FLAG_FLIP)) {
      sh_video->o_bih.biHeight=-sh_video->bih->biHeight; // flip image!
  }

  if(yuv && !(sh_video->codec->outflags[sh_video->outfmtidx] & CODECS_FLAG_YUVHACK))
	 sh_video->o_bih.biCompression = outfmt;
  else
         sh_video->o_bih.biCompression = 0;

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

  ret = ex ?
      ICDecompressQueryEx(sh_video->hic, sh_video->bih, &sh_video->o_bih) :
      ICDecompressQuery(sh_video->hic, sh_video->bih, &sh_video->o_bih);
  if(ret){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICDecompressQuery failed: Error %d\n", (int)ret);
//    return 0;
  } else
  mp_msg(MSGT_WIN32,MSGL_V,"ICDecompressQuery OK\n");

  ret = ex ?
      ICDecompressBeginEx(sh_video->hic, sh_video->bih, &sh_video->o_bih) :
      ICDecompressBegin(sh_video->hic, sh_video->bih, &sh_video->o_bih);
  if(ret){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICDecompressBegin failed: Error %d\n", (int)ret);
//    return 0;
  }

  sh_video->our_out_buffer = (char*)memalign(64,sh_video->o_bih.biSizeImage);
  if(!sh_video->our_out_buffer){
    mp_msg(MSGT_WIN32,MSGL_ERR,MSGTR_NoMemForDecodedImage, sh_video->o_bih.biSizeImage);
    return 0;
  }

  if(yuv && sh_video->codec->outflags[sh_video->outfmtidx] & CODECS_FLAG_YUVHACK)
    sh_video->o_bih.biCompression = outfmt;

//  avi_header.our_in_buffer=malloc(avi_header.video.dwSuggestedBufferSize); // FIXME!!!!

  ICSendMessage(sh_video->hic, ICM_USER+80, (long)(&divx_quality) ,NULL);

  mp_msg(MSGT_WIN32,MSGL_V,"VIDEO CODEC Init OK!!! ;-)\n");
  return 1;
}

int vfw_set_postproc(sh_video_t* sh_video,int quality){
  // Works only with opendivx/divx4 based DLL
  return ICSendMessage(sh_video->hic, ICM_USER+80, (long)(&quality) ,NULL);
}

int vfw_decode_video(sh_video_t* sh_video,void* start,int in_size,int drop_frame,int ex){
    HRESULT ret;

    sh_video->bih->biSizeImage = in_size;

    if(ex)
      ret = ICDecompressEx(sh_video->hic, 
	  ( (sh_video->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( (drop_frame==2 && !(sh_video->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ) , 
                         sh_video->bih,   start,
                        &sh_video->o_bih,
                        drop_frame ? 0 : sh_video->our_out_buffer);
    else
      ret = ICDecompress(sh_video->hic, 
	  ( (sh_video->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( (drop_frame==2 && !(sh_video->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ) , 
                         sh_video->bih,   start,
                        &sh_video->o_bih,
                        drop_frame ? 0 : sh_video->our_out_buffer);

    return (int)ret;
}

int vfw_close_video_codec(sh_video_t *sh_video, int ex)
{
    HRESULT ret;
    
    ret = ICDecompressEnd(sh_video->hic);
    if (ret)
    {
	mp_msg(MSGT_WIN32, MSGL_WARN, "ICDecompressEnd failed: %d\n", ret);
	return(0);
    }

    ret = ICClose(sh_video->hic);
    if (ret)
    {
	mp_msg(MSGT_WIN32, MSGL_WARN, "ICClose failed: %d\n", ret);
	return(0);
    }

    return(1);
}

/************************ VFW COMPRESSION *****************************/

static int encoder_hic=0;
static void* encoder_buf=NULL;
static int encoder_buf_size=0;
static int encoder_frameno=0;

//int init_vfw_encoder(char *dll_name, BITMAPINFOHEADER *input_bih, BITMAPINFOHEADER *output_bih)
BITMAPINFOHEADER* vfw_open_encoder(char *dll_name, BITMAPINFOHEADER *input_bih,unsigned int out_fourcc)
{
  HRESULT ret;
  BITMAPINFOHEADER* output_bih=NULL;
  int temp_len;

//sh_video = malloc(sizeof(sh_video_t));

  mp_msg(MSGT_WIN32,MSGL_V,"======= Win32 (VFW) VIDEO Encoder init =======\n");

//  memset(&sh_video->o_bih, 0, sizeof(BITMAPINFOHEADER));
//  output_bih->biSize = sizeof(BITMAPINFOHEADER);

  win32_codec_name = dll_name;
  encoder_hic = ICOpen( 0x63646976, out_fourcc, ICMODE_COMPRESS);
  if(!encoder_hic){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICOpen failed! unknown codec / wrong parameters?\n");
    return NULL;
  }
  printf("HIC: %x\n", encoder_hic);

#if 1
{
  ICINFO icinfo;

  ret = ICGetInfo(encoder_hic, &icinfo, sizeof(ICINFO));
  printf("%d - %d - %d\n", ret, icinfo.dwSize, sizeof(ICINFO));
printf("Compressor type: %.4x\n", icinfo.fccType);
printf("Compressor subtype: %.4x\n", icinfo.fccHandler);
printf("Compressor flags: %lu, version %lu, ICM version: %lu\n",
    icinfo.dwFlags, icinfo.dwVersion, icinfo.dwVersionICM);
//printf("Compressor name: %s\n", icinfo.szName);
//printf("Compressor description: %s\n", icinfo.szDescription);

printf("Flags:");
if (icinfo.dwFlags & VIDCF_QUALITY)
    printf(" quality");
if (icinfo.dwFlags & VIDCF_FASTTEMPORALD)
    printf(" fast-decompr");
if (icinfo.dwFlags & VIDCF_QUALITYTIME)
    printf(" temp-quality");
printf("\n");
}
#endif

  temp_len = ICCompressGetFormatSize(encoder_hic, input_bih);
  printf("ICCompressGetFormatSize ret: %d\n", temp_len);

  if (temp_len < sizeof(BITMAPINFOHEADER)) temp_len=sizeof(BITMAPINFOHEADER);

  output_bih = malloc(temp_len+4);
  memset(output_bih,0,temp_len);
  output_bih->biSize = temp_len; //sizeof(BITMAPINFOHEADER);

  return output_bih;
}

int vfw_start_encoder(BITMAPINFOHEADER *input_bih, BITMAPINFOHEADER *output_bih){
  HRESULT ret;
  int temp_len=output_bih->biSize;
  int i;

  ret = ICCompressGetFormat(encoder_hic, input_bih, output_bih);
  if(ret < 0){
    unsigned char* temp=output_bih;
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICCompressGetFormat failed: Error %d  (0x%X)\n", (int)ret, (int)ret);
    for (i=0; i < temp_len; i++) mp_msg(MSGT_WIN32, MSGL_DBG2, "%02x ", temp[i]);
    return 0;
  }
  mp_msg(MSGT_WIN32,MSGL_V,"ICCompressGetFormat OK\n");
  
  if (temp_len > sizeof(BITMAPINFOHEADER))
  {
    unsigned char* temp=output_bih;
    mp_msg(MSGT_WIN32, MSGL_V, "Extra info in o_bih (%d bytes)!\n",
	temp_len-sizeof(BITMAPINFOHEADER));
    for(i=sizeof(output_bih);i<temp_len;i++) mp_msg(MSGT_WIN32, MSGL_DBG2, "%02X ",temp[i]);
  }

//  if(verbose) {
    printf("Starting compression:\n");
    printf(" Input format:\n");
	printf("  biSize %ld\n", input_bih->biSize);
	printf("  biWidth %ld\n", input_bih->biWidth);
	printf("  biHeight %ld\n", input_bih->biHeight);
	printf("  biPlanes %d\n", input_bih->biPlanes);
	printf("  biBitCount %d\n", input_bih->biBitCount);
	printf("  biCompression 0x%lx ('%.4s')\n", input_bih->biCompression, (char *)&input_bih->biCompression);
	printf("  biSizeImage %ld\n", input_bih->biSizeImage);
    printf(" Output format:\n");
	printf("  biSize %ld\n", output_bih->biSize);
	printf("  biWidth %ld\n", output_bih->biWidth);
	printf("  biHeight %ld\n", output_bih->biHeight);
	printf("  biPlanes %d\n", output_bih->biPlanes);
	printf("  biBitCount %d\n", output_bih->biBitCount);
	printf("  biCompression 0x%lx ('%.4s')\n", output_bih->biCompression, (char *)&output_bih->biCompression);
	printf("  biSizeImage %ld\n", output_bih->biSizeImage);
//  }

  output_bih->biWidth=input_bih->biWidth;
  output_bih->biHeight=input_bih->biHeight;

  ret = ICCompressQuery(encoder_hic, input_bih, output_bih);
  if(ret){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICCompressQuery failed: Error %d\n", (int)ret);
    return 0;
  } else
  mp_msg(MSGT_WIN32,MSGL_V,"ICCompressQuery OK\n");

  ret = ICCompressBegin(encoder_hic, input_bih, output_bih);
  if(ret){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICCompressBegin failed: Error %d\n", (int)ret);
//    return 0;
  } else
  mp_msg(MSGT_WIN32,MSGL_V,"ICCompressBegin OK\n");

    printf(" Output format after query/begin:\n");
	printf("  biSize %ld\n", output_bih->biSize);
	printf("  biWidth %ld\n", output_bih->biWidth);
	printf("  biHeight %ld\n", output_bih->biHeight);
	printf("  biPlanes %d\n", output_bih->biPlanes);
	printf("  biBitCount %d\n", output_bih->biBitCount);
	printf("  biCompression 0x%lx ('%.4s')\n", output_bih->biCompression, (char *)&output_bih->biCompression);
	printf("  biSizeImage %ld\n", output_bih->biSizeImage);
  
  encoder_buf_size=input_bih->biSizeImage;
  encoder_buf=malloc(encoder_buf_size);
  encoder_frameno=0;

  mp_msg(MSGT_WIN32,MSGL_V,"VIDEO CODEC Init OK!!! ;-)\n");
  return 1;
}

int vfw_encode_frame(BITMAPINFOHEADER* biOutput,void* OutBuf,
		     BITMAPINFOHEADER* biInput,void* Image,
		     long* keyframe, int quality){
    HRESULT ret;

//long VFWAPIV ICCompress(
//	HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiOutput,void* lpOutputBuf,
//	LPBITMAPINFOHEADER lpbiInput,void* lpImage,long* lpckid,
//	long* lpdwFlags,long lFrameNum,long dwFrameSize,long dwQuality,
//	LPBITMAPINFOHEADER lpbiInputPrev,void* lpImagePrev
//);

//    printf("vfw_encode_frame(%p,%p, %p,%p, %p,%d)\n",biOutput,OutBuf,biInput,Image,keyframe,quality);

    ret=ICCompress(encoder_hic, 0,
	biOutput, OutBuf,
	biInput, Image,
	NULL, keyframe, encoder_frameno, 0, quality,
	biInput, encoder_buf);

//    printf("ok. size=%d\n",biOutput->biSizeImage);

    memcpy(encoder_buf,Image,encoder_buf_size);
    ++encoder_frameno;

    return (int)ret;
}

