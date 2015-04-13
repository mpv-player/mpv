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

#include <windows.h>

#include "config.h"
#include "osdep/io.h"
#include "osdep/terminal.h"

#include "core.h"

int wmain(int argc, wchar_t *argv[]);

// mpv does its own wildcard expansion in the option parser
int _dowildcard = 0;

static bool is_valid_handle(HANDLE h)
{
    return h != INVALID_HANDLE_VALUE && h != NULL &&
           GetFileType(h) != FILE_TYPE_UNKNOWN;
}

static bool has_redirected_stdio(void)
{
    return is_valid_handle(GetStdHandle(STD_INPUT_HANDLE)) ||
           is_valid_handle(GetStdHandle(STD_OUTPUT_HANDLE)) ||
           is_valid_handle(GetStdHandle(STD_ERROR_HANDLE));
}

int wmain(int argc, wchar_t *argv[])
{
    // If started from the console wrapper (see osdep/win32-console-wrapper.c),
    // attach to the console and set up the standard IO handles
    bool has_console = terminal_try_attach();

    // If mpv is started from Explorer, the Run dialog or the Start Menu, it
    // will have no console and no standard IO handles. In this case, the user
    // is expecting mpv to show some UI, so enable the pseudo-GUI profile.
    bool gui = !has_console && !has_redirected_stdio();

    int argv_len = 0;
    char **argv_u8 = NULL;

    // Build mpv's UTF-8 argv, and add the pseudo-GUI profile if necessary
    if (argv[0])
        MP_TARRAY_APPEND(NULL, argv_u8, argv_len, mp_to_utf8(argv_u8, argv[0]));
    if (gui)
        MP_TARRAY_APPEND(NULL, argv_u8, argv_len, "--profile=pseudo-gui");
    for (int i = 1; i < argc; i++)
        MP_TARRAY_APPEND(NULL, argv_u8, argv_len, mp_to_utf8(argv_u8, argv[i]));
    MP_TARRAY_APPEND(NULL, argv_u8, argv_len, NULL);

    int ret = mpv_main(argv_len - 1, argv_u8);

    talloc_free(argv_u8);
    return ret;
}
