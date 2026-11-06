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

#ifndef MP_PRESENT_SYNC_H
#define MP_PRESENT_SYNC_H

#include <stdbool.h>
#include <stdint.h>
#include "vo.h"

/* Presentation feedback for backends that learn when a swapped frame was
 * shown after the fact: the time it turned into light, the refresh count of
 * the display at that point and the swap it belongs to. */

struct mp_present {
    struct mp_vo_opts *opts;
    int64_t swaps;              // frames swapped so far
    int64_t presented;          // swap index of the last presented frame, 0 if none
    int64_t ust, msc;           // of the last presented frame
    int64_t vsync_duration;     // -1 until known
    bool pending;
    int64_t queue_display_time;
};

struct mp_present *mp_present_initialize(void *talloc_ctx, struct mp_vo_opts *opts);

// Used during the get_vsync call to deliver the presentation statistics to the VO.
void present_sync_get_info(struct mp_present *present, struct vo_vsync_info *info);

// Called right after every buffer swap, before waiting for the compositor, to
// update presentation statistics.
void present_sync_swap(struct mp_present *present);

// Called when the backend learns that a swapped frame was shown: ust is the
// time on CLOCK_MONOTONIC, msc the refresh count of the display at that
// point, 0 if the backend has none. refresh_ns is the refresh interval if the
// backend knows it, else 0. index is the swap the frame belongs to, counted
// from 1 (the next swap is swaps + 1).
void present_sync_update_values(struct mp_present *present, int64_t ust,
                                int64_t msc, int64_t refresh_ns, int64_t index);

#endif /* MP_PRESENT_SYNC_H */
