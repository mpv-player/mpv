#ifndef GUI_BITMAP_H
#define GUI_BITMAP_H

typedef struct txSample
{
 unsigned long Width;
 unsigned long Height;
 unsigned int  BPP;
 unsigned long ImageSize;
 char *        Image;
} txSample;

int bpRead( char * fname, txSample * bf );
void Convert32to1( txSample * in,txSample * out,int adaptivlimit );

#endif /* GUI_BITMAP_H */
