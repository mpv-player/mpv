
#ifndef _MYFONT
#define _MYFONT

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

extern fntChar    Fnt[256];
extern txSample   Bitmap;
extern bmpFont  * Fonts[25];

extern int  fntAddNewFont( char * name );
extern void fntFreeFont( int id );
extern int  fntFindID( char * name );

extern int        fntRead( char * path,char * fname,int id );
extern txSample * fntRender( int id,int px,int sx,char * fmt,... );

#endif