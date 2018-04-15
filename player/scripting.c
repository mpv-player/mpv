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
#include <strings.h>
#include <sys/types.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <assert.h>

#include "config.h"

#include "osdep/io.h"
#include "osdep/threads.h"

#include "common/common.h"
#include "common/msg.h"
#include "options/path.h"
#include "misc/bstr.h"
#include "core.h"
#include "client.h"
#include "libmpv/client.h"

extern const struct mp_scripting mp_scripting_lua;
extern const struct mp_scripting mp_scripting_cplugin;
extern const struct mp_scripting mp_scripting_js;

static const struct mp_scripting *const scripting_backends[] = {
#if HAVE_LUA
    &mp_scripting_lua,
#endif
#if HAVE_CPLUGINS
    &mp_scripting_cplugin,
#endif
#if HAVE_JAVASCRIPT
    &mp_scripting_js,
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
    struct mp_log *log;
    const struct mp_scripting *backend;
    mpv_handle *client;
    const char *fname;
};

static void *script_thread(void *p)
{
    pthread_detach(pthread_self());

    struct thread_arg *arg = p;

    char name[90];
    snprintf(name, sizeof(name), "%s (%s)", arg->backend->name,
             mpv_client_name(arg->client));
    mpthread_set_name(name);

    if (arg->backend->load(arg->client, arg->fname) < 0)
        MP_ERR(arg, "Could not load %s %s\n", arg->backend->name, arg->fname);

    mpv_destroy(arg->client);
    talloc_free(arg);
    return NULL;
}

static int mp_load_script(struct MPContext *mpctx, const char *fname)
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
        return -1;
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
        return -1;
    }
    mp_client_set_weak(arg->client);
    arg->log = mp_client_get_log(arg->client);

    MP_DBG(arg, "Loading %s %s...\n", backend->name, fname);

    pthread_t thread;
    if (pthread_create(&thread, NULL, script_thread, arg)) {
        mpv_destroy(arg->client);
        talloc_free(arg);
        return -1;
    }

    return 0;
}

int mp_load_user_script(struct MPContext *mpctx, const char *fname)
{
    char *path = mp_get_user_path(NULL, mpctx->global, fname);
    int ret = mp_load_script(mpctx, path);
    talloc_free(path);
    return ret;
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
        char *fname = mp_path_join(talloc_ctx, path, ep->d_name);
        struct stat s;
        if (!stat(fname, &s) && S_ISREG(s.st_mode))
            MP_TARRAY_APPEND(talloc_ctx, files, count, fname);
    }
    closedir(dp);
    if (files)
        qsort(files, count, sizeof(char *), compare_filename);
    MP_TARRAY_APPEND(talloc_ctx, files, count, NULL);
    return files;
}

static void load_builtin_script(struct MPContext *mpctx, bool enable,
                                const char *fname)
{
    void *tmp = talloc_new(NULL);
    // (The name doesn't have to match if there were conflicts with other
    // scripts, so this is on best-effort basis.)
    char *name = script_name_from_filename(tmp, fname);
    if (enable != mp_client_exists(mpctx, name)) {
        if (enable) {
            mp_load_script(mpctx, fname);
        } else {
            // Try to unload it by sending a shutdown event. This can be
            // unreliable, because user scripts could have clashing names, or
            // disabling and then quickly re-enabling a builtin script might
            // detect the still-terminating script as loaded.
            mp_client_send_event(mpctx, name, 0, MPV_EVENT_SHUTDOWN, NULL);
        }
    }
    talloc_free(tmp);
}

void mp_load_builtin_scripts(struct MPContext *mpctx)
{
    load_builtin_script(mpctx, mpctx->opts->lua_load_osc, "@osc.lua");
    load_builtin_script(mpctx, mpctx->opts->lua_load_ytdl, "@ytdl_hook.lua");
    load_builtin_script(mpctx, mpctx->opts->lua_load_stats, "@stats.lua");
}

void mp_load_scripts(struct MPContext *mpctx)
{
    // Load scripts from options
    char **files = mpctx->opts->script_files;
    for (int n = 0; files && files[n]; n++) {
        if (files[n][0])
            mp_load_user_script(mpctx, files[n]);
    }
    if (!mpctx->opts->auto_load_scripts)
        return;

    // Load all scripts
    void *tmp = talloc_new(NULL);
    char **scriptsdir = mp_find_all_config_files(tmp, mpctx->global, "scripts");
    for (int i = 0; scriptsdir && scriptsdir[i]; i++) {
        files = list_script_files(tmp, scriptsdir[i]);
        for (int n = 0; files && files[n]; n++)
            mp_load_script(mpctx, files[n]);
    }
    talloc_free(tmp);
}

#if HAVE_CPLUGINS

#include <dlfcn.h>

#define MPV_DLOPEN_FN "mpv_open_cplugin"
typedef int (*mpv_open_cplugin)(mpv_handle *handle);

static int load_cplugin(struct mpv_handle *client, const char *fname)
{
    MPContext *ctx = mp_client_get_core(client);
    void *lib = dlopen(fname, RTLD_NOW | RTLD_LOCAL);
    if (!lib)
        goto error;
    // Note: once loaded, we never unload, as unloading the libraries linked to
    //       the plugin can cause random serious problems.
    mpv_open_cplugin sym = (mpv_open_cplugin)dlsym(lib, MPV_DLOPEN_FN);
    if (!sym)
        goto error;
    return sym(client) ? -1 : 0;
error: ;
    char *err = dlerror();
    if (err)
        MP_ERR(ctx, "C plugin error: '%s'\n", err);
    return -1;
}

const struct mp_scripting mp_scripting_cplugin = {
    .name = "SO plugin",
    .file_ext = "so",
    .load = load_cplugin,
};

#endif
