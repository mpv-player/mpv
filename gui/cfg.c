/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "mixer.h"
#include "mplayer.h"
#include "m_config.h"
#include "m_option.h"
#include "get_path.h"
#include "libvo/sub.h"
#include "libvo/video_out.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libass/ass.h"
#include "libass/ass_mp.h"

#include "cfg.h"
#include "app.h"
#include "interface.h"
#include "mplayer/play.h"

// --- params

int    gtkEnableAudioEqualizer = 0;

int    gtkVfPP = 0;
#ifdef CONFIG_LIBAVCODEC
 int    gtkVfLAVC = 0;
#endif

int    gtkAONorm = 0;
int    gtkAOSurround = 0;
int    gtkAOExtraStereo = 0;
float  gtkAOExtraStereoMul = 1.0;
#ifdef CONFIG_OSS_AUDIO
char * gtkAOOSSMixer;
char * gtkAOOSSMixerChannel;
char * gtkAOOSSDevice;
#endif
#ifdef CONFIG_ALSA
char * gtkAOALSAMixer;
char * gtkAOALSAMixerChannel;
char * gtkAOALSADevice;
#endif
#ifdef CONFIG_SDL
char * gtkAOSDLDriver;
#endif
#ifdef CONFIG_ESD
char * gtkAOESDDevice;
#endif

int    gtkCacheOn = 0;
int    gtkCacheSize = 2048;

int    gtkAutoSyncOn = 0;
int    gtkAutoSync = 0;

#ifdef CONFIG_DXR3
 char * gtkDXR3Device;
#endif

int    gtkSubDumpMPSub = 0;
int    gtkSubDumpSrt = 0;

int    gtkLoadFullscreen = 0;
int    gtkShowVideoWindow = 1;
int    gtkEnablePlayBar = 1;

int    gui_save_pos = 1;
int    gui_main_pos_x = -2;
int    gui_main_pos_y = -2;
int    gui_sub_pos_x = -1;
int    gui_sub_pos_y = -1;

#ifdef CONFIG_ASS
gtkASS_t gtkASS;
#endif
// ---

extern int    stop_xscreensaver;
extern int    disable_gui_conf;
int m_config_parse_config_file(m_config_t* config, char *conffile);

static m_config_t * gui_conf;
static const m_option_t gui_opts[] =
{
 { "enable_audio_equ",&gtkEnableAudioEqualizer,CONF_TYPE_FLAG,0,0,1,NULL },
 
 { "vo_driver",&video_driver_list,CONF_TYPE_STRING_LIST,0,0,0,NULL },
 { "vo_panscan",&vo_panscan,CONF_TYPE_FLOAT,CONF_RANGE,0.0,1.0,NULL },
 { "vo_doublebuffering",&vo_doublebuffering,CONF_TYPE_FLAG,0,0,1,NULL },
 { "vo_direct_render",&vo_directrendering,CONF_TYPE_FLAG,0,0,1,NULL },
#ifdef CONFIG_DXR3
 { "vo_dxr3_device",&gtkDXR3Device,CONF_TYPE_STRING,0,0,0,NULL },
#endif

 { "v_framedrop",&frame_dropping,CONF_TYPE_INT,CONF_RANGE,0,2,NULL },
 { "v_flip",&flip,CONF_TYPE_INT,CONF_RANGE,-1,1,NULL },
 { "v_ni",&force_ni,CONF_TYPE_FLAG,0,0,1,NULL },
 { "v_idx",&index_mode,CONF_TYPE_INT,CONF_RANGE,-1,2,NULL },
 { "v_vfm",&video_fm_list,CONF_TYPE_STRING_LIST,0,0,0,NULL },
 { "a_afm",&audio_fm_list,CONF_TYPE_STRING_LIST,0,0,0,NULL },

 { "vf_pp",&gtkVfPP,CONF_TYPE_FLAG,0,0,1,NULL },
 { "vf_autoq",&auto_quality,CONF_TYPE_INT,CONF_RANGE,0,100,NULL },
#ifdef CONFIG_LIBAVCODEC
 { "vf_lavc",&gtkVfLAVC,CONF_TYPE_FLAG,0,0,1,NULL },
#endif

 { "ao_driver",&audio_driver_list,CONF_TYPE_STRING_LIST,0,0,0,NULL },
 { "ao_volnorm",&gtkAONorm,CONF_TYPE_FLAG,0,0,1,NULL },
 { "softvol",&soft_vol,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_surround",&gtkAOSurround,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_extra_stereo",&gtkAOExtraStereo,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ao_extra_stereo_coefficient",&gtkAOExtraStereoMul,CONF_TYPE_FLOAT,CONF_RANGE,-10,10,NULL },
#ifdef CONFIG_OSS_AUDIO
 { "ao_oss_mixer",&gtkAOOSSMixer,CONF_TYPE_STRING,0,0,0,NULL },
 { "ao_oss_mixer_channel",&gtkAOOSSMixerChannel,CONF_TYPE_STRING,0,0,0,NULL },
 { "ao_oss_device",&gtkAOOSSDevice,CONF_TYPE_STRING,0,0,0,NULL },
#endif
#ifdef CONFIG_ALSA
 { "ao_alsa_mixer",&gtkAOALSAMixer,CONF_TYPE_STRING,0,0,0,NULL },
 { "ao_alsa_mixer_channel",&gtkAOALSAMixerChannel,CONF_TYPE_STRING,0,0,0,NULL },
 { "ao_alsa_device",&gtkAOALSADevice,CONF_TYPE_STRING,0,0,0,NULL },
#endif
#ifdef CONFIG_SDL
 { "ao_sdl_subdriver",&gtkAOSDLDriver,CONF_TYPE_STRING,0,0,0,NULL },
#endif
#ifdef CONFIG_ESD
 { "ao_esd_device",&gtkAOESDDevice,CONF_TYPE_STRING,0,0,0,NULL },
#endif

 { "dvd_device",&dvd_device,CONF_TYPE_STRING,0,0,0,NULL },
 { "cdrom_device",&cdrom_device,CONF_TYPE_STRING,0,0,0,NULL },
 
 { "osd_level",&osd_level,CONF_TYPE_INT,CONF_RANGE,0,3,NULL },
 { "sub_auto_load",&sub_auto,CONF_TYPE_FLAG,0,0,1,NULL },
 { "sub_unicode",&sub_unicode,CONF_TYPE_FLAG,0,0,1,NULL },
#ifdef CONFIG_ASS
 { "ass_enabled",&ass_enabled,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ass_use_margins",&ass_use_margins,CONF_TYPE_FLAG,0,0,1,NULL },
 { "ass_top_margin",&ass_top_margin,CONF_TYPE_INT,CONF_RANGE,0,512,NULL },
 { "ass_bottom_margin",&ass_bottom_margin,CONF_TYPE_INT,CONF_RANGE,0,512,NULL },
#endif
 { "sub_pos",&sub_pos,CONF_TYPE_INT,CONF_RANGE,0,200,NULL },
 { "sub_overlap",&suboverlap_enabled,CONF_TYPE_FLAG,0,0,0,NULL },
#ifdef CONFIG_ICONV
 { "sub_cp",&sub_cp,CONF_TYPE_STRING,0,0,0,NULL },
#endif
 { "font_factor",&font_factor,CONF_TYPE_FLOAT,CONF_RANGE,0.0,10.0,NULL },
 { "font_name",&font_name,CONF_TYPE_STRING,0,0,0,NULL },
#ifdef CONFIG_FREETYPE
 { "font_encoding",&subtitle_font_encoding,CONF_TYPE_STRING,0,0,0,NULL },
 { "font_text_scale",&text_font_scale_factor,CONF_TYPE_FLOAT,CONF_RANGE,0,100,NULL },
 { "font_osd_scale",&osd_font_scale_factor,CONF_TYPE_FLOAT,CONF_RANGE,0,100,NULL },
 { "font_blur",&subtitle_font_radius,CONF_TYPE_FLOAT,CONF_RANGE,0,8,NULL },
 { "font_outline",&subtitle_font_thickness,CONF_TYPE_FLOAT,CONF_RANGE,0,8,NULL },
 { "font_autoscale",&subtitle_autoscale,CONF_TYPE_INT,CONF_RANGE,0,3,NULL },
#endif

 { "cache",&gtkCacheOn,CONF_TYPE_FLAG,0,0,1,NULL },
 { "cache_size",&gtkCacheSize,CONF_TYPE_INT,CONF_RANGE,-1,65535,NULL },

 { "playbar",&gtkEnablePlayBar,CONF_TYPE_FLAG,0,0,1,NULL }, 
 { "load_fullscreen",&gtkLoadFullscreen,CONF_TYPE_FLAG,0,0,1,NULL },
 { "show_videowin", &gtkShowVideoWindow,CONF_TYPE_FLAG,0,0,1,NULL },
 { "stopxscreensaver",&stop_xscreensaver,CONF_TYPE_FLAG,0,0,1,NULL },

 { "autosync",&gtkAutoSyncOn,CONF_TYPE_FLAG,0,0,1,NULL },
 { "autosync_size",&gtkAutoSync,CONF_TYPE_INT,CONF_RANGE,0,10000,NULL },
 
 { "gui_skin",&skinName,CONF_TYPE_STRING,0,0,0,NULL },

 { "gui_save_pos", &gui_save_pos, CONF_TYPE_FLAG,0,0,1,NULL},
 { "gui_main_pos_x", &gui_main_pos_x, CONF_TYPE_INT,0,0,0,NULL},
 { "gui_main_pos_y", &gui_main_pos_y, CONF_TYPE_INT,0,0,0,NULL},
 { "gui_video_out_pos_x", &gui_sub_pos_x, CONF_TYPE_INT,0,0,0,NULL},
 { "gui_video_out_pos_y", &gui_sub_pos_y, CONF_TYPE_INT,0,0,0,NULL},

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
 mp_msg( MSGT_GPLAYER,MSGL_V,"[cfg] reading config file: %s\n",cfg );
 gui_conf=m_config_new();
 m_config_register_options( gui_conf,gui_opts );
 if ( !disable_gui_conf && m_config_parse_config_file( gui_conf,cfg ) < 0 ) 
  {
   mp_msg( MSGT_GPLAYER,MSGL_FATAL,MSGTR_ConfigFileError );
//   exit( 1 );
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

// -- reade file loader history
 cfg=get_path( "gui.history" );
 if ( (f=fopen( cfg,"rt+" )) )
  {
   int i = 0;
   while ( !feof( f ) )
    {
     char tmp[512];
     if ( gfgets( tmp,512,f ) == NULL ) continue;
     fsHistory[i++]=gstrdup( tmp );
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

// -- save configuration 
 if ( (f=fopen( cfg,"wt+" )) )
  {
   for ( i=0;gui_opts[i].name;i++ )
    {
      char* v = m_option_print(&gui_opts[i],gui_opts[i].p);
      if(v == (char *)-1) {
        mp_msg(MSGT_GPLAYER,MSGL_WARN,MSGTR_UnableToSaveOption, gui_opts[i].name);
        v = NULL;
      }
      if(v) {
	fprintf( f,"%s = \"%s\"\n",gui_opts[i].name, v);
	free(v);
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

// -- save file loader history
 cfg=get_path( "gui.history" );
 if ( (f=fopen( cfg,"wt+" )) )
  {
   int i = 0;
//   while ( fsHistory[i] != NULL )
   for ( i=0;i < 5; i++)
     if( fsHistory[i] ) fprintf( f,"%s\n",fsHistory[i] );
   fclose( f );
  }
 free( cfg );

 return 0;
}

