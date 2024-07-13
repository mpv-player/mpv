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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#define MPV_STRINGIFY_(X) #X
#define MPV_STRINGIFY(X) MPV_STRINGIFY_(X)

static inline void check_error(int status)
{
    if (status < 0) {
        fprintf(stderr, "mpv API error: %s\n", mpv_error_string(status));
        exit(1);
    }
}

static inline bool str_startswith(const char *str, size_t str_len,
                                  const char *prefix, size_t prefix_len)
{
    if (str_len < prefix_len)
        return false;
    return !memcmp(str, prefix, prefix_len);
}

#ifndef PLAYBACK_TIME_LIMIT
#define PLAYBACK_TIME_LIMIT 5
#endif

static inline void player_loop(mpv_handle *ctx)
{
    bool playing = false;
    bool loaded = false;
    int timeout = -1;
    while (1) {
        mpv_event *event = mpv_wait_event(ctx, timeout);
        if (timeout == PLAYBACK_TIME_LIMIT && event->event_id == MPV_EVENT_NONE)
            break;
        if (event->event_id == MPV_EVENT_START_FILE)
            loaded = playing = true;
        if (event->event_id == MPV_EVENT_END_FILE) {
            playing = false;
            timeout = -1;
        }
        if (playing && event->event_id == MPV_EVENT_PLAYBACK_RESTART)
            timeout = PLAYBACK_TIME_LIMIT;
        if (loaded && event->event_id == MPV_EVENT_IDLE)
            break;
    }
}
