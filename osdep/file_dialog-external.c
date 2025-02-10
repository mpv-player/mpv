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

#include "file_dialog.h"

#include <player/core.h>

#include "subprocess.h"

#define MAX_ARGS 32

struct subprocess_fd_ctx {
    struct mp_log *log;
    void* talloc_ctx;
    int fd;
    bstr output;
};

static void subprocess_read(void *p, char *data, size_t size)
{
    struct subprocess_fd_ctx *ctx = p;
    if (ctx->fd == STDOUT_FILENO) {
        bstr_xappend(ctx->talloc_ctx, &ctx->output, (bstr){data, size});
    } else {
        // Redirect stderr to trace, because it can be spammy, and most of the
        // time not interesting GTK warnings/errors.
        mp_msg(ctx->log, MSGL_TRACE, "%.*s", (int)size, data);
    }
}

static void kdialog(void *talloc_ctx, const char *args[MAX_ARGS],
                    const mp_file_dialog_params *params)
{
    int i = 0;

    args[i++] = "kdialog";

    if (params->flags & MP_FILE_DIALOG_SAVE) {
        args[i++] = "--getsavefilename";
    } else if (params->flags & MP_FILE_DIALOG_DIRECTORY) {
        args[i++] = "--getexistingdirectory";
    } else {
        args[i++] = "--getopenfilename";
        if (params->flags & MP_FILE_DIALOG_MULTIPLE) {
            args[i++] = "--multiple";
            args[i++] = "--separate-output";
        }
    }

    args[i++] = params->initial_dir ? params->initial_dir : ".";

    if (params->filters) {
        char *filter = talloc_strdup(talloc_ctx, "All Files (*)");
        for (const mp_file_dialog_filters *f = params->filters; f->name; f++) {
            filter = talloc_asprintf_append(filter, "|%s (", f->name);
            for (char **ext = f->extensions; *ext; ext++)
                filter = talloc_asprintf_append(filter, ext[1] ?  "*.%s " : "*.%s)", *ext);
        }
        args[i++] = filter;
    }

    args[i++] = NULL;
}

static void zenity(void *talloc_ctx, const char *args[MAX_ARGS],
                   const mp_file_dialog_params *params)
{
    int i = 0;

    args[i++] = "zenity";
    args[i++] = "--file-selection";

    if (params->flags & MP_FILE_DIALOG_DIRECTORY) {
        args[i++] = "--directory";
    } else if (params->flags & MP_FILE_DIALOG_SAVE) {
        args[i++] = "--save";
    } else if (params->flags & MP_FILE_DIALOG_MULTIPLE) {
        args[i++] = "--multiple";
        args[i++] = "--separator=\n";
    }

    if (params->title) {
        args[i++] = "--title";
        args[i++] = params->title;
    }

    if (params->initial_dir) {
        args[i++] = "--filename";
        args[i++] = talloc_asprintf(talloc_ctx, "%s/", params->initial_dir);
    }

    if (params->filters) {
        args[i++] = "--file-filter";
        args[i++] = talloc_strdup(talloc_ctx, "All Files | *");
        for (const mp_file_dialog_filters *f = params->filters; f->name; f++) {
            if (i >= MAX_ARGS - 2) {
                assert(false);
                break;
            }
            char *filter = talloc_asprintf(talloc_ctx, "%s |", f->name);
            for (char **ext = f->extensions; *ext; ext++)
                filter = talloc_asprintf_append(filter, " *.%s", *ext);
            args[i++] = "--file-filter";
            args[i++] = filter;
        }
    }

    args[i++] = NULL;
}


typedef void (*dialog_func_t)(void *talloc_ctx, const char *args[MAX_ARGS],
                              const mp_file_dialog_params *params);

static const dialog_func_t dialogs[] = {
    kdialog,
    zenity,
    NULL,
};

char **mp_file_dialog_get_files(void *talloc_ctx, const mp_file_dialog_params *params)
{
    void *tmp = talloc_new(NULL);
    const char *args[MAX_ARGS] = {0};
    struct mp_subprocess_opts opts = {
        .args = (char **)args,
    };
    struct mp_log *log = mp_log_new(tmp, params->log, "file_dialog");
    struct subprocess_fd_ctx ctx[2];
    for (int fd = 1; fd < 3; fd++) {
        ctx[opts.num_fds] = (struct subprocess_fd_ctx) {
            .log = log,
            .talloc_ctx = tmp,
            .fd = fd,
        };
        opts.fds[opts.num_fds] = (struct mp_subprocess_fd) {
            .fd = fd,
            .src_fd = -1,
            .on_read = subprocess_read,
            .on_read_ctx = &ctx[opts.num_fds],
        };
        opts.num_fds++;
    }

    struct mp_subprocess_result res;

    for (int i = 0; dialogs[i]; i++) {
        dialogs[i](tmp, (const char **)opts.args, params);
        opts.exe = opts.args[0];
        mp_subprocess(mp_null_log, &opts, &res);
        if (res.error == MP_SUBPROCESS_OK)
            break;
    }

    char **ret = NULL;
    size_t ret_count = 0;
    if (res.error == MP_SUBPROCESS_OK && res.exit_status == 0) {
        bstr out = ctx[0].output;
        while (out.len) {
            bstr line = bstr_getline(out, &out);
            line = bstr_strip_linebreaks(line);
            char *item = bstrdup0(NULL, line);
            MP_TARRAY_APPEND(talloc_ctx, ret, ret_count, item);
            talloc_steal(ret, item);
        }
    }

    if (ret_count)
        MP_TARRAY_APPEND(talloc_ctx, ret, ret_count, NULL);

    talloc_free(tmp);
    return ret;
}
