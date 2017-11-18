/*
 * unicode/utf-8 I/O helpers and wrappers for Windows
 *
 * Contains parts based on libav code (http://libav.org).
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

#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "mpv_talloc.h"

#include "config.h"
#include "osdep/io.h"
#include "osdep/terminal.h"

#if HAVE_UWP
// Missing from MinGW headers.
#include <windows.h>
WINBASEAPI UINT WINAPI GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpPrefixString,
                                        UINT uUnique, LPWSTR lpTempFileName);
WINBASEAPI DWORD WINAPI GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer);
WINBASEAPI DWORD WINAPI GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength,
                                         LPWSTR lpBuffer, LPWSTR *lpFilePart);
#endif

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

void mp_flush_wakeup_pipe(int pipe_end)
{
#ifndef __MINGW32__
    char buf[100];
    (void)read(pipe_end, buf, sizeof(buf));
#endif
}

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

static void set_errno_from_lasterror(void)
{
    // This just handles the error codes expected from CreateFile at the moment
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND:
        errno = ENOENT;
        break;
    case ERROR_SHARING_VIOLATION:
    case ERROR_ACCESS_DENIED:
        errno = EACCES;
        break;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        errno = EEXIST;
        break;
    case ERROR_PIPE_BUSY:
        errno = EAGAIN;
        break;
    default:
        errno = EINVAL;
        break;
    }
}

static time_t filetime_to_unix_time(int64_t wintime)
{
    static const int64_t hns_per_second = 10000000ll;
    static const int64_t win_to_unix_epoch = 11644473600ll;
    return wintime / hns_per_second - win_to_unix_epoch;
}

static bool get_file_ids_win8(HANDLE h, dev_t *dev, ino_t *ino)
{
    FILE_ID_INFO ii;
    if (!GetFileInformationByHandleEx(h, FileIdInfo, &ii, sizeof(ii)))
        return false;
    *dev = ii.VolumeSerialNumber;
    // The definition of FILE_ID_128 differs between mingw-w64 and the Windows
    // SDK, but we can ignore that by just memcpying it. This will also
    // truncate the file ID on 32-bit Windows, which doesn't support __int128.
    // 128-bit file IDs are only used for ReFS, so that should be okay.
    assert(sizeof(*ino) <= sizeof(ii.FileId));
    memcpy(ino, &ii.FileId, sizeof(*ino));
    return true;
}

#if HAVE_UWP
static bool get_file_ids(HANDLE h, dev_t *dev, ino_t *ino)
{
    return false;
}
#else
static bool get_file_ids(HANDLE h, dev_t *dev, ino_t *ino)
{
    // GetFileInformationByHandle works on FAT partitions and Windows 7, but
    // doesn't work in UWP and can produce non-unique IDs on ReFS
    BY_HANDLE_FILE_INFORMATION bhfi;
    if (!GetFileInformationByHandle(h, &bhfi))
        return false;
    *dev = bhfi.dwVolumeSerialNumber;
    *ino = ((ino_t)bhfi.nFileIndexHigh << 32) | bhfi.nFileIndexLow;
    return true;
}
#endif

// Like fstat(), but with a Windows HANDLE
static int hstat(HANDLE h, struct mp_stat *buf)
{
    // Handle special (or unknown) file types first
    switch (GetFileType(h) & ~FILE_TYPE_REMOTE) {
    case FILE_TYPE_PIPE:
        *buf = (struct mp_stat){ .st_nlink = 1, .st_mode = _S_IFIFO | 0644 };
        return 0;
    case FILE_TYPE_CHAR: // character device
        *buf = (struct mp_stat){ .st_nlink = 1, .st_mode = _S_IFCHR | 0644 };
        return 0;
    case FILE_TYPE_UNKNOWN:
        errno = EBADF;
        return -1;
    }

    struct mp_stat st = { 0 };

    FILE_BASIC_INFO bi;
    if (!GetFileInformationByHandleEx(h, FileBasicInfo, &bi, sizeof(bi))) {
        errno = EBADF;
        return -1;
    }
    st.st_atime = filetime_to_unix_time(bi.LastAccessTime.QuadPart);
    st.st_mtime = filetime_to_unix_time(bi.LastWriteTime.QuadPart);
    st.st_ctime = filetime_to_unix_time(bi.ChangeTime.QuadPart);

    FILE_STANDARD_INFO si;
    if (!GetFileInformationByHandleEx(h, FileStandardInfo, &si, sizeof(si))) {
        errno = EBADF;
        return -1;
    }
    st.st_nlink = si.NumberOfLinks;

    // Here we pretend Windows has POSIX permissions by pretending all
    // directories are 755 and regular files are 644
    if (si.Directory) {
        st.st_mode |= _S_IFDIR | 0755;
    } else {
        st.st_mode |= _S_IFREG | 0644;
        st.st_size = si.EndOfFile.QuadPart;
    }

    if (!get_file_ids_win8(h, &st.st_dev, &st.st_ino)) {
        // Fall back to the Windows 7 method (also used for FAT in Win8)
        if (!get_file_ids(h, &st.st_dev, &st.st_ino)) {
            errno = EBADF;
            return -1;
        }
    }

    *buf = st;
    return 0;
}

int mp_stat(const char *path, struct mp_stat *buf)
{
    wchar_t *wpath = mp_from_utf8(NULL, path);
    HANDLE h = CreateFileW(wpath, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | SECURITY_SQOS_PRESENT |
        SECURITY_IDENTIFICATION, NULL);
    talloc_free(wpath);
    if (h == INVALID_HANDLE_VALUE) {
        set_errno_from_lasterror();
        return -1;
    }

    int ret = hstat(h, buf);
    CloseHandle(h);
    return ret;
}

int mp_fstat(int fd, struct mp_stat *buf)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    // Use mpv's hstat() function rather than MSVCRT's fstat() because mpv's
    // supports directories and device/inode numbers.
    return hstat(h, buf);
}

#if HAVE_UWP
static int mp_vfprintf(FILE *stream, const char *format, va_list args)
{
    return vfprintf(stream, format, args);
}
#else
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
#endif

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
    // Always use all share modes, which is useful for opening files that are
    // open in other processes, and also more POSIX-like
    static const DWORD share =
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    // Setting FILE_APPEND_DATA and avoiding GENERIC_WRITE/FILE_WRITE_DATA
    // will make the file handle use atomic append behavior
    static const DWORD append =
        FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA;

    DWORD access = 0;
    DWORD disposition = 0;
    DWORD flags = 0;

    switch (oflag & (_O_RDONLY | _O_RDWR | _O_WRONLY | _O_APPEND)) {
    case _O_RDONLY:
        access = GENERIC_READ;
        flags |= FILE_FLAG_BACKUP_SEMANTICS; // For opening directories
        break;
    case _O_RDWR:
        access = GENERIC_READ | GENERIC_WRITE;
        break;
    case _O_RDWR | _O_APPEND:
    case _O_RDONLY | _O_APPEND:
        access = GENERIC_READ | append;
        break;
    case _O_WRONLY:
        access = GENERIC_WRITE;
        break;
    case _O_WRONLY | _O_APPEND:
        access = append;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    switch (oflag & (_O_CREAT | _O_EXCL | _O_TRUNC)) {
    case 0:
    case _O_EXCL: // Like MSVCRT, ignore invalid use of _O_EXCL
        disposition = OPEN_EXISTING;
        break;
    case _O_TRUNC:
    case _O_TRUNC | _O_EXCL:
        disposition = TRUNCATE_EXISTING;
        break;
    case _O_CREAT:
        disposition = OPEN_ALWAYS;
        flags |= FILE_ATTRIBUTE_NORMAL;
        break;
    case _O_CREAT | _O_TRUNC:
        disposition = CREATE_ALWAYS;
        break;
    case _O_CREAT | _O_EXCL:
    case _O_CREAT | _O_EXCL | _O_TRUNC:
        disposition = CREATE_NEW;
        flags |= FILE_ATTRIBUTE_NORMAL;
        break;
    }

    // Opening a named pipe as a file can allow the pipe server to impersonate
    // mpv's process, which could be a security issue. Set SQOS flags, so pipe
    // servers can only identify the mpv process, not impersonate it.
    if (disposition != CREATE_NEW)
        flags |= SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION;

    // Keep the same semantics for some MSVCRT-specific flags
    if (oflag & _O_TEMPORARY) {
        flags |= FILE_FLAG_DELETE_ON_CLOSE;
        access |= DELETE;
    }
    if (oflag & _O_SHORT_LIVED)
        flags |= FILE_ATTRIBUTE_TEMPORARY;
    if (oflag & _O_SEQUENTIAL) {
        flags |= FILE_FLAG_SEQUENTIAL_SCAN;
    } else if (oflag & _O_RANDOM) {
        flags |= FILE_FLAG_RANDOM_ACCESS;
    }

    // Open the Windows file handle
    wchar_t *wpath = mp_from_utf8(NULL, filename);
    HANDLE h = CreateFileW(wpath, access, share, NULL, disposition, flags, NULL);
    talloc_free(wpath);
    if (h == INVALID_HANDLE_VALUE) {
        set_errno_from_lasterror();
        return -1;
    }

    // Map the Windows file handle to a CRT file descriptor. Note: MSVCRT only
    // cares about the following oflags.
    oflag &= _O_APPEND | _O_RDONLY | _O_RDWR | _O_WRONLY;
    oflag |= _O_NOINHERIT; // We never create inheritable handles
    int fd = _open_osfhandle((intptr_t)h, oflag);
    if (fd < 0) {
        CloseHandle(h);
        return -1;
    }

    return fd;
}

int mp_creat(const char *filename, int mode)
{
    return mp_open(filename, _O_CREAT | _O_WRONLY | _O_TRUNC, mode);
}

FILE *mp_fopen(const char *filename, const char *mode)
{
    if (!mode[0]) {
        errno = EINVAL;
        return NULL;
    }

    int rwmode;
    int oflags = 0;
    switch (mode[0]) {
    case 'r':
        rwmode = _O_RDONLY;
        break;
    case 'w':
        rwmode = _O_WRONLY;
        oflags |= _O_CREAT | _O_TRUNC;
        break;
    case 'a':
        rwmode = _O_WRONLY;
        oflags |= _O_CREAT | _O_APPEND;
        break;
    default:
        errno = EINVAL;
        return NULL;
    }

    // Parse extra mode flags
    for (const char *pos = mode + 1; *pos; pos++) {
        switch (*pos) {
        case '+': rwmode = _O_RDWR;  break;
        case 'x': oflags |= _O_EXCL; break;
        // Ignore unknown flags (glibc does too)
        default: break;
        }
    }

    // Open a CRT file descriptor
    int fd = mp_open(filename, rwmode | oflags);
    if (fd < 0)
        return NULL;

    // Add 'b' to the mode so the CRT knows the file is opened in binary mode
    char bmode[] = { mode[0], 'b', rwmode == _O_RDWR ? '+' : '\0', '\0' };
    FILE *fp = fdopen(fd, bmode);
    if (!fp) {
        close(fd);
        return NULL;
    }

    return fp;
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

char *mp_win32_getcwd(char *buf, size_t size)
{
    if (size >= SIZE_MAX / 3 - 1) {
        errno = ENOMEM;
        return NULL;
    }
    size_t wbuffer = size * 3 + 1;
    wchar_t *wres = talloc_array(NULL, wchar_t, wbuffer);
    DWORD wlen = GetFullPathNameW(L".", wbuffer, wres, NULL);
    if (wlen >= wbuffer || wlen == 0) {
        talloc_free(wres);
        errno = wlen ? ERANGE : ENOENT;
        return NULL;
    }
    char *t = mp_to_utf8(NULL, wres);
    talloc_free(wres);
    size_t st = strlen(t);
    if (st >= size) {
        talloc_free(t);
        errno = ERANGE;
        return NULL;
    }
    memcpy(buf, t, st + 1);
    talloc_free(t);
    return buf;
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
#if !HAVE_UWP
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
#endif
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

#if HAVE_UWP
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    errno = ENOSYS;
    return MAP_FAILED;
}

int munmap(void *addr, size_t length)
{
    errno = ENOSYS;
    return -1;
}

int msync(void *addr, size_t length, int flags)
{
    errno = ENOSYS;
    return -1;
}
#else
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
#endif

locale_t newlocale(int category, const char *locale, locale_t base)
{
    return (locale_t)1;
}

locale_t uselocale(locale_t locobj)
{
    return (locale_t)1;
}

void freelocale(locale_t locobj)
{
}

#endif // __MINGW32__
