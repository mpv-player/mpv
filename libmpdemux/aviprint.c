
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

// for avi_stream_id():
#include "stream.h"
#include "demuxer.h"

#include "aviheader.h"
#include "ms_hdr.h"

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
  printf("==========================\n");
}

void print_strh(AVIStreamHeader *h){
  printf("====== STREAM Header =====\n");
  printf("Type: %.4s   FCC: %.4s (%X)\n",(char *)&h->fccType,(char *)&h->fccHandler,(unsigned int)h->fccHandler);
  printf("Flags: %ld\n",h->dwFlags);
  printf("Priority: %d   Language: %d\n",h->wPriority,h->wLanguage);
  printf("InitialFrames: %ld\n",h->dwInitialFrames);
  printf("Rate: %ld/%ld = %5.3f\n",h->dwRate,h->dwScale,(float)h->dwRate/(float)h->dwScale);
  printf("Start: %ld   Len: %ld\n",h->dwStart,h->dwLength);
  printf("Suggested BufferSize: %ld\n",h->dwSuggestedBufferSize);
  printf("Quality %ld\n",h->dwQuality);
  printf("Sample size: %ld\n",h->dwSampleSize);
  printf("==========================\n");
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
      MPEGLAYER3WAVEFORMAT* h2=(MPEGLAYER3WAVEFORMAT *)h;
      printf("mp3.wID=%d\n",h2->wID);
      printf("mp3.fdwFlags=0x%lX\n",h2->fdwFlags);
      printf("mp3.nBlockSize=%d\n",h2->nBlockSize);
      printf("mp3.nFramesPerBlock=%d\n",h2->nFramesPerBlock);
      printf("mp3.nCodecDelay=%d\n",h2->nCodecDelay);
  }
  else if (h->cbSize > 0)
  {
    int i;
    uint8_t* p = ((uint8_t*)h) + sizeof(WAVEFORMATEX);
    printf("Unknown extra header dump: ");
    for (i = 0; i < h->cbSize; i++)
	printf("[%x] ", p[i]);
    printf("\n");
  }
  printf("===========================\n");
}


void print_video_header(BITMAPINFOHEADER *h){
  printf("======= VIDEO Format ======\n");
	printf("  biSize %d\n", h->biSize);
	printf("  biWidth %d\n", h->biWidth);
	printf("  biHeight %d\n", h->biHeight);
	printf("  biPlanes %d\n", h->biPlanes);
	printf("  biBitCount %d\n", h->biBitCount);
	printf("  biCompression %d='%.4s'\n", h->biCompression, (char *)&h->biCompression);
	printf("  biSizeImage %d\n", h->biSizeImage);
  if (h->biSize > sizeof(BITMAPINFOHEADER))
  {
    int i;
    uint8_t* p = ((uint8_t*)h) + sizeof(BITMAPINFOHEADER);
    printf("Unknown extra header dump: ");
    for (i = 0; i < h->biSize-sizeof(BITMAPINFOHEADER); i++)
	printf("[%x] ", *(p+i));
    printf("\n");
  }
  printf("===========================\n");
}

void print_vprp(VideoPropHeader *vprp){
  int i;
  printf("======= Video Properties Header =======\n");
  printf("Format: %d  VideoStandard: %d\n",
         vprp->VideoFormatToken,vprp->VideoStandard);
  printf("VRefresh: %d  HTotal: %d  VTotal: %d\n",
         vprp->dwVerticalRefreshRate, vprp->dwHTotalInT, vprp->dwVTotalInLines);
  printf("FrameAspect: %d:%d  Framewidth: %d  Frameheight: %d\n",
         vprp->dwFrameAspectRatio >> 16, vprp->dwFrameAspectRatio & 0xffff,
         vprp->dwFrameWidthInPixels, vprp->dwFrameHeightInLines);
  printf("Fields: %d\n", vprp->nbFieldPerFrame);
  for (i=0; i<vprp->nbFieldPerFrame; i++) {
    VIDEO_FIELD_DESC *vfd = &vprp->FieldInfo[i];
    printf("  == Field %d description ==\n", i);
    printf("  CompressedBMHeight: %d  CompressedBMWidth: %d\n",
           vfd->CompressedBMHeight, vfd->CompressedBMWidth);
    printf("  ValidBMHeight: %d  ValidBMWidth: %d\n",
           vfd->ValidBMHeight, vfd->ValidBMWidth);
    printf("  ValidBMXOffset: %d  ValidBMYOffset: %d\n",
           vfd->ValidBMXOffset, vfd->ValidBMYOffset);
    printf("  VideoXOffsetInT: %d  VideoYValidStartLine: %d\n",
           vfd->VideoXOffsetInT, vfd->VideoYValidStartLine);
  }
  printf("=======================================\n");
}

void print_index(AVIINDEXENTRY *idx,int idx_size){
  int i;
  unsigned int pos[256];
  unsigned int num[256];
  for(i=0;i<256;i++) num[i]=pos[i]=0;
  for(i=0;i<idx_size;i++){
    int id=avi_stream_id(idx[i].ckid);
    if(id<0 || id>255) id=255;
    printf("%5d:  %.4s  %4X  %016llX  len:%6ld  pos:%7d->%7.3f %7d->%7.3f\n",i,
      (char *)&idx[i].ckid,
      (unsigned int)idx[i].dwFlags&0xffff,
      (uint64_t)AVI_IDX_OFFSET(&idx[i]),
//      idx[i].dwChunkOffset+demuxer->movi_start,
      idx[i].dwChunkLength,
      pos[id],(float)pos[id]/18747.0f,
      num[id],(float)num[id]/23.976f
    );
    pos[id]+=idx[i].dwChunkLength;
    ++num[id];
  }
}

void print_avistdindex_chunk(avistdindex_chunk *h){
    mp_msg (MSGT_HEADER, MSGL_V, "====== AVI Standard Index Header ========\n");
    mp_msg (MSGT_HEADER, MSGL_V, "  FCC (%.4s) dwSize (%d) wLongsPerEntry(%d)\n", h->fcc, h->dwSize, h->wLongsPerEntry);
    mp_msg (MSGT_HEADER, MSGL_V, "  bIndexSubType (%d) bIndexType (%d)\n", h->bIndexSubType, h->bIndexType);
    mp_msg (MSGT_HEADER, MSGL_V, "  nEntriesInUse (%d) dwChunkId (%.4s)\n", h->nEntriesInUse, h->dwChunkId);
    mp_msg (MSGT_HEADER, MSGL_V, "  qwBaseOffset (0x%llX) dwReserved3 (%d)\n", h->qwBaseOffset, h->dwReserved3);
    mp_msg (MSGT_HEADER, MSGL_V, "===========================\n");
}
void print_avisuperindex_chunk(avisuperindex_chunk *h){
    mp_msg (MSGT_HEADER, MSGL_V, "====== AVI Super Index Header ========\n");
    mp_msg (MSGT_HEADER, MSGL_V, "  FCC (%.4s) dwSize (%d) wLongsPerEntry(%d)\n", h->fcc, h->dwSize, h->wLongsPerEntry);
    mp_msg (MSGT_HEADER, MSGL_V, "  bIndexSubType (%d) bIndexType (%d)\n", h->bIndexSubType, h->bIndexType);
    mp_msg (MSGT_HEADER, MSGL_V, "  nEntriesInUse (%d) dwChunkId (%.4s)\n", h->nEntriesInUse, h->dwChunkId);
    mp_msg (MSGT_HEADER, MSGL_V, "  dwReserved[0] (%d) dwReserved[1] (%d) dwReserved[2] (%d)\n", 
	    h->dwReserved[0], h->dwReserved[1], h->dwReserved[2]);
    mp_msg (MSGT_HEADER, MSGL_V, "===========================\n");
}

