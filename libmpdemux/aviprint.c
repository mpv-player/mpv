
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#include "stream.h"
#include "demuxer.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

//#include "codec-cfg.h"
//#include "stheader.h"

void print_avih_flags(MainAVIHeader *h){
  printf("MainAVIHeader.dwFlags: (%ld)%s%s%s%s%s%s\n",h->dwFlags,
    (h->dwFlags&AVIF_HASINDEX)?" HAS_INDEX":"",
    (h->dwFlags&AVIF_MUSTUSEINDEX)?" MUST_USE_INDEX":"",
    (h->dwFlags&AVIF_ISINTERLEAVED)?" IS_INTERLEAVED":"",
    (h->dwFlags&AVIF_TRUSTCKTYPE)?" TRUST_CKTYPE":"",
    (h->dwFlags&AVIF_WASCAPTUREFILE)?" WAS_CAPTUREFILE":"",
    (h->dwFlags&AVIF_COPYRIGHTED)?" COPYRIGHTED":""
  );
}

void print_avih(MainAVIHeader *h){
  printf("======= AVI Header =======\n");
  printf("us/frame: %ld  (fps=%5.3f)\n",h->dwMicroSecPerFrame,1000000.0f/(float)h->dwMicroSecPerFrame);
  printf("max bytes/sec: %ld\n",h->dwMaxBytesPerSec);
  printf("padding: %ld\n",h->dwPaddingGranularity);
  print_avih_flags(h);
  printf("frames  total: %ld   initial: %ld\n",h->dwTotalFrames,h->dwInitialFrames);
  printf("streams: %ld\n",h->dwStreams);
  printf("Suggested BufferSize: %ld\n",h->dwSuggestedBufferSize);
  printf("Size:  %ld x %ld\n",h->dwWidth,h->dwHeight);
}

void print_strh(AVIStreamHeader *h){
  printf("======= STREAM Header =======\n");
  printf("Type: %.4s   FCC: %.4s (%X)\n",(char *)&h->fccType,(char *)&h->fccHandler,(unsigned int)h->fccHandler);
  printf("Flags: %ld\n",h->dwFlags);
  printf("Priority: %d   Language: %d\n",h->wPriority,h->wLanguage);
  printf("InitialFrames: %ld\n",h->dwInitialFrames);
  printf("Rate: %ld/%ld = %5.3f\n",h->dwRate,h->dwScale,(float)h->dwRate/(float)h->dwScale);
  printf("Start: %ld   Len: %ld\n",h->dwStart,h->dwLength);
  printf("Suggested BufferSize: %ld\n",h->dwSuggestedBufferSize);
  printf("Quality %ld\n",h->dwQuality);
  printf("Sample size: %ld\n",h->dwSampleSize);
}

void print_wave_header(WAVEFORMATEX *h){
  printf("======= WAVE Format =======\n");
  printf("Format Tag: %d (0x%X)\n",h->wFormatTag,h->wFormatTag);
  printf("Channels: %d\n",h->nChannels);
  printf("Samplerate: %ld\n",h->nSamplesPerSec);
  printf("avg byte/sec: %ld\n",h->nAvgBytesPerSec);
  printf("Block align: %d\n",h->nBlockAlign);
  printf("bits/sample: %d\n",h->wBitsPerSample);
  printf("cbSize: %d\n",h->cbSize);
  if(h->wFormatTag==0x55 && h->cbSize>=12){
      MPEGLAYER3WAVEFORMAT* h2=h;
      printf("mp3.wID=%d\n",h2->wID);
      printf("mp3.fdwFlags=0x%X\n",h2->fdwFlags);
      printf("mp3.nBlockSize=%d\n",h2->nBlockSize);
      printf("mp3.nFramesPerBlock=%d\n",h2->nFramesPerBlock);
      printf("mp3.nCodecDelay=%d\n",h2->nCodecDelay);
  }
}


void print_video_header(BITMAPINFOHEADER *h){
  printf("======= VIDEO Format ======\n");
	printf("  biSize %ld\n", h->biSize);
	printf("  biWidth %ld\n", h->biWidth);
	printf("  biHeight %ld\n", h->biHeight);
	printf("  biPlanes %d\n", h->biPlanes);
	printf("  biBitCount %d\n", h->biBitCount);
	printf("  biCompression %ld='%.4s'\n", h->biCompression, (char *)&h->biCompression);
	printf("  biSizeImage %ld\n", h->biSizeImage);
  printf("===========================\n");
}


void print_index(AVIINDEXENTRY *idx,int idx_size){
  int i;
  unsigned int pos[256];
  unsigned int num[256];
  for(i=0;i<256;i++) num[i]=pos[i]=0;
  for(i=0;i<idx_size;i++){
    int id=avi_stream_id(idx[i].ckid);
    if(id<0 || id>255) id=255;
    printf("%5d:  %.4s  %4X  %08X  len:%6ld  pos:%7d->%7.3f %7d->%7.3f\n",i,
      (char *)&idx[i].ckid,
      (unsigned int)idx[i].dwFlags,
      (unsigned int)idx[i].dwChunkOffset,
//      idx[i].dwChunkOffset+demuxer->movi_start,
      idx[i].dwChunkLength,
      pos[id],(float)pos[id]/18747.0f,
      num[id],(float)num[id]/23.976f
    );
    pos[id]+=idx[i].dwChunkLength;
    ++num[id];
  }
}


