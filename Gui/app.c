
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"
#include "../help_mp.h"

#include "app.h"
#include "wm/wskeys.h"
#include "skin/skin.h"
#include "mplayer/mplayer.h"
#include "interface.h"

listItems   appMPlayer;

char      * skinDirInHome=NULL;
char      * skinMPlayerDir=NULL;
char      * skinName = NULL;

void appClearItem( wItem * item )
{
 item->type=0;
// ---
 item->x=0; item->y=0; item->width=0; item->height=0;
// ---
 item->px=0; item->py=0; item->psx=0; item->psy=0;
// ---
 item->msg=0; item->msg2=0;
 item->pressed=btnReleased;
 item->tmp=0;
 item->key=0; item->key2=0;
 item->Bitmap.Width=0; item->Bitmap.Height=0; item->Bitmap.BPP=0; item->Bitmap.ImageSize=0;
 if ( item->Bitmap.Image ) free( item->Bitmap.Image ); 
 item->Bitmap.Image=NULL;
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
 item->mainDecoration=0;
 appClearItem( &item->sub );
 item->sub.Bitmap.Width=384; item->sub.Bitmap.Height=384;
 item->sub.width=384; item->sub.height=384;
 item->sub.x=-1; item->sub.y=-1;
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

void appInit( void * disp )
{
 skinDirInHome=get_path("Skin");
 skinMPlayerDir=DATADIR "/Skin";
 printf("SKIN dir 1: '%s'\n",skinDirInHome);
 printf("SKIN dir 2: '%s'\n",skinMPlayerDir);
 if ( !skinName ) skinName=strdup( "default" );
 switch ( skinRead( skinName ) )
  {
   case -1: mp_msg( MSGT_GPLAYER,MSGL_ERR,MSGTR_SKIN_SKINCFG_SkinNotFound,skinName ); exit( 0 );
   case -2: mp_msg( MSGT_GPLAYER,MSGL_ERR,MSGTR_SKIN_SKINCFG_SkinCfgReadError,skinName ); exit( 0 );
  }
 mplInit( disp ); // does gtk & ws initialization, create windows
}
