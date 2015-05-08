/*
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

#ifndef MP_CHMAP_H
#define MP_CHMAP_H

#include <inttypes.h>
#include <stdbool.h>
#include "misc/bstr.h"

#define MP_NUM_CHANNELS 8

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
     // Inofficial/libav* extensions
    MP_SPEAKER_ID_DL = 29,      // STEREO_LEFT (stereo downmix special speakers)
    MP_SPEAKER_ID_DR,           // STEREO_RIGHT
    MP_SPEAKER_ID_WL,           // WIDE_LEFT
    MP_SPEAKER_ID_WR,           // WIDE_RIGHT
    MP_SPEAKER_ID_SDL,          // SURROUND_DIRECT_LEFT
    MP_SPEAKER_ID_SDR,          // SURROUND_DIRECT_RIGHT
    MP_SPEAKER_ID_LFE2,         // LOW_FREQUENCY_2

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
void mp_chmap_from_channels_alsa(struct mp_chmap *dst, int num_channels);

void mp_chmap_remove_useless_channels(struct mp_chmap *map,
                                      const struct mp_chmap *requested);

uint64_t mp_chmap_to_lavc(const struct mp_chmap *src);
uint64_t mp_chmap_to_lavc_unchecked(const struct mp_chmap *src);
void mp_chmap_from_lavc(struct mp_chmap *dst, uint64_t src);

bool mp_chmap_is_lavc(const struct mp_chmap *src);
void mp_chmap_reorder_to_lavc(struct mp_chmap *map);

void mp_chmap_get_reorder(int src[MP_NUM_CHANNELS], const struct mp_chmap *from,
                          const struct mp_chmap *to);

int mp_chmap_diffn(const struct mp_chmap *a, const struct mp_chmap *b);

char *mp_chmap_to_str_buf(char *buf, size_t buf_size, const struct mp_chmap *src);
#define mp_chmap_to_str(m) mp_chmap_to_str_buf((char[64]){0}, 64, (m))

bool mp_chmap_from_str(struct mp_chmap *dst, bstr src);

struct mp_log;
void mp_chmap_print_help(struct mp_log *log);

// Use these to avoid chaos in case lavc's definition should diverge from MS.
#define mp_chmap_to_waveext mp_chmap_to_lavc
#define mp_chmap_from_waveext mp_chmap_from_lavc
#define mp_chmap_is_waveext mp_chmap_is_lavc
#define mp_chmap_reorder_to_waveext mp_chmap_reorder_to_lavc

#endif
