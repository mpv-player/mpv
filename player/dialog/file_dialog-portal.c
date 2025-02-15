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

#include <gio/gio.h>

#include <common/common.h>

#define PORTAL_BUS "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT "/org/freedesktop/portal/desktop"

struct priv {
    char **files;
    bool *error;
    GMainLoop *loop;
};

static void handle_response(GDBusConnection *connection, const gchar *sender_name,
                            const gchar *object_path, const gchar *interface_name,
                            const gchar *signal_name, GVariant *parameters,
                            gpointer user_data)
{
    struct priv *p = user_data;

    if (p->loop) {
        g_main_loop_quit(p->loop);
        g_main_loop_unref(p->loop);
    }

    g_autoptr(GVariant) response = NULL;
    g_variant_get(parameters, "(u@a{sv})", NULL, &response);

    *p->error = false;

    if (!response)
        return;

    g_autofree gchar **uris = NULL;
    if (g_variant_lookup(response, "uris", "^a&s", &uris) && uris) {
        p->files = NULL;
        size_t ret_count = 0;
        gchar **uri = uris;
        while (*uri) {
            char *item = talloc_strdup(NULL, *uri);
            MP_TARRAY_APPEND(NULL, p->files, ret_count, item);
            talloc_steal(p->files, item);
            uri++;
        }
        if (ret_count)
            MP_TARRAY_APPEND(NULL, p->files, ret_count, NULL);
    }
}

static void add_media_type(GVariantBuilder *filter_builder, const char *name,
                           char **extensions)
{
    if (!extensions)
        return;

    GVariantBuilder extensions_builder;
    g_variant_builder_init(&extensions_builder, G_VARIANT_TYPE("a(us)"));

    for (int j = 0; extensions[j]; j++) {
        g_variant_builder_add(&extensions_builder, "(us)", 0,
            mp_tprintf(42, extensions[j][0] == '*' ? "*" : "*.%s", extensions[j]));
    }

    g_variant_builder_add(filter_builder, "(sa(us))", name, &extensions_builder);
}

static GVariant *convert_filters_to_gvariant(const mp_file_dialog_filters *f)
{
    if (!f)
        return NULL;

    GVariantBuilder filter_builder;
    g_variant_builder_init(&filter_builder, G_VARIANT_TYPE("a(sa(us))"));

    add_media_type(&filter_builder, "All Files", (char *[]){"*", NULL});
    for (int i = 0; f[i].name; i++)
        add_media_type(&filter_builder, f[i].name, f[i].extensions);

    return g_variant_builder_end(&filter_builder);
}

static char **get_files(void *talloc_ctx, const mp_file_dialog_params *params,
                        bool *error)
{
    struct priv p = { .error = error };
    *p.error = true;

    g_autoptr(GDBusConnection) connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    if (!connection)
        return NULL;

    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);

    if (params->initial_selection && params->initial_selection[0])
        g_variant_builder_add(&options, "{sv}", "current_name", g_variant_new_string(params->initial_selection));

    if (params->initial_dir && params->initial_dir[0])
        g_variant_builder_add(&options, "{sv}", "current_folder", g_variant_new_bytestring(params->initial_dir));

    if (params->flags & MP_FILE_DIALOG_DIRECTORY)
        g_variant_builder_add(&options, "{sv}", "directory", g_variant_new_boolean(TRUE));

    if (params->flags & MP_FILE_DIALOG_MULTIPLE)
        g_variant_builder_add(&options, "{sv}", "multiple", g_variant_new_boolean(TRUE));

    GVariant *filters = convert_filters_to_gvariant(params->filters);
    if (filters)
        g_variant_builder_add(&options, "{sv}", "filters", filters);

    const char *title = params->title ? params->title : "";
    g_autoptr(GVariant) response = g_dbus_connection_call_sync(
        connection, PORTAL_BUS, PORTAL_OBJECT,
        "org.freedesktop.portal.FileChooser",
        params->flags & MP_FILE_DIALOG_SAVE ? "SaveFile" : "OpenFile",
        g_variant_new("(ssa{sv})", "", title, &options),
        G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);

    if (!response)
        return NULL;

    g_autofree gchar *obj = NULL;
    g_variant_get(response, "(o)", &obj);

    guint signal_id = g_dbus_connection_signal_subscribe(
        connection, PORTAL_BUS, "org.freedesktop.portal.Request", "Response",
        obj, NULL, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, handle_response, &p,
        NULL);

    p.loop = g_main_loop_new(NULL, FALSE);
    if (p.loop)
        g_main_loop_run(p.loop);

    g_dbus_connection_signal_unsubscribe(connection, signal_id);

    return talloc_steal(talloc_ctx, p.files);
}

const struct file_dialog_provider file_dialog_portal = {
    .name = "portal",
    .desc = "Desktop FileChooser portal",
    .get_files = get_files,
};
