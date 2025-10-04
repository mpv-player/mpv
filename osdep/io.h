/*
 * unicode/utf-8 I/O helpers and wrappers for Windows
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

#ifndef MPLAYER_OSDEP_IO
#define MPLAYER_OSDEP_IO

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <locale.h>

#include "compiler.h"

#if HAVE_GLOB_POSIX
#include <glob.h>
#endif

#if HAVE_ANDROID
#  include <unistd.h>
#  include <stdio.h>

// replace lseek with the 64bit variant
#ifdef lseek
#  undef lseek
#endif
#define lseek(f,p,w) lseek64((f), (p), (w))

// replace possible fseeko with a
// lseek64 based solution.
#ifdef fseeko
#  undef fseeko
#endif
static inline int mp_fseeko(FILE* fp, off64_t offset, int whence) {
    int ret = -1;
    if ((ret = fflush(fp)) != 0) {
        return ret;
    }

    return lseek64(fileno(fp), offset, whence) >= 0 ? 0 : -1;
}
#define fseeko(f,p,w) mp_fseeko((f), (p), (w))

#endif // HAVE_ANDROID

#ifndef O_BINARY
#define O_BINARY 0
#endif

// This is in POSIX.1-2008, but support outside of Linux is scarce.
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef FD_CLOEXEC
#define FD_CLOEXEC 0
#endif

bool mp_set_cloexec(int fd);
int mp_make_cloexec_pipe(int pipes[2]);
int mp_make_wakeup_pipe(int pipes[2]);
void mp_flush_wakeup_pipe(int pipe_end);

#ifdef _WIN32

#include <wchar.h>
wchar_t *mp_from_utf8(void *talloc_ctx, const char *s);
char *mp_to_utf8(void *talloc_ctx, const wchar_t *s);

// Use this in win32-specific code rather than PATH_MAX or MAX_PATH.
// This is necessary because we declare long-path aware support which raises
// the effective limit without affecting any defines.
// The actual limit is 32767 but there's a few edge cases that reduce
// it. So pick this nice round number.
// Note that this is wchars, not chars.
#define MP_PATH_MAX (32000)

#endif

#if defined(_WIN32) && !defined(__MINGW32__)
#include <io.h>
#include "dirent-win.h"
#else
#include <dirent.h>
#include <unistd.h>
#endif

#ifdef _WIN32

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

size_t mp_fwrite(const void *restrict buffer, size_t size, size_t count,
                 FILE *restrict stream);
int mp_printf(const char *format, ...) MP_PRINTF_ATTRIBUTE(1, 2);
int mp_fprintf(FILE *stream, const char *format, ...) MP_PRINTF_ATTRIBUTE(2, 3);
int mp_open(const char *filename, int oflag, ...);
int mp_creat(const char *filename, int mode);
int mp_rename(const char *oldpath, const char *newpath);
FILE *mp_fopen(const char *filename, const char *mode);
DIR *mp_opendir(const char *path);
struct dirent *mp_readdir(DIR *dir);
int mp_closedir(DIR *dir);
int mp_mkdir(const char *path, int mode);
int mp_unlink(const char *path);
char *mp_win32_getcwd(char *buf, size_t size);
char *mp_getenv(const char *name);

#ifdef environ  /* mingw defines it as _environ */
#undef environ
#endif
#define environ (*mp_penviron())  /* ensure initialization and l-value */
char ***mp_penviron(void);

#undef off_t
#define off_t int64_t
off_t mp_lseek64(int fd, off_t offset, int whence);
int mp_ftruncate64(int fd, off_t length);
void *mp_dlopen(const char *filename, int flag);
void *mp_dlsym(void *handle, const char *symbol);
char *mp_dlerror(void);

// mp_stat types. MSVCRT's dev_t and ino_t are way too short to be unique.
typedef uint64_t mp_dev_t_;
#ifdef _WIN64
typedef unsigned __int128 mp_ino_t_;
#else
// 32-bit Windows doesn't have a __int128-type, which means ReFS file IDs will
// be truncated and might collide. This is probably not a problem because ReFS
// is not available in consumer versions of Windows.
typedef uint64_t mp_ino_t_;
#endif
#define dev_t mp_dev_t_
#define ino_t mp_ino_t_

// mp_stat uses a different structure to MSVCRT, with 64-bit inodes
struct mp_stat {
    dev_t st_dev;
    ino_t st_ino;
    unsigned short st_mode;
    unsigned int st_nlink;
    short st_uid;
    short st_gid;
    dev_t st_rdev;
    int64_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

int mp_stat(const char *path, struct mp_stat *buf);
int mp_fstat(int fd, struct mp_stat *buf);

typedef struct {
    size_t gl_pathc;
    char **gl_pathv;
    size_t gl_offs;
    void *ctx;
} mp_glob_t;

// glob-win.c
int mp_glob(const char *restrict pattern, int flags,
            int (*errfunc)(const char*, int), mp_glob_t *restrict pglob);
void mp_globfree(mp_glob_t *pglob);

#define fwrite(...) mp_fwrite(__VA_ARGS__)
#define ftell(...) _ftelli64(__VA_ARGS__)
#define printf(...) mp_printf(__VA_ARGS__)
#define fprintf(...) mp_fprintf(__VA_ARGS__)
#define open(...) mp_open(__VA_ARGS__)
#define creat(...) mp_creat(__VA_ARGS__)
#define rename(...) mp_rename(__VA_ARGS__)
#define fopen(...) mp_fopen(__VA_ARGS__)
#define opendir(...) mp_opendir(__VA_ARGS__)
#define readdir(...) mp_readdir(__VA_ARGS__)
#define closedir(...) mp_closedir(__VA_ARGS__)
#define mkdir(...) mp_mkdir(__VA_ARGS__)
#define unlink(...) mp_unlink(__VA_ARGS__)
#define getcwd(...) mp_win32_getcwd(__VA_ARGS__)
#define getenv(...) mp_getenv(__VA_ARGS__)

#undef lseek
#define lseek(...) mp_lseek64(__VA_ARGS__)

#undef ftruncate
#define ftruncate(...) mp_ftruncate64(__VA_ARGS__)

#define RTLD_NOW 0
#define RTLD_LOCAL 0
#define dlopen(fn,fg) mp_dlopen((fn), (fg))
#define dlsym(h,s) mp_dlsym((h), (s))
#define dlerror mp_dlerror

// Affects both "stat()" and "struct stat".
#undef stat
#define stat mp_stat

#undef fstat
#define fstat(...) mp_fstat(__VA_ARGS__)

#define utime(...) _utime(__VA_ARGS__)
#define utimbuf _utimbuf

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int msync(void *addr, size_t length, int flags);
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define MAP_FAILED ((void *)-1)
#define MS_ASYNC 1
#define MS_SYNC 2
#define MS_INVALIDATE 4

#ifndef GLOB_NOMATCH
#define GLOB_NOMATCH 3
#endif

#define glob_t mp_glob_t
#define glob(...) mp_glob(__VA_ARGS__)
#define globfree(...) mp_globfree(__VA_ARGS__)

// These are stubs since there is not anything that helps with this on Windows.
#define locale_t int
#define LC_CTYPE_MASK 0
locale_t newlocale(int, const char *, locale_t);
locale_t uselocale(locale_t);
void freelocale(locale_t);

#else /* __MINGW32__ */

#include <sys/mman.h>

extern char **environ;

#endif /* __MINGW32__ */

#endif
