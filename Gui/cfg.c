
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"
#include "../mplayer.h"
#include "../cfgparser.h"

#include "../../libvo/video_out.h"

#include "cfg.h"
#include "interface.h"
#include "mplayer/play.h"

// --- params

int    gtkEnableAudioEqualizer = 0;
int    gtkEnableVideoEqualizer = 0;

char * gtkVODriver = NULL;
int    gtkVODoubleBuffer = 1;
int    gtkVODirectRendering = 0;
int    gtkVFrameDrop = 1;
int    gtkVHardFrameDrop = 0;
int    gtkVNIAVI = 0;
int    gtkVFlip = 0;
int    gtkVIndex = 1;
int    gtkVVFM = -1;
int    gtkVPP = 0;
int    gtkVAutoq = 0;

char * gtkAODriver = NULL;
int    gtkAONoSound = 0;
float  gtkAODelay = 0.0f;
int    gtkAONorm = 0;
int    gtkAOSurround = 0;
int    gtkAOExtraStereo = 0;
float  gtkAOExtraStereoMul = 1.0;
char * gtkAOOSSMixer;
char * gtkAOOSSDevice;

int    gtkSubAuto = 1; //
int    gtkSubUnicode = 0; //
int    gtkSubDumpMPSub = 0;
int    gtkSubDumpSrt = 0;
float  gtkSubDelay = 0.0f;
float  gtkSubFPS = 0.0f;
int    gtkSubPos = 100; //
float  gtkSubFFactor = 0.75;

// ---

extern char * get_path( char * filename );
extern int    flip;
extern int    frame_dropping;

static m_config_t * gui_conf;
static config_t gui_opts[] =
{
 { "enable_audio_equ",&gtkEnableAudioEqualizer,CONF_TYPE_FLAG,0,0,1,NULL },
 { "enable_video_equ",&gtkEnableVideoEqualizer,CONF_TYPE_FLAG,0,0,1,NULL },
 
 { "vo_driver",&gtkVODriver,CONF_TYPE_STRING,0,0,0,NULL },
 { "vo_panscan",&vo_panscan,CONF_TYPE_FLOAT,CONF_RANGE,0.0,1.0,NULL },
 { "vo_doublebuffering",&vo_doublebuffering,CONF_TYPE_FLAG,0,0,1,NULL },
 { "vo_direct_render",&gtkVODirectRendering,CONF_TYPE_FLAG,0,0,1,NULL },

 { "v_framedrop",&gtkVFrameDrop,CONF_TYPE_FLAG,0,0,1,NULL },
 { "v_hard_framedrop",&gtkVHardFrameDrop,CONF_TYPE_FLAG,0,0,1,NULL },
 { "v_flip",&gtkVFlip,CONF_TYPE_FLAG,0,0,1,NULL },
 { "v_ni",&gtkVNIAVI,CONF_TYPE_FLAG,0,0,1,NULL },
 { "v_idx",&gtkVIndex,CONF_TYPE_FLAG,0,0,1,NULL },
 { "v_vfm",&gtkVVFM,CONF_TYPE_INT,CONF_RANGE,-1,10,NULL },
 { "vf_pp",&gtkVPP,CONF_TYPE_FLAG,0,0,1,NULL },
 { "vf_autoq",&gtkVAutoq,CONF_TYPE_INT,CONF_RANGE,0,100,NULL },

 { "ao_driver",&gtkAODriver,CONF_TYPE_STRING,0,0,0,NULL },
 { "ao_nosound",&gtkAONoSound,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_volnorm",&gtkAONorm,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_surround",&gtkAOSurround,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_extra_stereo",&gtkAOExtraStereo,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_extra_stereo_coefficient",&gtkAOExtraStereoMul,CONF_TYPE_FLOAT,CONF_RANGE,-10,10,NULL },
 { "ao_delay",&gtkAODelay,CONF_TYPE_FLOAT,CONF_RANGE,-100,100,NULL },
 { "ao_oss_mixer",&gtkAOOSSMixer,CONF_TYPE_STRING,0,0,0,NULL },
 { "ao_oss_device",&gtkAOOSSDevice,CONF_TYPE_STRING,0,0,0,NULL },
 
 { "osd_level",&osd_level,CONF_TYPE_INT,CONF_RANGE,0,2,NULL },
 { "sub_auto_load",&gtkSubAuto,CONF_TYPE_FLAG,0,0,1,NULL },
 { "sub_unicode",&gtkSubUnicode,CONF_TYPE_FLAG,0,0,1,NULL },
 { "sub_pos",&gtkSubPos,CONF_TYPE_INT,CONF_RANGE,0,200,NULL },
 { "font_factor",&gtkSubFFactor,CONF_TYPE_FLOAT,CONF_RANGE,0.0,10.0,NULL },
 { "font_name",&guiIntfStruct.Fontname,CONF_TYPE_STRING,0,0,0,NULL },

 { "equ_channel_1",&gtkEquChannel1,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_2",&gtkEquChannel2,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_3",&gtkEquChannel3,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_4",&gtkEquChannel4,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_5",&gtkEquChannel5,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_6",&gtkEquChannel6,CONF_TYPE_STRING,0,0,0,NULL },
 
#if 1
#define audio_equ_row( i,j ) { "equ_band_"#i#j,&gtkEquChannels[i][j],CONF_TYPE_FLOAT,CONF_RANGE,-5.0,5.0,NULL },
   audio_equ_row( 0,0 ) audio_equ_row( 0,1 ) audio_equ_row( 0,2 ) audio_equ_row( 0,3 ) audio_equ_row( 0,4 ) audio_equ_row( 0,5 ) audio_equ_row( 0,6 ) audio_equ_row( 0,7 ) audio_equ_row( 0,8 ) audio_equ_row( 0,9 )
   audio_equ_row( 1,0 ) audio_equ_row( 1,1 ) audio_equ_row( 1,2 ) audio_equ_row( 1,3 ) audio_equ_row( 1,4 ) audio_equ_row( 1,5 ) audio_equ_row( 1,6 ) audio_equ_row( 1,7 ) audio_equ_row( 1,8 ) audio_equ_row( 1,9 )
   audio_equ_row( 2,0 ) audio_equ_row( 2,1 ) audio_equ_row( 2,2 ) audio_equ_row( 2,3 ) audio_equ_row( 2,4 ) audio_equ_row( 2,5 ) audio_equ_row( 2,6 ) audio_equ_row( 2,7 ) audio_equ_row( 2,8 ) audio_equ_row( 2,9 )
   audio_equ_row( 3,0 ) audio_equ_row( 3,1 ) audio_equ_row( 3,2 ) audio_equ_row( 3,3 ) audio_equ_row( 3,4 ) audio_equ_row( 3,5 ) audio_equ_row( 3,6 ) audio_equ_row( 3,7 ) audio_equ_row( 3,8 ) audio_equ_row( 3,9 )
   audio_equ_row( 4,0 ) audio_equ_row( 4,1 ) audio_equ_row( 4,2 ) audio_equ_row( 4,3 ) audio_equ_row( 4,4 ) audio_equ_row( 4,5 ) audio_equ_row( 4,6 ) audio_equ_row( 4,7 ) audio_equ_row( 4,8 ) audio_equ_row( 4,9 )
   audio_equ_row( 5,0 ) audio_equ_row( 5,1 ) audio_equ_row( 5,2 ) audio_equ_row( 5,3 ) audio_equ_row( 5,4 ) audio_equ_row( 5,5 ) audio_equ_row( 5,6 ) audio_equ_row( 5,7 ) audio_equ_row( 5,8 ) audio_equ_row( 5,9 )
#undef audio_equ_row
#endif

 { NULL, NULL, 0, 0, 0, 0, NULL }
};

int cfg_read( void )
{
 char * cfg = get_path( "gui.conf" );

// -- read configuration
 mp_msg( MSGT_GPLAYER,MSGL_STATUS,"[cfg] read config file: %s\n",cfg );
 gui_conf=m_config_new( play_tree_new() ); 
 m_config_register_options( gui_conf,gui_opts );
 if ( m_config_parse_config_file( gui_conf,cfg ) < 0 ) 
  {
   mp_msg( MSGT_GPLAYER,MSGL_FATAL,"[cfg] config file read error ...\n" );
   exit( 1 );
  }
 free( cfg );

// -- read pl
 {
  FILE * f;
  cfg=get_path( "gui.pl" );
  if ( (f=fopen( cfg,"rt" )) == NULL ) return 1;
  while ( !feof( f ) )
   {
    char tmp[512]; plItem * item = calloc( 1,sizeof( plItem ) ); char c;
    if ( fgets( tmp,512,f ) == NULL ) continue;
    c=tmp[ strlen( tmp ) - 1 ]; if ( c == '\n' || c == '\r' ) tmp[ strlen( tmp ) - 1 ]=0;
    c=tmp[ strlen( tmp ) - 1 ]; if ( c == '\n' || c == '\r' ) tmp[ strlen( tmp ) - 1 ]=0;
    item->path=strdup( tmp );
    fgets( tmp,512,f );
    c=tmp[ strlen( tmp ) - 1 ]; if ( c == '\n' || c == '\r' ) tmp[ strlen( tmp ) - 1 ]=0;
    c=tmp[ strlen( tmp ) - 1 ]; if ( c == '\n' || c == '\r' ) tmp[ strlen( tmp ) - 1 ]=0;
    item->name=strdup( tmp );
    gtkSet( gtkAddPlItem,0,(void*)item );
   }
  fclose( f );
  free( cfg );
 }
 return 0;
}

int cfg_write( void )
{
 char * cfg = get_path( "gui.conf" );
 FILE * f;
 int    i;

// -- save configuration 
 if ( (f=fopen( cfg,"wt+" )) )
  {
   for ( i=0;gui_opts[i].name;i++ )
    {
     switch ( gui_opts[i].type )
      {
       case CONF_TYPE_INT:
       case CONF_TYPE_FLAG:   fprintf( f,"%s = %d\n",gui_opts[i].name,*( (int *)gui_opts[i].p ) );   				      break;
       case CONF_TYPE_FLOAT:  fprintf( f,"%s = %f\n",gui_opts[i].name,*( (float *)gui_opts[i].p ) ); 				      break;
       case CONF_TYPE_STRING: 
            {
	     char * tmp = *( (char **)gui_opts[i].p );
	     if ( tmp && tmp[0] ) fprintf( f,"%s = \"%s\"\n",gui_opts[i].name,tmp );
	     break;
	    }
      }
    }
   fclose( f );
  }
 free( cfg );
 
// -- save playlist
 cfg=get_path( "gui.pl" );
 if ( (f=fopen( cfg,"wt+" )) )
  {
   plCurrent=plList;
   while ( plCurrent )
    {
     if ( plCurrent->path && plCurrent->name )
      { 
       fprintf( f,"%s\n",plCurrent->path );
       fprintf( f,"%s\n",plCurrent->name );
      }
     plCurrent=plCurrent->next;
    }
   fclose( f );
  }
 free( cfg );

 return 0;
}

