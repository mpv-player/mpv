#ifndef __GUI_BITMAP_H
#define __GUI_BITMAP_H

typedef struct _txSample
{
 unsigned long Width;
 unsigned long Height;
 unsigned int  BPP;
 unsigned long ImageSize;
 char *        Image;
} txSample;

int bpRead( char * fname, txSample * bf );
void Convert32to1( txSample * in,txSample * out,int adaptivlimit );

#endif /* __GUI_BITMAP_H */
