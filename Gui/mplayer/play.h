
#ifndef __GUI_PLAY_H
#define __GUI_PLAY_H

#include "../../config.h"

#include "./psignal.h"
#include "./mplayer.h"

#ifdef USE_DVDREAD
 #include "../../libmpdemux/stream.h"
#endif

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

#ifdef USE_DVDREAD
typedef struct
{
 int titles;
 int chapters;
 int angles;
 int current_chapter;
 int current_title;
 int nr_of_audio_channels;
 audio_stream_t audio_streams[8];
 int nr_of_subtitles;
 subtitle_t subtitles[32]; 
} mplDVDStruct;
#endif

typedef struct
{
 int message;
   mplResizeStruct      resize;
   mplVideoStruct       videodata;
   mplUnknowErrorStruct error;
#ifdef USE_DVDREAD
   mplDVDStruct         DVD;
#endif

   int    Playing;
   float  Position;

   float  Volume;
   int    VolumeChanged;
   float  Balance;
   int    Mute;
   
   int    Track;
   int    AudioType;
   int    StreamType;
   int    TimeSec;
   int    LengthInSec;
   int    FrameDrop;
   
   char   Filename[4096];
   int    FilenameChanged;
   
   int    SkinChange;
} mplCommStruct;

extern mplCommStruct * mplShMem;
extern char * Filename;

extern int   mplParent;

extern int   moviex;
extern int   moviey;
extern int   moviewidth;
extern int   movieheight;

extern mplCommStruct * mplShMem;

extern void mplMPlayerInit( int argc,char* argv[], char *envp[] );

extern void mplStop();
extern void mplFullScreen( void );
extern void mplPlay( void );
extern void mplPause( void );
extern void mplResize( unsigned int X,unsigned int Y,unsigned int width,unsigned int height );
extern void mplResizeToMovieSize( unsigned int width,unsigned int height );

extern void mplIncAudioBufDelay( void );
extern void mplDecAudioBufDelay( void );

extern void  mplRelSeek( float s );
extern void  mplAbsSeek( float s );
extern float mplGetPosition( void );

extern void mplPlayFork( void );
extern void mplSigHandler( int s );
extern void mplPlayerThread( void );

extern void ChangeSkin( void );
extern void EventHandling( void );

extern void mplSetFileName( char * fname );

#endif
