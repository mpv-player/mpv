
#ifndef _INTERFACE_H
#define _INTERFACE_H

#include "../config.h"
#include "mplayer/play.h"
#include "libvo/font_load.h"
#include "cfg.h"

#ifdef USE_DVDREAD
 #include "stream/stream.h"
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
} guiUnknownErrorStruct;

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
   guiResizeStruct       resize;
   guiVideoStruct        videodata;
   guiUnknownErrorStruct error;
   
   struct MPContext * mpcontext;
   void * sh_video;
   void * afilter;
   void * demuxer;
   void * event_struct;

   int    DiskChanged;
   int    NewPlay;

#ifdef USE_DVDREAD
   guiDVDStruct         DVD;
   int			Title;
   int			Angle;
   int			Chapter;
#endif

#ifdef HAVE_VCD
   int    VCDTracks;
#endif

   int    Playing;
   float  Position;

   int    MovieWidth;
   int    MovieHeight;
   int    NoWindow;

   float  Volume;
   float  Balance;

   int    Track;
   int    AudioType;
   int    StreamType;
   int	  AudioOnly;
   int    TimeSec;
   int    LengthInSec;
   int    FrameDrop;
   int    FileFormat;
   float  FPS;

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
#define guiReDraw	    10
#define guiSetVolume        11
#define guiSetDefaults	    12
#define guiSetValues	    13
#define guiSetFileFormat    14
#define guiSetDemuxer       15
#define guiSetParameters    16
#define guiSetAfilter       17
#define guiSetContext       18

#define guiSetStop  0
#define guiSetPlay  1
#define guiSetPause 2

#define guiDVD      	1
#define guiVCD		2
#define guiFilenames	4
#define guiALL		0xffffffff

extern int use_gui;

extern char *get_path(const char *filename); 

extern void guiInit( void );
extern void guiDone( void );
extern int  guiGetEvent( int type,char * arg );
extern void guiEventHandling( void );
extern void guiLoadFont( void );
extern void guiLoadSubtitle( char * name );
extern void guiMessageBox(int level, char * str);

typedef struct _plItem 
{
 struct _plItem * prev,* next;
 int       played;
 char    * path;
 char    * name;
} plItem;

typedef struct _urlItem
{
 struct _urlItem *next;
 char    * url;
} URLItem;

extern plItem * plList;
extern plItem * plCurrent;
extern plItem * plLastPlayed;

extern URLItem * URLList;

#define fsPersistant_MaxPath 512
#define fsPersistant_MaxPos 5
extern char * fsHistory[fsPersistant_MaxPos];

#define gtkSetContrast       0
#define gtkSetBrightness     1
#define gtkSetHue	     2
#define gtkSetSaturation     3
#define gtkSetEqualizer      4
#define gtkAddPlItem         5
#define gtkGetNextPlItem     6
#define gtkGetPrevPlItem     7
#define gtkGetCurrPlItem     8
#define gtkDelPl             9
#define gtkSetExtraStereo   10
#define gtkSetPanscan       11
#define gtkSetFontFactor    12
#define gtkSetAutoq         13
#define gtkClearStruct      14
#define gtkAddURLItem       15
#define gtkSetFontOutLine   16
#define gtkSetFontBlur      17
#define gtkSetFontTextScale 18
#define gtkSetFontOSDScale  19
#define gtkSetFontEncoding  20
#define gtkSetFontAutoScale 21
#define gtkSetSubEncoding   22
#define gtkDelCurrPlItem    23
#define gtkInsertPlItem     24
#define gtkSetCurrPlItem    25

extern float gtkEquChannels[6][10];

extern void * gtkSet( int cmd,float param, void * vparam );

extern char * gconvert_uri_to_filename( char * str );
extern char * gstrdup( const char * str );
extern int    gstrcmp( const char * a,const char * b );
extern void   gfree( void ** p );
extern void   gaddlist( char *** list,const char * entry );
extern char * gstrchr( char * str,int c );

#define guiSetFilename( s,n ) { gfree( (void **)&s ); s=gstrdup( n ); }

#define guiSetDF( s,d,n )                       \
 {                                              \
  gfree( (void **)&s );                          \
  s=malloc( strlen( d ) + strlen( n ) + 5 );    \
  sprintf( s,"%s/%s",d,n );                     \
 }

#endif
