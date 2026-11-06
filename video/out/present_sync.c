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

#include <time.h>

#include "common/common.h"
#include "mpv_talloc.h"
#include "osdep/timer.h"
#include "present_sync.h"

/* The backend reports when a swapped frame was shown, with the swap it
 * belongs to. vo.c expects last_queue_display_time to be the display time of
 * the frame just swapped, so the swap reports the time of the last presented
 * frame plus one vsync for every swap since it. Whether the feedback of the
 * swapped frame itself arrives before the swap returns depends on the
 * compositor, the swap count gives the same result either way.
 */

struct mp_present *mp_present_initialize(void *talloc_ctx, struct mp_vo_opts *opts)
{
    struct mp_present *present = talloc_zero(talloc_ctx, struct mp_present);
    present->opts = opts;
    present->vsync_duration = -1;
    return present;
}

void present_sync_get_info(struct mp_present *present, struct vo_vsync_info *info)
{
    if (!present->pending)
        return;
    present->pending = false;

    info->vsync_duration = present->vsync_duration;
    info->skipped_vsyncs = -1;
    info->last_queue_display_time = present->queue_display_time;
}

void present_sync_swap(struct mp_present *present)
{
    present->swaps++;
    present->pending = true;
    present->queue_display_time = -1;

    // Every swap since the last presented frame takes a vsync.
    int64_t behind = MPMAX(present->swaps - present->presented, 0);
    if (!present->presented || behind > present->opts->swapchain_depth + 1 ||
        (behind && present->vsync_duration <= 0))
        return;

    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return;

    int64_t now = mp_time_ns();
    int64_t now_monotonic = MP_TIME_S_TO_NS(ts.tv_sec) + ts.tv_nsec;
    int64_t ust_mp_time = now - (now_monotonic - present->ust);
    int64_t vd = present->vsync_duration;
    int64_t display = ust_mp_time + behind * vd;
    // A frame swapped more than a vsync after the last presented one cannot
    // be displayed before the first vsync after the swap.
    if (vd > 0 && now > ust_mp_time)
        display = MPMAX(display, ust_mp_time + ((now - ust_mp_time) / vd + 1) * vd);
    present->queue_display_time = display;
}

void present_sync_update_values(struct mp_present *present, int64_t ust,
                                int64_t msc, int64_t refresh_ns, int64_t index)
{
    if (present->presented && msc > present->msc && ust > present->ust) {
        present->vsync_duration = (ust - present->ust) / (msc - present->msc);
    } else if (refresh_ns > 0) {
        present->vsync_duration = refresh_ns;
    }

    present->presented = index;
    present->ust = ust;
    present->msc = msc;
}
