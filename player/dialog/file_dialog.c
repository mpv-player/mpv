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

#include <config.h>

#include <player/core.h>

extern const struct file_dialog_provider file_dialog_kdialog;
extern const struct file_dialog_provider file_dialog_mac;
extern const struct file_dialog_provider file_dialog_portal;
extern const struct file_dialog_provider file_dialog_win32;
extern const struct file_dialog_provider file_dialog_zenity;

static const struct file_dialog_provider *const dialogs[] = {
#if HAVE_WIN32_DESKTOP
    &file_dialog_win32,
#endif
#if HAVE_COCOA
    &file_dialog_mac,
#endif
#if HAVE_GIO
    &file_dialog_portal,
#endif
#if !HAVE_WIN32 && !HAVE_DARWIN
    &file_dialog_kdialog,
    &file_dialog_zenity,
#endif
};

static bool get_desc(struct m_obj_desc *dst, int index)
{
    if (index >= MP_ARRAY_SIZE(dialogs))
        return false;
    const struct file_dialog_provider *provider = dialogs[index];
    *dst = (struct m_obj_desc) {
        .name = provider->name,
        .description = provider->desc,
    };
    return true;
}

struct file_dialog_opts {
    struct m_obj_settings *providers;
};

static const struct m_obj_list provider_obj_list = {
    .get_desc = get_desc,
    .description = "file dialog providers",
    .allow_trailer = true,
    .disallow_positional_parameters = true,
    .use_global_options = true,
};

#define OPT_BASE_STRUCT struct file_dialog_opts
const struct m_sub_options file_dialog_conf = {
    .opts = (const struct m_option[]) {
        {"providers", OPT_SETTINGSLIST(providers, &provider_obj_list)},
        {0}
    },
    .defaults = &(const struct file_dialog_opts) {
        .providers = (struct m_obj_settings[]) {
            {.name = "win32", .enabled = true},
            {.name = "mac", .enabled = true},
            {.name = "portal", .enabled = true},
            {.name = "kdialog", .enabled = true},
            {.name = "zenity", .enabled = true},
            {0}
        }
    },
    .size = sizeof(struct file_dialog_opts)
};

char **mp_file_dialog_get_files(void *talloc_ctx, const mp_file_dialog_params *params)
{
    struct m_obj_settings *providers = params->opts->providers;
    for (int i = 0; providers && providers[i].name; ++i) {
        if (!providers[i].enabled)
            continue;
        for (int j = 0; j < MP_ARRAY_SIZE(dialogs); ++j) {
            const struct file_dialog_provider *dialog = dialogs[j];
            if (strcmp(providers[i].name, dialog->name))
                continue;
            bool error = false;
            char **files = dialog->get_files(talloc_ctx, params, &error);
            if (files)
                return files;
            if (!error)
                return NULL;
            break;
        }
    }
    return NULL;
}
