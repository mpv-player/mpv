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

#include <limits.h>

#include "config.h"

#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "input.h"
#include "cmd_list.h"
#include "cmd_parse.h"

// 0: no, 1: maybe, 2: sure
static int is_abort_cmd(struct mp_cmd *cmd)
{
    if (cmd->def->is_abort)
        return 2;
    if (cmd->def->is_soft_abort)
        return 1;
    if (cmd->def == &mp_cmd_list) {
        int r = 0;
        for (struct mp_cmd *sub = cmd->args[0].v.p; sub; sub = sub->queue_next) {
            int x = is_abort_cmd(sub);
            r = MPMAX(r, x);
        }
        return r;
    }
    return 0;
}

bool mp_input_is_maybe_abort_cmd(struct mp_cmd *cmd)
{
    return is_abort_cmd(cmd) >= 1;
}

bool mp_input_is_abort_cmd(struct mp_cmd *cmd)
{
    return is_abort_cmd(cmd) >= 2;
}

bool mp_input_is_repeatable_cmd(struct mp_cmd *cmd)
{
    return (cmd->def->allow_auto_repeat) || cmd->def == &mp_cmd_list ||
           (cmd->flags & MP_ALLOW_REPEAT);
}

bool mp_input_is_scalable_cmd(struct mp_cmd *cmd)
{
    return cmd->def->scalable;
}

void mp_print_cmd_list(struct mp_log *out)
{
    for (int i = 0; mp_cmds[i].name; i++) {
        const struct mp_cmd_def *def = &mp_cmds[i];
        mp_info(out, "%-20.20s", def->name);
        for (int j = 0; j < MP_CMD_DEF_MAX_ARGS && def->args[j].type; j++) {
            const char *type = def->args[j].type->name;
            if (def->args[j].defval)
                mp_info(out, " [%s]", type);
            else
                mp_info(out, " %s", type);
        }
        mp_info(out, "\n");
    }
}
