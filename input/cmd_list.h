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

#ifndef MP_COMMAND_LIST_H
#define MP_COMMAND_LIST_H

#include <stdbool.h>
#include "options/m_option.h"

#define MP_CMD_DEF_MAX_ARGS 9

#define MP_CMD_OPT_ARG 0x1000

struct mp_cmd_ctx;

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

// Executing this command will maybe abort playback (play something else, or quit).
struct mp_cmd;
bool mp_input_is_maybe_abort_cmd(struct mp_cmd *cmd);
// This command will definitely abort playback.
bool mp_input_is_abort_cmd(struct mp_cmd *cmd);

bool mp_input_is_repeatable_cmd(struct mp_cmd *cmd);

bool mp_input_is_scalable_cmd(struct mp_cmd *cmd);

struct mp_log;
void mp_print_cmd_list(struct mp_log *out);

#endif
