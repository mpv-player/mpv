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
    {"sub_load",                "sub-add"},
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
    {"osd_show_text",           "show-text"},
    {"osd_show_property_text",  "show-text"},
    {"osd_show_progression",    "show-progress"},
    {"show_chapters_osd",       "show-text ${chapter-list}"},
    {"show_chapters",           "show-text ${chapter-list}"},
    {"show_tracks_osd",         "show-text ${track-list}"},
    {"show_tracks",             "show-text ${track-list}"},
    {"show_playlist",           "show-text ${playlist}"},
    {"speed_mult",              "multiply speed"},

    // Approximate (can fail if user added additional whitespace)
    {"pt_step 1",               "playlist-next"},
    {"pt_step -1",              "playlist-prev"},
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

// 0: no, 1: maybe, 2: sure
static int is_abort_cmd(struct mp_cmd *cmd)
{
    switch (cmd->id) {
    case MP_CMD_QUIT:
    case MP_CMD_QUIT_WATCH_LATER:
    case MP_CMD_STOP:
        return 2;
    case MP_CMD_PLAYLIST_NEXT:
    case MP_CMD_PLAYLIST_PREV:
        return 1;
    case MP_CMD_COMMAND_LIST:;
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
    return (cmd->def && cmd->def->allow_auto_repeat) ||
           cmd->id == MP_CMD_COMMAND_LIST ||
           (cmd->flags & MP_ALLOW_REPEAT);
}

bool mp_input_is_scalable_cmd(struct mp_cmd *cmd)
{
    return cmd->def && cmd->def->scalable;
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
