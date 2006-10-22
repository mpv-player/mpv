#ifndef __CODEC_CFG_H
#define __CODEC_CFG_H

#define CODEC_CFG_MIN	20061022

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


#if !defined(GUID_TYPE) && !defined(GUID_DEFINED)
#define GUID_TYPE 1
#define GUID_DEFINED 1
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
	char* drv;
	GUID guid;
//	short driver;
	short flags;
	short status;
	short cpuflags;
} codecs_t;

int parse_codec_cfg(const char *cfgfile);
codecs_t* find_video_codec(unsigned int fourcc, unsigned int *fourccmap,
                           codecs_t *start, int force);
codecs_t* find_audio_codec(unsigned int fourcc, unsigned int *fourccmap,
                           codecs_t *start, int force);
codecs_t* find_codec(unsigned int fourcc, unsigned int *fourccmap,
                     codecs_t *start, int audioflag, int force);
void select_codec(char* codecname,int audioflag);
void list_codecs(int audioflag);
void codecs_reset_selection(int audioflag);
void codecs_uninit_free(void);

#endif
