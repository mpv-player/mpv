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

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "talloc.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)
static const char dir_separators[] = "/\\:";
#else
static const char dir_separators[] = "/";
#endif

char **find_files(const char *original_file, const char *suffix,
                  int *num_results_ptr)
{
    void *tmpmem = talloc_new(NULL);
    char *fname = talloc_strdup(tmpmem, original_file);
    char *basename = NULL;
    char *next = fname;
    while (1) {
        next = strpbrk(next, dir_separators);
        if (!next)
            break;
        basename = next++;
    }
    char *directory;
    if (basename) {
        directory = fname;
        *basename++ = 0;
    } else {
        directory = ".";
        basename = fname;
    }


    char **results = talloc_size(NULL, 0);
    DIR *dp = opendir(directory);
    struct dirent *ep;
    char ***names_by_matchlen = talloc_array(tmpmem, char **,
                                             strlen(basename) + 1);
    memset(names_by_matchlen, 0, talloc_get_size(names_by_matchlen));
    int num_results = 0;
    while ((ep = readdir(dp))) {
        int suffix_offset = strlen(ep->d_name) - strlen(suffix);
        // name must end with suffix
        if (suffix_offset < 0 || strcmp(ep->d_name + suffix_offset, suffix))
            continue;
        // don't list the original name
        if (!strcmp(ep->d_name, basename))
            continue;

        char *name = talloc_asprintf(results, "%s/%s", directory, ep->d_name);
        char *s1 = ep->d_name;
        char *s2 = basename;
        int matchlen = 0;
        while (*s1 && *s1++ == *s2++)
            matchlen++;
        int oldcount = talloc_get_size(names_by_matchlen[matchlen]) /
            sizeof(char **);
        names_by_matchlen[matchlen] = talloc_realloc(names_by_matchlen,
                                                  names_by_matchlen[matchlen],
                                                  char *, oldcount + 1);
        names_by_matchlen[matchlen][oldcount] = name;
        num_results++;
    }
    closedir(dp);
    results = talloc_realloc(NULL, results, char *, num_results);
    char **resptr = results;
    for (int i = strlen(basename); i >= 0; i--) {
        char **p = names_by_matchlen[i];
        for (int j = 0; j < talloc_get_size(p) / sizeof(char *); j++)
            *resptr++ = p[j];
    }
    assert(resptr == results + num_results);
    talloc_free(tmpmem);
    *num_results_ptr = num_results;
    return results;
}
