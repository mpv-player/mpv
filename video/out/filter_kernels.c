/*
 * This file is part of mplayer2.
 *
 * Most code for computing the weights is taken from Anti-Grain Geometry (AGG)
 * (licensed under GPL 2 or later), with modifications.
 * Copyright (C) 2002-2006 Maxim Shemanarev
 * http://vector-agg.cvs.sourceforge.net/viewvc/vector-agg/agg-2.5/include/agg_image_filters.h?view=markup
 *
 * Also see glumpy (BSD licensed), contains the same code in Python:
 * http://code.google.com/p/glumpy/source/browse/glumpy/image/filter.py
 *
 * Also see: Paul Heckbert's "zoom"
 *
 * Also see XBMC: ConvolutionKernels.cpp etc.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stddef.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "filter_kernels.h"

// NOTE: all filters are separable, symmetric, and are intended for use with
//       a lookup table/texture.

const struct filter_kernel *mp_find_filter_kernel(const char *name)
{
    for (const struct filter_kernel *k = mp_filter_kernels; k->name; k++) {
        if (strcmp(k->name, name) == 0)
            return k;
    }
    return NULL;
}

// sizes = sorted list of available filter sizes, terminated with size 0
// inv_scale = source_size / dest_size
bool mp_init_filter(struct filter_kernel *filter, const int *sizes,
                    double inv_scale)
{
    // only downscaling requires widening the filter
    filter->inv_scale = inv_scale >= 1.0 ? inv_scale : 1.0;
    double support = filter->radius * filter->inv_scale;
    int size = ceil(2.0 * support);
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
        filter->inv_scale = filter->size / 2.0 / filter->radius;
        return false;
    }
}

// Calculate the 1D filtering kernel for N sample points.
// N = number of samples, which is filter->size
// The weights will be stored in out_w[0] to out_w[N - 1]
// f = x0 - abs(x0), subpixel position in the range [0,1) or [0,1].
void mp_compute_weights(struct filter_kernel *filter, double f, float *out_w)
{
    assert(filter->size > 0);
    double sum = 0;
    for (int n = 0; n < filter->size; n++) {
        double x = f - (n - filter->size / 2 + 1);
        double w = filter->weight(filter, fabs(x) / filter->inv_scale);
        out_w[n] = w;
        sum += w;
    }
    //normalize
    for (int n = 0; n < filter->size; n++)
        out_w[n] /= sum;
}

// Fill the given array with weights for the range [0.0, 1.0]. The array is
// interpreted as rectangular array of count * filter->size items.
void mp_compute_lut(struct filter_kernel *filter, int count, float *out_array)
{
    for (int n = 0; n < count; n++) {
        mp_compute_weights(filter, n / (double)(count - 1),
                           out_array + filter->size * n);
    }
}

typedef struct filter_kernel kernel;

static double nearest(kernel *k, double x)
{
    return x > 0.5 ? 0.0 : 1.0;
}

static double bilinear(kernel *k, double x)
{
    return 1.0 - x;
}

static double hanning(kernel *k, double x)
{
    return 0.5 + 0.5 * cos(M_PI * x);
}

static double hamming(kernel *k, double x)
{
    return 0.54 + 0.46 * cos(M_PI * x);
}

static double hermite(kernel *k, double x)
{
    return (2.0 * x - 3.0) * x * x + 1.0;
}

static double quadric(kernel *k, double x)
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

static double bicubic(kernel *k, double x)
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

static double kaiser(kernel *k, double x)
{
    double a = k->params[0];
    double b = k->params[1];
    double epsilon = 1e-12;
    double i0a = 1 / bessel_i0(epsilon, b);
    return bessel_i0(epsilon, a * sqrt(1 - x * x)) * i0a;
}

static double catmull_rom(kernel *k, double x)
{
    if (x < 1.0)
        return 0.5 * (2.0 + x * x * (-5.0 + x * 3.0));
    if (x < 2.0)
        return 0.5 * (4.0 + x * (-8.0 + x * (5.0 - x)));
    return 0;
}

// Mitchell-Netravali
static double mitchell(kernel *k, double x)
{
    double b = k->params[0];
    double c = k->params[1];
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

static double spline16(kernel *k, double x)
{
    if (x < 1.0)
        return ((x - 9.0/5.0 ) * x - 1.0/5.0 ) * x + 1.0;
    return ((-1.0/3.0 * (x-1) + 4.0/5.0) * (x-1) - 7.0/15.0 ) * (x-1);
}

static double spline36(kernel *k, double x)
{
    if(x < 1.0)
        return ((13.0/11.0 * x - 453.0/209.0) * x - 3.0/209.0) * x + 1.0;
    if(x < 2.0)
        return ((-6.0/11.0 * (x - 1) + 270.0/209.0) * (x - 1) - 156.0/209.0)
               * (x - 1);
    return ((1.0/11.0 * (x - 2) - 45.0/209.0) * (x - 2) +  26.0/209.0)
           * (x - 2);
}

static double gaussian(kernel *k, double x)
{
    return exp(-2.0 * x * x) * sqrt(2.0 / M_PI);
}

static double sinc(kernel *k, double x)
{
    if (x == 0.0)
        return 1.0;
    double pix = M_PI * x;
    return sin(pix) / pix;
}

static double lanczos(kernel *k, double x)
{
    double radius = k->size / 2;
    if (x < -radius || x > radius)
        return 0;
    if (x == 0)
        return 1;
    double pix = M_PI * x;
    return radius * sin(pix) * sin(pix / radius) / (pix * pix);
}

static double blackman(kernel *k, double x)
{
    double radius = k->size / 2;
    if (x == 0.0)
        return 1.0;
    if (x > radius)
        return 0.0;
    x *= M_PI;
    double xr = x / radius;
    return (sin(x) / x) * (0.42 + 0.5 * cos(xr) + 0.08 * cos(2 * xr));
}

const struct filter_kernel mp_filter_kernels[] = {
    {"nearest",        0.5, nearest},
    {"bilinear_slow",  1,   bilinear},
    {"hanning",        1,   hanning},
    {"hamming",        1,   hamming},
    {"hermite",        1,   hermite},
    {"quadric",        1.5, quadric},
    {"bicubic",        2,   bicubic},
    {"kaiser",         1,   kaiser, .params = {6.33, 6.33} },
    {"catmull_rom",    2,   catmull_rom},
    {"mitchell",       2,   mitchell, .params = {1.0/3.0, 1.0/3.0} },
    {"spline16",       2,   spline16},
    {"spline36",       3,   spline36},
    {"gaussian",       2,   gaussian},
    {"sinc2",          2,   sinc},
    {"sinc3",          3,   sinc},
    {"sinc4",          4,   sinc},
    {"lanczos2",       2,   lanczos},
    {"lanczos3",       3,   lanczos},
    {"lanczos4",       4,   lanczos},
    {"blackman2",      2,   blackman},
    {"blackman3",      3,   blackman},
    {"blackman4",      4,   blackman},
    {0}
};
