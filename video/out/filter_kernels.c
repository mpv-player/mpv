/*
 * Most code for computing the weights is taken from Anti-Grain Geometry (AGG)
 * (licensed under GPL 2 or later), with modifications.
 *
 * Copyright (C) 2002-2006 Maxim Shemanarev
 *
 * http://vector-agg.cvs.sourceforge.net/viewvc/vector-agg/agg-2.5/include/agg_image_filters.h?view=markup
 *
 * Also see:
 * - glumpy (BSD licensed), contains the same code in Python:
 *   http://code.google.com/p/glumpy/source/browse/glumpy/image/filter.py
 * - Vapoursynth plugin fmtconv (WTFPL Licensed), which is based on
 *   dither plugin for avisynth from the same author:
 *   https://github.com/vapoursynth/fmtconv/tree/master/src/fmtc
 * - Paul Heckbert's "zoom"
 * - XBMC: ConvolutionKernels.cpp etc.
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "filter_kernels.h"

// NOTE: all filters are designed for discrete convolution

const struct filter_window *mp_find_filter_window(const char *name)
{
    if (!name)
        return NULL;
    for (const struct filter_window *w = mp_filter_windows; w->name; w++) {
        if (strcmp(w->name, name) == 0)
            return w;
    }
    return NULL;
}

const struct filter_kernel *mp_find_filter_kernel(const char *name)
{
    if (!name)
        return NULL;
    for (const struct filter_kernel *k = mp_filter_kernels; k->f.name; k++) {
        if (strcmp(k->f.name, name) == 0)
            return k;
    }
    return NULL;
}

// sizes = sorted list of available filter sizes, terminated with size 0
// inv_scale = source_size / dest_size
bool mp_init_filter(struct filter_kernel *filter, const int *sizes,
                    double inv_scale)
{
    assert(filter->f.radius > 0);
    // Only downscaling requires widening the filter
    filter->inv_scale = inv_scale >= 1.0 ? inv_scale : 1.0;
    filter->f.radius *= filter->inv_scale;
    // Polar filters are dependent solely on the radius
    if (filter->polar) {
        filter->f.radius = fmin(filter->f.radius, 16.0);
        filter->size = 1;
        // Safety precaution to avoid generating a gigantic shader
        if (filter->f.radius > 16.0) {
            filter->f.radius = 16.0;
            return false;
        }
        return true;
    }
    int size = ceil(2.0 * filter->f.radius);
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
        filter->inv_scale *= (filter->size/2.0) / filter->f.radius;
        return false;
    }
}

// Sample from the blurred, windowed kernel. Note: The window is always
// stretched to the true radius, regardless of the filter blur/scale.
static double sample_filter(struct filter_kernel *filter,
                            struct filter_window *window, double x)
{
    double bk = filter->f.blur > 0.0 ? filter->f.blur : 1.0;
    double bw = window->blur > 0.0 ? window->blur : 1.0;
    double c = fabs(x) / (filter->inv_scale * bk);
    double w = window->weight ? window->weight(window, x/bw * window->radius
                                                            / filter->f.radius)
                              : 1.0;
    return c < filter->f.radius ? w * filter->f.weight(&filter->f, c) : 0.0;
}

// Calculate the 1D filtering kernel for N sample points.
// N = number of samples, which is filter->size
// The weights will be stored in out_w[0] to out_w[N - 1]
// f = x0 - abs(x0), subpixel position in the range [0,1) or [0,1].
static void mp_compute_weights(struct filter_kernel *filter,
                               struct filter_window *window,
                               double f, float *out_w)
{
    assert(filter->size > 0);
    double sum = 0;
    for (int n = 0; n < filter->size; n++) {
        double x = f - (n - filter->size / 2 + 1);
        double w = sample_filter(filter, window, x);
        out_w[n] = w;
        sum += w;
    }
    // Normalize to preserve energy
    for (int n = 0; n < filter->size; n++)
        out_w[n] /= sum;
}

// Fill the given array with weights for the range [0.0, 1.0]. The array is
// interpreted as rectangular array of count * filter->size items.
void mp_compute_lut(struct filter_kernel *filter, int count, float *out_array)
{
    struct filter_window *window = &filter->w;
    if (filter->polar) {
        // Compute a 1D array indexed by radius
        for (int x = 0; x < count; x++) {
            double r = x * filter->f.radius / (count - 1);
            out_array[x] = sample_filter(filter, window, r);
        }
    } else {
        // Compute a 2D array indexed by subpixel position
        for (int n = 0; n < count; n++) {
            mp_compute_weights(filter, window,  n / (double)(count - 1),
                               out_array + filter->size * n);
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
    // NOTE: glumpy uses 0.75, AGG uses 0.5
    if (x < 0.5)
        return 0.75 - x * x;
    if (x < 1.5)
        return 0.5 * (x - 1.5) * (x - 1.5);
    return 0;
}

static double bc_pow3(double x)
{
    return (x <= 0) ? 0 : x * x * x;
}

static double bicubic(params *p, double x)
{
    return (1.0/6.0) * (      bc_pow3(x + 2)
                        - 4 * bc_pow3(x + 1)
                        + 6 * bc_pow3(x)
                        - 4 * bc_pow3(x - 1));
}

static double bessel_i0(double epsilon, double x)
{
    double sum = 1;
    double y = x * x / 4;
    double t = y;
    for (int i = 2; t > epsilon; i++) {
        sum += t;
        t *= y / (i * i);
    }
    return sum;
}

static double kaiser(params *p, double x)
{
    double a = p->params[0];
    double epsilon = 1e-12;
    double i0a = 1 / bessel_i0(epsilon, a);
    return bessel_i0(epsilon, a * sqrt(1 - x * x)) * i0a;
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
    double b = p->params[0];
    double c = p->params[1];
    double
        p0 = (6.0 - 2.0 * b) / 6.0,
        p2 = (-18.0 + 12.0 * b + 6.0 * c) / 6.0,
        p3 = (12.0 - 9.0 * b - 6.0 * c) / 6.0,
        q0 = (8.0 * b + 24.0 * c) / 6.0,
        q1 = (-12.0 * b - 48.0 * c) / 6.0,
        q2 = (6.0 * b + 30.0 * c) / 6.0,
        q3 = (-b - 6.0 * c) / 6.0;
    if (x < 1.0)
        return p0 + x * x * (p2 + x * p3);
    if (x < 2.0)
        return q0 + x * (q1 + x * (q2 + x * q3));
    return 0;
}

static double spline16(params *p, double x)
{
    if (x < 1.0)
        return ((x - 9.0/5.0 ) * x - 1.0/5.0 ) * x + 1.0;
    return ((-1.0/3.0 * (x-1) + 4.0/5.0) * (x-1) - 7.0/15.0 ) * (x-1);
}

static double spline36(params *p, double x)
{
    if(x < 1.0)
        return ((13.0/11.0 * x - 453.0/209.0) * x - 3.0/209.0) * x + 1.0;
    if(x < 2.0)
        return ((-6.0/11.0 * (x - 1) + 270.0/209.0) * (x - 1) - 156.0/209.0)
               * (x - 1);
    return ((1.0/11.0 * (x - 2) - 45.0/209.0) * (x - 2) +  26.0/209.0)
           * (x - 2);
}

static double spline64(params *p, double x)
{
    if (x < 1.0)
        return ((49.0 / 41.0 * x - 6387.0 / 2911.0) * x - 3.0 / 2911.0) * x + 1.0;
    if (x < 2.0)
        return ((-24.0 / 41.0 * (x - 1) + 4032.0 / 2911.0) * (x - 1) - 2328.0 / 2911.0)
               * (x - 1);
    if (x < 3.0)
        return ((6.0 / 41.0 * (x - 2) - 1008.0 / 2911.0) * (x - 2) + 582.0 / 2911.0)
               * (x - 2);
    return ((-1.0 / 41.0 * (x - 3) + 168.0 / 2911.0) * (x - 3) - 97.0 / 2911.0)
           * (x - 3);
}

static double gaussian(params *p, double x)
{
    return pow(2.0, -(M_E / p->params[0]) * x * x);
}

static double sinc(params *p, double x)
{
    if (fabs(x) < 1e-8)
        return 1.0;
    double pix = M_PI * x;
    return sin(pix) / pix;
}

static double jinc(params *p, double x)
{
    if (fabs(x) < 1e-8)
        return 1.0;
    double pix = M_PI * x;
    return 2.0 * j1(pix) / pix;
}

static double sphinx(params *p, double x)
{
    if (fabs(x) < 1e-8)
        return 1.0;
    double pix = M_PI * x;
    return 3.0 * (sin(pix) - pix * cos(pix)) / (pix * pix * pix);
}

const struct filter_window mp_filter_windows[] = {
    {"box",            1,   box},
    {"triangle",       1,   triangle},
    {"bartlett",       1,   triangle},
    {"hanning",        1,   hanning},
    {"hamming",        1,   hamming},
    {"quadric",        1.5, quadric},
    {"welch",          1,   welch},
    {"kaiser",         1,   kaiser,   .params = {6.33, NAN} },
    {"blackman",       1,   blackman, .params = {0.16, NAN} },
    {"gaussian",       2,   gaussian, .params = {1.0,  NAN} },
    {"sinc",           1,   sinc},
    {"jinc",           1.2196698912665045, jinc},
    {"sphinx",         1.4302966531242027, sphinx},
    {0}
};

const struct filter_kernel mp_filter_kernels[] = {
    // Spline filters
    {{"spline16",       2,   spline16}},
    {{"spline36",       3,   spline36}},
    {{"spline64",       4,   spline64}},
    // Sinc filters
    {{"sinc",           2,  sinc, .resizable = true}},
    {{"lanczos",        3,  sinc, .resizable = true}, .window = "sinc"},
    {{"ginseng",        3,  sinc, .resizable = true}, .window = "jinc"},
    // Jinc filters
    {{"jinc",           3,  jinc, .resizable = true}, .polar = true},
    {{"ewa_lanczos",    3,  jinc, .resizable = true}, .polar = true, .window = "jinc"},
    {{"ewa_hanning",    3,  jinc, .resizable = true}, .polar = true, .window = "hanning" },
    {{"ewa_ginseng",    3,  jinc, .resizable = true}, .polar = true, .window = "sinc"},
    // Radius is based on the true jinc radius, slightly sharpened as per
    // calculations by Nicolas Robidoux. Source: Imagemagick's magick/resize.c
    {{"ewa_lanczossharp", 3.2383154841662362, jinc, .blur = 0.9812505644269356,
          .resizable = true}, .polar = true, .window = "jinc"},
    // Similar to the above, but softened instead. This one makes hash patterns
    // disappear completely. Blur determined by trial and error.
    {{"ewa_lanczossoft", 3.2383154841662362, jinc, .blur = 1.015,
          .resizable = true}, .polar = true, .window = "jinc"},
    // Very soft (blurred) hanning-windowed jinc; removes almost all aliasing.
    // Blur paramater picked to match orthogonal and diagonal contributions
    {{"haasnsoft", 3.2383154841662362, jinc, .blur = 1.11, .resizable = true},
          .polar = true, .window = "hanning"},
    // Cubic filters
    {{"bicubic",        2,   bicubic}},
    {{"bcspline",       2,   cubic_bc, .params = {0.5, 0.5} }},
    {{"catmull_rom",    2,   cubic_bc, .params = {0.0, 0.5} }},
    {{"mitchell",       2,   cubic_bc, .params = {1.0/3.0, 1.0/3.0} }},
    {{"robidoux",       2,   cubic_bc, .params = {0.3782, 0.3109}}, .polar = true},
    {{"robidouxsharp",  2,   cubic_bc, .params = {0.2620, 0.3690}}, .polar = true},
    // Miscalleaneous filters
    {{"box",            1,   box, .resizable = true}},
    {{"nearest",        0.5, box}},
    {{"triangle",       1,   triangle, .resizable = true}},
    {{"gaussian",       2,   gaussian, .params = {1.0, NAN}, .resizable = true}},
    {{0}}
};
