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

#include <stdlib.h>
#include <assert.h>

#include "chmap_sel.h"

// 5.1 and 5.1(side) are practically the same. It doesn't make much sense to
// reject either of them.
static const int replaceable_speakers[][2] = {
    {MP_SPEAKER_ID_SL, MP_SPEAKER_ID_BL},
    {MP_SPEAKER_ID_SR, MP_SPEAKER_ID_BR},
    {-1},
};

// list[] contains a list of speaker pairs, with each pair indicating how
// a speaker can be swapped for another speaker. Try to replace speakers from
// the left of the list with the ones on the right, or the other way around.
static bool replace_speakers(struct mp_chmap *map, const int list[][2])
{
    if (!mp_chmap_is_valid(map))
        return false;
    for (int dir = 0; dir < 2; dir++) {
        int from = dir ? 0 : 1;
        int to   = dir ? 1 : 0;
        bool replaced = false;
        struct mp_chmap t = *map;
        for (int n = 0; n < t.num; n++) {
            for (int i = 0; list[i][0] != -1; i++) {
                if (t.speaker[n] == list[i][from]) {
                    t.speaker[n] = list[i][to];
                    replaced = true;
                    break;
                }
            }
        }
        if (replaced && mp_chmap_is_valid(&t)) {
            *map = t;
            return true;
        }
    }
    return false;
}

// Allow all channel layouts that can be expressed with mp_chmap.
// (By default, all layouts are rejected.)
void mp_chmap_sel_add_any(struct mp_chmap_sel *s)
{
    s->allow_any = true;
}

// Allow all waveext formats, and force waveext channel order.
void mp_chmap_sel_add_waveext(struct mp_chmap_sel *s)
{
    s->allow_waveext = true;
}

void mp_chmap_sel_add_alsa_def(struct mp_chmap_sel *s)
{
    for (int n = 0; n < MP_NUM_CHANNELS; n++) {
        struct mp_chmap t;
        mp_chmap_from_channels_alsa(&t, n);
        if (t.num)
            mp_chmap_sel_add_map(s, &t);
    }
}

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

// Add a channel map that should be allowed.
void mp_chmap_sel_add_map(struct mp_chmap_sel *s, const struct mp_chmap *map)
{
    assert(s->num_chmaps < ARRAY_LEN(s->chmaps));
    if (mp_chmap_is_valid(map))
        s->chmaps[s->num_chmaps++] = *map;
}

// Allow all waveext formats in default order.
void mp_chmap_sel_add_waveext_def(struct mp_chmap_sel *s)
{
    for (int n = 1; n < MP_NUM_CHANNELS; n++) {
        struct mp_chmap map;
        mp_chmap_from_channels(&map, n);
        mp_chmap_sel_add_map(s, &map);
    }
}

// Whitelist a speaker (MP_SPEAKER_ID_...). All layouts that contain whitelisted
// speakers are allowed.
void mp_chmap_sel_add_speaker(struct mp_chmap_sel *s, int id)
{
    assert(id >= 0 && id < MP_SPEAKER_ID_COUNT);
    s->speakers[id] = true;
}

static bool test_speakers(const struct mp_chmap_sel *s, struct mp_chmap *map)
{
    for (int n = 0; n < map->num; n++) {
        if (!s->speakers[map->speaker[n]])
            return false;
    }
    return true;
}

static bool test_maps(const struct mp_chmap_sel *s, struct mp_chmap *map)
{
    for (int n = 0; n < s->num_chmaps; n++) {
        if (mp_chmap_equals_reordered(&s->chmaps[n], map)) {
            *map = s->chmaps[n];
            return true;
        }
    }
    return false;
}

static bool test_waveext(const struct mp_chmap_sel *s, struct mp_chmap *map)
{
    if (s->allow_waveext) {
        struct mp_chmap t = *map;
        mp_chmap_reorder_to_waveext(&t);
        if (mp_chmap_is_waveext(&t)) {
            *map = t;
            return true;
        }
    }
    return false;
}

static bool test_layout(const struct mp_chmap_sel *s, struct mp_chmap *map)
{
    if (!mp_chmap_is_valid(map))
        return false;

    return s->allow_any || test_waveext(s, map) || test_speakers(s, map) ||
           test_maps(s, map);
}

// Determine which channel map to use given a source channel map, and various
// parameters restricting possible choices. If the map doesn't match, select
// a fallback and set it.
// If no matching layout is found, a reordered layout may be returned.
// If that is not possible, a fallback for up/downmixing may be returned.
// If no choice is possible, set *map to empty.
bool mp_chmap_sel_adjust(const struct mp_chmap_sel *s, struct mp_chmap *map)
{
    if (test_layout(s, map))
        return true;
    if (mp_chmap_is_unknown(map)) {
        struct mp_chmap t = {0};
        if (mp_chmap_sel_get_def(s, &t, map->num) && test_layout(s, &t)) {
            *map = t;
            return true;
        }
    }
    // 5.1 <-> 5.1(side)
    if (replace_speakers(map, replaceable_speakers) && test_layout(s, map))
        return true;
    // Fallback to mono/stereo as last resort
    if (map->num == 1) {
        *map = (struct mp_chmap) MP_CHMAP_INIT_MONO;
    } else if (map->num >= 2) {
        *map = (struct mp_chmap) MP_CHMAP_INIT_STEREO;
    }
    if (test_layout(s, map))
        return true;
    *map = (struct mp_chmap) {0};
    return false;
}

// Set map to a default layout with num channels. Used for audio APIs that
// return a channel count as part of format negotiation, but give no
// information about the channel layout.
// If the channel count is correct, do nothing and leave *map untouched.
bool mp_chmap_sel_get_def(const struct mp_chmap_sel *s, struct mp_chmap *map,
                          int num)
{
    if (map->num != num) {
        *map = (struct mp_chmap) {0};
        // Set of speakers or waveext might allow it.
        struct mp_chmap t;
        mp_chmap_from_channels(&t, num);
        mp_chmap_reorder_to_waveext(&t);
        if (test_layout(s, &t)) {
            *map = t;
        } else {
            for (int n = 0; n < s->num_chmaps; n++) {
                if (s->chmaps[n].num == num) {
                    *map = s->chmaps[n];
                    break;
                }
            }
        }
    }
    return map->num > 0;
}
