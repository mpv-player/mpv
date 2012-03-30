/*
 * This file is part of mplayer2.
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

#ifndef MPLAYER_FILTER_KERNELS_H
#define MPLAYER_FILTER_KERNELS_H

#include <stdbool.h>

struct filter_kernel {
    const char *name;
    double radius;
    double (*weight)(struct filter_kernel *kernel, double x);

    // The filter params can be changed at runtime. Only used by some filters.
    float params[2];
    // The following values are set by mp_init_filter() at runtime.
    // Number of coefficients; equals the rounded up radius multiplied with 2.
    int size;
    double inv_scale;
};

extern const struct filter_kernel mp_filter_kernels[];

const struct filter_kernel *mp_find_filter_kernel(const char *name);
bool mp_init_filter(struct filter_kernel *filter, const int *sizes,
                    double scale);
void mp_compute_weights(struct filter_kernel *filter, double f, float *out_w);
void mp_compute_lut(struct filter_kernel *filter, int count, float *out_array);

#endif /* MPLAYER_FILTER_KERNELS_H */
