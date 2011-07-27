#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "mp_msg.h"
#include "options.h"
#include "path.h"
#include "mpcommon.h"
#include "sub/find_subfiles.h"
#include "sub/sub.h"

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

struct subfn {
    int priority;
    char *fname;
};

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
    while (i >= 0 && isalpha(name.start[i])) {
        n++;
        if (n > 3)
            return (struct bstr){NULL, 0};
        i--;
    }
    if (n < 2)
        return (struct bstr){NULL, 0};
    return (struct bstr){name.start + i + 1, n};
}

struct sub_list {
    struct subfn subs[MAX_SUBTITLE_FILES];
    int sid;
    void *ctx;
};

/**
 * @brief Append all the subtitles in the given path matching fname
 * @param opts MPlayer options
 * @param slist pointer to the subtitles list tallocated
 * @param nsub pointer to the number of subtitles
 * @param path Look for subtitles in this directory
 * @param fname Subtitle filename (pattern)
 * @param limit_fuzziness Ignore flag when sub_fuziness == 2
 */
static void append_dir_subtitles(struct MPOpts *opts,
                                 struct subfn **slist, int *nsub,
                                 struct bstr path, const char *fname,
                                 int limit_fuzziness)
{
    char *sub_exts[] = {"utf", "utf8", "utf-8", "sub", "srt", "smi", "rt", "txt", "ssa", "aqt", "jss", "js", "ass", NULL};
    void *tmpmem = talloc_new(NULL);
    FILE *f;
    assert(strlen(fname) < 1e6);

    struct bstr f_fname = bstr(mp_basename(fname));
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
    mp_msg(MSGT_SUBREADER, MSGL_INFO, "Load subtitles in %.*s\n", BSTR_P(path));
    struct dirent *de;
    while ((de = readdir(d))) {
        struct bstr dename = bstr(de->d_name);
        void *tmpmem2 = talloc_new(tmpmem);

        // retrieve various parts of the filename
        struct bstr tmp_fname_noext = bstrdup(tmpmem2, strip_ext(dename));
        bstr_lower(tmp_fname_noext);
        struct bstr tmp_fname_ext = get_ext(dename);
        struct bstr tmp_fname_trim = bstr_strip(tmp_fname_noext);

        // If it's a .sub, check if there is a .idx with the same name. If
        // there is one, it's certainly a vobsub so we skip it.
        if (bstrcasecmp(tmp_fname_ext, bstr("sub")) == 0) {
            char *idxname = talloc_asprintf(tmpmem2, "%.*s.idx",
                                            (int)tmp_fname_noext.len,
                                            de->d_name);
            char *idx = mp_path_join(tmpmem2, path, bstr(idxname));
            f = fopen(idx, "rt");
            if (f) {
                fclose(f);
                goto next_sub;
            }
        }

        // does it end with a subtitle extension?
#ifdef CONFIG_ICONV
#ifdef CONFIG_ENCA
        int i = (sub_cp && strncasecmp(sub_cp, "enca", 4) != 0) ? 3 : 0;
#else
        int i = sub_cp ? 3 : 0;
#endif
#else
        int i = 0;
#endif
        while (1) {
            if (!sub_exts[i])
                goto next_sub;
            if (bstrcasecmp(bstr(sub_exts[i]), tmp_fname_ext) == 0)
                break;
            i++;
        }

        // we have a (likely) subtitle file
        int prio = 0;
        if (opts->sub_lang) {
            if (bstr_startswith(tmp_fname_trim, f_fname_trim)) {
                struct bstr lang = guess_lang_from_filename(tmp_fname_trim);
                if (lang.len) {
                    for (int n = 0; opts->sub_lang[n]; n++) {
                        if (bstr_startswith(lang,
                                            bstr(opts->sub_lang[n]))) {
                            prio = 4; // matches the movie name + lang extension
                            break;
                        }
                    }
                }
            }
        }
        if (!prio && bstrcmp(tmp_fname_trim, f_fname_trim) == 0)
            prio = 3; // matches the movie name
        if (!prio && bstr_find(tmp_fname_trim, f_fname_trim) >= 0
            && sub_match_fuzziness >= 1)
            prio = 2; // contains the movie name
        if (!prio) {
            // doesn't contain the movie name
            // don't try in the mplayer subtitle directory
            if (!limit_fuzziness && sub_match_fuzziness >= 2) {
                prio = 1;
            }
        }

        mp_msg(MSGT_SUBREADER, MSGL_DBG2, "Potential sub file: "
               "\"%s\"  Priority: %d\n", de->d_name, prio);
        if (prio) {
            prio += prio;
#ifdef CONFIG_ICONV
            if (i < 3) // prefer UTF-8 coded
                prio++;
#endif
            char *subpath = mp_path_join(*slist, path, dename);
            if ((f = fopen(subpath, "rt"))) {
                MP_GROW_ARRAY(*slist, *nsub);
                struct subfn *sub = *slist + (*nsub)++;

                fclose(f);
                sub->priority = prio;
                sub->fname    = subpath;
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

char **find_text_subtitles(struct MPOpts *opts, const char *fname)
{
    char **subnames = NULL;
    struct subfn *slist = talloc_array_ptrtype(NULL, slist, 1);
    int n = 0;

    // Load subtitles from current media directory
    append_dir_subtitles(opts, &slist, &n, mp_dirname(fname), fname, 0);

    // Load subtitles in dirs specified by sub-paths option
    if (opts->sub_paths) {
        for (int i = 0; opts->sub_paths[i]; i++) {
            char *path = mp_path_join(slist, mp_dirname(fname),
                                      bstr(opts->sub_paths[i]));
            append_dir_subtitles(opts, &slist, &n, bstr(path), fname, 0);
        }
    }

    // Load subtitles in ~/.mplayer/sub limiting sub fuzziness
    char *mp_subdir = get_path("sub/");
    if (mp_subdir)
        append_dir_subtitles(opts, &slist, &n, bstr(mp_subdir), fname, 1);
    free(mp_subdir);

    // Sort subs by priority and append them
    qsort(slist, n, sizeof(*slist), compare_sub_priority);

    subnames = talloc_array_ptrtype(NULL, subnames, n);
    for (int i = 0; i < n; i++)
        subnames[i] = talloc_strdup(subnames, slist[i].fname);

    talloc_free(slist);
    return subnames;
}

char **find_vob_subtitles(struct MPOpts *opts, const char *fname)
{
    char **vobs = talloc_array_ptrtype(NULL, vobs, 1);
    int n = 0;

    // Potential vobsub in the media directory
    struct bstr bname = bstr(mp_basename(fname));
    int pdot = bstrrchr(bname, '.');
    if (pdot >= 0)
        bname.len = pdot;
    vobs[n++] = mp_path_join(vobs, mp_dirname(fname), bname);

    // Potential vobsubs in directories specified by sub-paths option
    if (opts->sub_paths) {
        for (int i = 0; opts->sub_paths[i]; i++) {
            char *path = mp_path_join(NULL, mp_dirname(fname),
                                      bstr(opts->sub_paths[i]));
            MP_GROW_ARRAY(vobs, n);
            vobs[n++] = mp_path_join(vobs, bstr(path), bname);
            talloc_free(path);
        }
    }

    // Potential vobsub in ~/.mplayer/sub
    char *mp_subdir = get_path("sub/");
    if (mp_subdir) {
        MP_GROW_ARRAY(vobs, n);
        vobs[n++] = mp_path_join(vobs, bstr(mp_subdir), bname);
    }

    free(mp_subdir);
    MP_RESIZE_ARRAY(NULL, vobs, n);
    return vobs;
}
