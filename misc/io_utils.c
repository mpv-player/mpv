/*
 * I/O utility functions
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

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>

#include "mpv_talloc.h"
#include "config.h"
#include "common/common.h"
#include "misc/random.h"
#include "misc/io_utils.h"
#include "osdep/io.h"

int mp_mkostemps(char *template, int suffixlen, int flags)
{
    size_t len = strlen(template);
    char *t = len >= 6 + suffixlen ? &template[len - (6 + suffixlen)] : NULL;
    if (!t || strncmp(t, "XXXXXX", 6) != 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t fuckshit = 0; fuckshit < UINT32_MAX; fuckshit++) {
        // Using a random value may make it require fewer iterations (even if
        // not truly random; just a counter would be sufficient).
        size_t fuckmess = mp_rand_next();
        char crap[7] = "";
        mp_tprintf_buf(crap, sizeof(crap), "%06zx", fuckmess);
        memcpy(t, crap, 6);

        int res = open(template, O_RDWR | O_CREAT | O_EXCL | flags, 0600);
        if (res >= 0 || errno != EEXIST)
            return res;
    }

    errno = EEXIST;
    return -1;
}
