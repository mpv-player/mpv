#ifndef __CODEC_CFG_H
#define __CODEC_CFG_H

#define CODEC_CFG_MIN	20020626

#define CODECS_MAX_FOURCC	32
#define CODECS_MAX_OUTFMT	16
#define CODECS_MAX_INFMT	16

// Global flags:
#define CODECS_FLAG_SEEKABLE	(1<<0)
#define CODECS_FLAG_ALIGN16	(1<<1)
#define CODECS_FLAG_SELECTED	(1<<15)  /* for internal use */

// Outfmt flags:
#define CODECS_FLAG_FLIP	(1<<0)
#define CODECS_FLAG_NOFLIP	(1<<1)
#define CODECS_FLAG_YUVHACK	(1<<2)
#define CODECS_FLAG_QUERY	(1<<3)
#define CODECS_FLAG_STATIC	(1<<4)

#define CODECS_STATUS__MIN		0
#define CODECS_STATUS_NOT_WORKING	-1
#define CODECS_STATUS_PROBLEMS		0
#define CODECS_STATUS_WORKING		1
#define CODECS_STATUS_UNTESTED		2
#define CODECS_STATUS__MAX		2

// Codec family/driver:
#define AFM_MPEG 1
#define AFM_PCM 2
#define AFM_AC3 3
#define AFM_ACM 4
#define AFM_ALAW 5
#define AFM_GSM 6
#define AFM_DSHOW 7
#define AFM_DVDPCM 8
#define AFM_HWAC3 9
#define AFM_VORBIS 10
#define AFM_FFMPEG 11
#define AFM_MAD 12
#define AFM_MSADPCM 13
#define AFM_A52 14
#define AFM_G72X 15
#define AFM_IMAADPCM 16
#define AFM_DK4ADPCM 17
#define AFM_DK3ADPCM 18
#define AFM_ROQAUDIO 19
#define AFM_AAC 20
#define AFM_REAL 21
#define AFM_LIBDV 22

#define VFM_MPEG 1
#define VFM_VFW 2
#define VFM_ODIVX 3
#define VFM_DSHOW 4
#define VFM_FFMPEG 5
#define VFM_VFWEX 6
#define VFM_DIVX4 7
#define VFM_RAW 8
#define VFM_MSRLE 9
#define VFM_XANIM 10
#define VFM_MSVIDC 11
#define VFM_FLI 12
#define VFM_CINEPAK 13
#define VFM_QTRLE 14
#define VFM_NUV 15
#define VFM_CYUV 16
#define VFM_QTSMC 17
#define VFM_DUCKTM1 18
#define VFM_ROQVIDEO 19
#define VFM_QTRPZA 20
#define VFM_MPNG 21
#define VFM_IJPG 22
#define VFM_HUFFYUV 23
#define VFM_ZLIB 24
#define VFM_MPEGPES 25
#define VFM_REAL 26
#define VFM_SVQ1 27
#define VFM_XVID 28
#define VFM_LIBDV 29

#ifndef GUID_TYPE
#define GUID_TYPE
typedef struct {
	unsigned long f1;
	unsigned short f2;
	unsigned short f3;
	unsigned char f4[8];
} GUID;
#endif


typedef struct codecs_st {
	unsigned int fourcc[CODECS_MAX_FOURCC];
	unsigned int fourccmap[CODECS_MAX_FOURCC];
	unsigned int outfmt[CODECS_MAX_OUTFMT];
	unsigned char outflags[CODECS_MAX_OUTFMT];
	unsigned int infmt[CODECS_MAX_INFMT];
	unsigned char inflags[CODECS_MAX_INFMT];
	char *name;
	char *info;
	char *comment;
	char *dll;
	GUID guid;
	short driver;
	short flags;
	short status;
	short cpuflags;
  short priority;
} codecs_t;

int parse_codec_cfg(char *cfgfile);
codecs_t* find_video_codec(unsigned int fourcc, unsigned int *fourccmap, codecs_t *start);
codecs_t* find_audio_codec(unsigned int fourcc, unsigned int *fourccmap, codecs_t *start);
codecs_t* find_codec(unsigned int fourcc,unsigned int *fourccmap,codecs_t *start,int audioflag);
void list_codecs(int audioflag);
void codecs_reset_selection(int audioflag);

#endif
