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

// Whether the created uploader can move the hw frame src to its hw format
// without going through system memory, by mapping it or by a direct libavutil
// transfer on the GPU. Support cannot be queried, so it is tested with src
// against the uploader's own device and pool parameters. Only valid for
// uploaders created with src_is_same_hw=false.
bool mp_hwupload_probe_hw_to_hw(struct mp_hwupload *u, struct mp_image *src);

// A filter which downloads sw frames from hw. Ignores sw frames.
struct mp_hwdownload {
    struct mp_filter *f;

    struct mp_image_pool *pool;
};

struct mp_hwdownload *mp_hwdownload_create(struct mp_filter *parent);
