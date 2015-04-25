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

#ifndef MPLAYER_STREAM_DVDNAV_H
#define MPLAYER_STREAM_DVDNAV_H

#include <inttypes.h>
#include <stdbool.h>
#include "stream.h"

// Sent from stream to player.
// Note: order matters somewhat (stream_dvdnav sends them in numeric order)
enum mp_nav_event_type {
    MP_NAV_EVENT_NONE,
    MP_NAV_EVENT_MENU_MODE,     // menu mode on/off
    MP_NAV_EVENT_HIGHLIGHT,     // highlight changed
    MP_NAV_EVENT_RESET,         // reinitialize some things
    MP_NAV_EVENT_RESET_CLUT,    // reinitialize sub palette
    MP_NAV_EVENT_RESET_ALL,     // reinitialize all things
    MP_NAV_EVENT_DRAIN,         // reply with MP_NAV_CMD_DRAIN_OK
    MP_NAV_EVENT_STILL_FRAME,   // keep displaying current frame
    MP_NAV_EVENT_EOF,           // it's over
    MP_NAV_EVENT_OVERLAY,       // overlay changed
};

struct sub_bitmap;

struct mp_nav_event {
    enum mp_nav_event_type event;
    union {
        struct {
            int seconds; // -1: infinite
        } still_frame;
        struct {
            int display;
            int sx, sy, ex, ey;
            uint32_t palette;
        } highlight;
        struct {
            bool enable;
        } menu_mode;
        struct {
            struct sub_bitmap *images[2];
        } overlay;
    } u;
};

// Sent from player to stream.
enum mp_nav_cmd_type {
    MP_NAV_CMD_NONE,
    MP_NAV_CMD_ENABLE,        // enable interactive navigation
    MP_NAV_CMD_DRAIN_OK,      // acknowledge EVENT_DRAIN
    MP_NAV_CMD_RESUME,
    MP_NAV_CMD_SKIP_STILL,    // after showing the frame in EVENT_STILL_FRAME
    MP_NAV_CMD_MENU,
    MP_NAV_CMD_MOUSE_POS,
};

struct mp_nav_cmd {
    enum mp_nav_cmd_type event;
    union {
        struct {
            const char *action;
        } menu;
        struct {
            int x, y;
        } mouse_pos;
    } u;
    bool mouse_on_button;
};

#endif /* MPLAYER_STREAM_DVDNAV_H */
