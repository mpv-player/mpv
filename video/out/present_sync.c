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

#include "mpv_talloc.h"
#include "present_sync.h"

struct mp_present *mp_present_initialize(void *talloc_ctx)
{
    struct mp_present *present = talloc_zero(talloc_ctx, struct mp_present);
    present->vsync_duration = -1;
    return present;
}

void present_sync_get_info(struct mp_present *present, struct vo_vsync_info *info)
{
    if (!present->pending)
        return;
    present->pending = false;

    info->vsync_duration = present->vsync_duration;
    // A presented frame further back than the largest swapchain holds frames
    // is not the last one displayed, the feedback of the later ones got lost.
    // swapchain-depth is no bound, the driver may make more images than that
    // and queues a frame in each.
    int64_t age = present->swaps - present->presented;
    if (!present->presented || age > VO_MAX_SWAPCHAIN_DEPTH + 1)
        return;
    info->displayed_time = present->time;
    info->displayed_age = age;
}

void present_sync_swap(struct mp_present *present)
{
    present->swaps++;
    present->pending = true;
}

void present_sync_update_values(struct mp_present *present, int64_t time, int64_t msc,
                                int64_t refresh_ns, int64_t swap)
{
    // A frame that was not swapped yet is not on the display. Such feedback
    // is of something else, on Wayland of a commit without a buffer swap.
    if (swap > present->swaps)
        return;

    if (present->presented && msc > present->msc && time > present->time) {
        present->vsync_duration = (time - present->time) / (msc - present->msc);
    } else if (refresh_ns > 0) {
        present->vsync_duration = refresh_ns;
    }

    present->presented = swap;
    present->time = time;
    present->msc = msc;
}
