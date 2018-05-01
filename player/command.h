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

#ifndef MPLAYER_COMMAND_H
#define MPLAYER_COMMAND_H

#include <stdbool.h>

struct MPContext;
struct mp_cmd;
struct mp_log;
struct mpv_node;
struct m_config_option;

void command_init(struct MPContext *mpctx);
void command_uninit(struct MPContext *mpctx);

// Runtime context for a single command.
struct mp_cmd_ctx {
    struct MPContext *mpctx;
    struct mp_cmd *cmd; // original command
    // Fields from cmd (for convenience)
    struct mp_cmd_arg *args;
    int num_args;
    const void *priv;   // cmd->def->priv
    // OSD control
    int on_osd;         // MP_ON_OSD_FLAGS;
    bool msg_osd;       // OSD message requested
    bool bar_osd;       // OSD bar requested
    bool seek_msg_osd;  // same as above, but for seek commands
    bool seek_bar_osd;
    // Return values
    bool success;       // true by default
    struct mpv_node *result;
};

int run_command(struct MPContext *mpctx, struct mp_cmd *cmd, struct mpv_node *res);
char *mp_property_expand_string(struct MPContext *mpctx, const char *str);
char *mp_property_expand_escaped_string(struct MPContext *mpctx, const char *str);
void property_print_help(struct MPContext *mpctx);
int mp_property_do(const char* name, int action, void* val,
                   struct MPContext *mpctx);

int mp_on_set_option(void *ctx, struct m_config_option *co, void *data, int flags);
void mp_option_change_callback(void *ctx, struct m_config_option *co, int flags);

void mp_notify(struct MPContext *mpctx, int event, void *arg);
void mp_notify_property(struct MPContext *mpctx, const char *property);

void handle_command_updates(struct MPContext *mpctx);

int mp_get_property_id(struct MPContext *mpctx, const char *name);
uint64_t mp_get_property_event_mask(const char *name);

enum {
    // Must start with the first unused positive value in enum mpv_event_id
    // MPV_EVENT_* and MP_EVENT_* must not overlap.
    INTERNAL_EVENT_BASE = 26,
    MP_EVENT_CHANGE_ALL,
    MP_EVENT_CACHE_UPDATE,
    MP_EVENT_WIN_RESIZE,
    MP_EVENT_WIN_STATE,
    MP_EVENT_CHANGE_PLAYLIST,
    MP_EVENT_CORE_IDLE,
    MP_EVENT_DURATION_UPDATE,
};

bool mp_hook_test_completion(struct MPContext *mpctx, char *type);
void mp_hook_start(struct MPContext *mpctx, char *type);
int mp_hook_continue(struct MPContext *mpctx, char *client, uint64_t id);
void mp_hook_add(struct MPContext *mpctx, const char *client, const char *name,
                 uint64_t user_id, int pri, bool legacy);

void mark_seek(struct MPContext *mpctx);

#endif /* MPLAYER_COMMAND_H */
