#ifndef MPLAYER_GUI_BITMAP_H
#define MPLAYER_GUI_BITMAP_H

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

#endif /* MPLAYER_GUI_BITMAP_H */
