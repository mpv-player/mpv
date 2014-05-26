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

#include <limits.h>

#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "input.h"
#include "cmd_list.h"
#include "cmd_parse.h"

// This does not specify the real destination of the command parameter values,
// it just provides a dummy for the OPT_ macros.
#define OPT_BASE_STRUCT struct mp_cmd_arg
#define ARG(t) "", v. t

/* This array defines all known commands.
 * The first field is an id used to recognize the command.
 * The second is the command name used in slave mode and input.conf.
 * Then comes the definition of each argument, first mandatory arguments
 * (ARG_INT, ARG_FLOAT, ARG_STRING) if any, then optional arguments
 * (OARG_INT(default), etc) if any. The command will be given the default
 * argument value if the user didn't give enough arguments to specify it.
 * A command can take a maximum of MP_CMD_MAX_ARGS arguments.
 */

#define ARG_INT                 OPT_INT(ARG(i), 0)
#define ARG_FLOAT               OPT_FLOAT(ARG(f), 0)
#define ARG_DOUBLE              OPT_DOUBLE(ARG(d), 0)
#define ARG_STRING              OPT_STRING(ARG(s), 0)
#define ARG_CHOICE(c)           OPT_CHOICE(ARG(i), 0, c)
#define ARG_CHOICE_OR_INT(...)  OPT_CHOICE_OR_INT(ARG(i), 0, __VA_ARGS__)
#define ARG_TIME                OPT_TIME(ARG(d), 0)
#define OARG_DOUBLE(def)        OPT_DOUBLE(ARG(d), 0, OPTDEF_DOUBLE(def))
#define OARG_INT(def)           OPT_INT(ARG(i), 0, OPTDEF_INT(def))
#define OARG_CHOICE(def, c)     OPT_CHOICE(ARG(i), 0, c, OPTDEF_INT(def))

#define OARG_CYCLEDIR(def)      OPT_CYCLEDIR(ARG(d), 0, OPTDEF_DOUBLE(def))

const struct mp_cmd_def mp_cmds[] = {
  { MP_CMD_IGNORE, "ignore", },
    
  { MP_CMD_WRITE_WATCH_LATER_CONF, "write_watch_later_conf", },

  { MP_CMD_SEEK, "seek", {
      ARG_TIME,
      OARG_CHOICE(0, ({"relative", 0},          {"0", 0}, {"-", 0},
                      {"absolute-percent", 1},  {"1", 1},
                      {"absolute", 2},          {"2", 2})),
      OARG_CHOICE(0, ({"default-precise", 0},   {"0", 0},
                      {"exact", 1},             {"1", 1},
                      {"keyframes", -1},        {"-1", -1})),
    },
    .allow_auto_repeat = true,
  },
  { MP_CMD_REVERT_SEEK, "revert_seek", },
  { MP_CMD_QUIT, "quit", { OARG_INT(0) } },
  { MP_CMD_QUIT_WATCH_LATER, "quit_watch_later", { OARG_INT(0) } },
  { MP_CMD_STOP, "stop", },
  { MP_CMD_FRAME_STEP, "frame_step", .allow_auto_repeat = true },
  { MP_CMD_FRAME_BACK_STEP, "frame_back_step", .allow_auto_repeat = true },
  { MP_CMD_PLAYLIST_NEXT, "playlist_next", {
      OARG_CHOICE(0, ({"weak", 0},              {"0", 0},
                      {"force", 1},             {"1", 1})),
  }},
  { MP_CMD_PLAYLIST_PREV, "playlist_prev", {
      OARG_CHOICE(0, ({"weak", 0},              {"0", 0},
                      {"force", 1},             {"1", 1})),
  }},
  { MP_CMD_SUB_STEP, "sub_step", { ARG_INT }, .allow_auto_repeat = true },
  { MP_CMD_SUB_SEEK, "sub_seek", { ARG_INT }, .allow_auto_repeat = true },
  { MP_CMD_OSD, "osd", { OARG_INT(-1) } },
  { MP_CMD_PRINT_TEXT, "print_text", { ARG_STRING }, .allow_auto_repeat = true },
  { MP_CMD_SHOW_TEXT, "show_text", { ARG_STRING, OARG_INT(-1), OARG_INT(0) },
    .allow_auto_repeat = true},
  { MP_CMD_SHOW_PROGRESS, "show_progress",  .allow_auto_repeat = true},
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
                      {"subtitles", 2}, {"-", 2})),
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
      OPT_KEYVALUELIST(ARG(str_list), MP_CMD_OPT_ARG),
  }},
  { MP_CMD_LOADLIST, "loadlist", {
      ARG_STRING,
      OARG_CHOICE(0, ({"replace", 0},          {"0", 0},
                      {"append", 1},           {"1", 1})),
  }},
  { MP_CMD_PLAYLIST_CLEAR, "playlist_clear", },
  { MP_CMD_PLAYLIST_REMOVE, "playlist_remove", {
      ARG_CHOICE_OR_INT(0, INT_MAX, ({"current", -1})),
  }},
  { MP_CMD_PLAYLIST_MOVE, "playlist_move", { ARG_INT, ARG_INT } },
  { MP_CMD_RUN, "run", { ARG_STRING, ARG_STRING }, .vararg = true },

  { MP_CMD_SET, "set", { ARG_STRING,  ARG_STRING } },
  { MP_CMD_GET_PROPERTY, "get_property", { ARG_STRING } },
  { MP_CMD_ADD, "add", { ARG_STRING, OARG_DOUBLE(0) },
    .allow_auto_repeat = true},
  { MP_CMD_CYCLE, "cycle", {
      ARG_STRING,
      OARG_CYCLEDIR(1),
    },
    .allow_auto_repeat = true
  },
  { MP_CMD_MULTIPLY, "multiply", { ARG_STRING, ARG_DOUBLE },
    .allow_auto_repeat = true},

  { MP_CMD_CYCLE_VALUES, "cycle_values", { ARG_STRING, ARG_STRING, ARG_STRING },
    .vararg = true, .allow_auto_repeat = true},

  { MP_CMD_ENABLE_INPUT_SECTION,  "enable_section",  {
      ARG_STRING,
      OARG_CHOICE(0, ({"default", 0},
                      {"exclusive", 1})),
  }},
  { MP_CMD_DISABLE_INPUT_SECTION, "disable_section", { ARG_STRING } },

  { MP_CMD_DISCNAV, "discnav", { ARG_STRING } },

  { MP_CMD_AF, "af", { ARG_STRING, ARG_STRING } },

  { MP_CMD_VF, "vf", { ARG_STRING, ARG_STRING } },

  { MP_CMD_VO_CMDLINE, "vo_cmdline", { ARG_STRING } },

  { MP_CMD_SCRIPT_DISPATCH, "script_dispatch", { ARG_STRING, ARG_INT } },
  { MP_CMD_SCRIPT_MESSAGE, "script_message", { ARG_STRING }, .vararg = true },
  { MP_CMD_SCRIPT_MESSAGE_TO, "script_message_to", { ARG_STRING, ARG_STRING },
    .vararg = true },

  { MP_CMD_OVERLAY_ADD, "overlay_add",
      { ARG_INT, ARG_INT, ARG_INT, ARG_STRING, ARG_INT, ARG_STRING, ARG_INT,
        ARG_INT, ARG_INT }},
  { MP_CMD_OVERLAY_REMOVE, "overlay_remove", { ARG_INT } },

  {0}
};

#undef OPT_BASE_STRUCT
#undef ARG

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
    {"show_chapters",           "show_text ${chapter-list}"},
    {"show_tracks_osd",         "show_text ${track-list}"},
    {"show_tracks",             "show_text ${track-list}"},
    {"show_playlist",           "show_text ${playlist}"},
    {"speed_mult",              "multiply speed"},
    {"dvdnav",                  "discnav"},

    // Approximate (can fail if user added additional whitespace)
    {"pt_step 1",               "playlist_next"},
    {"pt_step -1",              "playlist_prev"},
    // Switch_ratio without argument resets aspect ratio
    {"switch_ratio ",           "set aspect "},
    {"switch_ratio",            "set aspect 0"},
    {0}
};

bool mp_replace_legacy_cmd(void *t, bstr *s)
{
    for (const struct legacy_cmd *entry = legacy_cmds; entry->old; entry++) {
        bstr old = bstr0(entry->old);
        if (bstrcasecmp(bstr_splice(*s, 0, old.len), old) == 0) {
            bstr rest = bstr_cut(*s, old.len);
            *s = bstr0(talloc_asprintf(t, "%s%.*s", entry->new, BSTR_P(rest)));
            return true;
        }
    }
    return false;
}

static bool is_abort_cmd(int cmd_id)
{
    switch (cmd_id) {
    case MP_CMD_QUIT:
    case MP_CMD_PLAYLIST_NEXT:
    case MP_CMD_PLAYLIST_PREV:
        return true;
    }
    return false;
}

bool mp_input_is_abort_cmd(struct mp_cmd *cmd)
{
    if (is_abort_cmd(cmd->id))
        return true;
    if (cmd->id == MP_CMD_COMMAND_LIST) {
        for (struct mp_cmd *sub = cmd->args[0].v.p; sub; sub = sub->queue_next)
        {
            if (mp_input_is_abort_cmd(sub))
                return true;
        }
    }
    return false;
}

void mp_print_cmd_list(struct mp_log *out)
{
    for (int i = 0; mp_cmds[i].name; i++) {
        const struct mp_cmd_def *def = &mp_cmds[i];
        mp_info(out, "%-20.20s", def->name);
        for (int j = 0; j < MP_CMD_MAX_ARGS && def->args[j].type; j++) {
            const char *type = def->args[j].type->name;
            if (def->args[j].defval)
                mp_info(out, " [%s]", type);
            else
                mp_info(out, " %s", type);
        }
        mp_info(out, "\n");
    }
}
