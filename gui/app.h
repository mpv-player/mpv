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

#ifndef MPLAYER_GUI_APP_H
#define MPLAYER_GUI_APP_H

#include "bitmap.h"
#include "wm/ws.h"
#include "wm/wskeys.h"

// --- User events ------

#define evNone              0
#define evPlay              1
#define evStop              2
#define evPause             3
#define evPrev              6
#define evNext              7
#define evLoad              8
#define evEqualizer         9
#define evPlayList          10
#define evIconify           11
#define evAbout             12
#define evLoadPlay          13
#define evPreferences       14
#define evSkinBrowser       15
#define evPlaySwitchToPause 16
#define evPauseSwitchToPlay 17

#define evBackward10sec     18
#define evForward10sec      19
#define evBackward1min      20
#define evForward1min       21
#define evBackward10min     22
#define evForward10min      23

#define evHalfSize          301
#define evNormalSize        24
#define evDoubleSize        25
#define evFullScreen        26

#define evSetMoviePosition  27
#define evSetVolume         28
#define evSetBalance        29
#define evMute              30

#define evIncVolume         31
#define evDecVolume         32
#define evIncAudioBufDelay  33
#define evDecAudioBufDelay  34
#define evIncBalance        35
#define evDecBalance        36

#define evHelp              37

#define evLoadSubtitle      38
#define evDropSubtitle      43
#define evPlayDVD           39
#define evPlayVCD	    40
#define evPlayNetwork       41
#define evLoadAudioFile	    42
#define evSetAspect         44
#define evSetAudio	    45
#define evSetVideo	    46
#define evSetSubtitle       47
// 48 ...

#define evExit              1000

// --- General events ---

#define evFileLoaded      5000
#define evHideMouseCursor 5001
#define evMessageBox      5002
#define evGeneralTimer    5003
#define evGtkIsOk         5004
#define evShowPopUpMenu   5005
#define evHidePopUpMenu   5006
#define evSetDVDAudio     5007
#define evSetDVDSubtitle  5008
#define evSetDVDTitle     5009
#define evSetDVDChapter   5010
#define evSubtitleLoaded  5011
#define evSetVCDTrack     5012
#define evSetURL          5013

#define evFName           7000
#define evMovieTime       7001
#define evRedraw          7002
#define evHideWindow      7003
#define evShowWindow      7004
#define evFirstLoad       7005

// ----------------------

typedef struct
{
 int    msg;
 const char * name;
} evName;

#define itNULL      0
#define itButton    101 // button
#define itHPotmeter 102 // horizontal potmeter
#define itVPotmeter 103 // vertical potmeter
#define itSLabel    104 // static label
#define itDLabel    105 // dynamic label
#define itBase      106
#define itPotmeter  107
#define itFont      108
// ---
#define btnPressed  0
#define btnReleased 1
#define btnDisabled 2
// ---
typedef struct
{
 int        type;
// ---
 int        x,y;
 int        width,height;
// ---
 int        px,py,psx,psy;
// ---
 int        msg,msg2;
 int        pressed,tmp;
 int        key,key2;
 int        phases;
 float      value;
 txSample   Bitmap;
 txSample   Mask;
// ---
 int        fontid;
 int        align;
 char     * label;
// ---
 int        event;
// --- 
 int        R,G,B;
} wItem;

typedef struct
{
 wItem           main;
 wsTWindow       mainWindow;
 int             mainDecoration;

 wItem           sub;
 wsTWindow       subWindow;

 wItem           bar;
 wsTWindow       barWindow;
 int             barIsPresent;
  
 wItem           menuBase;
 wItem           menuSelected;
 wsTWindow       menuWindow;
 int		 menuIsPresent;

// ---
 int             NumberOfItems;
 wItem           Items[256];
// ---
 int             NumberOfMenuItems;
 wItem           MenuItems[64];
// ---
 int		 NumberOfBarItems;
 wItem		 barItems[256];
} listItems;

extern listItems   appMPlayer;

extern char      * skinDirInHome;
extern char      * skinDirInHome_obsolete;
extern char      * skinMPlayerDir;
extern char      * skinMPlayerDir_obsolete;
extern char      * skinName;

void appInitStruct( listItems * item );
void appClearItem( wItem * item );
void appCopy( listItems * item1, listItems * item2 );
int appFindMessage( unsigned char * str );
int appFindKey( unsigned char * name );

void btnModify( int event, float state );
float btnGetValue( int event );
void btnSet( int event, int set );

#endif /* MPLAYER_GUI_APP_H */
