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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>

#include "osdep/io.h"

#include "config.h"

#if HAVE_GLOB
#include <glob.h>
#else
#include "osdep/glob.h"
#endif

#include "talloc.h"
#include "common/msg.h"
#include "stream/stream.h"
#include "options/path.h"

#include "mf.h"

double mf_fps = 1.0;
char *mf_type = NULL;  //"jpg";

static void mf_add(mf_t *mf, const char *fname)
{
    char *entry = talloc_strdup(mf, fname);
    MP_TARRAY_APPEND(mf, mf->names, mf->nr_of_files, entry);
}

mf_t *open_mf_pattern(void *talloc_ctx, struct mp_log *log, char *filename)
{
#if defined(HAVE_GLOB) || defined(__MINGW32__)
    int error_count = 0;
    int count = 0;

    mf_t *mf = talloc_zero(talloc_ctx, mf_t);
    mf->log = log;

    if (filename[0] == '@') {
        FILE *lst_f = fopen(filename + 1, "r");
        if (lst_f) {
            char *fname = talloc_size(mf, 512);
            while (fgets(fname, 512, lst_f)) {
                /* remove spaces from end of fname */
                char *t = fname + strlen(fname) - 1;
                while (t > fname && isspace((unsigned char)*t))
                    *(t--) = 0;
                if (!mp_path_exists(fname)) {
                    mp_verbose(log, "file not found: '%s'\n", fname);
                } else {
                    mf_add(mf, fname);
                }
            }
            fclose(lst_f);

            mp_info(log, "number of files: %d\n", mf->nr_of_files);
            goto exit_mf;
        }
        mp_info(log, "%s is not indirect filelist\n", filename + 1);
    }

    if (strchr(filename, ',')) {
        mp_info(log, "filelist: %s\n", filename);
        bstr bfilename = bstr0(filename);

        while (bfilename.len) {
            bstr bfname;
            bstr_split_tok(bfilename, ",", &bfname, &bfilename);
            char *fname2 = bstrdup0(mf, bfname);

            if (!mp_path_exists(fname2))
                mp_verbose(log, "file not found: '%s'\n", fname2);
            else {
                mf_add(mf, fname2);
            }
            talloc_free(fname2);
        }
        mp_info(log, "number of files: %d\n", mf->nr_of_files);

        goto exit_mf;
    }

    char *fname = talloc_size(mf, strlen(filename) + 32);

    if (!strchr(filename, '%')) {
        strcpy(fname, filename);
        if (!strchr(filename, '*'))
            strcat(fname, "*");

        mp_info(log, "search expr: %s\n", fname);

        glob_t gg;
        if (glob(fname, 0, NULL, &gg)) {
            talloc_free(mf);
            return NULL;
        }

        for (int i = 0; i < gg.gl_pathc; i++) {
            if (mp_path_isdir(gg.gl_pathv[i]))
                continue;
            mf_add(mf, gg.gl_pathv[i]);
        }
        mp_info(log, "number of files: %d\n", mf->nr_of_files);
        globfree(&gg);
        goto exit_mf;
    }

    mp_info(log, "search expr: %s\n", filename);

    while (error_count < 5) {
        sprintf(fname, filename, count++);
        if (!mp_path_exists(fname)) {
            error_count++;
            mp_verbose(log, "file not found: '%s'\n", fname);
        } else {
            mf_add(mf, fname);
        }
    }

    mp_info(log, "number of files: %d\n", mf->nr_of_files);

exit_mf:
    return mf;
#else
    mp_fatal(log, "mf support is disabled on your os\n");
    return 0;
#endif
}

mf_t *open_mf_single(void *talloc_ctx, struct mp_log *log, char *filename)
{
    mf_t *mf = talloc_zero(talloc_ctx, mf_t);
    mf->log = log;
    mf_add(mf, filename);
    return mf;
}
