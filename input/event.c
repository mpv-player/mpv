/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "event.h"
#include "input.h"
#include "common/msg.h"
#include "sub/find_subfiles.h"

void mp_event_drop_files(struct input_ctx *ictx, int num_files, char **files)
{
    bool all_sub = true;
    for (int i = 0; i < num_files; i++)
        all_sub &= mp_might_be_subtitle_file(files[i]);

    if (all_sub) {
        for (int i = 0; i < num_files; i++) {
            const char *cmd[] = {
                "osd-auto",
                "sub_add",
                files[i],
                NULL
            };
            mp_input_run_cmd(ictx, cmd);
        }
    } else {
        for (int i = 0; i < num_files; i++) {
            const char *cmd[] = {
                "osd-auto",
                "loadfile",
                files[i],
                /* Start playing the dropped files right away */
                (i == 0) ? "replace" : "append",
                NULL
            };
            mp_input_run_cmd(ictx, cmd);
        }
    }
}

int mp_event_drop_mime_data(struct input_ctx *ictx, const char *mime_type,
                            bstr data)
{
    // X11 and Wayland file list format.
    if (strcmp(mime_type, "text/uri-list") == 0) {
        void *tmp = talloc_new(NULL);
        int num_files = 0;
        char **files = NULL;
        while (data.len) {
            bstr line = bstr_getline(data, &data);
            line = bstr_strip_linebreaks(line);
            if (bstr_startswith0(line, "#"))
                continue;
            char *s = bstrto0(tmp, line);
            MP_TARRAY_APPEND(tmp, files, num_files, s);
        }
        mp_event_drop_files(ictx, num_files, files);
        talloc_free(tmp);
        return num_files > 0;
    } else {
        return -1;
    }
}
