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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>

#include "osdep/io.h"

#include "input.h"
#include "mp_fifo.h"
#include "keycodes.h"
#include "osdep/timer.h"
#include "libavutil/avstring.h"
#include "libavutil/common.h"
#include "mp_msg.h"
#include "m_config.h"
#include "m_option.h"
#include "path.h"
#include "talloc.h"
#include "options.h"
#include "bstr.h"
#include "stream/stream.h"

#include "joystick.h"

#ifdef CONFIG_LIRC
#include "lirc.h"
#endif

#ifdef CONFIG_LIRCC
#include <lirc/lircc.h>
#endif

#include "ar.h"

#ifdef CONFIG_COCOA
#include "osdep/cocoa_events.h"
#endif

#define MP_MAX_KEY_DOWN 32

struct cmd_bind {
    int input[MP_MAX_KEY_DOWN + 1];
    char *cmd;
};

struct key_name {
    int key;
    char *name;
};

/* This array defines all known commands.
 * The first field is an id used to recognize the command.
 * The second is the command name used in slave mode and input.conf.
 * Then comes the definition of each argument, first mandatory arguments
 * (ARG_INT, ARG_FLOAT, ARG_STRING) if any, then optional arguments
 * (OARG_INT(default), etc) if any. The command will be given the default
 * argument value if the user didn't give enough arguments to specify it.
 * A command can take a maximum of MP_CMD_MAX_ARGS arguments (10).
 */
#define ARG_INT { .type = MP_CMD_ARG_INT }
#define OARG_INT(def) { .type = MP_CMD_ARG_INT, .optional = true, .v.i = def }
#define ARG_FLOAT { .type = MP_CMD_ARG_FLOAT }
#define OARG_FLOAT(def) { .type = MP_CMD_ARG_FLOAT, .optional = true, .v.f = def }
#define ARG_STRING { .type = MP_CMD_ARG_STRING }
#define OARG_STRING(def) { .type = MP_CMD_ARG_STRING, .optional = true, .v.s = def }

static const mp_cmd_t mp_cmds[] = {
#ifdef CONFIG_RADIO
  { MP_CMD_RADIO_STEP_CHANNEL, "radio_step_channel", { ARG_INT } },
  { MP_CMD_RADIO_SET_CHANNEL, "radio_set_channel", { ARG_STRING } },
  { MP_CMD_RADIO_SET_FREQ, "radio_set_freq", { ARG_FLOAT } },
  { MP_CMD_RADIO_STEP_FREQ, "radio_step_freq", {ARG_FLOAT } },
#endif
  { MP_CMD_SEEK, "seek", { ARG_FLOAT, OARG_INT(0), OARG_INT(0) } },
  { MP_CMD_EDL_MARK, "edl_mark", },
  { MP_CMD_AUDIO_DELAY, "audio_delay", { ARG_FLOAT, OARG_INT(0) } },
  { MP_CMD_SPEED_INCR, "speed_incr", { ARG_FLOAT } },
  { MP_CMD_SPEED_MULT, "speed_mult", { ARG_FLOAT } },
  { MP_CMD_SPEED_SET, "speed_set", { ARG_FLOAT } },
  { MP_CMD_QUIT, "quit", { OARG_INT(0) } },
  { MP_CMD_STOP, "stop", },
  { MP_CMD_PAUSE, "pause", },
  { MP_CMD_FRAME_STEP, "frame_step", },
  { MP_CMD_PLAYLIST_NEXT, "playlist_next", { OARG_INT(0) } },
  { MP_CMD_PLAYLIST_PREV, "playlist_prev", { OARG_INT(0) } },
  { MP_CMD_LOOP, "loop", { ARG_INT, OARG_INT(0) } },
  { MP_CMD_SUB_DELAY, "sub_delay", { ARG_FLOAT, OARG_INT(0) } },
  { MP_CMD_SUB_STEP, "sub_step", { ARG_INT, OARG_INT(0) } },
  { MP_CMD_OSD, "osd", { OARG_INT(-1) } },
  { MP_CMD_OSD_SHOW_TEXT, "osd_show_text", { ARG_STRING, OARG_INT(-1), OARG_INT(0) } },
  { MP_CMD_OSD_SHOW_PROPERTY_TEXT, "osd_show_property_text", { ARG_STRING, OARG_INT(-1), OARG_INT(0) } },
  { MP_CMD_OSD_SHOW_PROGRESSION, "osd_show_progression", },
  { MP_CMD_VOLUME, "volume", { ARG_FLOAT, OARG_INT(0) } },
  { MP_CMD_BALANCE, "balance", { ARG_FLOAT, OARG_INT(0) } },
  { MP_CMD_MIXER_USEMASTER, "use_master", },
  { MP_CMD_MUTE, "mute", { OARG_INT(-1) } },
  { MP_CMD_CONTRAST, "contrast", { ARG_INT, OARG_INT(0) } },
  { MP_CMD_GAMMA, "gamma", { ARG_INT, OARG_INT(0) }  },
  { MP_CMD_BRIGHTNESS, "brightness", { ARG_INT, OARG_INT(0) }  },
  { MP_CMD_HUE, "hue", { ARG_INT, OARG_INT(0) } },
  { MP_CMD_SATURATION, "saturation", { ARG_INT, OARG_INT(0) }  },
  { MP_CMD_FRAMEDROPPING, "frame_drop", { OARG_INT(-1) } },
  { MP_CMD_SUB_POS, "sub_pos", { ARG_INT, OARG_INT(0) } },
  { MP_CMD_SUB_ALIGNMENT, "sub_alignment", { OARG_INT(-1) } },
  { MP_CMD_SUB_VISIBILITY, "sub_visibility", { OARG_INT(-1) } },
  { MP_CMD_SUB_LOAD, "sub_load", { ARG_STRING } },
  { MP_CMD_SUB_SELECT, "vobsub_lang", { OARG_INT(-2) } }, // for compatibility
  { MP_CMD_SUB_SELECT, "sub_select", { OARG_INT(-2) } },
  { MP_CMD_SUB_SCALE, "sub_scale", { ARG_FLOAT, OARG_INT(0) } },
#ifdef CONFIG_ASS
  { MP_CMD_ASS_USE_MARGINS, "ass_use_margins", { OARG_INT(-1) } },
#endif
  { MP_CMD_GET_PERCENT_POS, "get_percent_pos", },
  { MP_CMD_GET_TIME_POS, "get_time_pos", },
  { MP_CMD_GET_TIME_LENGTH, "get_time_length", },
  { MP_CMD_GET_FILENAME, "get_file_name", },
  { MP_CMD_GET_VIDEO_CODEC, "get_video_codec", },
  { MP_CMD_GET_VIDEO_BITRATE, "get_video_bitrate", },
  { MP_CMD_GET_VIDEO_RESOLUTION, "get_video_resolution", },
  { MP_CMD_GET_AUDIO_CODEC, "get_audio_codec", },
  { MP_CMD_GET_AUDIO_BITRATE, "get_audio_bitrate", },
  { MP_CMD_GET_AUDIO_SAMPLES, "get_audio_samples", },
  { MP_CMD_GET_META_TITLE, "get_meta_title", },
  { MP_CMD_GET_META_ARTIST, "get_meta_artist", },
  { MP_CMD_GET_META_ALBUM, "get_meta_album", },
  { MP_CMD_GET_META_YEAR, "get_meta_year", },
  { MP_CMD_GET_META_COMMENT, "get_meta_comment", },
  { MP_CMD_GET_META_TRACK, "get_meta_track", },
  { MP_CMD_GET_META_GENRE, "get_meta_genre", },
  { MP_CMD_SWITCH_AUDIO, "switch_audio", { OARG_INT(-1) } },
  { MP_CMD_SWITCH_ANGLE, "switch_angle", { OARG_INT(-1) } },
  { MP_CMD_SWITCH_TITLE, "switch_title", { OARG_INT(-1) } },
#ifdef CONFIG_TV
  { MP_CMD_TV_START_SCAN, "tv_start_scan", },
  { MP_CMD_TV_STEP_CHANNEL, "tv_step_channel", { ARG_INT } },
  { MP_CMD_TV_STEP_NORM, "tv_step_norm", },
  { MP_CMD_TV_STEP_CHANNEL_LIST, "tv_step_chanlist", },
  { MP_CMD_TV_SET_CHANNEL, "tv_set_channel", { ARG_STRING } },
  { MP_CMD_TV_LAST_CHANNEL, "tv_last_channel", },
  { MP_CMD_TV_SET_FREQ, "tv_set_freq", { ARG_FLOAT } },
  { MP_CMD_TV_STEP_FREQ, "tv_step_freq", { ARG_FLOAT } },
  { MP_CMD_TV_SET_NORM, "tv_set_norm", { ARG_STRING } },
  { MP_CMD_TV_SET_BRIGHTNESS, "tv_set_brightness", { ARG_INT, OARG_INT(1) } },
  { MP_CMD_TV_SET_CONTRAST, "tv_set_contrast", { ARG_INT, OARG_INT(1) } },
  { MP_CMD_TV_SET_HUE, "tv_set_hue", { ARG_INT, OARG_INT(1) } },
  { MP_CMD_TV_SET_SATURATION, "tv_set_saturation", { ARG_INT, OARG_INT(1) } },
#endif
  { MP_CMD_SUB_FORCED_ONLY, "forced_subs_only", { OARG_INT(-1) } },
#ifdef CONFIG_DVBIN
  { MP_CMD_DVB_SET_CHANNEL, "dvb_set_channel", { ARG_INT, ARG_INT } },
#endif
  { MP_CMD_SWITCH_RATIO, "switch_ratio", { OARG_FLOAT(0) } },
  { MP_CMD_VO_FULLSCREEN, "vo_fullscreen", { OARG_INT(-1) } },
  { MP_CMD_VO_ONTOP, "vo_ontop", { OARG_INT(-1) } },
  { MP_CMD_VO_ROOTWIN, "vo_rootwin", { OARG_INT(-1) } },
  { MP_CMD_VO_BORDER, "vo_border", { OARG_INT(-1) } },
  { MP_CMD_SCREENSHOT, "screenshot", { OARG_INT(0), OARG_INT(0) } },
  { MP_CMD_PANSCAN, "panscan", { ARG_FLOAT, OARG_INT(0) } },
  { MP_CMD_SWITCH_VSYNC, "switch_vsync", { OARG_INT(0) } },
  { MP_CMD_LOADFILE, "loadfile", { ARG_STRING, OARG_INT(0) } },
  { MP_CMD_LOADLIST, "loadlist", { ARG_STRING, OARG_INT(0) } },
  { MP_CMD_PLAYLIST_CLEAR, "playlist_clear", },
  { MP_CMD_RUN, "run", { ARG_STRING } },

  { MP_CMD_GET_VO_FULLSCREEN, "get_vo_fullscreen", },
  { MP_CMD_GET_SUB_VISIBILITY, "get_sub_visibility", },
  { MP_CMD_KEYDOWN_EVENTS, "key_down_event", { ARG_INT } },
  { MP_CMD_SET_PROPERTY, "set_property", { ARG_STRING,  ARG_STRING } },
  { MP_CMD_SET_PROPERTY_OSD, "set_property_osd", { ARG_STRING,  ARG_STRING } },
  { MP_CMD_GET_PROPERTY, "get_property", { ARG_STRING } },
  { MP_CMD_STEP_PROPERTY, "step_property", { ARG_STRING, OARG_FLOAT(0), OARG_INT(0) } },
  { MP_CMD_STEP_PROPERTY_OSD, "step_property_osd", { ARG_STRING, OARG_FLOAT(0), OARG_INT(0) } },

  { MP_CMD_SEEK_CHAPTER, "seek_chapter", { ARG_INT, OARG_INT(0) } },
  { MP_CMD_SET_MOUSE_POS, "set_mouse_pos", { ARG_INT, ARG_INT } },

  { MP_CMD_AF_SWITCH, "af_switch", { ARG_STRING } },
  { MP_CMD_AF_ADD, "af_add", { ARG_STRING } },
  { MP_CMD_AF_DEL, "af_del", { ARG_STRING } },
  { MP_CMD_AF_CLR, "af_clr", },
  { MP_CMD_AF_CMDLINE, "af_cmdline", { ARG_STRING, ARG_STRING } },

  { MP_CMD_SHOW_CHAPTERS, "show_chapters_osd", },
  { MP_CMD_SHOW_TRACKS, "show_tracks_osd", },

  { MP_CMD_VO_CMDLINE, "vo_cmdline", { ARG_STRING } },

  {0}
};

/// The names of the keys as used in input.conf
/// If you add some new keys, you also need to add them here

static const struct key_name key_names[] = {
  { ' ', "SPACE" },
  { '#', "SHARP" },
  { KEY_ENTER, "ENTER" },
  { KEY_TAB, "TAB" },
  { KEY_BACKSPACE, "BS" },
  { KEY_DELETE, "DEL" },
  { KEY_INSERT, "INS" },
  { KEY_HOME, "HOME" },
  { KEY_END, "END" },
  { KEY_PAGE_UP, "PGUP" },
  { KEY_PAGE_DOWN, "PGDWN" },
  { KEY_ESC, "ESC" },
  { KEY_PRINT, "PRINT" },
  { KEY_RIGHT, "RIGHT" },
  { KEY_LEFT, "LEFT" },
  { KEY_DOWN, "DOWN" },
  { KEY_UP, "UP" },
  { KEY_F+1, "F1" },
  { KEY_F+2, "F2" },
  { KEY_F+3, "F3" },
  { KEY_F+4, "F4" },
  { KEY_F+5, "F5" },
  { KEY_F+6, "F6" },
  { KEY_F+7, "F7" },
  { KEY_F+8, "F8" },
  { KEY_F+9, "F9" },
  { KEY_F+10, "F10" },
  { KEY_F+11, "F11" },
  { KEY_F+12, "F12" },
  { KEY_KP0, "KP0" },
  { KEY_KP1, "KP1" },
  { KEY_KP2, "KP2" },
  { KEY_KP3, "KP3" },
  { KEY_KP4, "KP4" },
  { KEY_KP5, "KP5" },
  { KEY_KP6, "KP6" },
  { KEY_KP7, "KP7" },
  { KEY_KP8, "KP8" },
  { KEY_KP9, "KP9" },
  { KEY_KPDEL, "KP_DEL" },
  { KEY_KPDEC, "KP_DEC" },
  { KEY_KPINS, "KP_INS" },
  { KEY_KPENTER, "KP_ENTER" },
  { MOUSE_BTN0, "MOUSE_BTN0" },
  { MOUSE_BTN1, "MOUSE_BTN1" },
  { MOUSE_BTN2, "MOUSE_BTN2" },
  { MOUSE_BTN3, "MOUSE_BTN3" },
  { MOUSE_BTN4, "MOUSE_BTN4" },
  { MOUSE_BTN5, "MOUSE_BTN5" },
  { MOUSE_BTN6, "MOUSE_BTN6" },
  { MOUSE_BTN7, "MOUSE_BTN7" },
  { MOUSE_BTN8, "MOUSE_BTN8" },
  { MOUSE_BTN9, "MOUSE_BTN9" },
  { MOUSE_BTN10, "MOUSE_BTN10" },
  { MOUSE_BTN11, "MOUSE_BTN11" },
  { MOUSE_BTN12, "MOUSE_BTN12" },
  { MOUSE_BTN13, "MOUSE_BTN13" },
  { MOUSE_BTN14, "MOUSE_BTN14" },
  { MOUSE_BTN15, "MOUSE_BTN15" },
  { MOUSE_BTN16, "MOUSE_BTN16" },
  { MOUSE_BTN17, "MOUSE_BTN17" },
  { MOUSE_BTN18, "MOUSE_BTN18" },
  { MOUSE_BTN19, "MOUSE_BTN19" },
  { MOUSE_BTN0_DBL, "MOUSE_BTN0_DBL" },
  { MOUSE_BTN1_DBL, "MOUSE_BTN1_DBL" },
  { MOUSE_BTN2_DBL, "MOUSE_BTN2_DBL" },
  { MOUSE_BTN3_DBL, "MOUSE_BTN3_DBL" },
  { MOUSE_BTN4_DBL, "MOUSE_BTN4_DBL" },
  { MOUSE_BTN5_DBL, "MOUSE_BTN5_DBL" },
  { MOUSE_BTN6_DBL, "MOUSE_BTN6_DBL" },
  { MOUSE_BTN7_DBL, "MOUSE_BTN7_DBL" },
  { MOUSE_BTN8_DBL, "MOUSE_BTN8_DBL" },
  { MOUSE_BTN9_DBL, "MOUSE_BTN9_DBL" },
  { MOUSE_BTN10_DBL, "MOUSE_BTN10_DBL" },
  { MOUSE_BTN11_DBL, "MOUSE_BTN11_DBL" },
  { MOUSE_BTN12_DBL, "MOUSE_BTN12_DBL" },
  { MOUSE_BTN13_DBL, "MOUSE_BTN13_DBL" },
  { MOUSE_BTN14_DBL, "MOUSE_BTN14_DBL" },
  { MOUSE_BTN15_DBL, "MOUSE_BTN15_DBL" },
  { MOUSE_BTN16_DBL, "MOUSE_BTN16_DBL" },
  { MOUSE_BTN17_DBL, "MOUSE_BTN17_DBL" },
  { MOUSE_BTN18_DBL, "MOUSE_BTN18_DBL" },
  { MOUSE_BTN19_DBL, "MOUSE_BTN19_DBL" },
  { JOY_AXIS1_MINUS, "JOY_UP" },
  { JOY_AXIS1_PLUS, "JOY_DOWN" },
  { JOY_AXIS0_MINUS, "JOY_LEFT" },
  { JOY_AXIS0_PLUS, "JOY_RIGHT" },

  { JOY_AXIS0_PLUS, "JOY_AXIS0_PLUS" },
  { JOY_AXIS0_MINUS, "JOY_AXIS0_MINUS" },
  { JOY_AXIS1_PLUS, "JOY_AXIS1_PLUS" },
  { JOY_AXIS1_MINUS, "JOY_AXIS1_MINUS" },
  { JOY_AXIS2_PLUS, "JOY_AXIS2_PLUS" },
  { JOY_AXIS2_MINUS, "JOY_AXIS2_MINUS" },
  { JOY_AXIS3_PLUS, "JOY_AXIS3_PLUS" },
  { JOY_AXIS3_MINUS, "JOY_AXIS3_MINUS" },
  { JOY_AXIS4_PLUS, "JOY_AXIS4_PLUS" },
  { JOY_AXIS4_MINUS, "JOY_AXIS4_MINUS" },
  { JOY_AXIS5_PLUS, "JOY_AXIS5_PLUS" },
  { JOY_AXIS5_MINUS, "JOY_AXIS5_MINUS" },
  { JOY_AXIS6_PLUS, "JOY_AXIS6_PLUS" },
  { JOY_AXIS6_MINUS, "JOY_AXIS6_MINUS" },
  { JOY_AXIS7_PLUS, "JOY_AXIS7_PLUS" },
  { JOY_AXIS7_MINUS, "JOY_AXIS7_MINUS" },
  { JOY_AXIS8_PLUS, "JOY_AXIS8_PLUS" },
  { JOY_AXIS8_MINUS, "JOY_AXIS8_MINUS" },
  { JOY_AXIS9_PLUS, "JOY_AXIS9_PLUS" },
  { JOY_AXIS9_MINUS, "JOY_AXIS9_MINUS" },

  { JOY_BTN0, "JOY_BTN0" },
  { JOY_BTN1, "JOY_BTN1" },
  { JOY_BTN2, "JOY_BTN2" },
  { JOY_BTN3, "JOY_BTN3" },
  { JOY_BTN4, "JOY_BTN4" },
  { JOY_BTN5, "JOY_BTN5" },
  { JOY_BTN6, "JOY_BTN6" },
  { JOY_BTN7, "JOY_BTN7" },
  { JOY_BTN8, "JOY_BTN8" },
  { JOY_BTN9, "JOY_BTN9" },

  { AR_PLAY, "AR_PLAY" },
  { AR_PLAY_HOLD, "AR_PLAY_HOLD" },
  { AR_NEXT, "AR_NEXT" },
  { AR_NEXT_HOLD, "AR_NEXT_HOLD" },
  { AR_PREV, "AR_PREV" },
  { AR_PREV_HOLD, "AR_PREV_HOLD" },
  { AR_MENU, "AR_MENU" },
  { AR_MENU_HOLD, "AR_MENU_HOLD" },
  { AR_VUP, "AR_VUP" },
  { AR_VDOWN, "AR_VDOWN" },

  { KEY_POWER, "POWER" },
  { KEY_MENU, "MENU" },
  { KEY_PLAY, "PLAY" },
  { KEY_PAUSE, "PAUSE" },
  { KEY_PLAYPAUSE, "PLAYPAUSE" },
  { KEY_STOP, "STOP" },
  { KEY_FORWARD, "FORWARD" },
  { KEY_REWIND, "REWIND" },
  { KEY_NEXT, "NEXT" },
  { KEY_PREV, "PREV" },
  { KEY_VOLUME_UP, "VOLUME_UP" },
  { KEY_VOLUME_DOWN, "VOLUME_DOWN" },
  { KEY_MUTE, "MUTE" },

  // These are kept for backward compatibility
  { KEY_PAUSE, "XF86_PAUSE" },
  { KEY_STOP, "XF86_STOP" },
  { KEY_PREV, "XF86_PREV" },
  { KEY_NEXT, "XF86_NEXT" },

  { KEY_CLOSE_WIN, "CLOSE_WIN" },

  { 0, NULL }
};

struct key_name modifier_names[] = {
    { KEY_MODIFIER_SHIFT, "Shift" },
    { KEY_MODIFIER_CTRL,  "Ctrl" },
    { KEY_MODIFIER_ALT,   "Alt" },
    { KEY_MODIFIER_META,  "Meta" },
    { 0 }
};

#define KEY_MODIFIER_MASK (KEY_MODIFIER_SHIFT | KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT | KEY_MODIFIER_META)

#ifndef MP_MAX_KEY_FD
#define MP_MAX_KEY_FD 10
#endif

#ifndef MP_MAX_CMD_FD
#define MP_MAX_CMD_FD 10
#endif

struct input_fd {
    int fd;
    union {
        int (*key)(void *ctx, int fd);
        int (*cmd)(int fd, char *dest, int size);
    } read_func;
    int (*close_func)(int fd);
    void *ctx;
    unsigned eof : 1;
    unsigned drop : 1;
    unsigned dead : 1;
    unsigned got_cmd : 1;
    unsigned no_select : 1;
    // These fields are for the cmd fds.
    char *buffer;
    int pos, size;
};

struct cmd_bind_section {
    struct cmd_bind *cmd_binds;
    bool is_builtin;
    char *section;
    struct cmd_bind_section *next;
};

struct cmd_queue {
    struct mp_cmd *first;
};

struct input_ctx {
    // Autorepeat stuff
    short ar_state;
    mp_cmd_t *ar_cmd;
    unsigned int last_ar;
    // Autorepeat config
    unsigned int ar_delay;
    unsigned int ar_rate;
    // Maximum number of queued commands from keypresses (limit to avoid
    // repeated slow commands piling up)
    int key_fifo_size;

    // these are the keys currently down
    int key_down[MP_MAX_KEY_DOWN];
    unsigned int num_key_down;
    unsigned int last_key_down;

    bool default_bindings;
    // List of command binding sections
    struct cmd_bind_section *cmd_bind_sections;
    // Name of currently used command section
    char *section;
    // Bitfield of mp_input_section_flags
    int section_flags;

    // Used to track whether we managed to read something while checking
    // events sources. If yes, the sources may have more queued.
    bool got_new_events;

    struct input_fd key_fds[MP_MAX_KEY_FD];
    unsigned int num_key_fd;

    struct input_fd cmd_fds[MP_MAX_CMD_FD];
    unsigned int num_cmd_fd;

    struct cmd_queue key_cmd_queue;
    struct cmd_queue control_cmd_queue;

    int wakeup_pipe[2];
};


int async_quit_request;

static int print_key_list(m_option_t *cfg, char *optname, char *optparam);
static int print_cmd_list(m_option_t *cfg, char *optname, char *optparam);

// Our command line options
static const m_option_t input_conf[] = {
    OPT_STRING("conf", input.config_file, CONF_GLOBAL, OPTDEF_STR("input.conf")),
    OPT_INT("ar-delay", input.ar_delay, CONF_GLOBAL),
    OPT_INT("ar-rate", input.ar_rate, CONF_GLOBAL),
    { "keylist", print_key_list, CONF_TYPE_PRINT_FUNC, CONF_GLOBAL | CONF_NOCFG },
    { "cmdlist", print_cmd_list, CONF_TYPE_PRINT_FUNC, CONF_GLOBAL | CONF_NOCFG },
    OPT_STRING("js-dev", input.js_dev, CONF_GLOBAL),
    OPT_STRING("ar-dev", input.ar_dev, CONF_GLOBAL),
    OPT_STRING("file", input.in_file, CONF_GLOBAL),
    OPT_MAKE_FLAGS("default-bindings", input.default_bindings, CONF_GLOBAL),
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

static const m_option_t mp_input_opts[] = {
    { "input", (void *)&input_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    OPT_MAKE_FLAGS("joystick", input.use_joystick, CONF_GLOBAL),
    OPT_MAKE_FLAGS("lirc", input.use_lirc, CONF_GLOBAL),
    OPT_MAKE_FLAGS("lircc", input.use_lircc, CONF_GLOBAL),
    OPT_MAKE_FLAGS("ar", input.use_ar, CONF_GLOBAL),
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

static int default_cmd_func(int fd, char *buf, int l);

static const char builtin_input_conf[] =
#include "input/input_conf.h"
;

// Encode the unicode codepoint as UTF-8, and append to the end of the
// talloc'ed buffer.
static char *append_utf8_buffer(char *buffer, uint32_t codepoint)
{
    char data[8];
    uint8_t tmp;
    char *output = data;
    PUT_UTF8(codepoint, tmp, *output++ = tmp;);
    return talloc_strndup_append_buffer(buffer, data, output - data);
}

static char *get_key_name(int key, char *ret)
{
    for (int i = 0; modifier_names[i].name; i++) {
        if (modifier_names[i].key & key) {
            ret = talloc_asprintf_append_buffer(ret, "%s+",
                                                modifier_names[i].name);
            key -= modifier_names[i].key;
        }
    }
    for (int i = 0; key_names[i].name != NULL; i++) {
        if (key_names[i].key == key)
            return talloc_asprintf_append_buffer(ret, "%s", key_names[i].name);
    }

    // printable, and valid unicode range
    if (key >= 32 && key <= 0x10FFFF)
        return append_utf8_buffer(ret, key);

    // Print the hex key code
    return talloc_asprintf_append_buffer(ret, "%#-8x", key);
}

static char *get_key_combo_name(int *keys, int max)
{
    char *ret = talloc_strdup(NULL, "");
    while (1) {
        ret = get_key_name(*keys, ret);
        if (--max && *++keys)
            talloc_asprintf_append_buffer(ret, "-");
        else
            break;
    }
    return ret;
}

bool mp_input_is_abort_cmd(int cmd_id)
{
    switch (cmd_id) {
    case MP_CMD_QUIT:
    case MP_CMD_PLAYLIST_NEXT:
    case MP_CMD_PLAYLIST_PREV:
        return true;
    }
    return false;
}

static int queue_count_cmds(struct cmd_queue *queue)
{
    int res = 0;
    for (struct mp_cmd *cmd = queue->first; cmd; cmd = cmd->queue_next)
        res++;
    return res;
}

static bool queue_has_abort_cmds(struct cmd_queue *queue)
{
    for (struct mp_cmd *cmd = queue->first; cmd; cmd = cmd->queue_next) {
        if (mp_input_is_abort_cmd(cmd->id))
            return true;
    }
    return false;
}

static void queue_remove(struct cmd_queue *queue, struct mp_cmd *cmd)
{
    struct mp_cmd **p_prev = &queue->first;
    while (*p_prev != cmd) {
        p_prev = &(*p_prev)->queue_next;
    }
    // if this fails, cmd was not in the queue
    assert(*p_prev == cmd);
    *p_prev = cmd->queue_next;
}

static void queue_pop(struct cmd_queue *queue)
{
    queue_remove(queue, queue->first);
}

static void queue_add(struct cmd_queue *queue, struct mp_cmd *cmd,
                      bool at_head)
{
    if (at_head) {
        cmd->queue_next = queue->first;
        queue->first = cmd;
    } else {
        struct mp_cmd **p_prev = &queue->first;
        while (*p_prev)
            p_prev = &(*p_prev)->queue_next;
        *p_prev = cmd;
        cmd->queue_next = NULL;
    }
}

int mp_input_add_cmd_fd(struct input_ctx *ictx, int fd, int select,
                        int read_func(int fd, char *dest, int size),
                        int close_func(int fd))
{
    if (ictx->num_cmd_fd == MP_MAX_CMD_FD) {
        mp_tmsg(MSGT_INPUT, MSGL_ERR, "Too many command file descriptors, "
                "cannot register file descriptor %d.\n", fd);
        return 0;
    }
    if (select && fd < 0) {
        mp_msg(MSGT_INPUT, MSGL_ERR,
               "Invalid fd %d in mp_input_add_cmd_fd", fd);
        return 0;
    }

    ictx->cmd_fds[ictx->num_cmd_fd] = (struct input_fd){
        .fd = fd,
        .read_func.cmd = read_func ? read_func : default_cmd_func,
        .close_func = close_func,
        .no_select = !select
    };
    ictx->num_cmd_fd++;

    return 1;
}

void mp_input_rm_cmd_fd(struct input_ctx *ictx, int fd)
{
    struct input_fd *cmd_fds = ictx->cmd_fds;
    unsigned int i;

    for (i = 0; i < ictx->num_cmd_fd; i++) {
        if (cmd_fds[i].fd == fd)
            break;
    }
    if (i == ictx->num_cmd_fd)
        return;
    if (cmd_fds[i].close_func)
        cmd_fds[i].close_func(cmd_fds[i].fd);
    talloc_free(cmd_fds[i].buffer);

    if (i + 1 < ictx->num_cmd_fd)
        memmove(&cmd_fds[i], &cmd_fds[i + 1],
                (ictx->num_cmd_fd - i - 1) * sizeof(struct input_fd));
    ictx->num_cmd_fd--;
}

void mp_input_rm_key_fd(struct input_ctx *ictx, int fd)
{
    struct input_fd *key_fds = ictx->key_fds;
    unsigned int i;

    for (i = 0; i < ictx->num_key_fd; i++) {
        if (key_fds[i].fd == fd)
            break;
    }
    if (i == ictx->num_key_fd)
        return;
    if (key_fds[i].close_func)
        key_fds[i].close_func(key_fds[i].fd);

    if (i + 1 < ictx->num_key_fd)
        memmove(&key_fds[i], &key_fds[i + 1],
                (ictx->num_key_fd - i - 1) * sizeof(struct input_fd));
    ictx->num_key_fd--;
}

int mp_input_add_key_fd(struct input_ctx *ictx, int fd, int select,
                        int read_func(void *ctx, int fd),
                        int close_func(int fd), void *ctx)
{
    if (ictx->num_key_fd == MP_MAX_KEY_FD) {
        mp_tmsg(MSGT_INPUT, MSGL_ERR, "Too many key file descriptors, "
                "cannot register file descriptor %d.\n", fd);
        return 0;
    }
    if (select && fd < 0) {
        mp_msg(MSGT_INPUT, MSGL_ERR,
               "Invalid fd %d in mp_input_add_key_fd", fd);
        return 0;
    }

    ictx->key_fds[ictx->num_key_fd] = (struct input_fd){
        .fd = fd,
        .read_func.key = read_func,
        .close_func = close_func,
        .no_select = !select,
        .ctx = ctx,
    };
    ictx->num_key_fd++;

    return 1;
}

mp_cmd_t *mp_input_parse_cmd(char *str)
{
    int i, l;
    int pausing = 0;
    char *ptr;
    const mp_cmd_t *cmd_def;

    // Ignore heading spaces.
    while (str[0] == ' ' || str[0] == '\t')
        ++str;

    if (strncmp(str, "pausing ", 8) == 0) {
        pausing = 1;
        str = &str[8];
    } else if (strncmp(str, "pausing_keep ", 13) == 0) {
        pausing = 2;
        str = &str[13];
    } else if (strncmp(str, "pausing_toggle ", 15) == 0) {
        pausing = 3;
        str = &str[15];
    } else if (strncmp(str, "pausing_keep_force ", 19) == 0) {
        pausing = 4;
        str = &str[19];
    }

    ptr = str + strcspn(str, "\t ");
    if (*ptr != 0)
        l = ptr - str;
    else
        l = strlen(str);

    if (l == 0)
        return NULL;

    for (i = 0; mp_cmds[i].name != NULL; i++) {
        if (strncasecmp(mp_cmds[i].name, str, l) == 0)
            break;
    }

    if (mp_cmds[i].name == NULL)
        return NULL;

    cmd_def = &mp_cmds[i];

    mp_cmd_t *cmd = talloc_ptrtype(NULL, cmd);
    *cmd = (mp_cmd_t){
        .id = cmd_def->id,
        .name = talloc_strdup(cmd, cmd_def->name),
        .pausing = pausing,
    };

    ptr = str;

    for (i = 0; ptr && i < MP_CMD_MAX_ARGS; i++) {
        while (ptr[0] != ' ' && ptr[0] != '\t' && ptr[0] != '\0')
            ptr++;
        if (ptr[0] == '\0')
            break;
        while (ptr[0] == ' ' || ptr[0] == '\t')
            ptr++;
        if (ptr[0] == '\0' || ptr[0] == '#')
            break;
        cmd->args[i].type = cmd_def->args[i].type;
        switch (cmd_def->args[i].type) {
        case MP_CMD_ARG_INT:
            errno = 0;
            cmd->args[i].v.i = atoi(ptr);
            if (errno != 0) {
                mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command %s: argument %d "
                        "isn't an integer.\n", cmd_def->name, i + 1);
                goto error;
            }
            break;
        case MP_CMD_ARG_FLOAT:
            errno = 0;
            cmd->args[i].v.f = atof(ptr);
            if (errno != 0) {
                mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command %s: argument %d "
                        "isn't a float.\n", cmd_def->name, i + 1);
                goto error;
            }
            break;
        case MP_CMD_ARG_STRING: {
            int term = ' ';
            if (*ptr == '\'' || *ptr == '"')
                term = *ptr++;
            char *argptr = talloc_size(cmd, strlen(ptr) + 1);
            cmd->args[i].v.s = argptr;
            while (1) {
                if (*ptr == 0) {
                    if (term == ' ')
                        break;
                    mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command %s: argument %d is "
                            "unterminated.\n", cmd_def->name, i + 1);
                    goto error;
                }
                if (*ptr == term)
                    break;
                if (*ptr == '\\')
                    ptr++;
                if (*ptr != 0)
                    *argptr++ = *ptr++;
            }
            *argptr = 0;
            break;
        }
        case 0:
            ptr = NULL;
            break;
        default:
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Unknown argument %d\n", i);
        }
    }
    cmd->nargs = i;

    int min_args;
    for (min_args = 0; min_args < MP_CMD_MAX_ARGS
             && cmd_def->args[min_args].type
             && !cmd_def->args[min_args].optional; min_args++);
    if (cmd->nargs < min_args) {
        mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command \"%s\" requires at least %d "
                "arguments, we found only %d so far.\n", cmd_def->name,
                min_args, cmd->nargs);
        goto error;
    }

    for (; i < MP_CMD_MAX_ARGS && cmd_def->args[i].type; i++) {
        memcpy(&cmd->args[i], &cmd_def->args[i], sizeof(struct mp_cmd_arg));
        if (cmd_def->args[i].type == MP_CMD_ARG_STRING
            && cmd_def->args[i].v.s != NULL)
            cmd->args[i].v.s = talloc_strdup(cmd, cmd_def->args[i].v.s);
    }

    if (i < MP_CMD_MAX_ARGS)
        cmd->args[i].type = 0;

    return cmd;

 error:
    mp_cmd_free(cmd);
    return NULL;
}

#define MP_CMD_MAX_SIZE 4096

static int read_cmd(struct input_fd *mp_fd, char **ret)
{
    char *end;
    *ret = NULL;

    // Allocate the buffer if it doesn't exist
    if (!mp_fd->buffer) {
        mp_fd->buffer = talloc_size(NULL, MP_CMD_MAX_SIZE);
        mp_fd->pos = 0;
        mp_fd->size = MP_CMD_MAX_SIZE;
    }

    // Get some data if needed/possible
    while (!mp_fd->got_cmd && !mp_fd->eof && (mp_fd->size - mp_fd->pos > 1)) {
        int r = mp_fd->read_func.cmd(mp_fd->fd, mp_fd->buffer + mp_fd->pos,
                                     mp_fd->size - 1 - mp_fd->pos);
        // Error ?
        if (r < 0) {
            switch (r) {
            case MP_INPUT_ERROR:
            case MP_INPUT_DEAD:
                mp_tmsg(MSGT_INPUT, MSGL_ERR, "Error while reading "
                        "command file descriptor %d: %s\n",
                        mp_fd->fd, strerror(errno));
            case MP_INPUT_NOTHING:
                return r;
            case MP_INPUT_RETRY:
                continue;
            }
            // EOF ?
        } else if (r == 0) {
            mp_fd->eof = 1;
            break;
        }
        mp_fd->pos += r;
        break;
    }

    mp_fd->got_cmd = 0;

    while (1) {
        int l = 0;
        // Find the cmd end
        mp_fd->buffer[mp_fd->pos] = '\0';
        end = strchr(mp_fd->buffer, '\r');
        if (end)
            *end = '\n';
        end = strchr(mp_fd->buffer, '\n');
        // No cmd end ?
        if (!end) {
            // If buffer is full we must drop all until the next \n
            if (mp_fd->size - mp_fd->pos <= 1) {
                mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command buffer of file "
                        "descriptor %d is full: dropping content.\n",
                        mp_fd->fd);
                mp_fd->pos = 0;
                mp_fd->drop = 1;
            }
            break;
        }
        // We already have a cmd : set the got_cmd flag
        else if ((*ret)) {
            mp_fd->got_cmd = 1;
            break;
        }

        l = end - mp_fd->buffer;

        // Not dropping : put the cmd in ret
        if (!mp_fd->drop)
            *ret = talloc_strndup(NULL, mp_fd->buffer, l);
        else
            mp_fd->drop = 0;
        mp_fd->pos -= l + 1;
        memmove(mp_fd->buffer, end + 1, mp_fd->pos);
    }

    if (*ret)
        return 1;
    else
        return MP_INPUT_NOTHING;
}

static int default_cmd_func(int fd, char *buf, int l)
{
    while (1) {
        int r = read(fd, buf, l);
        // Error ?
        if (r < 0) {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
                return MP_INPUT_NOTHING;
            return MP_INPUT_ERROR;
            // EOF ?
        }
        return r;
    }
}

static int read_wakeup(void *ctx, int fd)
{
    char buf[100];
    read(fd, buf, sizeof(buf));
    return MP_INPUT_NOTHING;
}


static char *find_bind_for_key(const struct cmd_bind *binds, int n, int *keys)
{
    int j;

    if (n <= 0)
        return NULL;
    for (j = 0; binds[j].cmd != NULL; j++) {
        int found = 1, s;
        for (s = 0; s < n && binds[j].input[s] != 0; s++) {
            if (binds[j].input[s] != keys[s]) {
                found = 0;
                break;
            }
        }
        if (found && binds[j].input[s] == 0 && s == n)
            break;
    }
    return binds[j].cmd;
}

static struct cmd_bind_section *get_bind_section(struct input_ctx *ictx,
                                                 bool builtin,
                                                 char *section)
{
    struct cmd_bind_section *bind_section = ictx->cmd_bind_sections;

    if (section == NULL)
        section = "default";
    while (bind_section) {
        if (strcmp(section, bind_section->section) == 0
            && builtin == bind_section->is_builtin)
            return bind_section;
        if (bind_section->next == NULL)
            break;
        bind_section = bind_section->next;
    }
    if (bind_section) {
        bind_section->next = talloc_ptrtype(ictx, bind_section->next);
        bind_section = bind_section->next;
    } else {
        ictx->cmd_bind_sections = talloc_ptrtype(ictx, ictx->cmd_bind_sections);
        bind_section = ictx->cmd_bind_sections;
    }
    bind_section->cmd_binds = NULL;
    bind_section->section = talloc_strdup(bind_section, section);
    bind_section->is_builtin = builtin;
    bind_section->next = NULL;
    return bind_section;
}

static char *section_find_bind_for_key(struct input_ctx *ictx,
                                       bool builtin, char *section,
                                       int n, int *keys)
{
    struct cmd_bind_section *bs = get_bind_section(ictx, builtin, section);
    const struct cmd_bind *binds = bs->cmd_binds;
    return binds ? find_bind_for_key(binds, n, keys) : NULL;
}

static mp_cmd_t *get_cmd_from_keys(struct input_ctx *ictx, int n, int *keys)
{
    char *cmd = NULL;
    mp_cmd_t *ret;

    cmd = section_find_bind_for_key(ictx, false, ictx->section, n, keys);
    if (ictx->default_bindings && cmd == NULL)
        cmd = section_find_bind_for_key(ictx, true, ictx->section, n, keys);
    if (!(ictx->section_flags & MP_INPUT_NO_DEFAULT_SECTION)) {
        if (cmd == NULL)
            cmd = section_find_bind_for_key(ictx, false, "default", n, keys);
        if (ictx->default_bindings && cmd == NULL)
            cmd = section_find_bind_for_key(ictx, true, "default", n, keys);
    }

    if (cmd == NULL) {
        char *key_buf = get_key_combo_name(keys, n);
        mp_tmsg(MSGT_INPUT, MSGL_WARN,
                "No bind found for key '%s'.\n", key_buf);
        talloc_free(key_buf);
        return NULL;
    }
    if (strcmp(cmd, "ignore") == 0)
        return NULL;
    ret =  mp_input_parse_cmd(cmd);
    if (!ret) {
        char *key_buf = get_key_combo_name(keys, n);
        mp_tmsg(MSGT_INPUT, MSGL_ERR,
                "Invalid command for bound key '%s': '%s'\n", key_buf, cmd);
        talloc_free(key_buf);
    }
    return ret;
}


static mp_cmd_t *interpret_key(struct input_ctx *ictx, int code)
{
    unsigned int j;
    mp_cmd_t *ret;

    /* On normal keyboards shift changes the character code of non-special
     * keys, so don't count the modifier separately for those. In other words
     * we want to have "a" and "A" instead of "a" and "Shift+A"; but a separate
     * shift modifier is still kept for special keys like arrow keys.
     */
    int unmod = code & ~KEY_MODIFIER_MASK;
    if (unmod >= 32 && unmod < MP_KEY_BASE)
        code &= ~KEY_MODIFIER_SHIFT;

    if (code & MP_KEY_DOWN) {
        if (ictx->num_key_down > MP_MAX_KEY_DOWN) {
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Too many key down events "
                    "at the same time\n");
            return NULL;
        }
        code &= ~MP_KEY_DOWN;
        // Check if we don't already have this key as pushed
        for (j = 0; j < ictx->num_key_down; j++) {
            if (ictx->key_down[j] == code)
                break;
        }
        if (j != ictx->num_key_down)
            return NULL;
        ictx->key_down[ictx->num_key_down] = code;
        ictx->num_key_down++;
        ictx->last_key_down = GetTimer();
        ictx->ar_state = 0;
        return NULL;
    }
    // button released or press of key with no separate down/up events
    for (j = 0; j < ictx->num_key_down; j++) {
        if (ictx->key_down[j] == code)
            break;
    }
    bool doubleclick = code >= MOUSE_BTN0_DBL && code < MOUSE_BTN_DBL_END;
    if (doubleclick) {
        int btn = code - MOUSE_BTN0_DBL + MOUSE_BTN0;
        if (!ictx->num_key_down
            || ictx->key_down[ictx->num_key_down - 1] != btn)
            return NULL;
        j = ictx->num_key_down - 1;
        ictx->key_down[j] = code;
    }
    if (j == ictx->num_key_down) {  // was not already down; add temporarily
        if (ictx->num_key_down > MP_MAX_KEY_DOWN) {
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Too many key down events "
                    "at the same time\n");
            return NULL;
        }
        ictx->key_down[ictx->num_key_down] = code;
        ictx->num_key_down++;
        ictx->last_key_down = 1;
    }
    // Interpret only maximal point of multibutton event
    ret = ictx->last_key_down ?
          get_cmd_from_keys(ictx, ictx->num_key_down, ictx->key_down)
          : NULL;
    if (doubleclick) {
        ictx->key_down[j] = code - MOUSE_BTN0_DBL + MOUSE_BTN0;
        return ret;
    }
    // Remove the key
    if (j + 1 < ictx->num_key_down)
        memmove(&ictx->key_down[j], &ictx->key_down[j + 1],
                (ictx->num_key_down - (j + 1)) * sizeof(int));
    ictx->num_key_down--;
    ictx->last_key_down = 0;
    ictx->ar_state = -1;
    mp_cmd_free(ictx->ar_cmd);
    ictx->ar_cmd = NULL;
    return ret;
}

static mp_cmd_t *check_autorepeat(struct input_ctx *ictx)
{
    // No input : autorepeat ?
    if (ictx->ar_rate > 0 && ictx->ar_state >= 0 && ictx->num_key_down > 0
        && !(ictx->key_down[ictx->num_key_down - 1] & MP_NO_REPEAT_KEY)) {
        unsigned int t = GetTimer();
        // First time : wait delay
        if (ictx->ar_state == 0
            && (t - ictx->last_key_down) >= ictx->ar_delay * 1000) {
            ictx->ar_cmd = get_cmd_from_keys(ictx, ictx->num_key_down,
                                             ictx->key_down);
            if (!ictx->ar_cmd) {
                ictx->ar_state = -1;
                return NULL;
            }
            ictx->ar_state = 1;
            ictx->last_ar = t;
            return mp_cmd_clone(ictx->ar_cmd);
            // Then send rate / sec event
        } else if (ictx->ar_state == 1
                   && (t - ictx->last_ar) >= 1000000 / ictx->ar_rate) {
            ictx->last_ar = t;
            return mp_cmd_clone(ictx->ar_cmd);
        }
    }
    return NULL;
}

void mp_input_feed_key(struct input_ctx *ictx, int code)
{
    ictx->got_new_events = true;
    if (code == MP_INPUT_RELEASE_ALL) {
        memset(ictx->key_down, 0, sizeof(ictx->key_down));
        ictx->num_key_down = 0;
        ictx->last_key_down = 0;
        return;
    }
    struct mp_cmd *cmd = interpret_key(ictx, code);
    if (!cmd)
        return;
    struct cmd_queue *queue = &ictx->key_cmd_queue;
    if (queue_count_cmds(queue) >= ictx->key_fifo_size &&
            (!mp_input_is_abort_cmd(cmd->id) || queue_has_abort_cmds(queue)))
        return;
    queue_add(queue, cmd, false);
}

static void read_cmd_fd(struct input_ctx *ictx, struct input_fd *cmd_fd)
{
    int r;
    char *text;
    while ((r = read_cmd(cmd_fd, &text)) >= 0) {
        ictx->got_new_events = true;
        struct mp_cmd *cmd = mp_input_parse_cmd(text);
        talloc_free(text);
        if (cmd)
            queue_add(&ictx->control_cmd_queue, cmd, false);
        if (!cmd_fd->got_cmd)
            return;
    }
    if (r == MP_INPUT_ERROR)
        mp_tmsg(MSGT_INPUT, MSGL_ERR, "Error on command file descriptor %d\n",
                cmd_fd->fd);
    else if (r == MP_INPUT_DEAD)
        cmd_fd->dead = true;
}

static void read_key_fd(struct input_ctx *ictx, struct input_fd *key_fd)
{
    int code = key_fd->read_func.key(key_fd->ctx, key_fd->fd);
    if (code >= 0 || code == MP_INPUT_RELEASE_ALL) {
        mp_input_feed_key(ictx, code);
        return;
    }

    if (code == MP_INPUT_ERROR)
        mp_tmsg(MSGT_INPUT, MSGL_ERR,
                "Error on key input file descriptor %d\n", key_fd->fd);
    else if (code == MP_INPUT_DEAD) {
        mp_tmsg(MSGT_INPUT, MSGL_ERR,
                "Dead key input on file descriptor %d\n", key_fd->fd);
        key_fd->dead = true;
    }
}

/**
 * \param time time to wait at most for an event in milliseconds
 */
static void read_events(struct input_ctx *ictx, int time)
{
    ictx->got_new_events = false;
    struct input_fd *key_fds = ictx->key_fds;
    struct input_fd *cmd_fds = ictx->cmd_fds;
    for (int i = 0; i < ictx->num_key_fd; i++)
        if (key_fds[i].dead) {
            mp_input_rm_key_fd(ictx, key_fds[i].fd);
            i--;
        } else if (time && key_fds[i].no_select)
            read_key_fd(ictx, &key_fds[i]);
    for (int i = 0; i < ictx->num_cmd_fd; i++)
        if (cmd_fds[i].dead || cmd_fds[i].eof) {
            mp_input_rm_cmd_fd(ictx, cmd_fds[i].fd);
            i--;
        } else if (time && cmd_fds[i].no_select)
            read_cmd_fd(ictx, &cmd_fds[i]);
    if (ictx->got_new_events)
        time = 0;
#ifdef HAVE_POSIX_SELECT
    fd_set fds;
    FD_ZERO(&fds);
    int max_fd = 0;
    for (int i = 0; i < ictx->num_key_fd; i++) {
        if (key_fds[i].no_select)
            continue;
        if (key_fds[i].fd > max_fd)
            max_fd = key_fds[i].fd;
        FD_SET(key_fds[i].fd, &fds);
    }
    for (int i = 0; i < ictx->num_cmd_fd; i++) {
        if (cmd_fds[i].no_select)
            continue;
        if (cmd_fds[i].fd > max_fd)
            max_fd = cmd_fds[i].fd;
        FD_SET(cmd_fds[i].fd, &fds);
    }
    struct timeval tv, *time_val;
    if (time >= 0) {
        tv.tv_sec = time / 1000;
        tv.tv_usec = (time % 1000) * 1000;
        time_val = &tv;
    } else
        time_val = NULL;
    if (select(max_fd + 1, &fds, NULL, NULL, time_val) < 0) {
        if (errno != EINTR)
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Select error: %s\n",
                    strerror(errno));
        FD_ZERO(&fds);
    }
#else
    if (time)
        usec_sleep(time * 1000);
#endif


    for (int i = 0; i < ictx->num_key_fd; i++) {
#ifdef HAVE_POSIX_SELECT
        if (!key_fds[i].no_select && !FD_ISSET(key_fds[i].fd, &fds))
            continue;
#endif
        read_key_fd(ictx, &key_fds[i]);
    }

    for (int i = 0; i < ictx->num_cmd_fd; i++) {
#ifdef HAVE_POSIX_SELECT
        if (!cmd_fds[i].no_select && !FD_ISSET(cmd_fds[i].fd, &fds))
            continue;
#endif
        read_cmd_fd(ictx, &cmd_fds[i]);
    }
}

/* To support blocking file descriptors we don't loop the read over
 * every source until it's known to be empty. Instead we use this wrapper
 * to run select() again.
 */
static void read_all_fd_events(struct input_ctx *ictx, int time)
{
    while (1) {
        read_events(ictx, time);
        if (!ictx->got_new_events)
            return;
        time = 0;
    }
}

static void read_all_events(struct input_ctx *ictx, int time)
{
#ifdef CONFIG_COCOA
    cocoa_events_read_all_events(ictx, time);
#else
    read_all_fd_events(ictx, time);
#endif
}

int mp_input_queue_cmd(struct input_ctx *ictx, mp_cmd_t *cmd)
{
    ictx->got_new_events = true;
    if (!cmd)
        return 0;
    queue_add(&ictx->control_cmd_queue, cmd, true);
    return 1;
}

/**
 * \param peek_only when set, the returned command stays in the queue.
 * Do not free the returned cmd whe you set this!
 */
mp_cmd_t *mp_input_get_cmd(struct input_ctx *ictx, int time, int peek_only)
{
    if (async_quit_request)
        return mp_input_parse_cmd("quit 1");

    if (ictx->control_cmd_queue.first || ictx->key_cmd_queue.first)
        time = 0;
    read_all_events(ictx, time);
    struct mp_cmd *ret;
    struct cmd_queue *queue = &ictx->control_cmd_queue;
    if (!queue->first)
        queue = &ictx->key_cmd_queue;
    if (!queue->first) {
        ret = check_autorepeat(ictx);
        if (!ret)
            return NULL;
        queue_add(queue, ret, false);
    } else
        ret = queue->first;

    if (!peek_only)
        queue_pop(queue);

    return ret;
}

void mp_cmd_free(mp_cmd_t *cmd)
{
    talloc_free(cmd);
}

mp_cmd_t *mp_cmd_clone(mp_cmd_t *cmd)
{
    mp_cmd_t *ret;
    int i;

    ret = talloc_memdup(NULL, cmd, sizeof(mp_cmd_t));
    ret->name = talloc_strdup(ret, cmd->name);
    for (i = 0; i < MP_CMD_MAX_ARGS && cmd->args[i].type; i++) {
        if (cmd->args[i].type == MP_CMD_ARG_STRING && cmd->args[i].v.s != NULL)
            ret->args[i].v.s = talloc_strdup(ret, cmd->args[i].v.s);
    }

    return ret;
}

int mp_input_get_key_from_name(const char *name)
{
    int modifiers = 0;
    const char *p;
    while ((p = strchr(name, '+'))) {
        for (struct key_name *m = modifier_names; m->name; m++)
            if (!bstrcasecmp(bstr0(m->name),
                             (struct bstr){(char *)name, p - name})) {
                modifiers |= m->key;
                goto found;
            }
        if (!strcmp(name, "+"))
            return '+' + modifiers;
        return -1;
found:
        name = p + 1;
    }

    struct bstr bname = bstr0(name);

    struct bstr rest;
    int code = bstr_decode_utf8(bname, &rest);
    if (code >= 0 && rest.len == 0)
        return code + modifiers;

    if (bstr_startswith0(bname, "0x"))
        return strtol(name, NULL, 16) + modifiers;

    for (int i = 0; key_names[i].name != NULL; i++) {
        if (strcasecmp(key_names[i].name, name) == 0)
            return key_names[i].key + modifiers;
    }

    return -1;
}

static int get_input_from_name(char *name, int *keys)
{
    char *end, *ptr;
    int n = 0;

    ptr = name;
    n = 0;
    for (end = strchr(ptr, '-'); ptr != NULL; end = strchr(ptr, '-')) {
        if (end && end[1] != '\0') {
            if (end[1] == '-')
                end = &end[1];
            end[0] = '\0';
        }
        keys[n] = mp_input_get_key_from_name(ptr);
        if (keys[n] < 0)
            return 0;
        n++;
        if (end && end[1] != '\0' && n < MP_MAX_KEY_DOWN)
            ptr = &end[1];
        else
            break;
    }
    keys[n] = 0;
    return 1;
}

static void bind_keys(struct input_ctx *ictx, bool builtin,
                      const int keys[MP_MAX_KEY_DOWN + 1], bstr command)
{
    int i = 0, j;
    struct cmd_bind *bind = NULL;
    struct cmd_bind_section *bind_section = NULL;
    char *section = NULL;

    if (bstr_startswith0(command, "{")) {
        int p = bstrchr(command, '}');
        if (p != -1) {
            bstr bsection = bstr_strip(bstr_splice(command, 1, p));
            section = bstrdup0(NULL, bsection);
            command = bstr_lstrip(bstr_cut(command, p + 1));
        }
    }
    bind_section = get_bind_section(ictx, builtin, section);
    talloc_free(section);

    if (bind_section->cmd_binds) {
        for (i = 0; bind_section->cmd_binds[i].cmd != NULL; i++) {
            for (j = 0; bind_section->cmd_binds[i].input[j] == keys[j] && keys[j] != 0; j++)
                /* NOTHING */;
            if (keys[j] == 0 && bind_section->cmd_binds[i].input[j] == 0 ) {
                bind = &bind_section->cmd_binds[i];
                break;
            }
        }
    }

    if (!bind) {
        bind_section->cmd_binds = talloc_realloc(bind_section,
                                                 bind_section->cmd_binds,
                                                 struct cmd_bind, i + 2);
        memset(&bind_section->cmd_binds[i], 0, 2 * sizeof(struct cmd_bind));
        bind = &bind_section->cmd_binds[i];
    }
    talloc_free(bind->cmd);
    bind->cmd = bstrdup0(bind_section->cmd_binds, command);
    memcpy(bind->input, keys, (MP_MAX_KEY_DOWN + 1) * sizeof(int));
}

static int parse_config(struct input_ctx *ictx, bool builtin, bstr data)
{
    int n_binds = 0, keys[MP_MAX_KEY_DOWN + 1];

    while (data.len) {
        bstr line = bstr_strip_linebreaks(bstr_getline(data, &data));
        line = bstr_lstrip(line);
        if (line.len == 0 || bstr_startswith0(line, "#"))
            continue;
        struct bstr command;
        // Find the key name starting a line
        struct bstr keyname = bstr_split(line, WHITESPACE, &command);
        command = bstr_strip(command);
        if (command.len == 0) {
            mp_tmsg(MSGT_INPUT, MSGL_ERR,
                    "Unfinished key binding: %.*s\n", BSTR_P(line));
            continue;
        }
        char *name = bstrdup0(NULL, keyname);
        if (!get_input_from_name(name, keys)) {
            talloc_free(name);
            mp_tmsg(MSGT_INPUT, MSGL_ERR,
                    "Unknown key '%.*s'\n", BSTR_P(keyname));
            continue;
        }
        talloc_free(name);
        bind_keys(ictx, builtin, keys, command);
        n_binds++;
    }

    return n_binds;
}

static int parse_config_file(struct input_ctx *ictx, char *file)
{
    struct bstr res = {0};
    stream_t *s = open_stream(file, NULL, NULL);
    if (!s) {
        mp_msg(MSGT_INPUT, MSGL_V, "Can't open input config file %s.\n", file);
        return 0;
    }
    res = stream_read_complete(s, NULL, 1000000, 0);
    free_stream(s);
    mp_msg(MSGT_INPUT, MSGL_V, "Parsing input config file %s\n", file);
    int n_binds = parse_config(ictx, false, res);
    talloc_free(res.start);
    mp_msg(MSGT_INPUT, MSGL_V, "Input config file %s parsed: %d binds\n",
           file, n_binds);
    return 1;
}

void mp_input_set_section(struct input_ctx *ictx, char *name, int flags)
{
    talloc_free(ictx->section);
    ictx->section = talloc_strdup(ictx, name ? name : "default");
    ictx->section_flags = flags;
}

char *mp_input_get_section(struct input_ctx *ictx)
{
    return ictx->section;
}

struct input_ctx *mp_input_init(struct input_conf *input_conf)
{
    struct input_ctx *ictx = talloc_ptrtype(NULL, ictx);
    *ictx = (struct input_ctx){
        .key_fifo_size = input_conf->key_fifo_size,
        .ar_state = -1,
        .ar_delay = input_conf->ar_delay,
        .ar_rate = input_conf->ar_rate,
        .default_bindings = input_conf->default_bindings,
        .wakeup_pipe = {-1, -1},
    };
    ictx->section = talloc_strdup(ictx, "default");

    parse_config(ictx, true, bstr0(builtin_input_conf));

#ifdef CONFIG_COCOA
    cocoa_events_init(ictx, read_all_fd_events);
#endif

#ifndef __MINGW32__
    long ret = pipe(ictx->wakeup_pipe);
    for (int i = 0; i < 2 && ret >= 0; i++) {
        ret = fcntl(ictx->wakeup_pipe[i], F_GETFL);
        if (ret < 0)
            break;
        ret = fcntl(ictx->wakeup_pipe[i], F_SETFL, ret | O_NONBLOCK);
    }
    if (ret < 0)
        mp_msg(MSGT_INPUT, MSGL_ERR,
               "Failed to initialize wakeup pipe: %s\n", strerror(errno));
    else
        mp_input_add_key_fd(ictx, ictx->wakeup_pipe[0], true, read_wakeup,
                            NULL, NULL);
#endif

    char *file;
    char *config_file = input_conf->config_file;
    file = config_file[0] != '/' ? get_path(config_file) : config_file;
    if (!file)
        return ictx;

    if (!parse_config_file(ictx, file)) {
        // free file if it was allocated by get_path(),
        // before it gets overwritten
        if (file != config_file)
            free(file);
        // Try global conf dir
        file = MPLAYER_CONFDIR "/input.conf";
        if (!parse_config_file(ictx, file))
            mp_msg(MSGT_INPUT, MSGL_V, "Falling back on default (hardcoded) "
                   "input config\n");
    } else {
        // free file if it was allocated by get_path()
        if (file != config_file)
            free(file);
    }

#ifdef CONFIG_JOYSTICK
    if (input_conf->use_joystick) {
        int fd = mp_input_joystick_init(input_conf->js_dev);
        if (fd < 0)
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Can't init input joystick\n");
        else
            mp_input_add_key_fd(ictx, fd, 1, mp_input_joystick_read,
                                close, NULL);
    }
#endif

#ifdef CONFIG_LIRC
    if (input_conf->use_lirc) {
        int fd = mp_input_lirc_init();
        if (fd > 0)
            mp_input_add_cmd_fd(ictx, fd, 0, mp_input_lirc_read,
                                mp_input_lirc_close);
    }
#endif

#ifdef CONFIG_LIRCC
    if (input_conf->use_lircc) {
        int fd = lircc_init("mplayer", NULL);
        if (fd >= 0)
            mp_input_add_cmd_fd(ictx, fd, 1, NULL, lircc_cleanup);
    }
#endif

#ifdef CONFIG_APPLE_REMOTE
    if (input_conf->use_ar) {
        if (mp_input_ar_init() < 0)
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Can't init Apple Remote.\n");
        else
            mp_input_add_key_fd(ictx, -1, 0, mp_input_ar_read,
                                mp_input_ar_close, NULL);
    }
#endif

#ifdef CONFIG_APPLE_IR
    if (input_conf->use_ar) {
        int fd = mp_input_appleir_init(input_conf->ar_dev);
        if (fd < 0)
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Can't init Apple Remote.\n");
        else
            mp_input_add_key_fd(ictx, fd, 1, mp_input_appleir_read,
                                close, NULL);
    }
#endif

    if (input_conf->in_file) {
        int mode = O_RDONLY;
#ifndef __MINGW32__
        // Use RDWR for FIFOs to ensure they stay open over multiple accesses.
        // Note that on Windows due to how the API works, using RDONLY should
        // be ok.
        struct stat st;
        if (stat(input_conf->in_file, &st) == 0 && S_ISFIFO(st.st_mode))
            mode = O_RDWR;
        mode |= O_NONBLOCK;
#endif
        int in_file_fd = open(input_conf->in_file, mode);
        if (in_file_fd >= 0)
            mp_input_add_cmd_fd(ictx, in_file_fd, 1, NULL, close);
        else
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Can't open %s: %s\n",
                    input_conf->in_file, strerror(errno));
    }

    return ictx;
}

void mp_input_uninit(struct input_ctx *ictx)
{
#ifdef CONFIG_COCOA
    cocoa_events_uninit();
#endif

    if (!ictx)
        return;

    for (int i = 0; i < ictx->num_key_fd; i++) {
        if (ictx->key_fds[i].close_func)
            ictx->key_fds[i].close_func(ictx->key_fds[i].fd);
    }
    for (int i = 0; i < ictx->num_cmd_fd; i++) {
        if (ictx->cmd_fds[i].close_func)
            ictx->cmd_fds[i].close_func(ictx->cmd_fds[i].fd);
    }
    for (int i = 0; i < 2; i++)
        if (ictx->wakeup_pipe[i] != -1)
            close(ictx->wakeup_pipe[i]);
    talloc_free(ictx);
}

void mp_input_register_options(m_config_t *cfg)
{
    m_config_register_options(cfg, mp_input_opts);
}

static int print_key_list(m_option_t *cfg, char *optname, char *optparam)
{
    int i;
    printf("\n");
    for (i = 0; key_names[i].name != NULL; i++)
        printf("%s\n", key_names[i].name);
    return M_OPT_EXIT;
}

static int print_cmd_list(m_option_t *cfg, char *optname, char *optparam)
{
    const mp_cmd_t *cmd;
    int i, j;
    const char *type;

    for (i = 0; (cmd = &mp_cmds[i])->name != NULL; i++) {
        printf("%-20.20s", cmd->name);
        for (j = 0; j < MP_CMD_MAX_ARGS && cmd->args[j].type; j++) {
            switch (cmd->args[j].type) {
            case MP_CMD_ARG_INT:
                type = "Integer";
                break;
            case MP_CMD_ARG_FLOAT:
                type = "Float";
                break;
            case MP_CMD_ARG_STRING:
                type = "String";
                break;
            default:
                type = "??";
            }
            if (cmd->args[j].optional)
                printf(" [%s]", type);
            else
                printf(" %s", type);
        }
        printf("\n");
    }
    return M_OPT_EXIT;
}

void mp_input_wakeup(struct input_ctx *ictx)
{
    if (ictx->wakeup_pipe[1] >= 0)
        write(ictx->wakeup_pipe[1], &(char){0}, 1);
}

/**
 * \param time time to wait for an interruption in milliseconds
 */
int mp_input_check_interrupt(struct input_ctx *ictx, int time)
{
    for (int i = 0; ; i++) {
        if (async_quit_request || queue_has_abort_cmds(&ictx->key_cmd_queue) ||
                queue_has_abort_cmds(&ictx->control_cmd_queue)) {
            mp_tmsg(MSGT_INPUT, MSGL_WARN, "Received command to move to "
                   "another file. Aborting current processing.\n");
            return true;
        }
        if (i)
            return false;
        read_all_events(ictx, time);
    }
}
