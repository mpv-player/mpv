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

#include <unistd.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include "libmpv_common.h"

static bool have_english_locale(void)
{
#ifdef __APPLE__
    CFArrayRef arr = CFLocaleCopyPreferredLanguages();
    if (!arr)
        return false;
    CFIndex count = CFArrayGetCount(arr);
    if (!count)
        return false;

    bool ret = false;
    CFRange range = CFRangeMake(0, 2);
    CFStringRef en = CFStringCreateWithCString(NULL, "en", kCFStringEncodingMacRoman);
    for (CFIndex i = 0; i < count; i++) {
        CFStringRef cfstr = CFArrayGetValueAtIndex(arr, i);
        if (CFStringCompareWithOptions(cfstr, en, range, 0) == 0) {
            ret = true;
            break;
        }
    }
    CFRelease(en);
    return ret;
#endif

#ifdef _WIN32
    ULONG count = 0;
    ULONG size = 0;
    wchar_t buf[1024];

    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, buf, &size)) {
        for (ULONG pos = 0; buf[pos]; pos += wcslen(buf + pos) + 1) {
            if (wcsncmp(buf + pos, L"en", 2) == 0)
                return true;
        }
    }

    if (GetSystemPreferredUILanguages(MUI_LANGUAGE_NAME, &count, buf, &size)) {
        for (ULONG pos = 0; buf[pos]; pos += wcslen(buf + pos) + 1) {
            if (wcsncmp(buf + pos, L"en", 2) == 0)
                return true;
        }
    }

    return false;
#endif
    return true;
}

static void check_string(const char *property, const char *expect)
{
    char *result_string;
    check_api_error(mpv_get_property(ctx, property, MPV_FORMAT_STRING, &result_string));
    if (strcmp(expect, result_string) != 0)
        fail("String: expected '%s' but got '%s'!\n", expect, result_string);
    mpv_free(result_string);
}

static void reload_file(const char *path)
{
    const char *cmd[] = {"loadfile", path, NULL};
    check_api_error(mpv_command(ctx, cmd));
    bool loaded = false;
    while (!loaded) {
        mpv_event *event = wrap_wait_event();
        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED:
            loaded = true;
            break;
        }
    }
}

static void test_track_selection(char *file, char *path)
{
    int ret = access(path, F_OK);
    if (ret)
        fail("Test file, '%s', was not found!\n", path);

    if (strcmp(file, "eng_default.mkv") == 0) {
        // --no-config
        reload_file(path);
        check_string("current-tracks/sub/selected", "yes");

        // --subs-falback=no
        check_api_error(mpv_set_property_string(ctx, "subs-fallback", "no"));
        reload_file(path);
        check_string("track-list/2/selected", "no");
    } else if (strcmp(file, "eng_default_forced.mkv") == 0) {
        // --subs-fallback-forced=no
        check_api_error(mpv_set_property_string(ctx, "subs-fallback-forced", "no"));
        reload_file(path);
        check_string("current-tracks/sub/selected", "yes");
    } else if (strcmp(file, "eng_forced_matching_audio.mkv") == 0) {
        // select forced track
        reload_file(path);
        check_string("current-tracks/sub/selected", "yes");
    } else if (strcmp(file, "eng_forced_no_matching_audio.mkv") == 0) {
        // forced track should not be selected
        reload_file(path);
        check_string("track-list/2/selected", "no");
    } else if (strcmp(file, "eng_forced_always_audio.mkv") == 0) {
        // forced track should be selected anyway despite no matching audio
        check_api_error(mpv_set_property_string(ctx, "subs-fallback-forced", "always"));
        reload_file(path);
        check_string("current-tracks/sub/selected", "yes");
    } else if (strcmp(file, "eng_no_default.mkv") == 0) {
        // track should not be selected
        reload_file(path);
        check_string("track-list/2/selected", "no");

        // --subs-fallback=yes
        check_api_error(mpv_set_property_string(ctx, "subs-fallback", "yes"));
        reload_file(path);
        check_string("current-tracks/sub/selected", "yes");
    } else if (strcmp(file, "locale.mkv") == 0) {
        // default english subs
        reload_file(path);
        check_string("current-tracks/sub/lang", "eng");

        // default german subs
        check_api_error(mpv_set_property_string(ctx, "subs-match-os-language", "no"));
        reload_file(path);
        check_string("current-tracks/sub/lang", "ger");
    } else if (strcmp(file, "multilang.mkv") == 0) {
        // --alang=jpn should select forced jpn subs
        check_api_error(mpv_set_property_string(ctx, "alang", "jpn"));
        reload_file(path);
        check_string("current-tracks/audio/lang", "jpn");
        check_string("current-tracks/sub/lang", "jpn");

        // --alang=pol should select default, non-forced ger subs
        check_api_error(mpv_set_property_string(ctx, "alang", "pol"));
        reload_file(path);
        check_string("current-tracks/audio/lang", "pol");
        check_string("current-tracks/sub/lang", "ger");

        // --slang=eng and --subs-with-matching-audio should not pick any subs
        check_api_error(mpv_set_property_string(ctx, "alang", "eng"));
        check_api_error(mpv_set_property_string(ctx, "slang", "eng"));
        check_api_error(mpv_set_property_string(ctx, "subs-with-matching-audio", "no"));
        reload_file(path);
        check_string("current-tracks/audio/lang", "eng");
        check_string("track-list/5/selected", "no");
        check_string("track-list/6/selected", "no");
        check_string("track-list/7/selected", "no");
        check_string("track-list/8/selected", "no");

        // --subs-with-matching-audio=forced checks
        check_api_error(mpv_set_property_string(ctx, "subs-with-matching-audio", "forced"));
        reload_file(path);
        check_string("current-tracks/audio/lang", "eng");
        check_string("current-tracks/sub/lang", "eng");

        // forced jpn subs should be selected
        check_api_error(mpv_set_property_string(ctx, "alang", "jpn"));
        check_api_error(mpv_set_property_string(ctx, "slang", "jpn"));
        reload_file(path);
        check_string("current-tracks/audio/lang", "jpn");
        check_string("current-tracks/sub/lang", "jpn");

        // default+forced eng subs should be selected
        check_api_error(mpv_set_property_string(ctx, "alang", "ger"));
        check_api_error(mpv_set_property_string(ctx, "slang", "ger"));
        reload_file(path);
        check_string("current-tracks/audio/lang", "ger");
        check_string("current-tracks/sub/lang", "eng");

        // eng audio and pol subs should be selected
        check_api_error(mpv_set_property_string(ctx, "alang", "it"));
        check_api_error(mpv_set_property_string(ctx, "slang", "pt,it,pol,ger"));
        reload_file(path);
        check_string("current-tracks/audio/lang", "eng");
        check_string("current-tracks/sub/lang", "pol");

        // forced jpn subs should be selected
        check_api_error(mpv_set_property_string(ctx, "alang", "ger"));
        check_api_error(mpv_set_property_string(ctx, "slang", "jpn,pol"));
        check_api_error(mpv_set_property_string(ctx, "subs-with-matching-audio", "yes"));
        check_api_error(mpv_set_property_string(ctx, "subs-fallback-forced", "always"));
        reload_file(path);
        check_string("current-tracks/audio/lang", "ger");
        check_string("current-tracks/sub/lang", "jpn");
    } else if (strcmp(file, "multilang2.mkv") == 0) {
        // default jpn subs
        check_api_error(mpv_set_property_string(ctx, "subs-match-os-language", "no"));
        check_api_error(mpv_set_property_string(ctx, "alang", "jpn"));
        reload_file(path);
        check_string("track-list/3/selected", "yes");

        // forced eng subs
        check_api_error(mpv_set_property_string(ctx, "alang", "eng"));
        reload_file(path);
        check_string("track-list/4/selected", "yes");

        // default jpn subs
        check_api_error(mpv_set_property_string(ctx, "subs-fallback-forced", "no"));
        reload_file(path);
        check_string("track-list/3/selected", "yes");

        // default eng subs
        check_api_error(mpv_set_property_string(ctx, "slang", "eng"));
        reload_file(path);
        check_string("track-list/6/selected", "yes");

        // no subs
        check_api_error(mpv_set_property_string(ctx, "slang", ""));
        check_api_error(mpv_set_property_string(ctx, "subs-fallback", "no"));
        reload_file(path);
        check_string("track-list/3/selected", "no");
        check_string("track-list/4/selected", "no");
        check_string("track-list/5/selected", "no");
        check_string("track-list/6/selected", "no");

        // untagged eng subs
        check_api_error(mpv_set_property_string(ctx, "sid", "3"));
        reload_file(path);
        check_string("track-list/5/selected", "yes");
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
        return 1;

    if (!strcmp(argv[1], "locale.mkv") && !have_english_locale()) {
        printf("Non English language detected. Skipping locale test.\n");
        return 77;
    }

    ctx = mpv_create();
    if (!ctx)
        return 1;

    atexit(exit_cleanup);

    initialize();

    const char *fmt = "================ TEST: %s %s ================\n";
    printf(fmt, "test_track_selection", argv[1]);
    test_track_selection(argv[1], argv[2]);
    printf("================ SHUTDOWN ================\n");

    mpv_command_string(ctx, "quit");
    while (wrap_wait_event()->event_id != MPV_EVENT_SHUTDOWN) {}

    return 0;
}
