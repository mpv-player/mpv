
#ifndef _INTERFACE_H
#define _INTERFACE_H

#include "../config.h"
#include "mplayer/play.h"
#include "../mplayer.h"

#ifdef USE_DVDREAD
 #include "../libmpdemux/stream.h"
#endif


typedef struct
{
 int x;
 int y;
 int width;
 int height;
} guiResizeStruct;

typedef struct
{
 int  signal;
 char module[512];
} guiUnknowErrorStruct;

typedef struct
{
 int  seek;
 int  format;
 int  width;
 int  height;
 char codecdll[128];
} guiVideoStruct;

#ifdef USE_DVDREAD
typedef struct
{
 int titles;
 int chapters;
 int angles;
 int current_chapter;
 int current_title;
 int current_angle;
 int nr_of_audio_channels;
 stream_language_t audio_streams[32];
 int nr_of_subtitles;
 stream_language_t subtitles[32];
} guiDVDStruct;
#endif

typedef struct
{
 int message;
   guiResizeStruct      resize;
   guiVideoStruct       videodata;
   guiUnknowErrorStruct error;
#ifdef USE_DVDREAD
   guiDVDStruct         DVD;
   int                  DVDChanged;
#endif

   int    Playing;
   float  Position;

   int    MovieWidth;
   int    MovieHeight;

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

   char * Filename;
   int    FilenameChanged;

   char * Subtitlename;
   int    SubtitleChanged;

   char * Othername;
   int    OtherChanged;

   int    SkinChange;
} guiInterface_t;

extern guiInterface_t guiIntfStruct;

#define guiXEvent 0
#define guiCEvent 1

extern void guiInit( int argc,char* argv[], char *envp[] );
extern void guiGetEvent( int type,char * arg );
extern void guiEventHandling( void );

#define guiSetFilename( s,n ) \
 { if ( s ) free( s ); s=NULL; s=strdup( n ); }

#define guiSetDF( s,d,n )                       \
 {                                              \
  if ( s ) free( s ); s=NULL;                   \
  s=malloc( strlen( d ) + strlen( n ) + 5 );    \
  sprintf( s,"%s/%s",d,n );                     \
 }

#endif