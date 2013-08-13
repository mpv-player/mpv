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

#include <libavutil/avstring.h>
#include <libavutil/common.h>

#include "osdep/io.h"
#include "osdep/getch2.h"

#include "input.h"
#include "keycodes.h"
#include "osdep/timer.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/m_config.h"
#include "mpvcore/m_option.h"
#include "mpvcore/path.h"
#include "talloc.h"
#include "mpvcore/options.h"
#include "mpvcore/bstr.h"
#include "stream/stream.h"
#include "mpvcore/mp_common.h"

#include "joystick.h"

#ifdef CONFIG_LIRC
#include "lirc.h"
#endif

#ifdef CONFIG_LIRCC
#include <lirc/lircc.h>
#endif

#ifdef CONFIG_COCOA
#include "osdep/macosx_events.h"
#endif

#define MP_MAX_KEY_DOWN 4

#if HAVE_PTHREADS
#include <pthread.h>
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
#define queue_lock()   pthread_mutex_lock(&queue_mutex)
#define queue_unlock() pthread_mutex_unlock(&queue_mutex)
#else
#define queue_lock() 0
#define queue_lock() 0
#endif

struct cmd_bind {
    int keys[MP_MAX_KEY_DOWN];
    int num_keys;
    char *cmd;
    char *location;     // filename/line number of definition
    bool is_builtin;
    struct cmd_bind_section *owner;
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

#define ARG_INT                 { .type = {"", NULL, &m_option_type_int} }
#define ARG_FLOAT               { .type = {"", NULL, &m_option_type_float} }
#define ARG_DOUBLE              { .type = {"", NULL, &m_option_type_double} }
#define ARG_STRING              { .type = {"", NULL, &m_option_type_string} }
#define ARG_CHOICE(c)           { .type = {"", NULL, &m_option_type_choice,    \
                                           M_CHOICES(c)} }
#define ARG_TIME                { .type = {"", NULL, &m_option_type_time} }

#define OARG_DOUBLE(def)        { .type = {"", NULL, &m_option_type_double},   \
                                  .optional = true, .v.d = def }
#define OARG_INT(def)           { .type = {"", NULL, &m_option_type_int},      \
                                  .optional = true, .v.i = def }
#define OARG_CHOICE(def, c)     { .type = {"", NULL, &m_option_type_choice,    \
                                           M_CHOICES(c)},                      \
                                  .optional = true, .v.i = def }

static int parse_cycle_dir(const struct m_option *opt, struct bstr name,
                           struct bstr param, void *dst);
static const struct m_option_type m_option_type_cycle_dir = {
    .name = "up|down",
    .parse = parse_cycle_dir,
};

static const mp_cmd_t mp_cmds[] = {
  { MP_CMD_IGNORE, "ignore", },

  { MP_CMD_RADIO_STEP_CHANNEL, "radio_step_channel", { ARG_INT } },
  { MP_CMD_RADIO_SET_CHANNEL, "radio_set_channel", { ARG_STRING } },
  { MP_CMD_RADIO_SET_FREQ, "radio_set_freq", { ARG_FLOAT } },
  { MP_CMD_RADIO_STEP_FREQ, "radio_step_freq", {ARG_FLOAT } },

  { MP_CMD_SEEK, "seek", {
      ARG_TIME,
      OARG_CHOICE(0, ({"relative", 0},          {"0", 0},
                      {"absolute-percent", 1},  {"1", 1},
                      {"absolute", 2},          {"2", 2})),
      OARG_CHOICE(0, ({"default-precise", 0},   {"0", 0},
                      {"exact", 1},             {"1", 1},
                      {"keyframes", -1},        {"-1", -1})),
  }},
  { MP_CMD_SPEED_MULT, "speed_mult", { ARG_DOUBLE } },
  { MP_CMD_QUIT, "quit", { OARG_INT(0) } },
  { MP_CMD_QUIT_WATCH_LATER, "quit_watch_later", },
  { MP_CMD_STOP, "stop", },
  { MP_CMD_FRAME_STEP, "frame_step", },
  { MP_CMD_FRAME_BACK_STEP, "frame_back_step", },
  { MP_CMD_PLAYLIST_NEXT, "playlist_next", {
      OARG_CHOICE(0, ({"weak", 0},              {"0", 0},
                      {"force", 1},             {"1", 1})),
  }},
  { MP_CMD_PLAYLIST_PREV, "playlist_prev", {
      OARG_CHOICE(0, ({"weak", 0},              {"0", 0},
                      {"force", 1},             {"1", 1})),
  }},
  { MP_CMD_SUB_STEP, "sub_step", { ARG_INT } },
  { MP_CMD_OSD, "osd", { OARG_INT(-1) } },
  { MP_CMD_PRINT_TEXT, "print_text", { ARG_STRING } },
  { MP_CMD_SHOW_TEXT, "show_text", { ARG_STRING, OARG_INT(-1), OARG_INT(0) } },
  { MP_CMD_SHOW_PROGRESS, "show_progress", },
  { MP_CMD_SUB_ADD, "sub_add", { ARG_STRING } },
  { MP_CMD_SUB_REMOVE, "sub_remove", { OARG_INT(-1) } },
  { MP_CMD_SUB_RELOAD, "sub_reload", { OARG_INT(-1) } },

  { MP_CMD_TV_START_SCAN, "tv_start_scan", },
  { MP_CMD_TV_STEP_CHANNEL, "tv_step_channel", { ARG_INT } },
  { MP_CMD_TV_STEP_NORM, "tv_step_norm", },
  { MP_CMD_TV_STEP_CHANNEL_LIST, "tv_step_chanlist", },
  { MP_CMD_TV_SET_CHANNEL, "tv_set_channel", { ARG_STRING } },
  { MP_CMD_TV_LAST_CHANNEL, "tv_last_channel", },
  { MP_CMD_TV_SET_FREQ, "tv_set_freq", { ARG_FLOAT } },
  { MP_CMD_TV_STEP_FREQ, "tv_step_freq", { ARG_FLOAT } },
  { MP_CMD_TV_SET_NORM, "tv_set_norm", { ARG_STRING } },

  { MP_CMD_DVB_SET_CHANNEL, "dvb_set_channel", { ARG_INT, ARG_INT } },

  { MP_CMD_SCREENSHOT, "screenshot", {
      OARG_CHOICE(2, ({"video", 0},
                      {"window", 1},
                      {"subtitles", 2})),
      OARG_CHOICE(0, ({"single", 0},
                      {"each-frame", 1})),
  }},
  { MP_CMD_SCREENSHOT_TO_FILE, "screenshot_to_file", {
      ARG_STRING,
      OARG_CHOICE(2, ({"video", 0},
                      {"window", 1},
                      {"subtitles", 2})),
  }},
  { MP_CMD_LOADFILE, "loadfile", {
      ARG_STRING,
      OARG_CHOICE(0, ({"replace", 0},          {"0", 0},
                      {"append", 1},           {"1", 1})),
  }},
  { MP_CMD_LOADLIST, "loadlist", {
      ARG_STRING,
      OARG_CHOICE(0, ({"replace", 0},          {"0", 0},
                      {"append", 1},           {"1", 1})),
  }},
  { MP_CMD_PLAYLIST_CLEAR, "playlist_clear", },
  { MP_CMD_PLAYLIST_REMOVE, "playlist_remove", { ARG_INT } },
  { MP_CMD_PLAYLIST_MOVE, "playlist_move", { ARG_INT, ARG_INT } },
  { MP_CMD_RUN, "run", { ARG_STRING } },

  { MP_CMD_KEYDOWN_EVENTS, "key_down_event", { ARG_INT } },
  { MP_CMD_SET, "set", { ARG_STRING,  ARG_STRING } },
  { MP_CMD_GET_PROPERTY, "get_property", { ARG_STRING } },
  { MP_CMD_ADD, "add", { ARG_STRING, OARG_DOUBLE(0) } },
  { MP_CMD_CYCLE, "cycle", {
      ARG_STRING,
      { .type = {"", NULL, &m_option_type_cycle_dir},
        .optional = true,
        .v.d = 1 },
  }},

  { MP_CMD_ENABLE_INPUT_SECTION,  "enable_section",  {
      ARG_STRING,
      OARG_CHOICE(0, ({"default", 0},
                      {"exclusive", 1})),
  }},
  { MP_CMD_DISABLE_INPUT_SECTION, "disable_section", { ARG_STRING } },

  { MP_CMD_AF, "af", { ARG_STRING, ARG_STRING } },

  { MP_CMD_VF, "vf", { ARG_STRING, ARG_STRING } },

  { MP_CMD_VO_CMDLINE, "vo_cmdline", { ARG_STRING } },

  {0}
};

// Map legacy commands to proper commands
struct legacy_cmd {
    const char *old, *new;
};
static const struct legacy_cmd legacy_cmds[] = {
    {"loop",                    "cycle loop"},
    {"seek_chapter",            "add chapter"},
    {"switch_angle",            "cycle angle"},
    {"pause",                   "cycle pause"},
    {"volume",                  "add volume"},
    {"mute",                    "cycle mute"},
    {"audio_delay",             "add audio-delay"},
    {"switch_audio",            "cycle audio"},
    {"balance",                 "add balance"},
    {"vo_fullscreen",           "cycle fullscreen"},
    {"panscan",                 "add panscan"},
    {"vo_ontop",                "cycle ontop"},
    {"vo_border",               "cycle border"},
    {"frame_drop",              "cycle framedrop"},
    {"gamma",                   "add gamma"},
    {"brightness",              "add brightness"},
    {"contrast",                "add contrast"},
    {"saturation",              "add saturation"},
    {"hue",                     "add hue"},
    {"switch_vsync",            "cycle vsync"},
    {"sub_load",                "sub_add"},
    {"sub_select",              "cycle sub"},
    {"sub_pos",                 "add sub-pos"},
    {"sub_delay",               "add sub-delay"},
    {"sub_visibility",          "cycle sub-visibility"},
    {"forced_subs_only",        "cycle sub-forced-only"},
    {"sub_scale",               "add sub-scale"},
    {"ass_use_margins",         "cycle ass-use-margins"},
    {"tv_set_brightness",       "add tv-brightness"},
    {"tv_set_hue",              "add tv-hue"},
    {"tv_set_saturation",       "add tv-saturation"},
    {"tv_set_contrast",         "add tv-contrast"},
    {"step_property_osd",       "cycle"},
    {"step_property",           "no-osd cycle"},
    {"set_property",            "no-osd set"},
    {"set_property_osd",        "set"},
    {"speed_set",               "set speed"},
    {"osd_show_text",           "show_text"},
    {"osd_show_property_text",  "show_text"},
    {"osd_show_progression",    "show_progress"},
    {"show_chapters_osd",       "show_text ${chapter-list}"},
    {"!show_chapters",          "show_text ${chapter-list}"},
    {"show_tracks_osd",         "show_text ${track-list}"},
    {"!show_tracks",            "show_text ${track-list}"},
    {"!show_playlist",          "show_text ${playlist}"},

    // Approximate (can fail if user added additional whitespace)
    {"pt_step 1",               "playlist_next"},
    {"pt_step -1",              "playlist_prev"},
    // Switch_ratio without argument resets aspect ratio
    {"switch_ratio ",           "set aspect "},
    {"switch_ratio",            "set aspect 0"},
    {0}
};


/// The names of the keys as used in input.conf
/// If you add some new keys, you also need to add them here

static const struct key_name key_names[] = {
  { ' ', "SPACE" },
  { '#', "SHARP" },
  { MP_KEY_ENTER, "ENTER" },
  { MP_KEY_TAB, "TAB" },
  { MP_KEY_BACKSPACE, "BS" },
  { MP_KEY_DELETE, "DEL" },
  { MP_KEY_INSERT, "INS" },
  { MP_KEY_HOME, "HOME" },
  { MP_KEY_END, "END" },
  { MP_KEY_PAGE_UP, "PGUP" },
  { MP_KEY_PAGE_DOWN, "PGDWN" },
  { MP_KEY_ESC, "ESC" },
  { MP_KEY_PRINT, "PRINT" },
  { MP_KEY_RIGHT, "RIGHT" },
  { MP_KEY_LEFT, "LEFT" },
  { MP_KEY_DOWN, "DOWN" },
  { MP_KEY_UP, "UP" },
  { MP_KEY_F+1, "F1" },
  { MP_KEY_F+2, "F2" },
  { MP_KEY_F+3, "F3" },
  { MP_KEY_F+4, "F4" },
  { MP_KEY_F+5, "F5" },
  { MP_KEY_F+6, "F6" },
  { MP_KEY_F+7, "F7" },
  { MP_KEY_F+8, "F8" },
  { MP_KEY_F+9, "F9" },
  { MP_KEY_F+10, "F10" },
  { MP_KEY_F+11, "F11" },
  { MP_KEY_F+12, "F12" },
  { MP_KEY_KP0, "KP0" },
  { MP_KEY_KP1, "KP1" },
  { MP_KEY_KP2, "KP2" },
  { MP_KEY_KP3, "KP3" },
  { MP_KEY_KP4, "KP4" },
  { MP_KEY_KP5, "KP5" },
  { MP_KEY_KP6, "KP6" },
  { MP_KEY_KP7, "KP7" },
  { MP_KEY_KP8, "KP8" },
  { MP_KEY_KP9, "KP9" },
  { MP_KEY_KPDEL, "KP_DEL" },
  { MP_KEY_KPDEC, "KP_DEC" },
  { MP_KEY_KPINS, "KP_INS" },
  { MP_KEY_KPENTER, "KP_ENTER" },
  { MP_MOUSE_BTN0, "MOUSE_BTN0" },
  { MP_MOUSE_BTN1, "MOUSE_BTN1" },
  { MP_MOUSE_BTN2, "MOUSE_BTN2" },
  { MP_MOUSE_BTN3, "MOUSE_BTN3" },
  { MP_MOUSE_BTN4, "MOUSE_BTN4" },
  { MP_MOUSE_BTN5, "MOUSE_BTN5" },
  { MP_MOUSE_BTN6, "MOUSE_BTN6" },
  { MP_MOUSE_BTN7, "MOUSE_BTN7" },
  { MP_MOUSE_BTN8, "MOUSE_BTN8" },
  { MP_MOUSE_BTN9, "MOUSE_BTN9" },
  { MP_MOUSE_BTN10, "MOUSE_BTN10" },
  { MP_MOUSE_BTN11, "MOUSE_BTN11" },
  { MP_MOUSE_BTN12, "MOUSE_BTN12" },
  { MP_MOUSE_BTN13, "MOUSE_BTN13" },
  { MP_MOUSE_BTN14, "MOUSE_BTN14" },
  { MP_MOUSE_BTN15, "MOUSE_BTN15" },
  { MP_MOUSE_BTN16, "MOUSE_BTN16" },
  { MP_MOUSE_BTN17, "MOUSE_BTN17" },
  { MP_MOUSE_BTN18, "MOUSE_BTN18" },
  { MP_MOUSE_BTN19, "MOUSE_BTN19" },
  { MP_MOUSE_BTN0_DBL, "MOUSE_BTN0_DBL" },
  { MP_MOUSE_BTN1_DBL, "MOUSE_BTN1_DBL" },
  { MP_MOUSE_BTN2_DBL, "MOUSE_BTN2_DBL" },
  { MP_MOUSE_BTN3_DBL, "MOUSE_BTN3_DBL" },
  { MP_MOUSE_BTN4_DBL, "MOUSE_BTN4_DBL" },
  { MP_MOUSE_BTN5_DBL, "MOUSE_BTN5_DBL" },
  { MP_MOUSE_BTN6_DBL, "MOUSE_BTN6_DBL" },
  { MP_MOUSE_BTN7_DBL, "MOUSE_BTN7_DBL" },
  { MP_MOUSE_BTN8_DBL, "MOUSE_BTN8_DBL" },
  { MP_MOUSE_BTN9_DBL, "MOUSE_BTN9_DBL" },
  { MP_MOUSE_BTN10_DBL, "MOUSE_BTN10_DBL" },
  { MP_MOUSE_BTN11_DBL, "MOUSE_BTN11_DBL" },
  { MP_MOUSE_BTN12_DBL, "MOUSE_BTN12_DBL" },
  { MP_MOUSE_BTN13_DBL, "MOUSE_BTN13_DBL" },
  { MP_MOUSE_BTN14_DBL, "MOUSE_BTN14_DBL" },
  { MP_MOUSE_BTN15_DBL, "MOUSE_BTN15_DBL" },
  { MP_MOUSE_BTN16_DBL, "MOUSE_BTN16_DBL" },
  { MP_MOUSE_BTN17_DBL, "MOUSE_BTN17_DBL" },
  { MP_MOUSE_BTN18_DBL, "MOUSE_BTN18_DBL" },
  { MP_MOUSE_BTN19_DBL, "MOUSE_BTN19_DBL" },
  { MP_JOY_AXIS1_MINUS, "JOY_UP" },
  { MP_JOY_AXIS1_PLUS, "JOY_DOWN" },
  { MP_JOY_AXIS0_MINUS, "JOY_LEFT" },
  { MP_JOY_AXIS0_PLUS, "JOY_RIGHT" },

  { MP_JOY_AXIS0_PLUS,  "JOY_AXIS0_PLUS" },
  { MP_JOY_AXIS0_MINUS, "JOY_AXIS0_MINUS" },
  { MP_JOY_AXIS1_PLUS,  "JOY_AXIS1_PLUS" },
  { MP_JOY_AXIS1_MINUS, "JOY_AXIS1_MINUS" },
  { MP_JOY_AXIS2_PLUS,  "JOY_AXIS2_PLUS" },
  { MP_JOY_AXIS2_MINUS, "JOY_AXIS2_MINUS" },
  { MP_JOY_AXIS3_PLUS,  "JOY_AXIS3_PLUS" },
  { MP_JOY_AXIS3_MINUS, "JOY_AXIS3_MINUS" },
  { MP_JOY_AXIS4_PLUS,  "JOY_AXIS4_PLUS" },
  { MP_JOY_AXIS4_MINUS, "JOY_AXIS4_MINUS" },
  { MP_JOY_AXIS5_PLUS,  "JOY_AXIS5_PLUS" },
  { MP_JOY_AXIS5_MINUS, "JOY_AXIS5_MINUS" },
  { MP_JOY_AXIS6_PLUS,  "JOY_AXIS6_PLUS" },
  { MP_JOY_AXIS6_MINUS, "JOY_AXIS6_MINUS" },
  { MP_JOY_AXIS7_PLUS,  "JOY_AXIS7_PLUS" },
  { MP_JOY_AXIS7_MINUS, "JOY_AXIS7_MINUS" },
  { MP_JOY_AXIS8_PLUS,  "JOY_AXIS8_PLUS" },
  { MP_JOY_AXIS8_MINUS, "JOY_AXIS8_MINUS" },
  { MP_JOY_AXIS9_PLUS,  "JOY_AXIS9_PLUS" },
  { MP_JOY_AXIS9_MINUS, "JOY_AXIS9_MINUS" },

  { MP_JOY_BTN0,        "JOY_BTN0" },
  { MP_JOY_BTN1,        "JOY_BTN1" },
  { MP_JOY_BTN2,        "JOY_BTN2" },
  { MP_JOY_BTN3,        "JOY_BTN3" },
  { MP_JOY_BTN4,        "JOY_BTN4" },
  { MP_JOY_BTN5,        "JOY_BTN5" },
  { MP_JOY_BTN6,        "JOY_BTN6" },
  { MP_JOY_BTN7,        "JOY_BTN7" },
  { MP_JOY_BTN8,        "JOY_BTN8" },
  { MP_JOY_BTN9,        "JOY_BTN9" },

  { MP_AR_PLAY,         "AR_PLAY" },
  { MP_AR_PLAY_HOLD,    "AR_PLAY_HOLD" },
  { MP_AR_CENTER,       "AR_CENTER" },
  { MP_AR_CENTER_HOLD,  "AR_CENTER_HOLD" },
  { MP_AR_NEXT,         "AR_NEXT" },
  { MP_AR_NEXT_HOLD,    "AR_NEXT_HOLD" },
  { MP_AR_PREV,         "AR_PREV" },
  { MP_AR_PREV_HOLD,    "AR_PREV_HOLD" },
  { MP_AR_MENU,         "AR_MENU" },
  { MP_AR_MENU_HOLD,    "AR_MENU_HOLD" },
  { MP_AR_VUP,          "AR_VUP" },
  { MP_AR_VUP_HOLD,     "AR_VUP_HOLD" },
  { MP_AR_VDOWN,        "AR_VDOWN" },
  { MP_AR_VDOWN_HOLD,   "AR_VDOWN_HOLD" },

  { MP_MK_PLAY,         "MK_PLAY" },
  { MP_MK_PREV,         "MK_PREV" },
  { MP_MK_NEXT,         "MK_NEXT" },

  { MP_AXIS_UP,         "AXIS_UP" },
  { MP_AXIS_DOWN,       "AXIS_DOWN" },
  { MP_AXIS_LEFT,       "AXIS_LEFT" },
  { MP_AXIS_RIGHT,      "AXIS_RIGHT" },

  { MP_KEY_POWER,       "POWER" },
  { MP_KEY_MENU,        "MENU" },
  { MP_KEY_PLAY,        "PLAY" },
  { MP_KEY_PAUSE,       "PAUSE" },
  { MP_KEY_PLAYPAUSE,   "PLAYPAUSE" },
  { MP_KEY_STOP,        "STOP" },
  { MP_KEY_FORWARD,     "FORWARD" },
  { MP_KEY_REWIND,      "REWIND" },
  { MP_KEY_NEXT,        "NEXT" },
  { MP_KEY_PREV,        "PREV" },
  { MP_KEY_VOLUME_UP,   "VOLUME_UP" },
  { MP_KEY_VOLUME_DOWN, "VOLUME_DOWN" },
  { MP_KEY_MUTE,        "MUTE" },

  // These are kept for backward compatibility
  { MP_KEY_PAUSE,   "XF86_PAUSE" },
  { MP_KEY_STOP,    "XF86_STOP" },
  { MP_KEY_PREV,    "XF86_PREV" },
  { MP_KEY_NEXT,    "XF86_NEXT" },

  { MP_KEY_CLOSE_WIN,   "CLOSE_WIN" },
  { MP_KEY_MOUSE_MOVE,  "MOUSE_MOVE" },
  { MP_KEY_MOUSE_LEAVE, "MOUSE_LEAVE" },

  { 0, NULL }
};

struct key_name modifier_names[] = {
    { MP_KEY_MODIFIER_SHIFT, "Shift" },
    { MP_KEY_MODIFIER_CTRL,  "Ctrl" },
    { MP_KEY_MODIFIER_ALT,   "Alt" },
    { MP_KEY_MODIFIER_META,  "Meta" },
    { 0 }
};

#define MP_MAX_FDS 10

struct input_fd {
    int fd;
    int (*read_key)(void *ctx, int fd);
    int (*read_cmd)(int fd, char *dest, int size);
    int (*close_func)(int fd);
    void *ctx;
    unsigned eof : 1;
    unsigned drop : 1;
    unsigned dead : 1;
    unsigned got_cmd : 1;
    unsigned select : 1;
    // These fields are for the cmd fds.
    char *buffer;
    int pos, size;
};

struct cmd_bind_section {
    struct cmd_bind *binds;
    int num_binds;
    char *section;
    struct mp_rect mouse_area;  // set at runtime, if at all
    bool mouse_area_set;        // mouse_area is valid and should be tested
    struct cmd_bind_section *next;
};

#define MAX_ACTIVE_SECTIONS 5

struct active_section {
    char *name;
    int flags;
};

struct cmd_queue {
    struct mp_cmd *first;
};

struct input_ctx {
    bool using_ar;
    bool using_cocoa_media_keys;

    // Autorepeat stuff
    short ar_state;
    int64_t last_ar;
    // Autorepeat config
    unsigned int ar_delay;
    unsigned int ar_rate;
    // Maximum number of queued commands from keypresses (limit to avoid
    // repeated slow commands piling up)
    int key_fifo_size;

    // these are the keys currently down
    int key_down[MP_MAX_KEY_DOWN];
    unsigned int num_key_down;
    int64_t last_key_down;
    struct mp_cmd *current_down_cmd;

    int doubleclick_time;
    int last_doubleclick_key_down;
    double last_doubleclick_time;

    // Mouse position on the consumer side (as command.c sees it)
    int mouse_x, mouse_y;
    char *mouse_section; // last section to receive mouse event

    // Mouse position on the producer side (as the VO sees it)
    // Unlike mouse_x/y, this can be used to resolve mouse click bindings.
    int mouse_vo_x, mouse_vo_y;

    bool test;

    bool default_bindings;
    // List of command binding sections
    struct cmd_bind_section *cmd_bind_sections;

    // List currently active command sections
    struct active_section active_sections[MAX_ACTIVE_SECTIONS];
    int num_active_sections;

    // Used to track whether we managed to read something while checking
    // events sources. If yes, the sources may have more queued.
    bool got_new_events;

    unsigned int mouse_event_counter;

    struct input_fd fds[MP_MAX_FDS];
    unsigned int num_fds;

    struct cmd_queue key_cmd_queue;
    struct cmd_queue control_cmd_queue;

    int wakeup_pipe[2];
};


int async_quit_request;

static int print_key_list(m_option_t *cfg, char *optname, char *optparam);
static int print_cmd_list(m_option_t *cfg, char *optname, char *optparam);

#define OPT_BASE_STRUCT struct MPOpts

// Our command line options
static const m_option_t input_config[] = {
    OPT_STRING("conf", input.config_file, CONF_GLOBAL),
    OPT_INT("ar-delay", input.ar_delay, CONF_GLOBAL),
    OPT_INT("ar-rate", input.ar_rate, CONF_GLOBAL),
    { "keylist", print_key_list, CONF_TYPE_PRINT_FUNC, CONF_GLOBAL | CONF_NOCFG },
    { "cmdlist", print_cmd_list, CONF_TYPE_PRINT_FUNC, CONF_GLOBAL | CONF_NOCFG },
    OPT_STRING("js-dev", input.js_dev, CONF_GLOBAL),
    OPT_STRING("file", input.in_file, CONF_GLOBAL),
    OPT_FLAG("default-bindings", input.default_bindings, CONF_GLOBAL),
    OPT_FLAG("test", input.test, CONF_GLOBAL),
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

const m_option_t mp_input_opts[] = {
    { "input", (void *)&input_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    OPT_INTRANGE("doubleclick-time", input.doubleclick_time, 0, 0, 1000),
    OPT_FLAG("joystick", input.use_joystick, CONF_GLOBAL),
    OPT_FLAG("lirc", input.use_lirc, CONF_GLOBAL),
    OPT_FLAG("lircc", input.use_lircc, CONF_GLOBAL),
#ifdef CONFIG_COCOA
    OPT_FLAG("ar", input.use_ar, CONF_GLOBAL),
    OPT_FLAG("media-keys", input.use_media_keys, CONF_GLOBAL),
#endif
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

static int default_cmd_func(int fd, char *buf, int l);

static const char builtin_input_conf[] =
#include "mpvcore/input/input_conf.h"
;

static bool test_rect(struct mp_rect *rc, int x, int y)
{
    return x >= rc->x0 && y >= rc->y0 && x < rc->x1 && y < rc->y1;
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
        return mp_append_utf8_buffer(ret, key);

    // Print the hex key code
    return talloc_asprintf_append_buffer(ret, "%#-8x", key);
}

static char *get_key_combo_name(int *keys, int max)
{
    char *ret = talloc_strdup(NULL, "");
    while (max > 0) {
        ret = get_key_name(*keys, ret);
        if (--max && *++keys)
            ret = talloc_asprintf_append_buffer(ret, "-");
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
    queue_lock();
    int res = 0;
    for (struct mp_cmd *cmd = queue->first; cmd; cmd = cmd->queue_next)
        res++;
    queue_unlock();
    return res;
}

static bool queue_has_abort_cmds(struct cmd_queue *queue)
{
    queue_lock();
    bool ret = false;
    for (struct mp_cmd *cmd = queue->first; cmd; cmd = cmd->queue_next)
        if (mp_input_is_abort_cmd(cmd->id)) {
            ret = true;
            break;
        }
    queue_unlock();
    return ret;
}

static void queue_remove(struct cmd_queue *queue, struct mp_cmd *cmd)
{
    queue_lock();
    struct mp_cmd **p_prev = &queue->first;
    while (*p_prev != cmd) {
        p_prev = &(*p_prev)->queue_next;
    }
    // if this fails, cmd was not in the queue
    assert(*p_prev == cmd);
    *p_prev = cmd->queue_next;
    queue_unlock();
}

static void queue_add(struct cmd_queue *queue, struct mp_cmd *cmd,
                      bool at_head)
{
    queue_lock();
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
    queue_unlock();
}

static struct mp_cmd *queue_peek(struct cmd_queue *queue)
{
    struct mp_cmd *ret = NULL;
    queue_lock();
    ret = queue->first;
    queue_unlock();
    return ret;
}

static struct input_fd *mp_input_add_fd(struct input_ctx *ictx)
{
    if (ictx->num_fds == MP_MAX_FDS) {
        mp_tmsg(MSGT_INPUT, MSGL_ERR, "Too many file descriptors.\n");
        return NULL;
    }

    struct input_fd *fd = &ictx->fds[ictx->num_fds];
    *fd = (struct input_fd){
        .fd = -1,
    };
    ictx->num_fds++;

    return fd;
}

int mp_input_add_cmd_fd(struct input_ctx *ictx, int unix_fd, int select,
                        int read_func(int fd, char *dest, int size),
                        int close_func(int fd))
{
    if (select && unix_fd < 0) {
        mp_msg(MSGT_INPUT, MSGL_ERR,
               "Invalid fd %d in mp_input_add_cmd_fd", unix_fd);
        return 0;
    }

    struct input_fd *fd = mp_input_add_fd(ictx);
    if (!fd)
        return 0;
    fd->fd = unix_fd;
    fd->select = select;
    fd->read_cmd = read_func ? read_func : default_cmd_func;
    fd->close_func = close_func;
    return 1;
}

int mp_input_add_key_fd(struct input_ctx *ictx, int unix_fd, int select,
                        int read_func(void *ctx, int fd),
                        int close_func(int fd), void *ctx)
{
    if (select && unix_fd < 0) {
        mp_msg(MSGT_INPUT, MSGL_ERR,
               "Invalid fd %d in mp_input_add_key_fd", unix_fd);
        return 0;
    }
    assert(read_func);

    struct input_fd *fd = mp_input_add_fd(ictx);
    if (!fd)
        return 0;
    fd->fd = unix_fd;
    fd->select = select;
    fd->read_key = read_func;
    fd->close_func = close_func;
    fd->ctx = ctx;
    return 1;
}


static void mp_input_rm_fd(struct input_ctx *ictx, int fd)
{
    struct input_fd *fds = ictx->fds;
    unsigned int i;

    for (i = 0; i < ictx->num_fds; i++) {
        if (fds[i].fd == fd)
            break;
    }
    if (i == ictx->num_fds)
        return;
    if (fds[i].close_func)
        fds[i].close_func(fds[i].fd);
    talloc_free(fds[i].buffer);

    if (i + 1 < ictx->num_fds)
        memmove(&fds[i], &fds[i + 1],
                (ictx->num_fds - i - 1) * sizeof(struct input_fd));
    ictx->num_fds--;
}

void mp_input_rm_key_fd(struct input_ctx *ictx, int fd)
{
    mp_input_rm_fd(ictx, fd);
}

static int parse_cycle_dir(const struct m_option *opt, struct bstr name,
                           struct bstr param, void *dst)
{
    double val;
    if (bstrcmp0(param, "up") == 0) {
        val = +1;
    } else if (bstrcmp0(param, "down") == 0) {
        val = -1;
    } else {
        return m_option_type_double.parse(opt, name, param, dst);
    }
    *(double *)dst = val;
    return 1;
}

static bool read_token(bstr str, bstr *out_rest, bstr *out_token)
{
    bstr t = bstr_lstrip(str);
    int next = bstrcspn(t, WHITESPACE "#");
    // Handle comments
    if (t.len && t.start[next] == '#')
        t = bstr_splice(t, 0, next);
    if (!t.len)
        return false;
    *out_token = bstr_splice(t, 0, next);
    *out_rest = bstr_cut(t, next);
    return true;
}

static bool eat_token(bstr *str, const char *tok)
{
    bstr rest, token;
    if (read_token(*str, &rest, &token) && bstrcmp0(token, tok) == 0) {
        *str = rest;
        return true;
    }
    return false;
}

static bool read_escaped_string(void *talloc_ctx, bstr *str, bstr *literal)
{
    bstr t = *str;
    char *new = talloc_strdup(talloc_ctx, "");
    while (t.len) {
        if (t.start[0] == '"')
            break;
        if (t.start[0] == '\\') {
            t = bstr_cut(t, 1);
            if (!mp_parse_escape(&t, &new))
                goto error;
        } else {
            new = talloc_strndup_append_buffer(new, t.start, 1);
            t = bstr_cut(t, 1);
        }
    }
    int len = str->len - t.len;
    *literal = new ? bstr0(new) : bstr_splice(*str, 0, len);
    *str = bstr_cut(*str, len);
    return true;
error:
    talloc_free(new);
    return false;
}

// If dest is non-NULL when calling this function, append the command to the
// list formed by dest->queue_next, otherwise just set *dest = new_cmd;
static int parse_cmd(struct mp_cmd **dest, bstr str, const char *loc)
{
    int pausing = 0;
    int on_osd = MP_ON_OSD_AUTO;
    bool raw_args = false;
    struct mp_cmd *cmd = NULL;
    bstr start = str;
    bstr next = {0};
    void *tmp = talloc_new(NULL);

    str = bstr_lstrip(str);
    for (const struct legacy_cmd *entry = legacy_cmds; entry->old; entry++) {
        bstr old = bstr0(entry->old);
        bool silent = bstr_eatstart0(&old, "!");
        if (bstrcasecmp(bstr_splice(str, 0, old.len), old) == 0) {
            if (!silent) {
                mp_tmsg(MSGT_INPUT, MSGL_WARN, "Warning: command '%.*s' is "
                        "deprecated, replaced with '%s' at %s.\n",
                        BSTR_P(old), entry->new, loc);
            }
            bstr s = bstr_cut(str, old.len);
            str = bstr0(talloc_asprintf(tmp, "%s%.*s", entry->new, BSTR_P(s)));
            start = str;
            break;
        }
    }

    while (1) {
        if (eat_token(&str, "pausing")) {
            pausing = 1;
        } else if (eat_token(&str, "pausing_keep")) {
            pausing = 2;
        } else if (eat_token(&str, "pausing_toggle")) {
            pausing = 3;
        } else if (eat_token(&str, "pausing_keep_force")) {
            pausing = 4;
        } else if (eat_token(&str, "no-osd")) {
            on_osd = MP_ON_OSD_NO;
        } else if (eat_token(&str, "osd-bar")) {
            on_osd = MP_ON_OSD_BAR;
        } else if (eat_token(&str, "osd-msg")) {
            on_osd = MP_ON_OSD_MSG;
        } else if (eat_token(&str, "osd-msg-bar")) {
            on_osd = MP_ON_OSD_MSG | MP_ON_OSD_BAR;
        } else if (eat_token(&str, "osd-auto")) {
            // default
        } else if (eat_token(&str, "raw")) {
            raw_args = true;
        } else if (eat_token(&str, "expand-properties")) {
            // default
        } else {
            break;
        }
    }

    int cmd_idx = 0;
    while (mp_cmds[cmd_idx].name != NULL) {
        if (eat_token(&str, mp_cmds[cmd_idx].name))
            break;
        cmd_idx++;
    }

    if (mp_cmds[cmd_idx].name == NULL) {
        mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command '%.*s' not found.\n",
                BSTR_P(str));
        goto error;
    }

    cmd = talloc_ptrtype(NULL, cmd);
    *cmd = mp_cmds[cmd_idx];
    cmd->scale = 1;
    cmd->pausing = pausing;
    cmd->on_osd = on_osd;
    cmd->raw_args = raw_args;

    for (int i = 0; i < MP_CMD_MAX_ARGS; i++) {
        struct mp_cmd_arg *cmdarg = &cmd->args[i];
        if (!cmdarg->type.type)
            break;
        str = bstr_lstrip(str);
        if (eat_token(&str, ";")) {
            next = str;
            str.len = 0;
            break;
        }
        cmd->nargs++;
        bstr arg = {0};
        if (bstr_eatstart0(&str, "\"")) {
            if (!read_escaped_string(tmp, &str, &arg)) {
                mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command %s: argument %d "
                        "has broken string escapes.\n", cmd->name, i + 1);
                goto error;
            }
            if (!bstr_eatstart0(&str, "\"")) {
                mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command %s: argument %d is "
                        "unterminated.\n", cmd->name, i + 1);
                goto error;
            }
        } else {
            if (!read_token(str, &str, &arg))
                break;
            if (cmdarg->optional && bstrcmp0(arg, "-") == 0)
                continue;
        }
        // Prevent option API from trying to deallocate static strings
        cmdarg->v = ((struct mp_cmd_arg) {{0}}).v;
        int r = m_option_parse(&cmdarg->type, bstr0(cmd->name), arg, &cmdarg->v);
        if (r < 0) {
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command %s: argument %d "
                    "can't be parsed: %s.\n", cmd->name, i + 1,
                    m_option_strerror(r));
            goto error;
        }
        if (cmdarg->type.type == &m_option_type_string)
            cmdarg->v.s = talloc_steal(cmd, cmdarg->v.s);
    }

    if (eat_token(&str, ";")) {
        next = str;
        str.len = 0;
    }

    bstr dummy;
    if (read_token(str, &dummy, &dummy)) {
        mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command %s has trailing unused "
                "arguments: '%.*s'.\n", cmd->name, BSTR_P(str));
        // Better make it fatal to make it clear something is wrong.
        goto error;
    }

    int min_args = 0;
    while (min_args < MP_CMD_MAX_ARGS && cmd->args[min_args].type.type
           && !cmd->args[min_args].optional)
    {
        min_args++;
    }
    if (cmd->nargs < min_args) {
        mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command %s requires at least %d "
                "arguments, we found only %d so far.\n", cmd->name, min_args,
                cmd->nargs);
        goto error;
    }

    bstr orig = (bstr) {start.start, str.start - start.start};
    cmd->original = bstrdup(cmd, bstr_strip(orig));

    while (*dest)
        dest = &(*dest)->queue_next;
    *dest = cmd;

    next = bstr_strip(next);
    if (next.len) {
        if (parse_cmd(dest, next, loc) < 0) {
            *dest = NULL;
            goto error;
        }
    }

    talloc_free(tmp);
    return 1;

error:
    mp_tmsg(MSGT_INPUT, MSGL_ERR, "Command was defined at %s.\n", loc);
    talloc_free(cmd);
    talloc_free(tmp);
    return -1;
}

mp_cmd_t *mp_input_parse_cmd(bstr str, const char *loc)
{
    struct mp_cmd *cmd = NULL;
    if (parse_cmd(&cmd, str, loc) < 0) {
        assert(!cmd);
    }
    // Other input.c code uses queue_next for its own purposes, so explicitly
    // wrap lists in a pseudo-command.
    if (cmd && cmd->queue_next) {
        struct mp_cmd *list = talloc_ptrtype(NULL, list);
        *list = (struct mp_cmd) {
            .id = MP_CMD_COMMAND_LIST,
            .name = "list",
            .original = bstrdup(list, str),
        };
        list->args[0].v.p = cmd;
        while (cmd) {
            talloc_steal(list, cmd);
            cmd = cmd->queue_next;
        }
        cmd = list;
    }
    return cmd;
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
        int r = mp_fd->read_cmd(mp_fd->fd, mp_fd->buffer + mp_fd->pos,
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

#ifndef __MINGW32__
static int read_wakeup(void *ctx, int fd)
{
    char buf[100];
    read(fd, buf, sizeof(buf));
    return MP_INPUT_NOTHING;
}
#endif

static bool bind_matches_key(struct cmd_bind *bind, int n, const int *keys);

static void append_bind_info(char **pmsg, struct cmd_bind *bind)
{
    char *msg = *pmsg;
    struct mp_cmd *cmd = mp_input_parse_cmd(bstr0(bind->cmd), bind->location);
    bstr stripped = cmd ? cmd->original : bstr0(bind->cmd);
    msg = talloc_asprintf_append(msg, " '%.*s'", BSTR_P(stripped));
    if (!cmd)
        msg = talloc_asprintf_append(msg, " (invalid)");
    if (strcmp(bind->owner->section, "default") != 0)
        msg = talloc_asprintf_append(msg, " in section {%s}",
                                     bind->owner->section);
    msg = talloc_asprintf_append(msg, " in %s", bind->location);
    if (bind->is_builtin)
        msg = talloc_asprintf_append(msg, " (default)");
    talloc_free(cmd);
    *pmsg = msg;
}

static mp_cmd_t *handle_test(struct input_ctx *ictx, int n, int *keys)
{
    char *key_buf = get_key_combo_name(keys, n);
    // "$>" to disable property substitution when invoking "show_text"
    char *msg = talloc_asprintf(NULL, "$>Key %s is bound to:\n", key_buf);
    talloc_free(key_buf);

    int count = 0;
    for (struct cmd_bind_section *bs = ictx->cmd_bind_sections;
         bs; bs = bs->next)
    {
        for (int i = 0; i < bs->num_binds; i++) {
            if (bind_matches_key(&bs->binds[i], n, keys)) {
                count++;
                msg = talloc_asprintf_append(msg, "%d. ", count);
                append_bind_info(&msg, &bs->binds[i]);
                msg = talloc_asprintf_append(msg, "\n");
            }
        }
    }

    if (!count)
        msg = talloc_asprintf_append(msg, "(nothing)");

    mp_msg(MSGT_INPUT, MSGL_V, "[input] %s\n", msg);

    mp_cmd_t *res = mp_input_parse_cmd(bstr0("show_text \"\""), "");
    res->args[0].v.s = talloc_steal(res, msg);
    return res;
}

static bool bind_matches_key(struct cmd_bind *bind, int num_keys, const int *keys)
{
    if (bind->num_keys != num_keys)
        return false;
    for (int i = 0; i < num_keys; i++) {
        if (bind->keys[i] != keys[i])
            return false;
    }
    return true;
}

static struct cmd_bind_section *get_bind_section(struct input_ctx *ictx,
                                                 bstr section)
{
    struct cmd_bind_section *bind_section = ictx->cmd_bind_sections;

    if (section.len == 0)
        section = bstr0("default");
    while (bind_section) {
        if (bstrcmp0(section, bind_section->section) == 0)
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
    *bind_section = (struct cmd_bind_section) {
        .section = bstrdup0(bind_section, section),
    };
    return bind_section;
}

static struct cmd_bind *find_bind_for_key_section(struct input_ctx *ictx,
                                                  char *section,
                                                  int num_keys, int *keys)
{
    struct cmd_bind_section *bs = get_bind_section(ictx, bstr0(section));

    if (!num_keys || !bs->num_binds)
        return NULL;

    // Prefer user-defined keys over builtin bindings
    for (int builtin = 0; builtin < 2; builtin++) {
        for (int n = 0; n < bs->num_binds; n++) {
            if (bs->binds[n].is_builtin == (bool)builtin &&
                bind_matches_key(&bs->binds[n], num_keys, keys))
                return &bs->binds[n];
        }
    }
    return NULL;
}

static struct cmd_bind *find_any_bind_for_key(struct input_ctx *ictx,
                                              char *force_section,
                                              int n, int *keys)
{
    if (force_section)
        return find_bind_for_key_section(ictx, force_section, n, keys);

    for (int i = ictx->num_active_sections - 1; i >= 0; i--) {
        struct active_section *s = &ictx->active_sections[i];
        struct cmd_bind *bind = find_bind_for_key_section(ictx, s->name, n, keys);
        if (bind) {
            struct cmd_bind_section *bs = bind->owner;
            for (int x = 0; x < n; x++) {
                if (MP_KEY_DEPENDS_ON_MOUSE_POS(keys[x]) && bs->mouse_area_set &&
                    !test_rect(&bs->mouse_area, ictx->mouse_vo_x, ictx->mouse_vo_y))
                    goto skip;
            }
            return bind;
        }
    skip: ;
        if (s->flags & MP_INPUT_EXCLUSIVE)
            break;
    }

    return NULL;
}

static mp_cmd_t *get_cmd_from_keys(struct input_ctx *ictx, char *force_section,
                                   int n, int *keys)
{
    if (ictx->test)
        return handle_test(ictx, n, keys);

    struct cmd_bind *cmd = find_any_bind_for_key(ictx, force_section, n, keys);
    if (cmd == NULL && n > 1) {
        // Hitting two keys at once, and if there's no binding for this
        // combination, the key hit last should be checked.
        cmd = find_any_bind_for_key(ictx, force_section, 1, (int[]){keys[n - 1]});
    }

    if (cmd == NULL) {
        char *key_buf = get_key_combo_name(keys, n);
        mp_tmsg(MSGT_INPUT, MSGL_WARN,
                "No bind found for key '%s'.\n", key_buf);
        talloc_free(key_buf);
        return NULL;
    }
    mp_cmd_t *ret = mp_input_parse_cmd(bstr0(cmd->cmd), cmd->location);
    if (ret) {
        ret->input_section = cmd->owner->section;
    } else {
        char *key_buf = get_key_combo_name(keys, n);
        mp_tmsg(MSGT_INPUT, MSGL_ERR,
                "Invalid command for bound key '%s': '%s'\n", key_buf, cmd->cmd);
        talloc_free(key_buf);
    }
    return ret;
}

static void release_down_cmd(struct input_ctx *ictx)
{
    if (ictx->current_down_cmd && ictx->current_down_cmd->key_up_follows) {
        ictx->current_down_cmd->key_up_follows = false;
        queue_add(&ictx->key_cmd_queue, ictx->current_down_cmd, false);
    } else {
        talloc_free(ictx->current_down_cmd);
    }
    ictx->current_down_cmd = NULL;
    ictx->last_key_down = 0;
    ictx->ar_state = -1;
}

static int find_key_down(struct input_ctx *ictx, int code)
{
    code &= ~(MP_KEY_STATE_UP | MP_KEY_STATE_DOWN);
    for (int j = 0; j < ictx->num_key_down; j++) {
        if (ictx->key_down[j] == code)
            return j;
    }
    return -1;
}

static void remove_key_down(struct input_ctx *ictx, int code)
{
    int index = find_key_down(ictx, code);
    if (index >= 0) {
        memmove(&ictx->key_down[index], &ictx->key_down[index + 1],
                (ictx->num_key_down - (index + 1)) * sizeof(int));
        ictx->num_key_down -= 1;
    }
}

static mp_cmd_t *interpret_key(struct input_ctx *ictx, int code)
{
    /* On normal keyboards shift changes the character code of non-special
     * keys, so don't count the modifier separately for those. In other words
     * we want to have "a" and "A" instead of "a" and "Shift+A"; but a separate
     * shift modifier is still kept for special keys like arrow keys.
     */
    int unmod = code & ~MP_KEY_MODIFIER_MASK;
    if (unmod >= 32 && unmod < MP_KEY_BASE)
        code &= ~MP_KEY_MODIFIER_SHIFT;

    if (!(code & MP_KEY_STATE_UP) && ictx->num_key_down >= MP_MAX_KEY_DOWN) {
        mp_tmsg(MSGT_INPUT, MSGL_ERR, "Too many key down events "
                "at the same time\n");
        return NULL;
    }

    bool key_was_down = find_key_down(ictx, code) >= 0;

    if (code & MP_KEY_STATE_DOWN) {
        // Check if we don't already have this key as pushed
        if (key_was_down)
            return NULL;
        // Cancel current down-event (there can be only one)
        release_down_cmd(ictx);
        ictx->key_down[ictx->num_key_down] = code & ~MP_KEY_STATE_DOWN;
        ictx->num_key_down++;
        ictx->last_key_down = mp_time_us();
        ictx->ar_state = 0;
        ictx->current_down_cmd = get_cmd_from_keys(ictx, NULL, ictx->num_key_down,
                                                   ictx->key_down);
        if (ictx->current_down_cmd && (code & MP_KEY_EMIT_ON_UP))
            ictx->current_down_cmd->key_up_follows = true;
        return mp_cmd_clone(ictx->current_down_cmd);
    } else if (code & MP_KEY_STATE_UP) {
        if (key_was_down) {
            remove_key_down(ictx, code);
            release_down_cmd(ictx);
        }
        return NULL;
    } else {
        // Press of key with no separate down/up events
        if (key_was_down) {
            // Mixing press events and up/down with the same key is not allowed
            mp_tmsg(MSGT_INPUT, MSGL_WARN, "Mixing key presses and up/down.\n");
        }
        // Add temporarily (include ongoing down/up events)
        int num_key_down = ictx->num_key_down;
        int key_down[MP_MAX_KEY_DOWN];
        memcpy(key_down, ictx->key_down, num_key_down * sizeof(int));
        // Assume doubleclick events never use down/up, while button events do
        if (MP_KEY_IS_MOUSE_BTN_DBL(code)) {
            // Don't emit "MOUSE_BTN0+MOUSE_BTN0_DBL", just "MOUSE_BTN0_DBL"
            int btn = code - MP_MOUSE_BTN0_DBL + MP_MOUSE_BTN0;
            if (!num_key_down || key_down[num_key_down - 1] != btn)
                return NULL;
            key_down[num_key_down - 1] = code;
        } else {
            key_down[num_key_down] = code;
            num_key_down++;
        }
        return get_cmd_from_keys(ictx, NULL, num_key_down, key_down);
    }
}

static mp_cmd_t *check_autorepeat(struct input_ctx *ictx)
{
    // No input : autorepeat ?
    if (ictx->ar_rate > 0 && ictx->ar_state >= 0 && ictx->num_key_down > 0
        && !(ictx->key_down[ictx->num_key_down - 1] & MP_NO_REPEAT_KEY)) {
        int64_t t = mp_time_us();
        if (ictx->last_ar + 2000000 < t)
            ictx->last_ar = t;
        // First time : wait delay
        if (ictx->ar_state == 0
            && (t - ictx->last_key_down) >= ictx->ar_delay * 1000)
        {
            if (!ictx->current_down_cmd) {
                ictx->ar_state = -1;
                return NULL;
            }
            ictx->ar_state = 1;
            ictx->last_ar = ictx->last_key_down + ictx->ar_delay * 1000;
            return mp_cmd_clone(ictx->current_down_cmd);
            // Then send rate / sec event
        } else if (ictx->ar_state == 1
                   && (t - ictx->last_ar) >= 1000000 / ictx->ar_rate) {
            ictx->last_ar += 1000000 / ictx->ar_rate;
            return mp_cmd_clone(ictx->current_down_cmd);
        }
    }
    return NULL;
}

static void add_key_cmd(struct input_ctx *ictx, struct mp_cmd *cmd)
{
    struct cmd_queue *queue = &ictx->key_cmd_queue;
    if (queue_count_cmds(queue) >= ictx->key_fifo_size &&
            (!mp_input_is_abort_cmd(cmd->id) || queue_has_abort_cmds(queue)))
    {
        talloc_free(cmd);
        return;
    }
    queue_add(queue, cmd, false);
}

// Whether a command can deal with redundant key up events.
static bool key_updown_ok(enum mp_command_type cmd)
{
    switch (cmd) {
    default:
        return false;
    }
}

static void mp_input_feed_key(struct input_ctx *ictx, int code)
{
    ictx->got_new_events = true;
    if (code == MP_INPUT_RELEASE_ALL) {
        mp_msg(MSGT_INPUT, MSGL_V, "input: release all\n");
        ictx->num_key_down = 0;
        release_down_cmd(ictx);
        return;
    }
    int unmod = code & ~MP_KEY_MODIFIER_MASK;
    if (MP_KEY_DEPENDS_ON_MOUSE_POS(unmod))
        ictx->mouse_event_counter++;
    mp_msg(MSGT_INPUT, MSGL_V, "input: key code=%#x\n", code);
    struct mp_cmd *cmd = interpret_key(ictx, code);
    if (!cmd)
        return;
    // Prevent redundant key-down events from being added to the queue. In some
    // cases (like MP_CMD_SEEK commands), duplicated events might severely
    // confuse the frontend.
    if (cmd->key_up_follows && !key_updown_ok(cmd->id)) {
        talloc_free(cmd);
        return;
    }
    add_key_cmd(ictx, cmd);
}

void mp_input_put_key(struct input_ctx *ictx, int code)
{
    double now = mp_time_sec();
    int doubleclick_time = ictx->doubleclick_time;
    // ignore system-doubleclick if we generate these events ourselves
    int unmod = code & ~MP_KEY_MODIFIER_MASK;
    if (doubleclick_time && MP_KEY_IS_MOUSE_BTN_DBL(unmod))
        return;
    mp_input_feed_key(ictx, code);
    if (code & MP_KEY_STATE_DOWN) {
        code &= ~MP_KEY_STATE_DOWN;
        if (ictx->last_doubleclick_key_down == code
            && now - ictx->last_doubleclick_time < doubleclick_time / 1000.0)
        {
            if (code >= MP_MOUSE_BTN0 && code <= MP_MOUSE_BTN2)
                mp_input_feed_key(ictx, code - MP_MOUSE_BTN0 + MP_MOUSE_BTN0_DBL);
        }
        ictx->last_doubleclick_key_down = code;
        ictx->last_doubleclick_time = now;
    }
}

void mp_input_put_key_utf8(struct input_ctx *ictx, int mods, struct bstr t)
{
    while (t.len) {
        int code = bstr_decode_utf8(t, &t);
        if (code < 0)
            break;
        mp_input_put_key(ictx, code | mods);
    }
}

void mp_input_put_axis(struct input_ctx *ictx, int direction, double value)
{
    struct mp_cmd *cmd = interpret_key(ictx, direction);
    if (!cmd)
        return;

    cmd->scale = value;

    ictx->got_new_events = true;
    add_key_cmd(ictx, cmd);
}

static void trigger_mouse_leave(struct input_ctx *ictx, char *new_section)
{
    if (!new_section)
        new_section = "default";

    char *old = ictx->mouse_section;
    ictx->mouse_section = new_section;

    if (old && strcmp(old, ictx->mouse_section) != 0) {
        struct mp_cmd *cmd =
            get_cmd_from_keys(ictx, old, 1, (int[]){MP_KEY_MOUSE_LEAVE});
        if (cmd)
            add_key_cmd(ictx, cmd);
    }
}


void mp_input_set_mouse_pos(struct input_ctx *ictx, int x, int y)
{
    // we're already there
    if (ictx->mouse_vo_x == x && ictx->mouse_vo_y == y)
        return;

    ictx->mouse_event_counter++;
    ictx->mouse_vo_x = x;
    ictx->mouse_vo_y = y;

    struct mp_cmd *cmd =
        get_cmd_from_keys(ictx, NULL, 1, (int[]){MP_KEY_MOUSE_MOVE});

    trigger_mouse_leave(ictx, cmd ? cmd->input_section : NULL);

    if (!cmd)
        return;
    cmd->mouse_move = true;
    cmd->mouse_x = x;
    cmd->mouse_y = y;
    add_key_cmd(ictx, cmd);
}

static void read_cmd_fd(struct input_ctx *ictx, struct input_fd *cmd_fd)
{
    int r;
    char *text;
    while ((r = read_cmd(cmd_fd, &text)) >= 0) {
        ictx->got_new_events = true;
        struct mp_cmd *cmd = mp_input_parse_cmd(bstr0(text), "<pipe>");
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
    int code = key_fd->read_key(key_fd->ctx, key_fd->fd);
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

static void read_fd(struct input_ctx *ictx, struct input_fd *fd)
{
    if (fd->read_cmd) {
        read_cmd_fd(ictx, fd);
    } else {
        read_key_fd(ictx, fd);
    }
}

static void remove_dead_fds(struct input_ctx *ictx)
{
    for (int i = 0; i < ictx->num_fds; i++) {
        if (ictx->fds[i].dead) {
            mp_input_rm_fd(ictx, ictx->fds[i].fd);
            i--;
        }
    }
}

#ifdef HAVE_POSIX_SELECT

static void input_wait_read(struct input_ctx *ictx, int time)
{
    fd_set fds;
    FD_ZERO(&fds);
    int max_fd = 0;
    for (int i = 0; i < ictx->num_fds; i++) {
        if (!ictx->fds[i].select)
            continue;
        if (ictx->fds[i].fd > max_fd)
            max_fd = ictx->fds[i].fd;
        FD_SET(ictx->fds[i].fd, &fds);
    }
    struct timeval tv, *time_val;
    tv.tv_sec = time / 1000;
    tv.tv_usec = (time % 1000) * 1000;
    time_val = &tv;
    if (select(max_fd + 1, &fds, NULL, NULL, time_val) < 0) {
        if (errno != EINTR)
            mp_tmsg(MSGT_INPUT, MSGL_ERR, "Select error: %s\n",
                    strerror(errno));
        FD_ZERO(&fds);
    }
    for (int i = 0; i < ictx->num_fds; i++) {
        if (ictx->fds[i].select && !FD_ISSET(ictx->fds[i].fd, &fds))
            continue;
        read_fd(ictx, &ictx->fds[i]);
    }
}

#else

static void input_wait_read(struct input_ctx *ictx, int time)
{
    if (time > 0)
        mp_sleep_us(time * 1000);

    for (int i = 0; i < ictx->num_fds; i++)
        read_fd(ictx, &ictx->fds[i]);
}

#endif

/**
 * \param time time to wait at most for an event in milliseconds
 */
static void read_events(struct input_ctx *ictx, int time)
{
    if (ictx->num_key_down) {
        time = FFMIN(time, 1000 / ictx->ar_rate);
        time = FFMIN(time, ictx->ar_delay);
    }
    time = FFMAX(time, 0);

    while (1) {
        if (ictx->got_new_events)
            time = 0;
        ictx->got_new_events = false;

        remove_dead_fds(ictx);

        if (time) {
            for (int i = 0; i < ictx->num_fds; i++) {
                if (!ictx->fds[i].select)
                    read_fd(ictx, &ictx->fds[i]);
            }
        }

        if (ictx->got_new_events)
            time = 0;

        input_wait_read(ictx, time);

        // Read until all input FDs are empty
        if (!ictx->got_new_events)
            break;
    }
}

static void read_all_events(struct input_ctx *ictx, int time)
{
    getch2_poll();
    read_events(ictx, time);
}

int mp_input_queue_cmd(struct input_ctx *ictx, mp_cmd_t *cmd)
{
    ictx->got_new_events = true;
    if (!cmd)
        return 0;
    queue_add(&ictx->control_cmd_queue, cmd, false);
    return 1;
}

/**
 * \param peek_only when set, the returned command stays in the queue.
 * Do not free the returned cmd whe you set this!
 */
mp_cmd_t *mp_input_get_cmd(struct input_ctx *ictx, int time, int peek_only)
{
    if (async_quit_request) {
        struct mp_cmd *cmd = mp_input_parse_cmd(bstr0("quit 1"), "");
        queue_add(&ictx->control_cmd_queue, cmd, true);
    }

    if (ictx->control_cmd_queue.first || ictx->key_cmd_queue.first)
        time = 0;
    read_all_events(ictx, time);
    struct cmd_queue *queue = &ictx->control_cmd_queue;
    if (!queue->first)
        queue = &ictx->key_cmd_queue;
    if (!queue->first) {
        struct mp_cmd *repeated = check_autorepeat(ictx);
        if (repeated)
            queue_add(queue, repeated, false);
    }
    struct mp_cmd *ret = queue_peek(queue);
    if (!ret)
        return NULL;

    if (!peek_only) {
        queue_remove(queue, ret);
        if (ret->mouse_move) {
            ictx->mouse_x = ret->mouse_x;
            ictx->mouse_y = ret->mouse_y;
        }
    }

    return ret;
}

void mp_input_get_mouse_pos(struct input_ctx *ictx, int *x, int *y)
{
    *x = ictx->mouse_x;
    *y = ictx->mouse_y;
}

void mp_cmd_free(mp_cmd_t *cmd)
{
    talloc_free(cmd);
}

mp_cmd_t *mp_cmd_clone(mp_cmd_t *cmd)
{
    mp_cmd_t *ret;
    int i;

    if (!cmd)
        return NULL;

    ret = talloc_memdup(NULL, cmd, sizeof(mp_cmd_t));
    ret->name = talloc_strdup(ret, cmd->name);
    for (i = 0; i < MP_CMD_MAX_ARGS; i++) {
        if (cmd->args[i].type.type == &m_option_type_string)
            ret->args[i].v.s = talloc_strdup(ret, cmd->args[i].v.s);
    }

    if (cmd->id == MP_CMD_COMMAND_LIST) {
        struct mp_cmd *prev = NULL;
        for (struct mp_cmd *sub = cmd->args[0].v.p; sub; sub = sub->queue_next) {
            sub = mp_cmd_clone(sub);
            talloc_steal(ret, sub);
            if (prev) {
                prev->queue_next = sub;
            } else {
                ret->args[0].v.p = sub;
            }
            prev = sub;
        }
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

static int get_input_from_name(char *name, int *out_num_keys, int *keys)
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
    *out_num_keys = n;
    return 1;
}

static void bind_dealloc(struct cmd_bind *bind)
{
    talloc_free(bind->cmd);
    talloc_free(bind->location);
}

static void bind_keys(struct input_ctx *ictx, bool builtin, bstr section,
                      const int *keys, int num_keys, bstr command,
                      const char *loc)
{
    struct cmd_bind_section *bs = get_bind_section(ictx, section);
    struct cmd_bind *bind = NULL;

    assert(num_keys <= MP_MAX_KEY_DOWN);

    for (int n = 0; n < bs->num_binds; n++) {
        struct cmd_bind *b = &bs->binds[n];
        if (bind_matches_key(b, num_keys, keys) && b->is_builtin == builtin) {
            bind = b;
            break;
        }
    }

    if (!bind) {
        struct cmd_bind empty = {{0}};
        MP_TARRAY_APPEND(bs, bs->binds, bs->num_binds, empty);
        bind = &bs->binds[bs->num_binds - 1];
    }

    bind_dealloc(bind);

    *bind = (struct cmd_bind) {
        .cmd = bstrdup0(bs->binds, command),
        .location = talloc_strdup(bs->binds, loc),
        .owner = bs,
        .is_builtin = builtin,
        .num_keys = num_keys,
    };
    memcpy(bind->keys, keys, num_keys * sizeof(bind->keys[0]));
}

// restrict_section: every entry is forced to this section name
//                   if NULL, load normally and allow any sections
static int parse_config(struct input_ctx *ictx, bool builtin, bstr data,
                        const char *location, const char *restrict_section)
{
    int n_binds = 0;
    int line_no = 0;
    char *cur_loc = NULL;

    while (data.len) {
        line_no++;
        if (cur_loc)
            talloc_free(cur_loc);
        cur_loc = talloc_asprintf(NULL, "%s:%d", location, line_no);

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
                    "Unfinished key binding: %.*s at %s\n", BSTR_P(line),
                    cur_loc);
            continue;
        }
        char *name = bstrdup0(NULL, keyname);
        int keys[MP_MAX_KEY_DOWN];
        int num_keys = 0;
        if (!get_input_from_name(name, &num_keys, keys)) {
            talloc_free(name);
            mp_tmsg(MSGT_INPUT, MSGL_ERR,
                    "Unknown key '%.*s' at %s\n", BSTR_P(keyname), cur_loc);
            continue;
        }
        talloc_free(name);

        bstr section = bstr0(restrict_section);
        if (!section.len) {
            if (bstr_startswith0(command, "{")) {
                int p = bstrchr(command, '}');
                if (p != -1) {
                    section = bstr_strip(bstr_splice(command, 1, p));
                    command = bstr_lstrip(bstr_cut(command, p + 1));
                }
            }
        }

        bind_keys(ictx, builtin, section, keys, num_keys, command, cur_loc);
        n_binds++;

        // Print warnings if invalid commands are encountered.
        talloc_free(mp_input_parse_cmd(command, cur_loc));
    }

    talloc_free(cur_loc);

    return n_binds;
}

static int parse_config_file(struct input_ctx *ictx, char *file, bool warn)
{
    if (!mp_path_exists(file)) {
        mp_msg(MSGT_INPUT, warn ? MSGL_ERR : MSGL_V,
               "Input config file %s not found.\n", file);
        return 0;
    }
    stream_t *s = stream_open(file, NULL);
    if (!s) {
        mp_msg(MSGT_INPUT, MSGL_ERR, "Can't open input config file %s.\n", file);
        return 0;
    }
    bstr res = stream_read_complete(s, NULL, 1000000);
    free_stream(s);
    mp_msg(MSGT_INPUT, MSGL_V, "Parsing input config file %s\n", file);
    int n_binds = parse_config(ictx, false, res, file, NULL);
    talloc_free(res.start);
    mp_msg(MSGT_INPUT, MSGL_V, "Input config file %s parsed: %d binds\n",
           file, n_binds);
    return 1;
}

// If name is NULL, return "default".
// Return a statically allocated name of the section (i.e. return value never
// gets deallocated).
static char *normalize_section(struct input_ctx *ictx, char *name)
{
    return get_bind_section(ictx, bstr0(name))->section;
}

void mp_input_disable_section(struct input_ctx *ictx, char *name)
{
    name = normalize_section(ictx, name);

    // Remove old section, or make sure it's on top if re-enabled
    for (int i = ictx->num_active_sections - 1; i >= 0; i--) {
        struct active_section *as = &ictx->active_sections[i];
        if (strcmp(as->name, name) == 0) {
            for (int x = i; i < ictx->num_active_sections - 1; i++)
                ictx->active_sections[x] = ictx->active_sections[x + 1];
            ictx->num_active_sections--;
        }
    }
}

void mp_input_enable_section(struct input_ctx *ictx, char *name, int flags)
{
    name = normalize_section(ictx, name);

    mp_input_disable_section(ictx, name);

    if (ictx->num_active_sections < MAX_ACTIVE_SECTIONS) {
        ictx->active_sections[ictx->num_active_sections++] =
            (struct active_section) {name, flags};
    }
}

void mp_input_disable_all_sections(struct input_ctx *ictx)
{
    ictx->num_active_sections = 0;
}

void mp_input_set_section_mouse_area(struct input_ctx *ictx, char *name,
                                     int x0, int y0, int x1, int y1)
{
    struct cmd_bind_section *s = get_bind_section(ictx, bstr0(name));
    s->mouse_area = (struct mp_rect){x0, y0, x1, y1};
    s->mouse_area_set = x0 != x1 && y0 != y1;
}

bool mp_input_test_mouse_active(struct input_ctx *ictx, int x, int y)
{
    for (int i = 0; i < ictx->num_active_sections; i++) {
        char *name = ictx->active_sections[i].name;
        struct cmd_bind_section *s = get_bind_section(ictx, bstr0(name));
        if (s->mouse_area_set && test_rect(&s->mouse_area, x, y))
            return true;
    }
    return false;
}

bool mp_input_test_dragging(struct input_ctx *ictx, int x, int y)
{
    return mp_input_test_mouse_active(ictx, x, y);
}

// builtin: if true, remove all builtin binds, else remove all user binds
static void remove_binds(struct cmd_bind_section *bs, bool builtin)
{
    for (int n = bs->num_binds - 1; n >= 0; n--) {
        if (bs->binds[n].is_builtin == builtin) {
            bind_dealloc(&bs->binds[n]);
            assert(bs->num_binds >= 1);
            bs->binds[n] = bs->binds[bs->num_binds - 1];
            bs->num_binds--;
        }
    }
}

void mp_input_define_section(struct input_ctx *ictx, char *name, char *location,
                             char *contents, bool builtin)
{
    if (!name || !name[0])
        return; // parse_config() changes semantics with restrict_section==empty
    if (contents) {
        parse_config(ictx, builtin, bstr0(contents), location, name);
    } else {
        // Disable:
        mp_input_disable_section(ictx, name);
        // Delete:
        struct cmd_bind_section *bs = get_bind_section(ictx, bstr0(name));
        remove_binds(bs, builtin);
    }
}

struct input_ctx *mp_input_init(struct MPOpts *opts)
{
    struct input_conf *input_conf = &opts->input;

    struct input_ctx *ictx = talloc_ptrtype(NULL, ictx);
    *ictx = (struct input_ctx){
        .key_fifo_size = input_conf->key_fifo_size,
        .doubleclick_time = input_conf->doubleclick_time,
        .ar_state = -1,
        .ar_delay = input_conf->ar_delay,
        .ar_rate = input_conf->ar_rate,
        .default_bindings = input_conf->default_bindings,
        .test = input_conf->test,
        .wakeup_pipe = {-1, -1},
    };
    mp_input_enable_section(ictx, NULL, 0);

    parse_config(ictx, true, bstr0(builtin_input_conf), "<builtin>", NULL);

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

    bool config_ok = false;
    if (input_conf->config_file)
        config_ok = parse_config_file(ictx, input_conf->config_file, true);
    if (!config_ok && opts->load_config) {
        // Try global conf dir
        char *file = mp_find_config_file("input.conf");
        config_ok = file && parse_config_file(ictx, file, false);
        talloc_free(file);
    }
    if (!config_ok) {
        mp_msg(MSGT_INPUT, MSGL_V, "Falling back on default (hardcoded) "
               "input config\n");
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
        int fd = lircc_init("mpv", NULL);
        if (fd >= 0)
            mp_input_add_cmd_fd(ictx, fd, 1, NULL, lircc_cleanup);
    }
#endif

#ifdef CONFIG_COCOA
    if (input_conf->use_ar) {
        cocoa_init_apple_remote();
        ictx->using_ar = true;
    }

    if (input_conf->use_media_keys) {
        cocoa_init_media_keys();
        ictx->using_cocoa_media_keys = true;
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

static void clear_queue(struct cmd_queue *queue)
{
    while (queue->first) {
        struct mp_cmd *item = queue->first;
        queue_remove(queue, item);
        talloc_free(item);
    }
}

void mp_input_uninit(struct input_ctx *ictx)
{
    if (!ictx)
        return;

#ifdef CONFIG_COCOA
    if (ictx->using_ar) {
        cocoa_uninit_apple_remote();
    }

    if (ictx->using_cocoa_media_keys) {
        cocoa_uninit_media_keys();
    }
#endif

    for (int i = 0; i < ictx->num_fds; i++) {
        if (ictx->fds[i].close_func)
            ictx->fds[i].close_func(ictx->fds[i].fd);
    }
    for (int i = 0; i < 2; i++) {
        if (ictx->wakeup_pipe[i] != -1)
            close(ictx->wakeup_pipe[i]);
    }
    clear_queue(&ictx->key_cmd_queue);
    clear_queue(&ictx->control_cmd_queue);
    talloc_free(ictx->current_down_cmd);
    talloc_free(ictx);
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

    for (i = 0; (cmd = &mp_cmds[i])->name != NULL; i++) {
        printf("%-20.20s", cmd->name);
        for (j = 0; j < MP_CMD_MAX_ARGS && cmd->args[j].type.type; j++) {
            const char *type = cmd->args[j].type.type->name;
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

unsigned int mp_input_get_mouse_event_counter(struct input_ctx *ictx)
{
    // Make the frontend always display the mouse cursor (as long as it's not
    // forced invisible) if mouse input is desired.
    if (mp_input_test_mouse_active(ictx, ictx->mouse_x, ictx->mouse_y))
        ictx->mouse_event_counter++;
    return ictx->mouse_event_counter;
}
