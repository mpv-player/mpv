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

#include "file_dialog.h"

#include <stddef.h>

#include <config.h>

#include "file_dialog-portal.h"
#include "file_dialog-external.h"

typedef char **(*get_files_t)(void *talloc_ctx, const mp_file_dialog_params *params, bool *error);

static const get_files_t dialogs[] = {
#if HAVE_GIO
    mp_file_dialog_get_files_portal,
#endif
    mp_file_dialog_get_files_external,
    NULL,
};

char **mp_file_dialog_get_files(void *talloc_ctx, const mp_file_dialog_params *params)
{
    for (int i = 0; dialogs[i]; i++) {
        bool error = false;
        char **files = dialogs[i](talloc_ctx, params, &error);
        if (files)
            return files;
        if (!error)
            break;
    }
    return NULL;
}
