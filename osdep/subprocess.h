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

#ifndef MP_SUBPROCESS_H_
#define MP_SUBPROCESS_H_

#include <stddef.h>

struct mp_cancel;

typedef void (*subprocess_read_cb)(void *ctx, char *data, size_t size);

// Start a subprocess. Uses callbacks to read from stdout and stderr.
int mp_subprocess(char **args, struct mp_cancel *cancel, void *ctx,
                  subprocess_read_cb on_stdout, subprocess_read_cb on_stderr,
                  char **error);

struct mp_log;
void mp_subprocess_detached(struct mp_log *log, char **args);

#endif
