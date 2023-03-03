#include "audio/chmap.h"
#include "audio/chmap_sel.h"
#include "config.h"
#include "test_utils.h"

#if HAVE_AV_CHANNEL_LAYOUT
#include "audio/chmap_avchannel.h"
#endif

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

#if HAVE_AV_CHANNEL_LAYOUT
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
    default:
        // TODO: handle custom layouts
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
#endif


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

#if HAVE_AV_CHANNEL_LAYOUT
    test_av_channel_layout_to_mp_chmap();
    test_mp_chmap_to_av_channel_layout();
#endif
    return 0;
}
