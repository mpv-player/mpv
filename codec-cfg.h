#ifndef __CODEC_CFG_H
#define __CODEC_CFG_H

#ifndef IMGFMT_YV12
#define IMGFMT_YV12 0x32315659
#define IMGFMT_YUY2 (('2'<<24)|('Y'<<16)|('U'<<8)|'Y')
#define IMGFMT_RGB_MASK 0xFFFFFF00
#define IMGFMT_RGB (('R'<<24)|('G'<<16)|('B'<<8))
#define IMGFMT_BGR_MASK 0xFFFFFF00
#define IMGFMT_BGR (('B'<<24)|('G'<<16)|('R'<<8))
#endif

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
