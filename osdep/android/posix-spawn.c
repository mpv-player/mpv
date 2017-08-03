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

#include <unistd.h>
#include <errno.h>
#include "osdep/android/posix-spawn.h"

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa, int fd, int newfd)
{
    if (fa->used >= MAX_FILE_ACTIONS)
        return -1;
    fa->action[fa->used].filedes = fd;
    fa->action[fa->used].newfiledes = newfd;
    fa->used++;
    return 0;
}

int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fa)
{
    fa->used = 0;
    return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa)
{
    return 0;
}

int posix_spawnp(pid_t *pid, const char *file,
    const posix_spawn_file_actions_t *fa,
    const posix_spawnattr_t *attrp,
    char *const argv[], char *const envp[])
{
    pid_t p;

    if (attrp != NULL)
        return EINVAL;

    p = fork();
    if (p == -1)
        return errno;

    if (p == 0) {
        for (int i = 0; i < fa->used; i++) {
            int err = dup2(fa->action[i].filedes, fa->action[i].newfiledes);
            if (err == -1)
                goto fail;
        }
        execvpe(file, argv, envp);
fail:
        _exit(127);
    }

    *pid = p;
    return 0;
}
