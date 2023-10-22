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

#include "mpv_talloc.h"
#include "osdep/timer.h"
#include "present_sync.h"

/* General nonsense about this mechanism.
 *
 * This requires that that caller has access to two, related values:
 * (ust, msc): clock time and incrementing counter of last vsync (this is
 *             increased continuously, even if we don't swap)
 *
 * Note that this concept originates from the GLX_OML_sync_control extension 
 * which includes another parameter: sbc (swap counter of frame that was
 * last displayed). Both the xorg present extension and wayland's
 * presentation-time protocol do not include sbc values so they are omitted
 * from this mechanism. mpv does not need to keep track of sbc calls and can
 * have reliable presentation without it.
 */

void present_sync_get_info(struct mp_present *present, struct vo_vsync_info *info)
{
    info->vsync_duration = present->vsync_duration;
    info->skipped_vsyncs = present->last_skipped_vsyncs;
    info->last_queue_display_time = present->last_queue_display_time;
}

void present_sync_swap(struct mp_present *present)
{
    int64_t ust = present->current_ust;
    int64_t msc = present->current_msc;

    // Avoid attempting to use any presentation statistics if the ust is 0 or has
    // not actually updated (i.e. the last_ust is equal to current_ust).
    if (!ust || ust == present->last_ust) {
        present->last_skipped_vsyncs = -1;
        present->vsync_duration = -1;
        present->last_queue_display_time = -1;
        return;
    }

    present->last_skipped_vsyncs = 0;

    int64_t ust_passed = ust ? ust - present->last_ust: 0;
    present->last_ust = ust;
    int64_t msc_passed = msc ? msc - present->last_msc: 0;
    present->last_msc = msc;

    if (msc_passed && ust_passed)
        present->vsync_duration = ust_passed / msc_passed;

    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return;

    int64_t now_monotonic = MP_TIME_S_TO_NS(ts.tv_sec) + ts.tv_nsec;
    int64_t ust_mp_time = mp_time_ns() - (now_monotonic - ust);

    present->last_queue_display_time = ust_mp_time;
}

void present_update_sync_values(struct mp_present *present, int64_t ust,
                                int64_t msc)
{
    present->current_ust = ust;
    present->current_msc = msc;
}
