/*
 * Based on GyS-TermIO v2.0 (for GySmail v3) (copyright (C) 1999 A'rpi/ESP-team)
 *
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

#ifndef MPLAYER_GETCH2_H
#define MPLAYER_GETCH2_H

#include <stdarg.h>
#include <stdbool.h>

#include "misc/bstr.h"

#define TERM_ESC_GOTO_YX            "\033[%d;%df"
#define TERM_ESC_HIDE_CURSOR        "\033[?25l"
#define TERM_ESC_RESTORE_CURSOR     "\033[?25h"
#define TERM_ESC_SYNC_UPDATE_BEGIN  "\033[?2026h"
#define TERM_ESC_SYNC_UPDATE_END    "\033[?2026l"

#define TERM_ESC_CLEAR_SCREEN       "\033[2J"
#define TERM_ESC_ALT_SCREEN         "\033[?1049h"
#define TERM_ESC_NORMAL_SCREEN      "\033[?1049l"

struct input_ctx;

/* Global initialization for terminal output. */
void terminal_init(void);

/* Setup ictx to read keys from the terminal */
void terminal_setup_getch(struct input_ctx *ictx);

/* Undo terminal_init(), and also terminal_setup_getch() */
void terminal_uninit(void);

/* Return whether the process has been backgrounded. */
bool terminal_in_background(void);

/* Get terminal-size in columns/rows. */
void terminal_get_size(int *w, int *h);

/* Get terminal-size in columns/rows and width/height in pixels. */
void terminal_get_size2(int *rows, int *cols, int *px_width, int *px_height);

// Windows only.
int mp_console_vfprintf(void *wstream, const char *format, va_list args);
int mp_console_write(void *wstream, bstr str);
bool mp_check_console(void *handle);

/* Windows-only function to attach to the parent process's console */
bool terminal_try_attach(void);

#endif /* MPLAYER_GETCH2_H */
