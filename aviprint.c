
#include <stdio.h>
#include <stdlib.h>

//extern int verbose; // defined in mplayer.c

#include "stream.h"
#include "demuxer.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

//#include "codec-cfg.h"
//#include "stheader.h"


void print_avih(MainAVIHeader *h){
  printf("======= AVI Header =======\n");
  printf("us/frame: %d  (fps=%5.3f)\n",h->dwMicroSecPerFrame,1000000.0f/(float)h->dwMicroSecPerFrame);
  printf("max bytes/sec: %d\n",h->dwMaxBytesPerSec);
  printf("padding: %d\n",h->dwPaddingGranularity);
  printf("flags: (%d)%s%s%s%s%s%s\n",h->dwFlags,
    (h->dwFlags&AVIF_HASINDEX)?" HAS_INDEX":"",
    (h->dwFlags&AVIF_MUSTUSEINDEX)?" MUST_USE_INDEX":"",
    (h->dwFlags&AVIF_ISINTERLEAVED)?" IS_INTERLEAVED":"",
    (h->dwFlags&AVIF_TRUSTCKTYPE)?" TRUST_CKTYPE":"",
    (h->dwFlags&AVIF_WASCAPTUREFILE)?" WAS_CAPTUREFILE":"",
    (h->dwFlags&AVIF_COPYRIGHTED)?" COPYRIGHTED":""
  );
  printf("frames  total: %d   initial: %d\n",h->dwTotalFrames,h->dwInitialFrames);
  printf("streams: %d\n",h->dwStreams);
  printf("Suggested BufferSize: %d\n",h->dwSuggestedBufferSize);
  printf("Size:  %d x %d\n",h->dwWidth,h->dwHeight);
}

void print_strh(AVIStreamHeader *h){
  printf("======= STREAM Header =======\n");
  printf("Type: %.4s   FCC: %.4s (%X)\n",&h->fccType,&h->fccHandler,h->fccHandler);
  printf("Flags: %d\n",h->dwFlags);
  printf("Priority: %d   Language: %d\n",h->wPriority,h->wLanguage);
  printf("InitialFrames: %d\n",h->dwInitialFrames);
  printf("Rate: %d/%d = %5.3f\n",h->dwRate,h->dwScale,(float)h->dwRate/(float)h->dwScale);
  printf("Start: %d   Len: %d\n",h->dwStart,h->dwLength);
  printf("Suggested BufferSize: %d\n",h->dwSuggestedBufferSize);
  printf("Quality %d\n",h->dwQuality);
  printf("Sample size: %d\n",h->dwSampleSize);
}

void print_wave_header(WAVEFORMATEX *h){

  printf("======= WAVE Format =======\n");
  printf("Format Tag: %d (0x%X)\n",h->wFormatTag,h->wFormatTag);
  printf("Channels: %d\n",h->nChannels);
  printf("Samplerate: %d\n",h->nSamplesPerSec);
  printf("avg byte/sec: %d\n",h->nAvgBytesPerSec);
  printf("Block align: %d\n",h->nBlockAlign);
  printf("bits/sample: %d\n",h->wBitsPerSample);
  printf("cbSize: %d\n",h->cbSize);
}


void print_video_header(BITMAPINFOHEADER *h){
  printf("======= VIDEO Format ======\n");
	printf("  biSize %d\n", h->biSize);
	printf("  biWidth %d\n", h->biWidth);
	printf("  biHeight %d\n", h->biHeight);
	printf("  biPlanes %d\n", h->biPlanes);
	printf("  biBitCount %d\n", h->biBitCount);
	printf("  biCompression %d='%.4s'\n", h->biCompression, &h->biCompression);
	printf("  biSizeImage %d\n", h->biSizeImage);
  printf("===========================\n");
}


void print_index(AVIINDEXENTRY *idx,int idx_size){
  int i;
  for(i=0;i<idx_size;i++){
    printf("%5d:  %.4s  %4X  %08X  %d\n",i,
      &idx[i].ckid,
      idx[i].dwFlags,
      idx[i].dwChunkOffset,
//      idx[i].dwChunkOffset+demuxer->movi_start,
      idx[i].dwChunkLength
    );
  }
}


