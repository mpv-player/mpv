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

#include <stdbool.h>
#include <stdio.h>

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

// Windows only.
void mp_write_console_ansi(void *wstream, char *buf);

/* Windows-only function to attach to the parent process's console */
bool terminal_try_attach(void);

#endif /* MPLAYER_GETCH2_H */
