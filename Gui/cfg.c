
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"
#include "../mplayer.h"
#include "../cfgparser.h"

#ifdef USE_SETLOCALE
#include <locale.h>
#endif

#include "../../libvo/video_out.h"

#include "cfg.h"
#include "app.h"
#include "interface.h"
#include "mplayer/play.h"

// --- params

int    gtkEnableAudioEqualizer = 0;

int    gtkVopPP = 0;
int    gtkVopLAVC = 0;
int    gtkVopFAME = 0;

int    gtkAONoSound = 0;
float  gtkAODelay = 0.0f;
int    gtkAONorm = 0;
int    gtkAOSurround = 0;
int    gtkAOExtraStereo = 0;
float  gtkAOExtraStereoMul = 1.0;
char * gtkAOOSSMixer;
char * gtkAOOSSDevice;

int    gtkSubDumpMPSub = 0;
int    gtkSubDumpSrt = 0;

// ---

extern char * get_path( char * filename );
extern int    flip;
extern int    frame_dropping;

static m_config_t * gui_conf;
static config_t gui_opts[] =
{
 { "enable_audio_equ",&gtkEnableAudioEqualizer,CONF_TYPE_FLAG,0,0,1,NULL },
 
 { "vo_driver",&video_driver,CONF_TYPE_STRING,0,0,0,NULL },
 { "vo_panscan",&vo_panscan,CONF_TYPE_FLOAT,CONF_RANGE,0.0,1.0,NULL },
 { "vo_doublebuffering",&vo_doublebuffering,CONF_TYPE_FLAG,0,0,1,NULL },
 { "vo_direct_render",&vo_directrendering,CONF_TYPE_FLAG,0,0,1,NULL },

 { "v_framedrop",&frame_dropping,CONF_TYPE_INT,CONF_RANGE,0,2,NULL },
 { "v_flip",&flip,CONF_TYPE_INT,CONF_RANGE,-1,1,NULL },
 { "v_ni",&force_ni,CONF_TYPE_FLAG,0,0,1,NULL },
 { "v_idx",&index_mode,CONF_TYPE_INT,CONF_RANGE,-1,2,NULL },
 { "v_vfm",&video_family,CONF_TYPE_INT,CONF_RANGE,-1,10,NULL },

 { "vf_pp",&gtkVopPP,CONF_TYPE_FLAG,0,0,1,NULL },
 { "vf_autoq",&auto_quality,CONF_TYPE_INT,CONF_RANGE,0,100,NULL },
 { "vf_lavc",&gtkVopLAVC,CONF_TYPE_FLAG,0,0,1,NULL },
 { "vf_fame",&gtkVopFAME,CONF_TYPE_FLAG,0,0,1,NULL },

 { "ao_driver",&audio_driver,CONF_TYPE_STRING,0,0,0,NULL },
 { "ao_nosound",&gtkAONoSound,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_volnorm",&gtkAONorm,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_surround",&gtkAOSurround,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_extra_stereo",&gtkAOExtraStereo,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_extra_stereo_coefficient",&gtkAOExtraStereoMul,CONF_TYPE_FLOAT,CONF_RANGE,-10,10,NULL },
 { "ao_delay",&gtkAODelay,CONF_TYPE_FLOAT,CONF_RANGE,-100,100,NULL },
 { "ao_oss_mixer",&gtkAOOSSMixer,CONF_TYPE_STRING,0,0,0,NULL },
 { "ao_oss_device",&gtkAOOSSDevice,CONF_TYPE_STRING,0,0,0,NULL },
 
 { "osd_level",&osd_level,CONF_TYPE_INT,CONF_RANGE,0,2,NULL },
 { "sub_auto_load",&sub_auto,CONF_TYPE_FLAG,0,0,1,NULL },
 { "sub_unicode",&sub_unicode,CONF_TYPE_FLAG,0,0,1,NULL },
 { "sub_pos",&sub_pos,CONF_TYPE_INT,CONF_RANGE,0,200,NULL },
 { "font_factor",&font_factor,CONF_TYPE_FLOAT,CONF_RANGE,0.0,10.0,NULL },
 { "font_name",&font_name,CONF_TYPE_STRING,0,0,0,NULL },
#ifdef HAVE_FREETYPE 
 { "font_encoding",&subtitle_font_encoding,CONF_TYPE_STRING,0,0,0,NULL },
 { "font_text_scale",&text_font_scale_factor,CONF_TYPE_FLOAT,CONF_RANGE,0,100,NULL },
 { "font_osd_scale",&osd_font_scale_factor,CONF_TYPE_FLOAT,CONF_RANGE,0,100,NULL },
 { "font_blur",&subtitle_font_thickness,CONF_TYPE_FLOAT,CONF_RANGE,0,8,NULL },
 { "font_outline",&subtitle_font_thickness,CONF_TYPE_FLOAT,CONF_RANGE,0,8,NULL },
 { "font_autoscale",&subtitle_autoscale,CONF_TYPE_INT,CONF_RANGE,0,3,NULL },
#endif
 
 { "gui_skin",&skinName,CONF_TYPE_STRING,0,0,0,NULL },

 { "equ_channel_1",&gtkEquChannel1,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_2",&gtkEquChannel2,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_3",&gtkEquChannel3,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_4",&gtkEquChannel4,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_5",&gtkEquChannel5,CONF_TYPE_STRING,0,0,0,NULL },
 { "equ_channel_6",&gtkEquChannel6,CONF_TYPE_STRING,0,0,0,NULL },
 
#if 1
#define audio_equ_row( i,j ) { "equ_band_"#i#j,&gtkEquChannels[i][j],CONF_TYPE_FLOAT,CONF_RANGE,-15.0,15.0,NULL },
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

char * gfgets( char * str, int size, FILE * f )
{
 char * s = fgets( str,size,f );
 char   c;
 if ( s )
  {
   c=s[ strlen( s ) - 1 ]; if ( c == '\n' || c == '\r' ) s[ strlen( s ) - 1 ]=0;
   c=s[ strlen( s ) - 1 ]; if ( c == '\n' || c == '\r' ) s[ strlen( s ) - 1 ]=0;
  }
 return s;
}

int cfg_read( void )
{
 char * cfg = get_path( "gui.conf" );
 FILE * f;

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
 cfg=get_path( "gui.pl" );
 if ( (f=fopen( cfg,"rt" )) )
  {
   while ( !feof( f ) )
    {
     char tmp[512]; plItem * item;
     if ( gfgets( tmp,512,f ) == NULL ) continue;
     item=calloc( 1,sizeof( plItem ) );
     item->path=strdup( tmp );
     gfgets( tmp,512,f );
     item->name=strdup( tmp );
     gtkSet( gtkAddPlItem,0,(void*)item );
    }
   fclose( f );
  }
 free( cfg );

  //-- read previously visited urls
 cfg=get_path( "gui.url" );
 if ( (f=fopen( cfg,"rt" )) )
  {
   while ( !feof( f ) )
    {
     char tmp[512]; URLItem * item;
     if ( gfgets( tmp,512,f ) == NULL ) continue;
     item=calloc( 1,sizeof( URLItem ) );
     item->url=strdup( tmp );
     gtkSet( gtkAddURLItem,0,(void*)item );
    }
   fclose( f );
  }
 free( cfg );

 return 0;
}

int cfg_write( void )
{
 char * cfg = get_path( "gui.conf" );
 FILE * f;
 int    i;

#ifdef USE_SETLOCALE
 setlocale( LC_ALL,"" );
#endif

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

// -- save URL's
 cfg=get_path( "gui.url" );
 if ( (f=fopen( cfg,"wt+" )) )
  {
   while ( URLList )
    {
     if ( URLList->url ) fprintf( f,"%s\n",URLList->url );
     URLList=URLList->next;
    }
   fclose( f );
  }
 free( cfg );

 return 0;
}

