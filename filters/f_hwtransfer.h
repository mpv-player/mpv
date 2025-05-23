#pragma once

#include "filter.h"

// A filter which uploads sw frames to hw. Ignores hw frames.
struct mp_hwupload {
    // Indicates if the filter was successfully initialised, or not.
    // If not, the state of other members is undefined.
    bool successful_init;

    // The filter to use for uploads. NULL if none is required.
    struct mp_filter *f;

    // The underlying format of uploaded frames
    int selected_sw_imgfmt;
};

struct mp_hwupload mp_hwupload_create(struct mp_filter *parent, int hw_imgfmt,
                                       int sw_imgfmt, bool src_is_same_hw);

// A filter which downloads sw frames from hw. Ignores sw frames.
struct mp_hwdownload {
    struct mp_filter *f;

    struct mp_image_pool *pool;
};

struct mp_hwdownload *mp_hwdownload_create(struct mp_filter *parent);
