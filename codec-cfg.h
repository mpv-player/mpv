#ifndef __CODEC_CFG_H
#define __CODEC_CFG_H

//#include <inttypes.h>

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

#define CODECS_FLAG_AUDIO	(1<<0)

#define CODECS_FLAG_FLIP	(1<<0)
#define CODECS_FLAG_NOFLIP	(1<<1)
#define CODECS_FLAG_YUVHACK	(1<<2)


#warning nem kellene ket typedef GUID-nak...
typedef struct {
	unsigned long f1;
	unsigned short f2;
	unsigned short f3;
	unsigned char f4[8];
} GUID;

typedef struct {
	char *name;
	char *info;
	char *comment;
	unsigned int fourcc[CODECS_MAX_FOURCC];
	unsigned int fourccmap[CODECS_MAX_FOURCC];
	short driver;
	short flags;
	char *dll;
	GUID guid;
	unsigned int outfmt[CODECS_MAX_OUTFMT];
	unsigned char outflags[CODECS_MAX_OUTFMT];
} codecs_t;

codecs_t *parse_codec_cfg(char *cfgfile);

#endif
