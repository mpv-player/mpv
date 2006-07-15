
#ifndef _FONT_H
#define _FONT_H

#include "bitmap.h"
#include "app.h"

#define fntAlignLeft   0
#define fntAlignCenter 1
#define fntAlignRight  2

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
extern txSample * fntRender( wItem * item,int px,const char * fmt,... );

#endif

