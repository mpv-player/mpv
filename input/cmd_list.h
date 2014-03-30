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

#ifndef MP_COMMAND_LIST_H
#define MP_COMMAND_LIST_H

#include <stdbool.h>
#include "options/m_option.h"

#define MP_CMD_MAX_ARGS 10

#define MP_CMD_OPT_ARG 0x1000

struct mp_cmd_def {
    int id;             // one of MP_CMD_...
    const char *name;   // user-visible name (as used in input.conf)
    const struct m_option args[MP_CMD_MAX_ARGS];
    bool allow_auto_repeat; // react to repeated key events
    bool vararg;        // last argument can be given 0 to multiple times
};

extern const struct mp_cmd_def mp_cmds[];

// All command IDs
enum mp_command_type {
    MP_CMD_IGNORE,
    MP_CMD_SEEK,
    MP_CMD_REVERT_SEEK,
    MP_CMD_QUIT,
    MP_CMD_QUIT_WATCH_LATER,
    MP_CMD_PLAYLIST_NEXT,
    MP_CMD_PLAYLIST_PREV,
    MP_CMD_OSD,
    MP_CMD_TV_STEP_CHANNEL,
    MP_CMD_TV_STEP_NORM,
    MP_CMD_TV_STEP_CHANNEL_LIST,
    MP_CMD_SCREENSHOT,
    MP_CMD_SCREENSHOT_TO_FILE,
    MP_CMD_LOADFILE,
    MP_CMD_LOADLIST,
    MP_CMD_PLAYLIST_CLEAR,
    MP_CMD_PLAYLIST_REMOVE,
    MP_CMD_PLAYLIST_MOVE,
    MP_CMD_SUB_STEP,
    MP_CMD_SUB_SEEK,
    MP_CMD_TV_SET_CHANNEL,
    MP_CMD_TV_LAST_CHANNEL,
    MP_CMD_TV_SET_FREQ,
    MP_CMD_TV_SET_NORM,
    MP_CMD_FRAME_STEP,
    MP_CMD_FRAME_BACK_STEP,
    MP_CMD_RUN,
    MP_CMD_SUB_ADD,
    MP_CMD_SUB_REMOVE,
    MP_CMD_SUB_RELOAD,
    MP_CMD_SET,
    MP_CMD_GET_PROPERTY,
    MP_CMD_PRINT_TEXT,
    MP_CMD_SHOW_TEXT,
    MP_CMD_SHOW_PROGRESS,
    MP_CMD_ADD,
    MP_CMD_CYCLE,
    MP_CMD_MULTIPLY,
    MP_CMD_CYCLE_VALUES,
    MP_CMD_TV_STEP_FREQ,
    MP_CMD_TV_START_SCAN,
    MP_CMD_STOP,

    MP_CMD_ENABLE_INPUT_SECTION,
    MP_CMD_DISABLE_INPUT_SECTION,

    MP_CMD_DISCNAV,

    /// DVB commands
    MP_CMD_DVB_SET_CHANNEL,

    /// Audio Filter commands
    MP_CMD_AF,

    /// Video filter commands
    MP_CMD_VF,

    /// Video output commands
    MP_CMD_VO_CMDLINE,

    /// Internal for Lua scripts
    MP_CMD_SCRIPT_DISPATCH,
    MP_CMD_SCRIPT_MESSAGE,
    MP_CMD_SCRIPT_MESSAGE_TO,

    MP_CMD_OVERLAY_ADD,
    MP_CMD_OVERLAY_REMOVE,

    // Internal
    MP_CMD_COMMAND_LIST, // list of sub-commands in args[0].v.p
};

// Executing this command will abort playback (play something else, or quit).
struct mp_cmd;
bool mp_input_is_abort_cmd(struct mp_cmd *cmd);

struct bstr;
bool mp_replace_legacy_cmd(void *talloc_ctx, struct bstr *s);

struct mp_log;
void mp_print_cmd_list(struct mp_log *out);

#endif
