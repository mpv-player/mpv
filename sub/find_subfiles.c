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

static int whiteonly(char *s)
{
    while (*s) {
        if (!isspace(*s))
            return 0;
        s++;
    }
    return 1;
}

typedef struct subfn {
    int priority;
    char *fname;
} subfn;

static int compare_sub_priority(const void *a, const void *b)
{
    if (((const subfn*)a)->priority > ((const subfn*)b)->priority) {
        return -1;
    } else if (((const subfn*)a)->priority < ((const subfn*)b)->priority) {
        return 1;
    } else {
        return strcoll(((const subfn*)a)->fname, ((const subfn*)b)->fname);
    }
}

/**
 * @brief Append all the subtitles in the given path matching fname
 * @param slist pointer to the subtitles list tallocated
 * @param nsub pointer to the number of subtitles
 * @param path Look for subtitles in this directory
 * @param fname Subtitle filename (pattern)
 * @param limit_fuzziness Ignore flag when sub_fuziness == 2
 */
static void append_dir_subtitles(struct subfn **slist, int *nsub,
                                 struct bstr path, const char *fname,
                                 int limit_fuzziness)
{
    char *f_fname, *f_fname_noext, *f_fname_trim, *tmp, *tmp_sub_id;
    char *tmp_fname_noext, *tmp_fname_trim, *tmp_fname_ext, *tmpresult;

    int len, found, i;
    char *sub_exts[] = {"utf", "utf8", "utf-8", "sub", "srt", "smi", "rt", "txt", "ssa", "aqt", "jss", "js", "ass", NULL};
    FILE *f;

    DIR *d;
    struct dirent *de;

    len =   (strlen(fname) > 256 ? strlen(fname) : 256)
          + (path.len      > 256 ? path.len      : 256) + 2;

    f_fname       = mp_basename(fname);
    f_fname_noext = malloc(len);
    f_fname_trim  = malloc(len);

    tmp_fname_noext = malloc(len);
    tmp_fname_trim  = malloc(len);
    tmp_fname_ext   = malloc(len);

    tmpresult = malloc(len);

    strcpy_strip_ext(f_fname_noext, f_fname);
    strcpy_trim(f_fname_trim, f_fname_noext);

    /* The code using sub language here is broken - it assumes strict
     * "videoname languagename" syntax for the subtitle file, which is
     * very unlikely to match especially if language name uses "en,de"
     * syntax... */
    tmp_sub_id = NULL;
#if 0
    if (dvdsub_lang && !whiteonly(dvdsub_lang)) {
        tmp_sub_id = malloc(strlen(dvdsub_lang) + 1);
        strcpy_trim(tmp_sub_id, dvdsub_lang);
    }
#endif

    // 0 = nothing
    // 1 = any subtitle file
    // 2 = any sub file containing movie name
    // 3 = sub file containing movie name and the lang extension
    char *path0 = bstrdup0(NULL, path);
    d = opendir(path0);
    talloc_free(path0);
    if (d) {
        mp_msg(MSGT_SUBREADER, MSGL_INFO, "Load subtitles in %.*s\n", BSTR_P(path));
        while ((de = readdir(d))) {
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
                    continue;
                }
            }

            // does it end with a subtitle extension?
            found = 0;
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
                if (!prio && tmp_sub_id) {
                    sprintf(tmpresult, "%s %s", f_fname_trim, tmp_sub_id);
                    if (strcmp(tmp_fname_trim, tmpresult) == 0 && sub_match_fuzziness >= 1) {
                        // matches the movie name + lang extension
                        prio = 5;
                    }
                }
                if (!prio && strcmp(tmp_fname_trim, f_fname_trim) == 0) {
                    // matches the movie name
                    prio = 4;
                }
                if (!prio && (tmp = strstr(tmp_fname_trim, f_fname_trim)) && sub_match_fuzziness >= 1) {
                    // contains the movie name
                    tmp += strlen(f_fname_trim);
                    if (tmp_sub_id && strstr(tmp, tmp_sub_id)) {
                        // with sub_id specified prefer localized subtitles
                        prio = 3;
                    } else if ((tmp_sub_id == NULL) && whiteonly(tmp)) {
                        // without sub_id prefer "plain" name
                        prio = 3;
                    } else {
                        // with no localized subs found, try any else instead
                        prio = 2;
                    }
                }
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
        }
        closedir(d);
    }

    free(tmp_sub_id);

    free(f_fname_noext);
    free(f_fname_trim);

    free(tmp_fname_noext);
    free(tmp_fname_trim);
    free(tmp_fname_ext);

    free(tmpresult);
}

char **find_text_subtitles(struct MPOpts *opts, const char *fname)
{
    char **subnames = NULL;
    struct subfn *slist = talloc_array_ptrtype(NULL, slist, 1);
    int n = 0;

    // Load subtitles from current media directory
    append_dir_subtitles(&slist, &n, mp_dirname(fname), fname, 0);

    // Load subtitles in dirs specified by sub-paths option
    if (opts->sub_paths) {
        for (int i = 0; opts->sub_paths[i]; i++) {
            char *path = mp_path_join(slist, mp_dirname(fname),
                                      BSTR(opts->sub_paths[i]));
            append_dir_subtitles(&slist, &n, BSTR(path), fname, 0);
        }
    }

    // Load subtitles in ~/.mplayer/sub limiting sub fuzziness
    char *mp_subdir = get_path("sub/");
    if (mp_subdir)
        append_dir_subtitles(&slist, &n, BSTR(mp_subdir), fname, 1);
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
