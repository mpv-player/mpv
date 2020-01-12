#pragma once

#include "filter.h"

// A filter which uploads sw frames to hw. Ignores hw frames.
struct mp_hwupload {
    struct mp_filter *f;
};

struct mp_hwupload *mp_hwupload_create(struct mp_filter *parent, int hw_imgfmt);

// Return the best format suited for upload that is supported for a given input
// imgfmt. This returns the same as imgfmt if the format is natively supported,
// and otherwise a format that likely results in the least loss.
// Returns 0 if completely unsupported.
int mp_hwupload_find_upload_format(struct mp_hwupload *u, int imgfmt);

// A filter which downloads sw frames from hw. Ignores sw frames.
struct mp_hwdownload {
    struct mp_filter *f;

    struct mp_image_pool *pool;
};

struct mp_hwdownload *mp_hwdownload_create(struct mp_filter *parent);
