/*
 * unicode/utf-8 I/O helpers and wrappers for Windows
 *
 * Contains parts based on libav code (http://libav.org).
 *
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

#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "talloc.h"

#include "config.h"
#include "osdep/io.h"
#include "osdep/terminal.h"

// Set the CLOEXEC flag on the given fd.
// On error, false is returned (and errno set).
bool mp_set_cloexec(int fd)
{
#if defined(F_SETFD)
    if (fd >= 0) {
        int flags = fcntl(fd, F_GETFD);
        if (flags == -1)
            return false;
        if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
            return false;
    }
#endif
    return true;
}

#ifdef __MINGW32__
int mp_make_cloexec_pipe(int pipes[2])
{
    pipes[0] = pipes[1] = -1;
    return -1;
}
#else
int mp_make_cloexec_pipe(int pipes[2])
{
    if (pipe(pipes) != 0) {
        pipes[0] = pipes[1] = -1;
        return -1;
    }

    for (int i = 0; i < 2; i++)
        mp_set_cloexec(pipes[i]);
    return 0;
}
#endif

#ifdef __MINGW32__
int mp_make_wakeup_pipe(int pipes[2])
{
    return mp_make_cloexec_pipe(pipes);
}
#else
// create a pipe, and set it to non-blocking (and also set FD_CLOEXEC)
int mp_make_wakeup_pipe(int pipes[2])
{
    if (mp_make_cloexec_pipe(pipes) < 0)
        return -1;

    for (int i = 0; i < 2; i++) {
        int val = fcntl(pipes[i], F_GETFL) | O_NONBLOCK;
        fcntl(pipes[i], F_SETFL, val);
    }
    return 0;
}
#endif

#ifdef _WIN32

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stddef.h>

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
#include <pthread.h>

static void copy_stat(struct mp_stat *dst, struct _stat64 *src)
{
    dst->st_dev = src->st_dev;
    dst->st_ino = src->st_ino;
    dst->st_mode = src->st_mode;
    dst->st_nlink = src->st_nlink;
    dst->st_uid = src->st_uid;
    dst->st_gid = src->st_gid;
    dst->st_rdev = src->st_rdev;
    dst->st_size = src->st_size;
    dst->st_atime = src->st_atime;
    dst->st_mtime = src->st_mtime;
    dst->st_ctime = src->st_ctime;
}

int mp_stat(const char *path, struct mp_stat *buf)
{
    struct _stat64 buf_;
    wchar_t *wpath = mp_from_utf8(NULL, path);
    int res = _wstat64(wpath, &buf_);
    talloc_free(wpath);
    copy_stat(buf, &buf_);
    return res;
}

int mp_fstat(int fd, struct mp_stat *buf)
{
    struct _stat64 buf_;
    int res = _fstat64(fd, &buf_);
    copy_stat(buf, &buf_);
    return res;
}

static int mp_check_console(HANDLE wstream)
{
    if (wstream != INVALID_HANDLE_VALUE) {
        unsigned int filetype = GetFileType(wstream);

        if (!((filetype == FILE_TYPE_UNKNOWN) &&
            (GetLastError() != ERROR_SUCCESS)))
        {
            filetype &= ~(FILE_TYPE_REMOTE);

            if (filetype == FILE_TYPE_CHAR) {
                DWORD ConsoleMode;
                int ret = GetConsoleMode(wstream, &ConsoleMode);

                if (!(!ret && (GetLastError() == ERROR_INVALID_HANDLE))) {
                    // This seems to be a console
                    return 1;
                }
            }
        }
    }

    return 0;
}

static int mp_vfprintf(FILE *stream, const char *format, va_list args)
{
    int done = 0;

    HANDLE wstream = INVALID_HANDLE_VALUE;

    if (stream == stdout || stream == stderr) {
        wstream = GetStdHandle(stream == stdout ?
                               STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    }

    if (mp_check_console(wstream)) {
        size_t len = vsnprintf(NULL, 0, format, args) + 1;
        char *buf = talloc_array(NULL, char, len);

        if (buf) {
            done = vsnprintf(buf, len, format, args);
            mp_write_console_ansi(wstream, buf);
        }
        talloc_free(buf);
    } else {
        done = vfprintf(stream, format, args);
    }

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

// Windows' MAX_PATH/PATH_MAX/FILENAME_MAX is fixed to 260, but this limit
// applies to unicode paths encoded with wchar_t (2 bytes on Windows). The UTF-8
// version could end up bigger in memory. In the worst case each wchar_t is
// encoded to 3 bytes in UTF-8, so in the worst case we have:
//      wcslen(wpath) * 3 <= strlen(utf8path)
// Thus we need MP_PATH_MAX as the UTF-8/char version of PATH_MAX.
// Also make sure there's free space for the terminating \0.
// (For codepoints encoded as UTF-16 surrogate pairs, UTF-8 has the same length.)
#define MP_PATH_MAX (FILENAME_MAX * 3 + 1)

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

FILE *mp_tmpfile(void)
{
    // Reserve a file name in the format %TMP%\mpvXXXX.TMP
    wchar_t tmp_path[MAX_PATH + 1];
    if (!GetTempPathW(MAX_PATH + 1, tmp_path))
        return NULL;
    wchar_t tmp_name[MAX_PATH + 1];
    if (!GetTempFileNameW(tmp_path, L"mpv", 0, tmp_name))
        return NULL;

    // Create the file. FILE_ATTRIBUTE_TEMPORARY indicates the file will be
    // short-lived. Windows should avoid flushing it to disk while there is
    // sufficient cache.
    HANDLE file = CreateFileW(tmp_name, GENERIC_READ | GENERIC_WRITE | DELETE,
        FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DeleteFileW(tmp_name);
        return NULL;
    }

    int fd = _open_osfhandle((intptr_t)file, 0);
    if (fd < 0) {
        CloseHandle(file);
        return NULL;
    }
    FILE *fp = fdopen(fd, "w+b");
    if (!fp) {
        close(fd);
        return NULL;
    }

    return fp;
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
    static pthread_once_t once_init_getenv = PTHREAD_ONCE_INIT;
    pthread_once(&once_init_getenv, init_getenv);
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

off_t mp_lseek(int fd, off_t offset, int whence)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_DISK) {
        errno = ESPIPE;
        return (off_t)-1;
    }
    return _lseeki64(fd, offset, whence);
}

// Limited mmap() wrapper, inspired by:
// http://code.google.com/p/mman-win32/source/browse/trunk/mman.c

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    assert(addr == NULL); // not implemented
    assert(flags == MAP_SHARED); // not implemented

    HANDLE osf = (HANDLE)_get_osfhandle(fd);
    if (!osf) {
        errno = EBADF;
        return MAP_FAILED;
    }

    DWORD protect = 0;
    DWORD access = 0;
    if (prot & PROT_WRITE) {
        protect = PAGE_READWRITE;
        access = FILE_MAP_WRITE;
    } else if (prot & PROT_READ) {
        protect = PAGE_READONLY;
        access = FILE_MAP_READ;
    }

    DWORD l_low = (uint32_t)length;
    DWORD l_high = ((uint64_t)length) >> 32;
    HANDLE map = CreateFileMapping(osf, NULL, protect, l_high, l_low, NULL);

    if (!map) {
        errno = EACCES; // something random
        return MAP_FAILED;
    }

    DWORD o_low = (uint32_t)offset;
    DWORD o_high = ((uint64_t)offset) >> 32;
    void *p = MapViewOfFile(map, access, o_high, o_low, length);

    CloseHandle(map);

    if (!p) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    return p;
}

int munmap(void *addr, size_t length)
{
    UnmapViewOfFile(addr);
    return 0;
}

int msync(void *addr, size_t length, int flags)
{
    FlushViewOfFile(addr, length);
    return 0;
}

#endif // __MINGW32__
