#ifndef __MYSAMPLE
#define __MYSAMPLE

typedef struct _txSample
{
 unsigned long Width;
 unsigned long Height;
 unsigned int  BPP;
 unsigned long ImageSize;
 char *        Image;
} txSample;

extern int bpRead( char * fname, txSample * bf );
extern int conv24to32( txSample * bf );
extern void Convert32to1( txSample * in,txSample * out,int adaptivlimit );
extern void Convert1to32( txSample * in,txSample * out );

#endif
