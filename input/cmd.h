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
#define MP_CMD_OPT_ARG 0x1000

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
};

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
struct mp_cmd *mp_input_parse_cmd_(struct mp_log *log, bstr str, const char *loc);

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
