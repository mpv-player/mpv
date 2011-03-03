#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "mp_msg.h"
#include "options.h"
#include "path.h"
#include "mpcommon.h"
#include "sub/find_subfiles.h"
#include "sub/sub.h"

static void strcpy_trim(char *d, char *s)
{
    // skip leading whitespace
    while (*s && isspace(*s)) {
        s++;
    }
    for (;;) {
        // copy word
        while (*s && !isspace(*s)) {
            *d = tolower(*s);
            s++; d++;
        }
        if (*s == 0)
            break;
        // trim excess whitespace
        while (*s && isspace(*s)) {
            s++;
        }
        if (*s == 0)
            break;
        *d++ = ' ';
    }
    *d = 0;
}

static void strcpy_strip_ext(char *d, char *s)
{
    char *tmp = strrchr(s, '.');
    if (!tmp) {
        strcpy(d, s);
        return;
    } else {
        strncpy(d, s, tmp-s);
        d[tmp-s] = 0;
    }
    while (*d) {
        *d = tolower(*d);
        d++;
    }
}

static void strcpy_get_ext(char *d, char *s)
{
    char *tmp = strrchr(s, '.');
    if (!tmp) {
        strcpy(d, "");
        return;
    } else {
        strcpy(d, tmp+1);
    }
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

static void guess_lang_from_filename(char *dstlang, const char *name)
{
    int n = 0, i = strlen(name);

    if (i < 2)
        goto err;
    i--;
    if (name[i] == ')' || name[i] == ']')
        i--;
    while (i >= 0 && isalpha(name[i])) {
        n++;
        if (n > 3)
            goto err;
        i--;
    }
    if (n < 2)
        goto err;
    memcpy(dstlang, &name[i + 1], n);
    dstlang[n] = 0;
    return;

err:
    dstlang[0] = 0;
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
    FILE *f;
    char *f_fname = talloc_strdup(NULL, mp_basename(fname));
    size_t len = strlen(f_fname) + 1;
    char *f_fname_noext = talloc_size(f_fname, len);
    char *f_fname_trim  = talloc_size(f_fname, len);

    strcpy_strip_ext(f_fname_noext, f_fname);
    strcpy_trim(f_fname_trim, f_fname_noext);

    // 0 = nothing
    // 1 = any subtitle file
    // 2 = any sub file containing movie name
    // 3 = sub file containing movie name and the lang extension
    char *path0 = bstrdup0(f_fname, path);
    DIR *d = opendir(path0);
    if (d) {
        struct dirent *de;
        mp_msg(MSGT_SUBREADER, MSGL_INFO, "Load subtitles in %.*s\n", BSTR_P(path));
        while ((de = readdir(d))) {
            len = strlen(de->d_name) + 1;
            char *tmp_fname_noext = talloc_size(NULL, len);
            char *tmp_fname_ext   = talloc_size(tmp_fname_noext, len);
            char *tmp_fname_trim  = talloc_size(tmp_fname_noext, len);

            // retrieve various parts of the filename
            strcpy_strip_ext(tmp_fname_noext, de->d_name);
            strcpy_get_ext(tmp_fname_ext, de->d_name);
            strcpy_trim(tmp_fname_trim, tmp_fname_noext);

            // If it's a .sub, check if there is a .idx with the same name. If
            // there is one, it's certainly a vobsub so we skip it.
            if (strcasecmp(tmp_fname_ext, "sub") == 0) {
                struct bstr idxname = BSTR(talloc_strdup(NULL, de->d_name));
                strcpy(idxname.start + idxname.len - sizeof("idx") + 1, "idx");
                char *idx = mp_path_join(idxname.start, path, idxname);
                f = fopen(idx, "rt");
                talloc_free(idxname.start);
                if (f) {
                    fclose(f);
                    goto next_sub;
                }
            }

            // does it end with a subtitle extension?
            int i, found = 0;
#ifdef CONFIG_ICONV
#ifdef CONFIG_ENCA
            for (i = ((sub_cp && strncasecmp(sub_cp, "enca", 4) != 0) ? 3 : 0); sub_exts[i]; i++) {
#else
            for (i = (sub_cp ? 3 : 0); sub_exts[i]; i++) {
#endif
#else
            for (i = 0; sub_exts[i]; i++) {
#endif
                if (strcasecmp(sub_exts[i], tmp_fname_ext) == 0) {
                    found = 1;
                    break;
                }
            }

            // we have a (likely) subtitle file
            if (found) {
                int prio = 0;
                if (opts->sub_lang) {
                    char lang[4];

                    if (!strncmp(tmp_fname_trim, f_fname_trim,
                                 strlen(f_fname_trim))) {
                        guess_lang_from_filename(lang, tmp_fname_trim);
                        if (*lang) {
                            for (int n = 0; opts->sub_lang[n]; n++) {
                                if (!strncmp(lang, opts->sub_lang[n],
                                            strlen(opts->sub_lang[n]))) {
                                    prio = 4; // matches the movie name + lang extension
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!prio && strcmp(tmp_fname_trim, f_fname_trim) == 0)
                    prio = 3; // matches the movie name
                if (!prio && strstr(tmp_fname_trim, f_fname_trim)
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
                    char *subpath = mp_path_join(*slist, path, BSTR(de->d_name));
                    if ((f = fopen(subpath, "rt"))) {
                        MP_GROW_ARRAY(*slist, *nsub);
                        struct subfn *sub = *slist + (*nsub)++;

                        fclose(f);
                        sub->priority = prio;
                        sub->fname    = subpath;
                    } else
                        talloc_free(subpath);
                }
            }

next_sub:
            talloc_free(tmp_fname_noext);
        }
        closedir(d);
    }

    talloc_free(f_fname);
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
                                      BSTR(opts->sub_paths[i]));
            append_dir_subtitles(opts, &slist, &n, BSTR(path), fname, 0);
        }
    }

    // Load subtitles in ~/.mplayer/sub limiting sub fuzziness
    char *mp_subdir = get_path("sub/");
    if (mp_subdir)
        append_dir_subtitles(opts, &slist, &n, BSTR(mp_subdir), fname, 1);
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
    struct bstr bname = BSTR(mp_basename(fname));
    int pdot = bstrrchr(bname, '.');
    if (pdot >= 0)
        bname.len = pdot;
    vobs[n++] = mp_path_join(vobs, mp_dirname(fname), bname);

    // Potential vobsubs in directories specified by sub-paths option
    if (opts->sub_paths) {
        for (int i = 0; opts->sub_paths[i]; i++) {
            char *path = mp_path_join(NULL, mp_dirname(fname),
                                      BSTR(opts->sub_paths[i]));
            MP_GROW_ARRAY(vobs, n);
            vobs[n++] = mp_path_join(vobs, BSTR(path), bname);
            talloc_free(path);
        }
    }

    // Potential vobsub in ~/.mplayer/sub
    char *mp_subdir = get_path("sub/");
    if (mp_subdir) {
        MP_GROW_ARRAY(vobs, n);
        vobs[n++] = mp_path_join(vobs, BSTR(mp_subdir), bname);
    }

    free(mp_subdir);
    MP_RESIZE_ARRAY(NULL, vobs, n);
    return vobs;
}
