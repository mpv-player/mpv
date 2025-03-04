/*
 * Generate a dithering matrix for downsampling images.
 *
 * Copyright © 2013  Wessel Dankers <wsl@fruit.je>
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include <libavutil/lfg.h>

#include "misc/mp_assert.h"
#include "mpv_talloc.h"
#include "dither.h"

#define MAX_SIZEB 8
#define MAX_SIZE (1 << MAX_SIZEB)
#define MAX_SIZE2 (MAX_SIZE * MAX_SIZE)

#define WRAP_SIZE2(k, x) ((unsigned int)((unsigned int)(x) & ((k)->size2 - 1)))
#define XY(k, x, y) ((unsigned int)(((x) | ((y) << (k)->sizeb))))

struct ctx {
    unsigned int sizeb, size, size2;
    unsigned int gauss_radius;
    unsigned int gauss_middle;
    uint64_t gauss[MAX_SIZE2];
    unsigned int randomat[MAX_SIZE2];
    bool calcmat[MAX_SIZE2];
    uint64_t gaussmat[MAX_SIZE2];
    unsigned int unimat[MAX_SIZE2];
    AVLFG avlfg;
};

static void makegauss(struct ctx *k, unsigned int sizeb)
{
    mp_assert(sizeb >= 1 && sizeb <= MAX_SIZEB);

    av_lfg_init(&k->avlfg, 123);

    k->sizeb = sizeb;
    k->size = 1 << k->sizeb;
    k->size2 = k->size * k->size;

    k->gauss_radius = k->size / 2 - 1;
    k->gauss_middle = XY(k, k->gauss_radius, k->gauss_radius);

    unsigned int gauss_size = k->gauss_radius * 2 + 1;
    unsigned int gauss_size2 = gauss_size * gauss_size;

    for (unsigned int c = 0; c < k->size2; c++)
        k->gauss[c] = 0;

    double sigma = -log(1.5 / (double) UINT64_MAX * gauss_size2) / k->gauss_radius;

    for (unsigned int gy = 0; gy <= k->gauss_radius; gy++) {
        for (unsigned int gx = 0; gx <= gy; gx++) {
            int cx = (int)gx - k->gauss_radius;
            int cy = (int)gy - k->gauss_radius;
            int sq = cx * cx + cy * cy;
            double e = exp(-sqrt(sq) * sigma);
            uint64_t v = e / gauss_size2 * (double) UINT64_MAX;
            k->gauss[XY(k, gx, gy)] =
                k->gauss[XY(k, gy, gx)] =
                k->gauss[XY(k, gx, gauss_size - 1 - gy)] =
                k->gauss[XY(k, gy, gauss_size - 1 - gx)] =
                k->gauss[XY(k, gauss_size - 1 - gx, gy)] =
                k->gauss[XY(k, gauss_size - 1 - gy, gx)] =
                k->gauss[XY(k, gauss_size - 1 - gx, gauss_size - 1 - gy)] =
                k->gauss[XY(k, gauss_size - 1 - gy, gauss_size - 1 - gx)] = v;
        }
    }
    uint64_t total = 0;
    for (unsigned int c = 0; c < k->size2; c++) {
        uint64_t oldtotal = total;
        total += k->gauss[c];
        mp_assert(total >= oldtotal);
    }
}

static void setbit(struct ctx *k, unsigned int c)
{
    if (k->calcmat[c])
        return;
    k->calcmat[c] = true;
    uint64_t *m = k->gaussmat;
    uint64_t *me = k->gaussmat + k->size2;
    uint64_t *g = k->gauss + WRAP_SIZE2(k, k->gauss_middle + k->size2 - c);
    uint64_t *ge = k->gauss + k->size2;
    while (g < ge)
        *m++ += *g++;
    g = k->gauss;
    while (m < me)
        *m++ += *g++;
}

static unsigned int getmin(struct ctx *k)
{
    uint64_t min = UINT64_MAX;
    unsigned int resnum = 0;
    unsigned int size2 = k->size2;
    for (unsigned int c = 0; c < size2; c++) {
        if (k->calcmat[c])
            continue;
        uint64_t total = k->gaussmat[c];
        if (total <= min) {
            if (total != min) {
                min = total;
                resnum = 0;
            }
            k->randomat[resnum++] = c;
        }
    }
    if (resnum == 1)
        return k->randomat[0];
    if (resnum == size2)
        return size2 / 2;
    return k->randomat[av_lfg_get(&k->avlfg) % resnum];
}

static void makeuniform(struct ctx *k)
{
    unsigned int size2 = k->size2;
    for (unsigned int c = 0; c < size2; c++) {
        unsigned int r = getmin(k);
        setbit(k, r);
        k->unimat[r] = c;
    }
}

// out_matrix is a reactangular tsize * tsize array, where tsize = (1 << size).
void mp_make_fruit_dither_matrix(float *out_matrix, int size)
{
    struct ctx *k = talloc_zero(NULL, struct ctx);
    makegauss(k, size);
    makeuniform(k);
    float invscale = k->size2;
    for(unsigned int y = 0; y < k->size; y++) {
        for(unsigned int x = 0; x < k->size; x++)
            out_matrix[x + y * k->size] = k->unimat[XY(k, x, y)] / invscale;
    }
    talloc_free(k);
}

void mp_make_ordered_dither_matrix(unsigned char *m, int size)
{
    m[0] = 0;
    for (int sz = 1; sz < size; sz *= 2) {
        int offset[] = {sz*size, sz, sz * (size+1), 0};
        for (int i = 0; i < 4; i++)
            for (int y = 0; y < sz * size; y += size)
                for (int x = 0; x < sz; x++)
                    m[x+y+offset[i]] = m[x+y] * 4 + (3-i) * 256/size/size;
    }
}
