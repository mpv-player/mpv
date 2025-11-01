/*
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

#ifndef MP_CHMAP_H
#define MP_CHMAP_H

#include <inttypes.h>
#include <stdbool.h>
#include "misc/bstr.h"

#define MP_NUM_CHANNELS 64

// Speaker a channel can be assigned to.
// This corresponds to WAVEFORMATEXTENSIBLE channel mask bit indexes.
// E.g. channel_mask = (1 << MP_SPEAKER_ID_FL) | ...
enum mp_speaker_id {
    // Official WAVEFORMATEXTENSIBLE (shortened names)
    MP_SPEAKER_ID_FL = 0,       // FRONT_LEFT
    MP_SPEAKER_ID_FR,           // FRONT_RIGHT
    MP_SPEAKER_ID_FC,           // FRONT_CENTER
    MP_SPEAKER_ID_LFE,          // LOW_FREQUENCY
    MP_SPEAKER_ID_BL,           // BACK_LEFT
    MP_SPEAKER_ID_BR,           // BACK_RIGHT
    MP_SPEAKER_ID_FLC,          // FRONT_LEFT_OF_CENTER
    MP_SPEAKER_ID_FRC,          // FRONT_RIGHT_OF_CENTER
    MP_SPEAKER_ID_BC,           // BACK_CENTER
    MP_SPEAKER_ID_SL,           // SIDE_LEFT
    MP_SPEAKER_ID_SR,           // SIDE_RIGHT
    MP_SPEAKER_ID_TC,           // TOP_CENTER
    MP_SPEAKER_ID_TFL,          // TOP_FRONT_LEFT
    MP_SPEAKER_ID_TFC,          // TOP_FRONT_CENTER
    MP_SPEAKER_ID_TFR,          // TOP_FRONT_RIGHT
    MP_SPEAKER_ID_TBL,          // TOP_BACK_LEFT
    MP_SPEAKER_ID_TBC,          // TOP_BACK_CENTER
    MP_SPEAKER_ID_TBR,          // TOP_BACK_RIGHT
     // Unofficial/libav* extensions
    MP_SPEAKER_ID_DL = 29,      // STEREO_LEFT (stereo downmix special speakers)
    MP_SPEAKER_ID_DR,           // STEREO_RIGHT
    MP_SPEAKER_ID_WL,           // WIDE_LEFT
    MP_SPEAKER_ID_WR,           // WIDE_RIGHT
    MP_SPEAKER_ID_SDL,          // SURROUND_DIRECT_LEFT
    MP_SPEAKER_ID_SDR,          // SURROUND_DIRECT_RIGHT
    MP_SPEAKER_ID_LFE2,         // LOW_FREQUENCY_2
    MP_SPEAKER_ID_TSL,          // TOP_SIDE_LEFT
    MP_SPEAKER_ID_TSR,          // TOP_SIDE_RIGHT,
    MP_SPEAKER_ID_BFC,          // BOTTOM_FRONT_CENTER
    MP_SPEAKER_ID_BFL,          // BOTTOM_FRONT_LEFT
    MP_SPEAKER_ID_BFR,          // BOTTOM_FRONT_RIGHT

    // Speaker IDs >= 64 are not representable in WAVEFORMATEXTENSIBLE or libav*.

    // "Silent" channels. These are sometimes used to insert padding for
    // unused channels. Unlike other speaker types, multiple of these can
    // occur in a single mp_chmap.
    MP_SPEAKER_ID_NA = 64,

    // Including the unassigned IDs in between. This is not a valid ID anymore,
    // but is still within uint8_t.
    MP_SPEAKER_ID_COUNT,
};

struct mp_chmap {
    uint8_t num; // number of channels
    // Given a channel n, speaker[n] is the speaker ID driven by that channel.
    // Entries after speaker[num - 1] are undefined.
    uint8_t speaker[MP_NUM_CHANNELS];
};

typedef const char * const (mp_ch_layout_tuple)[2];

#define MP_SP(speaker) MP_SPEAKER_ID_ ## speaker

#define MP_CHMAP2(a, b) \
    {2, {MP_SP(a), MP_SP(b)}}
#define MP_CHMAP3(a, b, c) \
    {3, {MP_SP(a), MP_SP(b), MP_SP(c)}}
#define MP_CHMAP4(a, b, c, d) \
    {4, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d)}}
#define MP_CHMAP5(a, b, c, d, e) \
    {5, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e)}}
#define MP_CHMAP6(a, b, c, d, e, f) \
    {6, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f)}}
#define MP_CHMAP7(a, b, c, d, e, f, g) \
    {7, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g)}}
#define MP_CHMAP8(a, b, c, d, e, f, g, h) \
    {8, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g), MP_SP(h)}}
#define MP_CHMAP9(a, b, c, d, e, f, g, h, i) \
    {9, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g), MP_SP(h), MP_SP(i)}}
#define MP_CHMAP10(a, b, c, d, e, f, g, h, i, j) \
    {10, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g), MP_SP(h), MP_SP(i), MP_SP(j)}}
#define MP_CHMAP11(a, b, c, d, e, f, g, h, i, j, k) \
    {11, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g), MP_SP(h), MP_SP(i), MP_SP(j), MP_SP(k)}},
#define MP_CHMAP12(a, b, c, d, e, f, g, h, i, j, k, l) \
    {12, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g), MP_SP(h), MP_SP(i), MP_SP(j), MP_SP(k), MP_SP(l)}}
#define MP_CHMAP13(a, b, c, d, e, f, g, h, i, j, k, l, m) \
    {13, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g), MP_SP(h), MP_SP(i), MP_SP(j), MP_SP(k), MP_SP(l), MP_SP(m)}}
#define MP_CHMAP14(a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
    {14, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g), MP_SP(h), MP_SP(i), MP_SP(j), MP_SP(k), MP_SP(l), MP_SP(m), MP_SP(n)}}
#define MP_CHMAP15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) \
    {15, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g), MP_SP(h), MP_SP(i), MP_SP(j), MP_SP(k), MP_SP(l), MP_SP(m), MP_SP(n), MP_SP(o)}}
#define MP_CHMAP16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
    {16, {MP_SP(a), MP_SP(b), MP_SP(c), MP_SP(d), MP_SP(e), MP_SP(f), MP_SP(g), MP_SP(h), MP_SP(i), MP_SP(j), MP_SP(k), MP_SP(l), MP_SP(m), MP_SP(n), MP_SP(o), MP_SP(p)}}

#define MP_CHMAP_INIT_MONO {1, {MP_SPEAKER_ID_FC}}
#define MP_CHMAP_INIT_STEREO MP_CHMAP2(FL, FR)

bool mp_chmap_is_valid(const struct mp_chmap *src);
bool mp_chmap_is_empty(const struct mp_chmap *src);
bool mp_chmap_is_unknown(const struct mp_chmap *src);
bool mp_chmap_equals(const struct mp_chmap *a, const struct mp_chmap *b);
bool mp_chmap_equals_reordered(const struct mp_chmap *a, const struct mp_chmap *b);
bool mp_chmap_is_stereo(const struct mp_chmap *src);

void mp_chmap_reorder_norm(struct mp_chmap *map);
void mp_chmap_remove_na(struct mp_chmap *map);
void mp_chmap_fill_na(struct mp_chmap *map, int num);

void mp_chmap_from_channels(struct mp_chmap *dst, int num_channels);
void mp_chmap_set_unknown(struct mp_chmap *dst, int num_channels);

uint64_t mp_chmap_to_lavc(const struct mp_chmap *src);
uint64_t mp_chmap_to_lavc_unchecked(const struct mp_chmap *src);
void mp_chmap_from_lavc(struct mp_chmap *dst, uint64_t src);

bool mp_chmap_is_lavc(const struct mp_chmap *src);
void mp_chmap_reorder_to_lavc(struct mp_chmap *map);

void mp_chmap_get_reorder(int src[MP_NUM_CHANNELS], const struct mp_chmap *from,
                          const struct mp_chmap *to);

int mp_chmap_diffn(const struct mp_chmap *a, const struct mp_chmap *b);

char *mp_chmap_to_str_buf(char *buf, size_t buf_size, const struct mp_chmap *src);
#define mp_chmap_to_str_(m, sz) mp_chmap_to_str_buf((char[sz]){0}, sz, (m))
#define mp_chmap_to_str(m) mp_chmap_to_str_(m, MP_NUM_CHANNELS * 4)

char *mp_chmap_to_str_hr_buf(char *buf, size_t buf_size, const struct mp_chmap *src);
#define mp_chmap_to_str_hr_(m, sz) mp_chmap_to_str_hr_buf((char[sz]){0}, sz, (m))
#define mp_chmap_to_str_hr(m) mp_chmap_to_str_hr_(m, MP_NUM_CHANNELS * 4)

bool mp_chmap_from_str(struct mp_chmap *dst, bstr src);

/**
 * Iterate over all built-in channel layouts which have mapped channels.
 *
 * @param opaque a pointer where the iteration state is stored. Must point
 *               to nullptr to start the iteration.
 *
 * @return nullptr when the iteration is finished.
 *         Otherwise a pointer to an array of two char pointers.
 *         - [0] being the human-readable layout name.
 *         - [1] being the string representation of the layout.
 */
mp_ch_layout_tuple *mp_iterate_builtin_layouts(void **opaque);

struct mp_log;
void mp_chmap_print_help(struct mp_log *log);

// Use these to avoid chaos in case lavc's definition should diverge from MS.
#define mp_chmap_to_waveext mp_chmap_to_lavc
#define mp_chmap_from_waveext mp_chmap_from_lavc
#define mp_chmap_is_waveext mp_chmap_is_lavc
#define mp_chmap_reorder_to_waveext mp_chmap_reorder_to_lavc

#endif
