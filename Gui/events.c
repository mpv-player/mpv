
#include "events.h"

evName evNames[] =
 {
  { evNone,              "evNone"              }, // 1
  { evPlay,              "evPlay"              }, // 2
  { evStop,              "evStop"              }, // 3
  { evPause,             "evPause"             }, // 4
  { evPrev,              "evPrev"              }, // 7
  { evNext,              "evNext"              }, // 8
  { evLoad,              "evLoad"              }, // 9
  { evEqualeaser,        "evEqualeaser"        }, // 10
  { evPlayList,          "evPlaylist"          }, // 11
  { evExit,              "evExit"              }, // 12
  { evPlusVideo,         "evPlusVideo"         }, // 13
  { evMinusVideo,        "evMinusVideo"        }, // 14
  { evIconify,           "evIconify"           }, // 15
  { evPlusBalance,       "evPlusBalance"       }, // 16
  { evMinusBalance,      "evMinusBalance"      }, // 17
  { evFullScreen,        "evFullScreen"        }, // 18
  { evFName,             "evFName"             }, // 19
  { evMovieTime,         "evMovieTime"         }, // 20
  { evAbout,             "evAbout"             }, // 22
  { evLoadPlay,          "evLoadPlay"          }, // 23
  { evPreferences,       "evPreferences"       }, // 24
  { evSkinBrowser,       "evSkinBrowser"       }, // 25
  { evBackward10sec,     "evBackward10sec"     }, // 26
  { evForward10sec,      "evForward10sec"      }, // 27
  { evBackward1min,      "evBackward1min"      }, // 28
  { evForward1min,       "evForward1min"       }, // 29
  { evIncVolume,         "evIncVolume"         }, // 30
  { evDecVolume,         "evDecVolume"         }, // 31
  { evMute,              "evMute"              }, // 32
  { evIncAudioBufDelay,  "evIncAudioBufDelay"  }, // 33
  { evDecAudioBufDelay,  "evDecAudioBufDelay"  }, // 34
  { evPlaySwitchToPause, "evPlaySwitchToPause" }, // 35
  { evPauseSwitchToPlay, "evPauseSwitchToPlay" }, // 36
  { evNormalSize,        "evNormalSize"        }, // 37
  { evDoubleSize,        "evDoubleSize"        }, // 38
  { evSetMoviePosition,  "evSetMoviePosition"  }, // 39
  { evSetVolume,         "evSetVolume"         }, // 40
  { evSetBalance,        "evSetBalance"        }  // 41
 };

const int evBoxs = sizeof( evNames ) / sizeof( evName );
