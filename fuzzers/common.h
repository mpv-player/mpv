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

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mpv/client.h>

#define MAX_FUZZ_SIZE (100 << 10) // 100 KiB

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

static inline void set_fontconfig_sysroot(void)
{
#ifdef MPV_FONTCONFIG_SYSROOT
    const char *sysroot = MPV_STRINGIFY(MPV_FONTCONFIG_SYSROOT);
    char path[PATH_MAX];
    if (sysroot[0] != '/') {
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len < 0)
            exit(1);
        path[len] = '\0';
        char *name = strrchr(path, '/') + 1;
        snprintf(name, path + sizeof(path) - name, "%s", sysroot);
        sysroot = path;
    }
    setenv("FONTCONFIG_SYSROOT", sysroot, 1);
#endif
}

static inline bool wait_event(mpv_handle *ctx, mpv_event_id id)
{
    while (1) {
        mpv_event *event = mpv_wait_event(ctx, -1);
        if (event->event_id == id)
            return true;
        if (event->event_id == MPV_EVENT_SHUTDOWN)
            return false;
    }
}

// Waits for the core thread to process the current iteration and any pending requests.
static inline bool sync_core(mpv_handle *ctx)
{
    uint64_t sync_id = 1;
    mpv_observe_property(ctx, sync_id, "user-data/fuzzer-sync", MPV_FORMAT_NONE);
    bool ok;
    while (1) {
        mpv_event *event = mpv_wait_event(ctx, -1);
        ok = event->event_id != MPV_EVENT_SHUTDOWN;
        if (!ok || (event->event_id == MPV_EVENT_PROPERTY_CHANGE &&
                    event->reply_userdata == sync_id))
            break;
    }
    mpv_unobserve_property(ctx, sync_id);
    return ok;
}

static inline void initialize(mpv_handle *ctx)
{
    check_error(mpv_initialize(ctx));
    if (!wait_event(ctx, MPV_EVENT_IDLE) || !sync_core(ctx))
        exit(1);
}

static inline void destroy(mpv_handle *ctx)
{
    if (mpv_command(ctx, (const char *[]){"quit", NULL}) != MPV_ERROR_UNINITIALIZED)
        sync_core(ctx);
    mpv_terminate_destroy(ctx);
}

#ifndef PLAYBACK_TIME_LIMIT
#define PLAYBACK_TIME_LIMIT 1
#endif

#ifndef EVENTS_LIMIT
#define EVENTS_LIMIT 10
#endif

static inline void player_loop(mpv_handle *ctx)
{
    bool playing = false;
    bool loaded = false;
    int timeout = -1;
    int events_limit_after_restart = EVENTS_LIMIT;
    while (1) {
        mpv_event *event = mpv_wait_event(ctx, timeout);
        // If some events are spamming, like audio-reconfig preventing our
        // timeout to trigger stop after N events.
        if (timeout == PLAYBACK_TIME_LIMIT && events_limit_after_restart-- < 0)
            break;
        if (timeout == PLAYBACK_TIME_LIMIT && event->event_id == MPV_EVENT_NONE)
            break;
        if (event->event_id == MPV_EVENT_START_FILE)
            loaded = playing = true;
        if (event->event_id == MPV_EVENT_END_FILE) {
            playing = false;
            events_limit_after_restart = EVENTS_LIMIT;
            timeout = -1;
        }
        if (playing && event->event_id == MPV_EVENT_PLAYBACK_RESTART)
            timeout = PLAYBACK_TIME_LIMIT;
        if (loaded && event->event_id == MPV_EVENT_IDLE)
            break;
    }
}
