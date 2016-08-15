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

#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include "common/common.h"
#include "common/msg.h"
#include "chmap_sel.h"

static const struct mp_chmap speaker_replacements[][2] = {
    // 5.1 <-> 5.1 (side)
    { MP_CHMAP2(SL, SR), MP_CHMAP2(BL, BR) },
    // 7.1 <-> 7.1 (rear ext)
    { MP_CHMAP2(SL, SR), MP_CHMAP2(SDL, SDR) },
};

// Try to replace speakers from the left of the list with the ones on the
// right, or the other way around.
static bool replace_speakers(struct mp_chmap *map, struct mp_chmap list[2])
{
    assert(list[0].num == list[1].num);
    if (!mp_chmap_is_valid(map))
        return false;
    for (int dir = 0; dir < 2; dir++) {
        int from = dir ? 0 : 1;
        int to   = dir ? 1 : 0;
        bool replaced = false;
        struct mp_chmap t = *map;
        for (int n = 0; n < t.num; n++) {
            for (int i = 0; i < list[0].num; i++) {
                if (t.speaker[n] == list[from].speaker[i]) {
                    t.speaker[n] = list[to].speaker[i];
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

// These go strictly from the first to the second entry and always use the
// full layout (possibly reordered and/or padding channels added).
static const struct mp_chmap preferred_remix[][2] = {
    // mono can be perfectly played as stereo
    { MP_CHMAP_INIT_MONO, MP_CHMAP_INIT_STEREO },
};

// Conversion from src to dst is explicitly encouraged and should be preferred
// over "mathematical" upmixes or downmixes (which minimize lost channels).
static bool test_preferred_remix(const struct mp_chmap *src,
                                 const struct mp_chmap *dst)
{
    struct mp_chmap src_p = *src, dst_p = *dst;
    mp_chmap_remove_na(&src_p);
    mp_chmap_remove_na(&dst_p);
    for (int n = 0; n < MP_ARRAY_SIZE(preferred_remix); n++) {
        if (mp_chmap_equals_reordered(&src_p, &preferred_remix[n][0]) &&
            mp_chmap_equals_reordered(&dst_p, &preferred_remix[n][1]))
            return true;
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

// Add a channel map that should be allowed.
void mp_chmap_sel_add_map(struct mp_chmap_sel *s, const struct mp_chmap *map)
{
    if (!mp_chmap_is_valid(map))
        return;
    if (!s->chmaps)
        s->chmaps = s->chmaps_storage;
    if (s->num_chmaps == MP_ARRAY_SIZE(s->chmaps_storage)) {
        if (!s->tmp)
            return;
        s->chmaps = talloc_memdup(s->tmp, s->chmaps, sizeof(s->chmaps_storage));
    }
    if (s->chmaps != s->chmaps_storage)
        MP_TARRAY_GROW(s->tmp, s->chmaps, s->num_chmaps);
    s->chmaps[s->num_chmaps++] = *map;
}

// Allow all waveext formats in default order.
void mp_chmap_sel_add_waveext_def(struct mp_chmap_sel *s)
{
    for (int n = 1; n <= MP_NUM_CHANNELS; n++) {
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

    if (mp_chmap_sel_fallback(s, map))
        return true;

    for (int i = 0; i < MP_ARRAY_SIZE(speaker_replacements); i++) {
        struct mp_chmap  t = *map;
        struct mp_chmap *r = (struct mp_chmap *)speaker_replacements[i];
        if (replace_speakers(&t, r) && test_layout(s, &t)) {
            *map = t;
            return true;
        }
    }

    // Fallback to mono/stereo as last resort
    *map = (struct mp_chmap) MP_CHMAP_INIT_STEREO;
    if (test_layout(s, map))
        return true;
    *map = (struct mp_chmap) MP_CHMAP_INIT_MONO;
    if (test_layout(s, map))
        return true;
    *map = (struct mp_chmap) {0};
    return false;
}

// Like mp_chmap_diffn(), but find the minimum difference with all possible
// speaker replacements considered.
static int mp_chmap_diffn_r(const struct mp_chmap *a, const struct mp_chmap *b)
{
    int mindiff = INT_MAX;

    for (int i = -1; i < (int)MP_ARRAY_SIZE(speaker_replacements); i++) {
        struct mp_chmap ar = *a;
        if (i >= 0) {
            struct mp_chmap *r = (struct mp_chmap *)speaker_replacements[i];
            if (!replace_speakers(&ar, r))
                continue;
        }
        int d = mp_chmap_diffn(&ar, b);
        if (d < mindiff)
            mindiff = d;
    }

    // Special-case: we consider stereo a replacement for mono. (This is not
    // true in the other direction. Also, fl-fr is generally not a replacement
    // for fc. Thus it's not part of the speaker replacement list.)
    struct mp_chmap mono   = MP_CHMAP_INIT_MONO;
    struct mp_chmap stereo = MP_CHMAP_INIT_STEREO;
    if (mp_chmap_equals(&mono, b) && mp_chmap_equals(&stereo, a))
        mindiff = 0;

    return mindiff;
}

// Decide whether we should prefer old or new for the requested layout.
// Return true if new should be used, false if old should be used.
// If old is empty, always return new (initial case).
static bool mp_chmap_is_better(struct mp_chmap *req, struct mp_chmap *old,
                               struct mp_chmap *new)
{
    // Initial case
    if (!old->num)
        return true;

    // Exact pick - this also ensures that the best layout is chosen if the
    // layouts are the same, but with different order of channels.
    if (mp_chmap_equals(req, old))
        return false;
    if (mp_chmap_equals(req, new))
        return true;

    // If there's no exact match, strictly do a preferred conversion.
    bool old_pref = test_preferred_remix(req, old);
    bool new_pref = test_preferred_remix(req, new);
    if (old_pref && !new_pref)
        return false;
    if (!old_pref && new_pref)
        return true;

    int old_lost_r = mp_chmap_diffn_r(req, old); // num. channels only in req
    int new_lost_r = mp_chmap_diffn_r(req, new);

    // Imperfect upmix (no real superset) - minimize lost channels
    if (new_lost_r != old_lost_r)
        return new_lost_r < old_lost_r;

    struct mp_chmap old_p = *old, new_p = *new;
    mp_chmap_remove_na(&old_p);
    mp_chmap_remove_na(&new_p);

    // If the situation is equal with replaced speakers, but the replacement is
    // perfect for only one of them, let the better one win. This prefers
    // inexact equivalents over exact supersets.
    bool perfect_r_new = !new_lost_r && new_p.num <= old_p.num;
    bool perfect_r_old = !old_lost_r && old_p.num <= new_p.num;
    if (perfect_r_new != perfect_r_old)
        return perfect_r_new;

    int old_lost = mp_chmap_diffn(req, old);
    int new_lost = mp_chmap_diffn(req, new);
    // If the situation is equal with replaced speakers, pick the better one,
    // even if it means an upmix.
    if (new_lost != old_lost)
        return new_lost < old_lost;

    // Some kind of upmix. If it's perfect, prefer the smaller one. Even if not,
    // both have equal loss, so also prefer the smaller one.
    // Drop padding channels (NA) for the sake of this check, as the number of
    // padding channels isn't really meaningful.
    if (new_p.num != old_p.num)
        return new_p.num < old_p.num;

    // Again, with physical channels (minimizes number of NA channels).
    return new->num < old->num;
}

// Determine which channel map to fallback to given a source channel map.
bool mp_chmap_sel_fallback(const struct mp_chmap_sel *s, struct mp_chmap *map)
{
    struct mp_chmap best = {0};

    for (int n = 0; n < s->num_chmaps; n++) {
        struct mp_chmap e = s->chmaps[n];

        if (mp_chmap_is_unknown(&e))
            continue;

        if (mp_chmap_is_better(map, &best, &e))
            best = e;
    }

    if (best.num) {
        *map = best;
        return true;
    }

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

// Print the set of allowed channel layouts.
void mp_chmal_sel_log(const struct mp_chmap_sel *s, struct mp_log *log, int lev)
{
    if (!mp_msg_test(log, lev))
        return;

    for (int i = 0; i < s->num_chmaps; i++)
        mp_msg(log, lev, " - %s\n", mp_chmap_to_str(&s->chmaps[i]));
    for (int i = 0; i < MP_SPEAKER_ID_COUNT; i++) {
        if (!s->speakers[i])
            continue;
        struct mp_chmap l = {.num = 1, .speaker = { i }};
        mp_msg(log, lev, " - #%s\n",
                    i == MP_SPEAKER_ID_FC ? "fc" : mp_chmap_to_str_hr(&l));
    }
    if (s->allow_waveext)
        mp_msg(log, lev, " - waveext\n");
    if (s->allow_any)
        mp_msg(log, lev, " - anything\n");
}

// Select a channel map from the given list that fits best to c. Don't change
// *c if there's no match, or the list is empty.
void mp_chmap_sel_list(struct mp_chmap *c, struct mp_chmap *maps, int num_maps)
{
    // This is a separate function to keep messing with mp_chmap_sel internals
    // within this source file.
    struct mp_chmap_sel sel = {
        .chmaps = maps,
        .num_chmaps = num_maps,
    };
    mp_chmap_sel_fallback(&sel, c);
}
