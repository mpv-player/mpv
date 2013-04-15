/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "osdep/getch2.h"
#include "osdep/io.h"

#ifdef CONFIG_GETTEXT
#include <locale.h>
#include <libintl.h>
#endif

#ifndef __MINGW32__
#include <signal.h>
#endif

#include "core/mp_msg.h"

bool mp_msg_stdout_in_use = 0;

/* maximum message length of mp_msg */
#define MSGSIZE_MAX 6144

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#define hSTDOUT GetStdHandle(STD_OUTPUT_HANDLE)
#define hSTDERR GetStdHandle(STD_ERROR_HANDLE)
static short stdoutAttrs = 0;
static const unsigned char ansi2win32[10] = {
    0,
    FOREGROUND_RED,
    FOREGROUND_GREEN,
    FOREGROUND_GREEN | FOREGROUND_RED,
    FOREGROUND_BLUE,
    FOREGROUND_BLUE  | FOREGROUND_RED,
    FOREGROUND_BLUE  | FOREGROUND_GREEN,
    FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED,
    FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED,
    FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED
};
#endif

int mp_msg_levels[MSGT_MAX]; // verbose level of this module. initialized to -2
int mp_msg_level_all = MSGL_STATUS;
int verbose = 0;
int mp_msg_color = 1;
int mp_msg_module = 0;
int mp_msg_cancolor = 0;

static int mp_msg_docolor(void) {
	return mp_msg_cancolor && mp_msg_color;
}

void mp_msg_init(void){
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    long cmode = 0;
    GetConsoleMode(hSTDOUT, &cmode);
    cmode |= (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
    SetConsoleMode(hSTDOUT, cmode);
    SetConsoleMode(hSTDERR, cmode);
    GetConsoleScreenBufferInfo(hSTDOUT, &cinfo);
    stdoutAttrs = cinfo.wAttributes;
#endif
#ifndef __MINGW32__
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTTOU, &sa, NULL); // just write to stdout if you have to
#endif
    int i;
    char *env = getenv("MPV_VERBOSE");
    if (env)
        verbose = atoi(env);
    for(i=0;i<MSGT_MAX;i++) mp_msg_levels[i] = -2;
    mp_msg_cancolor = isatty(fileno(stdout));
    mp_msg_levels[MSGT_IDENTIFY] = -1; // no -identify output by default
#ifdef CONFIG_GETTEXT
    textdomain("mpv");
    char *localedir = getenv("MPV_LOCALEDIR");
    if (localedir == NULL && strlen(MPLAYER_LOCALEDIR))
        localedir = MPLAYER_LOCALEDIR;
    bindtextdomain("mpv", localedir);
    bind_textdomain_codeset("mpv", "UTF-8");
#endif
}

int mp_msg_test(int mod, int lev)
{
#ifndef __MINGW32__
    if (lev == MSGL_STATUS) {
        // skip status line output if stderr is a tty but in background
        if (isatty(2) && tcgetpgrp(2) != getpgrp())
            return false;
    }
#endif
    return lev <= (mp_msg_levels[mod] == -2 ? mp_msg_level_all + verbose : mp_msg_levels[mod]);
}

static void set_msg_color(FILE* stream, int lev)
{
    static const int v_colors[10] = {9, 1, 3, 3, -1, -1, 2, 8, 8, 8};
    int c = v_colors[lev];
#ifdef MP_ANNOY_ME
    /* that's only a silly color test */
    {
        int c;
        static int flag = 1;
        if (flag)
            for(c = 0; c < 24; c++)
                printf("\033[%d;3%dm***  COLOR TEST %d  ***\n", c>7, c&7, c);
        flag = 0;
    }
#endif
    if (mp_msg_docolor())
    {
#ifdef _WIN32
        HANDLE *wstream = stream == stderr ? hSTDERR : hSTDOUT;
        if (c == -1)
            c = 7;
        SetConsoleTextAttribute(wstream, ansi2win32[c] | FOREGROUND_INTENSITY);
#else
        if (c == -1) {
            fprintf(stream, "\033[0m");
        } else {
            fprintf(stream, "\033[%d;3%dm", c >> 3, c & 7);
        }
#endif
    }
}

static void print_msg_module(FILE* stream, int mod)
{
    static const char *module_text[MSGT_MAX] = {
        "GLOBAL",
        "CPLAYER",
        "GPLAYER",
        "VIDEOOUT",
        "AUDIOOUT",
        "DEMUXER",
        "DS",
        "DEMUX",
        "HEADER",
        "AVSYNC",
        "AUTOQ",
        "CFGPARSER",
        "DECAUDIO",
        "DECVIDEO",
        "SEEK",
        "WIN32",
        "OPEN",
        "DVD",
        "PARSEES",
        "LIRC",
        "STREAM",
        "CACHE",
        "MENCODER",
        "XACODEC",
        "TV",
        "OSDEP",
        "SPUDEC",
        "PLAYTREE",
        "INPUT",
        "VFILTER",
        "OSD",
        "NETWORK",
        "CPUDETECT",
        "CODECCFG",
        "SWS",
        "VOBSUB",
        "SUBREADER",
        "AFILTER",
        "NETST",
        "MUXER",
        "OSDMENU",
        "IDENTIFY",
        "RADIO",
        "ASS",
        "LOADER",
        "STATUSLINE",
    };
    int c2 = (mod + 1) % 15 + 1;

    if (!mp_msg_module)
        return;
#ifdef _WIN32
    HANDLE *wstream = stream == stderr ? hSTDERR : hSTDOUT;
    if (mp_msg_docolor())
        SetConsoleTextAttribute(wstream, ansi2win32[c2&7] | FOREGROUND_INTENSITY);
    fprintf(stream, "%9s", module_text[mod]);
    if (mp_msg_docolor())
        SetConsoleTextAttribute(wstream, stdoutAttrs);
#else
    if (mp_msg_docolor())
        fprintf(stream, "\033[%d;3%dm", c2 >> 3, c2 & 7);
    fprintf(stream, "%9s", module_text[mod]);
    if (mp_msg_docolor())
        fprintf(stream, "\033[0;37m");
#endif
    fprintf(stream, ": ");
}

void mp_msg_va(int mod, int lev, const char *format, va_list va)
{
    char tmp[MSGSIZE_MAX];
    FILE *stream =
        (mp_msg_stdout_in_use || (lev == MSGL_STATUS)) ? stderr : stdout;
    static int header = 1;
    // indicates if last line printed was a status line
    static int statusline;

    if (!mp_msg_test(mod, lev)) return; // do not display
    vsnprintf(tmp, MSGSIZE_MAX, format, va);
    tmp[MSGSIZE_MAX-2] = '\n';
    tmp[MSGSIZE_MAX-1] = 0;

    /* A status line is normally intended to be overwritten by the next
     * status line, and does not end with a '\n'. If we're printing a normal
     * line instead after the status one print '\n' to change line. */
    if (statusline && lev != MSGL_STATUS)
        fprintf(stderr, "\n");
    statusline = lev == MSGL_STATUS;

    if (header)
        print_msg_module(stream, mod);
    set_msg_color(stream, lev);

    size_t len = strlen(tmp);
    header = len && (tmp[len-1] == '\n' || tmp[len-1] == '\r');

    fprintf(stream, "%s", tmp);

    if (mp_msg_docolor())
    {
#ifdef _WIN32
        HANDLE *wstream = lev <= MSGL_WARN ? hSTDERR : hSTDOUT;
        SetConsoleTextAttribute(wstream, stdoutAttrs);
#else
        fprintf(stream, "\033[0m");
#endif
    }
    fflush(stream);
}

void mp_msg(int mod, int lev, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    mp_msg_va(mod, lev, format, va);
    va_end(va);
}

char *mp_gtext(const char *string)
{
#ifdef CONFIG_GETTEXT
    /* gettext expects the global locale to be set with
     * setlocale(LC_ALL, ""). However doing that would suck for a
     * couple of reasons (locale stuff is badly designed and sucks in
     * general).
     *
     * First, setting the locale, especially LC_CTYPE, changes the
     * behavior of various C functions and we don't want that - we
     * want isalpha() for example to always behave like in the C
     * locale.

     * Second, there is no way to enforce a sane character set. All
     * strings inside MPlayer must always be in utf-8, not in the
     * character set specified by the system locale which could be
     * something different and completely insane. The locale system
     * lacks any way to say "set LC_CTYPE to utf-8, ignoring the
     * default system locale if it specifies something different". We
     * could try to work around that flaw by leaving LC_CTYPE to the C
     * locale and only setting LC_MESSAGES (which is the variable that
     * must be set to tell gettext which language to translate
     * to). However if we leave LC_MESSAGES set then things like
     * strerror() may produce completely garbled output when they try
     * to translate their results but then try to convert some
     * translated non-ASCII text to the character set specified by
     * LC_CTYPE which would still be in the C locale (this doesn't
     * affect gettext itself because it supports specifying the
     * character set directly with bind_textdomain_codeset()).
     *
     * So the only solution (at least short of trying to work around
     * things possibly producing non-utf-8 output) is to leave all the
     * locale variables unset. Note that this means it's not possible
     * to get translated output from any libraries we call if they
     * only rely on the broken locale system to specify the language
     * to use; this is the case with libc for example.
     *
     * The locale changing below is rather ugly, but hard to avoid.
     * gettext doesn't support specifying the translation target
     * directly, only through locale.
     * The main actual problem this could cause is interference with
     * other threads; that could be avoided with thread-specific
     * locale changes, but such functionality is less standard and I
     * think it's not worth adding pre-emptively unless someone sees
     * an actual problem case.
     */
    setlocale(LC_MESSAGES, "");
    string = gettext(string);
    setlocale(LC_MESSAGES, "C");
#endif
    return (char *)string;
}

void mp_tmsg(int mod, int lev, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    mp_msg_va(mod, lev, mp_gtext(format), va);
    va_end(va);
}
