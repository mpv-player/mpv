#include "config.h"

#include <stdio.h>
#include <string.h>

#include <libavutil/mem.h>

#include "audio/chmap.h"
#include "audio/chmap_avchannel.h"
#include "audio/chmap_sel.h"
#include "test_utils.h"

#define LAYOUTS(...) (char*[]){__VA_ARGS__, NULL}

static void test_sel(const char *input, const char *expected_selection,
                     char **layouts)
{
    struct mp_chmap_sel s = {0};
    struct mp_chmap input_map;
    struct mp_chmap expected_map;

    assert_true(mp_chmap_from_str(&input_map, bstr0(input)));
    assert_true(mp_chmap_from_str(&expected_map, bstr0(expected_selection)));

    for (int n = 0; layouts[n]; n++) {
        struct mp_chmap tmp;
        assert_true(mp_chmap_from_str(&tmp, bstr0(layouts[n])));
        int count = s.num_chmaps;
        mp_chmap_sel_add_map(&s, &tmp);
        assert_true(s.num_chmaps > count); // assure validity and max. count
    }

    assert_true(mp_chmap_sel_fallback(&s, &input_map));
    // We convert expected_map to a chmap and then back to a string to avoid
    // problems with ambiguous layouts.
    assert_string_equal(mp_chmap_to_str(&input_map),
                        mp_chmap_to_str(&expected_map));
}

static bool channels_match(const AVChannelLayout *av_layout, const struct mp_chmap *mp_layout)
{
    for (int n = 0; n < mp_layout->num; n++) {
        enum AVChannel id = av_channel_layout_channel_from_index(av_layout, n);
        int sp = mp_layout->speaker[n];
        if (id != (sp == MP_SPEAKER_ID_NA ? AV_CHAN_UNUSED : sp))
            return false;
    }
    return true;
}

static bool native_order(const AVChannelLayout *layout)
{
    AVChannelLayout native = {0};
    uint64_t mask = av_channel_layout_subset(layout, ~(uint64_t)0);
    return av_channel_layout_from_mask(&native, mask) >= 0 &&
           !av_channel_layout_compare(&native, layout);
}

static void custom_layout(AVChannelLayout *dst, const char *names)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", names);
    enum AVChannel ids[MP_NUM_CHANNELS + 1];
    int num = 0;
    for (char *s = strtok(buf, "+"); s; s = strtok(NULL, "+")) {
        enum AVChannel id = !strcmp(s, "UNSD") ? AV_CHAN_UNUSED :
                            !strcmp(s, "UNK") ? AV_CHAN_UNKNOWN :
                            av_channel_from_string(s);
        assert_true(id != AV_CHAN_NONE && num < MP_ARRAY_SIZE(ids));
        ids[num++] = id;
    }
    av_channel_layout_uninit(dst);
    dst->order = AV_CHANNEL_ORDER_CUSTOM;
    dst->nb_channels = num;
    dst->u.map = av_calloc(num, sizeof(*dst->u.map));
    assert_true(dst->u.map);
    for (int n = 0; n < num; n++)
        dst->u.map[n].id = ids[n];
}

static uint32_t rnd(uint32_t *state, uint32_t range)
{
    *state = *state * 1664525 + 1013904223;
    return (*state >> 8) % range;
}

// Distinct speakers out of all 64 IDs, some or all channels NA, in native
// order (NA last) or shuffled.
static void random_map(struct mp_chmap *map, uint32_t *state)
{
    uint8_t ids[64];
    for (int n = 0; n < 64; n++)
        ids[n] = n;
    map->num = 1 + rnd(state, rnd(state, 2) ? 8 : MP_NUM_CHANNELS);
    int na = rnd(state, 3) ? 0 : 1 + rnd(state, map->num);
    for (int n = 0; n < map->num; n++) {
        int k = n + rnd(state, 64 - n);
        MPSWAP(uint8_t, ids[n], ids[k]);
        map->speaker[n] = n < na ? MP_SPEAKER_ID_NA : ids[n];
    }
    if (rnd(state, 2)) {
        for (int n = 1; n < map->num; n++) {
            for (int i = n; i > 0 && map->speaker[i - 1] > map->speaker[i]; i--)
                MPSWAP(uint8_t, map->speaker[i - 1], map->speaker[i]);
        }
    } else {
        for (int n = map->num - 1; n > 0; n--) {
            int k = rnd(state, n + 1);
            MPSWAP(uint8_t, map->speaker[n], map->speaker[k]);
        }
    }
    assert_true(mp_chmap_is_valid(map));
}

static void test_mp_chmap_to_av_layout_exact(void)
{
    static const char *const cases[][2] = {
        {"fc-fl-fr-sl-sr-lfe",              "FC+FL+FR+SL+SR+LFE"},
        {"fr-br-fc-fl-lfe-sr-bl-sl",        "FR+BR+FC+FL+LFE+SR+BL+SL"},
        {"lfe2-tsr-bfl-wl-sdr-dl-tbc-bfc",  "LFE2+TSR+BFL+WL+SDR+DL+TBC+BFC"},
        {"7.1(alsa)",                       "FL+FR+BL+BR+FC+LFE+SL+SR"},
        {"5.1(side)",                       "FL+FR+FC+LFE+SL+SR"},
        {"fl-fr-na",                        "FL+FR+UNSD"},
        {"na-fl-fr",                        "UNSD+FL+FR"},
        {"fl-na-fr-na-na",                  "FL+UNSD+FR+UNSD+UNSD"},
    };

    for (int i = 0; i < MP_ARRAY_SIZE(cases); i++) {
        struct mp_chmap mp_layout;
        AVChannelLayout av_layout = {0}, expected = {0};
        assert_true(mp_chmap_from_str(&mp_layout, bstr0(cases[i][0])));
        custom_layout(&expected, cases[i][1]);
        mp_chmap_to_av_layout(&av_layout, &mp_layout);
        assert_true(av_channel_layout_check(&av_layout));
        assert_int_equal(av_channel_layout_compare(&av_layout, &expected), 0);
        assert_int_equal(av_layout.order, native_order(&expected) ?
                         AV_CHANNEL_ORDER_NATIVE : AV_CHANNEL_ORDER_CUSTOM);
        av_channel_layout_uninit(&av_layout);
        av_channel_layout_uninit(&expected);
    }

    uint32_t state = 1;
    for (int i = 0; i < 100000; i++) {
        struct mp_chmap mp_layout;
        random_map(&mp_layout, &state);
        AVChannelLayout av_layout = {0};
        mp_chmap_to_av_layout(&av_layout, &mp_layout);
        assert_true(av_channel_layout_check(&av_layout));
        assert_int_equal(av_layout.nb_channels, mp_layout.num);
        if (mp_chmap_is_unknown(&mp_layout)) {
            assert_int_equal(av_layout.order, AV_CHANNEL_ORDER_UNSPEC);
        } else {
            assert_true(channels_match(&av_layout, &mp_layout));
            assert_int_equal(av_layout.order, native_order(&av_layout) ?
                             AV_CHANNEL_ORDER_NATIVE : AV_CHANNEL_ORDER_CUSTOM);

            struct mp_chmap back;
            assert_true(mp_chmap_from_av_layout(&back, &av_layout));
            assert_true(mp_chmap_equals(&back, &mp_layout));

            // Unknown channels in place of the unused ones are rejected.
            bool unknown = false;
            if (av_layout.order == AV_CHANNEL_ORDER_CUSTOM) {
                for (int n = 0; n < av_layout.nb_channels; n++) {
                    if (av_layout.u.map[n].id == AV_CHAN_UNUSED) {
                        av_layout.u.map[n].id = AV_CHAN_UNKNOWN;
                        unknown = true;
                    }
                }
            }
            assert_int_equal(mp_chmap_from_av_layout(&back, &av_layout), !unknown);
            assert_true(unknown ? mp_chmap_is_empty(&back) : mp_chmap_equals(&back, &mp_layout));
        }
        av_channel_layout_uninit(&av_layout);
    }
}

static void test_custom_av_layout_to_mp_chmap(void)
{
    // FFmpeg channels, mpv map or NULL if there is none
    static const char *const cases[][2] = {
        {"FC+FL+FR+SL+SR+LFE",              "fc-fl-fr-sl-sr-lfe"},
        {"FR+BR+FC+FL+LFE+SR+BL+SL",        "fr-br-fc-fl-lfe-sr-bl-sl"},
        {"LFE2+TSR+BFL+WL+SDR+DL+TBC+BFC",  "lfe2-tsr-bfl-wl-sdr-dl-tbc-bfc"},
        {"FL+FR+FC+LFE+SL+SR",              "5.1(side)"},
        {"UNSD+FL+FR",                      "na-fl-fr"},
        {"FL+UNSD+FR+UNSD",                 "fl-na-fr-na"},
        {"FL+UNK+FR",                       NULL},
        {"UNK+UNK+UNK",                     NULL},
        {"FL+FR+AMBI0",                     NULL},
        {"FL+FR+FL",                        NULL},
    };

    for (int i = 0; i < MP_ARRAY_SIZE(cases); i++) {
        AVChannelLayout av_layout = {0};
        custom_layout(&av_layout, cases[i][0]);
        struct mp_chmap mp_layout, expected = {0};
        if (cases[i][1])
            assert_true(mp_chmap_from_str(&expected, bstr0(cases[i][1])));
        assert_int_equal(mp_chmap_from_av_layout(&mp_layout, &av_layout), !!cases[i][1]);
        assert_true(mp_chmap_equals(&mp_layout, &expected));
        av_channel_layout_uninit(&av_layout);
    }

    // More channels than a map has.
    char names[(MP_NUM_CHANNELS + 1) * 5] = "";
    for (int n = 0; n <= MP_NUM_CHANNELS; n++)
        mp_snprintf_cat(names, sizeof(names), "%sUNSD", n ? "+" : "");
    AVChannelLayout av_layout = {0};
    custom_layout(&av_layout, names);
    struct mp_chmap mp_layout;
    assert_false(mp_chmap_from_av_layout(&mp_layout, &av_layout));
    assert_true(mp_chmap_is_empty(&mp_layout));
    av_channel_layout_uninit(&av_layout);
}

static bool layout_matches(const AVChannelLayout *av_layout,
                           const struct mp_chmap *mp_layout,
                           bool require_default_unspec)
{
    if (!mp_chmap_is_valid(mp_layout) ||
        !av_channel_layout_check(av_layout) ||
        av_layout->nb_channels != mp_layout->num ||
        mp_layout->num > MP_NUM_CHANNELS)
        return false;

    switch (av_layout->order) {
    case AV_CHANNEL_ORDER_UNSPEC:
    {
        if (!require_default_unspec)
            return true;

        // mp_chmap essentially does not have a concept of "unspecified"
        // so we check if the mp layout matches the default layout for such
        // channel count.
        struct mp_chmap default_layout = { 0 };
        mp_chmap_from_channels(&default_layout, mp_layout->num);
        return mp_chmap_equals(mp_layout, &default_layout);
    }
    case AV_CHANNEL_ORDER_NATIVE:
        return av_layout->u.mask == mp_chmap_to_lavc(mp_layout);
    case AV_CHANNEL_ORDER_CUSTOM:
        return channels_match(av_layout, mp_layout);
    default:
        return false;
    }

    return true;
}

static void test_mp_chmap_to_av_channel_layout(void)
{
    mp_ch_layout_tuple *mapping_array = NULL;
    void *iter = NULL;
    bool anything_failed = false;

    printf("Testing mp_chmap -> AVChannelLayout conversions\n");

    while ((mapping_array = mp_iterate_builtin_layouts(&iter))) {
        const char *mapping_name = (*mapping_array)[0];
        const char *mapping_str  = (*mapping_array)[1];
        struct mp_chmap mp_layout = { 0 };
        AVChannelLayout av_layout = { 0 };
        char layout_desc[128] = {0};

        assert_true(mp_chmap_from_str(&mp_layout, bstr0(mapping_str)));

        mp_chmap_to_av_layout(&av_layout, &mp_layout);

        assert_false(av_channel_layout_describe(&av_layout,
                                                layout_desc, 128) < 0);

        bool success =
            (strcmp(layout_desc, mp_chmap_to_str(&mp_layout)) == 0 ||
             layout_matches(&av_layout, &mp_layout, false));
        if (!success)
            anything_failed = true;

        printf("%s: %s (%s) -> %s\n",
               success ? "Success" : "Failure",
               mapping_str, mapping_name, layout_desc);

        av_channel_layout_uninit(&av_layout);
    }

    assert_false(anything_failed);
}

static void test_av_channel_layout_to_mp_chmap(void)
{
    const AVChannelLayout *av_layout = NULL;
    void *iter = NULL;
    bool anything_failed = false;

    printf("Testing AVChannelLayout -> mp_chmap conversions\n");

    while ((av_layout = av_channel_layout_standard(&iter))) {
        struct mp_chmap mp_layout = { 0 };
        char layout_desc[128] = {0};

        assert_false(av_channel_layout_describe(av_layout,
                                                layout_desc, 128) < 0);

        bool ret = mp_chmap_from_av_layout(&mp_layout, av_layout);
        if (!ret) {
            bool too_many_channels =
                av_layout->nb_channels > MP_NUM_CHANNELS;
            printf("Conversion from '%s' to mp_chmap failed (%s)!\n",
                   layout_desc,
                   too_many_channels ?
                   "channel count was over max, ignoring" :
                   "unexpected, failing");

            // we should for now only fail with things such as 22.2
            // due to mp_chmap being currently limited to 16 channels
            assert_true(too_many_channels);

            continue;
        }

        bool success =
            (strcmp(layout_desc, mp_chmap_to_str(&mp_layout)) == 0 ||
             layout_matches(av_layout, &mp_layout, true));
        if (!success)
            anything_failed = true;

        printf("%s: %s -> %s\n",
               success ? "Success" : "Failure",
               layout_desc, mp_chmap_to_str(&mp_layout));
    }

    assert_false(anything_failed);
}

int main(void)
{
    struct mp_chmap a;
    struct mp_chmap b;
    struct mp_chmap_sel s = {0};

    test_sel("5.1", "7.1", LAYOUTS("7.1"));
    test_sel("7.1", "5.1", LAYOUTS("5.1"));
    test_sel("7.1(wide-side)", "7.1", LAYOUTS("7.1"));
    test_sel("7.1(wide-side)", "5.1(side)", LAYOUTS("7.1", "5.1(side)"));
    test_sel("3.1", "5.1", LAYOUTS("7.1", "5.1", "2.1", "stereo", "mono"));
    test_sel("5.1", "7.1(rear)", LAYOUTS("7.1(rear)"));
    test_sel("5.1(side)", "5.1", LAYOUTS("5.1", "7.1"));
    test_sel("5.1", "7.1(alsa)", LAYOUTS("7.1(alsa)"));
    test_sel("mono", "stereo", LAYOUTS("stereo", "5.1"));
    test_sel("stereo", "stereo", LAYOUTS("stereo", "5.1"));
    test_sel("5.1(side)", "7.1(rear)", LAYOUTS("stereo", "7.1(rear)"));
    test_sel("7.1", "fl-fr-lfe-fc-bl-br-flc-frc",
             LAYOUTS("fl-fr-lfe-fc-bl-br-flc-frc", "3.0(back)"));

    mp_chmap_set_unknown(&a, 2);

    mp_chmap_from_str(&b, bstr0("5.1"));

    mp_chmap_sel_add_map(&s, &a);
    assert_false(mp_chmap_sel_fallback(&s, &b));
    assert_string_equal(mp_chmap_to_str(&b), "5.1");

    test_sel("quad", "quad(side)", LAYOUTS("quad(side)", "stereo"));
    test_sel("quad", "quad(side)", LAYOUTS("quad(side)", "7.0"));
    test_sel("quad", "quad(side)", LAYOUTS("7.0", "quad(side)"));
    test_sel("quad", "7.1(wide-side)", LAYOUTS("7.1(wide-side)", "stereo"));
    test_sel("quad", "7.1(wide-side)", LAYOUTS("stereo", "7.1(wide-side)"));
    test_sel("quad", "fl-fr-sl-sr",
             LAYOUTS("fl-fr-fc-bl-br", "fl-fr-sl-sr"));
    test_sel("quad", "fl-fr-bl-br-na-na-na-na",
             LAYOUTS("fl-fr-bl-br-na-na-na-na", "quad(side)", "stereo"));
    test_sel("quad", "fl-fr-bl-br-na-na-na-na",
             LAYOUTS("stereo", "quad(side)", "fl-fr-bl-br-na-na-na-na"));
    test_sel("fl-fr-fc-lfe-sl-sr", "fl-fr-lfe-fc-bl-br-na-na",
             LAYOUTS("fl-fr-lfe-fc-bl-br-na-na", "fl-fr-lfe-fc-bl-br-sdl-sdr"));
    test_sel("fl-fr-fc-lfe-sl-sr", "fl-fr-lfe-fc-bl-br-na-na",
             LAYOUTS("fl-fr-lfe-fc-bl-br-sdl-sdr", "fl-fr-lfe-fc-bl-br-na-na"));

    test_sel("na-fl-fr", "na-fl-fr", LAYOUTS("na-fl-fr-na", "fl-na-fr", "na-fl-fr",
                                             "fl-fr-na-na", "na-na-fl-fr"));

    mp_chmap_from_str(&a, bstr0("3.1"));
    mp_chmap_from_str(&b, bstr0("2.1"));

    assert_int_equal(mp_chmap_diffn(&a, &b), 1);

    mp_chmap_from_str(&b, bstr0("6.1(back)"));
    assert_int_equal(mp_chmap_diffn(&a, &b), 0);
    assert_int_equal(mp_chmap_diffn(&b, &a), 3);

    test_av_channel_layout_to_mp_chmap();
    test_mp_chmap_to_av_channel_layout();
    test_mp_chmap_to_av_layout_exact();
    test_custom_av_layout_to_mp_chmap();

    return 0;
}
