/*
 * This file is part of mpv.
 *
 * This file can be distributed under the 3-clause license ("New BSD License").
 *
 * You can alternatively redistribute the non-Glumpy parts of this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef MPLAYER_FILTER_KERNELS_H
#define MPLAYER_FILTER_KERNELS_H

#include <stdbool.h>

enum scaler_filter {
    SCALER_INHERIT,
    SCALER_BILINEAR,
    SCALER_BICUBIC_FAST,
    SCALER_OVERSAMPLE,
    SCALER_LINEAR,
    SCALER_SPLINE16,
    SCALER_SPLINE36,
    SCALER_SPLINE64,
    SCALER_SINC,
    SCALER_LANCZOS,
    SCALER_GINSENG,
    SCALER_JINC,
    SCALER_EWA_LANCZOS,
    SCALER_EWA_HANNING,
    SCALER_EWA_GINSENG,
    SCALER_EWA_LANCZOSSHARP,
    SCALER_EWA_LANCZOS4SHARPEST,
    SCALER_EWA_LANCZOSSOFT,
    SCALER_HAASNSOFT,
    SCALER_BICUBIC,
    SCALER_HERMITE,
    SCALER_CATMULL_ROM,
    SCALER_MITCHELL,
    SCALER_ROBIDOUX,
    SCALER_ROBIDOUXSHARP,
    SCALER_EWA_ROBIDOUX,
    SCALER_EWA_ROBIDOUXSHARP,
    SCALER_BOX,
    SCALER_NEAREST,
    SCALER_TRIANGLE,
    SCALER_GAUSSIAN,
    WINDOW_PREFERRED,
    WINDOW_BOX,
    WINDOW_TRIANGLE,
    WINDOW_BARTLETT,
    WINDOW_COSINE,
    WINDOW_HANNING,
    WINDOW_TUKEY,
    WINDOW_HAMMING,
    WINDOW_QUADRIC,
    WINDOW_WELCH,
    WINDOW_KAISER,
    WINDOW_BLACKMAN,
    WINDOW_GAUSSIAN,
    WINDOW_SINC,
    WINDOW_JINC,
    WINDOW_SPHINX,
};

struct filter_window {
    enum scaler_filter function;
    double radius; // Preferred radius, should only be changed if resizable
    double (*weight)(struct filter_window *k, double x);
    bool resizable; // Filter supports any given radius
    double params[2]; // User-defined custom filter parameters. Not used by
                      // all filters
    double blur; // Blur coefficient (sharpens or widens the filter)
    double taper; // Taper coefficient (flattens the filter's center)
};

struct filter_kernel {
    struct filter_window f; // the kernel itself
    struct filter_window w; // window storage
    double clamp; // clamping factor, affects negative weights
    // Constant values
    enum scaler_filter window; // default window
    bool polar;         // whether or not the filter uses polar coordinates
    // The following values are set by mp_init_filter() at runtime.
    int size;           // number of coefficients (may depend on radius)
    double radius;        // true filter radius, derived from f.radius and f.blur
    double filter_scale;  // Factor to convert the mathematical filter
                          // function radius to the possibly wider
                          // (in the case of downsampling) filter sample
                          // radius.
    double radius_cutoff; // the radius at which we can cut off the filter
};

extern const struct filter_window mp_filter_windows[];
extern const struct filter_kernel mp_filter_kernels[];

const struct filter_window *mp_find_filter_window(enum scaler_filter function);
const struct filter_kernel *mp_find_filter_kernel(enum scaler_filter function);

bool mp_init_filter(struct filter_kernel *filter, const int *sizes,
                    double scale);
void mp_compute_lut(struct filter_kernel *filter, int count, int stride,
                    float *out_array);

#endif /* MPLAYER_FILTER_KERNELS_H */
