
#ifndef __MYSAMPLE
#define __MYSAMPLE

typedef struct _txSample
{
 unsigned int  Width;
 unsigned int  Height;
 unsigned int  BPP;
 unsigned long ImageSize;
 char *        Image;
} txSample;

#include "tga/tga.h"
#include "bmp/bmp.h"
#include "png/png.h"

extern int bpRead( char * fname, txSample * bf );
extern int conv24to32( txSample * bf );

#endif