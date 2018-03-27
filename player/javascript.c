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

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <math.h>
#include <stdint.h>

#include <mujs.h>

#include "osdep/io.h"
#include "mpv_talloc.h"
#include "common/common.h"
#include "options/m_property.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "options/m_option.h"
#include "input/input.h"
#include "options/path.h"
#include "misc/bstr.h"
#include "osdep/subprocess.h"
#include "osdep/timer.h"
#include "osdep/threads.h"
#include "osdep/getpid.h"
#include "stream/stream.h"
#include "sub/osd.h"
#include "core.h"
#include "command.h"
#include "client.h"
#include "libmpv/client.h"

// List of builtin modules and their contents as strings.
// All these are generated from player/javascript/*.js
static const char *const builtin_files[][3] = {
    {"@/defaults.js",
#   include "player/javascript/defaults.js.inc"
    },
    {0}
};

// Represents a loaded script. Each has its own js state.
struct script_ctx {
    const char *filename;
    struct mpv_handle *client;
    struct MPContext *mpctx;
    struct mp_log *log;
    char *last_error_str;
};

static struct script_ctx *jctx(js_State *J)
{
    return (struct script_ctx *)js_getcontext(J);
}

static mpv_handle *jclient(js_State *J)
{
    return jctx(J)->client;
}

/**********************************************************************
 *  conventions, MuJS notes and vm errors
 *********************************************************************/
// - push_foo functions are called from C and push a value to the vm stack.
//
// - JavaScript C functions are code which the vm can call as a js function.
//   By convention, script_bar and script__baz are js C functions. The former
//   is exposed to end users as bar, and _baz is for internal use.
//
// - js C functions get a fresh vm stack with their arguments, and may
//   manipulate their stack as they see fit. On exit, the vm considers the
//   top value of their stack as their return value, and GC the rest.
//
// - js C function's stack[0] is "this", and the rest (1, 2, ...) are the args.
//   On entry the stack has at least the number of args defined for the func,
//   padded with undefined if called with less, or bigger if called with more.
//
// - Almost all vm APIs (js_*) may throw an error - a longjmp to the last
//   recovery/catch point, which could skip releasing resources. Use protected
//   code (e.g. js_pcall) between aquisition and release. Alternatively, use
//   the autofree mechanism to manage it more easily. See more details below.
//
// - Unless named s_foo, all the functions at this file (inc. init) which
//   touch the vm may throw, but either cleanup resources regardless (mostly
//   autofree) or leave allocated resources on caller-provided talloc context
//   which the caller should release, typically with autofree (e.g. makenode).
//
// - Functions named s_foo (safe foo) never throw, return 0 on success, else 1.

/**********************************************************************
 *  mpv scripting API error handling
 *********************************************************************/
// - Errors may be thrown on some cases - the reason is at the exception.
//
// - Some APIs also set last error which can be fetched with mp.last_error(),
//   where empty string (false-y) is success, or an error string otherwise.
//
// - The rest of the APIs are guaranteed to return undefined on error or a
//   true-thy value on success and may or may not set last error.
//
// - push_success, push_failure, push_status and pushed_error set last error.

// iserr as true indicates an error, and if so, str may indicate a reason.
// Internally ctx->last_error_str is never NULL, and empty indicates success.
static void set_last_error(struct script_ctx *ctx, bool iserr, const char *str)
{
    ctx->last_error_str[0] = 0;
    if (!iserr)
        return;
    if (!str || !str[0])
        str = "Error";
    ctx->last_error_str = talloc_strdup_append(ctx->last_error_str, str);
}

// For use only by wrappers at defaults.js.
// arg: error string. Use empty string to indicate success.
static void script__set_last_error(js_State *J)
{
    const char *e = js_tostring(J, 1);
    set_last_error(jctx(J), e[0], e);
}

// mp.last_error() . args: none. return the last error without modifying it.
static void script_last_error(js_State *J)
{
    js_pushstring(J, jctx(J)->last_error_str);
}

// Generic success for APIs which don't return an actual value.
static void push_success(js_State *J)
{
    set_last_error(jctx(J), 0, NULL);
    js_pushboolean(J, true);
}

// Doesn't (intentionally) throw. Just sets last_error and pushes undefined
static void push_failure(js_State *J, const char *str)
{
    set_last_error(jctx(J), 1, str);
    js_pushundefined(J);
}

// Most of the scripting APIs are either sending some values and getting status
// code in return, or requesting some value while providing a default in case an
// error happened. These simplify the C code for that and always set last_error.

static void push_status(js_State *J, int err)
{
    if (err >= 0) {
        push_success(J);
    } else {
        push_failure(J, mpv_error_string(err));
    }
}

 // If err is success then return 0, else push the item at def and return 1
static bool pushed_error(js_State *J, int err, int def)
{
    bool iserr = err < 0;
    set_last_error(jctx(J), iserr, iserr ? mpv_error_string(err) : NULL);
    if (!iserr)
        return false;

    js_copy(J, def);
    return true;
}

/**********************************************************************
 *  Autofree - care-free resource deallocation on vm errors, and otherwise
 *********************************************************************/
// - Autofree (af) functions are called with a talloc context argument which is
//   freed after the function exits - either normally or because it threw an
//   error, on the latter case it then re-throws the error after the cleanup.
//
//   Autofree js C functions should have an additional void* talloc arg and
//   inserted into the vm using af_newcfunction, but otherwise used normally.
//
//  To wrap an autofree function af_TARGET in C:
//  1. Create a wrapper s_TARGET which runs af_TARGET safely inside js_try.
//  2. Use s_TARGET like so (always autofree, and throws if af_TARGET threw):
//       void *af = talloc_new(NULL);
//       int r = s_TARGET(J, ..., af);  // use J, af where the callee expects.
//       talloc_free(af);
//       if (r)
//           js_throw(J);

// add_af_file, add_af_dir, add_af_mpv_alloc take a valid FILE*/DIR*/char* value
// respectively, and fclose/closedir/mpv_free it when the parent is freed.

static void destruct_af_file(void *p)
{
    fclose(*(FILE**)p);
}

static void add_af_file(void *parent, FILE *f)
{
    FILE **pf = talloc(parent, FILE*);
    *pf = f;
    talloc_set_destructor(pf, destruct_af_file);
}

static void destruct_af_dir(void *p)
{
    closedir(*(DIR**)p);
}

static void add_af_dir(void *parent, DIR *d)
{
    DIR **pd = talloc(parent, DIR*);
    *pd = d;
    talloc_set_destructor(pd, destruct_af_dir);
}

static void destruct_af_mpv_alloc(void *p)
{
    mpv_free(*(char**)p);
}

static void add_af_mpv_alloc(void *parent, char *ma)
{
    char **p = talloc(parent, char*);
    *p = ma;
    talloc_set_destructor(p, destruct_af_mpv_alloc);
}

static void destruct_af_mpv_node(void *p)
{
    mpv_free_node_contents((mpv_node*)p);  // does nothing for MPV_FORMAT_NONE
}

// returns a new zeroed allocated struct mpv_node, and free it and its content
// when the parent is freed.
static mpv_node *new_af_mpv_node(void *parent)
{
    mpv_node *p = talloc_zero(parent, mpv_node);  // .format == MPV_FORMAT_NONE
    talloc_set_destructor(p, destruct_af_mpv_node);
    return p;
}

// Prototype for autofree functions which can be called from inside the vm.
typedef void (*af_CFunction)(js_State*, void*);

// safely run autofree js c function directly
static int s_run_af_jsc(js_State *J, af_CFunction fn, void *af)
{
    if (js_try(J))
        return 1;
    fn(J, af);
    js_endtry(J);
    return 0;
}

// The trampoline function through which all autofree functions are called from
// inside the vm. Obtains the target function address and autofree-call it.
static void script__autofree(js_State *J)
{
    // The target function is at the "af_" property of this function instance.
    js_currentfunction(J);
    js_getproperty(J, -1, "af_");
    af_CFunction fn = (af_CFunction)js_touserdata(J, -1, "af_fn");
    js_pop(J, 2);

    void *af = talloc_new(NULL);
    int r = s_run_af_jsc(J, fn, af);
    talloc_free(af);
    if (r)
        js_throw(J);
}

// Identical to js_newcfunction, but the function is inserted with an autofree
// wrapper, and its prototype should have the additional af argument.
static void af_newcfunction(js_State *J, af_CFunction fn, const char *name,
                            int length)
{
    js_newcfunction(J, script__autofree, name, length);
    js_pushnull(J);  // a prototype for the userdata object
    js_newuserdata(J, "af_fn", fn, NULL);  // uses a "af_fn" verification tag
    js_defproperty(J, -2, "af_", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
}

/**********************************************************************
 *  Initialization and file loading
 *********************************************************************/

static const char *get_builtin_file(const char *name)
{
    for (int n = 0; builtin_files[n][0]; n++) {
        if (strcmp(builtin_files[n][0], name) == 0)
            return builtin_files[n][1];
    }
    return NULL;
}

// Push up to limit bytes of file fname: from builtin_files, else from the OS.
static void af_push_file(js_State *J, const char *fname, int limit, void *af)
{
    char *filename = mp_get_user_path(af, jctx(J)->mpctx->global, fname);
    MP_VERBOSE(jctx(J), "Reading file '%s'\n", filename);
    if (limit < 0)
        limit = INT_MAX - 1;

    const char *builtin = get_builtin_file(filename);
    if (builtin) {
        js_pushlstring(J, builtin, MPMIN(limit, strlen(builtin)));
        return;
    }

    FILE *f = fopen(filename, "rb");
    if (!f)
        js_error(J, "cannot open file: '%s'", filename);
    add_af_file(af, f);

    int len = MPMIN(limit, 32 * 1024);  // initial allocation, size*2 strategy
    int got = 0;
    char *s = NULL;
    while ((s = talloc_realloc(af, s, char, len))) {
        int want = len - got;
        int r = fread(s + got, 1, want, f);

        if (feof(f) || (len == limit && r == want)) {
            js_pushlstring(J, s, got + r);
            return;
        }
        if (r != want)
            js_error(J, "cannot read data from file: '%s'", filename);

        got = got + r;
        len = MPMIN(limit, len * 2);
    }

    js_error(J, "cannot allocate %d bytes for file: '%s'", len, filename);
}

// Safely run af_push_file.
static int s_push_file(js_State *J, const char *fname, int limit, void *af)
{
    if (js_try(J))
        return 1;
    af_push_file(J, fname, limit, af);
    js_endtry(J);
    return 0;
}

// Called directly, push up to limit bytes of file fname (from builtin/os).
static void push_file_content(js_State *J, const char *fname, int limit)
{
    void *af = talloc_new(NULL);
    int r = s_push_file(J, fname, limit, af);
    talloc_free(af);
    if (r)
        js_throw(J);
}

// utils.read_file(..). args: fname [,max]. returns [up to max] bytes as string.
static void script_read_file(js_State *J)
{
    int limit = js_isundefined(J, 2) ? -1 : js_tonumber(J, 2);
    push_file_content(J, js_tostring(J, 1), limit);
}

// Runs a file with the caller's this, leaves the stack as is.
static void run_file(js_State *J, const char *fname)
{
    MP_VERBOSE(jctx(J), "Loading file %s\n", fname);
    push_file_content(J, fname, -1);
    js_loadstring(J, fname, js_tostring(J, -1));
    js_copy(J, 0);  // use the caller's this
    js_call(J, 0);
    js_pop(J, 2);  // result, file content
}

// The spec defines .name and .message for Error objects. Most engines also set
// a very convenient .stack = name + message + trace, but MuJS instead sets
// .stackTrace = trace only. Normalize by adding such .stack if required.
// Run this before anything such that we can get traces on any following errors.
static const char *norm_err_proto_js = "\
    if (Error().stackTrace && !Error().stack) {\
        Object.defineProperty(Error.prototype, 'stack', {\
            get: function() {\
                return this.name + ': ' + this.message + this.stackTrace;\
            }\
        });\
    }\
";

static void add_functions(js_State*, struct script_ctx*);

// args: none. called as script, setup and run the main script
static void script__run_script(js_State *J)
{
    js_loadstring(J, "@/norm_err.js", norm_err_proto_js);
    js_copy(J, 0);
    js_pcall(J, 0);

    struct script_ctx *ctx = jctx(J);
    add_functions(J, ctx);
    run_file(J, "@/defaults.js");
    run_file(J, ctx->filename);  // the main file to run

    if (!js_hasproperty(J, 0, "mp_event_loop") || !js_iscallable(J, -1))
        js_error(J, "no event loop function");
    js_copy(J, 0);
    js_call(J, 0); // mp_event_loop
}

// Safely set last error from stack top: stack trace or toString or generic.
// May leave items on stack - the caller should detect and pop if it cares.
static void s_top_to_last_error(struct script_ctx *ctx, js_State *J)
{
    set_last_error(ctx, 1, "unknown error");
    if (js_try(J))
        return;
    if (js_isobject(J, -1))
        js_hasproperty(J, -1, "stack");  // fetches it if exists
    set_last_error(ctx, 1, js_tostring(J, -1));
    js_endtry(J);
}

// MuJS can report warnings through this.
static void report_handler(js_State *J, const char *msg)
{
    MP_WARN(jctx(J), "[JS] %s\n", msg);
}

// Safely setup the js vm for calling run_script.
static int s_init_js(js_State *J, struct script_ctx *ctx)
{
    if (js_try(J))
        return 1;
    js_setcontext(J, ctx);
    js_setreport(J, report_handler);
    js_newcfunction(J, script__run_script, "run_script", 0);
    js_pushglobal(J);  // 'this' for script__run_script
    js_endtry(J);
    return 0;
}

/**********************************************************************
 *  Initialization - booting the script
 *********************************************************************/
// s_load_javascript: (entry point) creates the js vm, runs the script, returns
//                    on script exit or uncaught js errors. Never throws.
// script__run_script: - loads the built in functions and vars into the vm
//                     - runs the default file[s] and the main script file
//                     - calls mp_event_loop, returns on script-exit or throws.
//
// Note: init functions don't need autofree. They can use ctx as a talloc
// context and free normally. If they throw - ctx is freed right afterwards.
static int s_load_javascript(struct mpv_handle *client, const char *fname)
{
    struct script_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct script_ctx) {
        .client = client,
        .mpctx = mp_client_get_core(client),
        .log = mp_client_get_log(client),
        .last_error_str = talloc_strdup(ctx, "Cannot initialize JavaScript"),
        .filename = fname,
    };

    int r = -1;
    js_State *J = js_newstate(NULL, NULL, 0);
    if (!J || s_init_js(J, ctx))
        goto error_out;

    set_last_error(ctx, 0, NULL);
    if (js_pcall(J, 0)) {  // script__run_script
        s_top_to_last_error(ctx, J);
        goto error_out;
    }

    r = 0;

error_out:
    if (r)
        MP_FATAL(ctx, "%s\n", ctx->last_error_str);
    if (J)
        js_freestate(J);

    talloc_free(ctx);
    return r;
}

/**********************************************************************
 *  Main mp.* scripting APIs and helpers
 *********************************************************************/
static void pushnode(js_State *J, mpv_node *node);
static void makenode(void *ta_ctx, mpv_node *dst, js_State *J, int idx);
static int jsL_checkint(js_State *J, int idx);
static int64_t jsL_checkint64(js_State *J, int idx);

// Return the index in opts of stack[idx] (or of def if undefined), else throws.
static int checkopt(js_State *J, int idx, const char *def, const char *opts[],
                    const char *desc)
{
    const char *opt = js_isundefined(J, idx) ? def : js_tostring(J, idx);
    for (int i = 0; opts[i]; i++) {
        if (strcmp(opt, opts[i]) == 0)
            return i;
    }
    js_error(J, "Invalid %s '%s'", desc, opt);
}

// args: level as string and a variable numbers of args to print. adds final \n
static void script_log(js_State *J)
{
    const char *level = js_tostring(J, 1);
    int msgl = mp_msg_find_level(level);
    if (msgl < 0)
        js_error(J, "Invalid log level '%s'", level);

    struct mp_log *log = jctx(J)->log;
    for (int top = js_gettop(J), i = 2; i < top; i++)
        mp_msg(log, msgl, (i == 2 ? "%s" : " %s"), js_tostring(J, i));
    mp_msg(log, msgl, "\n");
    push_success(J);
}

static void script_find_config_file(js_State *J, void *af)
{
    const char *fname = js_tostring(J, 1);
    char *path = mp_find_config_file(af, jctx(J)->mpctx->global, fname);
    if (path) {
        js_pushstring(J, path);
    } else {
        push_failure(J, "not found");
    }
}

static void script__request_event(js_State *J)
{
    const char *event = js_tostring(J, 1);
    bool enable = js_toboolean(J, 2);

    const char *name;
    for (int n = 0; n < 256 && (name = mpv_event_name(n)); n++) {
        if (strcmp(name, event) == 0) {
            push_status(J, mpv_request_event(jclient(J), n, enable));
            return;
        }
    }
    push_failure(J, "Unknown event name");
}

static void script_enable_messages(js_State *J)
{
    const char *level = js_tostring(J, 1);
    if (mp_msg_find_level(level) < 0)
        js_error(J, "Invalid log level '%s'", level);
    push_status(J, mpv_request_log_messages(jclient(J), level));
}

// args - command [with arguments] as string
static void script_command(js_State *J)
{
    push_status(J, mpv_command_string(jclient(J), js_tostring(J, 1)));
}

// args: strings of command and then variable number of arguments
static void script_commandv(js_State *J)
{
    const char *argv[MP_CMD_MAX_ARGS + 1];
    int length = js_gettop(J) - 1;
    if (length >= MP_ARRAY_SIZE(argv))
        js_error(J, "Too many arguments");

    for (int i = 0; i < length; i++)
        argv[i] = js_tostring(J, 1 + i);
    argv[length] = NULL;
    push_status(J, mpv_command(jclient(J), argv));
}

// args: name, string value
static void script_set_property(js_State *J)
{
    int e = mpv_set_property_string(jclient(J), js_tostring(J, 1),
                                                js_tostring(J, 2));
    push_status(J, e);
}

// args: name, boolean
static void script_set_property_bool(js_State *J)
{
    int v = js_toboolean(J, 2);
    int e = mpv_set_property(jclient(J), js_tostring(J, 1), MPV_FORMAT_FLAG, &v);
    push_status(J, e);
}

// args: name [,def]
static void script_get_property_number(js_State *J)
{
    double result;
    const char *name = js_tostring(J, 1);
    int e = mpv_get_property(jclient(J), name, MPV_FORMAT_DOUBLE, &result);
    if (!pushed_error(J, e, 2))
        js_pushnumber(J, result);
}

// args: name, native value
static void script_set_property_native(js_State *J, void *af)
{
    mpv_node node;
    makenode(af, &node, J, 2);
    mpv_handle *h = jclient(J);
    int e = mpv_set_property(h, js_tostring(J, 1), MPV_FORMAT_NODE, &node);
    push_status(J, e);
}

// args: name [,def]
static void script_get_property(js_State *J, void *af)
{
    mpv_handle *h = jclient(J);
    char *res = NULL;
    int e = mpv_get_property(h, js_tostring(J, 1), MPV_FORMAT_STRING, &res);
    if (e >= 0)
        add_af_mpv_alloc(af, res);
    if (!pushed_error(J, e, 2))
        js_pushstring(J, res);
}

// args: name [,def]
static void script_get_property_bool(js_State *J)
{
    int result;
    mpv_handle *h = jclient(J);
    int e = mpv_get_property(h, js_tostring(J, 1), MPV_FORMAT_FLAG, &result);
    if (!pushed_error(J, e, 2))
        js_pushboolean(J, result);
}

// args: name, number
static void script_set_property_number(js_State *J)
{
    double v = js_tonumber(J, 2);
    mpv_handle *h = jclient(J);
    int e = mpv_set_property(h, js_tostring(J, 1), MPV_FORMAT_DOUBLE, &v);
    push_status(J, e);
}

// args: name [,def]
static void script_get_property_native(js_State *J, void *af)
{
    const char *name = js_tostring(J, 1);
    mpv_handle *h = jclient(J);
    mpv_node *presult_node = new_af_mpv_node(af);
    int e = mpv_get_property(h, name, MPV_FORMAT_NODE, presult_node);
    if (!pushed_error(J, e, 2))
        pushnode(J, presult_node);
}

// args: name [,def]
static void script_get_property_osd(js_State *J, void *af)
{
    const char *name = js_tostring(J, 1);
    mpv_handle *h = jclient(J);
    char *res = NULL;
    int e = mpv_get_property(h, name, MPV_FORMAT_OSD_STRING, &res);
    if (e >= 0)
        add_af_mpv_alloc(af, res);
    if (!pushed_error(J, e, 2))
        js_pushstring(J, res);
}

// args: id, name, type
static void script__observe_property(js_State *J)
{
    const char *fmts[] = {"none", "native", "bool", "string", "number", NULL};
    const mpv_format mf[] = {MPV_FORMAT_NONE, MPV_FORMAT_NODE, MPV_FORMAT_FLAG,
                             MPV_FORMAT_STRING, MPV_FORMAT_DOUBLE};

    mpv_format f = mf[checkopt(J, 3, "none", fmts, "observe type")];
    int e = mpv_observe_property(jclient(J), js_tonumber(J, 1),
                                             js_tostring(J, 2),
                                             f);
    push_status(J, e);
}

// args: id
static void script__unobserve_property(js_State *J)
{
    int e = mpv_unobserve_property(jclient(J), js_tonumber(J, 1));
    push_status(J, e);
}

// args: native (array of command and args, similar to commandv) [,def]
static void script_command_native(js_State *J, void *af)
{
    mpv_node cmd;
    makenode(af, &cmd, J, 1);
    mpv_node *presult_node = new_af_mpv_node(af);
    int e = mpv_command_node(jclient(J), &cmd, presult_node);
    if (!pushed_error(J, e, 2))
        pushnode(J, presult_node);
}

// args: none, result in millisec
static void script_get_time_ms(js_State *J)
{
    js_pushnumber(J, mpv_get_time_us(jclient(J)) / (double)(1000));
}

static void script_set_osd_ass(js_State *J)
{
    struct script_ctx *ctx = jctx(J);
    int res_x = js_tonumber(J, 1);
    int res_y = js_tonumber(J, 2);
    const char *text = js_tostring(J, 3);
    osd_set_external(ctx->mpctx->osd, ctx->client, res_x, res_y, (char *)text);
    mp_wakeup_core(ctx->mpctx);
    push_success(J);
}

// push object with properties names (NULL terminated) with respective vals
static void push_nums_obj(js_State *J, const char * const names[],
                          const double vals[])
{
    js_newobject(J);
    for (int i = 0; names[i]; i++) {
        js_pushnumber(J, vals[i]);
        js_setproperty(J, -2, names[i]);
    }
}

// args: none, return: object with properties width, height, aspect
static void script_get_osd_size(js_State *J)
{
    struct mp_osd_res r = osd_get_vo_res(jctx(J)->mpctx->osd);
    double ar = 1.0 * r.w / MPMAX(r.h, 1) / (r.display_par ? r.display_par : 1);
    const char * const names[] = {"width", "height", "aspect", NULL};
    const double vals[] = {r.w, r.h, ar};
    push_nums_obj(J, names, vals);
}

// args: none, return: object with properties top, bottom, left, right
static void script_get_osd_margins(js_State *J)
{
    struct mp_osd_res r = osd_get_vo_res(jctx(J)->mpctx->osd);
    const char * const names[] = {"left", "top", "right", "bottom", NULL};
    const double vals[] = {r.ml, r.mt, r.mr, r.mb};
    push_nums_obj(J, names, vals);
}

// args: none, return: object with properties x, y
static void script_get_mouse_pos(js_State *J)
{
    int x, y;
    mp_input_get_mouse_pos(jctx(J)->mpctx->input, &x, &y);
    const char * const names[] = {"x", "y", NULL};
    const double vals[] = {x, y};
    push_nums_obj(J, names, vals);
}

// args: input-section-name, x0, y0, x1, y1
static void script_input_set_section_mouse_area(js_State *J)
{
    char *section = (char *)js_tostring(J, 1);
    mp_input_set_section_mouse_area(jctx(J)->mpctx->input, section,
        js_tonumber(J, 2), js_tonumber(J, 3),   // x0, y0
        js_tonumber(J, 4), js_tonumber(J, 5));  // x1, y1
    push_success(J);
}

// args: time-in-ms [,format-string]
static void script_format_time(js_State *J, void *af)
{
    double t = js_tonumber(J, 1);
    const char *fmt = js_isundefined(J, 2) ? "%H:%M:%S" : js_tostring(J, 2);
    char *r = talloc_steal(af, mp_format_time_fmt(fmt, t));
    if (!r)
        js_error(J, "Invalid time format string '%s'", fmt);
    js_pushstring(J, r);
}

// TODO: untested
static void script_get_wakeup_pipe(js_State *J)
{
    js_pushnumber(J, mpv_get_wakeup_pipe(jclient(J)));
}

// args: name (str), priority (int), id (uint)
static void script__hook_add(js_State *J)
{
    const char *name = js_tostring(J, 1);
    int pri = jsL_checkint(J, 2);
    uint64_t id = jsL_checkint64(J, 3);
    push_status(J, mpv_hook_add(jclient(J), id, name, pri));
}

// args: id (uint)
static void script__hook_continue(js_State *J)
{
    push_status(J, mpv_hook_continue(jclient(J), jsL_checkint64(J, 1)));
}

/**********************************************************************
 *  mp.utils
 *********************************************************************/

// args: [path [,filter]]
static void script_readdir(js_State *J, void *af)
{
    //                    0      1        2       3
    const char *filters[] = {"all", "files", "dirs", "normal", NULL};
    const char *path = js_isundefined(J, 1) ? "." : js_tostring(J, 1);
    int t = checkopt(J, 2, "normal", filters, "listing filter");

    DIR *dir = opendir(path);
    if (!dir) {
        push_failure(J, "Cannot open dir");
        return;
    }
    add_af_dir(af, dir);
    set_last_error(jctx(J), 0, NULL);
    js_newarray(J);  // the return value
    char *fullpath = talloc_strdup(af, "");
    struct dirent *e;
    int n = 0;
    while ((e = readdir(dir))) {
        char *name = e->d_name;
        if (t) {
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;
            if (fullpath)
                fullpath[0] = '\0';
            fullpath = talloc_asprintf_append(fullpath, "%s/%s", path, name);
            struct stat st;
            if (stat(fullpath, &st))
                continue;
            if (!(((t & 1) && S_ISREG(st.st_mode)) ||
                  ((t & 2) && S_ISDIR(st.st_mode))))
            {
                continue;
            }
        }
        js_pushstring(J, name);
        js_setindex(J, -2, n++);
    }
}

static void script_file_info(js_State *J)
{
    const char *path = js_tostring(J, 1);

    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        push_failure(J, "Cannot stat path");
        return;
    }
    // Clear last error
    set_last_error(jctx(J), 0, NULL);

    const char * stat_names[] = {
        "mode", "size",
        "atime", "mtime", "ctime", NULL
    };
    const double stat_values[] = {
        statbuf.st_mode,
        statbuf.st_size,
        statbuf.st_atime,
        statbuf.st_mtime,
        statbuf.st_ctime
    };
    // Create an object and add all fields
    push_nums_obj(J, stat_names, stat_values);

    // Convenience booleans
    js_pushboolean(J, S_ISREG(statbuf.st_mode));
    js_setproperty(J, -2, "is_file");

    js_pushboolean(J, S_ISDIR(statbuf.st_mode));
    js_setproperty(J, -2, "is_dir");
}


static void script_split_path(js_State *J)
{
    const char *p = js_tostring(J, 1);
    bstr fname = mp_dirname(p);
    js_newarray(J);
    js_pushlstring(J, fname.start, fname.len);
    js_setindex(J, -2, 0);
    js_pushstring(J, mp_basename(p));
    js_setindex(J, -2, 1);
}

static void script_join_path(js_State *J, void *af)
{
    js_pushstring(J, mp_path_join(af, js_tostring(J, 1), js_tostring(J, 2)));
}

static void script_get_user_path(js_State *J, void *af)
{
    const char *path = js_tostring(J, 1);
    js_pushstring(J, mp_get_user_path(af, jctx(J)->mpctx->global, path));
}

struct subprocess_cb_ctx {
    struct mp_log *log;
    void *talloc_ctx;
    int64_t max_size;
    bstr output;
    bstr err;
};

static void subprocess_stdout(void *p, char *data, size_t size)
{
    struct subprocess_cb_ctx *ctx = p;
    if (ctx->output.len < ctx->max_size)
        bstr_xappend(ctx->talloc_ctx, &ctx->output, (bstr){data, size});
}

static void subprocess_stderr(void *p, char *data, size_t size)
{
    struct subprocess_cb_ctx *ctx = p;
    if (ctx->err.len < ctx->max_size)
        bstr_xappend(ctx->talloc_ctx, &ctx->err, (bstr){data, size});
    MP_INFO(ctx, "%.*s", (int)size, data);
}

// args: client invocation args object. TODO: use common backend for js/lua
static void af_subprocess_common(js_State *J, int detach, void *af)
{
    struct script_ctx *ctx = jctx(J);
    if (!js_isobject(J, 1))
        js_error(J, "argument must be an object");

    js_getproperty(J, 1, "args"); // args
    int num_args = js_getlength(J, -1);
    if (!num_args) // not using js_isarray to also accept array-like objects
        js_error(J, "args must be an non-empty array");
    char *args[256];
    if (num_args > MP_ARRAY_SIZE(args) - 1) // last needs to be NULL
        js_error(J, "too many arguments");
    if (num_args < 1)
        js_error(J, "program name missing");

    for (int n = 0; n < num_args; n++) {
        js_getindex(J, -1, n);
        if (js_isundefined(J, -1))
            js_error(J, "program arguments must be strings");
        args[n] = talloc_strdup(af, js_tostring(J, -1));
        js_pop(J, 1); // args
    }
    args[num_args] = NULL;

    if (detach) {
        mp_subprocess_detached(ctx->log, args);
        push_success(J);
        return;
    }

    struct mp_cancel *cancel = NULL;
    if (js_hasproperty(J, 1, "cancellable") ? js_toboolean(J, -1) : true)
        cancel = ctx->mpctx->playback_abort;

    int64_t max_size = js_hasproperty(J, 1, "max_size") ? js_tointeger(J, -1)
                                                        : 16 * 1024 * 1024;
    struct subprocess_cb_ctx cb_ctx = {
        .log = ctx->log,
        .talloc_ctx = af,
        .max_size = max_size,
    };

    char *error = NULL;
    int status = mp_subprocess(args, cancel, &cb_ctx, subprocess_stdout,
                               subprocess_stderr, &error);

    js_newobject(J); // res
    if (error) {
        js_pushstring(J, error); // res e
        js_setproperty(J, -2, "error"); // res
    }
    js_pushnumber(J, status); // res s
    js_setproperty(J, -2, "status"); // res
    js_pushlstring(J, cb_ctx.output.start, cb_ctx.output.len); // res d
    js_setproperty(J, -2, "stdout"); // res
    js_pushlstring(J, cb_ctx.err.start, cb_ctx.err.len);
    js_setproperty(J, -2, "stderr");
    js_pushboolean(J, status == MP_SUBPROCESS_EKILLED_BY_US); // res b
    js_setproperty(J, -2, "killed_by_us"); // res
}

// args: client invocation args object (same also for _detached)
static void script_subprocess(js_State *J, void *af)
{
    af_subprocess_common(J, 0, af);
}

static void script_subprocess_detached(js_State *J, void *af)
{
    af_subprocess_common(J, 1, af);
}

// args: none
static void script_getpid(js_State *J)
{
    js_pushnumber(J, mp_getpid());
}

// args: prefixed file name, data (c-str)
static void script_write_file(js_State *J, void *af)
{
    static const char *prefix = "file://";
    const char *fname = js_tostring(J, 1);
    const char *data = js_tostring(J, 2);
    if (strstr(fname, prefix) != fname)  // simple protection for incorrect use
        js_error(J, "File name must be prefixed with '%s'", prefix);
    fname += strlen(prefix);
    fname = mp_get_user_path(af, jctx(J)->mpctx->global, fname);
    MP_VERBOSE(jctx(J), "Writing file '%s'\n", fname);

    FILE *f = fopen(fname, "wb");
    if (!f)
        js_error(J, "Cannot open file for writing: '%s'", fname);
    add_af_file(af, f);

    int len = strlen(data);  // limited by terminating null
    int wrote = fwrite(data, 1, len, f);
    if (len != wrote)
        js_error(J, "Cannot write to file: '%s'", fname);
}

// args: env var name
static void script_getenv(js_State *J)
{
    const char *v = getenv(js_tostring(J, 1));
    if (v) {
        js_pushstring(J, v);
    } else {
        js_pushundefined(J);
    }
}

// args: as-filename, content-string, returns the compiled result as a function
static void script_compile_js(js_State *J)
{
    js_loadstring(J, js_tostring(J, 1), js_tostring(J, 2));
}

// args: true = print info (with the warning report function - no info report)
static void script__gc(js_State *J)
{
    js_gc(J, js_toboolean(J, 1) ? 1 : 0);
    push_success(J);
}

/**********************************************************************
 *  Core functions: pushnode, makenode and the event loop backend
 *********************************************************************/

// pushes a js value/array/object from an mpv_node
static void pushnode(js_State *J, mpv_node *node)
{
    int len;
    switch (node->format) {
    case MPV_FORMAT_NONE:   js_pushnull(J); break;
    case MPV_FORMAT_STRING: js_pushstring(J, node->u.string); break;
    case MPV_FORMAT_INT64:  js_pushnumber(J, node->u.int64); break;
    case MPV_FORMAT_DOUBLE: js_pushnumber(J, node->u.double_); break;
    case MPV_FORMAT_FLAG:   js_pushboolean(J, node->u.flag); break;
    case MPV_FORMAT_NODE_ARRAY:
        js_newarray(J);
        len = node->u.list->num;
        for (int n = 0; n < len; n++) {
            pushnode(J, &node->u.list->values[n]);
            js_setindex(J, -2, n);
        }
        break;
    case MPV_FORMAT_NODE_MAP:
        js_newobject(J);
        len = node->u.list->num;
        for (int n = 0; n < len; n++) {
            pushnode(J, &node->u.list->values[n]);
            js_setproperty(J, -2, node->u.list->keys[n]);
        }
        break;
    default:
        js_pushstring(J, "[UNSUPPORTED_MPV_FORMAT]");
        break;
    }
}

// For the object at stack index idx, extract the (own) property names into
// keys array (and allocate it to accommodate) and return the number of keys.
static int get_obj_properties(void *ta_ctx, char ***keys, js_State *J, int idx)
{
    int length = 0;
    js_pushiterator(J, idx, 1);

    *keys = talloc_new(ta_ctx);
    const char *name;
    while ((name = js_nextiterator(J, -1)))
        MP_TARRAY_APPEND(ta_ctx, *keys, length, talloc_strdup(ta_ctx, name));

    js_pop(J, 1);  // the iterator
    return length;
}

// true if we don't lose (too much) precision when casting to int64
static bool same_as_int64(double d)
{
    // The range checks also validly filter inf and nan, so behavior is defined
    return d >= INT64_MIN && d <= INT64_MAX && d == (int64_t)d;
}

static int jsL_checkint(js_State *J, int idx)
{
    double d = js_tonumber(J, idx);
    if (!(d >= INT_MIN && d <= INT_MAX))
        js_error(J, "integer out of range at index %d", idx);
    return d;
}

static int64_t jsL_checkint64(js_State *J, int idx)
{
    double d = js_tonumber(J, idx);
    if (!(d >= INT64_MIN && d <= INT64_MAX))
        js_error(J, "integer out of range at index %d", idx);
    return d;
}

// From the js stack value/array/object at index idx
static void makenode(void *ta_ctx, mpv_node *dst, js_State *J, int idx)
{
    if (js_isundefined(J, idx) || js_isnull(J, idx)) {
        dst->format = MPV_FORMAT_NONE;

    } else if (js_isboolean(J, idx)) {
        dst->format = MPV_FORMAT_FLAG;
        dst->u.flag = js_toboolean(J, idx);

    } else if (js_isnumber(J, idx)) {
        double val = js_tonumber(J, idx);
        if (same_as_int64(val)) {  // use int, because we can
            dst->format = MPV_FORMAT_INT64;
            dst->u.int64 = val;
        } else {
            dst->format = MPV_FORMAT_DOUBLE;
            dst->u.double_ = val;
        }

    } else if (js_isarray(J, idx)) {
        dst->format = MPV_FORMAT_NODE_ARRAY;
        dst->u.list = talloc(ta_ctx, struct mpv_node_list);
        dst->u.list->keys = NULL;

        int length = js_getlength(J, idx);
        dst->u.list->num = length;
        dst->u.list->values = talloc_array(ta_ctx, mpv_node, length);
        for (int n = 0; n < length; n++) {
            js_getindex(J, idx, n);
            makenode(ta_ctx, &dst->u.list->values[n], J, -1);
            js_pop(J, 1);
        }

    } else if (js_isobject(J, idx)) {
        dst->format = MPV_FORMAT_NODE_MAP;
        dst->u.list = talloc(ta_ctx, struct mpv_node_list);

        int length = get_obj_properties(ta_ctx, &dst->u.list->keys, J, idx);
        dst->u.list->num = length;
        dst->u.list->values = talloc_array(ta_ctx, mpv_node, length);
        for (int n = 0; n < length; n++) {
            js_getproperty(J, idx, dst->u.list->keys[n]);
            makenode(ta_ctx, &dst->u.list->values[n], J, -1);
            js_pop(J, 1);
        }

    } else {  // string, or anything else as string
        dst->format = MPV_FORMAT_STRING;
        dst->u.string = talloc_strdup(ta_ctx, js_tostring(J, idx));
    }
}

// args: wait in secs (infinite if negative) if mpv doesn't send events earlier.
static void script_wait_event(js_State *J)
{
    int top = js_gettop(J);
    double timeout = js_isnumber(J, 1) ? js_tonumber(J, 1) : -1;
    mpv_event *event = mpv_wait_event(jclient(J), timeout);

    js_newobject(J); // the reply
    js_pushstring(J, mpv_event_name(event->event_id));
    js_setproperty(J, -2, "event");  // reply.event (is an event name)

    if (event->reply_userdata) {
        js_pushnumber(J, event->reply_userdata);
        js_setproperty(J, -2, "id");   // reply.id
    }

    if (event->error < 0) {
        // TODO: untested
        js_pushstring(J, mpv_error_string(event->error));
        js_setproperty(J, -2, "error");  // reply.error
    }

    switch (event->event_id) {
    case MPV_EVENT_LOG_MESSAGE: {
        mpv_event_log_message *msg = event->data;

        js_pushstring(J, msg->prefix);
        js_setproperty(J, -2, "prefix");  // reply.prefix (e.g. "cplayer")
        js_pushstring(J, msg->level);
        js_setproperty(J, -2, "level");  // reply.level (e.g. "v" or "info")
        js_pushstring(J, msg->text);
        js_setproperty(J, -2, "text");  // reply.text
        break;
    }

    case MPV_EVENT_CLIENT_MESSAGE: {
        mpv_event_client_message *msg = event->data;

        js_newarray(J);  // reply.args
        for (int n = 0; n < msg->num_args; n++) {
            js_pushstring(J, msg->args[n]);
            js_setindex(J, -2, n);
        }
        js_setproperty(J, -2, "args");  // reply.args (is a strings array)
        break;
    }

    case MPV_EVENT_END_FILE: {
        mpv_event_end_file *eef = event->data;
        const char *reason;

        switch (eef->reason) {
        case MPV_END_FILE_REASON_EOF: reason = "eof"; break;
        case MPV_END_FILE_REASON_STOP: reason = "stop"; break;
        case MPV_END_FILE_REASON_QUIT: reason = "quit"; break;
        case MPV_END_FILE_REASON_ERROR: reason = "error"; break;
        case MPV_END_FILE_REASON_REDIRECT: reason = "redirect"; break;
        default:
            reason = "unknown";
        }
        js_pushstring(J, reason);
        js_setproperty(J, -2, "reason");  // reply.reason

        if (eef->reason == MPV_END_FILE_REASON_ERROR) {
            js_pushstring(J, mpv_error_string(eef->error));
            js_setproperty(J, -2, "error");  // reply.error
        }
        break;
    }

    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = event->data;
        js_pushstring(J, prop->name);
        js_setproperty(J, -2, "name");  // reply.name (is a property name)

        switch (prop->format) {
        case MPV_FORMAT_NODE:   pushnode(J, prop->data); break;
        case MPV_FORMAT_DOUBLE: js_pushnumber(J, *(double *)prop->data); break;
        case MPV_FORMAT_INT64:  js_pushnumber(J, *(int64_t *)prop->data); break;
        case MPV_FORMAT_FLAG:   js_pushboolean(J, *(int *)prop->data); break;
        case MPV_FORMAT_STRING: js_pushstring(J, *(char **)prop->data); break;
        default:
            js_pushnull(J);  // also for FORMAT_NONE, e.g. observe type "none"
        }
        js_setproperty(J, -2, "data");  // reply.data (value as observed type)
        break;
    }

    case MPV_EVENT_HOOK: {
        mpv_event_hook *hook = event->data;
        js_pushnumber(J, hook->id);
        js_setproperty(J, -2, "hook_id");  // reply.hook_id (is a number)
        break;
    }
    }  // switch (event->event_id)

    assert(top == js_gettop(J) - 1);
}

/**********************************************************************
 *  Script functions setup
 *********************************************************************/
#define FN_ENTRY(name, length) {#name, length, script_ ## name, NULL}
#define AF_ENTRY(name, length) {#name, length, NULL, script_ ## name}
struct fn_entry {
    const char *name;
    int length;
    js_CFunction jsc_fn;
    af_CFunction afc_fn;
};

// Names starting with underscore are wrapped at @defaults.js
// FN_ENTRY is a normal js C function, AF_ENTRY is an autofree js C function.
static const struct fn_entry main_fns[] = {
    FN_ENTRY(log, 1),
    FN_ENTRY(wait_event, 1),
    FN_ENTRY(_request_event, 2),
    AF_ENTRY(find_config_file, 1),
    FN_ENTRY(command, 1),
    FN_ENTRY(commandv, 0),
    AF_ENTRY(command_native, 2),
    FN_ENTRY(get_property_bool, 2),
    FN_ENTRY(get_property_number, 2),
    AF_ENTRY(get_property_native, 2),
    AF_ENTRY(get_property, 2),
    AF_ENTRY(get_property_osd, 2),
    FN_ENTRY(set_property, 2),
    FN_ENTRY(set_property_bool, 2),
    FN_ENTRY(set_property_number, 2),
    AF_ENTRY(set_property_native, 2),
    FN_ENTRY(_observe_property, 3),
    FN_ENTRY(_unobserve_property, 1),
    FN_ENTRY(get_time_ms, 0),
    AF_ENTRY(format_time, 2),
    FN_ENTRY(enable_messages, 1),
    FN_ENTRY(get_wakeup_pipe, 0),
    FN_ENTRY(_hook_add, 3),
    FN_ENTRY(_hook_continue, 1),
    FN_ENTRY(set_osd_ass, 3),
    FN_ENTRY(get_osd_size, 0),
    FN_ENTRY(get_osd_margins, 0),
    FN_ENTRY(get_mouse_pos, 0),
    FN_ENTRY(input_set_section_mouse_area, 5),
    FN_ENTRY(last_error, 0),
    FN_ENTRY(_set_last_error, 1),
    {0}
};

static const struct fn_entry utils_fns[] = {
    AF_ENTRY(readdir, 2),
    FN_ENTRY(file_info, 1),
    FN_ENTRY(split_path, 1),
    AF_ENTRY(join_path, 2),
    AF_ENTRY(get_user_path, 1),
    AF_ENTRY(subprocess, 1),
    AF_ENTRY(subprocess_detached, 1),
    FN_ENTRY(getpid, 0),

    FN_ENTRY(read_file, 2),
    AF_ENTRY(write_file, 2),
    FN_ENTRY(getenv, 1),
    FN_ENTRY(compile_js, 2),
    FN_ENTRY(_gc, 1),
    {0}
};

// Adds an object <module> with the functions at e to the top object
static void add_package_fns(js_State *J, const char *module,
                            const struct fn_entry *e)
{
    js_newobject(J);
    for (int n = 0; e[n].name; n++) {
        if (e[n].jsc_fn) {
            js_newcfunction(J, e[n].jsc_fn, e[n].name, e[n].length);
        } else {
            af_newcfunction(J, e[n].afc_fn, e[n].name, e[n].length);
        }
        js_setproperty(J, -2, e[n].name);
    }
    js_setproperty(J, -2, module);
}

// Called directly, adds functions/vars to the caller's this.
static void add_functions(js_State *J, struct script_ctx *ctx)
{
    js_copy(J, 0);
    add_package_fns(J, "mp", main_fns);
    js_getproperty(J, 0, "mp");  // + this mp
    add_package_fns(J, "utils", utils_fns);

    js_pushstring(J, mpv_client_name(ctx->client));
    js_setproperty(J, -2, "script_name");

    js_pushstring(J, ctx->filename);
    js_setproperty(J, -2, "script_file");

    js_pop(J, 2);  // leave the stack as we got it
}

// main export of this file, used by cplayer to load js scripts
const struct mp_scripting mp_scripting_js = {
    .name = "javascript",
    .file_ext = "js",
    .load = s_load_javascript,
};
