
#ifndef __MY_APPS
#define __MY_APPS

#include "bitmap/bitmap.h"
#include "wm/ws.h"
#include "wm/wskeys.h"
#include "events.h"

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
 int        pressed,disabled,tmp;
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
} wItem;

typedef struct
{
 wItem           main;
 wsTWindow       mainWindow;
 int             mainDecoration;

 wItem           sub;
 wsTWindow       subWindow;
 int             subR,subG,subB;
 int             subPixel;

 wItem           eq;
 wsTWindow       eqWindow;

 wItem           menuBase;
 wItem           menuSelected;
 wsTWindow       menuWindow;

// ---
 int             NumberOfItems;
 wItem           Items[256];
// ---
 int             NumberOfMenuItems;
 wItem           MenuItems[32];
} listItems;

extern listItems   appMPlayer;

extern char      * skinDirInHome;
extern char      * skinMPlayerDir;
extern char      * skinName;

extern void appInit( int argc,char* argv[], char *envp[], void* disp );
extern void appInitStruct( listItems * item );
extern void appClearItem( wItem * item );
extern void appCopy( listItems * item1,listItems * item2 );
extern int appFindMessage( unsigned char * str );
extern int appFindKey( unsigned char * name );

#endif
