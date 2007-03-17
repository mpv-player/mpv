// All command IDs
#define MP_CMD_SEEK   0
#define MP_CMD_AUDIO_DELAY 1
#define MP_CMD_QUIT 2
#define MP_CMD_PAUSE 3
// #define MP_CMD_GRAB_FRAMES 4  // was a no-op command for years
#define MP_CMD_PLAY_TREE_STEP 5
#define MP_CMD_PLAY_TREE_UP_STEP 6
#define MP_CMD_PLAY_ALT_SRC_STEP 7
#define MP_CMD_SUB_DELAY 8
#define MP_CMD_OSD 9
#define MP_CMD_VOLUME 10
#define MP_CMD_MIXER_USEMASTER 11
#define MP_CMD_CONTRAST 12
#define MP_CMD_BRIGHTNESS 13
#define MP_CMD_HUE 14
#define MP_CMD_SATURATION 15
#define MP_CMD_FRAMEDROPPING 16
#define MP_CMD_TV_STEP_CHANNEL 17
#define MP_CMD_TV_STEP_NORM 18
#define MP_CMD_TV_STEP_CHANNEL_LIST 19
#define MP_CMD_VO_FULLSCREEN 20
#define MP_CMD_SUB_POS 21
#define MP_CMD_DVDNAV 22
#define MP_CMD_SCREENSHOT 23
#define MP_CMD_PANSCAN 24
#define MP_CMD_MUTE 25
#define MP_CMD_LOADFILE 26
#define MP_CMD_LOADLIST 27
#define MP_CMD_VF_CHANGE_RECTANGLE 28
#define MP_CMD_GAMMA 29
#define MP_CMD_SUB_VISIBILITY 30
// #define MP_CMD_VOBSUB_LANG 31 // combined with SUB_SELECT
#define MP_CMD_MENU 32
#define MP_CMD_SET_MENU 33
#define MP_CMD_GET_TIME_LENGTH 34
#define MP_CMD_GET_PERCENT_POS 35
#define MP_CMD_SUB_STEP 36
#define MP_CMD_TV_SET_CHANNEL 37
#define MP_CMD_EDL_MARK 38
#define MP_CMD_SUB_ALIGNMENT 39
#define MP_CMD_TV_LAST_CHANNEL 40
#define MP_CMD_OSD_SHOW_TEXT 41
#define MP_CMD_TV_SET_FREQ 42
#define MP_CMD_TV_SET_NORM 43
#define MP_CMD_TV_SET_BRIGHTNESS 44
#define MP_CMD_TV_SET_CONTRAST 45
#define MP_CMD_TV_SET_HUE 46
#define MP_CMD_TV_SET_SATURATION 47
#define MP_CMD_GET_VO_FULLSCREEN 48
#define MP_CMD_GET_SUB_VISIBILITY 49
#define MP_CMD_SUB_FORCED_ONLY 50
#define MP_CMD_VO_ONTOP 51
#define MP_CMD_SUB_SELECT 52
#define MP_CMD_VO_ROOTWIN 53
#define MP_CMD_SWITCH_VSYNC 54
#define MP_CMD_SWITCH_RATIO 55
#define MP_CMD_FRAME_STEP 56
#define MP_CMD_SPEED_INCR 57
#define MP_CMD_SPEED_MULT 58
#define MP_CMD_SPEED_SET 59
#define MP_CMD_RUN 60
#define MP_CMD_SUB_LOG 61
#define MP_CMD_SWITCH_AUDIO 62
#define MP_CMD_GET_TIME_POS 63
#define MP_CMD_SUB_LOAD 64
#define MP_CMD_SUB_REMOVE 65
#define MP_CMD_KEYDOWN_EVENTS 66
#define MP_CMD_VO_BORDER 67
#define MP_CMD_SET_PROPERTY 68
#define MP_CMD_GET_PROPERTY 69
#define MP_CMD_OSD_SHOW_PROPERTY_TEXT 70
#define MP_CMD_SEEK_CHAPTER 71
#define MP_CMD_FILE_FILTER 72
#define MP_CMD_GET_FILENAME 73
#define MP_CMD_GET_VIDEO_CODEC 74
#define MP_CMD_GET_VIDEO_BITRATE 75
#define MP_CMD_GET_VIDEO_RESOLUTION 76
#define MP_CMD_GET_AUDIO_CODEC 77
#define MP_CMD_GET_AUDIO_BITRATE 78
#define MP_CMD_GET_AUDIO_SAMPLES 79
#define MP_CMD_GET_META_TITLE 80
#define MP_CMD_GET_META_ARTIST 81
#define MP_CMD_GET_META_ALBUM 82
#define MP_CMD_GET_META_YEAR 83
#define MP_CMD_GET_META_COMMENT 84
#define MP_CMD_GET_META_TRACK 85
#define MP_CMD_GET_META_GENRE 86
#define MP_CMD_RADIO_STEP_CHANNEL 87
#define MP_CMD_RADIO_SET_CHANNEL 88
#define MP_CMD_RADIO_SET_FREQ 89
#define MP_CMD_SET_MOUSE_POS 90
#define MP_CMD_STEP_PROPERTY 91
#define MP_CMD_RADIO_STEP_FREQ 92
#define MP_CMD_TV_STEP_FREQ 93

#define MP_CMD_GUI_EVENTS       5000
#define MP_CMD_GUI_LOADFILE     5001
#define MP_CMD_GUI_LOADSUBTITLE 5002
#define MP_CMD_GUI_ABOUT        5003
#define MP_CMD_GUI_PLAY         5004
#define MP_CMD_GUI_STOP         5005
#define MP_CMD_GUI_PLAYLIST     5006
#define MP_CMD_GUI_PREFERENCES  5007
#define MP_CMD_GUI_FULLSCREEN   5008
#define MP_CMD_GUI_SKINBROWSER  5009

#ifdef HAS_DVBIN_SUPPORT
#define MP_CMD_DVB_SET_CHANNEL 5101
#endif

#define MP_CMD_DVDNAV_UP        1
#define MP_CMD_DVDNAV_DOWN      2
#define MP_CMD_DVDNAV_LEFT      3
#define MP_CMD_DVDNAV_RIGHT     4
#define MP_CMD_DVDNAV_MENU      5
#define MP_CMD_DVDNAV_SELECT    6
#define MP_CMD_DVDNAV_PREVMENU  7
#define MP_CMD_DVDNAV_MOUSECLICK  8

/// Console commands
#define MP_CMD_CHELP 7000
#define MP_CMD_CEXIT 7001
#define MP_CMD_CHIDE 7002

// The arg types
#define MP_CMD_ARG_INT 0
#define MP_CMD_ARG_FLOAT 1
#define MP_CMD_ARG_STRING 2
#define MP_CMD_ARG_VOID 3

#ifndef MP_CMD_MAX_ARGS
#define MP_CMD_MAX_ARGS 10
#endif

// Error codes for the drivers

// An error occurred but we can continue
#define MP_INPUT_ERROR -1
// A fatal error occurred, this driver should be removed
#define MP_INPUT_DEAD -2
// No input was available
#define MP_INPUT_NOTHING -3
//! Input will be available if you try again
#define MP_INPUT_RETRY -4

// For the key's drivers, if possible you can send key up and key down
// events. Key up is the default, to send a key down you must use the 
// OR operator between the key code and MP_KEY_DOWN.
#define MP_KEY_DOWN (1<<29)
// Use this when the key shouldn't be auto-repeated (like mouse buttons)
#define MP_NO_REPEAT_KEY (1<<28)

#ifndef MP_MAX_KEY_DOWN
#define MP_MAX_KEY_DOWN 32
#endif

typedef union mp_cmd_arg_value {
  int i;
  float f;
  char* s;
  void* v;
} mp_cmd_arg_value_t;

typedef struct mp_cmd_arg {
  int type;
  mp_cmd_arg_value_t v;
} mp_cmd_arg_t;

typedef struct mp_cmd {
  int id;
  char* name;
  int nargs;
  mp_cmd_arg_t args[MP_CMD_MAX_ARGS];
  int pausing;
} mp_cmd_t;


typedef struct mp_cmd_bind {
  int input[MP_MAX_KEY_DOWN+1];
  char* cmd;
} mp_cmd_bind_t;

typedef struct mp_key_name {
  int key;
  char* name;
} mp_key_name_t;

// These typedefs are for the drivers. They are the functions used to retrieve
// the next key code or command.

// These functions should return the key code or one of the error codes
typedef int (*mp_key_func_t)(int fd);
// These functions should act like read but they must use our error code (if needed ;-)
typedef int (*mp_cmd_func_t)(int fd,char* dest,int size);
// These are used to close the driver
typedef void (*mp_close_func_t)(int fd);

// Set this to grab all incoming key codes
extern void (*mp_input_key_cb)(int code);
// Should return 1 if the command was processed
typedef int (*mp_input_cmd_filter)(mp_cmd_t* cmd, int paused, void* ctx);

// This function adds a new key driver.
// The first arg is a file descriptor (use a negative value if you don't use any fd)
// The second arg tells if we use select on the fd to know if something is available.
// The third arg is optional. If null a default function wich reads an int from the
// fd will be used.
// The last arg can be NULL if nothing is needed to close the driver. The close
// function can be used
int
mp_input_add_cmd_fd(int fd, int select, mp_cmd_func_t read_func, mp_close_func_t close_func);

// This removes a cmd driver, you usually don't need to use it.
void
mp_input_rm_cmd_fd(int fd);

// The args are the same as for the key's drivers. If you don't use any valid fd you MUST
// give a read_func.
int
mp_input_add_key_fd(int fd, int select, mp_key_func_t read_func, mp_close_func_t close_func);

// As for the cmd one you usually don't need this function.
void
mp_input_rm_key_fd(int fd);

// This function can be used to put a command in the system again. It's used by libmpdemux
// when it performs a blocking operation to resend the command it received to the main
// loop.
int
mp_input_queue_cmd(mp_cmd_t* cmd);

// This function retrieves the next available command waiting no more than time msec.
// If pause is true, the next input will always return a pause command.
mp_cmd_t*
mp_input_get_cmd(int time, int paused, int peek_only);

mp_cmd_t*
mp_input_parse_cmd(char* str);

/// These filters allow you to process the command before MPlayer.
/// If a filter returns a true value mp_input_get_cmd will return NULL.
void
mp_input_add_cmd_filter(mp_input_cmd_filter, void* ctx);

// After getting a command from mp_input_get_cmd you need to free it using this
// function
void
mp_cmd_free(mp_cmd_t* cmd);

// This creates a copy of a command (used by the auto repeat stuff).
mp_cmd_t*
mp_cmd_clone(mp_cmd_t* cmd);

// When you create a new driver you should add it in these 2 functions.
void
mp_input_init(int use_gui);

void
mp_input_uninit(void);

// Interruptible usleep:  (used by libmpdemux)
int
mp_input_check_interrupt(int time);

