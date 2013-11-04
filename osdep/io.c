/*
 * unicode/utf-8 I/O helpers and wrappers for Windows
 *
 * This file is part of mplayer2.
 * Contains parts based on libav code (http://libav.org).
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _WIN32

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stddef.h>

#include "osdep/io.h"
#include "talloc.h"

//copied and modified from libav
//http://git.libav.org/?p=libav.git;a=blob;f=libavformat/os_support.c;h=a0fcd6c9ba2be4b0dbcc476f6c53587345cc1152;hb=HEADl30

wchar_t *mp_from_utf8(void *talloc_ctx, const char *s)
{
    int count = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (count <= 0)
        abort();
    wchar_t *ret = talloc_array(talloc_ctx, wchar_t, count);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, ret, count);
    return ret;
}

char *mp_to_utf8(void *talloc_ctx, const wchar_t *s)
{
    int count = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    if (count <= 0)
        abort();
    char *ret = talloc_array(talloc_ctx, char, count);
    WideCharToMultiByte(CP_UTF8, 0, s, -1, ret, count, NULL, NULL);
    return ret;
}

#endif // _WIN32

#ifdef __MINGW32__

#include <io.h>
#include <fcntl.h>

#if HAVE_PTHREADS
#include <pthread.h>
#endif

#include "mpvcore/mp_talloc.h"

//http://git.libav.org/?p=libav.git;a=blob;f=cmdutils.c;h=ade3f10ce2fc030e32e375a85fbd06c26d43a433#l161

static char** win32_argv_utf8;
static int win32_argc;

void mp_get_converted_argv(int *argc, char ***argv)
{
    if (!win32_argv_utf8) {
        win32_argc = 0;
        wchar_t **argv_w = CommandLineToArgvW(GetCommandLineW(), &win32_argc);
        if (win32_argc <= 0 || !argv_w)
            return;

        win32_argv_utf8 = talloc_zero_array(NULL, char*, win32_argc + 1);

        for (int i = 0; i < win32_argc; i++) {
            win32_argv_utf8[i] = mp_to_utf8(NULL, argv_w[i]);
        }

        LocalFree(argv_w);
    }

    *argc = win32_argc;
    *argv = win32_argv_utf8;
}

int mp_stat(const char *path, struct stat *buf)
{
    wchar_t *wpath = mp_from_utf8(NULL, path);
    int res = _wstati64(wpath, buf);
    talloc_free(wpath);
    return res;
}

static int mp_vfprintf(FILE *stream, const char *format, va_list args)
{
    int done = 0;

    if (stream == stdout || stream == stderr)
    {
        HANDLE *wstream = GetStdHandle(stream == stdout ?
                                       STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
        if (wstream != INVALID_HANDLE_VALUE)
        {
            // figure out whether we're writing to a console
            unsigned int filetype = GetFileType(wstream);
            if (!((filetype == FILE_TYPE_UNKNOWN) &&
                (GetLastError() != ERROR_SUCCESS)))
            {
                int isConsole;
                filetype &= ~(FILE_TYPE_REMOTE);
                if (filetype == FILE_TYPE_CHAR)
                {
                    DWORD ConsoleMode;
                    int ret = GetConsoleMode(wstream, &ConsoleMode);
                    if (!ret && (GetLastError() == ERROR_INVALID_HANDLE))
                        isConsole = 0;
                    else
                        isConsole = 1;
                }
                else
                    isConsole = 0;

                if (isConsole)
                {
                    int nchars = vsnprintf(NULL, 0, format, args) + 1;
                    char *buf = talloc_array(NULL, char, nchars);
                    if (buf)
                    {
                        vsnprintf(buf, nchars, format, args);
                        wchar_t *out = mp_from_utf8(NULL, buf);
                        size_t nchars = wcslen(out);
                        talloc_free(buf);
                        done = WriteConsoleW(wstream, out, nchars, NULL, NULL);
                        talloc_free(out);
                    }
                }
                else
                    done = vfprintf(stream, format, args);
            }
        }
    }
    else
        done = vfprintf(stream, format, args);

    return done;
}

int mp_fprintf(FILE *stream, const char *format, ...)
{
    int res;
    va_list args;
    va_start(args, format);
    res = mp_vfprintf(stream, format, args);
    va_end(args);
    return res;
}

int mp_printf(const char *format, ...)
{
    int res;
    va_list args;
    va_start(args, format);
    res = mp_vfprintf(stdout, format, args);
    va_end(args);
    return res;
}

int mp_open(const char *filename, int oflag, ...)
{
    int mode = 0;
    if (oflag & _O_CREAT) {
        va_list va;
        va_start(va, oflag);
        mode = va_arg(va, int);
        va_end(va);
    }
    wchar_t *wpath = mp_from_utf8(NULL, filename);
    int res = _wopen(wpath, oflag, mode);
    talloc_free(wpath);
    return res;
}

int mp_creat(const char *filename, int mode)
{
    return open(filename, O_CREAT|O_WRONLY|O_TRUNC, mode);
}

FILE *mp_fopen(const char *filename, const char *mode)
{
    wchar_t *wpath = mp_from_utf8(NULL, filename);
    wchar_t *wmode = mp_from_utf8(wpath, mode);
    FILE *res = _wfopen(wpath, wmode);
    talloc_free(wpath);
    return res;
}

struct mp_dir {
    DIR crap;   // must be first member
    _WDIR *wdir;
    union {
        struct dirent dirent;
        // dirent has space only for FILENAME_MAX bytes. _wdirent has space for
        // FILENAME_MAX wchar_t, which might end up bigger as UTF-8 in some
        // cases. Guarantee we can always hold _wdirent.d_name converted to
        // UTF-8 (see MP_PATH_MAX).
        // This works because dirent.d_name is the last member of dirent.
        char space[MP_PATH_MAX];
    };
};

DIR* mp_opendir(const char *path)
{
    wchar_t *wpath = mp_from_utf8(NULL, path);
    _WDIR *wdir = _wopendir(wpath);
    talloc_free(wpath);
    if (!wdir)
        return NULL;
    struct mp_dir *mpdir = talloc(NULL, struct mp_dir);
    // DIR is supposed to be opaque, but unfortunately the MinGW headers still
    // define it. Make sure nobody tries to use it.
    memset(&mpdir->crap, 0xCD, sizeof(mpdir->crap));
    mpdir->wdir = wdir;
    return (DIR*)mpdir;
}

struct dirent* mp_readdir(DIR *dir)
{
    struct mp_dir *mpdir = (struct mp_dir*)dir;
    struct _wdirent *wdirent = _wreaddir(mpdir->wdir);
    if (!wdirent)
        return NULL;
    size_t buffersize = sizeof(mpdir->space) - offsetof(struct dirent, d_name);
    WideCharToMultiByte(CP_UTF8, 0, wdirent->d_name, -1, mpdir->dirent.d_name,
                        buffersize, NULL, NULL);
    mpdir->dirent.d_ino = 0;
    mpdir->dirent.d_reclen = 0;
    mpdir->dirent.d_namlen = strlen(mpdir->dirent.d_name);
    return &mpdir->dirent;
}

int mp_closedir(DIR *dir)
{
    struct mp_dir *mpdir = (struct mp_dir*)dir;
    int res = _wclosedir(mpdir->wdir);
    talloc_free(mpdir);
    return res;
}

int mp_mkdir(const char *path, int mode)
{
    wchar_t *wpath = mp_from_utf8(NULL, path);
    int res = _wmkdir(wpath);
    talloc_free(wpath);
    return res;
}

static char **utf8_environ;
static void *utf8_environ_ctx;

static void free_env(void)
{
    talloc_free(utf8_environ_ctx);
    utf8_environ_ctx = NULL;
    utf8_environ = NULL;
}

// Note: UNIX getenv() returns static strings, and we try to do the same. Since
// using putenv() is not multithreading safe, we don't expect env vars to change
// at runtime, and converting/allocating them in advance is ok.
static void init_getenv(void)
{
    if (utf8_environ_ctx)
        return;
    wchar_t *wenv = GetEnvironmentStringsW();
    if (!wenv)
        return;
    utf8_environ_ctx = talloc_new(NULL);
    int num_env = 0;
    while (1) {
        size_t len = wcslen(wenv);
        if (!len)
            break;
        char *s = mp_to_utf8(utf8_environ_ctx, wenv);
        MP_TARRAY_APPEND(utf8_environ_ctx, utf8_environ, num_env, s);
        wenv += len + 1;
    }
    MP_TARRAY_APPEND(utf8_environ_ctx, utf8_environ, num_env, NULL);
    // Avoid showing up in leak detectors etc.
    atexit(free_env);
}

char *mp_getenv(const char *name)
{
#if HAVE_PTHREADS
    static pthread_once_t once_init_getenv = PTHREAD_ONCE_INIT;
    pthread_once(&once_init_getenv, init_getenv);
#else
    init_getenv();
#endif
    // Copied from musl, http://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
    // Copyright © 2005-2013 Rich Felker, standard MIT license
    int i;
    size_t l = strlen(name);
    if (!utf8_environ || !*name || strchr(name, '=')) return NULL;
    for (i=0; utf8_environ[i] && (strncmp(name, utf8_environ[i], l)
            || utf8_environ[i][l] != '='); i++) {}
    if (utf8_environ[i]) return utf8_environ[i] + l+1;
    return NULL;
}

#endif // __MINGW32__
