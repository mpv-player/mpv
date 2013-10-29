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

#ifndef MPLAYER_COMMAND_H
#define MPLAYER_COMMAND_H

struct MPContext;
struct mp_cmd;

void command_init(struct MPContext *mpctx);
void command_uninit(struct MPContext *mpctx);

void run_command(struct MPContext *mpctx, struct mp_cmd *cmd);
char *mp_property_expand_string(struct MPContext *mpctx, const char *str);
void property_print_help(void);
int mp_property_do(const char* name, int action, void* val,
                   struct MPContext *mpctx);

const struct m_option *mp_get_property_list(void);

enum mp_event {
    MP_EVENT_NONE,
    MP_EVENT_TICK,
    MP_EVENT_PROPERTY,          // char*, property that is changed
    MP_EVENT_TRACKS_CHANGED,
    MP_EVENT_START_FILE,
    MP_EVENT_END_FILE,
};

void mp_notify(struct MPContext *mpctx, enum mp_event event, void *arg);
void mp_notify_property(struct MPContext *mpctx, const char *property);

void mp_flush_events(struct MPContext *mpctx);

#endif /* MPLAYER_COMMAND_H */
