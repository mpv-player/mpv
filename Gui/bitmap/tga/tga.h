
#ifndef _MYTGA
#define _MYTGA

#include "../bitmap.h"

typedef struct
{
 char             tmp[12];
 unsigned short   sx;
 unsigned short   sy;
 unsigned char    depth;
 unsigned char    ctmp;
} tgaHeadert;

extern int tgaRead( char * filename,txSample * bf );
extern void tgaWriteBuffer( char * fname,unsigned char * Buffer,int sx,int sy,int BPP );
extern void tgaWriteTexture( char * filename,txSample * bf );


#endif
