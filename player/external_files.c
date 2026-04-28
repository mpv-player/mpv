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

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "osdep/io.h"

#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "misc/charset_conv.h"
#include "misc/language.h"
#include "misc/natural_sort.h"
#include "options/options.h"
#include "options/path.h"
#include "player/core.h"
#include "external_files.h"

static int test_ext(MPOpts *opts, bstr ext)
{
    if (bstr_in_list0(ext, opts->sub_auto_exts))
        return STREAM_SUB;
    if (bstr_in_list0(ext, opts->audio_exts))
        return STREAM_AUDIO;
    if (bstr_in_list0(ext, opts->image_exts))
        return STREAM_VIDEO;
    return -1;
}

static int test_cover_filename(bstr fname, char **cover_files)
{
    int idx = bstr_find_in_list0(fname, cover_files, false);
    // This equals to 0 if not in list (idx == -1)
    return -idx - 1;
}

static int compare_filename(const void *a, const void *b)
{
    const struct subfn *s1 = a;
    const struct subfn *s2 = b;
    return mp_natural_sort_cmp(s1->fname, s2->fname);
}

static int compare_priority(const void *a, const void *b)
{
    const struct subfn *s1 = a;
    const struct subfn *s2 = b;
    if (s1->priority > s2->priority)
        return -1;
    if (s1->priority < s2->priority)
        return 1;
    return mp_natural_sort_cmp(s1->fname, s2->fname);
}

static void append_dir_external_files(struct mpv_global *global, struct MPOpts *opts,
                                      struct subfn **slist, int *nsub,
                                      struct bstr path, const char *fname,
                                      int limit_fuzziness, int limit_type)
{
    void *tmpmem = talloc_new(NULL);
    struct mp_log *log = mp_log_new(tmpmem, global->log, "find_files");

    struct bstr f_fbname = bstr0(mp_basename(fname));
    struct bstr f_fname = mp_iconv_to_utf8(log, f_fbname,
                                           "UTF-8-MAC", MP_NO_LATIN1_FALLBACK);
    struct bstr f_fname_noext = bstrdup(tmpmem, bstr_strip_ext(f_fname));
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
        struct bstr tmp_fname_ext = bstr_get_ext(dename);
        struct bstr tmp_fname_trim = bstr_strip(tmp_fname_noext);

        if (den.start != dename.start)
            talloc_steal(tmpmem2, dename.start);

        // check what it is (most likely)
        int type = test_ext(opts, tmp_fname_ext);
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
        case STREAM_VIDEO:
            fuzz = opts->coverart_auto;
            break;
        }

        if (fuzz < 0 || (limit_type >= 0 && limit_type != type))
            goto next_file;

        // we have a (likely) subtitle file
        // higher prio -> auto-selection may prefer it (0 = not loaded)
        int prio = 0;

        if (bstrcasecmp(tmp_fname_trim, f_fname_trim) == 0)
            prio |= 32; // exact movie name match

        bstr lang = {0};
        int start = 0;
        enum track_flags flags = 0;
        lang = mp_guess_lang_from_filename(dename, &start, &flags);
        if (bstr_case_startswith(tmp_fname_trim, f_fname_trim)) {
            if (lang.len && start == f_fname_trim.len)
                prio |= 16; // exact movie name + followed by lang

            if (lang.len && fuzz >= 1)
                prio |= 4; // matches the movie name + a language was matched

            for (int n = 0; langs && langs[n]; n++) {
                if (lang.len && bstr_case_startswith(lang, bstr0(langs[n]))) {
                    if (fuzz >= 1)
                        prio |= 8; // known language -> boost priority
                    break;
                }
            }
        }

        if (bstr_find(tmp_fname_trim, f_fname_trim) >= 0 && fuzz >= 1)
            prio |= 2; // contains the movie name

        if (type == STREAM_VIDEO && prio == 0)
            prio = test_cover_filename(tmp_fname_trim, opts->coverart_whitelist);

        // doesn't contain the movie name
        // don't try in the mplayer subtitle directory
        if (!limit_fuzziness && fuzz >= 2 && prio == 0)
            prio = INT_MIN;

        if (prio) {
            mp_trace(log, "Potential external file: \"%s\"  Priority: %d\n",
                   de->d_name, prio);
            char *extpath = mp_path_join_bstr(*slist, path, dename);
            if (mp_path_exists(extpath)) {
                MP_TARRAY_GROW(NULL, *slist, *nsub);
                struct subfn *sub = *slist + (*nsub)++;

                // annoying and redundant
                if (strncmp(extpath, "./", 2) == 0)
                    extpath += 2;

                sub->type     = type;
                sub->priority = prio;
                sub->fname    = extpath;
                sub->lang     = lang.len ? bstrdup0(*slist, lang) : NULL;
                sub->flags    = flags;
            } else
                talloc_free(extpath);
        }

    next_file:
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
// Assumes slist is sorted by compare_filename.
static void filter_subidx(struct subfn **slist, int *nsub)
{
    const char *prev = NULL;
    for (int n = 0; n < *nsub; n++) {
        const char *fname = (*slist)[n].fname;
        if (case_endswith(fname, ".idx")) {
            prev = fname;
        } else if (case_endswith(fname, ".sub")) {
            if (prev && strncmp(prev, fname, strlen(fname) - 4) == 0)
                (*slist)[n].priority = 0;
        }
    }
    for (int n = *nsub - 1; n >= 0; n--) {
        if ((*slist)[n].priority == 0)
            MP_TARRAY_REMOVE_AT(*slist, *nsub, n);
    }
}

static void load_paths(struct mpv_global *global, struct MPOpts *opts,
                       struct subfn **slist, int *nsubs, const char *fname,
                       char **paths, char *cfg_path, int type)
{
    for (int i = 0; paths && paths[i]; i++) {
        char *expanded_path = mp_get_user_path(NULL, global, paths[i]);
        char *path = mp_path_join_bstr(
            *slist, mp_dirname(fname),
            bstr0(expanded_path ? expanded_path : paths[i]));
        append_dir_external_files(global, opts, slist, nsubs, bstr0(path),
                                  fname, 0, type);
        talloc_free(expanded_path);
    }

    // Load subtitles in ~/.mpv/sub (or similar) limiting sub fuzziness
    char *mp_subdir = mp_find_config_file(NULL, global, cfg_path);
    if (mp_subdir) {
        append_dir_external_files(global, opts, slist, nsubs, bstr0(mp_subdir),
                                  fname, 1, type);
    }
    talloc_free(mp_subdir);
}

// Return a list of subtitles and audio files found, sorted by priority.
// Last element is terminated with a fname==NULL entry.
struct subfn *find_external_files(struct mpv_global *global, const char *fname,
                                  struct MPOpts *opts)
{
    struct subfn *slist = talloc_array_ptrtype(NULL, slist, 1);
    int n = 0;

    // Load subtitles from current media directory
    append_dir_external_files(global, opts, &slist, &n, mp_dirname(fname), fname, 0, -1);

    // Load subtitles in dirs specified by sub-paths option
    if (opts->sub_auto >= 0) {
        load_paths(global, opts, &slist, &n, fname, opts->sub_paths, "sub",
                   STREAM_SUB);
    }

    if (opts->audiofile_auto >= 0) {
        load_paths(global, opts, &slist, &n, fname, opts->audiofile_paths,
                   "audio", STREAM_AUDIO);
    }

    // Sort by name for filter_subidx()
    qsort(slist, n, sizeof(*slist), compare_filename);

    filter_subidx(&slist, &n);

    // Sort subs by priority and append them
    qsort(slist, n, sizeof(*slist), compare_priority);

    struct subfn z = {0};
    MP_TARRAY_APPEND(NULL, slist, n, z);

    return slist;
}
