
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

   int                  DiskChanged;

#ifdef USE_DVDREAD
   guiDVDStruct         DVD;
#endif

#ifdef HAVE_VCD
   int    VCDTracks;
#endif

   int    Playing;
   float  Position;

   int    MovieWidth;
   int    MovieHeight;

   float  Volume;
   float  Balance;

   int    Track;
   int    AudioType;
   int    StreamType;
   int	  AudioOnly;
   int    TimeSec;
   int    LengthInSec;
   int    FrameDrop;

   char * Filename;
   int    FilenameChanged;

   char * Subtitlename;
   int    SubtitleChanged;

   char * Othername;
   int    OtherChanged;
   
   char * AudioFile;
   int    AudioFileChanged;

   int    SkinChange;
} guiInterface_t;

extern guiInterface_t guiIntfStruct;

#define guiXEvent           0
#define guiCEvent           1
#define guiIEvent           2
#define guiSetDVD           3
#define guiSetFileName      4
#define guiSetState         5
#define guiSetAudioOnly     6
#define guiReDrawSubWindow  7
#define guiSetShVideo       8
#define guiSetStream        9
#define guiClearStruct      10
#define guiReDraw	    11
#define guiSetVolume        12

#define guiSetStop  0
#define guiSetPlay  1
#define guiSetPause 2

#define guiDVD      	1
#define guiVCD		2
#define guiALL		0xffffffff

extern char *get_path(char *filename); 

extern void guiInit( void );
extern void guiDone( void );
extern void guiGetEvent( int type,char * arg );
extern void guiEventHandling( void );

#define gstrdup( s,ss ) { s=malloc( strlen( ss ) + 3 ); strcpy( s,ss ); }

#define guiSetFilename( s,n ) { if ( s ) free( s ); s=strdup( n ); }

#define guiSetDF( s,d,n )                       \
 {                                              \
  if ( s ) free( s ); s=NULL;                   \
  s=malloc( strlen( d ) + strlen( n ) + 5 );    \
  sprintf( s,"%s/%s",d,n );                     \
 }

#endif
