#ifndef __CODEC_CFG_H
#define __CODEC_CFG_H

#define CODECS_MAX_FOURCC	16
#define CODECS_MAX_OUTFMT	16

// Global flags:
#define CODECS_FLAG_SEEKABLE	(1<<0)

// Outfmt flags:
#define CODECS_FLAG_FLIP	(1<<0)
#define CODECS_FLAG_NOFLIP	(1<<1)
#define CODECS_FLAG_YUVHACK	(1<<2)

#define CODECS_STATUS_NOT_WORKING	0
#define CODECS_STATUS_UNTESTED		-1
#define CODECS_STATUS_PROBLEMS		1
#define CODECS_STATUS_WORKING		2

// Codec family/driver:
#define AFM_MPEG 1
#define AFM_PCM 2
#define AFM_AC3 3
#define AFM_ACM 4
#define AFM_ALAW 5
#define AFM_GSM 6
#define AFM_DSHOW 7
#define AFM_DVDPCM 8

#define VFM_MPEG 1
#define VFM_VFW 2
#define VFM_ODIVX 3
#define VFM_DSHOW 4
#define VFM_FFMPEG 5
#define VFM_VFWEX 6
#define VFM_DIVX4 7

typedef struct {
	unsigned long f1;
	unsigned short f2;
	unsigned short f3;
	unsigned char f4[8];
} GUID;

typedef struct {
	unsigned int fourcc[CODECS_MAX_FOURCC];
	unsigned int fourccmap[CODECS_MAX_FOURCC];
	unsigned int outfmt[CODECS_MAX_OUTFMT];
	unsigned char outflags[CODECS_MAX_OUTFMT];
	char *name;
	char *info;
	char *comment;
	char *dll;
	GUID guid;
	short driver;
	short flags;
	short status;
	short cpuflags;
} codecs_t;

codecs_t** parse_codec_cfg(char *cfgfile);
codecs_t* find_video_codec(unsigned int fourcc, unsigned int *fourccmap, codecs_t *start);
codecs_t* find_audio_codec(unsigned int fourcc, unsigned int *fourccmap, codecs_t *start);
codecs_t* find_codec(unsigned int fourcc,unsigned int *fourccmap,codecs_t *start,int audioflag);

#endif
