
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cut.h"
#include "font.h"
#include "app.h"

#include "../config.h"
#include "../mp_msg.h"
#include "../help_mp.h"
#include "mplayer/widgets.h"

//#define MSGL_DBG2 MSGL_STATUS

listItems     * skinAppMPlayer = &appMPlayer;

// ---

static int             linenumber;

static unsigned char   path[512],fn[512];

static listItems     * defList = NULL;
static unsigned char   window_name[32] = "";

static wItem         * currSection = NULL;
static int           * currSubItem = NULL;
static wItem         * currSubItems = NULL;

#include <stdarg.h>

void ERRORMESSAGE( const char * format, ... )
{
 char      p[512];
 char      tmp[512];
 va_list   ap;
 va_start( ap,format );
 vsnprintf( p,512,format,ap );
 va_end( ap );
 mp_msg( MSGT_GPLAYER,MSGL_STATUS,MSGTR_SKIN_ERRORMESSAGE,linenumber,p );
 snprintf( tmp,512,MSGTR_SKIN_ERRORMESSAGE,linenumber,p );
 gtkMessageBox( GTK_MB_FATAL,tmp );
}

#define CHECKDEFLIST( str ) \
{ \
 if ( defList == NULL ) \
  { \
   mp_msg( MSGT_GPLAYER,MSGL_STATUS,MSGTR_SKIN_WARNING1,linenumber,str ); \
   return 1; \
  } \
}

#define CHECKWINLIST( str ) \
{ \
 if ( !window_name[0] ) \
  { \
   mp_msg( MSGT_GPLAYER,MSGL_STATUS,MSGTR_SKIN_WARNING2,linenumber,str ); \
   return 1; \
  } \
}

#define CHECK( name ) \
{ \
 if ( !strcmp( window_name,name ) ) \
  { \
   mp_msg( MSGT_GPLAYER,MSGL_STATUS,MSGTR_SKIN_WARNING3,linenumber,name ); \
   return 1; \
  } \
}

static char * strlower( char * in )
{
 int i;
 for( i=0;i<(int)strlen( in );i++ ) in[i]=( in[i] >= 'A' ? ( in[i] <= 'Z' ?  in[i]+='A' : in[i] ) : in[i] );
 return in;
}

int skinBPRead( char * fname, txSample * bf )
{
 int i=bpRead( fname,bf );
 switch ( i )
  {
   case -1: ERRORMESSAGE( MSGTR_SKIN_BITMAP_16bit,fname ); break;
   case -2: ERRORMESSAGE( MSGTR_SKIN_BITMAP_FileNotFound,fname ); break;
   case -3: ERRORMESSAGE( MSGTR_SKIN_BITMAP_BMPReadError,fname ); break;
   case -4: ERRORMESSAGE( MSGTR_SKIN_BITMAP_TGAReadError,fname ); break;
   case -5: ERRORMESSAGE( MSGTR_SKIN_BITMAP_PNGReadError,fname ); break;
   case -6: ERRORMESSAGE( MSGTR_SKIN_BITMAP_RLENotSupported,fname ); break;
   case -7: ERRORMESSAGE( MSGTR_SKIN_BITMAP_UnknownFileType,fname ); break;
   case -8: ERRORMESSAGE( MSGTR_SKIN_BITMAP_ConversionError,fname ); break;
  }
 return i;
}

int cmd_section( char * in )
{
 strlower( in );
 defList=NULL;
 if ( !strcmp( in,"movieplayer" ) ) defList=skinAppMPlayer;
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] sectionname: %s\n",in );
 return 0;
}

int cmd_end( char * in )
{
 if ( strlen( window_name ) ) { window_name[0]=0; currSection=NULL; currSubItem=NULL; currSubItems=NULL; }
  else defList=NULL;
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] end section\n" );
 return 0;
}

int cmd_window( char * in )
{
 CHECKDEFLIST( "window" );

 strlcpy( window_name,strlower( in ),sizeof( window_name ) );
 if ( !strncmp( in,"main",4 ) ) { currSection=&skinAppMPlayer->main; currSubItem=&skinAppMPlayer->NumberOfItems; currSubItems=skinAppMPlayer->Items; }
  else if ( !strncmp( in,"sub",3 ) ) currSection=&skinAppMPlayer->sub;
   else if ( !strncmp( in,"playbar",7 ) ) { currSection=&skinAppMPlayer->bar; currSubItem=&skinAppMPlayer->NumberOfBarItems; currSubItems=skinAppMPlayer->barItems; }
    else if ( !strncmp( in,"menu",4 ) ) { currSection=&skinAppMPlayer->menuBase; currSubItem=&skinAppMPlayer->NumberOfMenuItems; currSubItems=skinAppMPlayer->MenuItems; }
     else ERRORMESSAGE( MSGTR_UNKNOWNWINDOWTYPE );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] window: %s\n",window_name );
 return 0;
}

int cmd_base( char * in )
{
 unsigned char fname[512];
 unsigned char tmp[512];
 int           x,y;
 int           sx=0,sy=0;

 CHECKDEFLIST( "base" );
 CHECKWINLIST( "base" );

 cutItem( in,fname,',',0 );
 x=cutItemToInt( in,',',1 );
 y=cutItemToInt( in,',',2 );
 sx=cutItemToInt( in,',',3 );
 sy=cutItemToInt( in,',',4 );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] base: %s x: %d y: %d ( %dx%d )\n",fname,x,y,sx,sy );
 if ( !strcmp( window_name,"main" ) )
  {
   defList->main.x=x;
   defList->main.y=y;
   defList->main.type=itBase;
   strlcpy(tmp, path, sizeof( tmp )); strlcat(tmp, fname, sizeof( tmp )); 
   if ( skinBPRead( tmp,&defList->main.Bitmap ) ) return 1;
   defList->main.width=defList->main.Bitmap.Width;
   defList->main.height=defList->main.Bitmap.Height;
#ifdef HAVE_XSHAPE
    Convert32to1( &defList->main.Bitmap,&defList->main.Mask,0x00ff00ff );
    mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  mask: %dx%d\n",defList->main.Mask.Width,defList->main.Mask.Height );
#else
    defList->main.Mask.Image=NULL;
#endif
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  width: %d height: %d\n",defList->main.width,defList->main.height );
  }
 if ( !strcmp( window_name,"sub" ) )
  {
   defList->sub.type=itBase;
   strlcpy(tmp, path, sizeof( tmp )); strlcat(tmp, fname, sizeof( tmp )); 
   if ( skinBPRead( tmp,&defList->sub.Bitmap ) ) return 1;
   defList->sub.x=x;
   defList->sub.y=y;
   defList->sub.width=defList->sub.Bitmap.Width;
   defList->sub.height=defList->sub.Bitmap.Height;
   if ( sx && sy )
    {
     defList->sub.width=sx;
     defList->sub.height=sy;
    }
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  %d,%d %dx%d\n",defList->sub.x,defList->sub.y,defList->sub.width,defList->sub.height );
  }
 if ( !strcmp( window_name,"menu" ) )
  {
   defList->menuIsPresent=1;
   defList->menuBase.type=itBase;
   strlcpy(tmp, path, sizeof( tmp )); strlcat(tmp, fname, sizeof( tmp )); 
   if ( skinBPRead( tmp,&defList->menuBase.Bitmap ) ) return 1;
   defList->menuBase.width=defList->menuBase.Bitmap.Width;
   defList->menuBase.height=defList->menuBase.Bitmap.Height;
#ifdef HAVE_XSHAPE
    Convert32to1( &defList->menuBase.Bitmap,&defList->menuBase.Mask,0x00ff00ff );
    mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  mask: %dx%d\n",defList->menuBase.Mask.Width,defList->menuBase.Mask.Height );
#else
    defList->menuBase.Mask.Image=NULL;
#endif
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  width: %d height: %d\n",defList->menuBase.width,defList->menuBase.height );
  }
 if ( !strcmp( window_name,"playbar" ) )
  {
   defList->barIsPresent=1;
   defList->bar.x=x;
   defList->bar.y=y;
   defList->bar.type=itBase;
   strlcpy(tmp, path, sizeof( tmp )); strlcat(tmp, fname, sizeof( tmp )); 
   if ( skinBPRead( tmp,&defList->bar.Bitmap ) ) return 1;
   defList->bar.width=defList->bar.Bitmap.Width;
   defList->bar.height=defList->bar.Bitmap.Height;
#ifdef HAVE_XSHAPE
    Convert32to1( &defList->bar.Bitmap,&defList->bar.Mask,0x00ff00ff );
    mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  mask: %dx%d\n",defList->bar.Mask.Width,defList->bar.Mask.Height );
#else
    defList->bar.Mask.Image=NULL;
#endif
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  width: %d height: %d\n",defList->bar.width,defList->bar.height );
  }
 return 0;
}

int cmd_background( char * in )
{
 CHECKDEFLIST( "background" );
 CHECKWINLIST( "background" );

 CHECK( "menu" );
 CHECK( "main" );
 
 currSection->R=cutItemToInt( in,',',0 );
 currSection->G=cutItemToInt( in,',',1 );
 currSection->B=cutItemToInt( in,',',2 );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin]  background color is #%x%x%x.\n",currSection->R,currSection->G,currSection->B );
 
 return 0;
}

int cmd_button( char * in )
{
 unsigned char   fname[512];
 unsigned char   tmp[512];
 int             x,y,sx,sy;
 char            msg[32];

 CHECKDEFLIST( "button" );
 CHECKWINLIST( "button" );

 CHECK( "sub" );
 CHECK( "menu" );  

 cutItem( in,fname,',',0 );
 x=cutItemToInt( in,',',1 );
 y=cutItemToInt( in,',',2 );
 sx=cutItemToInt( in,',',3 );
 sy=cutItemToInt( in,',',4 );
 cutItem( in,msg,',',5 );

 (*currSubItem)++;
 currSubItems[ *currSubItem ].type=itButton;
 currSubItems[ *currSubItem ].x=x;
 currSubItems[ *currSubItem ].y=y;
 currSubItems[ *currSubItem ].width=sx;
 currSubItems[ *currSubItem ].height=sy;
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] button: fname: %s\n",fname );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  x: %d y: %d sx: %d sy: %d\n",x,y,sx,sy );

 if ( ( currSubItems[ *currSubItem ].msg=appFindMessage( msg ) ) == -1 )
   { ERRORMESSAGE( MSGTR_SKIN_BITMAP_UnknownMessage,msg ); return 0; }
 currSubItems[ *currSubItem ].pressed=btnReleased;
 if ( currSubItems[ *currSubItem ].msg == evPauseSwitchToPlay ) currSubItems[ *currSubItem ].pressed=btnDisabled;
 currSubItems[ *currSubItem ].tmp=1;

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  message: %d\n",currSubItems[ *currSubItem ].msg );

 currSubItems[ *currSubItem ].Bitmap.Image=NULL;
 if ( strcmp( fname,"NULL" ) )
  {
   strlcpy(tmp, path, sizeof( tmp )); strlcat(tmp, fname, sizeof( tmp )); 
   if ( skinBPRead( tmp,&currSubItems[ *currSubItem ].Bitmap ) ) return 1;
  }

 return 0;
}

int cmd_selected( char * in )
{
 unsigned char   fname[512];
 unsigned char   tmp[512];

 CHECKDEFLIST( "selected" );
 CHECKWINLIST( "selected" );

 CHECK( "main" );
 CHECK( "sub" );
 CHECK( "playbar" );

 cutItem( in,fname,',',0 );
 defList->menuSelected.type=itBase;
 strlcpy(tmp, path, sizeof( tmp )); strlcat(tmp, fname, sizeof( tmp )); 
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] selected: %s\n",fname );
 if ( skinBPRead( tmp,&defList->menuSelected.Bitmap ) ) return 1;
 defList->menuSelected.width=defList->menuSelected.Bitmap.Width;
 defList->menuSelected.height=defList->menuSelected.Bitmap.Height;
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  width: %d height: %d\n",defList->menuSelected.width,defList->menuSelected.height );
 return 0;
}

int cmd_menu( char * in )
{ // menu = number,x,y,sx,sy,msg
 int             x,y,sx,sy,msg;
 unsigned char   tmp[64];

 CHECKDEFLIST( "menu" );
 CHECKWINLIST( "menu" );

 CHECK( "main" );
 CHECK( "sub" );
 CHECK( "playbar" );
 
 x=cutItemToInt( in,',',0 );
 y=cutItemToInt( in,',',1 );
 sx=cutItemToInt( in,',',2 );
 sy=cutItemToInt( in,',',3 );
 cutItem( in,tmp,',',4 ); msg=appFindMessage( tmp );

 defList->NumberOfMenuItems++;
 defList->MenuItems[ defList->NumberOfMenuItems ].x=x;
 defList->MenuItems[ defList->NumberOfMenuItems ].y=y;
 defList->MenuItems[ defList->NumberOfMenuItems ].width=sx;
 defList->MenuItems[ defList->NumberOfMenuItems ].height=sy;

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] menuitem: %d\n",defList->NumberOfMenuItems );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  x: %d y: %d sx: %d sy: %d\n",x,y,sx,sy );

 if ( ( defList->MenuItems[ defList->NumberOfMenuItems ].msg=msg ) == -1 )
  ERRORMESSAGE( MSGTR_SKIN_BITMAP_UnknownMessage,tmp );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  message: %d\n",defList->Items[ defList->NumberOfItems ].msg );

 defList->MenuItems[ defList->NumberOfMenuItems ].Bitmap.Image=NULL;
 return 0;
}

int cmd_hpotmeter( char * in )
{ // hpotmeter=buttonbitmaps,sx,sy,phasebitmaps,phases,default value,x,y,sx,sy,msg
 int             x,y,psx,psy,ph,sx,sy,msg,d;
 unsigned char   tmp[512];
 unsigned char   pfname[512];
 unsigned char   phfname[512];
 wItem         * item;

 CHECKDEFLIST( "hpotmeter" );
 CHECKWINLIST( "hpotmeter" );

 CHECK( "sub" );
 CHECK( "menu" );

 cutItem( in,pfname,',',0 );
 psx=cutItemToInt( in,',',1 );
 psy=cutItemToInt( in,',',2 );
 cutItem( in,phfname,',',3 );
 ph=cutItemToInt( in,',',4 );
 d=cutItemToInt( in,',',5 );
 x=cutItemToInt( in,',',6 );
 y=cutItemToInt( in,',',7 );
 sx=cutItemToInt( in,',',8 );
 sy=cutItemToInt( in,',',9 );
 cutItem( in,tmp,',',10 ); msg=appFindMessage( tmp );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] h/v potmeter: pointer filename: '%s'\n",pfname );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  pointer size is %dx%d\n",psx,psy );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  phasebitmaps filename: '%s'\n",phfname );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]   position: %d,%d %dx%d\n",x,y,sx,sy );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]   default value: %d\n",d );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  message: %d\n",msg );

 (*currSubItem)++;
 item=&currSubItems[ *currSubItem ];

 item->type=itHPotmeter;
 item->x=x; item->y=y; item->width=sx; item->height=sy;
 item->phases=ph;
 item->psx=psx; item->psy=psy;
 item->msg=msg;
 item->value=(float)d;
 item->pressed=btnReleased;

 item->Bitmap.Image=NULL;
 if ( strcmp( phfname,"NULL" ) )
  {
   strlcpy(tmp, path, sizeof( tmp )); strlcat(tmp, phfname, sizeof( tmp )); 
   if ( skinBPRead( tmp,&item->Bitmap ) ) return 1;
  }

 item->Mask.Image=NULL;
 if ( strcmp( pfname,"NULL" ) )
  {
   strlcpy(tmp, path, sizeof( tmp )); strlcat(tmp, pfname, sizeof( tmp )); 
   if ( skinBPRead( tmp,&item->Mask ) ) return 1;
  }
 return 0;
}

int cmd_vpotmeter( char * in )
{
 int     r = cmd_hpotmeter( in );
 wItem * item;

 item=&currSubItems[ *currSubItem ];
 item->type=itVPotmeter;
 return r;
}

int cmd_potmeter( char * in )
{ // potmeter=phasebitmaps,phases,default value,x,y,sx,sy,msg
 int             x,y,ph,sx,sy,msg,d;
 unsigned char   tmp[512];
 unsigned char   phfname[512];
 wItem         * item;

 CHECKDEFLIST( "potmeter" );
 CHECKWINLIST( "potmeter" );

 CHECK( "sub" );
 CHECK( "menu" );

 cutItem( in,phfname,',',0 );
 ph=cutItemToInt( in,',',1 );
 d=cutItemToInt( in,',',2 );
 x=cutItemToInt( in,',',3 );
 y=cutItemToInt( in,',',4 );
 sx=cutItemToInt( in,',',5 );
 sy=cutItemToInt( in,',',6 );
 cutItem( in,tmp,',',7 ); msg=appFindMessage( tmp );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] potmeter: phases filename: '%s'\n",phfname );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  position: %d,%d %dx%d\n",x,y,sx,sy );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  phases: %d\n",ph );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  default value: %d\n",d );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  message: %d\n",msg );

 (*currSubItem)++;
 item=&currSubItems[ *currSubItem ];

 item->type=itPotmeter;
 item->x=x; item->y=y;
 item->width=sx; item->height=sy;
 item->phases=ph;
 item->msg=msg;
 item->value=(float)d;

 item->Bitmap.Image=NULL;
 if ( strcmp( phfname,"NULL" ) )
  {
   strlcpy(tmp, path, sizeof( tmp )); strlcat(tmp, phfname, sizeof( tmp )); 
   if ( skinBPRead( tmp,&item->Bitmap ) ) return 1;
  }
 return 0;
}

int cmd_font( char * in )
{ // font=fontname,fontid
 char    name[512];
 char    id[512];
 wItem * item;

 CHECKDEFLIST( "font" );
 CHECKWINLIST( "font" );

 CHECK( "sub" );
 CHECK( "menu" );

 cutItem( in,name,',',0 );
 cutItem( in,id,',',1 );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] font\n" );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  name: %s\n",name );

 (*currSubItem)++;
 item=&currSubItems[ *currSubItem ];

 item->type=itFont;
 item->fontid=fntRead( path,name );
 switch ( item->fontid )
  {
   case -1: ERRORMESSAGE( MSGTR_SKIN_FONT_NotEnoughtMemory ); return 1;
   case -2: ERRORMESSAGE( MSGTR_SKIN_FONT_TooManyFontsDeclared ); return 1;
   case -3: ERRORMESSAGE( MSGTR_SKIN_FONT_FontFileNotFound ); return 1;
   case -4: ERRORMESSAGE( MSGTR_SKIN_FONT_FontImageNotFound ); return 1;
  }
 return 0;
}

int cmd_slabel( char * in )
{
 char    tmp[512];
 char    sid[63];
 int     x,y,id;
 wItem * item;

 CHECKDEFLIST( "slabel" );
 CHECKWINLIST( "slabel" );

 CHECK( "sub" );
 CHECK( "menu" );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] slabel\n" );

 x=cutItemToInt( in,',',0 );
 y=cutItemToInt( in,',',1 );
 cutItem( in,sid,',',2 ); id=fntFindID( sid );
 if ( id < 0 ) { ERRORMESSAGE( MSGTR_SKIN_FONT_NonExistentFontID,sid ); return 1; }
 cutItem( in,tmp,',',3 ); cutItem( tmp,tmp,'"',1 );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  pos: %d,%d\n",x,y );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  id: %s ( %d )\n",sid,id );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  str: '%s'\n",tmp );

 (*currSubItem)++;
 item=&currSubItems[ *currSubItem ];

 item->type=itSLabel;
 item->fontid=id;
 item->x=x; item->y=y;
 item->width=-1; item->height=-1;
 if ( ( item->label=malloc( strlen( tmp ) + 1 ) ) == NULL ) { ERRORMESSAGE( MSGTR_SKIN_FONT_NotEnoughtMemory ); return 1; }
 strcpy( item->label,tmp );

 return 0;
}

int cmd_dlabel( char * in )
{ // dlabel=x,y,sx,align,fontid,string ...
 char    tmp[512];
 char    sid[63];
 int     x,y,sx,id,a;
 wItem * item;

 CHECKDEFLIST( "dlabel" );
 CHECKWINLIST( "dlabel" );

 CHECK( "sub" );
 CHECK( "menu" );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] dlabel\n" );

 x=cutItemToInt( in,',',0 );
 y=cutItemToInt( in,',',1 );
 sx=cutItemToInt( in,',',2 );
 a=cutItemToInt( in,',',3 );
 cutItem( in,sid,',',4 ); id=fntFindID( sid );
 if ( id < 0 ) { ERRORMESSAGE( MSGTR_SKIN_FONT_NonExistentFontID,sid ); return 1; }
 cutItem( in,tmp,',',5 ); cutItem( tmp,tmp,'"',1 );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  pos: %d,%d width: %d align: %d\n",x,y,sx,a );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  id: %s ( %d )\n",sid,id );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin]  str: '%s'\n",tmp );

 (*currSubItem)++;
 item=&currSubItems[ *currSubItem ];

 item->type=itDLabel;
 item->fontid=id; item->align=a;
 item->x=x; item->y=y;
 item->width=sx; item->height=-1;
 if ( ( item->label=malloc( strlen( tmp ) + 1 ) ) == NULL ) { ERRORMESSAGE( MSGTR_SKIN_FONT_NotEnoughtMemory ); return 1; }
 strcpy( item->label,tmp );

 return 0;
}

int cmd_decoration( char * in )
{
 char    tmp[512];

 CHECKDEFLIST( "decoration" );
 CHECKWINLIST( "decoration" );

 CHECK( "sub" );
 CHECK( "menu" );
 CHECK( "playbar" );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] window decoration is %s\n",in );
 strlower( in );
 cutItem( in,tmp,',',0 );
 if ( strcmp( tmp,"enable" )&&strcmp( tmp,"disable" ) ) { ERRORMESSAGE( MSGTR_SKIN_UnknownParameter,tmp ); return 1; }
 if ( strcmp( tmp,"enable" ) ) defList->mainDecoration=0;
  else defList->mainDecoration=1;

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"\n[skin] window decoration is %s\n",(defList->mainDecoration?"enabled":"disabled") );
 return 0;
}

typedef struct
{
 const char * name;
 int  (*func)( char * in );
} _item;

_item skinItem[] =
 {
  { "section",     cmd_section     },
  { "end",         cmd_end         },
  { "window",      cmd_window      },
  { "base",        cmd_base        },
  { "button",      cmd_button      },
  { "selected",    cmd_selected    },
  { "background",  cmd_background  },
  { "vpotmeter",   cmd_vpotmeter   },
  { "hpotmeter",   cmd_hpotmeter   },
  { "potmeter",    cmd_potmeter    },
  { "font",        cmd_font        },
  { "slabel",      cmd_slabel      },
  { "dlabel",      cmd_dlabel      },
  { "decoration",  cmd_decoration  },
  { "menu",        cmd_menu        }
 };

#define ITEMS (int)( sizeof( skinItem )/sizeof( _item ) )

char * trimleft( char * in )
{
 int    c = 0;
 char * out;
 if ( strlen( in ) == 0 ) return NULL;
 while ( in[c] == ' ' ) c++;
 if ( c != 0 )
  {
   out=malloc( strlen( in ) - c  + 1 );
   memcpy( out,&in[c],strlen( in ) - c + 1 );
  }
  else out=in;
 return out;
}

char * strswap( char * in,char what,char whereof )
{
 int    i;
 if ( strlen( in ) == 0 ) return NULL;
 for ( i=0;i<(int)strlen( in );i++ )
   if ( in[i] == what ) in[i]=whereof;
 return in;
}

char * trim( char * in )
{
 int    c = 0,i = 0,id = 0;
 if ( strlen( in ) == 0 ) return NULL;
 while ( c != (int)strlen( in ) )
  {
   if ( in[c] == '"' ) id=!id;
   if ( ( in[c] == ' ' )&&( !id ) )
    {
     for ( i=0;i<(int)strlen( in ) - c; i++ ) in[c+i]=in[c+i+1];
     continue;
    }
   c++;
  }
 return in;
}

FILE * skinFile;

void setname( char * item1, char * item2 )
{
  strlcpy(fn, item1, sizeof( fn ));
  strlcat(fn, "/", sizeof( fn )); strlcat(fn, item2, sizeof( fn ));
  strlcpy(path, fn, sizeof( path )); strlcat(path, "/", sizeof( path ));
  strlcat(fn, "/skin", sizeof( fn ));
}

int skinRead( char * dname )
{
 unsigned char   tmp[255];
 unsigned char * ptmp;
 unsigned char   command[32];
 unsigned char   param[256];
 int             c,i;

 setname( skinDirInHome,dname );
 if ( ( skinFile = fopen( fn,"rt" ) ) == NULL )
  {
   setname( skinMPlayerDir,dname );
   if ( ( skinFile = fopen( fn,"rt" ) ) == NULL )
    {
     setname( skinDirInHome_obsolete,dname );
     if ( ( skinFile = fopen( fn,"rt" ) ) == NULL )
      {
       setname( skinMPlayerDir_obsolete,dname );
       if ( ( skinFile = fopen( fn,"rt" ) ) == NULL )
        {
         setname( skinMPlayerDir,dname );
         mp_msg( MSGT_GPLAYER,MSGL_STATUS,MSGTR_SKIN_SkinFileNotFound,fn );
         return -1;
        }
      }
    }
  }

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[skin] file: %s\n",fn );

 appInitStruct( skinAppMPlayer );

 linenumber=0;
 while (fgets(tmp, 255, skinFile))
  {
   linenumber++;

   c=tmp[ strlen( tmp ) - 1 ]; if ( c == '\n' || c == '\r' ) tmp[ strlen( tmp ) - 1 ]=0;
   c=tmp[ strlen( tmp ) - 1 ]; if ( c == '\n' || c == '\r' ) tmp[ strlen( tmp ) - 1 ]=0;
   for ( c=0;c<(int)strlen( tmp );c++ )
    if ( tmp[c] == ';' )
     {
      tmp[c]=0;
      break;
     }
   if ( strlen( tmp ) == 0 ) continue;
   ptmp=trimleft( tmp );
   if ( strlen( ptmp ) == 0 ) continue;
   ptmp=strswap( ptmp,'\t',' ' );
   ptmp=trim( ptmp );

   cutItem( ptmp,command,'=',0 ); cutItem( ptmp,param,'=',1 );
   strlower( command );
   for( i=0;i<ITEMS;i++ )
    if ( !strcmp( command,skinItem[i].name ) )
     if ( skinItem[i].func( param ) ) return -2;
  }
 if (linenumber == 0) {
   mp_msg(MSGT_GPLAYER, MSGL_FATAL, MSGTR_SKIN_SkinFileNotReadable, fn);
   return -1;
 }
 return 0;
}
