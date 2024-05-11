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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <libmpv/client.h>

#include "common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0)
        return -1;

    char filename[15 + 10 + 1];
    sprintf(filename, "/tmp/libfuzzer.%d", getpid());

    FILE *fp = fopen(filename, "wb");
    if (!fp)
        exit(1);

    if (fwrite(data, size, 1, fp) != 1)
        exit(1);

    if (fclose(fp))
        exit(1);

    mpv_handle *ctx = mpv_create();
    if (!ctx)
        exit(1);

    check_error(mpv_set_option_string(ctx, "vo", "null"));
    check_error(mpv_set_option_string(ctx, "ao", "null"));
    check_error(mpv_set_option_string(ctx, "ao-null-untimed", "yes"));
    check_error(mpv_set_option_string(ctx, "untimed", "yes"));
    check_error(mpv_set_option_string(ctx, "video-osd", "no"));
    check_error(mpv_set_option_string(ctx, "msg-level", "all=trace"));
    check_error(mpv_set_option_string(ctx, "network-timeout", "1"));

    check_error(mpv_initialize(ctx));

    const char *cmd[] = {"loadfile", filename, NULL};
    check_error(mpv_command(ctx, cmd));

    while (1) {
        mpv_event *event = mpv_wait_event(ctx, 10000);
        if (event->event_id == MPV_EVENT_IDLE)
            break;
    }

    mpv_terminate_destroy(ctx);

    return 0;
}
