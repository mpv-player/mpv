/*
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

#include <windows.h>
#include <stdbool.h>
#include <string.h>
#include "osdep/io.h"
#include "mpv_talloc.h"

#if HAVE_UWP
// Missing from MinGW headers.
WINBASEAPI HANDLE WINAPI FindFirstFileExW(LPCWSTR lpFileName,
    FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData,
    FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags);
#endif

static wchar_t *talloc_wcsdup(void *ctx, const wchar_t *wcs)
{
    size_t len = (wcslen(wcs) + 1) * sizeof(wchar_t);
    return talloc_memdup(ctx, (void*)wcs, len);
}

static int compare_wcscoll(const void *v1, const void *v2)
{
    wchar_t * const* p1 = v1;
    wchar_t * const* p2 = v2;
    return wcscoll(*p1, *p2);
}

static bool exists(const char *filename)
{
    wchar_t *wfilename = mp_from_utf8(NULL, filename);
    bool result = GetFileAttributesW(wfilename) != INVALID_FILE_ATTRIBUTES;
    talloc_free(wfilename);
    return result;
}

int mp_glob(const char *restrict pattern, int flags,
            int (*errfunc)(const char*, int), mp_glob_t *restrict pglob)
{
    // This glob implementation never calls errfunc and doesn't understand any
    // flags. These features are currently unused in mpv, however if new code
    // were to use these them, it would probably break on Windows.

    unsigned dirlen = 0;
    bool wildcards = false;

    // Check for drive relative paths eg. "C:*.flac"
    if (pattern[0] != '\0' && pattern[1] == ':')
        dirlen = 2;

    // Split the directory and filename. All files returned by FindFirstFile
    // will be in this directory. Also check the filename for wildcards.
    for (unsigned i = 0; pattern[i]; i ++) {
        if (pattern[i] == '?' || pattern[i] == '*')
            wildcards = true;

        if (pattern[i] == '\\' || pattern[i] == '/') {
            dirlen = i + 1;
            wildcards = false;
        }
    }

    // FindFirstFile is unreliable with certain input (it returns weird results
    // with paths like "." and "..", and presumably others.) If there are no
    // wildcards in the filename, don't call it, just check if the file exists.
    // The CRT globbing code does this too.
    if (!wildcards) {
        if (!exists(pattern)) {
            pglob->gl_pathc = 0;
            return GLOB_NOMATCH;
        }

        pglob->ctx = talloc_new(NULL);
        pglob->gl_pathc = 1;
        pglob->gl_pathv = talloc_array_ptrtype(pglob->ctx, pglob->gl_pathv, 2);
        pglob->gl_pathv[0] = talloc_strdup(pglob->ctx, pattern);
        pglob->gl_pathv[1] = NULL;
        return 0;
    }

    wchar_t *wpattern = mp_from_utf8(NULL, pattern);
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileExW(wpattern, FindExInfoBasic, &data, FindExSearchNameMatch, NULL, 0);
    talloc_free(wpattern);

    // Assume an error means there were no matches. mpv doesn't check for
    // glob() errors, so this should be fine for now.
    if (find == INVALID_HANDLE_VALUE) {
        pglob->gl_pathc = 0;
        return GLOB_NOMATCH;
    }

    size_t pathc = 0;
    void *tmp = talloc_new(NULL);
    wchar_t **wnamev = NULL;

    // Read a list of filenames. Unlike glob(), FindFirstFile doesn't return
    // the full path, since all files are relative to the directory specified
    // in the pattern.
    do {
        if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L".."))
            continue;

        wchar_t *wname = talloc_wcsdup(tmp, data.cFileName);
        MP_TARRAY_APPEND(tmp, wnamev, pathc, wname);
    } while (FindNextFileW(find, &data));
    FindClose(find);

    if (!wnamev) {
        talloc_free(tmp);
        pglob->gl_pathc = 0;
        return GLOB_NOMATCH;
    }

    // POSIX glob() is supposed to sort paths according to LC_COLLATE.
    // FindFirstFile just returns paths in the order they are read from the
    // directory, so sort them manually with wcscoll.
    qsort(wnamev, pathc, sizeof(wchar_t*), compare_wcscoll);

    pglob->ctx = talloc_new(NULL);
    pglob->gl_pathc = pathc;
    pglob->gl_pathv = talloc_array_ptrtype(pglob->ctx, pglob->gl_pathv,
                                           pathc + 1);

    // Now convert all filenames to UTF-8 (they had to be in UTF-16 for
    // sorting) and prepend the directory
    for (unsigned i = 0; i < pathc; i ++) {
        int namelen = WideCharToMultiByte(CP_UTF8, 0, wnamev[i], -1, NULL, 0,
                                          NULL, NULL);
        char *path = talloc_array(pglob->ctx, char, namelen + dirlen);

        memcpy(path, pattern, dirlen);
        WideCharToMultiByte(CP_UTF8, 0, wnamev[i], -1, path + dirlen,
                            namelen, NULL, NULL);
        pglob->gl_pathv[i] = path;
    }

    // gl_pathv must be null terminated
    pglob->gl_pathv[pathc] = NULL;
    talloc_free(tmp);
    return 0;
}

void mp_globfree(mp_glob_t *pglob)
{
    talloc_free(pglob->ctx);
}
