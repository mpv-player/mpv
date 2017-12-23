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

#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <assert.h>

#include "osdep/io.h"

#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "misc/ctype.h"
#include "misc/charset_conv.h"
#include "options/options.h"
#include "options/path.h"
#include "external_files.h"

static const char *const sub_exts[] = {"utf", "utf8", "utf-8", "idx", "sub",
                                       "srt", "rt", "ssa", "ass", "mks", "vtt",
                                       "sup", "scc", "smi",
                                       NULL};

static const char *const audio_exts[] = {"mp3", "aac", "mka", "dts", "flac",
                                         "ogg", "m4a", "ac3", "opus", "wav",
                                         "wv",
                                         NULL};

static bool test_ext_list(bstr ext, const char *const *list)
{
    for (int n = 0; list[n]; n++) {
        if (bstrcasecmp(bstr0(list[n]), ext) == 0)
            return true;
    }
    return false;
}

static int test_ext(bstr ext)
{
    if (test_ext_list(ext, sub_exts))
        return STREAM_SUB;
    if (test_ext_list(ext, audio_exts))
        return STREAM_AUDIO;
    return -1;
}

bool mp_might_be_subtitle_file(const char *filename)
{
    return test_ext(bstr_get_ext(bstr0(filename))) == STREAM_SUB;
}

static int compare_sub_filename(const void *a, const void *b)
{
    const struct subfn *s1 = a;
    const struct subfn *s2 = b;
    return strcoll(s1->fname, s2->fname);
}

static int compare_sub_priority(const void *a, const void *b)
{
    const struct subfn *s1 = a;
    const struct subfn *s2 = b;
    if (s1->priority > s2->priority)
        return -1;
    if (s1->priority < s2->priority)
        return 1;
    return strcoll(s1->fname, s2->fname);
}

static struct bstr guess_lang_from_filename(struct bstr name)
{
    if (name.len < 2)
        return (struct bstr){NULL, 0};

    int n = 0;
    int i = name.len - 1;

    if (name.start[i] == ')' || name.start[i] == ']')
        i--;
    while (i >= 0 && mp_isalpha(name.start[i])) {
        n++;
        if (n > 3)
            return (struct bstr){NULL, 0};
        i--;
    }
    if (n < 2)
        return (struct bstr){NULL, 0};
    return (struct bstr){name.start + i + 1, n};
}

static void append_dir_subtitles(struct mpv_global *global,
                                 struct subfn **slist, int *nsub,
                                 struct bstr path, const char *fname,
                                 int limit_fuzziness, int limit_type)
{
    void *tmpmem = talloc_new(NULL);
    struct MPOpts *opts = global->opts;
    struct mp_log *log = mp_log_new(tmpmem, global->log, "find_files");

    struct bstr f_fbname = bstr0(mp_basename(fname));
    struct bstr f_fname = mp_iconv_to_utf8(log, f_fbname,
                                           "UTF-8-MAC", MP_NO_LATIN1_FALLBACK);
    struct bstr f_fname_noext = bstrdup(tmpmem, bstr_strip_ext(f_fname));
    bstr_lower(f_fname_noext);
    struct bstr f_fname_trim = bstr_strip(f_fname_noext);

    if (f_fbname.start != f_fname.start)
        talloc_steal(tmpmem, f_fname.start);

    char *path0 = bstrdup0(tmpmem, path);

    if (mp_is_url(bstr0(path0)))
        goto out;

    DIR *d = opendir(path0);
    if (!d)
        goto out;
    mp_verbose(log, "Loading external files in %.*s\n", BSTR_P(path));
    struct dirent *de;
    while ((de = readdir(d))) {
        void *tmpmem2 = talloc_new(tmpmem);
        struct bstr den = bstr0(de->d_name);
        struct bstr dename = mp_iconv_to_utf8(log, den,
                                              "UTF-8-MAC", MP_NO_LATIN1_FALLBACK);
        // retrieve various parts of the filename
        struct bstr tmp_fname_noext = bstrdup(tmpmem2, bstr_strip_ext(dename));
        bstr_lower(tmp_fname_noext);
        struct bstr tmp_fname_ext = bstr_get_ext(dename);
        struct bstr tmp_fname_trim = bstr_strip(tmp_fname_noext);

        if (den.start != dename.start)
            talloc_steal(tmpmem2, dename.start);

        // check what it is (most likely)
        int type = test_ext(tmp_fname_ext);
        char **langs = NULL;
        int fuzz = -1;
        switch (type) {
        case STREAM_SUB:
            langs = opts->stream_lang[type];
            fuzz = opts->sub_auto;
            break;
        case STREAM_AUDIO:
            langs = opts->stream_lang[type];
            fuzz = opts->audiofile_auto;
            break;
        }

        if (fuzz < 0 || (limit_type >= 0 && limit_type != type))
            goto next_sub;

        // we have a (likely) subtitle file
        // 0 = nothing
        // 1 = any subtitle file
        // 2 = any sub file containing movie name
        // 3 = sub file containing movie name and the lang extension
        int prio = 0;

        bstr lang = {0};
        if (bstr_startswith(tmp_fname_trim, f_fname_trim))
            lang = guess_lang_from_filename(tmp_fname_trim);
        for (int n = 0; langs && langs[n]; n++) {
            if (lang.len && bstr_case_startswith(lang, bstr0(langs[n]))) {
                prio = 4; // matches the movie name + lang extension
                break;
            }
        }
        if (!prio && bstrcmp(tmp_fname_trim, f_fname_trim) == 0)
            prio = 3; // matches the movie name
        if (!prio && lang.len)
            prio = 3; // matches the movie name + a language was matched
        if (!prio && bstr_find(tmp_fname_trim, f_fname_trim) >= 0 && fuzz >= 1)
            prio = 2; // contains the movie name
        if (!prio) {
            // doesn't contain the movie name
            // don't try in the mplayer subtitle directory
            if (!limit_fuzziness && fuzz >= 2) {
                prio = 1;
            }
        }

        mp_dbg(log, "Potential external file: \"%s\"  Priority: %d\n",
               de->d_name, prio);

        if (prio) {
            prio += prio;
            char *subpath = mp_path_join_bstr(*slist, path, dename);
            if (mp_path_exists(subpath)) {
                MP_TARRAY_GROW(NULL, *slist, *nsub);
                struct subfn *sub = *slist + (*nsub)++;

                // annoying and redundant
                if (strncmp(subpath, "./", 2) == 0)
                    subpath += 2;

                sub->type     = type;
                sub->priority = prio;
                sub->fname    = subpath;
                sub->lang     = lang.len ? bstrdup0(*slist, lang) : NULL;
            } else
                talloc_free(subpath);
        }

    next_sub:
        talloc_free(tmpmem2);
    }
    closedir(d);

 out:
    talloc_free(tmpmem);
}

static bool case_endswith(const char *s, const char *end)
{
    size_t len = strlen(s);
    size_t elen = strlen(end);
    return len >= elen && strcasecmp(s + len - elen, end) == 0;
}

// Drop .sub file if .idx file exists.
// Assumes slist is sorted by compare_sub_filename.
static void filter_subidx(struct subfn **slist, int *nsub)
{
    const char *prev = NULL;
    for (int n = 0; n < *nsub; n++) {
        const char *fname = (*slist)[n].fname;
        if (case_endswith(fname, ".idx")) {
            prev = fname;
        } else if (case_endswith(fname, ".sub")) {
            if (prev && strncmp(prev, fname, strlen(fname) - 4) == 0)
                (*slist)[n].priority = -1;
        }
    }
    for (int n = *nsub - 1; n >= 0; n--) {
        if ((*slist)[n].priority < 0)
            MP_TARRAY_REMOVE_AT(*slist, *nsub, n);
    }
}

static void load_paths(struct mpv_global *global, struct subfn **slist,
                       int *nsubs, const char *fname, char **paths,
                       char *cfg_path, int type)
{
    for (int i = 0; paths && paths[i]; i++) {
        char *expanded_path = mp_get_user_path(NULL, global, paths[i]);
        char *path = mp_path_join_bstr(
            *slist, mp_dirname(fname),
            bstr0(expanded_path ? expanded_path : paths[i]));
        append_dir_subtitles(global, slist, nsubs, bstr0(path),
                             fname, 0, type);
        talloc_free(expanded_path);
    }

    // Load subtitles in ~/.mpv/sub (or similar) limiting sub fuzziness
    char *mp_subdir = mp_find_config_file(NULL, global, cfg_path);
    if (mp_subdir) {
        append_dir_subtitles(global, slist, nsubs, bstr0(mp_subdir), fname, 1,
                             type);
    }
    talloc_free(mp_subdir);
}

// Return a list of subtitles and audio files found, sorted by priority.
// Last element is terminated with a fname==NULL entry.
struct subfn *find_external_files(struct mpv_global *global, const char *fname)
{
    struct MPOpts *opts = global->opts;
    struct subfn *slist = talloc_array_ptrtype(NULL, slist, 1);
    int n = 0;

    // Load subtitles from current media directory
    append_dir_subtitles(global, &slist, &n, mp_dirname(fname), fname, 0, -1);

    // Load subtitles in dirs specified by sub-paths option
    if (opts->sub_auto >= 0) {
        load_paths(global, &slist, &n, fname, opts->sub_paths, "sub",
                   STREAM_SUB);
    }

    if (opts->audiofile_auto >= 0) {
        load_paths(global, &slist, &n, fname, opts->audiofile_paths, "audio",
                   STREAM_AUDIO);
    }

    // Sort by name for filter_subidx()
    qsort(slist, n, sizeof(*slist), compare_sub_filename);

    filter_subidx(&slist, &n);

    // Sort subs by priority and append them
    qsort(slist, n, sizeof(*slist), compare_sub_priority);

    struct subfn z = {0};
    MP_TARRAY_APPEND(NULL, slist, n, z);

    return slist;
}
