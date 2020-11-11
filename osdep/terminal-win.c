/* Windows TermIO
 *
 * copyright (C) 2003 Sascha Sommer
 *
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

#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <io.h>
#include <pthread.h>
#include <assert.h>
#include "common/common.h"
#include "input/keycodes.h"
#include "input/input.h"
#include "terminal.h"
#include "osdep/io.h"
#include "osdep/threads.h"
#include "osdep/w32_keyboard.h"

// https://docs.microsoft.com/en-us/windows/console/setconsolemode
// These values are effective on Windows 10 build 16257 (August 2017) or later
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
    #define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif

// Note: the DISABLE_NEWLINE_AUTO_RETURN docs say it enables delayed-wrap, but
// it's wrong. It does only what its names suggests - and we want it unset:
// https://github.com/microsoft/terminal/issues/4126#issuecomment-571418661
static void attempt_native_out_vt(HANDLE hOut, DWORD basemode)
{
    DWORD vtmode = basemode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    vtmode &= ~DISABLE_NEWLINE_AUTO_RETURN;
    if (!SetConsoleMode(hOut, vtmode))
        SetConsoleMode(hOut, basemode);
}

static bool is_native_out_vt(HANDLE hOut)
{
    DWORD cmode;
    return GetConsoleMode(hOut, &cmode) &&
           (cmode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) &&
           !(cmode & DISABLE_NEWLINE_AUTO_RETURN);
}

#define hSTDOUT GetStdHandle(STD_OUTPUT_HANDLE)
#define hSTDERR GetStdHandle(STD_ERROR_HANDLE)

#define FOREGROUND_ALL (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define BACKGROUND_ALL (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE)

static short stdoutAttrs = 0;  // copied from the screen buffer on init
static const unsigned char ansi2win32[8] = {
    0,
    FOREGROUND_RED,
    FOREGROUND_GREEN,
    FOREGROUND_GREEN | FOREGROUND_RED,
    FOREGROUND_BLUE,
    FOREGROUND_BLUE  | FOREGROUND_RED,
    FOREGROUND_BLUE  | FOREGROUND_GREEN,
    FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED,
};
static const unsigned char ansi2win32bg[8] = {
    0,
    BACKGROUND_RED,
    BACKGROUND_GREEN,
    BACKGROUND_GREEN | BACKGROUND_RED,
    BACKGROUND_BLUE,
    BACKGROUND_BLUE  | BACKGROUND_RED,
    BACKGROUND_BLUE  | BACKGROUND_GREEN,
    BACKGROUND_BLUE  | BACKGROUND_GREEN | BACKGROUND_RED,
};

static bool running;
static HANDLE death;
static pthread_t input_thread;
static struct input_ctx *input_ctx;

void terminal_get_size(int *w, int *h)
{
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hOut, &cinfo)) {
        *w = cinfo.dwMaximumWindowSize.X - (is_native_out_vt(hOut) ? 0 : 1);
        *h = cinfo.dwMaximumWindowSize.Y;
    }
}

void terminal_get_size2(int *rows, int *cols, int *px_width, int *px_height)
{
}

static bool has_input_events(HANDLE h)
{
    DWORD num_events;
    if (!GetNumberOfConsoleInputEvents(h, &num_events))
        return false;
    return !!num_events;
}

static void read_input(HANDLE in)
{
    // Process any input events in the buffer
    while (has_input_events(in)) {
        INPUT_RECORD event;
        if (!ReadConsoleInputW(in, &event, 1, &(DWORD){0}))
            break;

        // Only key-down events are interesting to us
        if (event.EventType != KEY_EVENT)
            continue;
        KEY_EVENT_RECORD *record = &event.Event.KeyEvent;
        if (!record->bKeyDown)
            continue;

        UINT vkey = record->wVirtualKeyCode;
        bool ext = record->dwControlKeyState & ENHANCED_KEY;

        int mods = 0;
        if (record->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
            mods |= MP_KEY_MODIFIER_ALT;
        if (record->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
            mods |= MP_KEY_MODIFIER_CTRL;
        if (record->dwControlKeyState & SHIFT_PRESSED)
            mods |= MP_KEY_MODIFIER_SHIFT;

        int mpkey = mp_w32_vkey_to_mpkey(vkey, ext);
        if (mpkey) {
            mp_input_put_key(input_ctx, mpkey | mods);
        } else {
            // Only characters should be remaining
            int c = record->uChar.UnicodeChar;
            // The ctrl key always produces control characters in the console.
            // Shift them back up to regular characters.
            if (c > 0 && c < 0x20 && (mods & MP_KEY_MODIFIER_CTRL))
                c += (mods & MP_KEY_MODIFIER_SHIFT) ? 0x40 : 0x60;
            if (c >= 0x20)
                mp_input_put_key(input_ctx, c | mods);
        }
    }
}

static void *input_thread_fn(void *ptr)
{
    mpthread_set_name("terminal");
    HANDLE in = ptr;
    HANDLE stuff[2] = {in, death};
    while (1) {
        DWORD r = WaitForMultipleObjects(2, stuff, FALSE, INFINITE);
        if (r != WAIT_OBJECT_0)
            break;
        read_input(in);
    }
    return NULL;
}

void terminal_setup_getch(struct input_ctx *ictx)
{
    if (running)
        return;

    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    if (GetNumberOfConsoleInputEvents(in, &(DWORD){0})) {
        input_ctx = ictx;
        death = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!death)
            return;
        if (pthread_create(&input_thread, NULL, input_thread_fn, in)) {
            CloseHandle(death);
            return;
        }
        running = true;
    }
}

void terminal_uninit(void)
{
    if (running) {
        SetEvent(death);
        pthread_join(input_thread, NULL);
        input_ctx = NULL;
        running = false;
    }
}

bool terminal_in_background(void)
{
    return false;
}

void mp_write_console_ansi(HANDLE wstream, char *buf)
{
    wchar_t *wbuf = mp_from_utf8(NULL, buf);
    wchar_t *pos = wbuf;

    while (*pos) {
        if (is_native_out_vt(wstream)) {
            WriteConsoleW(wstream, pos, wcslen(pos), NULL, NULL);
            break;
        }
        wchar_t *next = wcschr(pos, '\033');
        if (!next) {
            WriteConsoleW(wstream, pos, wcslen(pos), NULL, NULL);
            break;
        }
        next[0] = '\0';
        WriteConsoleW(wstream, pos, wcslen(pos), NULL, NULL);
        if (next[1] == '[') {
            // CSI - Control Sequence Introducer
            next += 2;

            // CSI codes generally follow this syntax:
            //    "\033[" [ <i> (';' <i> )* ] <c>
            // where <i> are integers, and <c> a single char command code.
            // Also see: http://en.wikipedia.org/wiki/ANSI_escape_code#CSI_codes
            int params[16]; // 'm' might be unlimited; ignore that
            int num_params = 0;
            while (num_params < MP_ARRAY_SIZE(params)) {
                wchar_t *end = next;
                long p = wcstol(next, &end, 10);
                if (end == next)
                    break;
                next = end;
                params[num_params++] = p;
                if (next[0] != ';' || !next[0])
                    break;
                next += 1;
            }
            wchar_t code = next[0];
            if (code)
                next += 1;
            CONSOLE_SCREEN_BUFFER_INFO info;
            GetConsoleScreenBufferInfo(wstream, &info);
            switch (code) {
            case 'K': {     // erase to end of line
                COORD at = info.dwCursorPosition;
                int len = info.dwSize.X - at.X;
                FillConsoleOutputCharacterW(wstream, ' ', len, at, &(DWORD){0});
                SetConsoleCursorPosition(wstream, at);
                break;
            }
            case 'A': {     // cursor up
                info.dwCursorPosition.Y -= 1;
                SetConsoleCursorPosition(wstream, info.dwCursorPosition);
                break;
            }
            case 'm': {     // "SGR"
                short attr = info.wAttributes;
                if (num_params == 0)  // reset
                    params[num_params++] = 0;

                // we don't emulate italic, reverse/underline don't always work
                for (int n = 0; n < num_params; n++) {
                    int p = params[n];
                    if (p == 0) {
                        attr = stdoutAttrs;
                    } else if (p == 1) {
                        attr |= FOREGROUND_INTENSITY;
                    } else if (p == 22) {
                        attr &= ~FOREGROUND_INTENSITY;
                    } else if (p == 4) {
                        attr |= COMMON_LVB_UNDERSCORE;
                    } else if (p == 24) {
                        attr &= ~COMMON_LVB_UNDERSCORE;
                    } else if (p == 7) {
                        attr |= COMMON_LVB_REVERSE_VIDEO;
                    } else if (p == 27) {
                        attr &= ~COMMON_LVB_REVERSE_VIDEO;
                    } else if (p >= 30 && p <= 37) {
                        attr &= ~FOREGROUND_ALL;
                        attr |= ansi2win32[p - 30];
                    } else if (p == 39) {
                        attr &= ~FOREGROUND_ALL;
                        attr |= stdoutAttrs & FOREGROUND_ALL;
                    } else if (p >= 40 && p <= 47) {
                        attr &= ~BACKGROUND_ALL;
                        attr |= ansi2win32bg[p - 40];
                    } else if (p == 49) {
                        attr &= ~BACKGROUND_ALL;
                        attr |= stdoutAttrs & BACKGROUND_ALL;
                    } else if (p == 38 || p == 48) {  // ignore and skip sub-values
                        // 256 colors: <38/48>;5;N  true colors: <38/48>;2;R;G;B
                        if (n+1 < num_params) {
                            n += params[n+1] == 5 ? 2
                               : params[n+1] == 2 ? 4
                               : num_params;  /* unrecognized -> the rest */
                        }
                    }
                }

                if (attr != info.wAttributes)
                    SetConsoleTextAttribute(wstream, attr);
                break;
            }
            }
        } else if (next[1] == ']') {
            // OSC - Operating System Commands
            next += 2;

            // OSC sequences generally follow this syntax:
            //    "\033]" <command> ST
            // Where <command> is a string command
            wchar_t *cmd = next;
            while (next[0]) {
                // BEL can be used instead of ST in xterm
                if (next[0] == '\007' || next[0] == 0x9c) {
                    next[0] = '\0';
                    next += 1;
                    break;
                }
                if (next[0] == '\033' && next[1] == '\\') {
                    next[0] = '\0';
                    next += 2;
                    break;
                }
                next += 1;
            }

            // Handle xterm-style OSC commands
            if (cmd[0] && cmd[1] == ';') {
                wchar_t code = cmd[0];
                wchar_t *param = cmd + 2;

                switch (code) {
                case '0': // Change Icon Name and Window Title
                case '2': // Change Window Title
                    SetConsoleTitleW(param);
                    break;
                }
            }
        } else {
            WriteConsoleW(wstream, L"\033", 1, NULL, NULL);
        }
        pos = next;
    }

    talloc_free(wbuf);
}

static bool is_a_console(HANDLE h)
{
    return GetConsoleMode(h, &(DWORD){0});
}

static void reopen_console_handle(DWORD std, int fd, FILE *stream)
{
    HANDLE handle = GetStdHandle(std);
    if (is_a_console(handle)) {
        if (fd == 0) {
            freopen("CONIN$", "rt", stream);
        } else {
            freopen("CONOUT$", "wt", stream);
        }
        setvbuf(stream, NULL, _IONBF, 0);

        // Set the low-level FD to the new handle value, since mp_subprocess2
        // callers might rely on low-level FDs being set. Note, with this
        // method, fileno(stdin) != STDIN_FILENO, but that shouldn't matter.
        int unbound_fd = -1;
        if (fd == 0) {
             unbound_fd = _open_osfhandle((intptr_t)handle, _O_RDONLY);
        } else {
             unbound_fd = _open_osfhandle((intptr_t)handle, _O_WRONLY);
        }
        // dup2 will duplicate the underlying handle. Don't close unbound_fd,
        // since that will close the original handle.
        dup2(unbound_fd, fd);
    }
}

bool terminal_try_attach(void)
{
    // mpv.exe is a flagged as a GUI application, but it acts as a console
    // application when started from the console wrapper (see
    // osdep/win32-console-wrapper.c). The console wrapper sets
    // _started_from_console=yes, so check that variable before trying to
    // attach to the console.
    wchar_t console_env[4] = { 0 };
    if (!GetEnvironmentVariableW(L"_started_from_console", console_env, 4))
        return false;
    if (wcsncmp(console_env, L"yes", 4))
        return false;
    SetEnvironmentVariableW(L"_started_from_console", NULL);

    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return false;

    // We have a console window. Redirect input/output streams to that console's
    // low-level handles, so things that use stdio work later on.
    reopen_console_handle(STD_INPUT_HANDLE, STDIN_FILENO, stdin);
    reopen_console_handle(STD_OUTPUT_HANDLE, STDOUT_FILENO, stdout);
    reopen_console_handle(STD_ERROR_HANDLE, STDERR_FILENO, stderr);

    return true;
}

void terminal_init(void)
{
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    DWORD cmode = 0;
    GetConsoleMode(hSTDOUT, &cmode);
    cmode |= (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
    attempt_native_out_vt(hSTDOUT, cmode);
    attempt_native_out_vt(hSTDERR, cmode);
    GetConsoleScreenBufferInfo(hSTDOUT, &cinfo);
    stdoutAttrs = cinfo.wAttributes;
}
