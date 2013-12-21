/*
 * GyS-TermIO v2.0 (for GySmail v3)
 * a very small replacement of ncurses library
 *
 * copyright (C) 1999 A'rpi/ESP-team
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_GETCH2_H
#define MPLAYER_GETCH2_H

#include <stdbool.h>
#include <stdio.h>

struct input_ctx;

/* Screen size. Initialized by load_termcap() and get_screen_size() */
extern int screen_width;
extern int screen_height;

/* Termcap code to erase to end of line */
extern char * erase_to_end_of_line;

/* Global initialization for terminal output. */
int terminal_init(void);

/* Setup ictx to read input commands from stdin (slave mode) */
void terminal_setup_stdin_cmd_input(struct input_ctx *ictx);

/* Setup ictx to read keys from the terminal */
void terminal_setup_getch(struct input_ctx *ictx);

/* Return whether the process has been backgrounded. */
bool terminal_in_background(void);

/* Set ANSI text foreground color. c is [-1, 7], where 0-7 are colors, and
 * -1 means reset to default. stream is either stdout or stderr. */
void terminal_set_foreground_color(FILE *stream, int c);

/* Get screen-size using IOCTL call. */
void get_screen_size(void);

/* Initialize getch2 */
void getch2_enable(void);
void getch2_disable(void);

/* Enable and disable STDIN line-buffering */
void getch2_poll(void);

#endif /* MPLAYER_GETCH2_H */
