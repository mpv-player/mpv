
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

#include "png/png.h"
#include "../../mp_msg.h"

extern int bpRead( char * fname, txSample * bf );
extern int conv24to32( txSample * bf );
extern void Convert32to1( txSample * in,txSample * out,int adaptivlimit );
extern void Convert1to32( txSample * in,txSample * out );

#endif
