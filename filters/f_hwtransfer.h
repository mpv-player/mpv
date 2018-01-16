#pragma once

#include "filter.h"

// A filter which uploads sw frames to hw. Ignores hw frames.
struct mp_hwupload {
    struct mp_filter *f;

    // Hardware wrapper format, e.g. IMGFMT_VAAPI.
    int hw_imgfmt;

    // List of supported underlying surface formats.
    int *fmts;
    int num_fmts;
    // List of supported upload image formats. May contain duplicate entries
    // (which should be ignored).
    int *upload_fmts;
    int num_upload_fmts;
    // For fmts[n], fmt_upload_index[n] gives the index of the first supported
    // upload format in upload_fmts[], and fmt_upload_num[n] gives the number
    // of formats at this position.
    int *fmt_upload_index;
    int *fmt_upload_num;
};

struct mp_hwupload *mp_hwupload_create(struct mp_filter *parent, int hw_imgfmt);

// Return the best format suited for upload that is supported for a given input
// imgfmt. This returns the same as imgfmt if the format is natively supported,
// and otherwise a format that likely results in the least loss.
// Returns 0 if completely unsupported.
int mp_hwupload_find_upload_format(struct mp_hwupload *u, int imgfmt);
