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

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include "common.h"

int mpv_initialize_opts(mpv_handle *ctx, char **options);

#define MAX_INPUT_SIZE (1 << 20)
#define MAX_OPTS_NUM 10000

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char buff[MAX_INPUT_SIZE + 2];

    if (!size || size > MAX_INPUT_SIZE)
        return 0;

    memcpy(buff, data, size);
    buff[size] = '\0';
    buff[size + 1] = '\0';

    char *opts[MAX_OPTS_NUM + 1];
    char *opt = buff;
    int count = 0;
    while (*opt && count < MAX_OPTS_NUM) {
        opts[count] = opt;

        while (*opt && !isspace(*opt))
            opt++;

        *opt = '\0';
        opt++;

        while (*opt && isspace(*opt))
            opt++;

        count++;
    }
    opts[count] = NULL;

    mpv_handle *ctx = mpv_create();
    if (!ctx)
        exit(1);

    mpv_initialize_opts(ctx, opts);

    mpv_terminate_destroy(ctx);

    return 0;
}
