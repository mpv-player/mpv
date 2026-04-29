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

#include "dnd.h"
#include "common/msg.h"
#include "misc/bstr.h"
#include "misc/node.h"
#include "misc/path_utils.h"
#include "osdep/threads.h"
#include "player/client.h"

static bool might_be_subtitle_file(mpv_node *sub_exts, char *file)
{
    for (int i = 0; i < sub_exts->u.list->num; i++) {
         mpv_node sub_ext = sub_exts->u.list->values[i];
         if (sub_ext.format != MPV_FORMAT_STRING)
             continue;
         if (!bstrcasecmp0(bstr_get_ext(bstr0(file)), sub_ext.u.string))
             return true;
    }
    return false;
}

static void handle_dnd(mpv_handle *mpv, mpv_node *files, char *action)
{
    mpv_node sub_exts = {0};
    mpv_node drop_type = {0};
    if (mpv_get_property(mpv, "sub-auto-exts", MPV_FORMAT_NODE, &sub_exts) != MPV_ERROR_SUCCESS ||
        sub_exts.format != MPV_FORMAT_NODE_ARRAY)
        goto end;
    if (mpv_get_property(mpv, "drag-and-drop", MPV_FORMAT_NODE, &drop_type) != MPV_ERROR_SUCCESS ||
        drop_type.format != MPV_FORMAT_STRING)
        goto end;
    if (!strcmp(drop_type.u.string, "no"))
        goto end;
    if (strcmp(drop_type.u.string, "auto"))
        action = drop_type.u.string;

    struct mpv_node_list *list = files->u.list;
    for (int i = 0; i < list->num; i++) {
          mpv_node file = list->values[i];
          if (file.format != MPV_FORMAT_STRING)
              goto end;
    }

    bool all_sub = true;
    for (int i = 0; i < list->num; i++)
        all_sub &= might_be_subtitle_file(&sub_exts, list->values[i].u.string);

    if (all_sub) {
        for (int i = 0; i < list->num; i++) {
            const char *cmd[] = {
                "osd-auto",
                "sub-add",
                list->values[i].u.string,
                NULL
            };
            mpv_command(mpv, cmd);
        }
    } else if (!strcmp(action, "insert-next")) {
        /* To insert the entries in the correct order, we iterate over them
           backwards */
        for (int i = list->num - 1; i >= 0; i--) {
            const char *cmd[] = {
                "osd-auto",
                "loadfile",
                list->values[i].u.string,
                /* Since we're inserting in reverse, wait til the final item
                   is added to start playing */
                (i > 0) ? "insert-next" : "insert-next-play",
                NULL
            };
            mpv_command(mpv, cmd);
        }
    } else {
        for (int i = 0; i < list->num; i++) {
            const char *cmd[] = {
                "osd-auto",
                "loadfile",
                list->values[i].u.string,
                /* Either start playing the dropped files right away
                   or add them to the end of the current playlist */
                (i == 0 && !strcmp(action, "replace")) ? "replace" : "append-play",
                NULL
            };
            mpv_command(mpv, cmd);
        }
    }

end:
    mpv_free_node_contents(&sub_exts);
    mpv_free_node_contents(&drop_type);
}

static MP_THREAD_VOID mpv_event_loop_fn(void *arg)
{
    mp_thread_set_name("dnd");
    mpv_handle *mpv = arg;
    bool enabled = false;
    mpv_observe_property(mpv, 0, "dropped-files", MPV_FORMAT_NODE);
    mpv_observe_property(mpv, 0, "input-builtin-drag-and-drop", MPV_FORMAT_FLAG);

    while (1) {
        mpv_event *event = mpv_wait_event(mpv, -1);
        if (event->event_id == MPV_EVENT_SHUTDOWN)
            break;
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            mpv_event_property *prop = event->data;
            if (enabled && !strcmp(prop->name, "dropped-files") && prop->format == MPV_FORMAT_NODE) {
                mpv_node *node = prop->data;
                mpv_node *action = node_map_get(node, "action");
                if (!action || action->format != MPV_FORMAT_STRING)
                    continue;

                mpv_node *files = node_map_get(node, "files");
                if (!files || files->format != MPV_FORMAT_NODE_ARRAY)
                    continue;
                handle_dnd(mpv, files, action->u.string);
            }
            if (!strcmp(prop->name, "input-builtin-drag-and-drop") && prop->format == MPV_FORMAT_FLAG)
                enabled = *(int *)prop->data;
        }
    }

    mpv_destroy(mpv);
    MP_THREAD_RETURN();
}

void mp_dnd_init(mpv_handle *mpv)
{
    mp_thread mpv_event_loop;
    if (!mp_thread_create(&mpv_event_loop, mpv_event_loop_fn, mpv))
        mp_thread_detach(mpv_event_loop);
    else
        mpv_destroy(mpv);
}
