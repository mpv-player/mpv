/*
 * posix-spawn replacement for Android
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

#pragma once

#include <sys/types.h>

#define MAX_FILE_ACTIONS 4

typedef struct {
    char dummy;
} posix_spawnattr_t; /* unsupported */

typedef struct {
    int used;
    struct {
        int filedes, newfiledes;
    } action[MAX_FILE_ACTIONS];
} posix_spawn_file_actions_t;

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t*, int, int);
int posix_spawn_file_actions_init(posix_spawn_file_actions_t*);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t*);

int posix_spawnp(pid_t*, const char*,
    const posix_spawn_file_actions_t*, const posix_spawnattr_t *,
    char *const [], char *const []);
