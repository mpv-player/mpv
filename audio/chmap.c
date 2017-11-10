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

#include <libavutil/common.h>

#include "common/common.h"
#include "common/msg.h"
#include "chmap.h"

// Names taken from libavutil/channel_layout.c (Not accessible by API.)
// Use of these names is hard-coded in some places (e.g. ao_alsa.c)
static const char *const speaker_names[MP_SPEAKER_ID_COUNT][2] = {
    [MP_SPEAKER_ID_FL]          = {"fl",   "front left"},
    [MP_SPEAKER_ID_FR]          = {"fr",   "front right"},
    [MP_SPEAKER_ID_FC]          = {"fc",   "front center"},
    [MP_SPEAKER_ID_LFE]         = {"lfe",  "low frequency"},
    [MP_SPEAKER_ID_BL]          = {"bl",   "back left"},
    [MP_SPEAKER_ID_BR]          = {"br",   "back right"},
    [MP_SPEAKER_ID_FLC]         = {"flc",  "front left-of-center"},
    [MP_SPEAKER_ID_FRC]         = {"frc",  "front right-of-center"},
    [MP_SPEAKER_ID_BC]          = {"bc",   "back center"},
    [MP_SPEAKER_ID_SL]          = {"sl",   "side left"},
    [MP_SPEAKER_ID_SR]          = {"sr",   "side right"},
    [MP_SPEAKER_ID_TC]          = {"tc",   "top center"},
    [MP_SPEAKER_ID_TFL]         = {"tfl",  "top front left"},
    [MP_SPEAKER_ID_TFC]         = {"tfc",  "top front center"},
    [MP_SPEAKER_ID_TFR]         = {"tfr",  "top front right"},
    [MP_SPEAKER_ID_TBL]         = {"tbl",  "top back left"},
    [MP_SPEAKER_ID_TBC]         = {"tbc",  "top back center"},
    [MP_SPEAKER_ID_TBR]         = {"tbr",  "top back right"},
    [MP_SPEAKER_ID_DL]          = {"dl",   "downmix left"},
    [MP_SPEAKER_ID_DR]          = {"dr",   "downmix right"},
    [MP_SPEAKER_ID_WL]          = {"wl",   "wide left"},
    [MP_SPEAKER_ID_WR]          = {"wr",   "wide right"},
    [MP_SPEAKER_ID_SDL]         = {"sdl",  "surround direct left"},
    [MP_SPEAKER_ID_SDR]         = {"sdr",  "surround direct right"},
    [MP_SPEAKER_ID_LFE2]        = {"lfe2", "low frequency 2"},
    [MP_SPEAKER_ID_NA]          = {"na",   "not available"},
};

// Names taken from libavutil/channel_layout.c (Not accessible by API.)
// Channel order corresponds to lavc/waveex, except for the alsa entries.
static const char *const std_layout_names[][2] = {
    {"empty",           ""}, // not in lavc
    {"mono",            "fc"},
    {"1.0",             "fc"}, // not in lavc
    {"stereo",          "fl-fr"},
    {"2.0",             "fl-fr"}, // not in lavc
    {"2.1",             "fl-fr-lfe"},
    {"3.0",             "fl-fr-fc"},
    {"3.0(back)",       "fl-fr-bc"},
    {"4.0",             "fl-fr-fc-bc"},
    {"quad",            "fl-fr-bl-br"},
    {"quad(side)",      "fl-fr-sl-sr"},
    {"3.1",             "fl-fr-fc-lfe"},
    {"3.1(back)",       "fl-fr-lfe-bc"}, // not in lavc
    {"5.0",             "fl-fr-fc-bl-br"},
    {"5.0(alsa)",       "fl-fr-bl-br-fc"}, // not in lavc
    {"5.0(side)",       "fl-fr-fc-sl-sr"},
    {"4.1",             "fl-fr-fc-lfe-bc"},
    {"4.1(alsa)",       "fl-fr-bl-br-lfe"}, // not in lavc
    {"5.1",             "fl-fr-fc-lfe-bl-br"},
    {"5.1(alsa)",       "fl-fr-bl-br-fc-lfe"}, // not in lavc
    {"5.1(side)",       "fl-fr-fc-lfe-sl-sr"},
    {"6.0",             "fl-fr-fc-bc-sl-sr"},
    {"6.0(front)",      "fl-fr-flc-frc-sl-sr"},
    {"hexagonal",       "fl-fr-fc-bl-br-bc"},
    {"6.1",             "fl-fr-fc-lfe-bc-sl-sr"},
    {"6.1(back)",       "fl-fr-fc-lfe-bl-br-bc"}, // lavc calls this "6.1" too
    {"6.1(top)",        "fl-fr-fc-lfe-bl-br-tc"}, // not in lavc
    {"6.1(front)",      "fl-fr-lfe-flc-frc-sl-sr"},
    {"7.0",             "fl-fr-fc-bl-br-sl-sr"},
    {"7.0(front)",      "fl-fr-fc-flc-frc-sl-sr"},
    {"7.0(rear)",       "fl-fr-fc-bl-br-sdl-sdr"}, // not in lavc
    {"7.1",             "fl-fr-fc-lfe-bl-br-sl-sr"},
    {"7.1(alsa)",       "fl-fr-bl-br-fc-lfe-sl-sr"}, // not in lavc
    {"7.1(wide)",       "fl-fr-fc-lfe-bl-br-flc-frc"},
    {"7.1(wide-side)",  "fl-fr-fc-lfe-flc-frc-sl-sr"},
    {"7.1(rear)",       "fl-fr-fc-lfe-bl-br-sdl-sdr"}, // not in lavc
    {"octagonal",       "fl-fr-fc-bl-br-bc-sl-sr"},
    {"auto",            ""}, // not in lavc
    {0}
};

static const struct mp_chmap default_layouts[] = {
    {0},                                        // empty
    MP_CHMAP_INIT_MONO,                         // mono
    MP_CHMAP2(FL, FR),                          // stereo
    MP_CHMAP3(FL, FR, LFE),                     // 2.1
    MP_CHMAP4(FL, FR, FC, BC),                  // 4.0
    MP_CHMAP5(FL, FR, FC, BL,  BR),             // 5.0
    MP_CHMAP6(FL, FR, FC, LFE, BL, BR),         // 5.1
    MP_CHMAP7(FL, FR, FC, LFE, BC, SL, SR),     // 6.1
    MP_CHMAP8(FL, FR, FC, LFE, BL, BR, SL, SR), // 7.1
};

// Returns true if speakers are mapped uniquely, and there's at least 1 channel.
bool mp_chmap_is_valid(const struct mp_chmap *src)
{
    bool mapped[MP_SPEAKER_ID_COUNT] = {0};
    for (int n = 0; n < src->num; n++) {
        int sp = src->speaker[n];
        if (sp >= MP_SPEAKER_ID_COUNT || mapped[sp])
            return false;
        if (sp != MP_SPEAKER_ID_NA)
            mapped[sp] = true;
    }
    return src->num > 0;
}

bool mp_chmap_is_empty(const struct mp_chmap *src)
{
    return src->num == 0;
}

// Return true if the channel map defines the number of the channels only, and
// the channels have to meaning associated with them.
bool mp_chmap_is_unknown(const struct mp_chmap *src)
{
    for (int n = 0; n < src->num; n++) {
        if (src->speaker[n] != MP_SPEAKER_ID_NA)
            return false;
    }
    return mp_chmap_is_valid(src);
}

// Note: empty channel maps compare as equal. Invalid ones can equal too.
bool mp_chmap_equals(const struct mp_chmap *a, const struct mp_chmap *b)
{
    if (a->num != b->num)
        return false;
    for (int n = 0; n < a->num; n++) {
        if (a->speaker[n] != b->speaker[n])
            return false;
    }
    return true;
}

// Whether they use the same speakers (even if in different order).
bool mp_chmap_equals_reordered(const struct mp_chmap *a, const struct mp_chmap *b)
{
    struct mp_chmap t1 = *a, t2 = *b;
    mp_chmap_reorder_norm(&t1);
    mp_chmap_reorder_norm(&t2);
    return mp_chmap_equals(&t1, &t2);
}

bool mp_chmap_is_stereo(const struct mp_chmap *src)
{
    static const struct mp_chmap stereo = MP_CHMAP_INIT_STEREO;
    return mp_chmap_equals(src, &stereo);
}

static int comp_uint8(const void *a, const void *b)
{
    return *(const uint8_t *)a - *(const uint8_t *)b;
}

// Reorder channels to normal order, with monotonically increasing speaker IDs.
// We define this order as the same order used with waveex.
void mp_chmap_reorder_norm(struct mp_chmap *map)
{
    uint8_t *arr = &map->speaker[0];
    qsort(arr, map->num, 1, comp_uint8);
}

// Remove silent (NA) channels, if any.
void mp_chmap_remove_na(struct mp_chmap *map)
{
    struct mp_chmap new = {0};
    for (int n = 0; n < map->num; n++) {
        int sp = map->speaker[n];
        if (sp != MP_SPEAKER_ID_NA)
            new.speaker[new.num++] = map->speaker[n];
    }
    *map = new;
}

// Add silent (NA) channels to map until map->num >= num.
void mp_chmap_fill_na(struct mp_chmap *map, int num)
{
    assert(num <= MP_NUM_CHANNELS);
    while (map->num < num)
        map->speaker[map->num++] = MP_SPEAKER_ID_NA;
}

// Set *dst to a standard layout with the given number of channels.
// If the number of channels is invalid, an invalid map is set, and
// mp_chmap_is_valid(dst) will return false.
void mp_chmap_from_channels(struct mp_chmap *dst, int num_channels)
{
    *dst = (struct mp_chmap) {0};
    if (num_channels >= 0 && num_channels < MP_ARRAY_SIZE(default_layouts))
        *dst = default_layouts[num_channels];
    if (!dst->num)
        mp_chmap_set_unknown(dst, num_channels);
}

// Set *dst to an unknown layout for the given numbers of channels.
// If the number of channels is invalid, an invalid map is set, and
// mp_chmap_is_valid(dst) will return false.
// A mp_chmap with all entries set to NA is treated specially in some
// contexts (watch out for mp_chmap_is_unknown()).
void mp_chmap_set_unknown(struct mp_chmap *dst, int num_channels)
{
    if (num_channels < 0 || num_channels > MP_NUM_CHANNELS) {
        *dst = (struct mp_chmap) {0};
    } else {
        dst->num = num_channels;
        for (int n = 0; n < dst->num; n++)
            dst->speaker[n] = MP_SPEAKER_ID_NA;
    }
}

// Return the ffmpeg/libav channel layout as in <libavutil/channel_layout.h>.
// Speakers not representable by ffmpeg/libav are dropped.
// Warning: this ignores the order of the channels, and will return a channel
//          mask even if the order is different from libavcodec's.
//          Also, "unknown" channel maps are translated to non-sense channel
//          maps with the same number of channels.
uint64_t mp_chmap_to_lavc_unchecked(const struct mp_chmap *src)
{
    struct mp_chmap t = *src;
    if (t.num > 64)
        return 0;
    // lavc has no concept for unknown layouts yet, so pick something that does
    // the job of signaling the number of channels, even if it makes no sense
    // as a proper layout.
    if (mp_chmap_is_unknown(&t))
        return t.num == 64 ? (uint64_t)-1 : (1ULL << t.num) - 1;
    uint64_t mask = 0;
    for (int n = 0; n < t.num; n++) {
        if (t.speaker[n] < 64) // ignore MP_SPEAKER_ID_NA etc.
            mask |= 1ULL << t.speaker[n];
    }
    return mask;
}

// Return the ffmpeg/libav channel layout as in <libavutil/channel_layout.h>.
// Returns 0 if the channel order doesn't match lavc's or if it's invalid.
uint64_t mp_chmap_to_lavc(const struct mp_chmap *src)
{
    if (!mp_chmap_is_lavc(src))
        return 0;
    return mp_chmap_to_lavc_unchecked(src);
}

// Set channel map from the ffmpeg/libav channel layout as in
// <libavutil/channel_layout.h>.
// If the number of channels exceed MP_NUM_CHANNELS, set dst to empty.
void mp_chmap_from_lavc(struct mp_chmap *dst, uint64_t src)
{
    dst->num = 0;
    for (int n = 0; n < 64; n++) {
        if (src & (1ULL << n)) {
            if (dst->num >= MP_NUM_CHANNELS) {
                dst->num = 0;
                return;
            }
            dst->speaker[dst->num] = n;
            dst->num++;
        }
    }
}

bool mp_chmap_is_lavc(const struct mp_chmap *src)
{
    if (!mp_chmap_is_valid(src))
        return false;
    if (mp_chmap_is_unknown(src))
        return true;
    // lavc's channel layout is a bit mask, and channels are always ordered
    // from LSB to MSB speaker bits, so speaker IDs have to increase.
    assert(src->num > 0);
    for (int n = 1; n < src->num; n++) {
        if (src->speaker[n - 1] >= src->speaker[n])
            return false;
    }
    for (int n = 0; n < src->num; n++) {
        if (src->speaker[n] >= 64)
            return false;
    }
    return true;
}

// Warning: for "unknown" channel maps, this returns something that may not
//          make sense. Invalid channel maps are not changed.
void mp_chmap_reorder_to_lavc(struct mp_chmap *map)
{
    if (!mp_chmap_is_valid(map))
        return;
    uint64_t mask = mp_chmap_to_lavc_unchecked(map);
    mp_chmap_from_lavc(map, mask);
}

// Get reordering array for from->to reordering. from->to must have the same set
// of speakers (i.e. same number and speaker IDs, just different order). Then,
// for each speaker n, src[n] will be set such that:
//      to->speaker[n] = from->speaker[src[n]]
// (src[n] gives the source channel for destination channel n)
// If *from and *to don't contain the same set of speakers, then the above
// invariant is not guaranteed. Instead, src[n] can be set to -1 if the channel
// at to->speaker[n] is unmapped.
void mp_chmap_get_reorder(int src[MP_NUM_CHANNELS], const struct mp_chmap *from,
                          const struct mp_chmap *to)
{
    for (int n = 0; n < MP_NUM_CHANNELS; n++)
        src[n] = -1;

    if (mp_chmap_is_unknown(from) || mp_chmap_is_unknown(to)) {
        for (int n = 0; n < to->num; n++)
            src[n] = n < from->num ? n : -1;
        return;
    }

    for (int n = 0; n < to->num; n++) {
        for (int i = 0; i < from->num; i++) {
            if (to->speaker[n] == from->speaker[i]) {
                src[n] = i;
                break;
            }
        }
    }

    for (int n = 0; n < to->num; n++)
        assert(src[n] < 0 || (to->speaker[n] == from->speaker[src[n]]));
}

// Return the number of channels only in a.
int mp_chmap_diffn(const struct mp_chmap *a, const struct mp_chmap *b)
{
    uint64_t a_mask = mp_chmap_to_lavc_unchecked(a);
    uint64_t b_mask = mp_chmap_to_lavc_unchecked(b);
    return av_popcount64((a_mask ^ b_mask) & a_mask);
}

// Returns something like "fl-fr-fc". If there's a standard layout in lavc
// order, return that, e.g. "3.0" instead of "fl-fr-fc".
// Unassigned but valid speakers get names like "sp28".
char *mp_chmap_to_str_buf(char *buf, size_t buf_size, const struct mp_chmap *src)
{
    buf[0] = '\0';

    if (mp_chmap_is_unknown(src)) {
        snprintf(buf, buf_size, "unknown%d", src->num);
        return buf;
    }

    for (int n = 0; n < src->num; n++) {
        int sp = src->speaker[n];
        const char *s = sp < MP_SPEAKER_ID_COUNT ? speaker_names[sp][0] : NULL;
        char sp_buf[10];
        if (!s) {
            snprintf(sp_buf, sizeof(sp_buf), "sp%d", sp);
            s = sp_buf;
        }
        mp_snprintf_cat(buf, buf_size, "%s%s", n > 0 ? "-" : "", s);
    }

    // To standard layout name
    for (int n = 0; std_layout_names[n][0]; n++) {
        if (strcmp(buf, std_layout_names[n][1]) == 0) {
            snprintf(buf, buf_size, "%s", std_layout_names[n][0]);
            break;
        }
    }

    return buf;
}

// If src can be parsed as channel map (as produced by mp_chmap_to_str()),
// return true and set *dst. Otherwise, return false and don't change *dst.
// Note: call mp_chmap_is_valid() to test whether the returned map is valid
//       the map could be empty, or contain multiply mapped channels
bool mp_chmap_from_str(struct mp_chmap *dst, bstr src)
{
    // Single number corresponds to mp_chmap_from_channels()
    if (src.len > 0) {
        bstr t = src;
        bool unknown = bstr_eatstart0(&t, "unknown");
        bstr rest;
        long long count = bstrtoll(t, &rest, 10);
        if (rest.len == 0) {
            struct mp_chmap res;
            if (unknown) {
                mp_chmap_set_unknown(&res, count);
            } else {
                mp_chmap_from_channels(&res, count);
            }
            if (mp_chmap_is_valid(&res)) {
                *dst = res;
                return true;
            }
        }
    }

    // From standard layout name
    for (int n = 0; std_layout_names[n][0]; n++) {
        if (bstr_equals0(src, std_layout_names[n][0])) {
            src = bstr0(std_layout_names[n][1]);
            break;
        }
    }

    // Explicit speaker list (separated by "-")
    struct mp_chmap res = {0};
    while (src.len) {
        bstr s;
        bstr_split_tok(src, "-", &s, &src);
        int speaker = -1;
        for (int n = 0; n < MP_SPEAKER_ID_COUNT; n++) {
            const char *name = speaker_names[n][0];
            if (name && bstr_equals0(s, name)) {
                speaker = n;
                break;
            }
        }
        if (speaker < 0) {
            if (bstr_eatstart0(&s, "sp")) {
                long long sp = bstrtoll(s, &s, 0);
                if (s.len == 0 && sp >= 0 && sp < MP_SPEAKER_ID_COUNT)
                    speaker = sp;
            }
            if (speaker < 0)
                return false;
        }
        if (res.num >= MP_NUM_CHANNELS)
            return false;
        res.speaker[res.num] = speaker;
        res.num++;
    }

    *dst = res;
    return true;
}

// Output a human readable "canonical" channel map string. Converting this from
// a string back to a channel map can yield a different map, but the string
// looks nicer. E.g. "fc-fl-fr-na" becomes "3.0".
char *mp_chmap_to_str_hr_buf(char *buf, size_t buf_size, const struct mp_chmap *src)
{
    struct mp_chmap map = *src;
    mp_chmap_remove_na(&map);
    for (int n = 0; std_layout_names[n][0]; n++) {
        struct mp_chmap s;
        if (mp_chmap_from_str(&s, bstr0(std_layout_names[n][0])) &&
            mp_chmap_equals_reordered(&s, &map))
        {
            map = s;
            break;
        }
    }
    return mp_chmap_to_str_buf(buf, buf_size, &map);
}

void mp_chmap_print_help(struct mp_log *log)
{
    mp_info(log, "Speakers:\n");
    for (int n = 0; n < MP_SPEAKER_ID_COUNT; n++) {
        if (speaker_names[n][0])
            mp_info(log, "    %-16s (%s)\n",
                    speaker_names[n][0], speaker_names[n][1]);
    }
    mp_info(log, "Standard layouts:\n");
    for (int n = 0; std_layout_names[n][0]; n++) {
        mp_info(log, "    %-16s (%s)\n",
                 std_layout_names[n][0], std_layout_names[n][1]);
    }
    for (int n = 0; n < MP_NUM_CHANNELS; n++)
        mp_info(log, "    unknown%d\n", n + 1);
}
