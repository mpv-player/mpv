#include <time.h>

#include "osdep/timer.h"
#include "oml_sync.h"
#include "video/out/vo.h"

// General nonsense about the associated extension.
//
// This extension returns two unrelated values:
//  (ust, msc): clock time and incrementing counter of last vsync (this is
//              reported continuously, even if we don't swap)
//  sbc:        swap counter of frame that was last displayed (every swap
//              increments the user_sbc, and the reported sbc is the sbc
//              of the frame that was just displayed)
// Invariants:
//  - ust and msc change in lockstep (no value can change without the other)
//  - msc is incremented; if you query it in a loop, and your thread isn't
//    frozen or starved by the scheduler, it will usually either not change, or
//    be incremented by 1 (while the ust will be incremented by vsync
//    duration)
//  - sbc is never higher than the user_sbc
//  - (ust, msc) are equal to or higher by vsync increments than the display
//    time of the frame referenced by the sbc
// Note that (ust, msc) and sbc are not locked to each other. The following
// can easily happen if vsync skips occur:
//  - you draw a frame, in the meantime hardware swaps sbc_1
//  - another display vsync happens during drawing
//  - you call swap()
//  - query (ust, msc) and sbc
//  - sbc contains sbc_1, but (ust, msc) contains the vsync after it
// As a consequence, it's hard to detect the latency or vsync skips.

static void oml_sync_reset(struct oml_sync *oml)
{
    oml->vsync_duration = -1;
    oml->last_skipped_vsyncs = -1;
    oml->last_queue_display_time = -1;
}

void oml_sync_swap(struct oml_sync *oml, int64_t ust, int64_t msc, int64_t sbc)
{
    if (!oml->state_ok)
        oml_sync_reset(oml);

    oml->last_skipped_vsyncs = 0;
    oml->user_sbc += 1;

    if (sbc < 0)
        return;

    oml->state_ok = true;

    int64_t ust_passed = oml->last_ust ? ust - oml->last_ust : 0;
    oml->last_ust = ust;

    int64_t msc_passed = oml->last_msc ? msc - oml->last_msc : 0;
    oml->last_msc = msc;

    int64_t sbc_passed = sbc - oml->last_sbc;
    oml->last_sbc = sbc;

    // Display frame duration. This makes assumptions about UST (see below).
    if (msc_passed && ust_passed)
        oml->vsync_duration = ust_passed / msc_passed;

    // Only if a new frame was displayed (sbc increased) we have sort-of a
    // chance that the current (ust, msc) is for the sbc. But this is racy,
    // because skipped frames drops could have increased the msc right after the
    // display event and before we queried the values. This code hopes for the
    // best and ignores this.
    if (sbc_passed) {
        // The GLX extension spec doesn't define what the UST is (not even its
        // unit). Simply assume UST is a simple CLOCK_MONOTONIC usec value. This
        // is what Mesa does, and what the Google EGL extension seems to imply
        // (they mention CLOCK_MONOTONIC, but not the unit).
        // The swap buffer call happened "some" but very small time ago, so we
        // can get away with querying the current time. There is also the
        // implicit assumption that mpv's timer and the UST use the same clock
        // (which it does on POSIX).
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts))
            return;
        uint64_t now_monotonic = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
        uint64_t ust_mp_time = mp_time_us() - (now_monotonic - ust);

        // Assume this is exactly when the actual display event for this sbc
        // happened. This is subject to the race mentioned above.
        oml->last_sbc_mp_time = ust_mp_time;
    }

    // At least one frame needs to be actually displayed before
    // oml->last_sbc_mp_time is set.
    if (!sbc)
        return;

    // Extrapolate from the last sbc time (when a frame was actually displayed),
    // and by adding the number of frames that were queued since to it.
    // For every unit the sbc is smaller than user_sbc, the actual display
    // is one frame ahead (assumes oml_sync_swap() is called for every
    // vsync).
    oml->last_queue_display_time =
        oml->last_sbc_mp_time + (oml->user_sbc - sbc) * oml->vsync_duration;
}

void oml_sync_get_info(struct oml_sync *oml, struct vo_vsync_info *info)
{
    if (!oml->state_ok)
        oml_sync_reset(oml);
    info->vsync_duration = oml->vsync_duration;
    info->skipped_vsyncs = oml->last_skipped_vsyncs;
    info->last_queue_display_time = oml->last_queue_display_time;
}
