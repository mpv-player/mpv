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

#include "core/mp_msg.h"
#include "chmap.h"

// Names taken from libavutil/channel_layout.c (Not accessible by API.)
// Use of these names is hard-coded in some places (e.g. ao_alsa.c)
static const char *speaker_names[MP_SPEAKER_ID_COUNT][2] = {
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
};

// Names taken from libavutil/channel_layout.c (Not accessible by API.)
// Channel order corresponds to lavc/waveex, except for the alsa entries.
static const char *std_layout_names[][2] = {
    {"empty",           ""}, // not in lavc
    {"mono",            "fc"},
    {"stereo",          "fl-fr"},
    {"2.1",             "fl-fr-lfe"},
    {"3.0",             "fl-fr-fc"},
    {"3.0(back)",       "fl-fr-bc"},
    {"4.0",             "fl-fr-fc-bc"},
    {"quad",            "fl-fr-bl-br"},
    {"quad(side)",      "fl-fr-sl-sr"},
    {"3.1",             "fl-fr-fc-lfe"},
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
    {"6.1(front)",      "fl-fr-lfe-flc-frc-sl-sr"},
    {"7.0",             "fl-fr-fc-bl-br-sl-sr"},
    {"7.0(front)",      "fl-fr-fc-flc-frc-sl-sr"},
    {"7.1",             "fl-fr-fc-lfe-bl-br-sl-sr"},
    {"7.1(alsa)",       "fl-fr-bl-br-fc-lfe-sl-sr"}, // not in lavc
    {"7.1(wide)",       "fl-fr-fc-lfe-bl-br-flc-frc"},
    {"7.1(wide-side)",  "fl-fr-fc-lfe-flc-frc-sl-sr"},
    {"octagonal",       "fl-fr-fc-bl-br-bc-sl-sr"},
    {"downmix",         "dl-dr"},
    {0}
};

static const struct mp_chmap default_layouts[MP_NUM_CHANNELS + 1] = {
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

// The channel order was lavc/waveex, but differs from lavc for 5, 6 and 8
// channels. 3 and 7 channels were likely undefined (no ALSA support).
// I'm not sure about the 4 channel case: ALSA uses "quad", while the ffmpeg
// default layout is "4.0".
static const char *mplayer_layouts[MP_NUM_CHANNELS + 1] = {
    [1] = "mono",
    [2] = "stereo",
    [4] = "quad",
    [5] = "5.0(alsa)",
    [6] = "5.1(alsa)",
    [8] = "7.1(alsa)",
};

// Returns true if speakers are mapped uniquely, and there's at least 1 channel.
bool mp_chmap_is_valid(const struct mp_chmap *src)
{
    bool mapped[MP_SPEAKER_ID_COUNT] = {0};
    for (int n = 0; n < src->num; n++) {
        int sp = src->speaker[n];
        if (sp >= MP_SPEAKER_ID_COUNT || mapped[sp])
            return false;
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
        int speaker = src->speaker[n];
        if (speaker >= MP_SPEAKER_ID_UNKNOWN0 &&
            speaker <= MP_SPEAKER_ID_UNKNOWN_LAST)
            return true;
    }
    return false;
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

bool mp_chmap_is_compatible(const struct mp_chmap *a, const struct mp_chmap *b)
{
    if (mp_chmap_equals(a, b))
        return true;
    if (a->num == b->num && (mp_chmap_is_unknown(a) || mp_chmap_is_unknown(b)))
        return true;
    return false;
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

// Set *dst to a standard layout with the given number of channels.
// If the number of channels is invalid, an invalid map is set, and
// mp_chmap_is_valid(dst) will return false.
void mp_chmap_from_channels(struct mp_chmap *dst, int num_channels)
{
    if (num_channels < 0 || num_channels > MP_NUM_CHANNELS) {
        *dst = (struct mp_chmap) {0};
    } else {
        *dst = default_layouts[num_channels];
    }
}

// Try to do what mplayer/mplayer2/mpv did before channel layouts were
// introduced, i.e. get the old default channel order.
void mp_chmap_from_channels_alsa(struct mp_chmap *dst, int num_channels)
{
    if (num_channels < 0 || num_channels > MP_NUM_CHANNELS) {
        *dst = (struct mp_chmap) {0};
    } else {
        mp_chmap_from_str(dst, bstr0(mplayer_layouts[num_channels]));
        if (!dst->num)
            mp_chmap_from_channels(dst, num_channels);
    }
}

// Set *dst to an unknown layout for the given numbers of channels.
// If the number of channels is invalid, an invalid map is set, and
// mp_chmap_is_valid(dst) will return false.
void mp_chmap_set_unknown(struct mp_chmap *dst, int num_channels)
{
    if (num_channels < 0 || num_channels > MP_NUM_CHANNELS) {
        *dst = (struct mp_chmap) {0};
    } else {
        dst->num = num_channels;
        for (int n = 0; n < dst->num; n++)
            dst->speaker[n] = MP_SPEAKER_ID_UNKNOWN0 + n;
    }
}

// Return channel index of the given speaker, or -1.
static int mp_chmap_find_speaker(const struct mp_chmap *map, int speaker)
{
    for (int n = 0; n < map->num; n++) {
        if (map->speaker[n] == speaker)
            return n;
    }
    return -1;
}

static void mp_chmap_remove_speaker(struct mp_chmap *map, int speaker)
{
    int index = mp_chmap_find_speaker(map, speaker);
    if (index >= 0) {
        for (int n = index; n < map->num - 1; n++)
            map->speaker[n] = map->speaker[n + 1];
        map->num--;
    }
}

// Some decoders output additional, redundant channels, which are usually
// useless and will mess up proper audio output channel handling.
// map: channel map from which the channels should be removed
// requested: if not NULL, and if it contains any of the "useless" channels,
//            don't remove them (this is for convenience)
void mp_chmap_remove_useless_channels(struct mp_chmap *map,
                                      const struct mp_chmap *requested)
{
    if (requested &&
        mp_chmap_find_speaker(requested, MP_SPEAKER_ID_DL) >= 0)
        return;

    if (map->num > 2) {
        mp_chmap_remove_speaker(map, MP_SPEAKER_ID_DL);
        mp_chmap_remove_speaker(map, MP_SPEAKER_ID_DR);
    }
}

// Return the ffmpeg/libav channel layout as in <libavutil/channel_layout.h>.
// Warning: this ignores the order of the channels, and will return a channel
//          mask even if the order is different from libavcodec's.
uint64_t mp_chmap_to_lavc_unchecked(const struct mp_chmap *src)
{
    // lavc has no concept for unknown layouts yet, so pick a default
    struct mp_chmap t = *src;
    if (mp_chmap_is_unknown(&t))
        mp_chmap_from_channels(&t, t.num);
    uint64_t mask = 0;
    for (int n = 0; n < t.num; n++)
        mask |= 1ULL << t.speaker[n];
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

void mp_chmap_reorder_to_lavc(struct mp_chmap *map)
{
    if (!mp_chmap_is_valid(map))
        return;
    uint64_t mask = mp_chmap_to_lavc_unchecked(map);
    mp_chmap_from_lavc(map, mask);
}

// Get reordering array for from->to reordering. from->to must have the same set
// of speakers (i.e. same number and speaker IDs, just different order). Then,
// for each speaker n, dst[n] will be set such that:
//      to->speaker[dst[n]] = from->speaker[n]
// (dst[n] gives the source channel for destination channel n)
void mp_chmap_get_reorder(int dst[MP_NUM_CHANNELS], const struct mp_chmap *from,
                          const struct mp_chmap *to)
{
    assert(from->num == to->num);
    if (mp_chmap_is_unknown(from) || mp_chmap_is_unknown(to)) {
        for (int n = 0; n < from->num; n++)
            dst[n] = n;
        return;
    }
    // Same set of speakers required
    assert(mp_chmap_equals_reordered(from, to));
    for (int n = 0; n < from->num; n++) {
        int src = from->speaker[n];
        dst[n] = -1;
        for (int i = 0; i < to->num; i++) {
            if (src == to->speaker[i]) {
                dst[n] = i;
                break;
            }
        }
        assert(dst[n] != -1);
    }
    for (int n = 0; n < from->num; n++)
        assert(to->speaker[dst[n]] == from->speaker[n]);
}

// Returns something like "fl-fr-fc". If there's a standard layout in lavc
// order, return that, e.g. "3.0" instead of "fl-fr-fc".
// Unassigned but valid speakers get names like "sp28".
char *mp_chmap_to_str(const struct mp_chmap *src)
{
    char *res = talloc_strdup(NULL, "");

    if (mp_chmap_is_unknown(src))
        return talloc_asprintf_append_buffer(res, "unknown%d", src->num);

    for (int n = 0; n < src->num; n++) {
        int sp = src->speaker[n];
        const char *s = sp < MP_SPEAKER_ID_COUNT ? speaker_names[sp][0] : NULL;
        char buf[10];
        if (!s) {
            snprintf(buf, sizeof(buf), "sp%d", sp);
            s = buf;
        }
        res = talloc_asprintf_append_buffer(res, "%s%s", n > 0 ? "-" : "", s);
    }

    // To standard layout name
    for (int n = 0; std_layout_names[n][0]; n++) {
        if (res && strcmp(res, std_layout_names[n][1]) == 0) {
            talloc_free(res);
            res = talloc_strdup(NULL, std_layout_names[n][0]);
            break;
        }
    }

    return res;
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

void mp_chmap_print_help(int msgt, int msgl)
{
    mp_msg(msgt, msgl, "Speakers:\n");
    for (int n = 0; n < MP_SPEAKER_ID_COUNT; n++) {
        if (speaker_names[n][0])
            mp_msg(msgt, msgl, "    %-16s (%s)\n",
                   speaker_names[n][0], speaker_names[n][1]);
    }
    mp_msg(msgt, msgl, "Standard layouts:\n");
    for (int n = 0; std_layout_names[n][0]; n++) {
        mp_msg(msgt, msgl, "    %-16s (%s)\n",
               std_layout_names[n][0], std_layout_names[n][1]);
    }
    for (int n = 0; n < MP_NUM_CHANNELS; n++)
        mp_msg(msgt, msgl, "    unknown%d\n", n);
}
