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

#include "file_dialog_detail.h"

#include <systemd/sd-bus.h>

#include <common/common.h>

#define PORTAL_BUS "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT "/org/freedesktop/portal/desktop"

struct priv {
    char **files;
    bool *error;
    sd_event *event;
};

#define CHECK(r) do { if (r < 0) goto error; } while (0)

static int handle_response(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct priv *p = userdata;
    *p->error = false;

    if (ret_error) {
        sd_bus_error_free(ret_error);
        goto error;
    }

    uint32_t response;
    CHECK(sd_bus_message_enter_container(m, SD_BUS_TYPE_STRUCT, "ua{sv}"));
    CHECK(sd_bus_message_read_basic(m, SD_BUS_TYPE_UINT32, &response));

    if (response != 0)
        goto error;

    CHECK(sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}"));
    while (sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv") == 1) {
        const char *str;
        CHECK(sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &str));
        if (strcmp(str, "uris")) {
            CHECK(sd_bus_message_skip(m, "v"));
            CHECK(sd_bus_message_exit_container(m));
            continue;
        }

        sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, "as");
        sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");

        p->files = NULL;
        size_t ret_count = 0;
        while (sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &str) > 0) {
            char *item = talloc_strdup(NULL, str);
            MP_TARRAY_APPEND(NULL, p->files, ret_count, item);
            talloc_steal(p->files, item);
        }
        if (ret_count)
            MP_TARRAY_APPEND(NULL, p->files, ret_count, NULL);

        CHECK(sd_bus_message_exit_container(m));
        CHECK(sd_bus_message_exit_container(m));

        CHECK(sd_bus_message_exit_container(m));
    }
    CHECK(sd_bus_message_exit_container(m));
    CHECK(sd_bus_message_exit_container(m));

error:
    sd_event_exit(p->event, 0);
    return 0;
}

static int add_media_type(sd_bus_message *m, const char *name, char **extensions)
{
    if (!extensions)
        return 0;

    CHECK(sd_bus_message_open_container(m, SD_BUS_TYPE_STRUCT, "sa(us)"));

    CHECK(sd_bus_message_append_basic(m, SD_BUS_TYPE_STRING, name));

    CHECK(sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, "(us)"));
    for (int j = 0; extensions[j]; j++) {
        CHECK(sd_bus_message_open_container(m, SD_BUS_TYPE_STRUCT, "us"));
        CHECK(sd_bus_message_append_basic(m, SD_BUS_TYPE_UINT32, &(int){0}));
        CHECK(sd_bus_message_append_basic(m, SD_BUS_TYPE_STRING,
            mp_tprintf(42, extensions[j][0] == '*' ? "*" : "*.%s", extensions[j])));
        CHECK(sd_bus_message_close_container(m));
    }
    CHECK(sd_bus_message_close_container(m));

    CHECK(sd_bus_message_close_container(m));

    return 0;

error:
    return -1;
}

static int message_append_filters(sd_bus_message *m, const mp_file_dialog_filters *f)
{
    if (!f)
        return 0;

    CHECK(sd_bus_message_open_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv"));

    CHECK(sd_bus_message_append_basic(m, SD_BUS_TYPE_STRING, "filters"));

    CHECK(sd_bus_message_open_container(m, SD_BUS_TYPE_VARIANT, "a(sa(us))"));
    CHECK(sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, "(sa(us))"));

    // Add "All Files" filter
    CHECK(add_media_type(m, "All Files", (char *[]){"*", NULL}));

    for (int i = 0; f[i].name; i++)
        CHECK(add_media_type(m, f[i].name, f[i].extensions));

    CHECK(sd_bus_message_close_container(m));
    CHECK(sd_bus_message_close_container(m));
    CHECK(sd_bus_message_close_container(m));

    return 0;

error:
    return -1;
}

static char **get_files(void *talloc_ctx, const mp_file_dialog_params *params, bool *error) {
    struct priv p = { .error = error };
    *p.error = true;

    sd_bus *bus = NULL;
    sd_bus_message *m = NULL;
    sd_bus_message *reply = NULL;
    sd_bus_slot *slot = NULL;

    CHECK(sd_bus_default(&bus));
    CHECK(sd_bus_message_new_method_call(bus, &m, PORTAL_BUS, PORTAL_OBJECT,
                                        "org.freedesktop.portal.FileChooser",
                                        params->flags & MP_FILE_DIALOG_SAVE ? "SaveFile" : "OpenFile"));

    CHECK(sd_bus_message_append(m, "ss", "", params->title ? params->title : ""));
    CHECK(sd_bus_message_open_container(m, SD_BUS_TYPE_ARRAY, "{sv}"));

    if (params->initial_selection && params->initial_selection[0])
        CHECK(sd_bus_message_append(m, "{sv}", "current_name", "s", params->initial_selection));

    if (params->initial_dir && params->initial_dir[0]) {
        CHECK(sd_bus_message_open_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv"));
        CHECK(sd_bus_message_append_basic(m, SD_BUS_TYPE_STRING, "current_folder"));
        CHECK(sd_bus_message_open_container(m, SD_BUS_TYPE_VARIANT, "ay"));
        CHECK(sd_bus_message_append_array(m, SD_BUS_TYPE_BYTE,
                                          params->initial_dir,
                                          strlen(params->initial_dir) + 1));
        CHECK(sd_bus_message_close_container(m));
        CHECK(sd_bus_message_close_container(m));
    }

    if (params->flags & MP_FILE_DIALOG_DIRECTORY)
        CHECK(sd_bus_message_append(m, "{sv}", "directory", "b", true));

    if (params->flags & MP_FILE_DIALOG_MULTIPLE)
        CHECK(sd_bus_message_append(m, "{sv}", "multiple", "b", true));

    CHECK(message_append_filters(m, params->filters));

    CHECK(sd_bus_message_close_container(m));

    CHECK(sd_bus_call(bus, m, 0, NULL, &reply));

    const char *obj;
    CHECK(sd_bus_message_read_basic(reply, SD_BUS_TYPE_OBJECT_PATH, &obj));
    CHECK(sd_bus_match_signal_async(bus, &slot, PORTAL_BUS, obj,
                                    "org.freedesktop.portal.Request", "Response",
                                    &handle_response, NULL, &p));

    CHECK(sd_event_default(&p.event));
    CHECK(sd_bus_attach_event(bus, p.event, SD_EVENT_PRIORITY_NORMAL));
    CHECK(sd_event_loop(p.event));

error:
    sd_bus_slot_unref(slot);
    sd_bus_message_unref(reply);
    sd_bus_message_unref(m);
    sd_bus_unref(bus);

    return talloc_steal(talloc_ctx, p.files);
}

const struct file_dialog_provider file_dialog_portal = {
    .name = "portal",
    .desc = "Desktop FileChooser portal (sd-bus)",
    .get_files = get_files,
};
