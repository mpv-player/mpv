/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <assert.h>

#include "config.h"

#include "osdep/io.h"

#include "common/common.h"
#include "common/msg.h"
#include "options/path.h"
#include "bstr/bstr.h"
#include "core.h"
#include "client.h"
#include "libmpv/client.h"

extern const struct mp_scripting mp_scripting_lua;

static const struct mp_scripting *const scripting_backends[] = {
#if HAVE_LUA
    &mp_scripting_lua,
#endif
    NULL
};

static char *script_name_from_filename(void *talloc_ctx, const char *fname)
{
    fname = mp_basename(fname);
    if (fname[0] == '@')
        fname += 1;
    char *name = talloc_strdup(talloc_ctx, fname);
    // Drop file extension
    char *dot = strrchr(name, '.');
    if (dot)
        *dot = '\0';
    // Turn it into a safe identifier - this is used with e.g. dispatching
    // input via: "send scriptname ..."
    for (int n = 0; name[n]; n++) {
        char c = name[n];
        if (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z') &&
            !(c >= '0' && c <= '9'))
            name[n] = '_';
    }
    return talloc_asprintf(talloc_ctx, "%s", name);
}

struct thread_arg {
    const struct mp_scripting *backend;
    mpv_handle *client;
    const char *fname;
};

static void *script_thread(void *p)
{
    pthread_detach(pthread_self());

    struct thread_arg *arg = p;
    struct mp_log *log = mp_client_get_log(arg->client);

    mp_verbose(log, "Loading script...\n");

    if (arg->backend->load(arg->client, arg->fname) < 0)
        mp_err(log, "Could not load script %s\n", arg->fname);

    mp_verbose(log, "Exiting...\n");

    mpv_detach_destroy(arg->client);
    talloc_free(arg);
    return NULL;
}

static void mp_load_script(struct MPContext *mpctx, const char *fname)
{
    char *ext = mp_splitext(fname, NULL);
    const struct mp_scripting *backend = NULL;
    for (int n = 0; scripting_backends[n]; n++) {
        const struct mp_scripting *b = scripting_backends[n];
        if (ext && strcasecmp(ext, b->file_ext) == 0) {
            backend = b;
            break;
        }
    }

    if (!backend) {
        MP_VERBOSE(mpctx, "Can't load unknown script: %s\n", fname);
        return;
    }

    struct thread_arg *arg = talloc_ptrtype(NULL, arg);
    char *name = script_name_from_filename(arg, fname);
    *arg = (struct thread_arg){
        .fname = talloc_strdup(arg, fname),
        .backend = backend,
        // Create the client before creating the thread; otherwise a race
        // condition could happen, where MPContext is destroyed while the
        // thread tries to create the client.
        .client = mp_new_client(mpctx->clients, name),
    };
    if (!arg->client) {
        talloc_free(arg);
        return;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, script_thread, arg))
        talloc_free(arg);

    return;
}

static int compare_filename(const void *pa, const void *pb)
{
    char *a = (char *)pa;
    char *b = (char *)pb;
    return strcmp(a, b);
}

static char **list_script_files(void *talloc_ctx, char *path)
{
    char **files = NULL;
    int count = 0;
    DIR *dp = opendir(path);
    if (!dp)
        return NULL;
    struct dirent *ep;
    while ((ep = readdir(dp))) {
        char *fname = mp_path_join(talloc_ctx, bstr0(path), bstr0(ep->d_name));
        struct stat s;
        if (!mp_stat(fname, &s) && S_ISREG(s.st_mode))
            MP_TARRAY_APPEND(talloc_ctx, files, count, fname);
    }
    closedir(dp);
    qsort(files, count, sizeof(char *), compare_filename);
    MP_TARRAY_APPEND(talloc_ctx, files, count, NULL);
    return files;
}

void mp_load_scripts(struct MPContext *mpctx)
{
    // Load scripts from options
    if (mpctx->opts->lua_load_osc)
        mp_load_script(mpctx, "@osc.lua");
    char **files = mpctx->opts->lua_files;
    for (int n = 0; files && files[n]; n++) {
        if (files[n][0])
            mp_load_script(mpctx, files[n]);
    }
    if (!mpctx->opts->auto_load_scripts)
        return;
    // Load ~/.mpv/lua/*
    void *tmp = talloc_new(NULL);
    char *script_path = mp_find_user_config_file(tmp, mpctx->global, "lua");
    if (script_path) {
        files = list_script_files(tmp, script_path);
        for (int n = 0; files && files[n]; n++)
            mp_load_script(mpctx, files[n]);
    }
    talloc_free(tmp);
}
