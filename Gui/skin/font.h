
#ifndef _FONT_H
#define _FONT_H

#include "../bitmap/bitmap.h"

typedef struct
{
 int x,y;   // location
 int sx,sy; // size
} fntChar;

typedef struct
{
 fntChar    Fnt[256];
 txSample   Bitmap;
 char       name[128];
} bmpFont;

extern txSample   Bitmap;
extern bmpFont  * Fonts[26];

extern int  fntAddNewFont( char * name );
extern void fntFreeFont( void );
extern int  fntFindID( char * name );
extern int  fntTextHeight( int id,char * str );
extern int  fntTextWidth( int id,char * str );

extern int        fntRead( char * path,char * fname );
extern txSample * fntRender( int id,int px,int sx,char * fmt,... );

#endif

