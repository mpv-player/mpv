
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "../../../config.h"
#include "../../../help_mp.h"
#include "../../../mixer.h"
#include "../../../libao2/audio_out.h"
#include "../../../libvo/video_out.h"

#include "../../cfg.h"
#include "../../interface.h"
#include "../widgets.h"
#include "opts.h"
#include "fs.h"
#include "common.h"

typedef struct sh_video_t sh_video_t;
typedef struct sh_audio_t sh_audio_t;

// for mpcodecs_[av]d_drivers:
#include "../../../libmpcodecs/vd.h"
#include "../../../libmpcodecs/ad.h"

       GtkWidget * Preferences;
static GtkWidget * AConfig;
static GtkWidget * VConfig;
//static GtkWidget * BLoadSubtitle;
static GtkWidget * BLoadFont;
static GtkWidget * BOk;
static GtkWidget * BCancel;

static GtkWidget * CLADrivers;
static GtkWidget * CLVDrivers;

//static GtkWidget * ESubtitleName;
       GtkWidget * prEFontName;
static GtkWidget * EVFM;
static GtkWidget * EAFM;

static GtkWidget * CBVFM;
static GtkWidget * CBAFM;
static GtkWidget * CBAudioEqualizer;
//static GtkWidget * CBSurround;
static GtkWidget * CBExtraStereo;
static GtkWidget * CBNoSound;
static GtkWidget * CBNormalize;
static GtkWidget * CBDoubleBuffer;
static GtkWidget * CBDR;
static GtkWidget * CBFramedrop;
static GtkWidget * CBHFramedrop;
//static GtkWidget * CBFullScreen;
static GtkWidget * CBNonInterlaved;
static GtkWidget * CBIndex;
static GtkWidget * CBFlip;
static GtkWidget * CBNoAutoSub;
static GtkWidget * CBSubUnicode;
static GtkWidget * CBDumpMPSub;
static GtkWidget * CBDumpSrt;
static GtkWidget * CBPostprocess;
static GtkWidget * CBCache;
static GtkWidget * CBLoadFullscreen;

static GtkWidget * SBCache;
static GtkAdjustment * SBCacheadj;

static GtkWidget * RBOSDNone;
static GtkWidget * RBOSDTandP;
static GtkWidget * RBOSDIndicator;

static GtkWidget * HSAudioDelay;
static GtkWidget * HSExtraStereoMul;
static GtkWidget * HSPanscan;
static GtkWidget * HSSubDelay;
static GtkWidget * HSSubPosition;
static GtkWidget * HSSubFPS;
static GtkWidget * HSPPQuality;

static GtkAdjustment * HSExtraStereoMuladj, * HSAudioDelayadj, * HSPanscanadj, * HSSubDelayadj;
static GtkAdjustment * HSSubPositionadj, * HSSubFPSadj, * HSPPQualityadj;

#ifndef HAVE_FREETYPE
static GtkWidget     * HSFontFactor;
static GtkAdjustment * HSFontFactoradj;
#else
static GtkWidget     * HSFontBlur, * HSFontOutLine, * HSFontTextScale, * HSFontOSDScale;
static GtkAdjustment * HSFontBluradj, * HSFontOutLineadj, * HSFontTextScaleadj, * HSFontOSDScaleadj;
static GtkWidget     * CBFontEncoding, * EFontEncoding;
static GtkWidget     * RBFontNoAutoScale, * BRFontAutoScaleWidth, * RBFontAutoScaleHeight, * RBFontAutoScaleDiagonal;
//static GtkWidget     * AutoScale;
#endif

#ifdef HAVE_FREETYPE
static struct 
{
 char * name;
 char * comment;
} lEncoding[] =
 {
  { "unicode",     MSGTR_PREFERENCES_FontEncoding1 },
  { "iso-8859-1",  MSGTR_PREFERENCES_FontEncoding2 },
  { "iso-8859-15", MSGTR_PREFERENCES_FontEncoding3 },
  { "iso-8859-2",  MSGTR_PREFERENCES_FontEncoding4 },
  { "iso-8859-3",  MSGTR_PREFERENCES_FontEncoding5 },
  { "iso-8859-4",  MSGTR_PREFERENCES_FontEncoding6 },
  { "iso-8859-5",  MSGTR_PREFERENCES_FontEncoding7 },
  { "cp1251",      MSGTR_PREFERENCES_FontEncoding21},
  { "iso-8859-6",  MSGTR_PREFERENCES_FontEncoding8 },
  { "iso-8859-7",  MSGTR_PREFERENCES_FontEncoding9 },
  { "iso-8859-9",  MSGTR_PREFERENCES_FontEncoding10 },
  { "iso-8859-13", MSGTR_PREFERENCES_FontEncoding11 },
  { "iso-8859-14", MSGTR_PREFERENCES_FontEncoding12 },
  { "iso-8859-8",  MSGTR_PREFERENCES_FontEncoding13 },
  { "koi8-r",      MSGTR_PREFERENCES_FontEncoding14 },
  { "koi8-u/ru",   MSGTR_PREFERENCES_FontEncoding15 },
  { "cp936",       MSGTR_PREFERENCES_FontEncoding16 },
  { "big5",        MSGTR_PREFERENCES_FontEncoding17 },
  { "shift-jis",   MSGTR_PREFERENCES_FontEncoding18 },
  { "cp949",       MSGTR_PREFERENCES_FontEncoding19 },
  { "cp874",       MSGTR_PREFERENCES_FontEncoding20 },
  { NULL,NULL } 
 };
char * lCEncoding = NULL;
#endif
	    
       int    gtkVPreferences = 0;
static int    old_audio_driver = 0;
static char * ao_driver[3];
static char * vo_driver[3];
static int    old_video_driver = 0;

#ifdef USE_OSS_AUDIO
 void ShowOSSConfig( void );
 void HideOSSConfig( void );
#endif
#ifdef HAVE_DXR3
 void ShowDXR3Config( void );
 void HideDXR3Config( void );
#endif
static gboolean prHScaler( GtkWidget * widget,GdkEventMotion  * event,gpointer user_data );
static void prToggled( GtkToggleButton * togglebutton,gpointer user_data );
static void prCListRow( GtkCList * clist,gint row,gint column,GdkEvent * event,gpointer user_data );
#ifdef HAVE_FREETYPE
static void prEntry( GtkContainer * container,GtkWidget * widget,gpointer user_data );
#endif

extern int    muted;

void ShowPreferences( void )
{
 if ( gtkVPreferences ) gtkActive( Preferences );
   else Preferences=create_Preferences();

// -- 1. page 
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBAudioEqualizer ),gtkEnableAudioEqualizer );
 gtkAONoSound=muted;
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBNoSound ),gtkAONoSound );
#if 0
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBSurround ),gtkAOSurround );
#endif
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBExtraStereo ),gtkAOExtraStereo );
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBNormalize ),gtkAONorm );
 gtk_adjustment_set_value( HSExtraStereoMuladj,gtkAOExtraStereoMul );
 {
  int    i = 0;
  char * tmp[3]; tmp[2]="";
  old_audio_driver=-1;
  while ( audio_out_drivers[i] )
   {
    const ao_info_t *info = audio_out_drivers[i++]->info;
    if ( !strcmp( info->short_name,"plugin" ) ) continue;
    if ( audio_driver_list )
     {
      char * name = gstrdup( audio_driver_list[0] );
      char * sep = strchr( audio_driver_list[0],':' );
      if ( sep ) *sep=0;
      if ( !gstrcmp( name,(char *)info->short_name ) ) old_audio_driver=i - 1;
     }
    tmp[0]=(char *)info->short_name; tmp[1]=(char *)info->name; gtk_clist_append( GTK_CLIST( CLADrivers ),tmp );
   }
  if ( old_audio_driver > -1 )
   {
    gtk_clist_select_row( GTK_CLIST( CLADrivers ),old_audio_driver,0 );
    gtk_clist_get_text( GTK_CLIST( CLADrivers ),old_audio_driver,0,(char **)&ao_driver );
    gtk_widget_set_sensitive( AConfig,FALSE );
#ifdef USE_OSS_AUDIO
    if ( !strncmp( ao_driver[0],"oss",3 ) ) gtk_widget_set_sensitive( AConfig,TRUE );
#endif
   }
 }

// -- 2. page
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBDoubleBuffer ),vo_doublebuffering );
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBDR ),vo_directrendering );

 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBFramedrop ),FALSE );
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBHFramedrop ),FALSE );
 switch ( frame_dropping )
  {
   case 2: gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBHFramedrop ),TRUE );
   case 1: gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBFramedrop ),TRUE );
  }

 if (flip != -1)
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBFlip ),flip );
 gtk_adjustment_set_value( HSPanscanadj,vo_panscan );

 {
  int i = 0, c = 0;
  char * tmp[3]; tmp[2]="";
  old_video_driver=0; 
  while ( video_out_drivers[i] )
   if ( video_out_drivers[i++]->control( VOCTRL_GUISUPPORT,NULL ) == VO_TRUE )
    { 
     if ( video_driver_list && !gstrcmp( video_driver_list[0],(char *)video_out_drivers[i - 1]->info->short_name ) ) old_video_driver=c; c++;
     tmp[0]=(char *)video_out_drivers[i - 1]->info->short_name; tmp[1]=(char *)video_out_drivers[i - 1]->info->name; 
     gtk_clist_append( GTK_CLIST( CLVDrivers ),tmp );
    }
  gtk_clist_select_row( GTK_CLIST( CLVDrivers ),old_video_driver,0 );
  gtk_clist_get_text( GTK_CLIST( CLVDrivers ),old_video_driver,0,(char **)&vo_driver );
  gtk_widget_set_sensitive( VConfig,FALSE );
#ifdef HAVE_DXR3
  if ( !gstrcmp( vo_driver[0],"dxr3" ) ) gtk_widget_set_sensitive( VConfig,TRUE );
#endif
 }

// -- 3. page
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBNoAutoSub ),!sub_auto );
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBDumpMPSub ),gtkSubDumpMPSub );
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBDumpSrt ),gtkSubDumpSrt );
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBSubUnicode ),sub_unicode );
 gtk_adjustment_set_value( HSSubDelayadj,sub_delay );
 gtk_adjustment_set_value( HSSubFPSadj,sub_fps );
 gtk_adjustment_set_value( HSSubPositionadj,sub_pos );
 switch ( osd_level )
  {
   case 0: gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RBOSDNone ),TRUE ); break;
   case 1: gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RBOSDIndicator ),TRUE ); break;
   case 2: gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RBOSDTandP ),TRUE ); break;
  }
#if 0
 if ( guiIntfStruct.Subtitlename ) gtk_entry_set_text( GTK_ENTRY( ESubtitleName ),guiIntfStruct.Subtitlename );
#endif
 // font ...
 if ( font_name ) gtk_entry_set_text( GTK_ENTRY( prEFontName ),font_name );
#ifndef HAVE_FREETYPE
 gtk_adjustment_set_value( HSFontFactoradj,font_factor );
#else
 gtk_adjustment_set_value( HSFontBluradj,( subtitle_font_radius / 8.0f ) * 100.0f );
 gtk_adjustment_set_value( HSFontOutLineadj,( subtitle_font_thickness / 8.0f ) * 100.0f );
 gtk_adjustment_set_value( HSFontTextScaleadj,text_font_scale_factor );
 gtk_adjustment_set_value( HSFontOSDScaleadj,osd_font_scale_factor );
 if ( subtitle_font_encoding )
  {
   int i;
   for ( i=0;lEncoding[i].name;i++ ) 
    if ( !gstrcmp( subtitle_font_encoding,lEncoding[i].name ) ) break;
   if ( lEncoding[i].name ) lCEncoding=lEncoding[i].comment;
   gtk_entry_set_text( GTK_ENTRY( EFontEncoding ),lCEncoding );
  }
 switch ( subtitle_autoscale )
  {
   case 0: gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RBFontNoAutoScale ),TRUE ); break;
   case 1: gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( BRFontAutoScaleWidth ),TRUE ); break;
   case 2: gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RBFontAutoScaleHeight ),TRUE ); break;
   case 3: gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RBFontAutoScaleDiagonal ),TRUE ); break;
  }
#endif

// -- 4. page
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBNonInterlaved ),force_ni );
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBIndex ),index_mode );
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBPostprocess ),gtkVopPP );
 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBLoadFullscreen ),gtkLoadFullscreen );
 gtk_adjustment_set_value( HSPPQualityadj,auto_quality );
 {
  int     i;
  GList * Items = NULL;
  char  * name = NULL;

  Items=g_list_append( Items,MSGTR_PREFERENCES_None );
  for( i=0;mpcodecs_vd_drivers[i];i++ )
   {
    Items=g_list_append( Items,(char *)mpcodecs_vd_drivers[i]->info->name );
    if ( video_fm_list && !gstrcmp( video_fm_list[0],(char *)mpcodecs_vd_drivers[i]->info->short_name ) ) name=(char *)mpcodecs_vd_drivers[i]->info->name;
   }
  gtk_combo_set_popdown_strings( GTK_COMBO( CBVFM ),Items );
  g_list_free( Items );
  if ( name ) gtk_entry_set_text( GTK_ENTRY( EVFM ),name );
 }

 {
  int     i;
  GList * Items = NULL;
  char  * name = NULL;

  Items=g_list_append( Items,MSGTR_PREFERENCES_None );
  for( i=0;mpcodecs_ad_drivers[i];i++ )
   {
    Items=g_list_append( Items,(char *)mpcodecs_ad_drivers[i]->info->name );
    if ( audio_fm_list && !gstrcmp( audio_fm_list[0],(char *)mpcodecs_ad_drivers[i]->info->short_name ) ) name=(char *)mpcodecs_ad_drivers[i]->info->name;
   }
  gtk_combo_set_popdown_strings( GTK_COMBO( CBAFM ),Items );
  g_list_free( Items );
  if ( name ) gtk_entry_set_text( GTK_ENTRY( EAFM ),name );
 }

 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBCache ),0 );
 if ( stream_cache_size )
  {
   gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBCache ),1 );
   gtk_adjustment_set_value( SBCacheadj,stream_cache_size );
  } else gtk_widget_set_sensitive( SBCache,FALSE );

// -- disables
#ifndef USE_SUB
 gtk_widget_set_sensitive( AConfig,FALSE );
 gtk_widget_set_sensitive( CBNoAutoSub,FALSE );
 gtk_widget_set_sensitive( CBSubUnicode,FALSE );
 gtk_widget_set_sensitive( CBDumpMPSub,FALSE );
 gtk_widget_set_sensitive( CBDumpSrt,FALSE );
 gtk_widget_set_sensitive( HSSubDelay,FALSE );
 gtk_widget_set_sensitive( HSSubPosition,FALSE );
 gtk_widget_set_sensitive( HSSubFPS,FALSE );
#endif

#ifndef USE_OSD
 gtk_widget_set_sensitive( RBOSDNone,FALSE );
 gtk_widget_set_sensitive( RBOSDTandP,FALSE );
 gtk_widget_set_sensitive( RBOSDIndicator,FALSE );
#endif

#if !defined( USE_OSD ) && !defined( USE_SUB )
 gtk_widget_set_sensitive( HSFontFactor,FALSE );
 gtk_widget_set_sensitive( prEFontName,FALSE );
 gtk_widget_set_sensitive( BLoadFont,FALSE );
#endif

// -- signals
 gtk_signal_connect( GTK_OBJECT( CBExtraStereo ),"toggled",GTK_SIGNAL_FUNC( prToggled ),(void*)0 );
 gtk_signal_connect( GTK_OBJECT( CBNormalize ),"toggled",GTK_SIGNAL_FUNC( prToggled ),(void*)1 );
 gtk_signal_connect( GTK_OBJECT( CBAudioEqualizer ),"toggled",GTK_SIGNAL_FUNC( prToggled ),(void*)2 );
 gtk_signal_connect( GTK_OBJECT( CBNoSound ),"toggled",GTK_SIGNAL_FUNC( prToggled ),(void*)3 );
#ifdef HAVE_FREETYPE
 gtk_signal_connect( GTK_OBJECT( RBFontNoAutoScale ),"toggled",GTK_SIGNAL_FUNC( prToggled ),(void*)4 );
 gtk_signal_connect( GTK_OBJECT( BRFontAutoScaleWidth ),"toggled",GTK_SIGNAL_FUNC( prToggled ),(void*)5 );
 gtk_signal_connect( GTK_OBJECT( RBFontAutoScaleHeight ),"toggled",GTK_SIGNAL_FUNC( prToggled ),(void*)6 );
 gtk_signal_connect( GTK_OBJECT( RBFontAutoScaleDiagonal ),"toggled",GTK_SIGNAL_FUNC( prToggled ),(void*)7 );
#endif
 gtk_signal_connect( GTK_OBJECT( CBCache ),"toggled",GTK_SIGNAL_FUNC( prToggled ),(void*)8);

 gtk_signal_connect( GTK_OBJECT( HSExtraStereoMul ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)0 );
 gtk_signal_connect( GTK_OBJECT( HSAudioDelay ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)1 );
 gtk_signal_connect( GTK_OBJECT( HSPanscan ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)2 );
 gtk_signal_connect( GTK_OBJECT( HSSubDelay ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)3 );
 gtk_signal_connect( GTK_OBJECT( HSSubPosition ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)4 );
#ifndef HAVE_FREETYPE
 gtk_signal_connect( GTK_OBJECT( HSFontFactor ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)5 );
#else
 gtk_signal_connect( GTK_OBJECT( HSFontBlur ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)6 );
 gtk_signal_connect( GTK_OBJECT( HSFontOutLine ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)7 );
 gtk_signal_connect( GTK_OBJECT( HSFontTextScale ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)8 );
 gtk_signal_connect( GTK_OBJECT( HSFontOSDScale ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)9 );
 gtk_signal_connect( GTK_OBJECT( EFontEncoding ),"changed",GTK_SIGNAL_FUNC( prEntry ),NULL );
#endif
 gtk_signal_connect( GTK_OBJECT( HSPPQuality ),"motion_notify_event",GTK_SIGNAL_FUNC( prHScaler ),(void*)10 );
 
 gtk_signal_connect( GTK_OBJECT( CLADrivers ),"select_row",GTK_SIGNAL_FUNC( prCListRow ),(void*)0 );
 gtk_signal_connect( GTK_OBJECT( CLVDrivers ),"select_row",GTK_SIGNAL_FUNC( prCListRow ),(void*)1 );

 gtkVPreferences=1;
 gtk_widget_show( Preferences );
 gtkSetLayer( Preferences );
 gtkMessageBox( GTK_MB_WARNING,MSGTR_PREFERENCES_Message );
}

void HidePreferences( void )
{
 if ( !gtkVPreferences ) return;
 gtkVPreferences=0;
 gtk_widget_hide( Preferences ); gtk_widget_destroy( Preferences );
#ifdef USE_OSS_AUDIO
 HideOSSConfig();
#endif
#ifdef HAVE_DXR3
 HideDXR3Config();
#endif
}

#ifdef HAVE_FREETYPE
static void prEntry( GtkContainer * container,GtkWidget * widget,gpointer user_data )
{
 char * comment = gtk_entry_get_text( GTK_ENTRY( EFontEncoding ) );
 int    i;
 
 for ( i=0;lEncoding[i].name;i++ )
  if ( !gstrcmp( lEncoding[i].comment,comment ) ) break;
  
 if ( lEncoding[i].comment ) gtkSet( gtkSetFontEncoding,0,lEncoding[i].name );
}
#endif

static void prDestroy( GtkObject * object,gpointer user_data )
{ HidePreferences(); }

static void prShow( GtkWidget * widget,gpointer user_data )
{ gtkVPreferences=(int)user_data; }

#define bAConfig   0
#define bVconfig   1
#define bOk	   2
#define bCancel    3
#define bLSubtitle 4
#define bLFont     5

void prButton( GtkButton * button,gpointer user_data )
{
 switch ( (int)user_data )
  {
   case bOk:
	// -- 1. page
        gtkEnableAudioEqualizer=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBAudioEqualizer ) );
	gtkAOExtraStereo=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBExtraStereo ) );
	gtkAONorm=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBNormalize ) );
	gtkAONoSound=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBNoSound ) );
	gtkSet( gtkSetExtraStereo,HSExtraStereoMuladj->value,NULL );
	audio_delay=HSAudioDelayadj->value;

	gaddlist( &audio_driver_list,ao_driver[0] );
	gaddlist( &video_driver_list,vo_driver[0] );

	// -- 2. page
	vo_doublebuffering=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBDoubleBuffer ) );
	vo_directrendering=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBDR ) );

        frame_dropping=0;
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBFramedrop ) ) == TRUE ) frame_dropping=1;
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBHFramedrop ) ) == TRUE ) frame_dropping=2;

	flip=-1;
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBFlip ) ) ) flip=1;
	
	// -- 3. page
	sub_auto=!gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBNoAutoSub ) );
	gtkSubDumpMPSub=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBDumpMPSub ) );
	gtkSubDumpSrt=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBDumpSrt ) );
	sub_unicode=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBSubUnicode ) );
	sub_delay=HSSubDelayadj->value;
	sub_fps=HSSubFPSadj->value;
	sub_pos=(int)HSSubPositionadj->value;
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( RBOSDNone ) ) ) osd_level=0;
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( RBOSDIndicator ) ) ) osd_level=1;
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( RBOSDTandP ) ) ) osd_level=2;

        // font ...	
	guiSetFilename( font_name,gtk_entry_get_text( GTK_ENTRY( prEFontName ) ) );
#ifndef HAVE_FREETYPE
	gtkSet( gtkSetFontFactor,HSFontFactoradj->value,NULL );
#else
	gtkSet( gtkSetFontBlur,HSFontBluradj->value,NULL );
	gtkSet( gtkSetFontOutLine,HSFontOutLineadj->value,NULL );
	gtkSet( gtkSetFontTextScale,HSFontTextScaleadj->value,NULL );
	gtkSet( gtkSetFontOSDScale,HSFontOSDScaleadj->value,NULL );
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( RBFontNoAutoScale ) ) ) gtkSet( gtkSetFontAutoScale,0,NULL );
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( BRFontAutoScaleWidth ) ) ) gtkSet( gtkSetFontAutoScale,1,NULL );
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( RBFontAutoScaleHeight ) ) ) gtkSet( gtkSetFontAutoScale,2,NULL );
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( RBFontAutoScaleDiagonal ) ) ) gtkSet( gtkSetFontAutoScale,3,NULL );
#endif

	// -- 4. page
	force_ni=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBNonInterlaved ) );
	index_mode=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBIndex ) );
	gtkVopPP=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBPostprocess ) ); 
	gtkLoadFullscreen=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBLoadFullscreen ) );
	gtkSet( gtkSetAutoq,HSPPQualityadj->value,NULL );

	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBCache ) ) ) stream_cache_size=SBCacheadj->value;
	 else stream_cache_size=0;

	{
	 int i;
	 char * tmp = gtk_entry_get_text( GTK_ENTRY( EVFM ) );
         for( i=0;mpcodecs_vd_drivers[i];i++ )
          if ( !gstrcmp( tmp,(char *)mpcodecs_vd_drivers[i]->info->name ) ) 
	   { gaddlist( &video_fm_list,(char *)mpcodecs_vd_drivers[i]->info->short_name ); break; }
	}

	{
	 int i;
	 char * tmp = gtk_entry_get_text( GTK_ENTRY( EAFM ) );
         for( i=0;mpcodecs_ad_drivers[i];i++ )
          if ( !gstrcmp( tmp,(char *)mpcodecs_ad_drivers[i]->info->name ) )
	   { gaddlist( &audio_fm_list,(char *)mpcodecs_ad_drivers[i]->info->short_name ); break; }
	}

   case bCancel:
	HidePreferences();
	break;
   case bAConfig:
        gtk_widget_set_sensitive( AConfig,FALSE );
#ifdef USE_OSS_AUDIO
        if ( !strncmp( ao_driver[0],"oss",3 ) ) { ShowOSSConfig(); gtk_widget_set_sensitive( AConfig,TRUE ); }
#endif
	break;
   case bVconfig:
        gtk_widget_set_sensitive( VConfig,FALSE );
#ifdef HAVE_DXR3
	if ( !gstrcmp( vo_driver[0],"dxr3" ) ) { ShowDXR3Config(); gtk_widget_set_sensitive( VConfig,TRUE ); }
#endif
	break;
#if 0
   case bLSubtitle:
	break;
#endif
   case bLFont:
        ShowFileSelect( fsFontSelector,FALSE );
	gtkSetLayer( fsFileSelect );
	break;
  }
}

static gboolean prHScaler( GtkWidget * widget,GdkEventMotion  * event,gpointer user_data )
{
 switch ( (int)user_data )
  {
   case 0: // extra stereo coefficient
	if ( !guiIntfStruct.Playing ) break;
	gtkSet( gtkSetExtraStereo,HSExtraStereoMuladj->value,NULL );
	break;
   case 1: // audio delay
	audio_delay=HSAudioDelayadj->value;
	break;
   case 2: // panscan
        gtkSet( gtkSetPanscan,HSPanscanadj->value,NULL );
	break;
   case 3: // sub delay
        sub_delay=HSSubDelayadj->value;
	break;
   case 4: // sub position
        sub_pos=(int)HSSubPositionadj->value;
	break;
#ifndef HAVE_FREETYPE
   case 5: // font factor
        gtkSet( gtkSetFontFactor,HSFontFactoradj->value,NULL );
	break;
#else
   case 6: // font blur
	gtkSet( gtkSetFontBlur,HSFontBluradj->value,NULL );
        break;
   case 7: // font outline
        gtkSet( gtkSetFontOutLine,HSFontOutLineadj->value,NULL );
        break;
   case 8: // text scale
        gtkSet( gtkSetFontTextScale,HSFontTextScaleadj->value,NULL );
	break;
   case 9: // osd scale
        gtkSet( gtkSetFontOSDScale,HSFontOSDScaleadj->value,NULL );
	break;
#endif
   case 10: // auto quality
	gtkSet( gtkSetAutoq,HSPPQualityadj->value,NULL );
	break;
  }
 return FALSE;
}

static void prToggled( GtkToggleButton * togglebutton,gpointer user_data )
{
 switch ( (int)user_data )
  {
   case 0: // extra stereo coefficient
	if ( guiIntfStruct.Playing ) 
	gtk_widget_set_sensitive( HSExtraStereoMul,gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBExtraStereo ) ) );
	break;
//   case 1: // normalize
//   case 2: // equalizer
//	if ( guiIntfStruct.Playing ) gtkMessageBox( GTK_MB_WARNING,"Please remember, this function need restart the playing." );
//	break;
   case 3: // no sound
        mixer_mute();
	break;
   case 4:
   case 5:
   case 6:
   case 7:
	gtkSet( gtkSetFontAutoScale,(float)((int)user_data - 4 ),NULL );
	break;
   case 8:
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( CBCache ) ) ) gtk_widget_set_sensitive( SBCache,TRUE );
	 else gtk_widget_set_sensitive( SBCache,FALSE );
	break;
  }
}

static void prCListRow( GtkCList * clist,gint row,gint column,GdkEvent * event,gpointer user_data )
{
 switch ( (int)user_data )
  {
   case 0: // audio driver 
	gtk_clist_get_text( GTK_CLIST( CLADrivers ),row,0,(char **)&ao_driver ); 
	gtk_widget_set_sensitive( AConfig,FALSE );
#ifdef USE_OSS_AUDIO
	if ( !strncmp( ao_driver[0],"oss",3 ) ) gtk_widget_set_sensitive( AConfig,TRUE );
#endif
	break;
   case 1: // video driver 
	gtk_clist_get_text( GTK_CLIST( CLVDrivers ),row,0,(char **)&vo_driver ); 
	gtk_widget_set_sensitive( VConfig,FALSE );
#ifdef HAVE_DXR3
	if ( !gstrcmp( vo_driver[0],"dxr3" ) ) gtk_widget_set_sensitive( VConfig,TRUE );
#endif
	break;
  } 
}

GtkWidget * create_Preferences( void )
{
  GtkWidget * label;
  GtkWidget * frame;

  GtkWidget * vbox1;
  GtkWidget * notebook1;
  GtkWidget * hbox1;
  GtkWidget * vbox2;
  GtkWidget * scrolledwindow3;
  GtkWidget * hbuttonbox2;
  GtkWidget * vbox3;
  GtkWidget * hbox8;
  GtkWidget * hbox2;
  GtkWidget * vbox4;
  GtkWidget * scrolledwindow2;
  GtkWidget * hbuttonbox3;
  GtkWidget * vbox5;
  GtkWidget * hbox3;
  GtkWidget * vbox6;
  GtkWidget * vbox600;
  GSList    * OSD_group = NULL;
  GSList    * Font_group = NULL;
  GList     * CBFontEncoding_items = NULL;
  GtkWidget * vbox7;
  GtkWidget * vbox8;
  GtkWidget * table1;
  GtkWidget * vbox9;
  GtkWidget * vbox603;
  GtkWidget * hbox6;
  GtkWidget * hbuttonbox5;
#ifndef HAVE_FREETYPE
  GtkWidget * hbox7;
#endif
  GtkWidget * vbox601;
  GtkWidget * vbox602;
  GtkWidget * hbox5;
  GtkWidget * hbuttonbox1;
  GtkAccelGroup * accel_group;

  accel_group=gtk_accel_group_new();

  Preferences=gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_widget_set_name( Preferences,"Preferences" );
  gtk_object_set_data( GTK_OBJECT( Preferences ),"Preferences",Preferences );
  gtk_window_set_title( GTK_WINDOW( Preferences ),MSGTR_Preferences );
  gtk_window_set_position( GTK_WINDOW( Preferences ),GTK_WIN_POS_CENTER );
//  gtk_window_set_policy( GTK_WINDOW( Preferences ),FALSE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( Preferences ),"Preferences","MPlayer" );
  
  gtk_widget_realize( Preferences );
  gtkAddIcon( Preferences );

  frame=AddFrame( NULL,GTK_SHADOW_IN,Preferences,1 );
  gtk_container_set_border_width( GTK_CONTAINER( frame ),1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  frame=AddFrame( NULL,GTK_SHADOW_ETCHED_OUT,frame,1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );

  vbox1=AddVBox( frame );

  notebook1=gtk_notebook_new();
  gtk_widget_set_name( notebook1,"notebook1" );
  gtk_widget_ref( notebook1 );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"notebook1",notebook1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( notebook1 );
  gtk_box_pack_start( GTK_BOX( vbox1 ),notebook1,TRUE,TRUE,0 );

  hbox1=AddHBox( notebook1,0 );

  frame=AddFrame( NULL,GTK_SHADOW_ETCHED_OUT,hbox1,1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );

  vbox2=AddVBox( frame );

  scrolledwindow3=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_set_name( scrolledwindow3,"scrolledwindow3" );
  gtk_widget_ref( scrolledwindow3 );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"scrolledwindow3",scrolledwindow3,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( scrolledwindow3 );
  gtk_box_pack_start( GTK_BOX( vbox2 ),scrolledwindow3,TRUE,TRUE,0 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow3 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  CLADrivers=gtk_clist_new( 2 );
  gtk_widget_set_name( CLADrivers,"CLADrivers" );
  gtk_widget_ref( CLADrivers );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"CLADrivers",CLADrivers,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CLADrivers );
  gtk_container_add( GTK_CONTAINER( scrolledwindow3 ),CLADrivers );
  gtk_clist_set_column_width( GTK_CLIST( CLADrivers ),0,50 );
  gtk_clist_column_titles_show( GTK_CLIST( CLADrivers ) );
  gtk_clist_set_shadow_type( GTK_CLIST( CLADrivers ),GTK_SHADOW_NONE );
  gtk_widget_set_usize( CLADrivers,200,-2 );

  label=AddLabel( MSGTR_PREFERENCES_AvailableDrivers,NULL );
    gtk_clist_set_column_widget( GTK_CLIST( CLADrivers ),0,label );

  hbuttonbox2=AddHButtonBox( vbox2 );
  AConfig=AddButton( MSGTR_ConfigDriver,hbuttonbox2 );

  frame=AddFrame( NULL,GTK_SHADOW_ETCHED_OUT,hbox1,1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox3=AddVBox( frame );
    gtk_widget_set_usize( vbox3,250,-2 );

  CBNoSound=AddCheckButton( MSGTR_PREFERENCES_DoNotPlaySound,vbox3 );
  AddHSeparator( vbox3 );
  CBNormalize=AddCheckButton( MSGTR_PREFERENCES_NormalizeSound,vbox3 );
  CBAudioEqualizer=AddCheckButton( MSGTR_PREFERENCES_EnEqualizer,vbox3 );
#if 0
  CBSurround=AddCheckButton( "Enable surround",vbox3 );
#endif
  CBExtraStereo=AddCheckButton( MSGTR_PREFERENCES_ExtraStereo,vbox3 );

  hbox8=AddHBox( vbox3,1 );

  label=AddLabel( MSGTR_PREFERENCES_Coefficient,hbox8 );
    gtk_misc_set_padding( GTK_MISC( label ),20,0 );

  HSExtraStereoMuladj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-10,10,0.1,0,0 ) );
  HSExtraStereoMul=AddHScaler( HSExtraStereoMuladj,hbox8,1 );
  hbox8=AddHBox( vbox3,1 );
  AddLabel( MSGTR_PREFERENCES_AudioDelay,hbox8 );

  HSAudioDelayadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-100,100,0.01,0,0 ) );
  HSAudioDelay=AddHScaler( HSAudioDelayadj,hbox8,2 );
  label=AddLabel( MSGTR_PREFERENCES_Audio,NULL );
    gtk_notebook_set_tab_label( GTK_NOTEBOOK( notebook1 ),gtk_notebook_get_nth_page( GTK_NOTEBOOK( notebook1 ),0 ),label );
  hbox2=AddHBox( notebook1,0 );

  frame=AddFrame( NULL,GTK_SHADOW_ETCHED_OUT,hbox2,1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox4=AddVBox( frame );

  scrolledwindow2=gtk_scrolled_window_new( NULL,NULL );
  gtk_widget_set_name( scrolledwindow2,"scrolledwindow2" );
  gtk_widget_ref( scrolledwindow2 );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"scrolledwindow2",scrolledwindow2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( scrolledwindow2 );
  gtk_box_pack_start( GTK_BOX( vbox4 ),scrolledwindow2,TRUE,TRUE,0 );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow2 ),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC );

  CLVDrivers=gtk_clist_new( 2 );
  gtk_widget_set_name( CLVDrivers,"CLVDrivers" );
  gtk_widget_ref( CLVDrivers );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"CLVDrivers",CLVDrivers,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CLVDrivers );
  gtk_container_add( GTK_CONTAINER( scrolledwindow2 ),CLVDrivers );
  gtk_clist_set_column_width( GTK_CLIST( CLVDrivers ),0,50 );
  gtk_clist_column_titles_show( GTK_CLIST( CLVDrivers ) );
  gtk_clist_set_shadow_type( GTK_CLIST( CLVDrivers ),GTK_SHADOW_NONE );
  gtk_widget_set_usize( CLVDrivers,200,-2 );

  label=AddLabel( MSGTR_PREFERENCES_AvailableDrivers,NULL );
    gtk_clist_set_column_widget( GTK_CLIST( CLVDrivers ),0,label );

  hbuttonbox3=AddHButtonBox( vbox4 );
  VConfig=AddButton( MSGTR_ConfigDriver,hbuttonbox3 );

  frame=AddFrame( NULL,GTK_SHADOW_ETCHED_OUT,hbox2,1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox5=AddVBox( frame );
    gtk_widget_set_usize( vbox5,250,-2 );

  CBDoubleBuffer=AddCheckButton( MSGTR_PREFERENCES_DoubleBuffer,vbox5 );
//  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( CBDoubleBuffer ),TRUE );
  CBDR=AddCheckButton( MSGTR_PREFERENCES_DirectRender,vbox5 );
  CBFramedrop=AddCheckButton( MSGTR_PREFERENCES_FrameDrop,vbox5 );
  CBHFramedrop=AddCheckButton( MSGTR_PREFERENCES_HFrameDrop,vbox5 );
  CBFlip=AddCheckButton( MSGTR_PREFERENCES_Flip,vbox5 );

  hbox3=AddHBox( vbox5,1 );

  AddLabel( MSGTR_PREFERENCES_Panscan,hbox3 );

  HSPanscanadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,0,1,0.001,0,0 ) );
  HSPanscan=AddHScaler( HSPanscanadj,hbox3,1 );

  label=AddLabel( MSGTR_PREFERENCES_Video,NULL );
    gtk_notebook_set_tab_label( GTK_NOTEBOOK( notebook1 ),gtk_notebook_get_nth_page( GTK_NOTEBOOK( notebook1 ),1 ),label );
  vbox6=AddVBox( notebook1 );

  frame=AddFrame( MSGTR_PREFERENCES_FRAME_OSD_Level,GTK_SHADOW_ETCHED_OUT,vbox6,1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox600=AddVBox( frame );

  RBOSDNone=AddRadioButton( MSGTR_PREFERENCES_None,&OSD_group,vbox600 );
  RBOSDTandP=AddRadioButton( MSGTR_PREFERENCES_OSDTimer,&OSD_group,vbox600 );
  RBOSDIndicator=AddRadioButton( MSGTR_PREFERENCES_OSDProgress,&OSD_group,vbox600 );

  frame=AddFrame( MSGTR_PREFERENCES_FRAME_Subtitle,GTK_SHADOW_ETCHED_OUT,vbox6,1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox7=AddVBox( frame );

#if 0
  hbox4=AddHBox( vbox7,1 );

  AddLabel( MSGTR_PREFERENCES_Subtitle,hbox4 );

  ESubtitleName=gtk_entry_new();
  gtk_widget_set_name( ESubtitleName,"ESubtitleName" );
  gtk_widget_ref( ESubtitleName );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"ESubtitleName",ESubtitleName,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( ESubtitleName );
  gtk_box_pack_start( GTK_BOX( hbox4 ),ESubtitleName,TRUE,TRUE,0 );

  hbuttonbox4=AddHButtonBox( hbuttonbox4 );
    gtk_container_set_border_width( GTK_CONTAINER( hbuttonbox4 ),3 );
  BLoadSubtitle=AddButton( MSGTR_Browse,hbuttonbox4 );
#endif

  vbox8=AddVBox( vbox7 );

  table1=gtk_table_new( 3,2,FALSE );
  gtk_widget_set_name( table1,"table1" );
  gtk_widget_ref( table1 );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"table1",table1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( table1 );
  gtk_box_pack_start( GTK_BOX( vbox8 ),table1,FALSE,FALSE,0 );

  label=AddLabel( MSGTR_PREFERENCES_SUB_Delay,NULL );
    gtk_table_attach( GTK_TABLE( table1 ),label,0,1,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  label=AddLabel( MSGTR_PREFERENCES_SUB_FPS,NULL );
    gtk_table_attach( GTK_TABLE( table1 ),label,0,1,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  HSSubDelayadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,-10.0,10,0.01,0,0 ) );
  HSSubDelay=AddHScaler( HSSubDelayadj,NULL,1 );
    gtk_table_attach( GTK_TABLE( table1 ),HSSubDelay,1,2,0,1,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  label=AddLabel( MSGTR_PREFERENCES_SUB_POS,NULL );
    gtk_table_attach( GTK_TABLE( table1 ),label,0,1,2,3,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );    

  HSSubPositionadj=GTK_ADJUSTMENT( gtk_adjustment_new( 100,0,100,1,0,0 ) );
  HSSubPosition=AddHScaler( HSSubPositionadj,NULL,0 );
    gtk_table_attach( GTK_TABLE( table1 ),HSSubPosition,1,2,2,3,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  HSSubFPSadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,0,100,0.01,0,0 ) );
  HSSubFPS=AddHScaler( HSSubFPSadj,NULL,1 );
    gtk_table_attach( GTK_TABLE( table1 ),HSSubFPS,1,2,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  vbox9=AddVBox( vbox8 );

  CBNoAutoSub=AddCheckButton( MSGTR_PREFERENCES_SUB_AutoLoad,vbox9 );
  CBSubUnicode=AddCheckButton( MSGTR_PREFERENCES_SUB_Unicode,vbox9 );
  CBDumpMPSub=AddCheckButton( MSGTR_PREFERENCES_SUB_MPSUB,vbox9 );
  CBDumpSrt=AddCheckButton( MSGTR_PREFERENCES_SUB_SRT,vbox9 );

  frame=AddFrame( MSGTR_PREFERENCES_FRAME_Font,GTK_SHADOW_ETCHED_OUT,vbox6,1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox603=AddVBox( frame );

  hbox6=AddHBox( vbox603,1 );

  AddLabel( MSGTR_PREFERENCES_Font,hbox6 );

  prEFontName=gtk_entry_new();
  gtk_widget_set_name( prEFontName,"prEFontName" );
  gtk_widget_ref( prEFontName );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"prEFontName",prEFontName,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( prEFontName );
  gtk_box_pack_start( GTK_BOX( hbox6 ),prEFontName,TRUE,TRUE,0 );

  hbuttonbox5=AddHButtonBox( hbox6 );
    gtk_container_set_border_width( GTK_CONTAINER( hbuttonbox5 ),3 );
  BLoadFont=AddButton( MSGTR_Browse,hbuttonbox5 );

#ifndef HAVE_FREETYPE
  hbox7=AddHBox( vbox603,1 );
  AddLabel( MSGTR_PREFERENCES_FontFactor,hbox7 );
  HSFontFactoradj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,0,10,0.05,0,0 ) );
  HSFontFactor=AddHScaler( HSFontFactoradj,hbox7,2 );
#else

  RBFontNoAutoScale=AddRadioButton( MSGTR_PREFERENCES_FontNoAutoScale,&Font_group,vbox603 );
  BRFontAutoScaleWidth=AddRadioButton( MSGTR_PREFERENCES_FontPropWidth,&Font_group,vbox603 );
  RBFontAutoScaleHeight=AddRadioButton( MSGTR_PREFERENCES_FontPropHeight,&Font_group,vbox603 );
  RBFontAutoScaleDiagonal=AddRadioButton( MSGTR_PREFERENCES_FontPropDiagonal,&Font_group,vbox603 );

  table1=gtk_table_new( 3,2,FALSE );
  gtk_widget_set_name( table1,"table1" );
  gtk_widget_ref( table1 );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"table1",table1,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( table1 );
  gtk_box_pack_start( GTK_BOX( vbox603 ),table1,FALSE,FALSE,0 );

  label=AddLabel( MSGTR_PREFERENCES_FontEncoding,NULL );
    gtk_table_attach( GTK_TABLE( table1 ),label,0,1,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  
  CBFontEncoding=gtk_combo_new();
  gtk_widget_set_name( CBFontEncoding,"CBFontEncoding" );
  gtk_widget_ref( CBFontEncoding );
  gtk_widget_show( CBFontEncoding );
  gtk_table_attach( GTK_TABLE( table1 ),CBFontEncoding,1,2,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  {
   int i;
   for ( i=0;lEncoding[i].name;i++ ) CBFontEncoding_items=g_list_append( CBFontEncoding_items,lEncoding[i].comment );
  }
  gtk_combo_set_popdown_strings( GTK_COMBO( CBFontEncoding ),CBFontEncoding_items );
  g_list_free( CBFontEncoding_items );

  EFontEncoding=GTK_COMBO( CBFontEncoding )->entry;
  gtk_widget_set_name( EFontEncoding,"EFontEncoding" );
  gtk_entry_set_editable( GTK_ENTRY( EFontEncoding ),FALSE );
  gtk_widget_ref( EFontEncoding );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"EFontEncoding",EFontEncoding,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( EFontEncoding );

  label=AddLabel( MSGTR_PREFERENCES_FontBlur,NULL );
    gtk_table_attach( GTK_TABLE( table1 ),label,0,1,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  HSFontBluradj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,0,100,0.1,0,0 ) );
  HSFontBlur=AddHScaler( HSFontBluradj,NULL,2 );
    gtk_table_attach( GTK_TABLE( table1 ),HSFontBlur,1,2,1,2,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  label=AddLabel( MSGTR_PREFERENCES_FontOutLine,NULL );
    gtk_table_attach( GTK_TABLE( table1 ),label,0,1,2,3,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  HSFontOutLineadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,0,100,0.1,0,0 ) );
  HSFontOutLine=AddHScaler( HSFontOutLineadj,NULL,2 );
    gtk_table_attach( GTK_TABLE( table1 ),HSFontOutLine,1,2,2,3,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  label=AddLabel( MSGTR_PREFERENCES_FontTextScale,NULL );
    gtk_table_attach( GTK_TABLE( table1 ),label,0,1,3,4,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  HSFontTextScaleadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,0,100,0.1,0,0 ) );
  HSFontTextScale=AddHScaler( HSFontTextScaleadj,NULL,2 );
    gtk_table_attach( GTK_TABLE( table1 ),HSFontTextScale,1,2,3,4,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  label=AddLabel( MSGTR_PREFERENCES_FontOSDScale,NULL );
    gtk_table_attach( GTK_TABLE( table1 ),label,0,1,4,5,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  HSFontOSDScaleadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,0,100,0.1,0,0 ) );
  HSFontOSDScale=AddHScaler( HSFontOSDScaleadj,NULL,2 );
    gtk_table_attach( GTK_TABLE( table1 ),HSFontOSDScale,1,2,4,5,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
#endif

  label=AddLabel( MSGTR_PREFERENCES_SubtitleOSD,NULL );
    gtk_notebook_set_tab_label( GTK_NOTEBOOK( notebook1 ),gtk_notebook_get_nth_page( GTK_NOTEBOOK( notebook1 ),2 ),label );
  vbox601=AddVBox( notebook1 );

  frame=AddFrame( MSGTR_PREFERENCES_FRAME_PostProcess,GTK_SHADOW_ETCHED_OUT,vbox601,0 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox602=AddVBox( frame );

  CBPostprocess=AddCheckButton( MSGTR_PREFERENCES_PostProcess,vbox602 );

  hbox5=AddHBox( vbox602,1 );

  AddLabel( MSGTR_PREFERENCES_AutoQuality,hbox5 );

  if ( guiIntfStruct.sh_video && guiIntfStruct.Playing ) HSPPQualityadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,0,get_video_quality_max( guiIntfStruct.sh_video ),0,0,0 ) );
   else HSPPQualityadj=GTK_ADJUSTMENT( gtk_adjustment_new( 0,0,100,0,0,0 ) );
  HSPPQuality=AddHScaler( HSPPQualityadj,hbox5,0 );

  frame=AddFrame( MSGTR_PREFERENCES_FRAME_CodecDemuxer,GTK_SHADOW_ETCHED_OUT,vbox601,0 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox602=AddVBox( frame );

  CBNonInterlaved=AddCheckButton( MSGTR_PREFERENCES_NI,vbox602 );
  CBIndex=AddCheckButton( MSGTR_PREFERENCES_IDX,vbox602 );

  hbox5=AddHBox( vbox602,1 );

  AddLabel( MSGTR_PREFERENCES_VideoCodecFamily,hbox5 );

  CBVFM=gtk_combo_new();
  gtk_widget_set_name( CBVFM,"CBVFM" );
  gtk_widget_ref( CBVFM );
  gtk_widget_show( CBVFM );
  gtk_box_pack_start( GTK_BOX( hbox5 ),CBVFM,TRUE,TRUE,0 );

  EVFM=GTK_COMBO( CBVFM )->entry;
  gtk_widget_set_name( EVFM,"CEVFM" );
  gtk_entry_set_editable( GTK_ENTRY( EVFM ),FALSE );
  gtk_widget_ref( EVFM );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"EVFM",EVFM,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( EVFM );

  hbox5=AddHBox( vbox602,1 );

  AddLabel( MSGTR_PREFERENCES_AudioCodecFamily,hbox5 );

  CBAFM=gtk_combo_new();
  gtk_widget_set_name( CBAFM,"CBAFM" );
  gtk_widget_ref( CBAFM );
  gtk_widget_show( CBAFM );
  gtk_box_pack_start( GTK_BOX( hbox5 ),CBAFM,TRUE,TRUE,0 );

  EAFM=GTK_COMBO( CBAFM )->entry;
  gtk_widget_set_name( EAFM,"EAFM" );
  gtk_entry_set_editable( GTK_ENTRY( EAFM ),FALSE );
  gtk_widget_ref( EAFM );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"EAFM",EAFM,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( EAFM );

  frame=AddFrame( MSGTR_PREFERENCES_FRAME_Cache,GTK_SHADOW_ETCHED_OUT,vbox601,0 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox602=AddVBox( frame );

  CBCache=AddCheckButton( MSGTR_PREFERENCES_Cache,vbox602 );
  
  hbox5=AddHBox( vbox602,1 );

  AddLabel( MSGTR_PREFERENCES_CacheSize,hbox5 );

  SBCacheadj=GTK_ADJUSTMENT( gtk_adjustment_new( 2048,4,65535,1,10,10 ) );
  SBCache=gtk_spin_button_new( GTK_ADJUSTMENT( SBCacheadj ),1,0 );
  gtk_widget_ref( SBCache );
  gtk_object_set_data_full( GTK_OBJECT( Preferences ),"SBCache",SBCache,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( SBCache );
  gtk_box_pack_start( GTK_BOX( hbox5 ),SBCache,TRUE,TRUE,0 );

  frame=AddFrame( MSGTR_PREFERENCES_FRAME_Misc,GTK_SHADOW_ETCHED_OUT,vbox601,0 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  vbox602=AddVBox( frame );

  CBLoadFullscreen=AddCheckButton( MSGTR_PREFERENCES_LoadFullscreen,vbox602 );

  label=AddLabel( MSGTR_PREFERENCES_Misc,NULL );
    gtk_notebook_set_tab_label( GTK_NOTEBOOK( notebook1 ),gtk_notebook_get_nth_page( GTK_NOTEBOOK( notebook1 ),3 ),label );

  AddHSeparator( vbox1 );

  hbuttonbox1=AddHButtonBox( vbox1 );
    gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
    gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );
  BOk=AddButton( MSGTR_Ok,hbuttonbox1 );
  BCancel=AddButton( MSGTR_Cancel,hbuttonbox1 );
  
  gtk_widget_add_accelerator( BOk,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
  gtk_widget_add_accelerator( BCancel,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

  gtk_signal_connect( GTK_OBJECT( Preferences ),"destroy",GTK_SIGNAL_FUNC( prDestroy ),NULL );
  gtk_signal_connect( GTK_OBJECT( Preferences ),"show",GTK_SIGNAL_FUNC( prShow ),(void*)1 );
  gtk_signal_connect( GTK_OBJECT( Preferences ),"hide",GTK_SIGNAL_FUNC( prShow ),(void*)0 );
  
  gtk_signal_connect( GTK_OBJECT( AConfig ),"released",GTK_SIGNAL_FUNC( prButton ),(void*)bAConfig );
  gtk_signal_connect( GTK_OBJECT( BOk ),"released",GTK_SIGNAL_FUNC( prButton ),(void*)bOk );
  gtk_signal_connect( GTK_OBJECT( BCancel ),"released",GTK_SIGNAL_FUNC( prButton ),(void*)bCancel );
  gtk_signal_connect( GTK_OBJECT( VConfig ),"released",GTK_SIGNAL_FUNC( prButton ),(void*)bVconfig );
#if 0
  gtk_signal_connect( GTK_OBJECT( BLoadSubtitle ),"released",GTK_SIGNAL_FUNC( prButton ),(void*)bLSubtitle );
#endif
  gtk_signal_connect( GTK_OBJECT( BLoadFont ),"released",GTK_SIGNAL_FUNC( prButton ),(void*)bLFont );

#if 0
  gtk_signal_connect( GTK_OBJECT( CBNoSound ),"toggled",GTK_SIGNAL_FUNC( on_CBNoSound_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBNormalize ),"toggled",GTK_SIGNAL_FUNC( on_CBNormalize_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBSurround ),"toggled",GTK_SIGNAL_FUNC( on_CBSurround_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBExtraStereo ),"toggled",GTK_SIGNAL_FUNC( on_CBExtraStereo_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBDoubleBuffer ),"toggled",GTK_SIGNAL_FUNC( on_CBDoubleBuffer_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBDR ),"toggled",GTK_SIGNAL_FUNC( on_CBDR_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBFramedrop ),"toggled",GTK_SIGNAL_FUNC( on_CBFramedrop_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBHFramedrop ),"toggled",GTK_SIGNAL_FUNC( on_CBHFramedrop_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBFullScreen ),"toggled",GTK_SIGNAL_FUNC( on_CBFullScreen_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBNonInterlaved ),"toggled",GTK_SIGNAL_FUNC( on_CBNonInterlaved_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBFlip ),"toggled",GTK_SIGNAL_FUNC( on_CBFlip_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBPostprocess ),"toggled",GTK_SIGNAL_FUNC( on_CBPostprocess_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBNoAutoSub ),"toggled",GTK_SIGNAL_FUNC( on_CBNoAutoSub_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBSubUnicode ),"toggled",GTK_SIGNAL_FUNC( on_CNSubUnicode_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBDumpMPSub ),"toggled",GTK_SIGNAL_FUNC( on_CBDumpMPSub_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBDumpSrt ),"toggled",GTK_SIGNAL_FUNC( on_CBDumpSrt_toggled ),NULL );
#endif
#if 0
  gtk_signal_connect( GTK_OBJECT( RBOSDNone ),"toggled",GTK_SIGNAL_FUNC( on_RBOSDNone_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( RBOSDTandP ),"toggled",GTK_SIGNAL_FUNC( on_RBOSDTandP_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( RBOSDIndicator ),"toggled",GTK_SIGNAL_FUNC( on_RBOSDIndicator_toggled ),NULL );
  gtk_signal_connect( GTK_OBJECT( CBAudioEqualizer ),"toggled",GTK_SIGNAL_FUNC( on_CBAudioEqualizer_toggled ),NULL );
#endif
#if 0
  gtk_signal_connect( GTK_OBJECT( HSAudioDelay ),"motion_notify_event",GTK_SIGNAL_FUNC( on_HSAudioDelay_motion_notify_event ),NULL );
  gtk_signal_connect( GTK_OBJECT( HSPanscan ),"motion_notify_event",GTK_SIGNAL_FUNC( on_HSPanscan_motion_notify_event ),NULL );
  gtk_signal_connect( GTK_OBJECT( label2 ),"motion_notify_event",GTK_SIGNAL_FUNC( on_label2_motion_notify_event ),NULL );
  gtk_signal_connect( GTK_OBJECT( HSSubDelay ),"motion_notify_event",GTK_SIGNAL_FUNC( on_HSSubDelay_motion_notify_event ),NULL );
  gtk_signal_connect( GTK_OBJECT( HSSubPosition ),"motion_notify_event",GTK_SIGNAL_FUNC( on_HSSubPosition_motion_notify_event ),NULL );
  gtk_signal_connect( GTK_OBJECT( HSSubFPS ),"motion_notify_event",GTK_SIGNAL_FUNC( on_HSSubFPS_motion_notify_event ),NULL );
  gtk_signal_connect( GTK_OBJECT( HSFontFactor ),"motion_notify_event",GTK_SIGNAL_FUNC( on_HSFontFactor_motion_notify_event ),NULL );
  gtk_signal_connect( GTK_OBJECT( HSPPQuality ),"motion_notify_event",GTK_SIGNAL_FUNC( on_HSPPQuality_motion_notify_event ),NULL );
#endif

  gtk_notebook_set_page( GTK_NOTEBOOK( notebook1 ),2 );

  gtk_window_add_accel_group( GTK_WINDOW( Preferences ),accel_group );

  return Preferences;
}

#ifdef USE_OSS_AUDIO
       GtkWidget * OSSConfig;
static GtkWidget * CEOssDevice;
static GtkWidget * CEOssMixer;
static GtkWidget * CBOssMixer;
static GtkWidget * CBOssDevice;
static GtkWidget * BOssOk;
static GtkWidget * BOssCancel;

       int         gtkVOSSConfig = 0;

void ShowOSSConfig( void )
{
 if ( gtkVOSSConfig ) gtkActive( OSSConfig );
   else OSSConfig=create_OSSConfig();

 gtk_entry_set_text( GTK_ENTRY( CEOssMixer ),gtkAOOSSMixer );
 gtk_entry_set_text( GTK_ENTRY( CEOssDevice ),gtkAOOSSDevice );

 gtk_widget_show( OSSConfig );
 gtkSetLayer( OSSConfig );
 gtkVOSSConfig=1;
}

void HideOSSConfig( void )
{
 if ( !gtkVOSSConfig ) return;
 gtk_widget_hide( OSSConfig );
 gtk_widget_destroy( OSSConfig ); 
 gtkVOSSConfig=0;
}

static void ossDestroy( GtkObject * object,gpointer user_data )
{ HideOSSConfig(); }

static void ossShow( GtkWidget * widget,gpointer user_data )
{ gtkVOSSConfig=(int)user_data; }

static void ossButton( GtkButton * button,gpointer user_data )
{
 switch( (int)user_data )
  {
   case 1:
        gfree( (void **)&gtkAOOSSMixer );  gtkAOOSSMixer=strdup( gtk_entry_get_text( GTK_ENTRY( CEOssMixer ) ) );
        gfree( (void **)&gtkAOOSSDevice ); gtkAOOSSDevice=strdup( gtk_entry_get_text( GTK_ENTRY( CEOssDevice ) ) );
   case 0:
	HideOSSConfig();
	break;
  }
}

GtkWidget * create_OSSConfig( void )
{
  GList 	* CBOssDevice_items=NULL;
  GList 	* CBOssMixer_items=NULL;
  GtkWidget * frame;
  GtkWidget * vbox604;
  GtkWidget * table2;
  GtkWidget * label;
  GtkWidget * hbuttonbox6;
  GtkAccelGroup * accel_group;

  accel_group=gtk_accel_group_new();

  OSSConfig=gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_widget_set_name( OSSConfig,"OSSConfig" );
  gtk_object_set_data( GTK_OBJECT( OSSConfig ),"OSSConfig",OSSConfig );
  gtk_widget_set_usize( OSSConfig,270,92 );
  gtk_window_set_title( GTK_WINDOW( OSSConfig ),MSGTR_OSSPreferences );
  gtk_window_set_position( GTK_WINDOW( OSSConfig ),GTK_WIN_POS_CENTER );
  gtk_window_set_policy( GTK_WINDOW( OSSConfig ),FALSE,FALSE,FALSE );
  gtk_window_set_wmclass( GTK_WINDOW( OSSConfig ),"OSS Config","MPlayer" );

  gtk_widget_realize( OSSConfig );
  gtkAddIcon( OSSConfig );

  frame=AddFrame( NULL,GTK_SHADOW_IN,OSSConfig,1 );
  gtk_container_set_border_width( GTK_CONTAINER( frame ),1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
  frame=AddFrame( NULL,GTK_SHADOW_ETCHED_OUT,frame,1 );
  frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );

  vbox604=AddVBox( frame );

  table2=gtk_table_new( 2,2,FALSE );
  gtk_widget_set_name( table2,"table2" );
  gtk_widget_ref( table2 );
  gtk_object_set_data_full( GTK_OBJECT( OSSConfig ),"table2",table2,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( table2 );
  gtk_box_pack_start( GTK_BOX( vbox604 ),table2,TRUE,TRUE,0 );

  label=AddLabel( MSGTR_PREFERENCES_OSS_Device,NULL );
    gtk_table_attach( GTK_TABLE( table2 ),label,0,1,0,1,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  label=AddLabel( MSGTR_PREFERENCES_OSS_Mixer,NULL );
    gtk_table_attach( GTK_TABLE( table2 ),label,0,1,1,2,(GtkAttachOptions)( GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );

  CBOssDevice=gtk_combo_new();
  gtk_widget_set_name( CBOssDevice,"CBOssDevice" );
  gtk_widget_ref( CBOssDevice );
  gtk_object_set_data_full( GTK_OBJECT( OSSConfig ),"CBOssDevice",CBOssDevice,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CBOssDevice );
  gtk_table_attach( GTK_TABLE( table2 ),CBOssDevice,1,2,0,1,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/dsp" );
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/dsp0" );
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/dsp1" );
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/dsp2" );
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/dsp3" );
#ifdef HAVE_DXR3
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/em8300_ma" );
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/em8300_ma-0" );
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/em8300_ma-1" );
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/em8300_ma-2" );
  CBOssDevice_items=g_list_append( CBOssDevice_items,(gpointer)"/dev/em8300_ma-3" );
#endif
  gtk_combo_set_popdown_strings( GTK_COMBO( CBOssDevice ),CBOssDevice_items );
  g_list_free( CBOssDevice_items );

  CEOssDevice=GTK_COMBO( CBOssDevice )->entry;
  gtk_widget_set_name( CEOssDevice,"CEOssDevice" );
  gtk_widget_ref( CEOssDevice );
  gtk_object_set_data_full( GTK_OBJECT( OSSConfig ),"CEOssDevice",CEOssDevice,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CEOssDevice );

  CBOssMixer=gtk_combo_new();
  gtk_widget_set_name( CBOssMixer,"CBOssMixer" );
  gtk_widget_ref( CBOssMixer );
  gtk_object_set_data_full( GTK_OBJECT( OSSConfig ),"CBOssMixer",CBOssMixer,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CBOssMixer );
  gtk_table_attach( GTK_TABLE( table2 ),CBOssMixer,1,2,1,2,(GtkAttachOptions)( GTK_EXPAND | GTK_FILL ),(GtkAttachOptions)( 0 ),0,0 );
  CBOssMixer_items=g_list_append( CBOssMixer_items,(gpointer)"/dev/mixer" );
  CBOssMixer_items=g_list_append( CBOssMixer_items,(gpointer)"/dev/mixer0" );
  CBOssMixer_items=g_list_append( CBOssMixer_items,(gpointer)"/dev/mixer1" );
  CBOssMixer_items=g_list_append( CBOssMixer_items,(gpointer)"/dev/mixer2" );
  CBOssMixer_items=g_list_append( CBOssMixer_items,(gpointer)"/dev/mixer3" );
  gtk_combo_set_popdown_strings( GTK_COMBO( CBOssMixer ),CBOssMixer_items );
  g_list_free( CBOssMixer_items );

  CEOssMixer=GTK_COMBO( CBOssMixer )->entry;
  gtk_widget_set_name( CEOssMixer,"CEOssMixer" );
  gtk_widget_ref( CEOssMixer );
  gtk_object_set_data_full( GTK_OBJECT( OSSConfig ),"CEOssMixer",CEOssMixer,(GtkDestroyNotify)gtk_widget_unref );
  gtk_widget_show( CEOssMixer );

  AddHSeparator( vbox604 );

  hbuttonbox6=AddHButtonBox( vbox604 );
    gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox6 ),GTK_BUTTONBOX_END );
    gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox6 ),10 );
  BOssOk=AddButton( MSGTR_Ok,hbuttonbox6 );
  BOssCancel=AddButton( MSGTR_Cancel,hbuttonbox6 );

  gtk_signal_connect( GTK_OBJECT( OSSConfig ),"destroy",GTK_SIGNAL_FUNC( ossDestroy ),NULL );
  gtk_signal_connect( GTK_OBJECT( OSSConfig ),"hide",GTK_SIGNAL_FUNC( ossShow ),(void*)0 );
  gtk_signal_connect( GTK_OBJECT( OSSConfig ),"show",GTK_SIGNAL_FUNC( ossShow ),(void*)1 );
  
  gtk_signal_connect( GTK_OBJECT( BOssOk ),"released",GTK_SIGNAL_FUNC( ossButton ),(void*)1 );
  gtk_signal_connect( GTK_OBJECT( BOssCancel ),"released",GTK_SIGNAL_FUNC( ossButton ),(void*)0 );

  gtk_widget_add_accelerator( BOssOk,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
  gtk_widget_add_accelerator( BOssCancel,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

  gtk_window_add_accel_group( GTK_WINDOW( OSSConfig ),accel_group );

  return OSSConfig;
}

#endif

#ifdef HAVE_DXR3
// --- dxr3 config box

static GtkWidget * DXR3Config;
static GtkWidget * CBDevice;
static GtkWidget * CEDXR3Device;
static GtkWidget * RBVNone;
#ifdef USE_LIBAVCODEC
 static GtkWidget * RBVLavc;
#endif
#ifdef USE_LIBFAME
 static GtkWidget * RBVFame;
#endif
static GtkWidget * dxr3BOk;
static GtkWidget * dxr3BCancel;

static int gtkVDXR3Config = 0;

GtkWidget * create_DXR3Config( void );

void ShowDXR3Config( void )
{
 if ( gtkVDXR3Config ) gtkActive( DXR3Config );
  else DXR3Config=create_DXR3Config();

 gtk_entry_set_text( GTK_ENTRY( CEDXR3Device ),gtkDXR3Device );

 gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RBVNone ),TRUE );
#ifdef USE_LIBAVCODEC
 if ( gtkVopLAVC ) gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RBVLavc ),TRUE );
#endif
#ifdef USE_LIBFAME
 if ( gtkVopFAME ) gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( RBVFame ),TRUE );
#endif
 
 gtk_widget_show( DXR3Config );
 gtkSetLayer( DXR3Config );
 gtkVDXR3Config=1;
}

void HideDXR3Config( void )
{
 if ( !gtkVDXR3Config ) return;
 gtk_widget_hide( DXR3Config );
 gtk_widget_destroy( DXR3Config );
 gtkVDXR3Config=0;
}

static void dxr3Button( GtkButton * button,gpointer user_data )
{
 switch ( (int)user_data )
 {
  case 0: // Ok
       gfree( (void **)&gtkDXR3Device ); gtkDXR3Device=strdup( gtk_entry_get_text( GTK_ENTRY( CEDXR3Device ) ) );
#ifdef USE_LIBAVCODEC
       gtkVopLAVC=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( RBVLavc ) );
#endif
#ifdef USE_LIBFAME
       gtkVopFAME=gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( RBVFame ) );
#endif
  case 2: // Destroy
  case 1: // Cancel
       HideDXR3Config();
       break;
  case 3: // Show
       gtkVDXR3Config=1;
       break;
  case 4: // Hide
       gtkVDXR3Config=0;
       break;
 }
}

GtkWidget * create_DXR3Config( void )
{
 GtkWidget * frame;
 GtkWidget * vbox1;
 GtkWidget * vbox2;
 GtkWidget * hbox1;
 GList     * CBDevice_items = NULL;
 GtkWidget * vbox3;
 GSList    * VEncoder_group = NULL;
 GtkWidget * hbuttonbox1;
 GtkAccelGroup * accel_group;

 accel_group=gtk_accel_group_new();

 DXR3Config=gtk_window_new( GTK_WINDOW_TOPLEVEL );
 gtk_widget_set_name( DXR3Config,"DXR3Config" );
 gtk_object_set_data( GTK_OBJECT( DXR3Config ),"DXR3Config",DXR3Config );
// gtk_widget_set_usize( DXR3Config,300,156 );
 GTK_WIDGET_SET_FLAGS( DXR3Config,GTK_CAN_DEFAULT );
 gtk_window_set_title( GTK_WINDOW( DXR3Config ),"DXR3/H+" );
 gtk_window_set_position( GTK_WINDOW( DXR3Config ),GTK_WIN_POS_CENTER );
 gtk_window_set_modal( GTK_WINDOW( DXR3Config ),TRUE );
 gtk_window_set_policy( GTK_WINDOW( DXR3Config ),FALSE,FALSE,FALSE );
 gtk_window_set_wmclass( GTK_WINDOW( DXR3Config ),"DXR3","MPlayer" );

 gtk_widget_realize( DXR3Config );
 gtkAddIcon( DXR3Config );

 frame=AddFrame( NULL,GTK_SHADOW_IN,DXR3Config,1 );
 gtk_container_set_border_width( GTK_CONTAINER( frame ),1 );
 frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );
 frame=AddFrame( NULL,GTK_SHADOW_ETCHED_OUT,frame,1 );
 frame=AddFrame( NULL,GTK_SHADOW_NONE,frame,1 );

 vbox1=AddVBox( frame );
 vbox2=AddVBox( vbox1 );
 hbox1=AddHBox( vbox2,1 );
 AddLabel( MSGTR_PREFERENCES_OSS_Device,hbox1 );

 CBDevice=gtk_combo_new();
 gtk_widget_set_name( CBDevice,"CBDevice" );
 gtk_widget_ref( CBDevice );
 gtk_object_set_data_full( GTK_OBJECT( DXR3Config ),"CBDevice",CBDevice,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( CBDevice );
 gtk_box_pack_start( GTK_BOX( hbox1 ),CBDevice,TRUE,TRUE,0 );
 CBDevice_items=g_list_append( CBDevice_items,( gpointer ) "/dev/em8300" );
 CBDevice_items=g_list_append( CBDevice_items,( gpointer ) "/dev/em8300-0" );
 CBDevice_items=g_list_append( CBDevice_items,( gpointer ) "/dev/em8300-1" );
 CBDevice_items=g_list_append( CBDevice_items,( gpointer ) "/dev/em8300-2" );
 CBDevice_items=g_list_append( CBDevice_items,( gpointer ) "/dev/em8300-3" );
 gtk_combo_set_popdown_strings( GTK_COMBO( CBDevice ),CBDevice_items );
 g_list_free( CBDevice_items );

 CEDXR3Device=GTK_COMBO( CBDevice )->entry;
 gtk_widget_set_name( CEDXR3Device,"CEDXR3Device" );
 gtk_widget_ref( CEDXR3Device );
 gtk_object_set_data_full( GTK_OBJECT( DXR3Config ),"CEDXR3Device",CEDXR3Device,(GtkDestroyNotify)gtk_widget_unref );
 gtk_widget_show( CEDXR3Device );
 gtk_entry_set_text( GTK_ENTRY( CEDXR3Device ),"/dev/em8300" );

#if defined( USE_LIBAVCODEC ) || defined( USE_LIBFAME )
 AddHSeparator( vbox2 );
 vbox3=AddVBox( vbox2 );
 AddLabel( MSGTR_PREFERENCES_DXR3_VENC,vbox3 );
 RBVNone=AddRadioButton( MSGTR_PREFERENCES_None,&VEncoder_group,vbox3 );
#ifdef USE_LIBAVCODEC
 RBVLavc=AddRadioButton( MSGTR_PREFERENCES_DXR3_LAVC,&VEncoder_group,vbox3 );
#endif
#ifdef USE_LIBFAME
 RBVFame=AddRadioButton( MSGTR_PREFERENCES_DXR3_FAME,&VEncoder_group,vbox3 );
#endif

#endif

 AddHSeparator( vbox1 );

 hbuttonbox1=AddHButtonBox( vbox1 );
   gtk_button_box_set_layout( GTK_BUTTON_BOX( hbuttonbox1 ),GTK_BUTTONBOX_END );
   gtk_button_box_set_spacing( GTK_BUTTON_BOX( hbuttonbox1 ),10 );
 dxr3BOk=AddButton( MSGTR_Ok,hbuttonbox1 );
 dxr3BCancel=AddButton( MSGTR_Cancel,hbuttonbox1 );

 gtk_widget_add_accelerator( dxr3BOk,"released",accel_group,GDK_Return,0,GTK_ACCEL_VISIBLE );
 gtk_widget_add_accelerator( dxr3BCancel,"released",accel_group,GDK_Escape,0,GTK_ACCEL_VISIBLE );

 gtk_signal_connect( GTK_OBJECT( DXR3Config ),"destroy",GTK_SIGNAL_FUNC( dxr3Button ),(void *)2 );
 gtk_signal_connect( GTK_OBJECT( DXR3Config ),"show",GTK_SIGNAL_FUNC( dxr3Button ),(void *)3 );
 gtk_signal_connect( GTK_OBJECT( DXR3Config ),"hide",GTK_SIGNAL_FUNC( dxr3Button ),(void *)4 );
 
 gtk_signal_connect( GTK_OBJECT( dxr3BOk ),"released",GTK_SIGNAL_FUNC( dxr3Button ),(void *)0 );
 gtk_signal_connect( GTK_OBJECT( dxr3BCancel ),"released",GTK_SIGNAL_FUNC( dxr3Button ),(void *)1 );

 gtk_window_add_accel_group( GTK_WINDOW( DXR3Config ),accel_group );

 return DXR3Config;
}

#endif
