
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cut.h"
#include "error.h"
#include "font.h"
#include "../app.h"
#include "../language.h"
#include "../../config.h"

char            SkinDir[] = "/.mplayer/Skin/";
char          * Skin;

listItems     * skinAppMPlayer = &appMPlayer;
listItems     * skinAppTV      = &appTV;
listItems     * skinAppRadio   = &appRadio;

int             linenumber;

unsigned char   path[512],fn[512];

listItems     * defList = NULL;
unsigned char   winList[32] = "";

#include <stdarg.h>

void ERRORMESSAGE( const char * format, ... )
{
 char      p[512];
 va_list   ap;
 va_start( ap,format );
 vsnprintf( p,512,format,ap );
 va_end( ap );
 message( False,"[skin] error in skin config file on line %d: %s",linenumber,p );
}

#define CHECKDEFLIST( str ) { \
                              if ( defList == NULL ) \
                               { \
                                message( False,"[skin] warning in skin config file on line %d: widget found but before \"section\" not found ("str")",linenumber ); \
                                return 1; \
                               } \
                            }
#define CHECKWINLIST( str ) { \
                              if ( !strlen( winList ) ) \
                               { \
                                message( False,"[skin] warning in skin config file on line %d: widget found but before \"subsection\" not found ("str")",linenumber ); \
                                return 1; \
                               } \
                            }

char * strlower( char * in )
{
 int i;
 for( i=0;i<strlen( in );i++ ) in[i]=( in[i] >= 'A' ? ( in[i] <= 'Z' ?  in[i]+='A' : in[i] ) : in[i] );
 return in;
}

int skinBPRead( char * fname, txSample * bf )
{
 int i=bpRead( fname,bf );
 switch ( i )
  {
   case -1: ERRORMESSAGE( "16 bits or less depth bitmap not supported ( %s ).\n",fname ); break;
   case -2: ERRORMESSAGE( "file not found ( %s )\n",fname ); break;
   case -3: ERRORMESSAGE( "bmp read error ( %s )\n",fname ); break;
   case -4: ERRORMESSAGE( "tga read error ( %s )\n",fname ); break;
   case -5: ERRORMESSAGE( "png read error ( %s )\n",fname ); break;
   case -6: ERRORMESSAGE( "RLE packed tga not supported ( %s )\n",fname ); break;
   case -7: ERRORMESSAGE( "unknown file type ( %s )\n",fname ); break;
   case -8: ERRORMESSAGE( "24 bit to 32 bit convert error ( %s )\n",fname ); break;
  }
 return i;
}

int __section( char * in )
{
 strlower( in );
 defList=NULL;
 if ( !strcmp( in,"movieplayer" ) ) defList=skinAppMPlayer;
 #ifdef DEBUG
  dbprintf( 3,"\n[skin] sectionname: %s\n",in );
 #endif
 return 0;
}

int __end( char * in )
{
 if ( strlen( winList ) ) winList[0]=0;
  else defList=NULL;
 #ifdef DEBUG
  dbprintf( 3,"\n[skin] end section\n" );
 #endif
 return 0;
}

int __window( char * in )
{
 CHECKDEFLIST( "window" );

 strlower( in );
 strcpy( winList,in );
 #ifdef DEBUG
  dbprintf( 3,"\n[skin] window: %s\n",winList );
 #endif
 return 0;
}

int __base( char * in )
{
 unsigned char fname[512];
 unsigned char tmp[512];
 int           x,y;

 CHECKDEFLIST( "base" );
 CHECKWINLIST( "base" );

 cutItem( in,fname,',',0 );
 cutItem( in,tmp,',',1 ); x=atoi( tmp );
 cutItem( in,tmp,',',2 ); y=atoi( tmp );
 #ifdef DEBUG
  dbprintf( 3,"\n[skin] base: %s x: %d y: %d\n",fname,x,y );
 #endif
 if ( !strcmp( winList,"main" ) )
  {
   defList->main.x=x;
   defList->main.y=y;
   defList->main.type=itBase;
   strcpy( tmp,path ); strcat( tmp,fname );
   if ( skinBPRead( tmp,&defList->main.Bitmap ) ) return 1;
   defList->main.width=defList->main.Bitmap.Width;
   defList->main.height=defList->main.Bitmap.Height;
   #ifdef HAVE_XSHAPE
    defList->main.Mask.Width=defList->main.Bitmap.Width;
    defList->main.Mask.Height=defList->main.Bitmap.Height;
    defList->main.Mask.BPP=1;
    defList->main.Mask.ImageSize=defList->main.Mask.Width * defList->main.Mask.Height / 8;
    defList->main.Mask.Image=(char *)calloc( 1,defList->main.Mask.ImageSize );
    if ( defList->main.Mask.Image == NULL ) message( True,langNEMFMM );
    {
     int i,b,c=0; unsigned long * buf = NULL; unsigned char tmp = 0;
     buf=(unsigned long *)defList->main.Bitmap.Image;
     for ( b=0,i=0;i < defList->main.Mask.Width * defList->main.Mask.Height;i++ )
      {
       if ( buf[i] != 0x00ff00ff ) tmp=( tmp >> 1 )|128;
        else { tmp=tmp >> 1; buf[i]=0; }
       if ( b++ == 7 ) { defList->main.Mask.Image[c++]=tmp; tmp=0; b=0; }
      }
     defList->main.Mask.Image[c++]=tmp;
    }
    #ifdef DEBUG
     dbprintf( 3,"[skin]  mask: %dX%d\n",defList->main.Mask.Width,defList->main.Mask.Height );
    #endif
   #else
    defList->main.Mask.Image=NULL;
   #endif
   #ifdef DEBUG
    dbprintf( 3,"[skin]  width: %d height: %d\n",defList->main.width,defList->main.height );
   #endif
  }
 if ( !strcmp( winList,"sub" ) )
  {
   defList->sub.x=x;
   defList->sub.y=y;
   defList->sub.type=itBase;
   strcpy( tmp,path ); strcat( tmp,fname );
   if ( skinBPRead( tmp,&defList->sub.Bitmap ) ) return 1;
   defList->sub.width=defList->sub.Bitmap.Width;
   defList->sub.height=defList->sub.Bitmap.Height;
   #ifdef DEBUG
    dbprintf( 3,"[skin]  width: %d height: %d\n",defList->sub.width,defList->sub.height );
   #endif
  }
/*
 if ( !strcmp( winList,"eq" ) )
  {
   defList->eq.x=x;
   defList->eq.y=y;
   defList->eq.type=itBase;
   strcpy( tmp,path ); strcat( tmp,fname );
   if ( skinBPRead( tmp,&defList->eq.Bitmap ) ) return 1;
   defList->eq.width=defList->eq.Bitmap.Width;
   defList->eq.height=defList->eq.Bitmap.Height;
   #ifdef DEBUG
    dbprintf( 3,"[skin]  width: %d height: %d\n",defList->eq.width,defList->eq.height );
   #endif
  }
*/
 if ( !strcmp( winList,"menu" ) )
  {
   defList->menuBase.type=itBase;
   strcpy( tmp,path ); strcat( tmp,fname );
   if ( skinBPRead( tmp,&defList->menuBase.Bitmap ) ) return 1;
   defList->menuBase.width=defList->menuBase.Bitmap.Width;
   defList->menuBase.height=defList->menuBase.Bitmap.Height;
   #ifdef DEBUG
    dbprintf( 3,"[skin]  width: %d height: %d\n",defList->menuBase.width,defList->menuBase.height );
   #endif
  }
 return 0;
}

int __background( char * in )
{
 unsigned char tmp[512];

 CHECKDEFLIST( "background" );
 CHECKWINLIST( "background" );

 if ( !strcmp( winList,"sub" ) )
  {
   cutItem( in,tmp,',',0 ); defList->subR=atoi( tmp );
   cutItem( in,tmp,',',1 ); defList->subG=atoi( tmp );
   cutItem( in,tmp,',',2 ); defList->subB=atoi( tmp );
   #ifdef DEBUG
    dbprintf( 3,"\n[skin] subwindow background color is #%x%x%x.\n",defList->subR,defList->subG,defList->subB );
   #endif
  }
 return 0;
}

int __button( char * in )
{
 unsigned char   fname[512];
 unsigned char   tmp[512];
 int             x,y,sx,sy;
 unsigned char   msg[32];

 CHECKDEFLIST( "button" );
 CHECKWINLIST( "button" );

// button=prev,17,89,23,18,Up,evPrev

 cutItem( in,fname,',',0 );
 cutItem( in,tmp,',',1 ); x=atoi( tmp );
 cutItem( in,tmp,',',2 ); y=atoi( tmp );
 cutItem( in,tmp,',',3 ); sx=atoi( tmp );
 cutItem( in,tmp,',',4 ); sy=atoi( tmp );
 cutItem( in,msg,',',5 );

 defList->NumberOfItems++;
 defList->Items[ defList->NumberOfItems ].type=itButton;
 defList->Items[ defList->NumberOfItems ].x=x;
 defList->Items[ defList->NumberOfItems ].y=y;
 defList->Items[ defList->NumberOfItems ].width=sx;
 defList->Items[ defList->NumberOfItems ].height=sy;
 #ifdef DEBUG
  dbprintf( 3,"\n[skin] button: fname: %s\n",fname );
  dbprintf( 3,"[skin]  x: %d y: %d sx: %d sy: %d\n",x,y,sx,sy );
 #endif

 if ( ( defList->Items[ defList->NumberOfItems ].msg=appFindMessage( msg ) ) == -1 )
   { ERRORMESSAGE( "unknown message: %s\n",msg ); return 1; }
 defList->Items[ defList->NumberOfItems ].pressed=btnReleased;
 if ( defList->Items[ defList->NumberOfItems ].msg == evPauseSwitchToPlay ) defList->Items[ defList->NumberOfItems ].pressed=btnDisabled;
 defList->Items[ defList->NumberOfItems ].tmp=1;

 #ifdef DEBUG
  dbprintf( 3,"[skin]  message: %d\n",
   defList->Items[ defList->NumberOfItems ].msg );
 #endif

 defList->Items[ defList->NumberOfItems ].Bitmap.Image=NULL;
 if ( strcmp( fname,"NULL" ) )
  {
   strcpy( tmp,path ); strcat( tmp,fname );
   if ( skinBPRead( tmp,&defList->Items[ defList->NumberOfItems ].Bitmap ) ) return 1;
  }
 return 0;
}

int __selected( char * in )
{
 unsigned char   fname[512];
 unsigned char   tmp[512];

 CHECKDEFLIST( "selected" );
 CHECKWINLIST( "selected" );

 cutItem( in,fname,',',0 );
 defList->menuSelected.type=itBase;
 strcpy( tmp,path ); strcat( tmp,fname );
 #ifdef DEBUG
  dbprintf( 3,"\n[skin] selected: %s\n",fname );
 #endif
 if ( skinBPRead( tmp,&defList->menuSelected.Bitmap ) ) return 1;
 defList->menuSelected.width=defList->menuSelected.Bitmap.Width;
 defList->menuSelected.height=defList->menuSelected.Bitmap.Height;
 #ifdef DEBUG
  dbprintf( 3,"[skin]  width: %d height: %d\n",defList->menuSelected.width,defList->menuSelected.height );
 #endif
 return 0;
}

int __menu( char * in )
{ // menu = number,x,y,sx,sy,msg
 int             x,y,sx,sy,msg;
 unsigned char   tmp[64];

 CHECKDEFLIST( "menu" );
 CHECKWINLIST( "menu" );

 cutItem( in,tmp,',',0 ); x=atoi( tmp );
 cutItem( in,tmp,',',1 ); y=atoi( tmp );
 cutItem( in,tmp,',',2 ); sx=atoi( tmp );
 cutItem( in,tmp,',',3 ); sy=atoi( tmp );
 cutItem( in,tmp,',',4 ); msg=appFindMessage( tmp );

 defList->NumberOfMenuItems++;
 defList->MenuItems[ defList->NumberOfMenuItems ].x=x;
 defList->MenuItems[ defList->NumberOfMenuItems ].y=y;
 defList->MenuItems[ defList->NumberOfMenuItems ].width=sx;
 defList->MenuItems[ defList->NumberOfMenuItems ].height=sy;

 #ifdef DEBUG
  dbprintf( 3,"\n[skin] menuitem: %d\n",defList->NumberOfMenuItems );
  dbprintf( 3,"[skin]  x: %d y: %d sx: %d sy: %d\n",x,y,sx,sy );
 #endif

 if ( ( defList->MenuItems[ defList->NumberOfMenuItems ].msg=msg ) == -1 )
  ERRORMESSAGE( "unknown message: %s\n",tmp );

 #ifdef DEBUG
  dbprintf( 3,"[skin]  message: %d\n",defList->Items[ defList->NumberOfItems ].msg );
 #endif

 defList->MenuItems[ defList->NumberOfMenuItems ].Bitmap.Image=NULL;
 return 0;
}

int __hpotmeter( char * in )
{ // hpotmeter=buttonbitmaps,sx,sy,phasebitmaps,phases,default value,x,y,sx,sy,msg
 int             x,y,psx,psy,ph,sx,sy,msg,d;
 unsigned char   tmp[512];
 unsigned char   pfname[512];
 unsigned char   phfname[512];
 wItem         * item;

 CHECKDEFLIST( "hpotmeter" );
 CHECKWINLIST( "hpotmeter" );

 cutItem( in,pfname,',',0 );
 cutItem( in,tmp,',',1 ); psx=atoi( tmp );
 cutItem( in,tmp,',',2 ); psy=atoi( tmp );
 cutItem( in,phfname,',',3 );
 cutItem( in,tmp,',',4 ); ph=atoi( tmp );
 cutItem( in,tmp,',',5 ); d=atoi( tmp );
 cutItem( in,tmp,',',6 ); x=atoi( tmp );
 cutItem( in,tmp,',',7 ); y=atoi( tmp );
 cutItem( in,tmp,',',8 ); sx=atoi( tmp );
 cutItem( in,tmp,',',9 ); sy=atoi( tmp );
 cutItem( in,tmp,',',10 ); msg=appFindMessage( tmp );

 #ifdef DEBUG
  dbprintf( 3,"\n[skin] hpotmeter: pointer filename: '%s'\n",pfname );
  dbprintf( 3,  "[skin]  pointer size is %dx%d\n",psx,psy );
  dbprintf( 3,  "[skin]  phasebitmaps filename: '%s'\n",phfname );
  dbprintf( 3,  "[skin]   position: %d,%d %dx%d\n",x,y,sx,sy );
  dbprintf( 3,  "[skin]   default value: %d\n",d );
  dbprintf( 3,  "[skin]  message: %d\n",msg );
 #endif

 defList->NumberOfItems++;
 item=&defList->Items[ defList->NumberOfItems ];
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
   strcpy( tmp,path ); strcat( tmp,phfname );
   if ( skinBPRead( tmp,&item->Bitmap ) ) return 1;
  }

 item->Mask.Image=NULL;
 if ( strcmp( pfname,"NULL" ) )
  {
   strcpy( tmp,path ); strcat( tmp,pfname );
   if ( skinBPRead( tmp,&item->Mask ) ) return 1;
  }

 return 0;
}

int __potmeter( char * in )
{ // potmeter=phasebitmaps,phases,default value,x,y,sx,sy,msg
 int             x,y,ph,sx,sy,msg,d;
 unsigned char   tmp[512];
 unsigned char   phfname[512];
 wItem         * item;

 CHECKDEFLIST( "potmeter" );
 CHECKWINLIST( "potmeter" );

 cutItem( in,phfname,',',0 );
 cutItem( in,tmp,',',1 ); ph=atoi( tmp );
 cutItem( in,tmp,',',2 ); d=atoi( tmp );
 cutItem( in,tmp,',',3 ); x=atoi( tmp );
 cutItem( in,tmp,',',4 ); y=atoi( tmp );
 cutItem( in,tmp,',',5 ); sx=atoi( tmp );
 cutItem( in,tmp,',',6 ); sy=atoi( tmp );
 cutItem( in,tmp,',',7 ); msg=appFindMessage( tmp );

 #ifdef DEBUG
  dbprintf( 3,"\n[skin] potmeter: phases filename: '%s'\n",phfname );
  dbprintf( 3,  "[skin]  position: %d,%d %dx%d\n",x,y,sx,sy );
  dbprintf( 3,  "[skin]  phases: %d\n",ph );
  dbprintf( 3,  "[skin]  default value: %d\n",d );
  dbprintf( 3,  "[skin]  message: %d\n",msg );
 #endif

 defList->NumberOfItems++;
 item=&defList->Items[ defList->NumberOfItems ];
 item->type=itPotmeter;
 item->x=x; item->y=y;
 item->width=sx; item->height=sy;
 item->phases=ph;
 item->msg=msg;
 item->value=(float)d;

 item->Bitmap.Image=NULL;
 if ( strcmp( phfname,"NULL" ) )
  {
   strcpy( tmp,path ); strcat( tmp,phfname );
   if ( skinBPRead( tmp,&item->Bitmap ) ) return 1;
  }
 return 0;
}

int __font( char * in )
{ // font=fontname,fontid
 char    name[512];
 char    id[512];
 wItem * item;

 CHECKDEFLIST( "font" );
 CHECKWINLIST( "font" );

 cutItem( in,name,',',0 );
 cutItem( in,id,',',1 );

 #ifdef DEBUG
  dbprintf( 3,"\n[skin] font\n" );
  dbprintf( 3,  "[skin]  name: %s\n",name );
 #endif

 defList->NumberOfItems++;
 item=&defList->Items[ defList->NumberOfItems ];
 item->type=itFont;
 item->fontid=fntAddNewFont( name );
 switch ( item->fontid )
  {
   case -1: ERRORMESSAGE( "not enought memory\n" ); return 1;
   case -2: ERRORMESSAGE( "too many fonts\n" ); return 1;
  }

 #ifdef DEBUG
  dbprintf( 3,  "[skin]  id: %s ( %d )\n",id,item->fontid );
 #endif

 switch ( fntRead( path,name,item->fontid ) )
  {
   case -1: ERRORMESSAGE( "font file not found\n" ); return 1;
   case -2: ERRORMESSAGE( "font image not found\n" ); return 1;
  }

 return 0;
}

int __slabel( char * in )
{
 char    tmp[512];
 char    sid[63];
 int     x,y,id;
 wItem * item;

 CHECKDEFLIST( "slabel" );
 CHECKWINLIST( "slabel" );

 #ifdef DEBUG
  dbprintf( 3,"\n[skin] slabel\n" );
 #endif

 cutItem( in,tmp,',',0 ); x=atoi( tmp );
 cutItem( in,tmp,',',1 ); y=atoi( tmp );
 cutItem( in,sid,',',2 ); id=fntFindID( sid );
 if ( id < 0 ) { ERRORMESSAGE( "nonexistent font id. ( %s )\n",sid ); return 1; }
 cutItem( in,tmp,',',3 ); cutItem( tmp,tmp,'"',1 );

 #ifdef DEBUG
  dbprintf( 3,  "[skin]  pos: %d,%d\n",x,y );
  dbprintf( 3,  "[skin]  id: %s ( %d )\n",sid,id );
  dbprintf( 3,  "[skin]  str: '%s'\n",tmp );
 #endif

 defList->NumberOfItems++;
 item=&defList->Items[ defList->NumberOfItems ];
 item->type=itSLabel;
 item->fontid=id;
 item->x=x; item->y=y;
 item->width=-1; item->height=-1;
 if ( ( item->label=malloc( strlen( tmp ) + 1 ) ) == NULL ) { ERRORMESSAGE( "not enought memory.\n" ); return 1; }
 strcpy( item->label,tmp );

 return 0;
}

int __dlabel( char * in )
{ // dlabel=x,y,sx,align,fontid,string ...
 char    tmp[512];
 char    sid[63];
 int     x,y,sx,id,a;
 wItem * item;

 CHECKDEFLIST( "dlabel" );
 CHECKWINLIST( "dlabel" );

 #ifdef DEBUG
  dbprintf( 3,"\n[skin] dlabel\n" );
 #endif

 cutItem( in,tmp,',',0 ); x=atoi( tmp );
 cutItem( in,tmp,',',1 ); y=atoi( tmp );
 cutItem( in,tmp,',',2 ); sx=atoi( tmp );
 cutItem( in,tmp,',',3 ); a=atoi( tmp );
 cutItem( in,sid,',',4 ); id=fntFindID( sid );
 if ( id < 0 ) { ERRORMESSAGE( "nonexistent font id. ( %s )\n",sid ); return 1; }
 cutItem( in,tmp,',',5 ); cutItem( tmp,tmp,'"',1 );

 #ifdef DEBUG
  dbprintf( 3,"[skin]  pos: %d,%d width: %d align: %d\n",x,y,sx,a );
  dbprintf( 3,"[skin]  id: %s ( %d )\n",sid,id );
  dbprintf( 3,"[skin]  str: '%s'\n",tmp );
 #endif

 defList->NumberOfItems++;
 item=&defList->Items[ defList->NumberOfItems ];
 item->type=itDLabel;
 item->fontid=id; item->align=a;
 item->x=x; item->y=y;
 item->width=sx; item->height=-1;
 if ( ( item->label=malloc( strlen( tmp ) + 1 ) ) == NULL ) { ERRORMESSAGE( "not enought memory.\n" ); return 1; }
 strcpy( item->label,tmp );

 return 0;
}

typedef struct
{
 char * name;
 int  (*func)( char * in );
} _item;

_item skinItem[] =
 {
  { "section",     __section     },
  { "end",         __end         },
  { "window",      __window      },
  { "base",        __base        },
  { "button",      __button      },
  { "selected",    __selected    },
  { "background",  __background  },
  { "hpotmeter",   __hpotmeter   },
  { "potmeter",    __potmeter    },
  { "font",        __font        },
  { "slabel",      __slabel      },
  { "dlabel",      __dlabel      },
  { "menu",        __menu        }
 };

#define ITEMS ( sizeof( skinItem )/sizeof( _item ) )

char * strdelspacesbeforecommand( char * in )
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
 for ( i=0;i<strlen( in );i++ )
   if ( in[i] == what ) in[i]=whereof;
 return in;
}

char * strdelspaces( char * in )
{
 int    c = 0,i = 0,id = 0;
 if ( strlen( in ) == 0 ) return NULL;
 while ( c != strlen( in ) )
  {
   if ( in[c] == '"' ) id=!id;
   if ( ( in[c] == ' ' )&&( !id ) )
    {
     for ( i=0;i<strlen( in ) - c; i++ ) in[c+i]=in[c+i+1];
     continue;
    }
   c++;
  }
 return in;
}

FILE * skinFile;

void setname( char * item1, char * item2 )
{ strcpy( fn,item1 ); strcat( fn,"/" ); strcat( fn,item2 ); strcpy( path,fn ); strcat( path,"/" ); strcat( fn,"/skin" ); }

int skinRead( char * dname )
{
 unsigned char   tmp[255];
 unsigned char * ptmp;
 unsigned char   command[32];
 unsigned char   param[256];
 int             c,i;

 setname( skinMPlayerDir,dname );
 if ( ( skinFile = fopen( fn,"rt" ) ) == NULL )
  {
   setname( skinDirInHome,dname );
   if ( ( skinFile = fopen( fn,"rt" ) ) == NULL )
    {
     dbprintf( 3,"[skin] file ( %s ) not found.\n",fn );
     return -1;
    }
  }

 #ifdef DEBUG
  dbprintf( 3,"[skin] file: %s\n",fn );
 #endif

 appInitStruct( &appMPlayer );

 linenumber=0;
 while ( !feof( skinFile ) )
  {
   fgets( tmp,255,skinFile ); linenumber++;

   c=tmp[ strlen( tmp ) - 1 ]; if ( c == '\n' || c == '\r' ) tmp[ strlen( tmp ) - 1 ]=0;
   c=tmp[ strlen( tmp ) - 1 ]; if ( c == '\n' || c == '\r' ) tmp[ strlen( tmp ) - 1 ]=0;
   for ( c=0;c<strlen( tmp );c++ )
    if ( tmp[c] == ';' )
     {
      tmp[c]=0;
      break;
     }
   if ( strlen( tmp ) == 0 ) continue;
   ptmp=strdelspacesbeforecommand( tmp );
   if ( strlen( ptmp ) == 0 ) continue;
   ptmp=strswap( ptmp,'\t',' ' );
   ptmp=strdelspaces( ptmp );

   cutItem( ptmp,command,'=',0 ); cutItem( ptmp,param,'=',1 );
   strlower( command );
   for( i=0;i<ITEMS;i++ )
    if ( !strcmp( command,skinItem[i].name ) )
     if ( skinItem[i].func( param ) ) return -2;
  }
 return 0;
}

void btnModify( int event,float state )
{
 int j;
 for ( j=0;j<appMPlayer.NumberOfItems + 1;j++ )
  if ( appMPlayer.Items[j].msg == event )
   {
    switch ( appMPlayer.Items[j].type )
     {
      case itButton:
           appMPlayer.Items[j].pressed=(int)state;
           break;
      case itPotmeter:
      case itHPotmeter:
           if ( state < 0.0f ) state=0.0f;
           if ( state > 100.f ) state=100.0f;
           appMPlayer.Items[j].value=state;
           break;
     }
   }
}

int btnGetValue( int event )
{
 int j;
 for ( j=0;j<appMPlayer.NumberOfItems + 1;j++ )
  if ( appMPlayer.Items[j].msg == event ) return appMPlayer.Items[j].value;
 return 0;
}
