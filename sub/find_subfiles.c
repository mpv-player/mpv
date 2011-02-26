#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "mp_msg.h"
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

char **sub_filenames(const char *path, char *fname)
{
    char *f_dir, *f_fname, *f_fname_noext, *f_fname_trim, *tmp, *tmp_sub_id;
    char *tmp_fname_noext, *tmp_fname_trim, *tmp_fname_ext, *tmpresult;

    int len, pos, found, i, j;
    char *sub_exts[] = {"utf", "utf8", "utf-8", "sub", "srt", "smi", "rt", "txt", "ssa", "aqt", "jss", "js", "ass", NULL};
    subfn *result;
    char **result2;

    int subcnt;

    FILE *f;

    DIR *d;
    struct dirent *de;

    len =   (strlen(fname) > 256 ? strlen(fname) : 256)
          + (strlen(path)  > 256 ? strlen(path)  : 256) + 2;

    f_dir         = malloc(len);
    f_fname       = malloc(len);
    f_fname_noext = malloc(len);
    f_fname_trim  = malloc(len);

    tmp_fname_noext = malloc(len);
    tmp_fname_trim  = malloc(len);
    tmp_fname_ext   = malloc(len);

    tmpresult = malloc(len);

    result = calloc(MAX_SUBTITLE_FILES, sizeof(*result));

    subcnt = 0;

    tmp = strrchr(fname,'/');
#if HAVE_DOS_PATHS
    if(!tmp)tmp = strrchr(fname,'\\');
    if(!tmp)tmp = strrchr(fname,':');
#endif

    // extract filename & dirname from fname
    if (tmp) {
        strcpy(f_fname, tmp+1);
        pos = tmp - fname;
        strncpy(f_dir, fname, pos+1);
        f_dir[pos+1] = 0;
    } else {
        strcpy(f_fname, fname);
        strcpy(f_dir, "./");
    }

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
    for (j = 0; j <= 1; j++) {
        d = opendir(j == 0 ? f_dir : path);
        if (d) {
            while ((de = readdir(d))) {
                // retrieve various parts of the filename
                strcpy_strip_ext(tmp_fname_noext, de->d_name);
                strcpy_get_ext(tmp_fname_ext, de->d_name);
                strcpy_trim(tmp_fname_trim, tmp_fname_noext);

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
                        if (j == 0 && sub_match_fuzziness >= 2) {
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
                        sprintf(tmpresult, "%s%s", j == 0 ? f_dir : path, de->d_name);
                        // fprintf(stderr, "%s priority %d\n", tmpresult, prio);
                        if ((f = fopen(tmpresult, "rt"))) {
                            fclose(f);
                            result[subcnt].priority = prio;
                            result[subcnt].fname = strdup(tmpresult);
                            subcnt++;
                        }
                    }

                }
                if (subcnt >= MAX_SUBTITLE_FILES) break;
            }
            closedir(d);
        }

    }

    free(tmp_sub_id);

    free(f_dir);
    free(f_fname);
    free(f_fname_noext);
    free(f_fname_trim);

    free(tmp_fname_noext);
    free(tmp_fname_trim);
    free(tmp_fname_ext);

    free(tmpresult);

    qsort(result, subcnt, sizeof(subfn), compare_sub_priority);

    result2 = calloc(subcnt + 1, sizeof(*result2));

    for (i = 0; i < subcnt; i++)
        result2[i] = result[i].fname;
    result2[subcnt] = NULL;

    free(result);

    return result2;
}
