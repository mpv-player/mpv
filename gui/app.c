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

#include "app.h"
#include "wm/wskeys.h"
#include "skin/skin.h"
#include "mplayer/gmplayer.h"
#include "interface.h"

static const evName evNames[] =
 {
  { evNone,              "evNone"              },
  { evPlay,              "evPlay"              },
  { evStop,              "evStop"              },
  { evPause,             "evPause"             },
  { evPrev,              "evPrev"              },
  { evNext,              "evNext"              },
  { evLoad,              "evLoad"              },
  { evEqualizer,         "evEqualizer"         },
  { evEqualizer,         "evEqualeaser"        },
  { evPlayList,          "evPlaylist"          },
  { evExit,              "evExit"              },
  { evIconify,           "evIconify"           },
  { evIncBalance,        "evIncBalance"        },
  { evDecBalance,        "evDecBalance"        },
  { evFullScreen,        "evFullScreen"        },
  { evFName,             "evFName"             },
  { evMovieTime,         "evMovieTime"         },
  { evAbout,             "evAbout"             },
  { evLoadPlay,          "evLoadPlay"          },
  { evPreferences,       "evPreferences"       },
  { evSkinBrowser,       "evSkinBrowser"       },
  { evBackward10sec,     "evBackward10sec"     },
  { evForward10sec,      "evForward10sec"      },
  { evBackward1min,      "evBackward1min"      },
  { evForward1min,       "evForward1min"       },
  { evBackward10min,     "evBackward10min"     },
  { evForward10min,      "evForward10min"      },
  { evIncVolume,         "evIncVolume"         },
  { evDecVolume,         "evDecVolume"         },
  { evMute,              "evMute"              },
  { evIncAudioBufDelay,  "evIncAudioBufDelay"  },
  { evDecAudioBufDelay,  "evDecAudioBufDelay"  },
  { evPlaySwitchToPause, "evPlaySwitchToPause" },
  { evPauseSwitchToPlay, "evPauseSwitchToPlay" },
  { evNormalSize,        "evHalfSize"          },
  { evNormalSize,        "evNormalSize"        },
  { evDoubleSize,        "evDoubleSize"        },
  { evSetMoviePosition,  "evSetMoviePosition"  },
  { evSetVolume,         "evSetVolume"         },
  { evSetBalance,        "evSetBalance"        },
  { evHelp,		 "evHelp"	       },
  { evLoadSubtitle,      "evLoadSubtitle"      },
  { evPlayDVD,		 "evPlayDVD"	       },
  { evPlayVCD,		 "evPlayVCD"	       },
  { evSetURL,		 "evSetURL"	       },
  { evLoadAudioFile,	 "evLoadAudioFile"     },
  { evDropSubtitle,      "evDropSubtitle"      },
  { evSetAspect,	 "evSetAspect"	       }
 };

static const int evBoxs = sizeof( evNames ) / sizeof( evName );

// ---

listItems   appMPlayer;

/* FIXME: Eventually remove the obsolete directory names. */
char      * skinDirInHome=NULL;
char      * skinDirInHome_obsolete=NULL;
char      * skinMPlayerDir=NULL;
char      * skinMPlayerDir_obsolete=NULL;
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
 for ( i=0;i<item->NumberOfBarItems;i++ )
  appClearItem( &item->barItems[i] );

 item->NumberOfItems=-1;
 memset( item->Items,0,256 * sizeof( wItem ) );
 item->NumberOfMenuItems=-1;
 memset( item->MenuItems,0,64 * sizeof( wItem ) );
 item->NumberOfBarItems=-1;
 memset( item->barItems,0,256 * sizeof( wItem ) );

 appClearItem( &item->main );
 item->mainDecoration=0;
 appClearItem( &item->sub );
 item->sub.width=0; item->sub.height=0;
 item->sub.x=-1; item->sub.y=-1;
 appClearItem( &item->menuBase );
 appClearItem( &item->menuSelected );
 item->sub.R=item->sub.G=item->sub.B=0;
 item->bar.R=item->bar.G=item->bar.B=0;
 item->main.R=item->main.G=item->main.B=0;
 item->barIsPresent=0;
 item->menuIsPresent=0;
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

void btnModify( int event,float state )
{
 int j;
 for ( j=0;j < appMPlayer.NumberOfItems + 1;j++ )
  if ( appMPlayer.Items[j].msg == event )
   {
    switch ( appMPlayer.Items[j].type )
     {
      case itButton:
            appMPlayer.Items[j].pressed=(int)state;
            appMPlayer.Items[j].tmp=(int)state;
            break;
      case itPotmeter:
      case itVPotmeter:
      case itHPotmeter:
    	    if ( state < 0.0f ) state=0.0f;
	    if ( state > 100.f ) state=100.0f;
	    appMPlayer.Items[j].value=state;
	    break;
     }
   }

 for ( j=0;j < appMPlayer.NumberOfBarItems + 1;j++ )
  if ( appMPlayer.barItems[j].msg == event )
   {
    switch ( appMPlayer.barItems[j].type )
     {
      case itButton:
            appMPlayer.barItems[j].pressed=(int)state;
            appMPlayer.barItems[j].tmp=(int)state;
            break;
      case itPotmeter:
      case itVPotmeter:
      case itHPotmeter:
    	    if ( state < 0.0f ) state=0.0f;
	    if ( state > 100.f ) state=100.0f;
	    appMPlayer.barItems[j].value=state;
	    break;
     }
   }
}

float btnGetValue( int event )
{
 int j;
 for ( j=0;j<appMPlayer.NumberOfItems + 1;j++ )
   if ( appMPlayer.Items[j].msg == event ) return appMPlayer.Items[j].value;
 return 0;
}

void btnSet( int event,int set )
{
 int j;
 for ( j=0;j<appMPlayer.NumberOfItems + 1;j++ )
   if ( appMPlayer.Items[j].msg == event )
    { appMPlayer.Items[j].pressed=set; appMPlayer.barItems[j].tmp=0; }
 for ( j=0;j<appMPlayer.NumberOfBarItems + 1;j++ )
   if ( appMPlayer.barItems[j].msg == event )
    { appMPlayer.barItems[j].pressed=set; appMPlayer.barItems[j].tmp=0; }
}
