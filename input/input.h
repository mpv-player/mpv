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

#ifndef MPLAYER_INPUT_H
#define MPLAYER_INPUT_H

// All command IDs
typedef enum {
  MP_CMD_SEEK,
  MP_CMD_AUDIO_DELAY,
  MP_CMD_QUIT,
  MP_CMD_PAUSE,
  MP_CMD_GRAB_FRAMES, // deprecated: was a no-op command for years
  MP_CMD_PLAY_TREE_STEP,
  MP_CMD_PLAY_TREE_UP_STEP,
  MP_CMD_PLAY_ALT_SRC_STEP,
  MP_CMD_SUB_DELAY,
  MP_CMD_OSD,
  MP_CMD_VOLUME,
  MP_CMD_MIXER_USEMASTER,
  MP_CMD_CONTRAST,
  MP_CMD_BRIGHTNESS,
  MP_CMD_HUE,
  MP_CMD_SATURATION,
  MP_CMD_FRAMEDROPPING,
  MP_CMD_TV_STEP_CHANNEL,
  MP_CMD_TV_STEP_NORM,
  MP_CMD_TV_STEP_CHANNEL_LIST,
  MP_CMD_VO_FULLSCREEN,
  MP_CMD_SUB_POS,
  MP_CMD_DVDNAV,
  MP_CMD_SCREENSHOT,
  MP_CMD_PANSCAN,
  MP_CMD_MUTE,
  MP_CMD_LOADFILE,
  MP_CMD_LOADLIST,
  MP_CMD_VF_CHANGE_RECTANGLE,
  MP_CMD_GAMMA,
  MP_CMD_SUB_VISIBILITY,
  MP_CMD_VOBSUB_LANG, // deprecated: combined with SUB_SELECT
  MP_CMD_MENU,
  MP_CMD_SET_MENU,
  MP_CMD_GET_TIME_LENGTH,
  MP_CMD_GET_PERCENT_POS,
  MP_CMD_SUB_STEP,
  MP_CMD_TV_SET_CHANNEL,
  MP_CMD_EDL_MARK,
  MP_CMD_SUB_ALIGNMENT,
  MP_CMD_TV_LAST_CHANNEL,
  MP_CMD_OSD_SHOW_TEXT,
  MP_CMD_TV_SET_FREQ,
  MP_CMD_TV_SET_NORM,
  MP_CMD_TV_SET_BRIGHTNESS,
  MP_CMD_TV_SET_CONTRAST,
  MP_CMD_TV_SET_HUE,
  MP_CMD_TV_SET_SATURATION,
  MP_CMD_GET_VO_FULLSCREEN,
  MP_CMD_GET_SUB_VISIBILITY,
  MP_CMD_SUB_FORCED_ONLY,
  MP_CMD_VO_ONTOP,
  MP_CMD_SUB_SELECT,
  MP_CMD_VO_ROOTWIN,
  MP_CMD_SWITCH_VSYNC,
  MP_CMD_SWITCH_RATIO,
  MP_CMD_FRAME_STEP,
  MP_CMD_SPEED_INCR,
  MP_CMD_SPEED_MULT,
  MP_CMD_SPEED_SET,
  MP_CMD_RUN,
  MP_CMD_SUB_LOG,
  MP_CMD_SWITCH_AUDIO,
  MP_CMD_GET_TIME_POS,
  MP_CMD_SUB_LOAD,
  MP_CMD_SUB_REMOVE,
  MP_CMD_KEYDOWN_EVENTS,
  MP_CMD_VO_BORDER,
  MP_CMD_SET_PROPERTY,
  MP_CMD_GET_PROPERTY,
  MP_CMD_OSD_SHOW_PROPERTY_TEXT,
  MP_CMD_SEEK_CHAPTER,
  MP_CMD_FILE_FILTER,
  MP_CMD_GET_FILENAME,
  MP_CMD_GET_VIDEO_CODEC,
  MP_CMD_GET_VIDEO_BITRATE,
  MP_CMD_GET_VIDEO_RESOLUTION,
  MP_CMD_GET_AUDIO_CODEC,
  MP_CMD_GET_AUDIO_BITRATE,
  MP_CMD_GET_AUDIO_SAMPLES,
  MP_CMD_GET_META_TITLE,
  MP_CMD_GET_META_ARTIST,
  MP_CMD_GET_META_ALBUM,
  MP_CMD_GET_META_YEAR,
  MP_CMD_GET_META_COMMENT,
  MP_CMD_GET_META_TRACK,
  MP_CMD_GET_META_GENRE,
  MP_CMD_RADIO_STEP_CHANNEL,
  MP_CMD_RADIO_SET_CHANNEL,
  MP_CMD_RADIO_SET_FREQ,
  MP_CMD_SET_MOUSE_POS,
  MP_CMD_STEP_PROPERTY,
  MP_CMD_RADIO_STEP_FREQ,
  MP_CMD_TV_STEP_FREQ,
  MP_CMD_LOOP,
  MP_CMD_BALANCE,
  MP_CMD_SUB_SCALE,
  MP_CMD_TV_TELETEXT_ADD_DEC,
  MP_CMD_TV_TELETEXT_GO_LINK,
  MP_CMD_TV_START_SCAN,
  MP_CMD_SUB_SOURCE,
  MP_CMD_SUB_FILE,
  MP_CMD_SUB_VOB,
  MP_CMD_SUB_DEMUX,
  MP_CMD_SWITCH_ANGLE,
  MP_CMD_ASS_USE_MARGINS,
  MP_CMD_SWITCH_TITLE,
  MP_CMD_STOP,

  /// DVDNAV commands
  MP_CMD_DVDNAV_UP = 1000,
  MP_CMD_DVDNAV_DOWN,
  MP_CMD_DVDNAV_LEFT,
  MP_CMD_DVDNAV_RIGHT,
  MP_CMD_DVDNAV_MENU,
  MP_CMD_DVDNAV_SELECT,
  MP_CMD_DVDNAV_PREVMENU,
  MP_CMD_DVDNAV_MOUSECLICK,
  
  /// GUI commands
  MP_CMD_GUI_EVENTS = 5000,
  MP_CMD_GUI_LOADFILE,
  MP_CMD_GUI_LOADSUBTITLE,
  MP_CMD_GUI_ABOUT,
  MP_CMD_GUI_PLAY,
  MP_CMD_GUI_STOP,
  MP_CMD_GUI_PLAYLIST,
  MP_CMD_GUI_PREFERENCES,
  MP_CMD_GUI_FULLSCREEN,
  MP_CMD_GUI_SKINBROWSER,

  /// DVB commands
  MP_CMD_DVB_SET_CHANNEL = 5101,

  /// Console commands
  MP_CMD_CHELP = 7000,
  MP_CMD_CEXIT,
  MP_CMD_CHIDE,
} mp_command_type;

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
extern int (*mp_input_key_cb)(int code);
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

int mp_input_add_event_fd(int fd, void (*read_func)(void));

void mp_input_rm_event_fd(int fd);

/// Get input key from its name.
int mp_input_get_key_from_name(const char *name);

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

/**
 * Parse and queue commands separated by '\n'.
 * @return count of commands new queued.
 */
int mp_input_parse_and_queue_cmds(const char *str);

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

// Set current input section
void
mp_input_set_section(char *name);

// Get current input section
char*
mp_input_get_section(void);

// When you create a new driver you should add it in these 2 functions.
void
mp_input_init(int use_gui);

void
mp_input_uninit(void);

// Interruptible usleep:  (used by libmpdemux)
int
mp_input_check_interrupt(int time);

extern int async_quit_request;

#endif /* MPLAYER_INPUT_H */
