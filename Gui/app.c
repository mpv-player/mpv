
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "../config.h"
#include "config.h"
#include "error.h"
#include "wm/wskeys.h"
#include "skin/skin.h"
#include "mplayer/mplayer.h"

listItems   appMPlayer;
listItems   appTV;
listItems   appRadio;

char      * appMPlayerDirInHome=NULL;
char      * appMPlayerDir=NULL;
char      * skinDirInHome=NULL;
char      * skinMPlayerDir=NULL;

void appClearItem( wItem * item )
{
 item->type=0;
// ---
 item->x=0; item->y=0; item->width=0; item->height=0;
// ---
 item->px=0; item->py=0; item->psx=0; item->psy=0;
// ---
 item->msg=0; item->msg2=0;
 item->pressed=0;
 item->tmp=0;
 item->key=0; item->key2=0;
 item->Bitmap.Width=0; item->Bitmap.Height=0; item->Bitmap.BPP=0; item->Bitmap.ImageSize=0;
 if ( item->Bitmap.Image )
  { free( item->Bitmap.Image ); item->Bitmap.Image=NULL; }
// ---
 item->fontid=0;
 if ( item->label ) free( item->label ); item->label=NULL;
 item->event=0;
}

void appCopy( listItems * dest,listItems * source )
{
 dest->NumberOfItems=source->NumberOfItems;
 memcpy( &dest->Items,&source->Items,128 * sizeof( wItem ) );

 dest->NumberOfMenuItems=source->NumberOfMenuItems;
 memcpy( &dest->MenuItems,&source->MenuItems,32 * sizeof( wItem ) );

 memcpy( &dest->main,&source->main,sizeof( wItem ) );
 memcpy( &dest->sub,&source->sub,sizeof( wItem ) );
 memcpy( &dest->eq,&source->eq,sizeof( wItem ) );
 memcpy( &dest->menuBase,&source->menuBase,sizeof( wItem ) );
 memcpy( &dest->menuSelected,&source->menuSelected,sizeof( wItem ) );
}

void appInitStruct( listItems * item )
{
 int i;
 for ( i=0;i<item->NumberOfItems;i++ )
  appClearItem( &item->Items[i] );
 for ( i=0;i<item->NumberOfMenuItems;i++ )
  appClearItem( &item->MenuItems[i] );

 item->NumberOfItems=-1;
 memset( item->Items,0,128 * sizeof( wItem ) );
 item->NumberOfMenuItems=-1;
 memset( item->MenuItems,0,32 * sizeof( wItem ) );

 appClearItem( &item->main );
 appClearItem( &item->sub );
 item->sub.Bitmap.Width=256; item->sub.Bitmap.Height=256;
 item->sub.width=256; item->sub.height=256;
 appClearItem( &item->menuBase );
 appClearItem( &item->menuSelected );
 item->subR=0;
 item->subG=0;
 item->subB=0;
}

int appFindKey( unsigned char * name )
{
 int i;
 for ( i=0;i<wsKeyNumber;i++ )
  if ( !strcmp( wsKeyNames[i].name,name ) ) return wsKeyNames[i].code;
 return -1;
}

int appFindMessage( unsigned char * str )
{
 int i;
 for ( i=0;i<evBoxs;i++ )
  if ( !strcmp( evNames[i].name,str ) ) return evNames[i].msg;
 return -1;
}

void appInit( int argc,char* argv[], char *envp[] )
{
 if ( ( appMPlayerDirInHome=(char *)calloc( 1,strlen( getenv( "HOME" ) ) + 9 ) ) != NULL )
  { strcpy( appMPlayerDirInHome,getenv( "HOME" ) ); strcat( appMPlayerDirInHome,"/.mplayer" ); }
 if ( ( skinDirInHome=(char *)calloc( 1,strlen( appMPlayerDirInHome ) + 5 ) ) != NULL )
  { strcpy( skinDirInHome,appMPlayerDirInHome ); strcat( skinDirInHome,"/Skin" ); }
 if ( ( appMPlayerDir=(char *)calloc( 1,strlen( PREFIX ) + 14 ) ) != NULL )
  { strcpy( appMPlayerDir,PREFIX ); strcat( appMPlayerDir,"/share/mplayer" ); }
 if ( ( skinMPlayerDir=(char *)calloc( 1,strlen( appMPlayerDir ) + 5 ) ) != NULL )
  { strcpy( skinMPlayerDir,appMPlayerDir ); strcat( skinMPlayerDir,"/Skin" ); }

 initDebug(NULL);

 cfgDefaults();
 cfgRead();
 if ( !strcmp( cfgAppName,"movieplayer" ) )
  {
   appMPlayer.sub.x=-1; appMPlayer.sub.y=-1; appMPlayer.sub.width=512; appMPlayer.sub.height=256;
   switch ( skinRead( cfgSkin ) )
    {
     case -1: dbprintf( 0,"[app] skin configfile not found.\n" ); exit( 0 );
     case -2: dbprintf( 0,"[app] skin configfile read error.\n" ); exit( 0 );
    }
   mplInit( argc,argv,envp );
  }
}
