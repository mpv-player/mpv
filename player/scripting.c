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
#include <sys/types.h>
#include <math.h>
#include <assert.h>

#include "config.h"

#include "osdep/io.h"
#include "osdep/subprocess.h"
#include "osdep/threads.h"

#include "common/common.h"
#include "common/msg.h"
#include "input/input.h"
#include "options/m_config.h"
#include "options/parse_configfile.h"
#include "options/path.h"
#include "misc/bstr.h"
#include "core.h"
#include "client.h"
#include "libmpv/client.h"
#include "libmpv/render.h"
#include "libmpv/stream_cb.h"

extern const struct mp_scripting mp_scripting_lua;
extern const struct mp_scripting mp_scripting_cplugin;
extern const struct mp_scripting mp_scripting_js;
extern const struct mp_scripting mp_scripting_run;

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
    &mp_scripting_run,
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

static void run_script(struct mp_script_args *arg)
{
    char *name = talloc_asprintf(NULL, "%s/%s", arg->backend->name,
                                 mpv_client_name(arg->client));
    mp_thread_set_name(name);
    talloc_free(name);

    if (arg->backend->load(arg) < 0)
        MP_ERR(arg, "Could not load %s script %s\n", arg->backend->name, arg->filename);

    mpv_destroy(arg->client);
    talloc_free(arg);
}

static MP_THREAD_VOID script_thread(void *p)
{
    struct mp_script_args *arg = p;
    run_script(arg);

    MP_THREAD_RETURN();
}

static int64_t mp_load_script(struct MPContext *mpctx, const char *fname)
{
    char *ext = mp_splitext(fname, NULL);
    if (ext && strcasecmp(ext, "disable") == 0)
        return 0;

    void *tmp = talloc_new(NULL);

    const char *path = NULL;
    char *script_name = NULL;
    const struct mp_scripting *backend = NULL;

    struct stat s;
    if (!stat(fname, &s) && S_ISDIR(s.st_mode)) {
        path = fname;
        fname = NULL;

        for (int n = 0; scripting_backends[n]; n++) {
            const struct mp_scripting *b = scripting_backends[n];
            char *filename = mp_tprintf(80, "main.%s", b->file_ext);
            fname = mp_path_join(tmp, path, filename);
            if (!stat(fname, &s) && S_ISREG(s.st_mode)) {
                backend = b;
                break;
            }
            talloc_free((void *)fname);
            fname = NULL;
        }

        if (!fname) {
            MP_ERR(mpctx, "Cannot find main.* for any supported scripting "
                   "backend in: %s\n", path);
            talloc_free(tmp);
            return -1;
        }

        script_name = talloc_strdup(tmp, path);
        mp_path_strip_trailing_separator(script_name);
        script_name = mp_basename(script_name);
    } else {
        for (int n = 0; scripting_backends[n]; n++) {
            const struct mp_scripting *b = scripting_backends[n];
            if (ext && strcasecmp(ext, b->file_ext) == 0) {
                backend = b;
                break;
            }
        }
        script_name = script_name_from_filename(tmp, fname);
    }

    if (!backend) {
        MP_ERR(mpctx, "Can't load unknown script: %s\n", fname);
        talloc_free(tmp);
        return -1;
    }

    struct mp_script_args *arg = talloc_ptrtype(NULL, arg);
    *arg = (struct mp_script_args){
        .mpctx = mpctx,
        .filename = talloc_strdup(arg, fname),
        .path = talloc_strdup(arg, path),
        .backend = backend,
        // Create the client before creating the thread; otherwise a race
        // condition could happen, where MPContext is destroyed while the
        // thread tries to create the client.
        .client = mp_new_client(mpctx->clients, script_name),
    };

    talloc_free(tmp);
    fname = NULL; // might have been freed so don't touch anymore

    if (!arg->client) {
        MP_ERR(mpctx, "Failed to create client for script: %s\n", arg->filename);
        talloc_free(arg);
        return -1;
    }

    mp_client_set_weak(arg->client);
    arg->log = mp_client_get_log(arg->client);
    int64_t id = mpv_client_id(arg->client);

    MP_DBG(arg, "Loading %s script %s...\n", backend->name, arg->filename);

    if (backend->no_thread) {
        run_script(arg);
    } else {
        mp_thread thread;
        if (mp_thread_create(&thread, script_thread, arg)) {
            mpv_destroy(arg->client);
            talloc_free(arg);
            return -1;
        }
        mp_thread_detach(thread);
    }

    return id;
}

int64_t mp_load_user_script(struct MPContext *mpctx, const char *fname)
{
    char *path = mp_get_user_path(NULL, mpctx->global, fname);
    int64_t ret = mp_load_script(mpctx, path);
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
        if (ep->d_name[0] != '.') {
            char *fname = mp_path_join(talloc_ctx, path, ep->d_name);
            struct stat s;
            if (!stat(fname, &s) && (S_ISREG(s.st_mode) || S_ISDIR(s.st_mode)))
                MP_TARRAY_APPEND(talloc_ctx, files, count, fname);
        }
    }
    closedir(dp);
    if (files)
        qsort(files, count, sizeof(char *), compare_filename);
    MP_TARRAY_APPEND(talloc_ctx, files, count, NULL);
    return files;
}

static void load_builtin_script(struct MPContext *mpctx, int slot, bool enable,
                                const char *fname)
{
    assert(slot < MP_ARRAY_SIZE(mpctx->builtin_script_ids));
    int64_t *pid = &mpctx->builtin_script_ids[slot];
    if (*pid > 0 && !mp_client_id_exists(mpctx, *pid))
        *pid = 0; // died
    if ((*pid > 0) != enable) {
        if (enable) {
            *pid = mp_load_script(mpctx, fname);
        } else {
            char *name = mp_tprintf(22, "@%"PRIi64, *pid);
            mp_client_send_event(mpctx, name, 0, MPV_EVENT_SHUTDOWN, NULL);
        }
    }
}

void mp_load_builtin_scripts(struct MPContext *mpctx)
{
    load_builtin_script(mpctx, 0, mpctx->opts->lua_load_osc, "@osc.lua");
    load_builtin_script(mpctx, 1, mpctx->opts->lua_load_ytdl, "@ytdl_hook.lua");
    load_builtin_script(mpctx, 2, mpctx->opts->lua_load_stats, "@stats.lua");
    load_builtin_script(mpctx, 3, mpctx->opts->lua_load_console, "@console.lua");
    load_builtin_script(mpctx, 4, mpctx->opts->lua_load_auto_profiles,
                        "@auto_profiles.lua");
    load_builtin_script(mpctx, 5, mpctx->opts->lua_load_select, "@select.lua");
}

bool mp_load_scripts(struct MPContext *mpctx)
{
    bool ok = true;

    // Load scripts from options
    char **files = mpctx->opts->script_files;
    for (int n = 0; files && files[n]; n++) {
        if (files[n][0])
            ok &= mp_load_user_script(mpctx, files[n]) >= 0;
    }
    if (!mpctx->opts->auto_load_scripts)
        return ok;

    // Load all scripts
    void *tmp = talloc_new(NULL);
    char **scriptsdir = mp_find_all_config_files(tmp, mpctx->global, "scripts");
    for (int i = 0; scriptsdir && scriptsdir[i]; i++) {
        files = list_script_files(tmp, scriptsdir[i]);
        for (int n = 0; files && files[n]; n++)
            ok &= mp_load_script(mpctx, files[n]) >= 0;
    }
    talloc_free(tmp);

    return ok;
}

#if HAVE_CPLUGINS

#ifndef _WIN32
#include <dlfcn.h>
#endif

#define MPV_DLOPEN_FN "mpv_open_cplugin"
typedef int (*mpv_open_cplugin)(mpv_handle *handle);

static void init_sym_table(struct mp_script_args *args, void *lib) {
#define INIT_SYM(name)                                                         \
    {                                                                          \
        void **sym = (void **)dlsym(lib, "pfn_" #name);                        \
        if (sym) {                                                             \
            if (*sym && *sym != &name)                                         \
                MP_ERR(args, "Overriding already set function " #name "\n");   \
            *sym = &name;                                                      \
        }                                                                      \
    }

    INIT_SYM(mpv_client_api_version);
    INIT_SYM(mpv_error_string);
    INIT_SYM(mpv_free);
    INIT_SYM(mpv_client_name);
    INIT_SYM(mpv_client_id);
    INIT_SYM(mpv_create);
    INIT_SYM(mpv_initialize);
    INIT_SYM(mpv_destroy);
    INIT_SYM(mpv_terminate_destroy);
    INIT_SYM(mpv_create_client);
    INIT_SYM(mpv_create_weak_client);
    INIT_SYM(mpv_load_config_file);
    INIT_SYM(mpv_get_time_us);
    INIT_SYM(mpv_free_node_contents);
    INIT_SYM(mpv_set_option);
    INIT_SYM(mpv_set_option_string);
    INIT_SYM(mpv_command);
    INIT_SYM(mpv_command_node);
    INIT_SYM(mpv_command_ret);
    INIT_SYM(mpv_command_string);
    INIT_SYM(mpv_command_async);
    INIT_SYM(mpv_command_node_async);
    INIT_SYM(mpv_abort_async_command);
    INIT_SYM(mpv_set_property);
    INIT_SYM(mpv_set_property_string);
    INIT_SYM(mpv_del_property);
    INIT_SYM(mpv_set_property_async);
    INIT_SYM(mpv_get_property);
    INIT_SYM(mpv_get_property_string);
    INIT_SYM(mpv_get_property_osd_string);
    INIT_SYM(mpv_get_property_async);
    INIT_SYM(mpv_observe_property);
    INIT_SYM(mpv_unobserve_property);
    INIT_SYM(mpv_event_name);
    INIT_SYM(mpv_event_to_node);
    INIT_SYM(mpv_request_event);
    INIT_SYM(mpv_request_log_messages);
    INIT_SYM(mpv_wait_event);
    INIT_SYM(mpv_wakeup);
    INIT_SYM(mpv_set_wakeup_callback);
    INIT_SYM(mpv_wait_async_requests);
    INIT_SYM(mpv_hook_add);
    INIT_SYM(mpv_hook_continue);
    INIT_SYM(mpv_get_wakeup_pipe);

    INIT_SYM(mpv_render_context_create);
    INIT_SYM(mpv_render_context_set_parameter);
    INIT_SYM(mpv_render_context_get_info);
    INIT_SYM(mpv_render_context_set_update_callback);
    INIT_SYM(mpv_render_context_update);
    INIT_SYM(mpv_render_context_render);
    INIT_SYM(mpv_render_context_report_swap);
    INIT_SYM(mpv_render_context_free);

    INIT_SYM(mpv_stream_cb_add_ro);

#undef INIT_SYM
}

static int load_cplugin(struct mp_script_args *args)
{
    void *lib = dlopen(args->filename, RTLD_NOW | RTLD_LOCAL);
    if (!lib)
        goto error;
    // Note: once loaded, we never unload, as unloading the libraries linked to
    //       the plugin can cause random serious problems.
    mpv_open_cplugin sym = (mpv_open_cplugin)dlsym(lib, MPV_DLOPEN_FN);
    if (!sym)
        goto error;

    init_sym_table(args, lib);

    return sym(args->client) ? -1 : 0;
error: ;
    char *err = dlerror();
    if (err)
        MP_ERR(args, "C plugin error: '%s'\n", err);
    return -1;
}

const struct mp_scripting mp_scripting_cplugin = {
    .name = "cplugin",
    #ifdef _WIN32
    .file_ext = "dll",
    #else
    .file_ext = "so",
    #endif
    .load = load_cplugin,
};

#endif

static int load_run(struct mp_script_args *args)
{
    // The arg->client object might die and with it args->log, so duplicate it.
    args->log = mp_log_new(args, args->log, NULL);

    int fds[2];
    if (!mp_ipc_start_anon_client(args->mpctx->ipc_ctx, args->client, fds))
        return -1;
    args->client = NULL; // ownership lost

    char *fdopt = fds[1] >= 0 ? mp_tprintf(80, "--mpv-ipc-fd=%d:%d", fds[0], fds[1])
                              : mp_tprintf(80, "--mpv-ipc-fd=%d", fds[0]);

    struct mp_subprocess_opts opts = {
        .exe = (char *)args->filename,
        .args = (char *[]){(char *)args->filename, fdopt, NULL},
        .fds = {
            // Keep terminal stuff
            {.fd = 0, .src_fd = 0,},
            {.fd = 1, .src_fd = 1,},
            {.fd = 2, .src_fd = 2,},
            // Just hope these don't step over each other (e.g. fds[1] could be
            // below 4, if the std FDs are missing).
            {.fd = fds[0], .src_fd = fds[0], },
            {.fd = fds[1], .src_fd = fds[1], },
        },
        .num_fds = fds[1] >= 0 ? 5 : 4,
        .detach = true,
    };
    struct mp_subprocess_result res;
    mp_subprocess2(&opts, &res);

    // Closing these will (probably) make the client exit, if it really died.
    // They _should_ be CLOEXEC, but are not, because
    // posix_spawn_file_actions_adddup2() may not clear the CLOEXEC flag
    // properly if by coincidence fd==src_fd.
    close(fds[0]);
    if (fds[1] >= 0)
        close(fds[1]);

    if (res.error < 0) {
        MP_ERR(args, "Starting '%s' failed: %s\n", args->filename,
               mp_subprocess_err_str(res.error));
        return -1;
    }

    return 0;
}

const struct mp_scripting mp_scripting_run = {
    .name = "ipc",
    .file_ext = "run",
    .no_thread = true,
    .load = load_run,
};
