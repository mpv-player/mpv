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

#include "libmpv_test_utils.h"

static void test_file_loading(mpv_handle *ctx, char *file)
{
    const char *cmd[] = {"loadfile", file, NULL};
    check_api_error(mpv_command(ctx, cmd));
    int loaded = 0;
    int finished = 0;
    while (!finished) {
        mpv_event *event = mpv_wait_event(ctx, 0);
        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED:
            // make sure it loads before exiting
            loaded = 1;
            break;
        case MPV_EVENT_END_FILE:
            if (loaded)
                finished = 1;
            break;
        }
    }
    if (!finished)
        fail(ctx, "Unable to load test file!\n");
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        return 1;

    mpv_handle *ctx = mpv_create();
    if (!ctx)
        return 1;

    // Use tct for all video-related stuff.
    check_api_error(mpv_set_property_string(ctx, "vo", "tct"));
    check_api_error(mpv_initialize(ctx));
    test_file_loading(ctx, argv[1]);

    mpv_destroy(ctx);
    return 0;
}
