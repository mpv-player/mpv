
#ifndef _MYEVENTHANDLER
#define _MYEVENTHANDLER

// --- User events ------

#define evNone              0
#define evPlay              1
#define evStop              2
#define evPause             3
#define evPrev              6
#define evNext              7
#define evLoad              8
#define evEqualeaser        9
#define evPlayList          10
#define evPlusVideo         11
#define evMinusVideo        12
#define evIconify           13
#define evPlusBalance       14
#define evMinusBalance      15
#define evFullScreen        16
#define evAbout             18
#define evLoadPlay          19
#define evPreferences       20
#define evSkinBrowser       21
#define evBackward10sec     22
#define evForward10sec      23
#define evBackward1min      24
#define evForward1min       25
#define evIncVolume         26
#define evDecVolume         27
#define evMute              28
#define evIncAudioBufDelay  29
#define evDecAudioBufDelay  30
#define evPlaySwitchToPause 31
#define evPauseSwitchToPlay 32
#define evNormalSize        33
#define evDoubleSize        34

#define evSetMoviePosition  35
#define evSetVolume         36
#define evSetBalance        37

#define evExit              1000

// --- General events ---

#define evFileLoaded      5000
#define evHideMouseCursor 5001
#define evMessageBox      5002
#define evGeneralTimer    5003
#define evGtkIsOk         5004

#define evFName           7000
#define evMovieTime       7001
#define evRedraw          7002
#define evHideWindow      7003
#define evShowWindow      7004

// ----------------------

typedef struct
{
 int    msg;
 char * name;
} evName;

extern int evBoxs;
extern evName evNames[];

#endif
