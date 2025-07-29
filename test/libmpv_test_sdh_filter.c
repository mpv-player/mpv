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

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "libmpv_common.h"

#ifndef F_OK
#define F_OK 0
#endif

static void test_sdh_filter(char *file, char *path, char *harder)
{
    int ret = access(path, F_OK);
    if (ret)
        fail("Test file, '%s', was not found!\n", path);

    check_api_error(mpv_set_property_string(ctx, "sub-filter-sdh", "yes"));
    if (harder)
        check_api_error(mpv_set_property_string(ctx, "sub-filter-sdh-harder", "yes"));

    char *expect = "";
    if (strcmp(file, "sdh_default") == 0 && harder) {
        expect = "Filter from string.";
    } else if (strcmp(file, "sdh_default") == 0) {
        expect = "Filter (things) from （random） string.";
    } else if (strcmp(file, "sdh_mixed") == 0 && harder) {
        expect = "漢字 and وotherو non-ASCII characters、";
    } else if (strcmp(file, "sdh_mixed") == 0) {
        expect = "all: 漢字 and وotherو non-ASCII characters、（together）";
    } else if (strcmp(file, "sdh_all") == 0 && harder) {
        expect = "";
    } else if (strcmp(file, "sdh_all") == 0) {
        expect = "(this)";
    } else if (strcmp(file, "sdh_mismatch") == 0 && harder) {
        expect = "(No]　（filter) (should） (be） [applied)";
    } else if (strcmp(file, "sdh_mismatch") == 0) {
        expect = "(No]　（filter) (should） (be） [applied)";
    }

    reload_file(path);
    bool sub_text = false;
    while (!sub_text) {
        mpv_event *event = wrap_wait_event();
        switch (event->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            mpv_event_property *prop = event->data;
            if (strcmp(prop->name, "sub-text") == 0) {
                char *value = *(char **)(prop->data);
                if (!value || strcmp(expect, value) != 0)
                    fail("String: expected '%s' but got '%s'!\n", expect, value);
                sub_text = true;
            }
            break;
        }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
        return 1;

    ctx = mpv_create();
    if (!ctx)
        return 1;

    atexit(exit_cleanup);

    initialize();

    const char *fmt = "================ TEST: %s %s ================\n";
    printf(fmt, "test_sdh_filter", argv[1]);
    mpv_observe_property(ctx, 0, "sub-text", MPV_FORMAT_STRING);
    test_sdh_filter(argv[1], argv[2], argv[3]);
    printf("================ SHUTDOWN ================\n");

    mpv_command_string(ctx, "quit");
    while (wrap_wait_event()->event_id != MPV_EVENT_SHUTDOWN) {}

    return 0;
}
