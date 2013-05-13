/*
 * common functions for reordering audio channels
 *
 * Copyright (C) 2007 Ulion <ulion A gmail P com>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "chmap.h"
#include "reorder_ch.h"

static inline void reorder_to_planar_(void *restrict out, const void *restrict in,
                                      size_t size, size_t nchan, size_t nmemb)
{
    size_t i, c;
    char *outptr = (char *) out;
    size_t instep = nchan * size;

    for (c = 0; c < nchan; ++c) {
        const char *inptr = ((const char *) in) + c * size;
        for (i = 0; i < nmemb; ++i, inptr += instep, outptr += size) {
            memcpy(outptr, inptr, size);
        }
    }
}

void reorder_to_planar(void *restrict out, const void *restrict in,
                       size_t size, size_t nchan, size_t nmemb)
{
    // special case for mono (nothing to do...)
    if (nchan == 1)
        memcpy(out, in, size * nchan * nmemb);
    // these calls exist to insert an inline copy of to_planar_ here with known
    // value of size to help the compiler replace the memcpy calls by mov
    // instructions
    else if (size == 1)
        reorder_to_planar_(out, in, 1, nchan, nmemb);
    else if (size == 2)
        reorder_to_planar_(out, in, 2, nchan, nmemb);
    else if (size == 4)
        reorder_to_planar_(out, in, 4, nchan, nmemb);
    // general case (calls memcpy a lot, should actually never happen, but
    // stays here for correctness purposes)
    else
        reorder_to_planar_(out, in, size, nchan, nmemb);
}

static inline void reorder_to_packed_(uint8_t *out, uint8_t **in,
                                      size_t size, size_t nchan, size_t nmemb)
{
    size_t outstep = nchan * size;

    for (size_t c = 0; c < nchan; ++c) {
        char *outptr = out + c * size;
        char *inptr = in[c];
        for (size_t i = 0; i < nmemb; ++i, outptr += outstep, inptr += size) {
            memcpy(outptr, inptr, size);
        }
    }
}

// out = destination array of packed samples of given size, nmemb frames
// in[channel] = source array of samples for the given channel
void reorder_to_packed(uint8_t *out, uint8_t **in,
                       size_t size, size_t nchan, size_t nmemb)
{
    if (nchan == 1)
        memcpy(out, in, size * nchan * nmemb);
    // See reorder_to_planar() why this is done this way
    else if (size == 1)
        reorder_to_packed_(out, in, 1, nchan, nmemb);
    else if (size == 2)
        reorder_to_packed_(out, in, 2, nchan, nmemb);
    else if (size == 4)
        reorder_to_packed_(out, in, 4, nchan, nmemb);
    else
        reorder_to_packed_(out, in, size, nchan, nmemb);
}

#define MAX_SAMPLESIZE 8

static void reorder_channels_(uint8_t *restrict data, int *restrict ch_order,
                              size_t sample_size, size_t num_ch,
                              size_t num_frames)
{
    char buffer[MP_NUM_CHANNELS * MAX_SAMPLESIZE];
    for (size_t f = 0; f < num_frames; f++) {
        for (uint8_t c = 0; c < num_ch; c++) {
            memcpy(buffer + sample_size * c, data + sample_size * ch_order[c],
                   sample_size);
        }
        memcpy(data, buffer, sample_size * num_ch);
        data += num_ch * sample_size;
    }
}

// Reorders for each channel:
//   out[ch] = in[ch_order[ch]] (but in-place)
// num_ch is the number of channels
// sample_size is e.g. 2 for s16le
// full byte size of in/out = num_ch * sample_size * num_frames
// Do not use this function in new code; use libavresample instead.
void reorder_channels(void *restrict data, int *restrict ch_order,
                      size_t sample_size, size_t num_ch, size_t num_frames)
{
    // Check 1:1 mapping
    bool need_reorder = false;
    for (int n = 0; n < num_ch; n++)
        need_reorder |= ch_order[n] != n;
    if (!need_reorder)
        return;
    assert(sample_size <= MAX_SAMPLESIZE);
    assert(num_ch <= MP_NUM_CHANNELS);
    // See reorder_to_planar() why this is done this way
    // s16 and float are the most common sample sizes, and 6 channels is the
    // most common case where reordering is required.
    if (sample_size == 2 && num_ch == 6)
        reorder_channels_(data, ch_order, 2, 6, num_frames);
    else if (sample_size == 2)
        reorder_channels_(data, ch_order, 2, num_ch, num_frames);
    else if (sample_size == 4 && num_ch == 6)
        reorder_channels_(data, ch_order, 4, 6, num_frames);
    else if (sample_size == 4)
        reorder_channels_(data, ch_order, 4, num_ch, num_frames);
    else
        reorder_channels_(data, ch_order, sample_size, num_ch, num_frames);
}
