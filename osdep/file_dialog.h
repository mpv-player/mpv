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

#pragma once

#include <stdbool.h>

struct mp_log;

enum mp_file_dialog_flags {
    MP_FILE_DIALOG_FILE = 0,           // Open file selection
    MP_FILE_DIALOG_DIRECTORY = 1 << 0, // Open directory selection
    MP_FILE_DIALOG_SAVE = 1 << 1,      // Open file save dialog
    MP_FILE_DIALOG_MULTIPLE = 1 << 2,  // Allow multiple selection
};

typedef struct mp_file_dialog_filters {
    const char *name;   // Name of the filter.
    char **extensions;  // List of extensions to filter by, terminated by NULL.
} mp_file_dialog_filters;


typedef struct mp_file_dialog_params {
    struct mp_log *log;             // The log context to derive from.
    const char *title;              // The title of the file dialog. Optional.
    const char *initial_selection;  // The initial filename. Optional.
    const char *initial_dir;        // The default path to start the dialog at. Optional.
    enum mp_file_dialog_flags flags;        // Flags, see mp_file_dialog_flags
    const mp_file_dialog_filters *filters;  // The filters to apply to the file dialog dialog. Optional.
    void *parent;                   // The parent window. Optional.
} mp_file_dialog_params;

/**
 * @brief Get a file from the user using the system file dialog dialog.
 *
 * @param talloc_ctx  The talloc context.
 * @param params      The parameters to apply to the file dialog dialog.
 * @return char** Returned list of files or directories. Terminated by NULL.
 *         NULL if no files were selected or error occurred.
 */
char **mp_file_dialog_get_files(void *talloc_ctx, const mp_file_dialog_params *params);
