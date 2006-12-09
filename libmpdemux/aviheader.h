#ifndef _aviheader_h
#define	_aviheader_h

//#include "config.h"	/* get correct definition WORDS_BIGENDIAN */
#include "libavutil/common.h"
#include "mpbswap.h"

#ifndef mmioFOURCC
#define mmioFOURCC( ch0, ch1, ch2, ch3 )				\
		( (uint32_t)(uint8_t)(ch0) | ( (uint32_t)(uint8_t)(ch1) << 8 ) |	\
		( (uint32_t)(uint8_t)(ch2) << 16 ) | ( (uint32_t)(uint8_t)(ch3) << 24 ) )
#endif

/* Macro to make a TWOCC out of two characters */
#ifndef aviTWOCC
#define aviTWOCC(ch0, ch1) ((uint16_t)(uint8_t)(ch0) | ((uint16_t)(uint8_t)(ch1) << 8))
#endif

//typedef uint16_t TWOCC;
//typedef uint32_t FOURCC;

/* form types, list types, and chunk types */
#define formtypeAVI             mmioFOURCC('A', 'V', 'I', ' ')
#define listtypeAVIHEADER       mmioFOURCC('h', 'd', 'r', 'l')
#define ckidAVIMAINHDR          mmioFOURCC('a', 'v', 'i', 'h')
#define listtypeSTREAMHEADER    mmioFOURCC('s', 't', 'r', 'l')
#define ckidSTREAMHEADER        mmioFOURCC('s', 't', 'r', 'h')
#define ckidSTREAMFORMAT        mmioFOURCC('s', 't', 'r', 'f')
#define ckidSTREAMHANDLERDATA   mmioFOURCC('s', 't', 'r', 'd')
#define ckidSTREAMNAME		mmioFOURCC('s', 't', 'r', 'n')

#define listtypeAVIMOVIE        mmioFOURCC('m', 'o', 'v', 'i')
#define listtypeAVIRECORD       mmioFOURCC('r', 'e', 'c', ' ')

#define ckidAVINEWINDEX         mmioFOURCC('i', 'd', 'x', '1')

/*
** Stream types for the <fccType> field of the stream header.
*/
#define streamtypeVIDEO         mmioFOURCC('v', 'i', 'd', 's')
#define streamtypeAUDIO         mmioFOURCC('a', 'u', 'd', 's')
#define streamtypeMIDI		mmioFOURCC('m', 'i', 'd', 's')
#define streamtypeTEXT          mmioFOURCC('t', 'x', 't', 's')

/* Basic chunk types */
#define cktypeDIBbits           aviTWOCC('d', 'b')
#define cktypeDIBcompressed     aviTWOCC('d', 'c')
#define cktypePALchange         aviTWOCC('p', 'c')
#define cktypeWAVEbytes         aviTWOCC('w', 'b')

/* Chunk id to use for extra chunks for padding. */
#define ckidAVIPADDING          mmioFOURCC('J', 'U', 'N', 'K')

/* flags for use in <dwFlags> in AVIFileHdr */
#define AVIF_HASINDEX		0x00000010	// Index at end of file?
#define AVIF_MUSTUSEINDEX	0x00000020
#define AVIF_ISINTERLEAVED	0x00000100
#define AVIF_TRUSTCKTYPE	0x00000800	// Use CKType to find key frames?
#define AVIF_WASCAPTUREFILE	0x00010000
#define AVIF_COPYRIGHTED	0x00020000

typedef struct
{
    uint32_t		dwMicroSecPerFrame;	// frame display rate (or 0L)
    uint32_t		dwMaxBytesPerSec;	// max. transfer rate
    uint32_t		dwPaddingGranularity;	// pad to multiples of this
                                                // size; normally 2K.
    uint32_t		dwFlags;		// the ever-present flags
    uint32_t		dwTotalFrames;		// # frames in file
    uint32_t		dwInitialFrames;
    uint32_t		dwStreams;
    uint32_t		dwSuggestedBufferSize;
    
    uint32_t		dwWidth;
    uint32_t		dwHeight;
    
    uint32_t		dwReserved[4];
} MainAVIHeader;

typedef struct rectangle_t {
    short  left;
    short  top;
    short  right;
    short  bottom;
} rectangle_t;

typedef struct {
    uint32_t		fccType;
    uint32_t		fccHandler;
    uint32_t		dwFlags;	/* Contains AVITF_* flags */
    uint16_t		wPriority;
    uint16_t		wLanguage;
    uint32_t		dwInitialFrames;
    uint32_t		dwScale;	
    uint32_t		dwRate;	/* dwRate / dwScale == samples/second */
    uint32_t		dwStart;
    uint32_t		dwLength; /* In units above... */
    uint32_t		dwSuggestedBufferSize;
    uint32_t		dwQuality;
    uint32_t		dwSampleSize;
    rectangle_t		rcFrame;
} AVIStreamHeader;

/* Flags for index */
#define AVIIF_LIST          0x00000001L // chunk is a 'LIST'
#define AVIIF_KEYFRAME      0x00000010L // this frame is a key frame.

#define AVIIF_NOTIME	    0x00000100L // this frame doesn't take any time
#define AVIIF_COMPUSE       0x0FFF0000L // these bits are for compressor use

#define FOURCC_RIFF     mmioFOURCC('R', 'I', 'F', 'F')
#define FOURCC_LIST     mmioFOURCC('L', 'I', 'S', 'T')

typedef struct
{
    uint32_t		ckid;
    uint32_t		dwFlags;
    uint32_t		dwChunkOffset;		// Position of chunk
    uint32_t		dwChunkLength;		// Length of chunk
} AVIINDEXENTRY;


typedef struct _avisuperindex_entry {
    uint64_t qwOffset;           // absolute file offset
    uint32_t dwSize;             // size of index chunk at this offset
    uint32_t dwDuration;         // time span in stream ticks
} avisuperindex_entry;

typedef struct _avistdindex_entry {
    uint32_t dwOffset;           // qwBaseOffset + this is absolute file offset
    uint32_t dwSize;             // bit 31 is set if this is NOT a keyframe
} avistdindex_entry;

// Standard index 
typedef struct __attribute__((packed)) _avistdindex_chunk {
    char           fcc[4];       // ix##
    uint32_t  dwSize;            // size of this chunk
    uint16_t wLongsPerEntry;     // must be sizeof(aIndex[0])/sizeof(DWORD)
    uint8_t  bIndexSubType;      // must be 0
    uint8_t  bIndexType;         // must be AVI_INDEX_OF_CHUNKS
    uint32_t  nEntriesInUse;     // first unused entry
    char           dwChunkId[4]; // '##dc' or '##db' or '##wb' etc..
    uint64_t qwBaseOffset;       // all dwOffsets in aIndex array are relative to this
    uint32_t  dwReserved3;       // must be 0
    avistdindex_entry *aIndex;   // the actual frames
} avistdindex_chunk;
    

// Base Index Form 'indx'
typedef struct _avisuperindex_chunk {
    char           fcc[4];
    uint32_t  dwSize;                // size of this chunk
    uint16_t wLongsPerEntry;         // size of each entry in aIndex array (must be 4*4 for us)
    uint8_t  bIndexSubType;          // future use. must be 0
    uint8_t  bIndexType;             // one of AVI_INDEX_* codes
    uint32_t  nEntriesInUse;         // index of first unused member in aIndex array
    char       dwChunkId[4];         // fcc of what is indexed
    uint32_t  dwReserved[3];         // meaning differs for each index type/subtype.
                                     // 0 if unused
    avisuperindex_entry *aIndex;     // position of ix## chunks
    avistdindex_chunk *stdidx;       // the actual std indices
} avisuperindex_chunk;

typedef struct {
	uint32_t CompressedBMHeight;
	uint32_t CompressedBMWidth;
	uint32_t ValidBMHeight;
	uint32_t ValidBMWidth;
	uint32_t ValidBMXOffset;
	uint32_t ValidBMYOffset;
	uint32_t VideoXOffsetInT;
	uint32_t VideoYValidStartLine;
} VIDEO_FIELD_DESC;

typedef struct {
	uint32_t VideoFormatToken;
	uint32_t VideoStandard;
	uint32_t dwVerticalRefreshRate;
	uint32_t dwHTotalInT;
	uint32_t dwVTotalInLines;
	uint32_t dwFrameAspectRatio;
	uint32_t dwFrameWidthInPixels;
	uint32_t dwFrameHeightInLines;
	uint32_t nbFieldPerFrame;
	VIDEO_FIELD_DESC FieldInfo[2];
} VideoPropHeader;

typedef enum {
	FORMAT_UNKNOWN,
	FORMAT_PAL_SQUARE,
	FORMAT_PAL_CCIR_601,
	FORMAT_NTSC_SQUARE,
	FORMAT_NTSC_CCIR_601,
} VIDEO_FORMAT;

typedef enum {
	STANDARD_UNKNOWN,
	STANDARD_PAL,
	STANDARD_NTSC,
	STANDARD_SECAM
} VIDEO_STANDARD;

#define MAKE_AVI_ASPECT(a, b) (((a)<<16)|(b))
#define GET_AVI_ASPECT(a) ((float)((a)>>16)/(float)((a)&0xffff))

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
#define le2me_AVISTDIDXCHUNK(h) {\
    char c; \
    c = (h)->fcc[0]; (h)->fcc[0] = (h)->fcc[3]; (h)->fcc[3] = c;  \
    c = (h)->fcc[1]; (h)->fcc[1] = (h)->fcc[2]; (h)->fcc[2] = c;  \
    (h)->dwSize = le2me_32((h)->dwSize);  \
    (h)->wLongsPerEntry = le2me_16((h)->wLongsPerEntry);  \
    (h)->nEntriesInUse = le2me_32((h)->nEntriesInUse);  \
    c = (h)->dwChunkId[0]; (h)->dwChunkId[0] = (h)->dwChunkId[3]; (h)->dwChunkId[3] = c;  \
    c = (h)->dwChunkId[1]; (h)->dwChunkId[1] = (h)->dwChunkId[2]; (h)->dwChunkId[2] = c;  \
    (h)->qwBaseOffset = le2me_64((h)->qwBaseOffset);  \
    (h)->dwReserved3 = le2me_32((h)->dwReserved3);  \
}
#define le2me_AVISTDIDXENTRY(h)  {\
    (h)->dwOffset = le2me_32((h)->dwOffset);  \
    (h)->dwSize = le2me_32((h)->dwSize);  \
}
#define le2me_VideoPropHeader(h) {					\
    (h)->VideoFormatToken = le2me_32((h)->VideoFormatToken);		\
    (h)->VideoStandard = le2me_32((h)->VideoStandard);			\
    (h)->dwVerticalRefreshRate = le2me_32((h)->dwVerticalRefreshRate);	\
    (h)->dwHTotalInT = le2me_32((h)->dwHTotalInT);			\
    (h)->dwVTotalInLines = le2me_32((h)->dwVTotalInLines);		\
    (h)->dwFrameAspectRatio = le2me_32((h)->dwFrameAspectRatio);	\
    (h)->dwFrameWidthInPixels = le2me_32((h)->dwFrameWidthInPixels);	\
    (h)->dwFrameHeightInLines = le2me_32((h)->dwFrameHeightInLines);	\
    (h)->nbFieldPerFrame = le2me_32((h)->nbFieldPerFrame);		\
}
#define le2me_VIDEO_FIELD_DESC(h) {					\
    (h)->CompressedBMHeight = le2me_32((h)->CompressedBMHeight);	\
    (h)->CompressedBMWidth = le2me_32((h)->CompressedBMWidth);		\
    (h)->ValidBMHeight = le2me_32((h)->ValidBMHeight);			\
    (h)->ValidBMWidth = le2me_32((h)->ValidBMWidth);			\
    (h)->ValidBMXOffset = le2me_32((h)->ValidBMXOffset);		\
    (h)->ValidBMYOffset = le2me_32((h)->ValidBMYOffset);		\
    (h)->VideoXOffsetInT = le2me_32((h)->VideoXOffsetInT);		\
    (h)->VideoYValidStartLine = le2me_32((h)->VideoYValidStartLine);	\
}

#else
#define	le2me_MainAVIHeader(h)	    /**/
#define le2me_AVIStreamHeader(h)    /**/
#define le2me_RECT(h)		    /**/
#define le2me_BITMAPINFOHEADER(h)   /**/
#define le2me_WAVEFORMATEX(h)	    /**/
#define le2me_AVIINDEXENTRY(h)	    /**/
#define le2me_AVISTDIDXCHUNK(h)     /**/
#define le2me_AVISTDIDXENTRY(h)     /**/
#define le2me_VideoPropHeader(h)    /**/
#define le2me_VIDEO_FIELD_DESC(h)   /**/
#endif

typedef struct {
  // index stuff:
  void* idx;
  int idx_size;
  off_t idx_pos;
  off_t idx_pos_a;
  off_t idx_pos_v;
  off_t idx_offset;  // ennyit kell hozzaadni az index offset ertekekhez
  // bps-based PTS stuff:
  int video_pack_no;
  int audio_block_size;
  off_t audio_block_no;
  // interleaved PTS stuff:
  int skip_video_frames;
  int audio_streams;
  float avi_audio_pts;
  float avi_video_pts;
  float pts_correction;
  unsigned int pts_corr_bytes;
  unsigned char pts_corrected;
  unsigned char pts_has_video;
  unsigned int numberofframes;
  avisuperindex_chunk *suidx;
  int suidx_size;
  int isodml;
} avi_priv_t;

#define AVI_PRIV ((avi_priv_t*)(demuxer->priv))

#define AVI_IDX_OFFSET(x) ((((uint64_t)(x)->dwFlags&0xffff0000)<<16)+(x)->dwChunkOffset)

#endif /* _aviheader_h */
