#pragma once

#include <stdbool.h>
#include <stdint.h>

// Must be initialized to {0} by user.
struct oml_sync {
    bool state_ok;
    int64_t last_ust;
    int64_t last_msc;
    int64_t last_sbc;
    int64_t last_sbc_mp_time;
    int64_t user_sbc;
    int64_t vsync_duration;
    int64_t last_skipped_vsyncs;
    int64_t last_queue_display_time;
};

struct vo_vsync_info;

// This must be called on every SwapBuffer call. Pass the ust/msc/sbc values
// returned by a successful GetSyncValues call. Pass -1 for all these 3 values
// if GetSyncValues returned failure (but note that you need to set them to -1
// manually).
void oml_sync_swap(struct oml_sync *oml, int64_t ust, int64_t msc, int64_t sbc);

// Can be called any time; returns state determined by last oml_sync_swap() call.
void oml_sync_get_info(struct oml_sync *oml, struct vo_vsync_info *info);
