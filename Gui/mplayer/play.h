
#ifndef __GUI_PLAY_H
#define __GUI_PLAY_H

#include "./psignal.h"

typedef struct
{
 int x;
 int y;
 int width;
 int height;
} mplResizeStruct;

typedef struct
{
 int  signal;
 char module[512];
} mplUnknowErrorStruct;

typedef struct
{
 int  seek;
 int  format;
 int  width;
 int  height;
 char codecdll[128];
} mplVideoStruct;

typedef struct
{
 int message;
   mplResizeStruct      resize;
   mplVideoStruct       videodata;
   mplUnknowErrorStruct error;

   int    Playing;
   float  Volume;
   float  Position;
   float  Balance;
   int    Track;
   int    AudioType;
   int    StreamType;
   int    TimeSec;
   int    LengthInSec;
} mplCommStruct;

extern mplCommStruct * mplShMem;
extern char * Filename;

extern int   mplParent;

extern int   mplx;
extern int   mply;
extern int   mplwidth;
extern int   mplheight;

extern mplCommStruct * mplShMem;

extern void mplMPlayerInit( int argc,char* argv[], char *envp[] );

extern void mplStop();
extern void mplFullScreen( void );
extern void mplPlay( void );
extern void mplPause( void );
extern void mplResize( unsigned int X,unsigned int Y,unsigned int width,unsigned int height );

extern void mplIncAudioBufDelay( void );
extern void mplDecAudioBufDelay( void );

extern void  mplRelSeek( float s );
extern void  mplAbsSeek( float s );
extern float mplGetPosition( void );

extern void mplPlayFork( void );
extern void mplSigHandler( int s );
extern void mplSendMessage( int msg );
extern void mplPlayerThread( void );

#endif