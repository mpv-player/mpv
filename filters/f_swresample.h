#pragma once

#include <stdbool.h>

#include "audio/chmap.h"
#include "filter.h"

// Resampler filter, wrapping libswresample or libavresample.
struct mp_swresample {
    struct mp_filter *f;
    // Desired output parameters. For unset parameters, passes through the
    // format.
    int out_rate;
    int out_format;
    struct mp_chmap out_channels;
    double speed;
};

struct mp_resample_opts {
    int filter_size;
    int phase_shift;
    int linear;
    double cutoff;
    int normalize;
    int allow_passthrough;
    double max_output_frame_size;
    char **avopts;
};

#define MP_RESAMPLE_OPTS_DEF {  \
    .filter_size = 16,          \
    .cutoff      = 0.0,         \
    .phase_shift = 10,          \
    .normalize   = 0,           \
    .max_output_frame_size = 40,\
    }

// Create the filter. If opts==NULL, use the global options as defaults.
// Free with talloc_free(mp_swresample.f).
struct mp_swresample *mp_swresample_create(struct mp_filter *parent,
                                           struct mp_resample_opts *opts);

// Internal resampler delay. Does not include data buffered in mp_pins and such.
double mp_swresample_get_delay(struct mp_swresample *s);
