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

#include "input.h"
#include "mouse.h"
#ifdef MP_DEBUG
#include <assert.h>
#endif
#include "mp_fifo.h"
#include "osdep/keycodes.h"
#include "osdep/timer.h"
#include "libavutil/avstring.h"
#include "mp_msg.h"
#include "m_config.h"
#include "m_option.h"
#include "path.h"
#include "talloc.h"
#include "options.h"
#include "bstr.h"

#include "joystick.h"

#ifdef CONFIG_LIRC
#include "lirc.h"
#endif

#ifdef CONFIG_LIRCC
#include <lirc/lircc.h>
#endif

#include "ar.h"

typedef struct mp_cmd_bind {
  int input[MP_MAX_KEY_DOWN+1];
  char* cmd;
} mp_cmd_bind_t;

typedef struct mp_key_name {
  int key;
  char* name;
} mp_key_name_t;

/// This array defines all known commands.
/// The first field is an id used to recognize the command without too many strcmp.
/// The second is obviously the command name.
/// The third is the minimum number of arguments this command needs.
/// Then comes the definition of each argument, terminated with an arg of type -1.
/// A command can take a maximum of MP_CMD_MAX_ARGS-1 arguments (-1 because of
/// the last one) which is actually 9.

/// For the args, the first field is the type (actually int, float or string), the second
/// is the default value wich is used for optional arguments

static const mp_cmd_t mp_cmds[] = {
#ifdef CONFIG_RADIO
  { MP_CMD_RADIO_STEP_CHANNEL, "radio_step_channel", 1,  { { MP_CMD_ARG_INT ,{0}}, {-1,{0}} }},
  { MP_CMD_RADIO_SET_CHANNEL, "radio_set_channel", 1, { { MP_CMD_ARG_STRING, {0}}, {-1,{0}}  }},
  { MP_CMD_RADIO_SET_FREQ, "radio_set_freq", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_RADIO_STEP_FREQ, "radio_step_freq", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
#endif
  { MP_CMD_SEEK, "seek", 1, { {MP_CMD_ARG_FLOAT,{0}}, {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_EDL_MARK, "edl_mark", 0, { {-1,{0}} } },
  { MP_CMD_AUDIO_DELAY, "audio_delay", 1, { {MP_CMD_ARG_FLOAT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SPEED_INCR, "speed_incr", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_SPEED_MULT, "speed_mult", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_SPEED_SET, "speed_set", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_QUIT, "quit", 0, { {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_STOP, "stop", 0, { {-1,{0}} } },
  { MP_CMD_PAUSE, "pause", 0, { {-1,{0}} } },
  { MP_CMD_FRAME_STEP, "frame_step", 0, { {-1,{0}} } },
  { MP_CMD_PLAY_TREE_STEP, "pt_step",1, { { MP_CMD_ARG_INT ,{0}}, { MP_CMD_ARG_INT ,{0}}, {-1,{0}} } },
  { MP_CMD_PLAY_TREE_UP_STEP, "pt_up_step",1,  { { MP_CMD_ARG_INT,{0} }, { MP_CMD_ARG_INT ,{0}}, {-1,{0}} } },
  { MP_CMD_PLAY_ALT_SRC_STEP, "alt_src_step",1, { { MP_CMD_ARG_INT,{0} }, {-1,{0}} } },
  { MP_CMD_LOOP, "loop", 1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SUB_DELAY, "sub_delay",1,  { {MP_CMD_ARG_FLOAT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SUB_STEP, "sub_step",1,  { { MP_CMD_ARG_INT,{0} }, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_OSD, "osd",0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_OSD_SHOW_TEXT, "osd_show_text", 1, { {MP_CMD_ARG_STRING, {0}}, {MP_CMD_ARG_INT,{-1}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_OSD_SHOW_PROPERTY_TEXT, "osd_show_property_text",1, { {MP_CMD_ARG_STRING, {0}}, {MP_CMD_ARG_INT,{-1}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_OSD_SHOW_PROGRESSION, "osd_show_progression", 0, { {-1,{0}} } },
  { MP_CMD_VOLUME, "volume", 1, { { MP_CMD_ARG_FLOAT,{0} }, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_BALANCE, "balance", 1, { { MP_CMD_ARG_FLOAT,{0} }, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_MIXER_USEMASTER, "use_master", 0, { {-1,{0}} } },
  { MP_CMD_MUTE, "mute", 0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_CONTRAST, "contrast",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_GAMMA, "gamma", 1, { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} }  },
  { MP_CMD_BRIGHTNESS, "brightness",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} }  },
  { MP_CMD_HUE, "hue",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SATURATION, "saturation",1,  { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} }  },
  { MP_CMD_FRAMEDROPPING, "frame_drop",0, { { MP_CMD_ARG_INT,{-1} }, {-1,{0}} } },
  { MP_CMD_SUB_POS, "sub_pos", 1, { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SUB_ALIGNMENT, "sub_alignment",0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_SUB_VISIBILITY, "sub_visibility", 0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_SUB_LOAD, "sub_load", 1, { {MP_CMD_ARG_STRING,{0}}, {-1,{0}} } },
  { MP_CMD_SUB_REMOVE, "sub_remove", 0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_SUB_SELECT, "vobsub_lang", 0, { { MP_CMD_ARG_INT,{-2} }, {-1,{0}} } }, // for compatibility
  { MP_CMD_SUB_SELECT, "sub_select", 0, { { MP_CMD_ARG_INT,{-2} }, {-1,{0}} } },
  { MP_CMD_SUB_SOURCE, "sub_source", 0, { { MP_CMD_ARG_INT,{-2} }, {-1,{0}} } },
  { MP_CMD_SUB_VOB, "sub_vob", 0, { { MP_CMD_ARG_INT,{-2} }, {-1,{0}} } },
  { MP_CMD_SUB_DEMUX, "sub_demux", 0, { { MP_CMD_ARG_INT,{-2} }, {-1,{0}} } },
  { MP_CMD_SUB_FILE, "sub_file", 0, { { MP_CMD_ARG_INT,{-2} }, {-1,{0}} } },
  { MP_CMD_SUB_LOG, "sub_log", 0, { {-1,{0}} } },
  { MP_CMD_SUB_SCALE, "sub_scale",1, { {MP_CMD_ARG_FLOAT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
#ifdef CONFIG_ASS
  { MP_CMD_ASS_USE_MARGINS, "ass_use_margins", 0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
#endif
  { MP_CMD_GET_PERCENT_POS, "get_percent_pos", 0, { {-1,{0}} } },
  { MP_CMD_GET_TIME_POS, "get_time_pos", 0, { {-1,{0}} } },
  { MP_CMD_GET_TIME_LENGTH, "get_time_length", 0, { {-1,{0}} } },
  { MP_CMD_GET_FILENAME, "get_file_name", 0, { {-1,{0}} } },
  { MP_CMD_GET_VIDEO_CODEC, "get_video_codec", 0, { {-1,{0}} } },
  { MP_CMD_GET_VIDEO_BITRATE, "get_video_bitrate", 0, { {-1,{0}} } },
  { MP_CMD_GET_VIDEO_RESOLUTION, "get_video_resolution", 0, { {-1,{0}} } },
  { MP_CMD_GET_AUDIO_CODEC, "get_audio_codec", 0, { {-1,{0}} } },
  { MP_CMD_GET_AUDIO_BITRATE, "get_audio_bitrate", 0, { {-1,{0}} } },
  { MP_CMD_GET_AUDIO_SAMPLES, "get_audio_samples", 0, { {-1,{0}} } },
  { MP_CMD_GET_META_TITLE, "get_meta_title", 0, { {-1,{0}} } },
  { MP_CMD_GET_META_ARTIST, "get_meta_artist", 0, { {-1,{0}} } },
  { MP_CMD_GET_META_ALBUM, "get_meta_album", 0, { {-1,{0}} } },
  { MP_CMD_GET_META_YEAR, "get_meta_year", 0, { {-1,{0}} } },
  { MP_CMD_GET_META_COMMENT, "get_meta_comment", 0, { {-1,{0}} } },
  { MP_CMD_GET_META_TRACK, "get_meta_track", 0, { {-1,{0}} } },
  { MP_CMD_GET_META_GENRE, "get_meta_genre", 0, { {-1,{0}} } },
  { MP_CMD_SWITCH_AUDIO, "switch_audio", 0, { { MP_CMD_ARG_INT,{-1} }, {-1,{0}} } },
  { MP_CMD_SWITCH_ANGLE, "switch_angle", 0, { { MP_CMD_ARG_INT,{-1} }, {-1,{0}} } },
  { MP_CMD_SWITCH_TITLE, "switch_title", 0, { { MP_CMD_ARG_INT,{-1} }, {-1,{0}} } },
#ifdef CONFIG_TV
  { MP_CMD_TV_START_SCAN, "tv_start_scan", 0,  { {-1,{0}} }},
  { MP_CMD_TV_STEP_CHANNEL, "tv_step_channel", 1,  { { MP_CMD_ARG_INT ,{0}}, {-1,{0}} }},
  { MP_CMD_TV_STEP_NORM, "tv_step_norm",0, { {-1,{0}} }  },
  { MP_CMD_TV_STEP_CHANNEL_LIST, "tv_step_chanlist", 0, { {-1,{0}} }  },
  { MP_CMD_TV_SET_CHANNEL, "tv_set_channel", 1, { { MP_CMD_ARG_STRING, {0}}, {-1,{0}}  }},
  { MP_CMD_TV_LAST_CHANNEL, "tv_last_channel", 0, { {-1,{0}} } },
  { MP_CMD_TV_SET_FREQ, "tv_set_freq", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_TV_STEP_FREQ, "tv_step_freq", 1, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_TV_SET_NORM, "tv_set_norm", 1, { {MP_CMD_ARG_STRING,{0}}, {-1,{0}} } },
  { MP_CMD_TV_SET_BRIGHTNESS, "tv_set_brightness", 1,  { { MP_CMD_ARG_INT ,{0}}, { MP_CMD_ARG_INT,{1} }, {-1,{0}} }},
  { MP_CMD_TV_SET_CONTRAST, "tv_set_contrast", 1,  { { MP_CMD_ARG_INT ,{0}}, { MP_CMD_ARG_INT,{1} }, {-1,{0}} }},
  { MP_CMD_TV_SET_HUE, "tv_set_hue", 1,  { { MP_CMD_ARG_INT ,{0}}, { MP_CMD_ARG_INT,{1} }, {-1,{0}} }},
  { MP_CMD_TV_SET_SATURATION, "tv_set_saturation", 1,  { { MP_CMD_ARG_INT ,{0}}, { MP_CMD_ARG_INT,{1} }, {-1,{0}} }},
#endif
  { MP_CMD_SUB_FORCED_ONLY, "forced_subs_only",  0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
#ifdef CONFIG_DVBIN
  { MP_CMD_DVB_SET_CHANNEL, "dvb_set_channel", 2, { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}}}},
#endif
  { MP_CMD_SWITCH_RATIO, "switch_ratio", 0, { {MP_CMD_ARG_FLOAT,{0}}, {-1,{0}} } },
  { MP_CMD_VO_FULLSCREEN, "vo_fullscreen", 0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_VO_ONTOP, "vo_ontop", 0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_FILE_FILTER, "file_filter", 1, { { MP_CMD_ARG_INT, {0}}, {-1,{0}}}},
  { MP_CMD_VO_ROOTWIN, "vo_rootwin", 0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_VO_BORDER, "vo_border", 0, { {MP_CMD_ARG_INT,{-1}}, {-1,{0}} } },
  { MP_CMD_SCREENSHOT, "screenshot", 0, { {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_PANSCAN, "panscan",1,  { {MP_CMD_ARG_FLOAT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SWITCH_VSYNC, "switch_vsync", 0, { {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_LOADFILE, "loadfile", 1, { {MP_CMD_ARG_STRING, {0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_LOADLIST, "loadlist", 1, { {MP_CMD_ARG_STRING, {0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_RUN, "run", 1, { {MP_CMD_ARG_STRING,{0}}, {-1,{0}} } },
  { MP_CMD_CAPTURING, "capturing", 0, { {-1,{0}} } },
  { MP_CMD_VF_CHANGE_RECTANGLE, "change_rectangle", 2, { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}}}},
  { MP_CMD_TV_TELETEXT_ADD_DEC, "teletext_add_dec", 1, { {MP_CMD_ARG_STRING,{0}}, {-1,{0}} } },
  { MP_CMD_TV_TELETEXT_GO_LINK, "teletext_go_link", 1, { {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },

#ifdef CONFIG_DVDNAV
  { MP_CMD_DVDNAV, "dvdnav", 1, { {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
#endif

#ifdef CONFIG_MENU
  { MP_CMD_MENU, "menu",1,  { {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
  { MP_CMD_SET_MENU, "set_menu",1,  { {MP_CMD_ARG_STRING, {0}},  {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
  { MP_CMD_CHELP, "help", 0, { {-1,{0}} } },
  { MP_CMD_CEXIT, "exit", 0, { {-1,{0}} } },
  { MP_CMD_CHIDE, "hide", 0, { {MP_CMD_ARG_INT,{3000}}, {-1,{0}} } },
#endif

  { MP_CMD_GET_VO_FULLSCREEN, "get_vo_fullscreen", 0, { {-1,{0}} } },
  { MP_CMD_GET_SUB_VISIBILITY, "get_sub_visibility", 0, { {-1,{0}} } },
  { MP_CMD_KEYDOWN_EVENTS, "key_down_event", 1, { {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SET_PROPERTY, "set_property", 2, { {MP_CMD_ARG_STRING, {0}},  {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
  { MP_CMD_SET_PROPERTY_OSD, "set_property_osd", 2, { {MP_CMD_ARG_STRING, {0}},  {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
  { MP_CMD_GET_PROPERTY, "get_property", 1, { {MP_CMD_ARG_STRING, {0}},  {-1,{0}} } },
  { MP_CMD_STEP_PROPERTY, "step_property", 1, { {MP_CMD_ARG_STRING, {0}}, {MP_CMD_ARG_FLOAT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_STEP_PROPERTY_OSD, "step_property_osd", 1, { {MP_CMD_ARG_STRING, {0}}, {MP_CMD_ARG_FLOAT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },

  { MP_CMD_SEEK_CHAPTER, "seek_chapter", 1, { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },
  { MP_CMD_SET_MOUSE_POS, "set_mouse_pos", 2, { {MP_CMD_ARG_INT,{0}}, {MP_CMD_ARG_INT,{0}}, {-1,{0}} } },

  { MP_CMD_AF_SWITCH, "af_switch", 1,  { {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
  { MP_CMD_AF_ADD, "af_add", 1,  { {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
  { MP_CMD_AF_DEL, "af_del", 1,  { {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },
  { MP_CMD_AF_CLR, "af_clr", 0, { {-1,{0}} } },
  { MP_CMD_AF_CMDLINE, "af_cmdline", 2, { {MP_CMD_ARG_STRING, {0}}, {MP_CMD_ARG_STRING, {0}}, {-1,{0}} } },

  { 0, NULL, 0, {} }
};

/// The names of the keys as used in input.conf
/// If you add some new keys, you also need to add them here

static const mp_key_name_t key_names[] = {
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

struct mp_key_name modifier_names[] = {
    { KEY_MODIFIER_SHIFT, "Shift" },
    { KEY_MODIFIER_CTRL,  "Ctrl" },
    { KEY_MODIFIER_ALT,   "Alt" },
    { KEY_MODIFIER_META,  "Meta" },
    { 0 }
};

#define KEY_MODIFIER_MASK (KEY_MODIFIER_SHIFT | KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT | KEY_MODIFIER_META)

// This is the default binding. The content of input.conf overrides these.
// The first arg is a null terminated array of key codes.
// The second is the command

static const mp_cmd_bind_t def_cmd_binds[] = {

  { {  MOUSE_BTN3, 0 }, "seek 10" },
  { {  MOUSE_BTN4, 0 }, "seek -10" },
  { {  MOUSE_BTN5, 0 }, "volume 1" },
  { {  MOUSE_BTN6, 0 }, "volume -1" },

#ifdef CONFIG_DVDNAV
  { { KEY_KP8, 0 }, "dvdnav up" },   // up
  { { KEY_KP2, 0 }, "dvdnav down" },   // down
  { { KEY_KP4, 0 }, "dvdnav left" },   // left
  { { KEY_KP6, 0 }, "dvdnav right" },   // right
  { { KEY_KP5, 0 }, "dvdnav menu" },   // menu
  { { KEY_KPENTER, 0 }, "dvdnav select" },   // select
  { { MOUSE_BTN0, 0 }, "dvdnav mouse" },   //select
  { { KEY_KP7, 0 }, "dvdnav prev" },   // previous menu
#endif

  { { KEY_RIGHT, 0 }, "seek 10" },
  { {  KEY_LEFT, 0 }, "seek -10" },
  { { KEY_MODIFIER_SHIFT + KEY_RIGHT, 0 }, "seek  1 0 1" },
  { { KEY_MODIFIER_SHIFT + KEY_LEFT,  0 }, "seek -1 0 1" },
  { {  KEY_UP, 0 }, "seek 60" },
  { {  KEY_DOWN, 0 }, "seek -60" },
  { { KEY_MODIFIER_SHIFT + KEY_UP,    0 }, "seek  5 0 1" },
  { { KEY_MODIFIER_SHIFT + KEY_DOWN,  0 }, "seek -5 0 1" },
  { {  KEY_PAGE_UP, 0 }, "seek 600" },
  { { KEY_PAGE_DOWN, 0 }, "seek -600" },
  { { '+', 0 }, "audio_delay 0.100" },
  { { '-', 0 }, "audio_delay -0.100" },
  { { '[', 0 }, "speed_mult 0.9091" },
  { { ']', 0 }, "speed_mult 1.1" },
  { { '{', 0 }, "speed_mult 0.5" },
  { { '}', 0 }, "speed_mult 2.0" },
  { { KEY_BACKSPACE, 0 }, "speed_set 1.0" },
  { { 'q', 0 }, "quit" },
  { { KEY_ESC, 0 }, "quit" },
  { { 'p', 0 }, "pause" },
  { { ' ', 0 }, "pause" },
  { { '.', 0 }, "frame_step" },
  { { KEY_HOME, 0 }, "pt_up_step 1" },
  { { KEY_END, 0 }, "pt_up_step -1" },
  { { '>', 0 }, "pt_step 1" },
  { { KEY_ENTER, 0 }, "pt_step 1 1" },
  { { '<', 0 }, "pt_step -1" },
  { { KEY_INS, 0 }, "alt_src_step 1" },
  { { KEY_DEL, 0 }, "alt_src_step -1" },
  { { 'o', 0 }, "osd" },
  { { 'I', 0 }, "osd_show_property_text \"${filename}\"" },
  { { 'P', 0 }, "osd_show_progression" },
  { { 'z', 0 }, "sub_delay -0.1" },
  { { 'x', 0 }, "sub_delay +0.1" },
  { { 'g', 0 }, "sub_step -1" },
  { { 'y', 0 }, "sub_step +1" },
  { { '9', 0 }, "volume -1" },
  { { '/', 0 }, "volume -1" },
  { { '0', 0 }, "volume 1" },
  { { '*', 0 }, "volume 1" },
  { { '(', 0 }, "balance -0.1" },
  { { ')', 0 }, "balance 0.1" },
  { { 'm', 0 }, "mute" },
  { { '1', 0 }, "contrast -1" },
  { { '2', 0 }, "contrast 1" },
  { { '3', 0 }, "brightness -1" },
  { { '4', 0 }, "brightness 1" },
  { { '5', 0 }, "hue -1" },
  { { '6', 0 }, "hue 1" },
  { { '7', 0 }, "saturation -1" },
  { { '8', 0 }, "saturation 1" },
  { { 'd', 0 }, "frame_drop" },
  { { 'D', 0 }, "step_property_osd deinterlace" },
  { { 'c', 0 }, "step_property_osd yuv_colorspace" },
  { { 'r', 0 }, "sub_pos -1" },
  { { 't', 0 }, "sub_pos +1" },
  { { 'a', 0 }, "sub_alignment" },
  { { 'v', 0 }, "sub_visibility" },
  { { 'j', 0 }, "sub_select" },
  { { 'J', 0 }, "sub_select -3" },
  { { 'F', 0 }, "forced_subs_only" },
  { { '#', 0 }, "switch_audio" },
  { { '_', 0 }, "step_property switch_video" },
  { { KEY_TAB, 0 }, "step_property switch_program" },
  { { 'i', 0 }, "edl_mark" },
#ifdef CONFIG_TV
  { { 'h', 0 }, "tv_step_channel 1" },
  { { 'k', 0 }, "tv_step_channel -1" },
  { { 'n', 0 }, "tv_step_norm" },
  { { 'u', 0 }, "tv_step_chanlist" },
#endif
  { { 'X', 0 }, "step_property teletext_mode 1" },
  { { 'W', 0 }, "step_property teletext_page 1" },
  { { 'Q', 0 }, "step_property teletext_page -1" },
#ifdef CONFIG_JOYSTICK
  { { JOY_AXIS0_PLUS, 0 }, "seek 10" },
  { { JOY_AXIS0_MINUS, 0 }, "seek -10" },
  { { JOY_AXIS1_MINUS, 0 }, "seek 60" },
  { { JOY_AXIS1_PLUS, 0 }, "seek -60" },
  { { JOY_BTN0, 0 }, "pause" },
  { { JOY_BTN1, 0 }, "osd" },
  { { JOY_BTN2, 0 }, "volume 1"},
  { { JOY_BTN3, 0 }, "volume -1"},
#endif
#ifdef CONFIG_APPLE_REMOTE
  { { AR_PLAY, 0}, "pause" },
  { { AR_PLAY_HOLD, 0}, "quit" },
  { { AR_NEXT, 0 }, "seek 30" },
  { { AR_NEXT_HOLD, 0 }, "seek 120" },
  { { AR_PREV, 0 }, "seek -10" },
  { { AR_PREV_HOLD, 0 }, "seek -120" },
  { { AR_MENU, 0 }, "osd" },
  { { AR_MENU_HOLD, 0 }, "mute" },
  { { AR_VUP, 0 }, "volume 1"},
  { { AR_VDOWN, 0 }, "volume -1"},
#endif
  { { 'T', 0 }, "vo_ontop" },
  { { 'f', 0 }, "vo_fullscreen" },
  { { 'C', 0 }, "step_property_osd capturing" },
  { { 's', 0 }, "screenshot 0" },
  { { 'S', 0 }, "screenshot 1" },
  { { 'w', 0 }, "panscan -0.1" },
  { { 'e', 0 }, "panscan +0.1" },

  { { KEY_POWER, 0 }, "quit" },
  { { KEY_MENU, 0 }, "osd" },
  { { KEY_PLAY, 0 }, "pause" },
  { { KEY_PAUSE, 0 }, "pause" },
  { { KEY_PLAYPAUSE, 0 }, "pause" },
  { { KEY_STOP, 0 }, "quit" },
  { { KEY_FORWARD, 0 }, "seek 60" },
  { { KEY_REWIND, 0 }, "seek -60" },
  { { KEY_NEXT, 0 }, "pt_step 1" },
  { { KEY_PREV, 0 }, "pt_step -1" },
  { { KEY_VOLUME_UP, 0 }, "volume 1" },
  { { KEY_VOLUME_DOWN, 0 }, "volume -1" },
  { { KEY_MUTE, 0 }, "mute" },

  { { KEY_CLOSE_WIN, 0 }, "quit" },

  { { '!', 0 }, "seek_chapter -1" },
  { { '@', 0 }, "seek_chapter 1" },
  { { 'A', 0 }, "switch_angle 1" },
  { { 'U', 0 }, "stop" },

  { { 0 }, NULL }
};


#ifndef MP_MAX_KEY_FD
#define MP_MAX_KEY_FD 10
#endif

#ifndef MP_MAX_CMD_FD
#define MP_MAX_CMD_FD 10
#endif

#define CMD_QUEUE_SIZE 100

typedef struct mp_input_fd {
  int fd;
  union {
      mp_key_func_t key;
      mp_cmd_func_t cmd;
  } read_func;
  mp_close_func_t close_func;
  void *ctx;
  unsigned eof : 1;
  unsigned drop : 1;
  unsigned dead : 1;
  unsigned got_cmd : 1;
  unsigned no_select : 1;
  // These fields are for the cmd fds.
  char* buffer;
  int pos,size;
} mp_input_fd_t;

typedef struct mp_cmd_filter mp_cmd_filter_t;

struct mp_cmd_filter {
  mp_input_cmd_filter filter;
  void* ctx;
  mp_cmd_filter_t* next;
};

typedef struct mp_cmd_bind_section mp_cmd_bind_section_t;

struct mp_cmd_bind_section {
  mp_cmd_bind_t* cmd_binds;
  char* section;
  mp_cmd_bind_section_t* next;
};

struct input_ctx {
    // Autorepeat stuff
    short ar_state;
    mp_cmd_t *ar_cmd;
    unsigned int last_ar;
    // Autorepeat config
    unsigned int ar_delay;
    unsigned int ar_rate;

    // these are the keys currently down
    int key_down[MP_MAX_KEY_DOWN];
    unsigned int num_key_down;
    unsigned int last_key_down;

    bool default_bindings;
    // List of command binding sections
    mp_cmd_bind_section_t *cmd_bind_sections;
    // Name of currently used command section
    char *section;
    // The command binds of current section
    mp_cmd_bind_t *cmd_binds;
    mp_cmd_bind_t *cmd_binds_default;

    mp_input_fd_t key_fds[MP_MAX_KEY_FD];
    unsigned int num_key_fd;

    mp_input_fd_t cmd_fds[MP_MAX_CMD_FD];
    unsigned int num_cmd_fd;

    mp_cmd_t *cmd_queue[CMD_QUEUE_SIZE];
    unsigned int cmd_queue_length, cmd_queue_start, cmd_queue_end;
};


static mp_cmd_filter_t* cmd_filters = NULL;

// Callback to allow the menu filter to grab the incoming keys
int (*mp_input_key_cb)(int code) = NULL;

int async_quit_request;

static int print_key_list(m_option_t* cfg);
static int print_cmd_list(m_option_t* cfg);

// Our command line options
static const m_option_t input_conf[] = {
    OPT_STRING("conf", input.config_file, CONF_GLOBAL),
    OPT_INT("ar-delay",input.ar_delay, CONF_GLOBAL),
    OPT_INT("ar-rate", input.ar_rate, CONF_GLOBAL),
  { "keylist", print_key_list, CONF_TYPE_FUNC, CONF_GLOBAL, 0, 0, NULL },
  { "cmdlist", print_cmd_list, CONF_TYPE_FUNC, CONF_GLOBAL, 0, 0, NULL },
    OPT_STRING("js-dev", input.js_dev, CONF_GLOBAL),
    OPT_STRING("ar-dev", input.ar_dev, CONF_GLOBAL),
    OPT_STRING("file", input.in_file, CONF_GLOBAL),
    OPT_MAKE_FLAGS("default-bindings", input.default_bindings, CONF_GLOBAL),
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static const m_option_t mp_input_opts[] = {
  { "input", &input_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    OPT_MAKE_FLAGS("joystick", input.use_joystick, CONF_GLOBAL),
    OPT_MAKE_FLAGS("lirc", input.use_lirc, CONF_GLOBAL),
    OPT_MAKE_FLAGS("lircc", input.use_lircc, CONF_GLOBAL),
    OPT_MAKE_FLAGS("ar", input.use_ar, CONF_GLOBAL),
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static int default_cmd_func(int fd,char* buf, int l);

static char *get_key_name(int key)
{
    char *ret = talloc_strdup(NULL, "");
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

    if (isascii(key))
        return talloc_asprintf_append_buffer(ret, "%c", key);

    // Print the hex key code
    return talloc_asprintf_append_buffer(ret, "%#-8x", key);
}

int mp_input_add_cmd_fd(struct input_ctx *ictx, int fd, int select,
                        mp_cmd_func_t read_func, mp_close_func_t close_func)
{
  if (ictx->num_cmd_fd == MP_MAX_CMD_FD) {
    mp_tmsg(MSGT_INPUT,MSGL_ERR,"Too many command file descriptors, cannot register file descriptor %d.\n",fd);
    return 0;
  }
  if (select && fd < 0) {
    mp_msg(MSGT_INPUT, MSGL_ERR, "Invalid fd %i in mp_input_add_cmd_fd", fd);
    return 0;
  }

  ictx->cmd_fds[ictx->num_cmd_fd] = (struct mp_input_fd){
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
    struct mp_input_fd *cmd_fds = ictx->cmd_fds;
  unsigned int i;

  for (i = 0; i < ictx->num_cmd_fd; i++) {
    if(cmd_fds[i].fd == fd)
      break;
  }
  if (i == ictx->num_cmd_fd)
    return;
  if(cmd_fds[i].close_func)
    cmd_fds[i].close_func(cmd_fds[i].fd);
  talloc_free(cmd_fds[i].buffer);

  if (i + 1 < ictx->num_cmd_fd)
      memmove(&cmd_fds[i], &cmd_fds[i+1],
              (ictx->num_cmd_fd - i - 1) * sizeof(mp_input_fd_t));
  ictx->num_cmd_fd--;
}

void mp_input_rm_key_fd(struct input_ctx *ictx, int fd)
{
    struct mp_input_fd *key_fds = ictx->key_fds;
  unsigned int i;

  for (i = 0; i < ictx->num_key_fd; i++) {
    if(key_fds[i].fd == fd)
      break;
  }
  if (i == ictx->num_key_fd)
    return;
  if(key_fds[i].close_func)
    key_fds[i].close_func(key_fds[i].fd);

  if(i + 1 < ictx->num_key_fd)
      memmove(&key_fds[i], &key_fds[i+1],
              (ictx->num_key_fd - i - 1) * sizeof(mp_input_fd_t));
  ictx->num_key_fd--;
}

int mp_input_add_key_fd(struct input_ctx *ictx, int fd, int select,
                        mp_key_func_t read_func, mp_close_func_t close_func,
                        void *ctx)
{
  if (ictx->num_key_fd == MP_MAX_KEY_FD) {
    mp_tmsg(MSGT_INPUT,MSGL_ERR,"Too many key file descriptors, cannot register file descriptor %d.\n",fd);
    return 0;
  }
  if (select && fd < 0) {
    mp_msg(MSGT_INPUT, MSGL_ERR, "Invalid fd %i in mp_input_add_key_fd", fd);
    return 0;
  }

  ictx->key_fds[ictx->num_key_fd] = (struct mp_input_fd){
      .fd = fd,
      .read_func.key = read_func,
      .close_func = close_func,
      .no_select = !select,
      .ctx = ctx,
  };
  ictx->num_key_fd++;

  return 1;
}

int mp_input_parse_and_queue_cmds(struct input_ctx *ictx, const char *str)
{
    int cmd_num = 0;

    while (*str == '\n' || *str == '\r' || *str == ' ')
        ++str;
    while (*str) {
        mp_cmd_t *cmd;
        size_t len = strcspn(str, "\r\n");
        char *cmdbuf = talloc_size(NULL, len+1);
        av_strlcpy(cmdbuf, str, len+1);
        cmd = mp_input_parse_cmd(cmdbuf);
        if (cmd) {
            mp_input_queue_cmd(ictx, cmd);
            ++cmd_num;
        }
        str += len;
        while (*str == '\n' || *str == '\r' || *str == ' ')
            ++str;
        talloc_free(cmdbuf);
    }
    return cmd_num;
}

mp_cmd_t*
mp_input_parse_cmd(char* str) {
  int i,l;
  int pausing = 0;
  char *ptr,*e;
  const mp_cmd_t *cmd_def;

#ifdef MP_DEBUG
  assert(str != NULL);
#endif

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

  for(ptr = str ; ptr[0] != '\0'  && ptr[0] != '\t' && ptr[0] != ' ' ; ptr++)
    /* NOTHING */;
  if(ptr[0] != '\0')
    l = ptr-str;
  else
    l = strlen(str);

  if(l == 0)
    return NULL;

  for(i=0; mp_cmds[i].name != NULL; i++) {
    if(strncasecmp(mp_cmds[i].name,str,l) == 0)
      break;
  }

  if(mp_cmds[i].name == NULL)
    return NULL;

  cmd_def = &mp_cmds[i];

  mp_cmd_t *cmd = talloc_ptrtype(NULL, cmd);
  *cmd = (mp_cmd_t){
      .id = cmd_def->id,
      .name = talloc_strdup(cmd, cmd_def->name),
      .pausing = pausing,
  };

  ptr = str;

  for(i=0; ptr && i < MP_CMD_MAX_ARGS; i++) {
    while(ptr[0] != ' ' && ptr[0] != '\t' && ptr[0] != '\0') ptr++;
    if(ptr[0] == '\0') break;
    while(ptr[0] == ' ' || ptr[0] == '\t') ptr++;
    if(ptr[0] == '\0' || ptr[0] == '#') break;
    cmd->args[i].type = cmd_def->args[i].type;
    switch(cmd_def->args[i].type) {
    case MP_CMD_ARG_INT:
      errno = 0;
      cmd->args[i].v.i = atoi(ptr);
      if(errno != 0) {
	mp_tmsg(MSGT_INPUT,MSGL_ERR,"Command %s: argument %d isn't an integer.\n",cmd_def->name,i+1);
	ptr = NULL;
      }
      break;
    case MP_CMD_ARG_FLOAT:
      errno = 0;
      cmd->args[i].v.f = atof(ptr);
      if(errno != 0) {
	mp_tmsg(MSGT_INPUT,MSGL_ERR,"Command %s: argument %d isn't a float.\n",cmd_def->name,i+1);
	ptr = NULL;
      }
      break;
    case MP_CMD_ARG_STRING: {
      char term;
      char* ptr2 = ptr, *start;

      if(ptr[0] == '\'' || ptr[0] == '"') {
	term = ptr[0];
	ptr2++;
      } else
	term = ' ';
      start = ptr2;
      while(1) {
	e = strchr(ptr2,term);
	if(!e) break;
	if(e <= ptr2 || *(e - 1) != '\\') break;
	ptr2 = e + 1;
      }

      if(term != ' ' && (!e || e[0] == '\0')) {
	mp_tmsg(MSGT_INPUT,MSGL_ERR,"Command %s: argument %d is unterminated.\n",cmd_def->name,i+1);
	ptr = NULL;
	break;
      } else if(!e) e = ptr+strlen(ptr);
      l = e-start;
      ptr2 = start;
      for(e = strchr(ptr2,'\\') ; e && e<start+l ; e = strchr(ptr2,'\\')) {
	memmove(e,e+1,strlen(e));
	ptr2 = e + 1;
        l--;
      }
      cmd->args[i].v.s = talloc_strndup(cmd, start, l);
      if(term != ' ') ptr += l+2;
    } break;
    case -1:
      ptr = NULL;
      break;
    default :
      mp_tmsg(MSGT_INPUT,MSGL_ERR,"Unknown argument %d\n",i);
    }
  }
  cmd->nargs = i;

  if(cmd_def->nargs > cmd->nargs) {
/*    mp_msg(MSGT_INPUT,MSGL_ERR,"Got command '%s' but\n",str); */
    mp_tmsg(MSGT_INPUT,MSGL_ERR,"Command %s requires at least %d arguments, we found only %d so far.\n",cmd_def->name,cmd_def->nargs,cmd->nargs);
    mp_cmd_free(cmd);
    return NULL;
  }

  for( ; i < MP_CMD_MAX_ARGS && cmd_def->args[i].type != -1 ; i++) {
    memcpy(&cmd->args[i],&cmd_def->args[i],sizeof(mp_cmd_arg_t));
    if(cmd_def->args[i].type == MP_CMD_ARG_STRING && cmd_def->args[i].v.s != NULL)
        cmd->args[i].v.s = talloc_strdup(cmd, cmd_def->args[i].v.s);
  }

  if(i < MP_CMD_MAX_ARGS)
    cmd->args[i].type = -1;

  return cmd;
}

#define MP_CMD_MAX_SIZE 4096

static int read_cmd(mp_input_fd_t* mp_fd, char** ret)
{
  char* end;
  (*ret) = NULL;

  // Allocate the buffer if it doesn't exist
  if(!mp_fd->buffer) {
    mp_fd->buffer = talloc_size(NULL, MP_CMD_MAX_SIZE);
    mp_fd->pos = 0;
    mp_fd->size = MP_CMD_MAX_SIZE;
  }

  // Get some data if needed/possible
  while (!mp_fd->got_cmd && !mp_fd->eof && (mp_fd->size - mp_fd->pos > 1) ) {
    int r = mp_fd->read_func.cmd(mp_fd->fd, mp_fd->buffer+mp_fd->pos,
                                 mp_fd->size - 1 - mp_fd->pos);
    // Error ?
    if(r < 0) {
      switch(r) {
      case MP_INPUT_ERROR:
      case MP_INPUT_DEAD:
	mp_tmsg(MSGT_INPUT,MSGL_ERR,"Error while reading command file descriptor %d: %s\n",mp_fd->fd,strerror(errno));
      case MP_INPUT_NOTHING:
	return r;
      case MP_INPUT_RETRY:
	continue;
      }
      // EOF ?
    } else if(r == 0) {
      mp_fd->eof = 1;
      break;
    }
    mp_fd->pos += r;
    break;
  }

  mp_fd->got_cmd = 0;

  while(1) {
    int l = 0;
    // Find the cmd end
    mp_fd->buffer[mp_fd->pos] = '\0';
    end = strchr(mp_fd->buffer,'\r');
    if (end) *end = '\n';
    end = strchr(mp_fd->buffer,'\n');
    // No cmd end ?
    if(!end) {
      // If buffer is full we must drop all until the next \n
      if(mp_fd->size - mp_fd->pos <= 1) {
	mp_tmsg(MSGT_INPUT,MSGL_ERR,"Command buffer of file descriptor %d is full: dropping content.\n",mp_fd->fd);
	mp_fd->pos = 0;
	mp_fd->drop = 1;
      }
      break;
    }
    // We already have a cmd : set the got_cmd flag
    else if((*ret)) {
      mp_fd->got_cmd = 1;
      break;
    }

    l = end - mp_fd->buffer;

    // Not dropping : put the cmd in ret
    if (!mp_fd->drop)
        *ret = talloc_strndup(NULL, mp_fd->buffer, l);
    else
      mp_fd->drop = 0;
    mp_fd->pos -= l+1;
    memmove(mp_fd->buffer, end+1, mp_fd->pos);
  }

  if(*ret)
    return 1;
  else
    return MP_INPUT_NOTHING;
}

static int default_cmd_func(int fd,char* buf, int l)
{
  while(1) {
    int r = read(fd,buf,l);
    // Error ?
    if(r < 0) {
      if(errno == EINTR)
	continue;
      else if(errno == EAGAIN)
	return MP_INPUT_NOTHING;
      return MP_INPUT_ERROR;
      // EOF ?
    }
    return r;
  }

}


void
mp_input_add_cmd_filter(mp_input_cmd_filter func, void* ctx) {
    mp_cmd_filter_t *filter = talloc_ptrtype(NULL, filter);

  filter->filter = func;
  filter->ctx = ctx;
  filter->next = cmd_filters;
  cmd_filters = filter;
}


static char *find_bind_for_key(const mp_cmd_bind_t* binds, int n,int* keys)
{
  int j;

  if (n <= 0) return NULL;
  for(j = 0; binds[j].cmd != NULL; j++) {
      int found = 1,s;
      for(s = 0; s < n && binds[j].input[s] != 0; s++) {
	if(binds[j].input[s] != keys[s]) {
	  found = 0;
	  break;
	}
      }
      if(found && binds[j].input[s] == 0 && s == n)
	break;
  }
  return binds[j].cmd;
}

static mp_cmd_bind_section_t *get_bind_section(struct input_ctx *ictx,
                                               char *section)
{
  mp_cmd_bind_section_t *bind_section = ictx->cmd_bind_sections;

  if (section==NULL) section="default";
  while (bind_section) {
    if(strcmp(section,bind_section->section)==0) return bind_section;
    if(bind_section->next==NULL) break;
    bind_section=bind_section->next;
  }
  if(bind_section) {
      bind_section->next = talloc_ptrtype(ictx, bind_section->next);
    bind_section=bind_section->next;
  } else {
      ictx->cmd_bind_sections = talloc_ptrtype(ictx, ictx->cmd_bind_sections);
    bind_section = ictx->cmd_bind_sections;
  }
  bind_section->cmd_binds=NULL;
  bind_section->section = talloc_strdup(bind_section, section);
  bind_section->next=NULL;
  return bind_section;
}

static mp_cmd_t *get_cmd_from_keys(struct input_ctx *ictx, int n, int *keys)
{
  char* cmd = NULL;
  mp_cmd_t* ret;

  if (ictx->cmd_binds)
    cmd = find_bind_for_key(ictx->cmd_binds, n, keys);
  if (ictx->cmd_binds_default && cmd == NULL)
    cmd = find_bind_for_key(ictx->cmd_binds_default, n, keys);
  if (ictx->default_bindings && cmd == NULL)
    cmd = find_bind_for_key(def_cmd_binds,n,keys);

  if(cmd == NULL) {
      char *key_buf = get_key_name(keys[0]);
      mp_tmsg(MSGT_INPUT,MSGL_WARN,"No bind found for key '%s'.", key_buf);
      talloc_free(key_buf);
    if(n > 1) {
      int s;
      for(s=1; s < n; s++) {
          key_buf = get_key_name(keys[s]);
          mp_msg(MSGT_INPUT,MSGL_WARN,"-%s", key_buf);
          talloc_free(key_buf);
      }
    }
    mp_msg(MSGT_INPUT,MSGL_WARN,"                         \n");
    return NULL;
  }
  if (strcmp(cmd, "ignore") == 0) return NULL;
  ret =  mp_input_parse_cmd(cmd);
  if(!ret) {
    char *key_buf = get_key_name(ictx->key_down[0]);
    mp_tmsg(MSGT_INPUT,MSGL_ERR,"Invalid command for bound key %s", key_buf);
    talloc_free(key_buf);
    if (ictx->num_key_down > 1) {
      unsigned int s;
      for(s=1; s < ictx->num_key_down; s++) {
          char *key_buf = get_key_name(ictx->key_down[s]);
          mp_msg(MSGT_INPUT,MSGL_ERR,"-%s", key_buf);
          talloc_free(key_buf);
      }
    }
    mp_msg(MSGT_INPUT,MSGL_ERR," : %s             \n",cmd);
  }
  return ret;
}


static mp_cmd_t* interpret_key(struct input_ctx *ictx, int code)
{
  unsigned int j;
  mp_cmd_t* ret;

    /* On normal keyboards shift changes the character code of non-special
     * keys, so don't count the modifier separately for those. In other words
     * we want to have "a" and "A" instead of "a" and "Shift+A"; but a separate
     * shift modifier is still kept for special keys like arrow keys.
     */
    int unmod = code & ~KEY_MODIFIER_MASK;
    if (unmod < 256 && unmod != KEY_ENTER && unmod != KEY_TAB)
        code &= ~KEY_MODIFIER_SHIFT;

  if(mp_input_key_cb) {
      if (code & MP_KEY_DOWN)
	  return NULL;
      code &= ~(MP_KEY_DOWN|MP_NO_REPEAT_KEY);
      if (mp_input_key_cb(code))
    return NULL;
  }

    if(code & MP_KEY_DOWN) {
      if (ictx->num_key_down > MP_MAX_KEY_DOWN) {
	mp_tmsg(MSGT_INPUT,MSGL_ERR,"Too many key down events at the same time\n");
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
    // key released
    // Check if the key is in the down key, driver which can't send push event
    // send only release event
    for (j = 0; j < ictx->num_key_down; j++) {
      if (ictx->key_down[j] == code)
	break;
    }
    if (j == ictx->num_key_down) { // key was not in the down keys : add it
      if (ictx->num_key_down > MP_MAX_KEY_DOWN) {
	mp_tmsg(MSGT_INPUT,MSGL_ERR,"Too many key down events at the same time\n");
	return NULL;
      }
      ictx->key_down[ictx->num_key_down] = code;
      ictx->num_key_down++;
      ictx->last_key_down = 1;
    }
    // We ignore key from last combination
    ret = ictx->last_key_down ?
        get_cmd_from_keys(ictx, ictx->num_key_down, ictx->key_down)
        : NULL;
    // Remove the key
    if (j+1 < ictx->num_key_down)
      memmove(&ictx->key_down[j], &ictx->key_down[j+1],
              (ictx->num_key_down-(j+1))*sizeof(int));
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
  if (ictx->ar_rate > 0 && ictx->ar_state >=0 && ictx->num_key_down > 0
      && !(ictx->key_down[ictx->num_key_down-1] & MP_NO_REPEAT_KEY)) {
    unsigned int t = GetTimer();
    // First time : wait delay
    if (ictx->ar_state == 0
        && (t - ictx->last_key_down) >= ictx->ar_delay*1000) {
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
               && (t -ictx->last_ar) >= 1000000 / ictx->ar_rate) {
      ictx->last_ar = t;
      return mp_cmd_clone(ictx->ar_cmd);
    }
  }
  return NULL;
}


/**
 * \param time time to wait at most for an event in milliseconds
 */
static mp_cmd_t *read_events(struct input_ctx *ictx, int time)
{
    int i;
    int got_cmd = 0;
    struct mp_input_fd *key_fds = ictx->key_fds;
    struct mp_input_fd *cmd_fds = ictx->cmd_fds;
    for (i = 0; i < ictx->num_key_fd; i++)
	if (key_fds[i].dead) {
	    mp_input_rm_key_fd(ictx, key_fds[i].fd);
	    i--;
	}
    for (i = 0; i < ictx->num_cmd_fd; i++)
	if (cmd_fds[i].dead || cmd_fds[i].eof) {
	    mp_input_rm_cmd_fd(ictx, cmd_fds[i].fd);
	    i--;
	}
	else if (cmd_fds[i].got_cmd)
	    got_cmd = 1;
#ifdef HAVE_POSIX_SELECT
    fd_set fds;
    FD_ZERO(&fds);
    if (!got_cmd) {
	int max_fd = 0, num_fd = 0;
	for (i = 0; i < ictx->num_key_fd; i++) {
	    if (key_fds[i].no_select)
		continue;
	    if (key_fds[i].fd > max_fd)
		max_fd = key_fds[i].fd;
	    FD_SET(key_fds[i].fd, &fds);
	    num_fd++;
	}
	for (i = 0; i < ictx->num_cmd_fd; i++) {
	    if (cmd_fds[i].no_select)
		continue;
	    if (cmd_fds[i].fd > max_fd)
		max_fd = cmd_fds[i].fd;
	    FD_SET(cmd_fds[i].fd, &fds);
	    num_fd++;
	}
	if (num_fd > 0) {
	    struct timeval tv, *time_val;
	    if (time >= 0) {
		tv.tv_sec = time / 1000;
		tv.tv_usec = (time % 1000) * 1000;
		time_val = &tv;
	    }
	    else
		time_val = NULL;
	    if (select(max_fd + 1, &fds, NULL, NULL, time_val) < 0) {
		if (errno != EINTR)
		    mp_tmsg(MSGT_INPUT, MSGL_ERR, "Select error: %s\n",
			    strerror(errno));
		FD_ZERO(&fds);
	    }
	}
    }
#else
    if (!got_cmd && time)
	usec_sleep(time * 1000);
#endif


    for (i = 0; i < ictx->num_key_fd; i++) {
#ifdef HAVE_POSIX_SELECT
	if (!key_fds[i].no_select && !FD_ISSET(key_fds[i].fd, &fds))
	    continue;
#endif

	int code = key_fds[i].read_func.key(key_fds[i].ctx, key_fds[i].fd);
	if (code >= 0) {
	    mp_cmd_t *ret = interpret_key(ictx, code);
	    if (ret)
		return ret;
	}
	else if (code == MP_INPUT_ERROR)
	    mp_tmsg(MSGT_INPUT, MSGL_ERR, "Error on key input file descriptor %d\n",
		   key_fds[i].fd);
	else if (code == MP_INPUT_DEAD) {
	    mp_tmsg(MSGT_INPUT, MSGL_ERR, "Dead key input on file descriptor %d\n",
		   key_fds[i].fd);
	    key_fds[i].dead = 1;
	}
    }
    mp_cmd_t *autorepeat_cmd = check_autorepeat(ictx);
    if (autorepeat_cmd)
	return autorepeat_cmd;

    for (i = 0; i < ictx->num_cmd_fd; i++) {
#ifdef HAVE_POSIX_SELECT
	if (!cmd_fds[i].no_select && !FD_ISSET(cmd_fds[i].fd, &fds) &&
	    !cmd_fds[i].got_cmd)
	    continue;
#endif
	char *cmd;
	int r = read_cmd(&cmd_fds[i], &cmd);
	if (r >= 0) {
	    mp_cmd_t *ret = mp_input_parse_cmd(cmd);
	    talloc_free(cmd);
	    if (ret)
		return ret;
	}
	else if (r == MP_INPUT_ERROR)
	    mp_tmsg(MSGT_INPUT, MSGL_ERR, "Error on command file descriptor %d\n",
		   cmd_fds[i].fd);
	else if (r == MP_INPUT_DEAD)
	    cmd_fds[i].dead = 1;
    }

    return NULL;
}


int mp_input_queue_cmd(struct input_ctx *ictx, mp_cmd_t* cmd)
{
  if (!cmd || ictx->cmd_queue_length  >= CMD_QUEUE_SIZE)
    return 0;
  ictx->cmd_queue[ictx->cmd_queue_end] = cmd;
  ictx->cmd_queue_end = (ictx->cmd_queue_end + 1) % CMD_QUEUE_SIZE;
  ictx->cmd_queue_length++;
  return 1;
}

static mp_cmd_t *get_queued_cmd(struct input_ctx *ictx, int peek_only)
{
  mp_cmd_t* ret;

  if (ictx->cmd_queue_length == 0)
    return NULL;

  ret = ictx->cmd_queue[ictx->cmd_queue_start];

  if (!peek_only) {
      ictx->cmd_queue_length--;
      ictx->cmd_queue_start = (ictx->cmd_queue_start + 1) % CMD_QUEUE_SIZE;
  }

  return ret;
}

/**
 * \param peek_only when set, the returned command stays in the queue.
 * Do not free the returned cmd whe you set this!
 */
mp_cmd_t *mp_input_get_cmd(struct input_ctx *ictx, int time, int peek_only)
{
  mp_cmd_t* ret = NULL;
  mp_cmd_filter_t* cf;
  int from_queue;

  if (async_quit_request)
    return mp_input_parse_cmd("quit 1");
  while(1) {
    from_queue = 1;
    ret = get_queued_cmd(ictx, peek_only);
    if(ret) break;
    from_queue = 0;
    ret = read_events(ictx, time);
    if (!ret) {
	from_queue = 1;
	ret = get_queued_cmd(ictx, peek_only);
    }
    break;
  }
  if(!ret) return NULL;

  for(cf = cmd_filters ; cf ; cf = cf->next) {
    if (cf->filter(ret, cf->ctx)) {
      if (peek_only && from_queue)
        // The filter ate the cmd, so we remove it from queue
          ret = get_queued_cmd(ictx, 0);
      mp_cmd_free(ret);
      return NULL;
    }
  }

  if (!from_queue && peek_only)
      mp_input_queue_cmd(ictx, ret);

  return ret;
}

void
mp_cmd_free(mp_cmd_t* cmd) {
    talloc_free(cmd);
}

mp_cmd_t*
mp_cmd_clone(mp_cmd_t* cmd) {
  mp_cmd_t* ret;
  int i;
#ifdef MP_DEBUG
  assert(cmd != NULL);
#endif

  ret = talloc_memdup(NULL, cmd, sizeof(mp_cmd_t));
  ret->name = talloc_strdup(ret, cmd->name);
  for(i = 0;  i < MP_CMD_MAX_ARGS && cmd->args[i].type != -1; i++) {
    if(cmd->args[i].type == MP_CMD_ARG_STRING && cmd->args[i].v.s != NULL)
        ret->args[i].v.s = talloc_strdup(ret, cmd->args[i].v.s);
  }

  return ret;
}

int mp_input_get_key_from_name(const char *name)
{
    int modifiers = 0;
    const char *p;
    while (p = strchr(name, '+')) {
        for (struct mp_key_name *m = modifier_names; m->name; m++)
            if (!bstrcasecmp(BSTR(m->name), (struct bstr){name, p - name})) {
                modifiers |= m->key;
                goto found;
            }
        if (!strcmp(name, "+"))
            return '+' + modifiers;
        return -1;
    found:
        name = p + 1;
    }
    int len = strlen(name);
    if (len == 1)   // Direct key code
        return (unsigned char)name[0] + modifiers;
    else if (len > 2 && strncasecmp("0x", name, 2) == 0)
        return strtol(name, NULL, 16) + modifiers;

    for (int i = 0; key_names[i].name != NULL; i++) {
        if (strcasecmp(key_names[i].name, name) == 0)
            return key_names[i].key + modifiers;
    }

    return -1;
}

static int get_input_from_name(char* name,int* keys) {
  char *end,*ptr;
  int n=0;

  ptr = name;
  n = 0;
  for(end = strchr(ptr,'-') ; ptr != NULL ; end = strchr(ptr,'-')) {
    if(end && end[1] != '\0') {
      if(end[1] == '-')
	end = &end[1];
      end[0] = '\0';
    }
    keys[n] = mp_input_get_key_from_name(ptr);
    if(keys[n] < 0) {
      return 0;
    }
    n++;
    if(end && end[1] != '\0' && n < MP_MAX_KEY_DOWN)
      ptr = &end[1];
    else
      break;
  }
  keys[n] = 0;
  return 1;
}

#define BS_MAX 256
#define SPACE_CHAR " \n\r\t"

static void bind_keys(struct input_ctx *ictx,
                      const int keys[MP_MAX_KEY_DOWN+1], char *cmd)
{
  int i = 0,j;
  mp_cmd_bind_t* bind = NULL;
  mp_cmd_bind_section_t* bind_section = NULL;
  char *section=NULL, *p;

#ifdef MP_DEBUG
  assert(keys != NULL);
  assert(cmd != NULL);
#endif

  if(*cmd=='{' && (p=strchr(cmd,'}'))) {
    *p=0;
    section=++cmd;
    cmd=++p;
    // Jump beginning space
    for(  ; cmd[0] != '\0' && strchr(SPACE_CHAR,cmd[0]) != NULL ; cmd++)
      /* NOTHING */;
  }
  bind_section = get_bind_section(ictx, section);

  if(bind_section->cmd_binds) {
    for(i = 0; bind_section->cmd_binds[i].cmd != NULL ; i++) {
      for(j = 0 ; bind_section->cmd_binds[i].input[j] == keys[j]  && keys[j] != 0 ; j++)
	/* NOTHING */;
      if(keys[j] == 0 && bind_section->cmd_binds[i].input[j] == 0 ) {
	bind = &bind_section->cmd_binds[i];
	break;
      }
    }
  }

  if(!bind) {
      bind_section->cmd_binds = talloc_realloc(bind_section,
                                               bind_section->cmd_binds,
                                               mp_cmd_bind_t, i + 2);
    memset(&bind_section->cmd_binds[i],0,2*sizeof(mp_cmd_bind_t));
    bind = &bind_section->cmd_binds[i];
  }
  talloc_free(bind->cmd);
  bind->cmd = talloc_strdup(bind_section->cmd_binds, cmd);
  memcpy(bind->input,keys,(MP_MAX_KEY_DOWN+1)*sizeof(int));
}

static int parse_config(struct input_ctx *ictx, char *file)
{
  int fd;
  int bs = 0,r,eof = 0,comments = 0;
  char *iter,*end;
  char buffer[BS_MAX];
  int n_binds = 0, keys[MP_MAX_KEY_DOWN+1] = { 0 };

  fd = open(file,O_RDONLY);

  if(fd < 0) {
    mp_msg(MSGT_INPUT,MSGL_V,"Can't open input config file %s: %s\n",file,strerror(errno));
    return 0;
  }

  mp_msg(MSGT_INPUT,MSGL_V,"Parsing input config file %s\n",file);

  while(1) {
    if(! eof && bs < BS_MAX-1) {
      if(bs > 0) bs--;
      r = read(fd,buffer+bs,BS_MAX-1-bs);
      if(r < 0) {
	if(errno == EINTR)
	  continue;
	mp_tmsg(MSGT_INPUT,MSGL_ERR,"Error while reading input config file %s: %s\n",file,strerror(errno));
	close(fd);
	return 0;
      } else if(r == 0) {
	eof = 1;
      } else {
	bs += r+1;
	buffer[bs-1] = '\0';
      }
    }
    // Empty buffer : return
    if(bs <= 1) {
      mp_msg(MSGT_INPUT,MSGL_V,"Input config file %s parsed: %d binds\n",file,n_binds);
      close(fd);
      return 1;
    }

    iter = buffer;

    if(comments) {
      for( ; iter[0] != '\0' && iter[0] != '\n' ; iter++)
	/* NOTHING */;
      if(iter[0] == '\0') { // Buffer was full of comment
	bs = 0;
	continue;
      }
      iter++;
      r = strlen(iter);
      memmove(buffer,iter,r+1);
      bs = r+1;
      comments = 0;
      continue;
    }

    // Find the wanted key
    if(keys[0] == 0) {
      // Jump beginning space
      for(  ; iter[0] != '\0' && strchr(SPACE_CHAR,iter[0]) != NULL ; iter++)
	/* NOTHING */;
      if(iter[0] == '\0') { // Buffer was full of space char
	bs = 0;
	continue;
      }
      if(iter[0] == '#') { // Comments
	comments = 1;
	continue;
      }
      // Find the end of the key code name
      for(end = iter; end[0] != '\0' && strchr(SPACE_CHAR,end[0]) == NULL ; end++)
	/*NOTHING */;
      if(end[0] == '\0') { // Key name doesn't fit in the buffer
	if(buffer == iter) {
	  if(eof && (buffer-iter) == bs)
	    mp_tmsg(MSGT_INPUT,MSGL_ERR,"Unfinished binding %s\n",iter);
	  else
	    mp_tmsg(MSGT_INPUT,MSGL_ERR,"Buffer is too small for this key name: %s\n",iter);
	  return 0;
	}
	memmove(buffer,iter,end-iter);
	bs = end-iter;
	continue;
      }
      {
	char name[end-iter+1];
	strncpy(name,iter,end-iter);
	name[end-iter] = '\0';
	if (!get_input_from_name(name,keys)) {
	  mp_tmsg(MSGT_INPUT,MSGL_ERR,"Unknown key '%s'\n",name);
	  close(fd);
	  return 0;
	}
      }
      if( bs > (end-buffer))
	memmove(buffer,end,bs - (end-buffer));
      bs -= end-buffer;
      continue;
    } else { // Get the command
      while(iter[0] == ' ' || iter[0] == '\t') iter++;
      // Found new line
      if(iter[0] == '\n' || iter[0] == '\r') {
	int i;
        char *key_buf  = get_key_name(keys[0]);
	mp_tmsg(MSGT_INPUT,MSGL_ERR,"No command found for key %s", key_buf);
        talloc_free(key_buf);
	for(i = 1; keys[i] != 0 ; i++) {
            char *key_buf  = get_key_name(keys[i]);
            mp_msg(MSGT_INPUT,MSGL_ERR,"-%s", key_buf);
            talloc_free(key_buf);
        }
        mp_msg(MSGT_INPUT,MSGL_ERR,"\n");
	keys[0] = 0;
	if(iter > buffer) {
	  memmove(buffer,iter,bs- (iter-buffer));
	  bs -= (iter-buffer);
	}
	continue;
      }
      for(end = iter ; end[0] != '\n' && end[0] != '\r' && end[0] != '\0' ; end++)
	/* NOTHING */;
      if(end[0] == '\0' && ! (eof && ((end+1) - buffer) == bs)) {
	if(iter == buffer) {
	  mp_tmsg(MSGT_INPUT,MSGL_ERR,"Buffer is too small for command %s\n",buffer);
	  close(fd);
	  return 0;
	}
	memmove(buffer,iter,end - iter);
	bs = end - iter;
	continue;
      }
      {
	char cmd[end-iter+1];
	strncpy(cmd,iter,end-iter);
	cmd[end-iter] = '\0';
	//printf("Set bind %d => %s\n",keys[0],cmd);
	bind_keys(ictx, keys,cmd);
	n_binds++;
      }
      keys[0] = 0;
      end++;
      if(bs > (end-buffer))
	memmove(buffer,end,bs-(end-buffer));
      bs -= (end-buffer);
      buffer[bs-1] = '\0';
      continue;
    }
  }
  mp_tmsg(MSGT_INPUT,MSGL_ERR,"What are we doing here?\n");
  close(fd);
  mp_input_set_section(ictx, NULL);
  return 0;
}

void mp_input_set_section(struct input_ctx *ictx, char *name)
{
    mp_cmd_bind_section_t* bind_section = NULL;

    ictx->cmd_binds = NULL;
    ictx->cmd_binds_default = NULL;
    talloc_free(ictx->section);
    if (name)
        ictx->section = talloc_strdup(ictx, name);
    else
        ictx->section = talloc_strdup(ictx, "default");
    if ((bind_section = get_bind_section(ictx, ictx->section)))
        ictx->cmd_binds = bind_section->cmd_binds;
    if (strcmp(ictx->section, "default") == 0)
        return;
    if ((bind_section = get_bind_section(ictx, NULL)))
        ictx->cmd_binds_default = bind_section->cmd_binds;
}

char *mp_input_get_section(struct input_ctx *ictx)
{
    return ictx->section;
}

struct input_ctx *mp_input_init(struct input_conf *input_conf)
{
    struct input_ctx *ictx = talloc_ptrtype(NULL, ictx);
    *ictx = (struct input_ctx){
        .ar_state = -1,
        .ar_delay = input_conf->ar_delay,
        .ar_rate = input_conf->ar_rate,
        .default_bindings = input_conf->default_bindings,
    };

  char* file;
  char *config_file = input_conf->config_file;
  file = config_file[0] != '/' ? get_path(config_file) : config_file;
  if(!file)
    return ictx;

  if (!parse_config(ictx, file)) {
    // free file if it was allocated by get_path(),
    // before it gets overwritten
    if( file != config_file)
    {
      free(file);
    }
    // Try global conf dir
    file = MPLAYER_CONFDIR "/input.conf";
    if (!parse_config(ictx, file))
      mp_msg(MSGT_INPUT,MSGL_V,"Falling back on default (hardcoded) input config\n");
  }
  else
  {
    // free file if it was allocated by get_path()
    if( file != config_file)
      free(file);
  }

#ifdef CONFIG_JOYSTICK
  if (input_conf->use_joystick) {
    int fd = mp_input_joystick_init(input_conf->js_dev);
    if(fd < 0)
      mp_tmsg(MSGT_INPUT,MSGL_ERR,"Can't init input joystick\n");
    else
        mp_input_add_key_fd(ictx, fd, 1, mp_input_joystick_read,
                            (mp_close_func_t)close,NULL);
  }
#endif

#ifdef CONFIG_LIRC
  if (input_conf->use_lirc) {
    int fd = mp_input_lirc_init();
    if(fd > 0)
        mp_input_add_cmd_fd(ictx, fd, 0, mp_input_lirc_read,
                            mp_input_lirc_close);
  }
#endif

#ifdef CONFIG_LIRCC
  if (input_conf->use_lircc) {
    int fd = lircc_init("mplayer", NULL);
    if(fd >= 0)
        mp_input_add_cmd_fd(ictx, fd, 1, NULL, (mp_close_func_t)lircc_cleanup);
  }
#endif

#ifdef CONFIG_APPLE_REMOTE
  if (input_conf->use_ar) {
    if(mp_input_ar_init() < 0)
      mp_tmsg(MSGT_INPUT,MSGL_ERR,"Can't init Apple Remote.\n");
    else
        mp_input_add_key_fd(ictx, -1, 0, mp_input_ar_read, mp_input_ar_close,
                            NULL);
  }
#endif

#ifdef CONFIG_APPLE_IR
  if (input_conf->use_ar) {
    int fd = mp_input_appleir_init(input_conf->ar_dev);
    if(fd < 0)
      mp_tmsg(MSGT_INPUT,MSGL_ERR,"Can't init Apple Remote.\n");
    else
        mp_input_add_key_fd(ictx, fd, 1, mp_input_appleir_read,
                            (mp_close_func_t)close, NULL);
  }
#endif

  if (input_conf->in_file) {
    struct stat st;
    int mode = O_RDONLY;
    // Use RDWR for FIFOs to ensure they stay open over multiple accesses.
    // Note that on Windows stat may fail for named pipes, but due to how the
    // API works, using RDONLY should be ok.
    if (stat(input_conf->in_file, &st) == 0 && S_ISFIFO(st.st_mode))
      mode = O_RDWR;
    int in_file_fd = open(input_conf->in_file, mode);
    if(in_file_fd >= 0)
      mp_input_add_cmd_fd(ictx, in_file_fd, 1, NULL,
                          (mp_close_func_t)close);
    else
      mp_tmsg(MSGT_INPUT, MSGL_ERR, "Can't open %s: %s\n",
              input_conf->in_file, strerror(errno));
  }
  return ictx;
}

void mp_input_uninit(struct input_ctx *ictx)
{
    if (!ictx)
        return;

  unsigned int i;

  for (i=0; i < ictx->num_key_fd; i++) {
      if (ictx->key_fds[i].close_func)
          ictx->key_fds[i].close_func(ictx->key_fds[i].fd);
  }

  for (i = 0; i < ictx->num_cmd_fd; i++) {
      if (ictx->cmd_fds[i].close_func)
          ictx->cmd_fds[i].close_func(ictx->cmd_fds[i].fd);
  }
  talloc_free(ictx);
}

void
mp_input_register_options(m_config_t* cfg) {
  m_config_register_options(cfg,mp_input_opts);
}

static int print_key_list(m_option_t* cfg)
{
  int i;
  printf("\n");
  for(i= 0; key_names[i].name != NULL ; i++)
    printf("%s\n",key_names[i].name);
  exit(0);
}

static int print_cmd_list(m_option_t* cfg)
{
  const mp_cmd_t *cmd;
  int i,j;
  const char* type;

  for(i = 0; (cmd = &mp_cmds[i])->name != NULL ; i++) {
    printf("%-20.20s",cmd->name);
    for(j= 0 ; j < MP_CMD_MAX_ARGS && cmd->args[j].type != -1 ; j++) {
      switch(cmd->args[j].type) {
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
      if(j+1 > cmd->nargs)
	printf(" [%s]",type);
      else
	printf(" %s",type);
    }
    printf("\n");
  }
  exit(0);
}

/**
 * \param time time to wait for an interruption in milliseconds
 */
int mp_input_check_interrupt(struct input_ctx *ictx, int time)
{
  mp_cmd_t* cmd;
  if ((cmd = mp_input_get_cmd(ictx, time, 1)) == NULL)
    return 0;
  switch(cmd->id) {
  case MP_CMD_QUIT:
  case MP_CMD_PLAY_TREE_STEP:
  case MP_CMD_PLAY_TREE_UP_STEP:
  case MP_CMD_PLAY_ALT_SRC_STEP:
    // The cmd will be executed when we are back in the main loop
    return 1;
  }
  // remove the cmd from the queue
  cmd = mp_input_get_cmd(ictx, time, 0);
  mp_cmd_free(cmd);
  return 0;
}
