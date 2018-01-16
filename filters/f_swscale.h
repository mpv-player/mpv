#pragma once

#include <stdbool.h>

struct mp_sws_filter {
    struct mp_filter *f;
    // Desired output imgfmt. If 0, uses the input format.
    int out_format;
    // private state
    struct mp_sws_context *sws;
    struct mp_image_pool *pool;
};

// Create the filter. Free it with talloc_free(mp_sws_filter.f).
struct mp_sws_filter *mp_sws_filter_create(struct mp_filter *parent);

// Return the best format based on the input format and a list of allowed output
// formats. This tries to set the output format to the one that will result in
// the least loss. Returns a format from out_formats[], or 0 if no format could
// be chosen (or it's not supported by libswscale).
int mp_sws_find_best_out_format(int in_format, int *out_formats,
                                int num_out_formats);

// Whether ther given format is supported as input format.
bool mp_sws_supports_input(int imgfmt);
