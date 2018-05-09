/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MP_PARSE_COMMAND_H
#define MP_PARSE_COMMAND_H

#include <stdbool.h>

#include "misc/bstr.h"
#include "options/m_option.h"

#define MP_CMD_DEF_MAX_ARGS 9
#define MP_CMD_OPT_ARG M_OPT_OPTIONAL_PARAM

struct mp_log;
struct mp_cmd;
struct mpv_node;

struct mp_cmd_def {
    const char *name;   // user-visible name (as used in input.conf)
    void (*handler)(void *ctx);
    const struct m_option args[MP_CMD_DEF_MAX_ARGS];
    const void *priv;   // for free use by handler()
    bool allow_auto_repeat; // react to repeated key events
    bool on_updown;     // always emit it on both up and down key events
    bool vararg;        // last argument can be given 0 to multiple times
    bool scalable;
    bool is_abort;
    bool is_soft_abort;
    bool is_ignore;
    bool default_async; // default to MP_ASYNC flag if none set by user
    // If you set this, handler() must ensure mp_cmd_ctx_complete() is called
    // at some point (can be after handler() returns). If you don't set it, the
    // common code will call mp_cmd_ctx_complete() when handler() returns.
    // You must make sure that the core cannot disappear while you do work. The
    // common code keeps the core referenced only until handler() returns.
    bool exec_async;
    // If set, handler() is run on a separate worker thread. This means you can
    // use mp_core_[un]lock() to temporarily unlock and re-lock the core (while
    // unlocked, you have no synchronized access to mpctx, but you can do long
    // running operations without blocking playback or input handling).
    bool spawn_thread;
};

enum mp_cmd_flags {
    MP_ON_OSD_NO = 0,           // prefer not using OSD
    MP_ON_OSD_AUTO = 1,         // use default behavior of the specific command
    MP_ON_OSD_BAR = 2,          // force a bar, if applicable
    MP_ON_OSD_MSG = 4,          // force a message, if applicable
    MP_EXPAND_PROPERTIES = 8,   // expand strings as properties
    MP_ALLOW_REPEAT = 16,       // if used as keybinding, allow key repeat

    // Exactly one of the following 2 bits is set. Which one is used depends on
    // the command parser (prefixes and mp_cmd_def.default_async).
    MP_ASYNC_CMD = 32,          // do not wait for command to complete
    MP_SYNC_CMD = 64,           // block on command completion

    MP_ON_OSD_FLAGS = MP_ON_OSD_NO | MP_ON_OSD_AUTO |
                      MP_ON_OSD_BAR | MP_ON_OSD_MSG,
};

// Arbitrary upper bound for sanity.
#define MP_CMD_MAX_ARGS 100

struct mp_cmd_arg {
    const struct m_option *type;
    union {
        int i;
        float f;
        double d;
        char *s;
        char **str_list;
        void *p;
    } v;
};

typedef struct mp_cmd {
    char *name;
    struct mp_cmd_arg *args;
    int nargs;
    int flags; // mp_cmd_flags bitfield
    bstr original;
    char *input_section;
    bool is_up_down : 1;
    bool is_up : 1;
    bool emit_on_up : 1;
    bool is_mouse_button : 1;
    bool repeated : 1;
    bool mouse_move : 1;
    int mouse_x, mouse_y;
    struct mp_cmd *queue_next;
    double scale;               // for scaling numeric arguments
    int scale_units;
    const struct mp_cmd_def *def;
    char *sender; // name of the client API user which sent this
    char *key_name; // string representation of the key binding
} mp_cmd_t;

extern const struct mp_cmd_def mp_cmds[];
extern const struct mp_cmd_def mp_cmd_list;

// Executing this command will maybe abort playback (play something else, or quit).
bool mp_input_is_maybe_abort_cmd(struct mp_cmd *cmd);
// This command will definitely abort playback.
bool mp_input_is_abort_cmd(struct mp_cmd *cmd);

bool mp_input_is_repeatable_cmd(struct mp_cmd *cmd);

bool mp_input_is_scalable_cmd(struct mp_cmd *cmd);

void mp_print_cmd_list(struct mp_log *out);

// Parse text and return corresponding struct mp_cmd.
// The location parameter is for error messages.
struct mp_cmd *mp_input_parse_cmd_str(struct mp_log *log, bstr str,
                                      const char *loc);

// Similar to mp_input_parse_cmd(), but takes a list of strings instead.
// Also, MP_ON_OSD_AUTO | MP_EXPAND_PROPERTIES are not set by default.
// Keep in mind that these functions (naturally) don't take multiple commands,
// i.e. a ";" argument does not start a new command.
struct mp_cmd *mp_input_parse_cmd_strv(struct mp_log *log, const char **argv);

struct mp_cmd *mp_input_parse_cmd_node(struct mp_log *log, struct mpv_node *node);

// After getting a command from mp_input_get_cmd you need to free it using this
// function
void mp_cmd_free(struct mp_cmd *cmd);

void mp_cmd_dump(struct mp_log *log, int msgl, char *header, struct mp_cmd *cmd);

// This creates a copy of a command (used by the auto repeat stuff).
struct mp_cmd *mp_cmd_clone(struct mp_cmd *cmd);

extern const struct m_option_type m_option_type_cycle_dir;

#define OPT_CYCLEDIR(...) \
    OPT_GENERAL(double, __VA_ARGS__, .type = &m_option_type_cycle_dir)

#endif
