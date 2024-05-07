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

#include <inttypes.h>
#include <libmpv/client.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <sys/types.h>
#include <io.h>
#else
#include <unistd.h>
#endif

// Stolen from osdep/compiler.h
#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format(printf, a1, a2)))
#define MP_NORETURN __attribute__((noreturn))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#define MP_NORETURN
#endif

// Broken crap with __USE_MINGW_ANSI_STDIO
#if defined(__MINGW32__) && defined(__GNUC__) && !defined(__clang__)
#undef PRINTF_ATTRIBUTE
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (gnu_printf, a1, a2)))
#endif

// Global handle
static mpv_handle *ctx;
// Temporary output file
static const char *out_path;

static void exit_cleanup(void)
{
    if (ctx)
        mpv_destroy(ctx);
    if (out_path)
        unlink(out_path);
}

MP_NORETURN PRINTF_ATTRIBUTE(1, 2)
static void fail(const char *fmt, ...)
{
    if (fmt) {
        va_list va;
        va_start(va, fmt);
        vfprintf(stderr, fmt, va);
        va_end(va);
    }
    exit(1);
}

static void check_api_error(int status)
{
    if (status < 0)
        fail("libmpv error: %s\n", mpv_error_string(status));
}

static void wait_done(void)
{
    while (1) {
        mpv_event *ev = mpv_wait_event(ctx, -1.0);
        if (ev->event_id == MPV_EVENT_NONE)
            continue;
        printf("event: %s\n", mpv_event_name(ev->event_id));
        if (ev->event_id == MPV_EVENT_SHUTDOWN)
            return;
    }
}

static void check_output(int fd)
{
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 100)
        fail("did not encode anything");

    char magic[4] = {0};
    lseek(fd, 0, SEEK_SET);
    read(fd, magic, sizeof(magic));
    static const char ebml_magic[] = {26, 69, 223, 163};
    if (memcmp(magic, ebml_magic, 4) != 0)
        fail("output was not Matroska");

    puts("output file ok");
}

int mp_mkostemps(char *template, int suffixlen, int flags);

int main(int argc, char *argv[])
{
    atexit(exit_cleanup);

    ctx = mpv_create();
    if (!ctx)
        return 1;

    int fd;
    {
        char path[] = "./testout.XXXXXX";
        fd = mp_mkostemps(path, 0, 0);
        if (fd == -1)
            fail("mkstemp failed");
        out_path = strdup(path);
    }
    check_api_error(mpv_set_option_string(ctx, "o", out_path));
    check_api_error(mpv_set_option_string(ctx, "of", "matroska"));
    check_api_error(mpv_set_option_string(ctx, "end", "1.5"));

    if (mpv_initialize(ctx) != 0)
        return 1;

    check_api_error(mpv_set_option_string(ctx, "terminal", "yes"));
    check_api_error(mpv_set_option_string(ctx, "msg-level", "all=v"));
    check_api_error(mpv_set_option_string(ctx, "idle", "once"));

    const char *cmd[] = {"loadfile", "av://lavfi:testsrc", NULL};
    check_api_error(mpv_command(ctx, cmd));

    wait_done();
    mpv_destroy(ctx);
    ctx = NULL;

    check_output(fd);
    close(fd);

    return 0;
}
