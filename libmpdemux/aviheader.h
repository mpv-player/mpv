#ifndef _aviheader_h
#define	_aviheader_h

//#include "config.h"	/* get correct definition WORDS_BIGENDIAN */
#include "bswap.h"

/*
 * Some macros to swap little endian structures read from an AVI file
 * into machine endian format
 */
#ifdef WORDS_BIGENDIAN
#define	le2me_MainAVIHeader(h) {					\
    (h)->dwMicroSecPerFrame = le2me_32((h)->dwMicroSecPerFrame);	\
    (h)->dwMaxBytesPerSec = le2me_32((h)->dwMaxBytesPerSec);		\
    (h)->dwPaddingGranularity = le2me_32((h)->dwPaddingGranularity);	\
    (h)->dwFlags = le2me_32((h)->dwFlags);				\
    (h)->dwTotalFrames = le2me_32((h)->dwTotalFrames);			\
    (h)->dwInitialFrames = le2me_32((h)->dwInitialFrames);		\
    (h)->dwStreams = le2me_32((h)->dwStreams);				\
    (h)->dwSuggestedBufferSize = le2me_32((h)->dwSuggestedBufferSize);	\
    (h)->dwWidth = le2me_32((h)->dwWidth);				\
    (h)->dwHeight = le2me_32((h)->dwHeight);				\
}

#define	le2me_AVIStreamHeader(h) {					\
    (h)->fccType = le2me_32((h)->fccType);				\
    (h)->fccHandler = le2me_32((h)->fccHandler);			\
    (h)->dwFlags = le2me_32((h)->dwFlags);				\
    (h)->wPriority = le2me_16((h)->wPriority);				\
    (h)->wLanguage = le2me_16((h)->wLanguage);				\
    (h)->dwInitialFrames = le2me_32((h)->dwInitialFrames);		\
    (h)->dwScale = le2me_32((h)->dwScale);				\
    (h)->dwRate = le2me_32((h)->dwRate);				\
    (h)->dwStart = le2me_32((h)->dwStart);				\
    (h)->dwLength = le2me_32((h)->dwLength);				\
    (h)->dwSuggestedBufferSize = le2me_32((h)->dwSuggestedBufferSize);	\
    (h)->dwQuality = le2me_32((h)->dwQuality);				\
    (h)->dwSampleSize = le2me_32((h)->dwSampleSize);			\
    le2me_RECT(&(h)->rcFrame);						\
}
#define	le2me_RECT(h) {							\
    (h)->left = le2me_16((h)->left);					\
    (h)->top = le2me_16((h)->top);					\
    (h)->right = le2me_16((h)->right);					\
    (h)->bottom = le2me_16((h)->bottom);				\
}
#define le2me_BITMAPINFOHEADER(h) {					\
    (h)->biSize = le2me_32((h)->biSize);				\
    (h)->biWidth = le2me_32((h)->biWidth);				\
    (h)->biHeight = le2me_32((h)->biHeight);				\
    (h)->biPlanes = le2me_16((h)->biPlanes);				\
    (h)->biBitCount = le2me_16((h)->biBitCount);			\
    (h)->biCompression = le2me_32((h)->biCompression);			\
    (h)->biSizeImage = le2me_32((h)->biSizeImage);			\
    (h)->biXPelsPerMeter = le2me_32((h)->biXPelsPerMeter);		\
    (h)->biYPelsPerMeter = le2me_32((h)->biYPelsPerMeter);		\
    (h)->biClrUsed = le2me_32((h)->biClrUsed);				\
    (h)->biClrImportant = le2me_32((h)->biClrImportant);		\
}
#define le2me_WAVEFORMATEX(h) {						\
    (h)->wFormatTag = le2me_16((h)->wFormatTag);			\
    (h)->nChannels = le2me_16((h)->nChannels);				\
    (h)->nSamplesPerSec = le2me_32((h)->nSamplesPerSec);		\
    (h)->nAvgBytesPerSec = le2me_32((h)->nAvgBytesPerSec);		\
    (h)->nBlockAlign = le2me_16((h)->nBlockAlign);			\
    (h)->wBitsPerSample = le2me_16((h)->wBitsPerSample);		\
    (h)->cbSize = le2me_16((h)->cbSize);				\
}
#define le2me_AVIINDEXENTRY(h) {					\
    (h)->ckid = le2me_32((h)->ckid);					\
    (h)->dwFlags = le2me_32((h)->dwFlags);				\
    (h)->dwChunkOffset = le2me_32((h)->dwChunkOffset);			\
    (h)->dwChunkLength = le2me_32((h)->dwChunkLength);			\
}
#else
#define	le2me_MainAVIHeader(h)	    /**/
#define le2me_AVIStreamHeader(h)    /**/
#define le2me_RECT(h)		    /**/
#define le2me_BITMAPINFOHEADER(h)   /**/
#define le2me_WAVEFORMATEX(h)	    /**/
#define le2me_AVIINDEXENTRY(h)	    /**/
#endif


#endif


typedef struct {
  // index stuff:
  void* idx;
  int idx_size;
  off_t idx_pos;
  off_t idx_pos_a;
  off_t idx_pos_v;
  off_t idx_offset;  // ennyit kell hozzaadni az index offset ertekekhez
  // interleaved PTS stuff:
  int skip_video_frames;
  int audio_streams;
  float avi_audio_pts;
  float avi_video_pts;
  float pts_correction;
  unsigned int pts_corr_bytes;
  unsigned char pts_corrected;
  unsigned char pts_has_video;
} avi_priv_t;

#define AVI_PRIV ((avi_priv_t*)(demuxer->priv))
