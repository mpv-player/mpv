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

#include <stdbool.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

// This is in POSIX.1-2008, but support outside of Linux is scarce.
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
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

// Windows' MAX_PATH/PATH_MAX/FILENAME_MAX is fixed to 260, but this limit
// applies to unicode paths encoded with wchar_t (2 bytes on Windows). The UTF-8
// version could end up bigger in memory. In the worst case each wchar_t is
// encoded to 3 bytes in UTF-8, so in the worst case we have:
//      wcslen(wpath) * 3 <= strlen(utf8path)
// Thus we need MP_PATH_MAX as the UTF-8/char version of PATH_MAX.
// Also make sure there's free space for the terminating \0.
// (For codepoints encoded as UTF-16 surrogate pairs, UTF-8 has the same length.)
#define MP_PATH_MAX (FILENAME_MAX * 3 + 1)

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

#else /* __MINGW32__ */

//GNU HURD doesn't define PATH_MAX
//http://www.gnu.org/software/hurd/community/gsoc/project_ideas/maxpath.html
#ifdef PATH_MAX
#define MP_PATH_MAX PATH_MAX
#else
#define MP_PATH_MAX 4096
#endif 

#define mp_stat(...) stat(__VA_ARGS__)

#endif /* __MINGW32__ */

#endif
