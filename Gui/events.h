
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
// 44 ...

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
 char * name;
} evName;

extern int evBoxs;
extern evName evNames[];

#endif
