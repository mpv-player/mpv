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

#include <limits.h>

#include "test_utils.h"

#include "common/common.h"
#include "misc/language.h"

#define LANGS(...) (char*[]){__VA_ARGS__, NULL}

int main(void)
{
    assert_int_equal(mp_match_lang(LANGS("fr-CA", "fr-FR")       , "fr-CA")  , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("fr-CA", "fr-FR")       , "fra")    , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("fr-CA", "fr-FR")       , "fre")    , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("fr-CA", "fr-FR")       , "fr-FR")  , INT_MAX - 1);
    assert_int_equal(mp_match_lang(LANGS("fr-FR", "fr")          , "fr-CA")  , INT_MAX - 1000);
    assert_int_equal(mp_match_lang(LANGS("fr", "fr-FR")          , "fr-CA")  , INT_MAX - 1000);
    assert_int_equal(mp_match_lang(LANGS("en", "fr-FR")          , "fr-CA")  , INT_MAX - 1000 - 1);
    assert_int_equal(mp_match_lang(LANGS("en", "fr-FR", "fr-CA") , "fr-CA")  , INT_MAX - 2);
    assert_int_equal(mp_match_lang(LANGS("fr-FR")                , "fr-CA")  , INT_MAX - 1000);
    assert_int_equal(mp_match_lang(LANGS("en", "fr-FR")          , "fr-CA")  , INT_MAX - 1000 - 1);
    assert_int_equal(mp_match_lang(LANGS("eng")                  , "eng")    , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("en")                   , "eng")    , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("eng")                  , "en")     , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("en")                   , "en")     , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("pt-BR", "pt-PT", "pt") , "pt-PT")  , INT_MAX - 1);
    assert_int_equal(mp_match_lang(LANGS("pt-BR", "en-US", "pt") , "pt-PT")  , INT_MAX - 1000);
    assert_int_equal(mp_match_lang(LANGS("pl-PL")                , "pol")    , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("pl-PL")                , "eng")    , 0);
    assert_int_equal(mp_match_lang(LANGS("gsw-u-sd-chzh") , "gsw-u-sd-chzh") , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("gsw-u-sd")      , "gsw-u-sd-chzh") , INT_MAX - 1000);
    assert_int_equal(mp_match_lang(LANGS("gsw-u-sd-chzh") , "gsw-u")         , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("ax")            , "en")            , 0);
    assert_int_equal(mp_match_lang(LANGS("en")            , "ax")            , 0);
    assert_int_equal(mp_match_lang(LANGS("ax")            , "ax")            , INT_MAX);
    assert_int_equal(mp_match_lang(LANGS("ax")            , "")              , 0);
    assert_int_equal(mp_match_lang(LANGS("ax")            , NULL)            , 0);
    assert_int_equal(mp_match_lang(LANGS("")              , "ax")            , 0);
    assert_int_equal(mp_match_lang((char*[]){NULL}        , "ax")            , 0);

    void *ta_ctx = talloc_new(NULL);

#define TEST_LANG_GUESS(filename, expected_lang, expected_start, expected_flags)  \
    do {                                                                          \
        int start;                                                                \
        enum track_flags flags;                                                   \
        bstr lang = mp_guess_lang_from_filename(bstr0(filename), &start, &flags); \
        assert_string_equal(bstrto0(ta_ctx, lang), expected_lang);                \
        assert_int_equal(start, expected_start);                                  \
        assert_int_equal(flags, expected_flags);                                  \
    } while (0)

    TEST_LANG_GUESS("foo.en.srt", "en", 3, 0);
    TEST_LANG_GUESS("foo.eng.srt", "eng", 3, 0);
    TEST_LANG_GUESS("foo.e.srt", "", -1, 0);
    TEST_LANG_GUESS("foo.engg.srt", "", -1, 0);
    TEST_LANG_GUESS("foo.00.srt", "", -1, 0);
    TEST_LANG_GUESS("foo.srt", "", -1, 0);
    TEST_LANG_GUESS(NULL, "", -1, 0);

    TEST_LANG_GUESS("foo.en-US.srt", "en-US", 3, 0);
    TEST_LANG_GUESS("foo.en-US.hi.srt", "en-US", 3, TRACK_HEARING_IMPAIRED);
    TEST_LANG_GUESS("foo.en-US.sdh.srt", "en-US", 3, TRACK_HEARING_IMPAIRED);
    TEST_LANG_GUESS("foo.en-US.forced.srt", "en-US", 3, TRACK_FORCED);
    TEST_LANG_GUESS("foo.en-US.forced.sdh.srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_FORCED);
    TEST_LANG_GUESS("foo.en-US.sdh.forced.srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_FORCED);
    TEST_LANG_GUESS("foo.en-US.default.srt", "en-US", 3, TRACK_DEFAULT);
    TEST_LANG_GUESS("foo.en-US.default.sdh.srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_DEFAULT);
    TEST_LANG_GUESS("foo.en-US.sdh.default.srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_DEFAULT);
    TEST_LANG_GUESS("foo.en-simple.srt", "en-simple", 3, 0);
    TEST_LANG_GUESS("foo.sgn-FSL.srt", "sgn-FSL", 3, 0);
    TEST_LANG_GUESS("foo.gsw-u-sd-chzh.srt", "gsw-u-sd-chzh", 3, 0);
    TEST_LANG_GUESS("foo.en-.srt", "", -1, 0);
    TEST_LANG_GUESS("foo.en-US-.srt", "", -1, 0);
    TEST_LANG_GUESS("foo.en-aaaaaaaaa.srt", "", -1, 0);
    TEST_LANG_GUESS("foo.en-0.srt", "", -1, 0);

    TEST_LANG_GUESS("foo[en].srt", "en", 3, 00);
    TEST_LANG_GUESS("foo[en-US].srt", "en-US", 3, 0);
    TEST_LANG_GUESS("foo[en-US][hi].srt", "en-US", 3, TRACK_HEARING_IMPAIRED);
    TEST_LANG_GUESS("foo[en-US][sdh].srt", "en-US", 3, TRACK_HEARING_IMPAIRED);
    TEST_LANG_GUESS("foo[en-US][forced].srt", "en-US", 3, TRACK_FORCED);
    TEST_LANG_GUESS("foo[en-US][forced][sdh].srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_FORCED);
    TEST_LANG_GUESS("foo[en-US][sdh][forced].srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_FORCED);
    TEST_LANG_GUESS("foo[en-US][default].srt", "en-US", 3, TRACK_DEFAULT);
    TEST_LANG_GUESS("foo[en-US][default][sdh].srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_DEFAULT);
    TEST_LANG_GUESS("foo[en-US][sdh][default].srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_DEFAULT);
    TEST_LANG_GUESS("foo[].srt", "", -1, 0);

    TEST_LANG_GUESS("foo(en).srt", "en", 3, 0);
    TEST_LANG_GUESS("foo(en-US).srt", "en-US", 3, 0);
    TEST_LANG_GUESS("foo(en-US)(hi).srt", "en-US", 3, TRACK_HEARING_IMPAIRED);
    TEST_LANG_GUESS("foo(en-US)(sdh).srt", "en-US", 3, TRACK_HEARING_IMPAIRED);
    TEST_LANG_GUESS("foo(en-US)(forced).srt", "en-US", 3, TRACK_FORCED);
    TEST_LANG_GUESS("foo(en-US)(forced)(sdh).srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_FORCED);
    TEST_LANG_GUESS("foo(en-US)(sdh)(forced).srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_FORCED);
    TEST_LANG_GUESS("foo(en-US)(default).srt", "en-US", 3, TRACK_DEFAULT);
    TEST_LANG_GUESS("foo(en-US)(default)(sdh).srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_DEFAULT);
    TEST_LANG_GUESS("foo(en-US)(sdh)(default).srt", "en-US", 3, TRACK_HEARING_IMPAIRED | TRACK_DEFAULT);
    TEST_LANG_GUESS("foo().srt", "", -1, 0);

    TEST_LANG_GUESS("foo.hi.forced.srt", "", -1, TRACK_HEARING_IMPAIRED | TRACK_FORCED);
    TEST_LANG_GUESS("foo.forced.hi.srt", "", -1, TRACK_HEARING_IMPAIRED | TRACK_FORCED);
    TEST_LANG_GUESS("foo.hi.default.srt", "", -1, TRACK_HEARING_IMPAIRED | TRACK_DEFAULT);
    TEST_LANG_GUESS("foo.default.hi.srt", "", -1, TRACK_HEARING_IMPAIRED | TRACK_DEFAULT);
    TEST_LANG_GUESS("foo.default.forced.srt", "", -1, TRACK_DEFAULT | TRACK_FORCED);
    TEST_LANG_GUESS("foo.hi.srt", "", -1, TRACK_HEARING_IMPAIRED);
    TEST_LANG_GUESS("foo.forced.srt", "", -1, TRACK_FORCED);
    TEST_LANG_GUESS("foo.default.srt", "", -1, TRACK_DEFAULT);

    talloc_free(ta_ctx);
}
