/*
 * Some of the filter code was taken from Glumpy:
 * # Copyright (c) 2009-2016 Nicolas P. Rougier. All rights reserved.
 * # Distributed under the (new) BSD License.
 * (https://github.com/glumpy/glumpy/blob/master/glumpy/library/build-spatial-filters.py)
 *
 * Also see:
 * - https://sourceforge.net/p/agg/svn/HEAD/tree/agg-2.4/include/agg_image_filters.h
 * - Vapoursynth plugin fmtconv (WTFPL Licensed), which is based on
 *   dither plugin for avisynth from the same author:
 *   https://gitlab.com/EleonoreMizo/fmtconv/-/tree/master/src/fmtc
 * - Paul Heckbert's "zoom"
 * - XBMC: ConvolutionKernels.cpp etc.
 *
 * This file is part of mpv.
 *
 * This file can be distributed under the 3-clause license ("New BSD License").
 *
 * You can alternatively redistribute the non-Glumpy parts of this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stddef.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "filter_kernels.h"
#include "common/common.h"

// NOTE: all filters are designed for discrete convolution

const struct filter_window *mp_find_filter_window(enum scaler_filter function)
{
    for (const struct filter_window *w = mp_filter_windows; w->function; w++) {
        if (w->function == function)
            return w;
    }
    return NULL;
}

const struct filter_kernel *mp_find_filter_kernel(enum scaler_filter function)
{
    for (const struct filter_kernel *k = mp_filter_kernels; k->f.function; k++) {
        if (k->f.function == function)
            return k;
    }
    return NULL;
}

// sizes = sorted list of available filter sizes, terminated with size 0
// inv_scale = source_size / dest_size
bool mp_init_filter(struct filter_kernel *filter, const int *sizes,
                    double inv_scale)
{
    mp_assert(filter->f.radius > 0);
    double blur = filter->f.blur > 0.0 ? filter->f.blur : 1.0;
    filter->radius = blur * filter->f.radius;

    // Only downscaling requires widening the filter
    filter->filter_scale = MPMAX(1.0, inv_scale);
    double src_radius = filter->radius * filter->filter_scale;
    // Polar filters are dependent solely on the radius
    if (filter->polar) {
        filter->size = 1; // Not meaningful for EWA/polar scalers.
        // Safety precaution to avoid generating a gigantic shader
        if (src_radius > 16.0) {
            src_radius = 16.0;
            filter->filter_scale = src_radius / filter->radius;
            return false;
        }
        return true;
    }
    int size = ceil(2.0 * src_radius);
    // round up to smallest available size that's still large enough
    if (size < sizes[0])
        size = sizes[0];
    const int *cursize = sizes;
    while (size > *cursize && *cursize)
        cursize++;
    if (*cursize) {
        filter->size = *cursize;
        return true;
    } else {
        // The filter doesn't fit - instead of failing completely, use the
        // largest filter available. This is incorrect, but better than refusing
        // to do anything.
        filter->size = cursize[-1];
        filter->filter_scale = (filter->size/2.0) / filter->radius;
        return false;
    }
}

// Sample from a blurred and tapered window
static double sample_window(struct filter_window *kernel, double x)
{
    if (!kernel->weight)
        return 1.0;

    // All windows are symmetric, this makes life easier
    x = fabs(x);

    // Stretch and taper the window size as needed
    x = kernel->blur > 0.0 ? x / kernel->blur : x;
    x = x <= kernel->taper ? 0.0 : (x - kernel->taper) / (1 - kernel->taper);

    if (x < kernel->radius)
        return kernel->weight(kernel, x);
    return 0.0;
}

// Evaluate a filter's kernel and window at a given absolute position
static double sample_filter(struct filter_kernel *filter, double x)
{
    // The window is always stretched to the entire kernel
    double w = sample_window(&filter->w, x / filter->radius * filter->w.radius);
    double k = w * sample_window(&filter->f, x);
    return k < 0 ? (1 - filter->clamp) * k : k;
}

// Calculate the 1D filtering kernel for N sample points.
// N = number of samples, which is filter->size
// The weights will be stored in out_w[0] to out_w[N - 1]
// f = x0 - abs(x0), subpixel position in the range [0,1) or [0,1].
static void mp_compute_weights(struct filter_kernel *filter, double f,
                               float *out_w)
{
    mp_assert(filter->size > 0);
    double sum = 0;
    for (int n = 0; n < filter->size; n++) {
        double x = f - (n - filter->size / 2 + 1);
        double w = sample_filter(filter, x / filter->filter_scale);
        out_w[n] = w;
        sum += w;
    }
    // Normalize to preserve energy
    for (int n = 0; n < filter->size; n++)
        out_w[n] /= sum;
}

// Fill the given array with weights for the range [0.0, 1.0]. The array is
// interpreted as rectangular array of count * filter->size items, with a
// stride of `stride` floats in between each array element. (For polar filters,
// the `count` indicates the row size and filter->size/stride are ignored)
//
// There will be slight sampling error if these weights are used in a OpenGL
// texture as LUT directly. The sampling point of a texel is located at its
// center, so out_array[0] will end up at 0.5 / count instead of 0.0.
// Correct lookup requires a linear coordinate mapping from [0.0, 1.0] to
// [0.5 / count, 1.0 - 0.5 / count].
void mp_compute_lut(struct filter_kernel *filter, int count, int stride,
                    float *out_array)
{
    if (filter->polar) {
        filter->radius_cutoff = 0.0;
        // Compute a 1D array indexed by radius
        for (int x = 0; x < count; x++) {
            double r = x * filter->radius / (count - 1);
            out_array[x] = sample_filter(filter, r);

            if (fabs(out_array[x]) > 1e-3f)
                filter->radius_cutoff = r;
        }
    } else {
        // Compute a 2D array indexed by subpixel position
        for (int n = 0; n < count; n++) {
            mp_compute_weights(filter, n / (double)(count - 1),
                               out_array + stride * n);
        }
    }
}

typedef struct filter_window params;

static double box(params *p, double x)
{
    // This is mathematically 1.0 everywhere, the clipping is done implicitly
    // based on the radius.
    return 1.0;
}

static double triangle(params *p, double x)
{
    return fmax(0.0, 1.0 - fabs(x / p->radius));
}

static double cosine(params *p, double x)
{
    return cos(x);
}

static double hanning(params *p, double x)
{
    return 0.5 + 0.5 * cos(M_PI * x);
}

static double hamming(params *p, double x)
{
    return 0.54 + 0.46 * cos(M_PI * x);
}

static double quadric(params *p, double x)
{
    if (x <  0.5) {
        return 0.75 - x * x;
    } else if (x <  1.5) {
        double t = x - 1.5;
        return 0.5 * t * t;
    }
    return 0.0;
}

static double bessel_i0(double x)
{
    double s = 1.0;
    double y = x * x / 4.0;
    double t = y;
    int i = 2;
    while (t > 1e-12) {
        s += t;
        t *= y / (i * i);
        i += 1;
    }
    return s;
}

static double kaiser(params *p, double x)
{
    if (x > 1)
        return 0;
    double i0a = 1.0 / bessel_i0(p->params[0]);
    return bessel_i0(p->params[0] * sqrt(1.0 - x * x)) * i0a;
}

static double blackman(params *p, double x)
{
    double a = p->params[0];
    double a0 = (1-a)/2.0, a1 = 1/2.0, a2 = a/2.0;
    double pix = M_PI * x;
    return a0 + a1*cos(pix) + a2*cos(2 * pix);
}

static double welch(params *p, double x)
{
    return 1.0 - x*x;
}

// Family of cubic B/C splines
static double cubic_bc(params *p, double x)
{
    double b = p->params[0],
           c = p->params[1];
    double p0 = (6.0 - 2.0 * b) / 6.0,
           p2 = (-18.0 + 12.0 * b + 6.0 * c) / 6.0,
           p3 = (12.0 - 9.0 * b - 6.0 * c) / 6.0,
           q0 = (8.0 * b + 24.0 * c) / 6.0,
           q1 = (-12.0 * b - 48.0 * c) / 6.0,
           q2 = (6.0 * b + 30.0 * c) / 6.0,
           q3 = (-b - 6.0 * c) / 6.0;

    if (x < 1.0) {
        return p0 + x * x * (p2 + x * p3);
    } else if (x < 2.0) {
        return q0 + x * (q1 + x * (q2 + x * q3));
    }
    return 0.0;
}

static double spline16(params *p, double x)
{
    if (x < 1.0) {
        return ((x - 9.0/5.0 ) * x - 1.0/5.0 ) * x + 1.0;
    } else {
        return ((-1.0/3.0 * (x-1) + 4.0/5.0) * (x-1) - 7.0/15.0 ) * (x-1);
    }
}

static double spline36(params *p, double x)
{
    if (x < 1.0) {
        return ((13.0/11.0 * x - 453.0/209.0) * x - 3.0/209.0) * x + 1.0;
    } else if (x < 2.0) {
        return ((-6.0/11.0 * (x-1) + 270.0/209.0) * (x-1) - 156.0/ 209.0) * (x-1);
    } else {
        return ((1.0/11.0 * (x-2) - 45.0/209.0) * (x-2) +  26.0/209.0) * (x-2);
    }
}

static double spline64(params *p, double x)
{
    if (x < 1.0) {
        return ((49.0/41.0 * x - 6387.0/2911.0) * x - 3.0/2911.0) * x + 1.0;
    } else if (x < 2.0) {
        return ((-24.0/41.0 * (x-1) + 4032.0/2911.0) * (x-1) - 2328.0/2911.0) * (x-1);
    } else if (x < 3.0) {
        return ((6.0/41.0 * (x-2) - 1008.0/2911.0) * (x-2) + 582.0/2911.0) * (x-2);
    } else {
        return ((-1.0/41.0 * (x-3) + 168.0/2911.0) * (x-3) - 97.0/2911.0) * (x-3);
    }
}

static double gaussian(params *p, double x)
{
    return exp(-2.0 * x * x / p->params[0]);
}

static double sinc(params *p, double x)
{
    if (fabs(x) < 1e-8)
        return 1.0;
    x *= M_PI;
    return sin(x) / x;
}

static double jinc(params *p, double x)
{
    if (fabs(x) < 1e-8)
        return 1.0;
    x *= M_PI;
    return 2.0 * j1(x) / x;
}

static double sphinx(params *p, double x)
{
    if (fabs(x) < 1e-8)
        return 1.0;
    x *= M_PI;
    return 3.0 * (sin(x) - x * cos(x)) / (x * x * x);
}

const struct filter_window mp_filter_windows[] = {
    {WINDOW_BOX,      1,   box},
    {WINDOW_TRIANGLE, 1,   triangle},
    {WINDOW_BARTLETT, 1,   triangle},
    {WINDOW_COSINE,   M_PI_2, cosine},
    {WINDOW_HANNING,  1,   hanning},
    {WINDOW_TUKEY,    1,   hanning, .taper = 0.5},
    {WINDOW_HAMMING,  1,   hamming},
    {WINDOW_QUADRIC,  1.5, quadric},
    {WINDOW_WELCH,    1,   welch},
    {WINDOW_KAISER,   1,   kaiser,   .params = {6.33, NAN} },
    {WINDOW_BLACKMAN, 1,   blackman, .params = {0.16, NAN} },
    {WINDOW_GAUSSIAN, 2,   gaussian, .params = {1.00, NAN} },
    {WINDOW_SINC,     1,   sinc},
    {WINDOW_JINC,     1.2196698912665045, jinc},
    {WINDOW_SPHINX,   1.4302966531242027, sphinx},
    {0}
};

#define JINC_R3 3.2383154841662362
#define JINC_R4 4.2410628637960699

const struct filter_kernel mp_filter_kernels[] = {
    // Spline filters
    {{SCALER_SPLINE16, 2, spline16}},
    {{SCALER_SPLINE36, 3, spline36}},
    {{SCALER_SPLINE64, 4, spline64}},
    // Sinc filters
    {{SCALER_SINC,    2,  sinc, .resizable = true}},
    {{SCALER_LANCZOS, 3,  sinc, .resizable = true}, .window = WINDOW_SINC},
    {{SCALER_GINSENG, 3,  sinc, .resizable = true}, .window = WINDOW_JINC},
    // Jinc filters
    {{SCALER_JINC,        JINC_R3, jinc, .resizable = true}, .polar = true},
    {{SCALER_EWA_LANCZOS, JINC_R3, jinc, .resizable = true}, .polar = true, .window = WINDOW_JINC},
    {{SCALER_EWA_HANNING, JINC_R3, jinc, .resizable = true}, .polar = true, .window = WINDOW_HANNING},
    // See <https://legacy.imagemagick.org/Usage/filter/nicolas/#upsampling>
    {{SCALER_EWA_GINSENG, JINC_R3, jinc, .resizable = true}, .polar = true, .window = WINDOW_SINC},
    // Slightly sharpened to minimize the 1D step response error (to better
    // preserve horizontal/vertical lines). Blur value determined by method
    // originally developed by Nicolas Robidoux for Image Magick, see:
    //   <https://www.imagemagick.org/discourse-server/viewtopic.php?p=89068#p89068>
    {{SCALER_EWA_LANCZOSSHARP, JINC_R3, jinc, .blur = 0.9812505837223707, .resizable = true},
        .polar = true, .window = WINDOW_JINC},
    // Similar to the above, but sharpened substantially to the point of
    // minimizing the total impulse response error on an integer grid. Tends
    // to preserve hash patterns well. Very sharp but rings a lot. See:
    //   <https://www.imagemagick.org/discourse-server/viewtopic.php?p=128587#p128587>
    {{SCALER_EWA_LANCZOS4SHARPEST, JINC_R4, jinc, .blur = 0.8845120932605005, .resizable = true},
        .polar = true, .window = WINDOW_JINC},
    // Similar to the above, but softened instead, to make even/odd integer
    // contributions exactly symmetrical. Designed to smooth out hash patterns.
    {{SCALER_EWA_LANCZOSSOFT, JINC_R3, jinc, .blur = 1.0164667662867047, .resizable = true},
        .polar = true, .window = WINDOW_JINC},
    // Very soft (blurred) hanning-windowed jinc; removes almost all aliasing.
    // Blur parameter picked to match orthogonal and diagonal contributions
    {{SCALER_HAASNSOFT, JINC_R3, jinc, .blur = 1.11, .resizable = true},
        .polar = true, .window = WINDOW_HANNING},
    // Cubic filters
    {{SCALER_BICUBIC,       2, cubic_bc, .params = {1.0, 0.0} }},
    {{SCALER_HERMITE,       1, cubic_bc, .params = {0.0, 0.0} }},
    {{SCALER_CATMULL_ROM,   2, cubic_bc, .params = {0.0, 0.5} }},
    {{SCALER_MITCHELL,      2, cubic_bc, .params = {1.0/3.0, 1.0/3.0} }},
    {{SCALER_ROBIDOUX,      2, cubic_bc,
      .params = {12 / (19 + 9 * M_SQRT2), 113 / (58 + 216 * M_SQRT2)} }},
    {{SCALER_ROBIDOUXSHARP, 2, cubic_bc,
      .params = {6 / (13 + 7 * M_SQRT2), 7 / (2 + 12 * M_SQRT2)} }},
    {{SCALER_EWA_ROBIDOUX,  2, cubic_bc,
      .params = {12 / (19 + 9 * M_SQRT2), 113 / (58 + 216 * M_SQRT2)}},
        .polar = true},
    {{SCALER_EWA_ROBIDOUXSHARP, 2,cubic_bc,
      .params = {6 / (13 + 7 * M_SQRT2), 7 / (2 + 12 * M_SQRT2)}},
        .polar = true},
    // Miscellaneous filters
    {{SCALER_BOX,      1,   box, .resizable = true}},
    {{SCALER_NEAREST,  0.5, box}},
    {{SCALER_TRIANGLE, 1,   triangle, .resizable = true}},
    {{SCALER_GAUSSIAN, 2,   gaussian, .params = {1.0, NAN}, .resizable = true}},
    {{0}}
};
