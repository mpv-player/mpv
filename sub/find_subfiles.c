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
#include "options/options.h"
#include "options/path.h"
#include "sub/find_subfiles.h"

static const char *const sub_exts[] = {"utf", "utf8", "utf-8", "idx", "sub", "srt",
                                       "smi", "rt", "txt", "ssa", "aqt", "jss",
                                       "js", "ass", "mks", "vtt", "sup", NULL};

static const char *const audio_exts[] = {"mp3", "aac", "mka", "dts", "flac",
                                         "ogg", "m4a", "ac3", NULL};

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

static struct bstr strip_ext(struct bstr str)
{
    int dotpos = bstrrchr(str, '.');
    if (dotpos < 0)
        return str;
    return (struct bstr){str.start, dotpos};
}

static struct bstr get_ext(struct bstr s)
{
    int dotpos = bstrrchr(s, '.');
    if (dotpos < 0)
        return (struct bstr){NULL, 0};
    return bstr_splice(s, dotpos + 1, s.len);
}

bool mp_might_be_subtitle_file(const char *filename)
{
    return test_ext(get_ext(bstr0(filename))) == STREAM_SUB;
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
                                 int limit_fuzziness)
{
    void *tmpmem = talloc_new(NULL);
    struct MPOpts *opts = global->opts;
    struct mp_log *log = mp_log_new(tmpmem, global->log, "find_files");

    if (mp_is_url(bstr0(fname)))
        goto out;

    struct bstr f_fname = bstr0(mp_basename(fname));
    struct bstr f_fname_noext = bstrdup(tmpmem, strip_ext(f_fname));
    bstr_lower(f_fname_noext);
    struct bstr f_fname_trim = bstr_strip(f_fname_noext);

    // 0 = nothing
    // 1 = any subtitle file
    // 2 = any sub file containing movie name
    // 3 = sub file containing movie name and the lang extension
    char *path0 = bstrdup0(tmpmem, path);
    DIR *d = opendir(path0);
    if (!d)
        goto out;
    mp_verbose(log, "Loading external files in %.*s\n", BSTR_P(path));
    struct dirent *de;
    while ((de = readdir(d))) {
        struct bstr dename = bstr0(de->d_name);
        void *tmpmem2 = talloc_new(tmpmem);

        // retrieve various parts of the filename
        struct bstr tmp_fname_noext = bstrdup(tmpmem2, strip_ext(dename));
        bstr_lower(tmp_fname_noext);
        struct bstr tmp_fname_ext = get_ext(dename);
        struct bstr tmp_fname_trim = bstr_strip(tmp_fname_noext);

        // check what it is (most likely)
        int type = test_ext(tmp_fname_ext);
        char **langs = NULL;
        int fuzz = -1;
        switch (type) {
        case STREAM_SUB:
            langs = opts->sub_lang;
            fuzz = opts->sub_auto;
            break;
        case STREAM_AUDIO:
            langs = opts->audio_lang;
            fuzz = opts->audiofile_auto;
            break;
        }

        if (fuzz < 0)
            goto next_sub;

        // we have a (likely) subtitle file
        int prio = 0;
        char *found_lang = NULL;
        if (langs) {
            if (bstr_startswith(tmp_fname_trim, f_fname_trim)) {
                struct bstr lang = guess_lang_from_filename(tmp_fname_trim);
                if (lang.len) {
                    for (int n = 0; langs[n]; n++) {
                        if (bstr_startswith0(lang, langs[n])) {
                            prio = 4; // matches the movie name + lang extension
                            found_lang = langs[n];
                            break;
                        }
                    }
                }
            }
        }
        if (!prio && bstrcmp(tmp_fname_trim, f_fname_trim) == 0)
            prio = 3; // matches the movie name
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
            char *subpath = mp_path_join(*slist, path, dename);
            if (mp_path_exists(subpath)) {
                MP_GROW_ARRAY(*slist, *nsub);
                struct subfn *sub = *slist + (*nsub)++;

                // annoying and redundant
                if (strncmp(subpath, "./", 2) == 0)
                    subpath += 2;

                sub->type     = type;
                sub->priority = prio;
                sub->fname    = subpath;
                sub->lang     = found_lang;
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

// Return a list of subtitles and audio files found, sorted by priority.
// Last element is terminated with a fname==NULL entry.
struct subfn *find_external_files(struct mpv_global *global, const char *fname)
{
    struct MPOpts *opts = global->opts;
    struct subfn *slist = talloc_array_ptrtype(NULL, slist, 1);
    int n = 0;

    // Load subtitles from current media directory
    append_dir_subtitles(global, &slist, &n, mp_dirname(fname), fname, 0);

    if (opts->sub_auto >= 0) {
        // Load subtitles in dirs specified by sub-paths option
        if (opts->sub_paths) {
            for (int i = 0; opts->sub_paths[i]; i++) {
                char *path = mp_path_join(slist, mp_dirname(fname),
                                        bstr0(opts->sub_paths[i]));
                append_dir_subtitles(global, &slist, &n, bstr0(path), fname, 0);
            }
        }

        // Load subtitles in ~/.mpv/sub limiting sub fuzziness
        char *mp_subdir = mp_find_config_file(NULL, global, "sub/");
        if (mp_subdir)
            append_dir_subtitles(global, &slist, &n, bstr0(mp_subdir), fname, 1);
        talloc_free(mp_subdir);
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
