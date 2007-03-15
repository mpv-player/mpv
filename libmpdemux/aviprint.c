
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

// for avi_stream_id():
#include "stream/stream.h"
#include "demuxer.h"

#include "aviheader.h"
#include "ms_hdr.h"

//#include "codec-cfg.h"
//#include "stheader.h"

void print_avih_flags(MainAVIHeader *h, int verbose_level){
  mp_msg(MSGT_HEADER, verbose_level, "MainAVIHeader.dwFlags: (%ld)%s%s%s%s%s%s\n",h->dwFlags,
    (h->dwFlags&AVIF_HASINDEX)?" HAS_INDEX":"",
    (h->dwFlags&AVIF_MUSTUSEINDEX)?" MUST_USE_INDEX":"",
    (h->dwFlags&AVIF_ISINTERLEAVED)?" IS_INTERLEAVED":"",
    (h->dwFlags&AVIF_TRUSTCKTYPE)?" TRUST_CKTYPE":"",
    (h->dwFlags&AVIF_WASCAPTUREFILE)?" WAS_CAPTUREFILE":"",
    (h->dwFlags&AVIF_COPYRIGHTED)?" COPYRIGHTED":""
  );
}

void print_avih(MainAVIHeader *h, int verbose_level){
  mp_msg(MSGT_HEADER, verbose_level, "======= AVI Header =======\n");
  mp_msg(MSGT_HEADER, verbose_level, "us/frame: %ld  (fps=%5.3f)\n",h->dwMicroSecPerFrame,1000000.0f/(float)h->dwMicroSecPerFrame);
  mp_msg(MSGT_HEADER, verbose_level, "max bytes/sec: %ld\n",h->dwMaxBytesPerSec);
  mp_msg(MSGT_HEADER, verbose_level, "padding: %ld\n",h->dwPaddingGranularity);
  print_avih_flags(h, verbose_level);
  mp_msg(MSGT_HEADER, verbose_level, "frames  total: %ld   initial: %ld\n",h->dwTotalFrames,h->dwInitialFrames);
  mp_msg(MSGT_HEADER, verbose_level, "streams: %ld\n",h->dwStreams);
  mp_msg(MSGT_HEADER, verbose_level, "Suggested BufferSize: %ld\n",h->dwSuggestedBufferSize);
  mp_msg(MSGT_HEADER, verbose_level, "Size:  %ld x %ld\n",h->dwWidth,h->dwHeight);
  mp_msg(MSGT_HEADER, verbose_level, "==========================\n");
}

void print_strh(AVIStreamHeader *h, int verbose_level){
  mp_msg(MSGT_HEADER, verbose_level, "====== STREAM Header =====\n");
  mp_msg(MSGT_HEADER, verbose_level, "Type: %.4s   FCC: %.4s (%X)\n",(char *)&h->fccType,(char *)&h->fccHandler,(unsigned int)h->fccHandler);
  mp_msg(MSGT_HEADER, verbose_level, "Flags: %ld\n",h->dwFlags);
  mp_msg(MSGT_HEADER, verbose_level, "Priority: %d   Language: %d\n",h->wPriority,h->wLanguage);
  mp_msg(MSGT_HEADER, verbose_level, "InitialFrames: %ld\n",h->dwInitialFrames);
  mp_msg(MSGT_HEADER, verbose_level, "Rate: %ld/%ld = %5.3f\n",h->dwRate,h->dwScale,(float)h->dwRate/(float)h->dwScale);
  mp_msg(MSGT_HEADER, verbose_level, "Start: %ld   Len: %ld\n",h->dwStart,h->dwLength);
  mp_msg(MSGT_HEADER, verbose_level, "Suggested BufferSize: %ld\n",h->dwSuggestedBufferSize);
  mp_msg(MSGT_HEADER, verbose_level, "Quality %ld\n",h->dwQuality);
  mp_msg(MSGT_HEADER, verbose_level, "Sample size: %ld\n",h->dwSampleSize);
  mp_msg(MSGT_HEADER, verbose_level, "==========================\n");
}

void print_wave_header(WAVEFORMATEX *h, int verbose_level){
  mp_msg(MSGT_HEADER, verbose_level, "======= WAVE Format =======\n");
  mp_msg(MSGT_HEADER, verbose_level, "Format Tag: %d (0x%X)\n",h->wFormatTag,h->wFormatTag);
  mp_msg(MSGT_HEADER, verbose_level, "Channels: %d\n",h->nChannels);
  mp_msg(MSGT_HEADER, verbose_level, "Samplerate: %ld\n",h->nSamplesPerSec);
  mp_msg(MSGT_HEADER, verbose_level, "avg byte/sec: %ld\n",h->nAvgBytesPerSec);
  mp_msg(MSGT_HEADER, verbose_level, "Block align: %d\n",h->nBlockAlign);
  mp_msg(MSGT_HEADER, verbose_level, "bits/sample: %d\n",h->wBitsPerSample);
  mp_msg(MSGT_HEADER, verbose_level, "cbSize: %d\n",h->cbSize);
  if(h->wFormatTag==0x55 && h->cbSize>=12){
      MPEGLAYER3WAVEFORMAT* h2=(MPEGLAYER3WAVEFORMAT *)h;
      mp_msg(MSGT_HEADER, verbose_level, "mp3.wID=%d\n",h2->wID);
      mp_msg(MSGT_HEADER, verbose_level, "mp3.fdwFlags=0x%lX\n",h2->fdwFlags);
      mp_msg(MSGT_HEADER, verbose_level, "mp3.nBlockSize=%d\n",h2->nBlockSize);
      mp_msg(MSGT_HEADER, verbose_level, "mp3.nFramesPerBlock=%d\n",h2->nFramesPerBlock);
      mp_msg(MSGT_HEADER, verbose_level, "mp3.nCodecDelay=%d\n",h2->nCodecDelay);
  }
  else if (h->cbSize > 0)
  {
    int i;
    uint8_t* p = ((uint8_t*)h) + sizeof(WAVEFORMATEX);
    mp_msg(MSGT_HEADER, verbose_level, "Unknown extra header dump: ");
    for (i = 0; i < h->cbSize; i++)
	mp_msg(MSGT_HEADER, verbose_level, "[%x] ", p[i]);
    mp_msg(MSGT_HEADER, verbose_level, "\n");
  }
  mp_msg(MSGT_HEADER, verbose_level, "==========================================================================\n");
}


void print_video_header(BITMAPINFOHEADER *h, int verbose_level){
  mp_msg(MSGT_HEADER, verbose_level, "======= VIDEO Format ======\n");
	mp_msg(MSGT_HEADER, verbose_level, "  biSize %d\n", h->biSize);
	mp_msg(MSGT_HEADER, verbose_level, "  biWidth %d\n", h->biWidth);
	mp_msg(MSGT_HEADER, verbose_level, "  biHeight %d\n", h->biHeight);
	mp_msg(MSGT_HEADER, verbose_level, "  biPlanes %d\n", h->biPlanes);
	mp_msg(MSGT_HEADER, verbose_level, "  biBitCount %d\n", h->biBitCount);
	mp_msg(MSGT_HEADER, verbose_level, "  biCompression %d='%.4s'\n", h->biCompression, (char *)&h->biCompression);
	mp_msg(MSGT_HEADER, verbose_level, "  biSizeImage %d\n", h->biSizeImage);
  if (h->biSize > sizeof(BITMAPINFOHEADER))
  {
    int i;
    uint8_t* p = ((uint8_t*)h) + sizeof(BITMAPINFOHEADER);
    mp_msg(MSGT_HEADER, verbose_level, "Unknown extra header dump: ");
    for (i = 0; i < h->biSize-sizeof(BITMAPINFOHEADER); i++)
	mp_msg(MSGT_HEADER, verbose_level, "[%x] ", *(p+i));
    mp_msg(MSGT_HEADER, verbose_level, "\n");
  }
  mp_msg(MSGT_HEADER, verbose_level, "===========================\n");
}

void print_vprp(VideoPropHeader *vprp, int verbose_level){
  int i;
  mp_msg(MSGT_HEADER, verbose_level, "======= Video Properties Header =======\n");
  mp_msg(MSGT_HEADER, verbose_level, "Format: %d  VideoStandard: %d\n",
         vprp->VideoFormatToken,vprp->VideoStandard);
  mp_msg(MSGT_HEADER, verbose_level, "VRefresh: %d  HTotal: %d  VTotal: %d\n",
         vprp->dwVerticalRefreshRate, vprp->dwHTotalInT, vprp->dwVTotalInLines);
  mp_msg(MSGT_HEADER, verbose_level, "FrameAspect: %d:%d  Framewidth: %d  Frameheight: %d\n",
         vprp->dwFrameAspectRatio >> 16, vprp->dwFrameAspectRatio & 0xffff,
         vprp->dwFrameWidthInPixels, vprp->dwFrameHeightInLines);
  mp_msg(MSGT_HEADER, verbose_level, "Fields: %d\n", vprp->nbFieldPerFrame);
  for (i=0; i<vprp->nbFieldPerFrame; i++) {
    VIDEO_FIELD_DESC *vfd = &vprp->FieldInfo[i];
    mp_msg(MSGT_HEADER, verbose_level, "  == Field %d description ==\n", i);
    mp_msg(MSGT_HEADER, verbose_level, "  CompressedBMHeight: %d  CompressedBMWidth: %d\n",
           vfd->CompressedBMHeight, vfd->CompressedBMWidth);
    mp_msg(MSGT_HEADER, verbose_level, "  ValidBMHeight: %d  ValidBMWidth: %d\n",
           vfd->ValidBMHeight, vfd->ValidBMWidth);
    mp_msg(MSGT_HEADER, verbose_level, "  ValidBMXOffset: %d  ValidBMYOffset: %d\n",
           vfd->ValidBMXOffset, vfd->ValidBMYOffset);
    mp_msg(MSGT_HEADER, verbose_level, "  VideoXOffsetInT: %d  VideoYValidStartLine: %d\n",
           vfd->VideoXOffsetInT, vfd->VideoYValidStartLine);
  }
  mp_msg(MSGT_HEADER, verbose_level, "=======================================\n");
}

void print_index(AVIINDEXENTRY *idx, int idx_size, int verbose_level){
  int i;
  unsigned int pos[256];
  unsigned int num[256];
  for(i=0;i<256;i++) num[i]=pos[i]=0;
  for(i=0;i<idx_size;i++){
    int id=avi_stream_id(idx[i].ckid);
    if(id<0 || id>255) id=255;
    mp_msg(MSGT_HEADER, verbose_level, "%5d:  %.4s  %4X  %016llX  len:%6ld  pos:%7d->%7.3f %7d->%7.3f\n",i,
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

void print_avistdindex_chunk(avistdindex_chunk *h, int verbose_level){
    mp_msg (MSGT_HEADER, verbose_level, "====== AVI Standard Index Header ========\n");
    mp_msg (MSGT_HEADER, verbose_level, "  FCC (%.4s) dwSize (%d) wLongsPerEntry(%d)\n", h->fcc, h->dwSize, h->wLongsPerEntry);
    mp_msg (MSGT_HEADER, verbose_level, "  bIndexSubType (%d) bIndexType (%d)\n", h->bIndexSubType, h->bIndexType);
    mp_msg (MSGT_HEADER, verbose_level, "  nEntriesInUse (%d) dwChunkId (%.4s)\n", h->nEntriesInUse, h->dwChunkId);
    mp_msg (MSGT_HEADER, verbose_level, "  qwBaseOffset (0x%"PRIX64") dwReserved3 (%d)\n", h->qwBaseOffset, h->dwReserved3);
    mp_msg (MSGT_HEADER, verbose_level, "===========================\n");
}
void print_avisuperindex_chunk(avisuperindex_chunk *h, int verbose_level){
    mp_msg (MSGT_HEADER, verbose_level, "====== AVI Super Index Header ========\n");
    mp_msg (MSGT_HEADER, verbose_level, "  FCC (%.4s) dwSize (%d) wLongsPerEntry(%d)\n", h->fcc, h->dwSize, h->wLongsPerEntry);
    mp_msg (MSGT_HEADER, verbose_level, "  bIndexSubType (%d) bIndexType (%d)\n", h->bIndexSubType, h->bIndexType);
    mp_msg (MSGT_HEADER, verbose_level, "  nEntriesInUse (%d) dwChunkId (%.4s)\n", h->nEntriesInUse, h->dwChunkId);
    mp_msg (MSGT_HEADER, verbose_level, "  dwReserved[0] (%d) dwReserved[1] (%d) dwReserved[2] (%d)\n", 
	    h->dwReserved[0], h->dwReserved[1], h->dwReserved[2]);
    mp_msg (MSGT_HEADER, verbose_level, "===========================\n");
}

