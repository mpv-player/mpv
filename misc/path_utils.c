/*
 * Path utility functions
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"

#include "mpv_talloc.h"
#include "osdep/io.h"
#include "misc/ctype.h"
#include "misc/path_utils.h"

char *mp_basename(const char *path)
{
    char *s;

#if HAVE_DOS_PATHS
    if (!mp_is_url(bstr0(path))) {
        s = strrchr(path, '\\');
        if (s)
            path = s + 1;
        s = strrchr(path, ':');
        if (s)
            path = s + 1;
    }
#endif
    s = strrchr(path, '/');
    return s ? s + 1 : (char *)path;
}

struct bstr mp_dirname(const char *path)
{
    struct bstr ret = {
        (uint8_t *)path, mp_basename(path) - path
    };
    if (ret.len == 0)
        return bstr0(".");
    return ret;
}


#if HAVE_DOS_PATHS
static const char mp_path_separators[] = "\\/";
#else
static const char mp_path_separators[] = "/";
#endif

// Mutates path and removes a trailing '/' (or '\' on Windows)
void mp_path_strip_trailing_separator(char *path)
{
    size_t len = strlen(path);
    if (len > 0 && strchr(mp_path_separators, path[len - 1]))
        path[len - 1] = '\0';
}

char *mp_splitext(const char *path, bstr *root)
{
    assert(path);
    int skip = (*path == '.'); // skip leading dot for "hidden" unix files
    const char *split = strrchr(path + skip, '.');
    if (!split || !split[1] || strchr(split, '/'))
        return NULL;
    if (root)
        *root = (bstr){(char *)path, split - path};
    return (char *)split + 1;
}

bool mp_path_is_absolute(struct bstr path)
{
    if (path.len && strchr(mp_path_separators, path.start[0]))
        return true;

#if HAVE_DOS_PATHS
    // Note: "X:filename" is a path relative to the current working directory
    //       of drive X, and thus is not an absolute path. It needs to be
    //       followed by \ or /.
    if (path.len >= 3 && path.start[1] == ':' &&
        strchr(mp_path_separators, path.start[2]))
        return true;
#endif

    return false;
}

char *mp_path_join_bstr(void *talloc_ctx, struct bstr p1, struct bstr p2)
{
    if (p1.len == 0)
        return bstrto0(talloc_ctx, p2);
    if (p2.len == 0)
        return bstrto0(talloc_ctx, p1);

    if (mp_path_is_absolute(p2))
        return bstrdup0(talloc_ctx, p2);

    bool have_separator = strchr(mp_path_separators, p1.start[p1.len - 1]);
#if HAVE_DOS_PATHS
    // "X:" only => path relative to "X:" current working directory.
    if (p1.len == 2 && p1.start[1] == ':')
        have_separator = true;
#endif

    return talloc_asprintf(talloc_ctx, "%.*s%s%.*s", BSTR_P(p1),
                           have_separator ? "" : "/", BSTR_P(p2));
}

char *mp_path_join(void *talloc_ctx, const char *p1, const char *p2)
{
    return mp_path_join_bstr(talloc_ctx, bstr0(p1), bstr0(p2));
}

char *mp_getcwd(void *talloc_ctx)
{
    char *e_wd = getenv("PWD");
    if (e_wd)
        return talloc_strdup(talloc_ctx, e_wd);

    char *wd = talloc_array(talloc_ctx, char, 20);
    while (getcwd(wd, talloc_get_size(wd)) == NULL) {
        if (errno != ERANGE) {
            talloc_free(wd);
            return NULL;
        }
        wd = talloc_realloc(talloc_ctx, wd, char, talloc_get_size(wd) * 2);
    }
    return wd;
}

char *mp_normalize_path(void *talloc_ctx, const char *path)
{
    if (mp_is_url(bstr0(path)))
        return talloc_strdup(talloc_ctx, path);

    return mp_path_join(talloc_ctx, mp_getcwd(talloc_ctx), path);
}

bool mp_path_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0;
}

bool mp_path_isdir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Return false if it's considered a normal local filesystem path.
bool mp_is_url(bstr path)
{
    int proto = bstr_find0(path, "://");
    if (proto < 1)
        return false;
    // Per RFC3986, the first character of the protocol must be alphabetic.
    // The rest must be alphanumeric plus -, + and .
    for (int i = 0; i < proto; i++) {
        unsigned char c = path.start[i];
        if ((i == 0 && !mp_isalpha(c)) ||
            (!mp_isalnum(c) && c != '.' && c != '-' && c != '+'))
        {
            return false;
        }
    }
    return true;
}

// Return the protocol part of path, e.g. "http" if path is "http://...".
// On success, out_url (if not NULL) is set to the part after the "://".
bstr mp_split_proto(bstr path, bstr *out_url)
{
    if (!mp_is_url(path))
        return (bstr){0};
    bstr r;
    bstr_split_tok(path, "://", &r, out_url ? out_url : &(bstr){0});
    return r;
}

void mp_mkdirp(const char *dir)
{
    char *path = talloc_strdup(NULL, dir);
    char *cdir = path + 1;

    while (cdir) {
        cdir = strchr(cdir, '/');
        if (cdir)
            *cdir = 0;

        mkdir(path, 0700);

        if (cdir)
            *cdir++ = '/';
    }

    talloc_free(path);
}
