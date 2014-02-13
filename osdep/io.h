/*
 * unicode/utf-8 I/O helpers and wrappers for Windows
 *
 * This file is part of mplayer2.
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

#ifndef MPLAYER_OSDEP_IO
#define MPLAYER_OSDEP_IO

#include "config.h"
#include <stdbool.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if HAVE_GLOB
#include <glob.h>
#endif

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

#ifdef _WIN32
#include <wchar.h>
wchar_t *mp_from_utf8(void *talloc_ctx, const char *s);
char *mp_to_utf8(void *talloc_ctx, const wchar_t *s);
#endif

#ifdef __CYGWIN__
#include <io.h>
#endif

#ifdef __MINGW32__

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

void mp_get_converted_argv(int *argc, char ***argv);

int mp_stat(const char *path, struct stat *buf);
int mp_printf(const char *format, ...);
int mp_fprintf(FILE *stream, const char *format, ...);
int mp_open(const char *filename, int oflag, ...);
int mp_creat(const char *filename, int mode);
FILE *mp_fopen(const char *filename, const char *mode);
DIR *mp_opendir(const char *path);
struct dirent *mp_readdir(DIR *dir);
int mp_closedir(DIR *dir);
int mp_mkdir(const char *path, int mode);
char *mp_getenv(const char *name);

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

// NOTE: stat is not overridden with mp_stat, because MinGW-w64 defines it as
//       macro.

#define printf(...) mp_printf(__VA_ARGS__)
#define fprintf(...) mp_fprintf(__VA_ARGS__)
#define open(...) mp_open(__VA_ARGS__)
#define creat(...) mp_creat(__VA_ARGS__)
#define fopen(...) mp_fopen(__VA_ARGS__)
#define opendir(...) mp_opendir(__VA_ARGS__)
#define readdir(...) mp_readdir(__VA_ARGS__)
#define closedir(...) mp_closedir(__VA_ARGS__)
#define mkdir(...) mp_mkdir(__VA_ARGS__)
#define getenv(...) mp_getenv(__VA_ARGS__)

#ifndef GLOB_NOMATCH
#define GLOB_NOMATCH 3
#endif

#define glob_t mp_glob_t
#define glob(...) mp_glob(__VA_ARGS__)
#define globfree(...) mp_globfree(__VA_ARGS__)

#else /* __MINGW32__ */

#define mp_stat(...) stat(__VA_ARGS__)

#endif /* __MINGW32__ */

#endif
