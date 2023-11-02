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

#include "misc/linked_list.h"
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
    struct mp_present_entry *cur = present->head;
    while (cur) {
        if (cur->queue_display_time)
            break;
        cur = cur->list_node.next;
    }
    if (!cur)
        return;

    info->vsync_duration = cur->vsync_duration;
    info->skipped_vsyncs = cur->skipped_vsyncs;
    info->last_queue_display_time = cur->queue_display_time;

    // Remove from the list, zero out everything, and append at the end
    LL_REMOVE(list_node, present, cur);
    *cur = (struct mp_present_entry){0};
    LL_APPEND(list_node, present, cur);
}

struct mp_present *mp_present_initialize(void *talloc_ctx, int entries)
{
    struct mp_present *present = talloc_zero(talloc_ctx, struct mp_present);
    for (int i = 0; i < entries; i++) {
        struct mp_present_entry *entry = talloc_zero(present, struct mp_present_entry);
        LL_APPEND(list_node, present, entry);
    }
    return present;
}

void present_sync_swap(struct mp_present *present)
{
    struct mp_present_entry *cur = present->head;
    while (cur) {
        if (!cur->queue_display_time)
            break;
        cur = cur->list_node.next;
    }
    if (!cur)
        return;

    int64_t ust = cur->ust;
    int64_t msc = cur->msc;
    int64_t last_ust = cur->list_node.prev ? cur->list_node.prev->ust : 0;
    int64_t last_msc = cur->list_node.prev ? cur->list_node.prev->msc : 0;

    // Avoid attempting to use any presentation statistics if the ust is 0 or has
    // not actually updated (i.e. the last_ust is equal to ust).
    if (!ust || ust == last_ust) {
        cur->skipped_vsyncs = -1;
        cur->vsync_duration = -1;
        cur->queue_display_time = -1;
        return;
    }

    cur->skipped_vsyncs = 0;
    int64_t ust_passed = ust ? ust - last_ust: 0;
    int64_t msc_passed = msc ? msc - last_msc: 0;
    if (msc_passed && ust_passed)
        cur->vsync_duration = ust_passed / msc_passed;

    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return;

    int64_t now_monotonic = MP_TIME_S_TO_NS(ts.tv_sec) + ts.tv_nsec;
    int64_t ust_mp_time = mp_time_ns() - (now_monotonic - ust);
    cur->queue_display_time = ust_mp_time;
}

void present_sync_update_values(struct mp_present *present, int64_t ust,
                                int64_t msc)
{
    struct mp_present_entry *cur = present->head;
    while (cur) {
        if (!cur->ust)
            break;
        cur = cur->list_node.next;
    }
    if (!cur)
        return;

    cur->ust = ust;
    cur->msc = msc;
}
