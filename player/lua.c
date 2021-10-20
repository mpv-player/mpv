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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "osdep/io.h"

#include "mpv_talloc.h"

#include "common/common.h"
#include "options/m_property.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "common/stats.h"
#include "options/m_option.h"
#include "input/input.h"
#include "options/path.h"
#include "misc/bstr.h"
#include "misc/json.h"
#include "osdep/subprocess.h"
#include "osdep/timer.h"
#include "osdep/threads.h"
#include "stream/stream.h"
#include "sub/osd.h"
#include "core.h"
#include "command.h"
#include "client.h"
#include "libmpv/client.h"

// List of builtin modules and their contents as strings.
// All these are generated from player/lua/*.lua
static const char * const builtin_lua_scripts[][2] = {
    {"mp.defaults",
#   include "generated/player/lua/defaults.lua.inc"
    },
    {"mp.assdraw",
#   include "generated/player/lua/assdraw.lua.inc"
    },
    {"mp.options",
#   include "generated/player/lua/options.lua.inc"
    },
    {"@osc.lua",
#   include "generated/player/lua/osc.lua.inc"
    },
    {"@ytdl_hook.lua",
#   include "generated/player/lua/ytdl_hook.lua.inc"
    },
    {"@stats.lua",
#   include "generated/player/lua/stats.lua.inc"
    },
    {"@console.lua",
#   include "generated/player/lua/console.lua.inc"
    },
    {"@auto_profiles.lua",
#   include "generated/player/lua/auto_profiles.lua.inc"
    },
    {0}
};

// Represents a loaded script. Each has its own Lua state.
struct script_ctx {
    const char *name;
    const char *filename;
    const char *path; // NULL if single file
    lua_State *state;
    struct mp_log *log;
    struct mpv_handle *client;
    struct MPContext *mpctx;
    size_t lua_malloc_size;
    lua_Alloc lua_allocf;
    void *lua_alloc_ud;
    struct stats_ctx *stats;
};

#if LUA_VERSION_NUM <= 501
#define mp_cpcall lua_cpcall
#define mp_lua_len lua_objlen
#else
// Curse whoever had this stupid idea. Curse whoever thought it would be a good
// idea not to include an emulated lua_cpcall() even more.
static int mp_cpcall (lua_State *L, lua_CFunction func, void *ud)
{
    lua_pushcfunction(L, func); // doesn't allocate in 5.2 (but does in 5.1)
    lua_pushlightuserdata(L, ud);
    return lua_pcall(L, 1, 0, 0);
}
#define mp_lua_len lua_rawlen
#endif

// Ensure that the given argument exists, even if it's nil. Can be used to
// avoid confusing the last missing optional arg with the first temporary value
// pushed to the stack.
static void mp_lua_optarg(lua_State *L, int arg)
{
    while (arg > lua_gettop(L))
        lua_pushnil(L);
}

// autofree: avoid leaks if a lua-error occurs between talloc new/free.
// If a lua c-function does a new allocation (not tied to an existing context),
// and an uncaught lua-error occures before "free" - the allocation is leaked.

// autofree lua C function: same as lua_CFunction but with these differences:
// - It accepts an additional void* argument - a pre-initialized talloc context
//   which it can use, and which is freed with its children once the function
//   completes - regardless if a lua error occured or not. If a lua error did
//   occur then it's re-thrown after the ctx is freed.
//   The stack/arguments/upvalues/return are the same as with lua_CFunction.
// - It's inserted into the lua VM using af_pushc{function,closure} instead of
//   lua_pushc{function,closure}, which takes care of wrapping it with the
//   automatic talloc alocation + lua-error-handling + talloc release.
//   This requires using AF_ENTRY instead of FN_ENTRY at struct fn_entry.
// - The autofree overhead per call is roughly two additional plain lua calls.
//   Typically that's up to 20% slower than plain new+free without "auto",
//   and at most about twice slower - compared to bare new+free lua_CFunction.
// - The overhead of af_push* is one aditional lua-c-closure with two upvalues.
typedef int (*af_CFunction)(lua_State *L, void *ctx);

static void af_pushcclosure(lua_State *L, af_CFunction fn, int n);
#define     af_pushcfunction(L, fn) af_pushcclosure((L), (fn), 0)


// add_af_dir, add_af_mpv_alloc take a valid DIR*/char* value respectively,
// and closedir/mpv_free it when the parent is freed.

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


// Perform the equivalent of mpv_free_node_contents(node) when tmp is freed.
static void steal_node_alloctions(void *tmp, mpv_node *node)
{
    talloc_steal(tmp, node_get_alloc(node));
}

// lua_Alloc compatible. Serves only to track memory usage. This wraps the
// existing allocator, partly because luajit requires the use of its internal
// allocator on 64-bit platforms.
static void *mp_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    struct script_ctx *ctx = ud;

    // Ah, what the fuck, screw whoever introduced this to Lua 5.2.
    if (!ptr)
        osize = 0;

    ptr = ctx->lua_allocf(ctx->lua_alloc_ud, ptr, osize, nsize);
    if (nsize && !ptr)
        return NULL; // allocation failed, so original memory left untouched

    ctx->lua_malloc_size = ctx->lua_malloc_size - osize + nsize;
    stats_size_value(ctx->stats, "mem", ctx->lua_malloc_size);

    return ptr;
}

static struct script_ctx *get_ctx(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "ctx");
    struct script_ctx *ctx = lua_touserdata(L, -1);
    lua_pop(L, 1);
    assert(ctx);
    return ctx;
}

static struct MPContext *get_mpctx(lua_State *L)
{
    return get_ctx(L)->mpctx;
}

static int error_handler(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);

    if (luaL_loadstring(L, "return debug.traceback('', 3)") == 0) { // e fn|err
        lua_call(L, 0, 1); // e backtrace
        const char *tr = lua_tostring(L, -1);
        MP_WARN(ctx, "%s\n", tr ? tr : "(unknown)");
    }
    lua_pop(L, 1); // e

    return 1;
}

// Check client API error code:
//  if err >= 0, push "true" to the stack, and return 1
//  if err < 0, push nil and then the error string to the stack, and return 2
static int check_error(lua_State *L, int err)
{
    if (err >= 0) {
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushnil(L);
    lua_pushstring(L, mpv_error_string(err));
    return 2;
}

static void add_functions(struct script_ctx *ctx);

static void load_file(lua_State *L, const char *fname)
{
    struct script_ctx *ctx = get_ctx(L);
    MP_DBG(ctx, "loading file %s\n", fname);
    struct bstr s = stream_read_file(fname, ctx, ctx->mpctx->global, 100000000);
    if (!s.start)
        luaL_error(L, "Could not read file.\n");
    if (luaL_loadbuffer(L, s.start, s.len, fname))
        lua_error(L);
    lua_call(L, 0, 1);
    talloc_free(s.start);
}

static int load_builtin(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    char dispname[80];
    snprintf(dispname, sizeof(dispname), "@%s", name);
    for (int n = 0; builtin_lua_scripts[n][0]; n++) {
        if (strcmp(name, builtin_lua_scripts[n][0]) == 0) {
            const char *script = builtin_lua_scripts[n][1];
            if (luaL_loadbuffer(L, script, strlen(script), dispname))
                lua_error(L);
            lua_call(L, 0, 1);
            return 1;
        }
    }
    luaL_error(L, "builtin module '%s' not found\n", name);
    return 0;
}

// Execute "require " .. name
static void require(lua_State *L, const char *name)
{
    struct script_ctx *ctx = get_ctx(L);
    MP_DBG(ctx, "loading %s\n", name);
    // Lazy, but better than calling the "require" function manually
    char buf[80];
    snprintf(buf, sizeof(buf), "require '%s'", name);
    if (luaL_loadstring(L, buf))
        lua_error(L);
    lua_call(L, 0, 0);
}

// Push the table of a module. If it doesn't exist, it's created.
// The Lua script can call "require(module)" to "load" it.
static void push_module_table(lua_State *L, const char *module)
{
    lua_getglobal(L, "package"); // package
    lua_getfield(L, -1, "loaded"); // package loaded
    lua_remove(L, -2); // loaded
    lua_getfield(L, -1, module); // loaded module
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // loaded
        lua_newtable(L); // loaded module
        lua_pushvalue(L, -1); // loaded module module
        lua_setfield(L, -3, module); // loaded module
    }
    lua_remove(L, -2); // module
}

static int load_scripts(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *fname = ctx->filename;

    require(L, "mp.defaults");

    if (fname[0] == '@') {
        require(L, fname);
    } else {
        load_file(L, fname);
    }

    lua_getglobal(L, "mp_event_loop"); // fn
    if (lua_isnil(L, -1))
        luaL_error(L, "no event loop function\n");
    lua_call(L, 0, 0); // -

    return 0;
}

static void fuck_lua(lua_State *L, const char *search_path, const char *extra)
{
    void *tmp = talloc_new(NULL);

    lua_getglobal(L, "package"); // package
    lua_getfield(L, -1, search_path); // package search_path
    bstr path = bstr0(lua_tostring(L, -1));
    char *newpath = talloc_strdup(tmp, "");

    // Script-directory paths take priority.
    if (extra) {
        newpath = talloc_asprintf_append(newpath, "%s%s",
                                         newpath[0] ? ";" : "",
                                         mp_path_join(tmp, extra, "?.lua"));
    }

    // Unbelievable but true: Lua loads .lua files AND dynamic libraries from
    // the working directory. This is highly security relevant.
    // Lua scripts are still supposed to load globally installed libraries, so
    // try to get by by filtering out any relative paths.
    while (path.len) {
        bstr item;
        bstr_split_tok(path, ";", &item, &path);
        if (mp_path_is_absolute(item)) {
            newpath = talloc_asprintf_append(newpath, "%s%.*s",
                                             newpath[0] ? ";" : "",
                                             BSTR_P(item));
        }
    }

    lua_pushstring(L, newpath);  // package search_path newpath
    lua_setfield(L, -3, search_path); // package search_path
    lua_pop(L, 2);  // -

    talloc_free(tmp);
}

static int run_lua(lua_State *L)
{
    struct script_ctx *ctx = lua_touserdata(L, -1);
    lua_pop(L, 1); // -

    luaL_openlibs(L);

    // used by get_ctx()
    lua_pushlightuserdata(L, ctx); // ctx
    lua_setfield(L, LUA_REGISTRYINDEX, "ctx"); // -

    add_functions(ctx); // mp

    push_module_table(L, "mp"); // mp

    // "mp" is available by default, and no "require 'mp'" is needed
    lua_pushvalue(L, -1); // mp mp
    lua_setglobal(L, "mp"); // mp

    lua_pushstring(L, ctx->name); // mp name
    lua_setfield(L, -2, "script_name"); // mp

    // used by pushnode()
    lua_newtable(L); // mp table
    lua_pushvalue(L, -1); // mp table table
    lua_setfield(L, LUA_REGISTRYINDEX, "UNKNOWN_TYPE"); // mp table
    lua_setfield(L, -2, "UNKNOWN_TYPE"); // mp
    lua_newtable(L); // mp table
    lua_pushvalue(L, -1); // mp table table
    lua_setfield(L, LUA_REGISTRYINDEX, "MAP"); // mp table
    lua_setfield(L, -2, "MAP"); // mp
    lua_newtable(L); // mp table
    lua_pushvalue(L, -1); // mp table table
    lua_setfield(L, LUA_REGISTRYINDEX, "ARRAY"); // mp table
    lua_setfield(L, -2, "ARRAY"); // mp

    lua_pop(L, 1); // -

    assert(lua_gettop(L) == 0);

    // Add a preloader for each builtin Lua module
    lua_getglobal(L, "package"); // package
    assert(lua_type(L, -1) == LUA_TTABLE);
    lua_getfield(L, -1, "preload"); // package preload
    assert(lua_type(L, -1) == LUA_TTABLE);
    for (int n = 0; builtin_lua_scripts[n][0]; n++) {
        lua_pushcfunction(L, load_builtin); // package preload load_builtin
        lua_setfield(L, -2, builtin_lua_scripts[n][0]);
    }
    lua_pop(L, 2); // -

    assert(lua_gettop(L) == 0);

    fuck_lua(L, "path", ctx->path);
    fuck_lua(L, "cpath", NULL);
    assert(lua_gettop(L) == 0);

    // run this under an error handler that can do backtraces
    lua_pushcfunction(L, error_handler); // errf
    lua_pushcfunction(L, load_scripts); // errf fn
    if (lua_pcall(L, 0, 0, -2)) { // errf [error]
        const char *e = lua_tostring(L, -1);
        MP_FATAL(ctx, "Lua error: %s\n", e ? e : "(unknown)");
    }

    return 0;
}

static int load_lua(struct mp_script_args *args)
{
    int r = -1;

    struct script_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct script_ctx) {
        .mpctx = args->mpctx,
        .client = args->client,
        .name = mpv_client_name(args->client),
        .log = args->log,
        .filename = args->filename,
        .path = args->path,
        .stats = stats_ctx_create(ctx, args->mpctx->global,
                    mp_tprintf(80, "script/%s", mpv_client_name(args->client))),
    };

    stats_register_thread_cputime(ctx->stats, "cpu");

    if (LUA_VERSION_NUM != 501 && LUA_VERSION_NUM != 502) {
        MP_FATAL(ctx, "Only Lua 5.1 and 5.2 are supported.\n");
        goto error_out;
    }

    lua_State *L = ctx->state = luaL_newstate();
    if (!L) {
        MP_FATAL(ctx, "Could not initialize Lua.\n");
        goto error_out;
    }

    // Wrap the internal allocator with our version that does accounting
    ctx->lua_allocf = lua_getallocf(L, &ctx->lua_alloc_ud);
    lua_setallocf(L, mp_lua_alloc, ctx);

    if (mp_cpcall(L, run_lua, ctx)) {
        const char *err = "unknown error";
        if (lua_type(L, -1) == LUA_TSTRING) // avoid allocation
            err = lua_tostring(L, -1);
        MP_FATAL(ctx, "Lua error: %s\n", err);
        goto error_out;
    }

    r = 0;

error_out:
    if (ctx->state)
        lua_close(ctx->state);
    talloc_free(ctx);
    return r;
}

static int check_loglevel(lua_State *L, int arg)
{
    const char *level = luaL_checkstring(L, arg);
    for (int n = 0; n < MSGL_MAX; n++) {
        if (mp_log_levels[n] && strcasecmp(mp_log_levels[n], level) == 0)
            return n;
    }
    luaL_error(L, "Invalid log level '%s'", level);
    abort();
}

static int script_log(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);

    int msgl = check_loglevel(L, 1);

    int last = lua_gettop(L);
    lua_getglobal(L, "tostring"); // args... tostring
    for (int i = 2; i <= last; i++) {
        lua_pushvalue(L, -1); // args... tostring tostring
        lua_pushvalue(L, i); // args... tostring tostring args[i]
        lua_call(L, 1, 1); // args... tostring str
        const char *s = lua_tostring(L, -1);
        if (s == NULL)
            return luaL_error(L, "Invalid argument");
        mp_msg(ctx->log, msgl, "%s%s", s, i > 0 ? " " : "");
        lua_pop(L, 1);  // args... tostring
    }
    mp_msg(ctx->log, msgl, "\n");

    return 0;
}

static int script_find_config_file(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    const char *s = luaL_checkstring(L, 1);
    char *path = mp_find_config_file(NULL, mpctx->global, s);
    if (path) {
        lua_pushstring(L, path);
    } else {
        lua_pushnil(L);
    }
    talloc_free(path);
    return 1;
}

static int script_get_script_directory(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    if (ctx->path) {
        lua_pushstring(L, ctx->path);
        return 1;
    }
    return 0;
}

static int script_suspend(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    MP_ERR(ctx, "mp.suspend() is deprecated and does nothing.\n");
    return 0;
}

static int script_resume(lua_State *L)
{
    return 0;
}

static int script_resume_all(lua_State *L)
{
    return 0;
}

static void pushnode(lua_State *L, mpv_node *node);

static int script_raw_wait_event(lua_State *L, void *tmp)
{
    struct script_ctx *ctx = get_ctx(L);

    mpv_event *event = mpv_wait_event(ctx->client, luaL_optnumber(L, 1, 1e20));

    struct mpv_node rn;
    mpv_event_to_node(&rn, event);
    steal_node_alloctions(tmp, &rn);

    pushnode(L, &rn); // event

    // return event
    return 1;
}

static int script_request_event(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *event = luaL_checkstring(L, 1);
    bool enable = lua_toboolean(L, 2);
    // brute force event name -> id; stops working for events > assumed max
    int event_id = -1;
    for (int n = 0; n < 256; n++) {
        const char *name = mpv_event_name(n);
        if (name && strcmp(name, event) == 0) {
            event_id = n;
            break;
        }
    }
    lua_pushboolean(L, mpv_request_event(ctx->client, event_id, enable) >= 0);
    return 1;
}

static int script_enable_messages(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *level = luaL_checkstring(L, 1);
    int r = mpv_request_log_messages(ctx->client, level);
    if (r == MPV_ERROR_INVALID_PARAMETER)
        luaL_error(L, "Invalid log level '%s'", level);
    return check_error(L, r);
}

static int script_command(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *s = luaL_checkstring(L, 1);

    return check_error(L, mpv_command_string(ctx->client, s));
}

static int script_commandv(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    int num = lua_gettop(L);
    const char *args[50];
    if (num + 1 > MP_ARRAY_SIZE(args))
        luaL_error(L, "too many arguments");
    for (int n = 1; n <= num; n++) {
        const char *s = lua_tostring(L, n);
        if (!s)
            luaL_error(L, "argument %d is not a string", n);
        args[n - 1] = s;
    }
    args[num] = NULL;
    return check_error(L, mpv_command(ctx->client, args));
}

static int script_set_property(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *p = luaL_checkstring(L, 1);
    const char *v = luaL_checkstring(L, 2);

    return check_error(L, mpv_set_property_string(ctx->client, p, v));
}

static int script_set_property_bool(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *p = luaL_checkstring(L, 1);
    int v = lua_toboolean(L, 2);

    return check_error(L, mpv_set_property(ctx->client, p, MPV_FORMAT_FLAG, &v));
}

static bool is_int(double d)
{
    int64_t v = d;
    return d == (double)v;
}

static int script_set_property_number(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *p = luaL_checkstring(L, 1);
    double d = luaL_checknumber(L, 2);
    // If the number might be an integer, then set it as integer. The mpv core
    // will (probably) convert INT64 to DOUBLE when setting, but not the other
    // way around.
    int res;
    if (is_int(d)) {
        res = mpv_set_property(ctx->client, p, MPV_FORMAT_INT64, &(int64_t){d});
    } else {
        res = mpv_set_property(ctx->client, p, MPV_FORMAT_DOUBLE, &d);
    }
    return check_error(L, res);
}

static void makenode(void *tmp, mpv_node *dst, lua_State *L, int t)
{
    luaL_checkstack(L, 6, "makenode");

    if (t < 0)
        t = lua_gettop(L) + (t + 1);
    switch (lua_type(L, t)) {
    case LUA_TNIL:
        dst->format = MPV_FORMAT_NONE;
        break;
    case LUA_TNUMBER: {
        double d = lua_tonumber(L, t);
        if (is_int(d)) {
            dst->format = MPV_FORMAT_INT64;
            dst->u.int64 = d;
        } else {
            dst->format = MPV_FORMAT_DOUBLE;
            dst->u.double_ = d;
        }
        break;
    }
    case LUA_TBOOLEAN:
        dst->format = MPV_FORMAT_FLAG;
        dst->u.flag = !!lua_toboolean(L, t);
        break;
    case LUA_TSTRING: {
        size_t len = 0;
        char *s = (char *)lua_tolstring(L, t, &len);
        bool has_zeros = !!memchr(s, 0, len);
        if (has_zeros) {
            mpv_byte_array *ba = talloc_zero(tmp, mpv_byte_array);
            *ba = (mpv_byte_array){talloc_memdup(tmp, s, len), len};
            dst->format = MPV_FORMAT_BYTE_ARRAY;
            dst->u.ba = ba;
        } else {
            dst->format = MPV_FORMAT_STRING;
            dst->u.string = talloc_strdup(tmp, s);
        }
        break;
    }
    case LUA_TTABLE: {
        // Lua uses the same type for arrays and maps, so guess the correct one.
        int format = MPV_FORMAT_NONE;
        if (lua_getmetatable(L, t)) { // mt
            lua_getfield(L, -1, "type"); // mt val
            if (lua_type(L, -1) == LUA_TSTRING) {
                const char *type = lua_tostring(L, -1);
                if (strcmp(type, "MAP") == 0) {
                    format = MPV_FORMAT_NODE_MAP;
                } else if (strcmp(type, "ARRAY") == 0) {
                    format = MPV_FORMAT_NODE_ARRAY;
                }
            }
            lua_pop(L, 2);
        }
        if (format == MPV_FORMAT_NONE) {
            // If all keys are integers, and they're in sequence, take it
            // as an array.
            int count = 0;
            for (int n = 1; ; n++) {
                lua_pushinteger(L, n); // n
                lua_gettable(L, t); // t[n]
                bool empty = lua_isnil(L, -1); // t[n]
                lua_pop(L, 1); // -
                if (empty) {
                    count = n - 1;
                    break;
                }
            }
            if (count > 0)
                format = MPV_FORMAT_NODE_ARRAY;
            lua_pushnil(L); // nil
            while (lua_next(L, t) != 0) { // key value
                count--;
                lua_pop(L, 1); // key
                if (count < 0) {
                    lua_pop(L, 1); // -
                    format = MPV_FORMAT_NODE_MAP;
                    break;
                }
            }
        }
        if (format == MPV_FORMAT_NONE)
            format = MPV_FORMAT_NODE_ARRAY; // probably empty table; assume array
        mpv_node_list *list = talloc_zero(tmp, mpv_node_list);
        dst->format = format;
        dst->u.list = list;
        if (format == MPV_FORMAT_NODE_ARRAY) {
            for (int n = 0; ; n++) {
                lua_pushinteger(L, n + 1); // n1
                lua_gettable(L, t); // t[n1]
                if (lua_isnil(L, -1))
                    break;
                MP_TARRAY_GROW(tmp, list->values, list->num);
                makenode(tmp, &list->values[n], L, -1);
                list->num++;
                lua_pop(L, 1); // -
            }
            lua_pop(L, 1); // -
        } else {
            lua_pushnil(L); // nil
            while (lua_next(L, t) != 0) { // key value
                MP_TARRAY_GROW(tmp, list->values, list->num);
                MP_TARRAY_GROW(tmp, list->keys, list->num);
                makenode(tmp, &list->values[list->num], L, -1);
                if (lua_type(L, -2) != LUA_TSTRING) {
                    luaL_error(L, "key must be a string, but got %s",
                               lua_typename(L, lua_type(L, -2)));
                }
                list->keys[list->num] = talloc_strdup(tmp, lua_tostring(L, -2));
                list->num++;
                lua_pop(L, 1); // key
            }
        }
        break;
    }
    default:
        // unknown type
        luaL_error(L, "disallowed Lua type found: %s\n", lua_typename(L, t));
    }
}

static int script_set_property_native(lua_State *L, void *tmp)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *p = luaL_checkstring(L, 1);
    struct mpv_node node;
    makenode(tmp, &node, L, 2);
    int res = mpv_set_property(ctx->client, p, MPV_FORMAT_NODE, &node);
    return check_error(L, res);

}

static int script_get_property_base(lua_State *L, void *tmp, int is_osd)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *name = luaL_checkstring(L, 1);
    int type = is_osd ? MPV_FORMAT_OSD_STRING : MPV_FORMAT_STRING;

    char *result = NULL;
    int err = mpv_get_property(ctx->client, name, type, &result);
    if (err >= 0) {
        add_af_mpv_alloc(tmp, result);
        lua_pushstring(L, result);
        return 1;
    } else {
        if (lua_isnoneornil(L, 2) && type == MPV_FORMAT_OSD_STRING) {
            lua_pushstring(L, "");
        } else {
            lua_pushvalue(L, 2);
        }
        lua_pushstring(L, mpv_error_string(err));
        return 2;
    }
}

static int script_get_property(lua_State *L, void *tmp)
{
    return script_get_property_base(L, tmp, 0);
}

static int script_get_property_osd(lua_State *L, void *tmp)
{
    return script_get_property_base(L, tmp, 1);
}

static int script_get_property_bool(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *name = luaL_checkstring(L, 1);

    int result = 0;
    int err = mpv_get_property(ctx->client, name, MPV_FORMAT_FLAG, &result);
    if (err >= 0) {
        lua_pushboolean(L, !!result);
        return 1;
    } else {
        lua_pushvalue(L, 2);
        lua_pushstring(L, mpv_error_string(err));
        return 2;
    }
}

static int script_get_property_number(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *name = luaL_checkstring(L, 1);

    // Note: the mpv core will (hopefully) convert INT64 to DOUBLE
    double result = 0;
    int err = mpv_get_property(ctx->client, name, MPV_FORMAT_DOUBLE, &result);
    if (err >= 0) {
        lua_pushnumber(L, result);
        return 1;
    } else {
        lua_pushvalue(L, 2);
        lua_pushstring(L, mpv_error_string(err));
        return 2;
    }
}

static void pushnode(lua_State *L, mpv_node *node)
{
    luaL_checkstack(L, 6, "pushnode");

    switch (node->format) {
    case MPV_FORMAT_STRING:
        lua_pushstring(L, node->u.string);
        break;
    case MPV_FORMAT_INT64:
        lua_pushnumber(L, node->u.int64);
        break;
    case MPV_FORMAT_DOUBLE:
        lua_pushnumber(L, node->u.double_);
        break;
    case MPV_FORMAT_NONE:
        lua_pushnil(L);
        break;
    case MPV_FORMAT_FLAG:
        lua_pushboolean(L, node->u.flag);
        break;
    case MPV_FORMAT_NODE_ARRAY:
        lua_newtable(L); // table
        lua_getfield(L, LUA_REGISTRYINDEX, "ARRAY"); // table mt
        lua_setmetatable(L, -2); // table
        for (int n = 0; n < node->u.list->num; n++) {
            pushnode(L, &node->u.list->values[n]); // table value
            lua_rawseti(L, -2, n + 1); // table
        }
        break;
    case MPV_FORMAT_NODE_MAP:
        lua_newtable(L); // table
        lua_getfield(L, LUA_REGISTRYINDEX, "MAP"); // table mt
        lua_setmetatable(L, -2); // table
        for (int n = 0; n < node->u.list->num; n++) {
            lua_pushstring(L, node->u.list->keys[n]); // table key
            pushnode(L, &node->u.list->values[n]); // table key value
            lua_rawset(L, -3);
        }
        break;
    case MPV_FORMAT_BYTE_ARRAY:
        lua_pushlstring(L, node->u.ba->data, node->u.ba->size);
        break;
    default:
        // unknown value - what do we do?
        // for now, set a unique dummy value
        lua_newtable(L); // table
        lua_getfield(L, LUA_REGISTRYINDEX, "UNKNOWN_TYPE");
        lua_setmetatable(L, -2); // table
        break;
    }
}

static int script_get_property_native(lua_State *L, void *tmp)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *name = luaL_checkstring(L, 1);
    mp_lua_optarg(L, 2);

    mpv_node node;
    int err = mpv_get_property(ctx->client, name, MPV_FORMAT_NODE, &node);
    if (err >= 0) {
        steal_node_alloctions(tmp, &node);
        pushnode(L, &node);
        return 1;
    }
    lua_pushvalue(L, 2);
    lua_pushstring(L, mpv_error_string(err));
    return 2;
}

static mpv_format check_property_format(lua_State *L, int arg)
{
    if (lua_isnil(L, arg))
        return MPV_FORMAT_NONE;
    const char *fmts[] = {"none", "native", "bool", "string", "number", NULL};
    switch (luaL_checkoption(L, arg, "none", fmts)) {
    case 0: return MPV_FORMAT_NONE;
    case 1: return MPV_FORMAT_NODE;
    case 2: return MPV_FORMAT_FLAG;
    case 3: return MPV_FORMAT_STRING;
    case 4: return MPV_FORMAT_DOUBLE;
    }
    abort();
}

// It has a raw_ prefix, because there is a more high level API in defaults.lua.
static int script_raw_observe_property(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    uint64_t id = luaL_checknumber(L, 1);
    const char *name = luaL_checkstring(L, 2);
    mpv_format format = check_property_format(L, 3);
    return check_error(L, mpv_observe_property(ctx->client, id, name, format));
}

static int script_raw_unobserve_property(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    uint64_t id = luaL_checknumber(L, 1);
    lua_pushnumber(L, mpv_unobserve_property(ctx->client, id));
    return 1;
}

static int script_command_native(lua_State *L, void *tmp)
{
    struct script_ctx *ctx = get_ctx(L);
    mp_lua_optarg(L, 2);
    struct mpv_node node;
    struct mpv_node result;
    makenode(tmp, &node, L, 1);
    int err = mpv_command_node(ctx->client, &node, &result);
    if (err >= 0) {
        steal_node_alloctions(tmp, &result);
        pushnode(L, &result);
        return 1;
    }
    lua_pushvalue(L, 2);
    lua_pushstring(L, mpv_error_string(err));
    return 2;
}

static int script_raw_command_native_async(lua_State *L, void *tmp)
{
    struct script_ctx *ctx = get_ctx(L);
    uint64_t id = luaL_checknumber(L, 1);
    struct mpv_node node;
    makenode(tmp, &node, L, 2);
    int res = mpv_command_node_async(ctx->client, id, &node);
    return check_error(L, res);
}

static int script_raw_abort_async_command(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    uint64_t id = luaL_checknumber(L, 1);
    mpv_abort_async_command(ctx->client, id);
    return 0;
}

static int script_get_time(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    lua_pushnumber(L, mpv_get_time_us(ctx->client) / (double)(1000 * 1000));
    return 1;
}

static int script_input_set_section_mouse_area(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);

    char *section = (char *)luaL_checkstring(L, 1);
    int x0 = luaL_checkinteger(L, 2);
    int y0 = luaL_checkinteger(L, 3);
    int x1 = luaL_checkinteger(L, 4);
    int y1 = luaL_checkinteger(L, 5);
    mp_input_set_section_mouse_area(mpctx->input, section, x0, y0, x1, y1);
    return 0;
}

static int script_format_time(lua_State *L)
{
    double t = luaL_checknumber(L, 1);
    const char *fmt = luaL_optstring(L, 2, "%H:%M:%S");
    char *r = mp_format_time_fmt(fmt, t);
    if (!r)
        luaL_error(L, "Invalid time format string '%s'", fmt);
    lua_pushstring(L, r);
    talloc_free(r);
    return 1;
}

static int script_get_wakeup_pipe(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    lua_pushinteger(L, mpv_get_wakeup_pipe(ctx->client));
    return 1;
}

static int script_raw_hook_add(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    uint64_t ud = luaL_checkinteger(L, 1);
    const char *name = luaL_checkstring(L, 2);
    int pri = luaL_checkinteger(L, 3);
    return check_error(L, mpv_hook_add(ctx->client, ud, name, pri));
}

static int script_raw_hook_continue(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    lua_Integer id = luaL_checkinteger(L, 1);
    return check_error(L, mpv_hook_continue(ctx->client, id));
}

static int script_readdir(lua_State *L, void *tmp)
{
    //                    0      1        2       3
    const char *fmts[] = {"all", "files", "dirs", "normal", NULL};
    const char *path = luaL_checkstring(L, 1);
    int t = luaL_checkoption(L, 2, "normal", fmts);
    DIR *dir = opendir(path);
    if (!dir) {
        lua_pushnil(L);
        lua_pushstring(L, "error");
        return 2;
    }
    add_af_dir(tmp, dir);
    lua_newtable(L); // list
    char *fullpath = talloc_strdup(tmp, "");
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
                continue;
        }
        lua_pushinteger(L, ++n); // list index
        lua_pushstring(L, name); // list index name
        lua_settable(L, -3); // list
    }
    return 1;
}

static int script_file_info(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);

    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        lua_pushnil(L);
        lua_pushstring(L, "error");
        return 2;
    }

    lua_newtable(L); // Result stat table

    const char * stat_names[] = {
        "mode", "size",
        "atime", "mtime", "ctime", NULL
    };
    const lua_Number stat_values[] = {
        statbuf.st_mode,
        statbuf.st_size,
        statbuf.st_atime,
        statbuf.st_mtime,
        statbuf.st_ctime
    };

    // Add all fields
    for (int i = 0; stat_names[i]; i++) {
        lua_pushnumber(L, stat_values[i]);
        lua_setfield(L, -2, stat_names[i]);
    }

    // Convenience booleans
    lua_pushboolean(L, S_ISREG(statbuf.st_mode));
    lua_setfield(L, -2, "is_file");

    lua_pushboolean(L, S_ISDIR(statbuf.st_mode));
    lua_setfield(L, -2, "is_dir");

    // Return table
    return 1;
}

static int script_split_path(lua_State *L)
{
    const char *p = luaL_checkstring(L, 1);
    bstr fname = mp_dirname(p);
    lua_pushlstring(L, fname.start, fname.len);
    lua_pushstring(L, mp_basename(p));
    return 2;
}

static int script_join_path(lua_State *L)
{
    const char *p1 = luaL_checkstring(L, 1);
    const char *p2 = luaL_checkstring(L, 2);
    char *r = mp_path_join(NULL, p1, p2);
    lua_pushstring(L, r);
    talloc_free(r);
    return 1;
}

static int script_parse_json(lua_State *L, void *tmp)
{
    mp_lua_optarg(L, 2);
    char *text = talloc_strdup(tmp, luaL_checkstring(L, 1));
    bool trail = lua_toboolean(L, 2);
    bool ok = false;
    struct mpv_node node;
    if (json_parse(tmp, &node, &text, 32) >= 0) {
        json_skip_whitespace(&text);
        ok = !text[0] || trail;
    }
    if (ok) {
        pushnode(L, &node);
        lua_pushnil(L);
    } else {
        lua_pushnil(L);
        lua_pushstring(L, "error");
    }
    lua_pushstring(L, text);
    return 3;
}

static int script_format_json(lua_State *L, void *tmp)
{
    struct mpv_node node;
    makenode(tmp, &node, L, 1);
    char *dst = talloc_strdup(tmp, "");
    if (json_write(&dst, &node) >= 0) {
        lua_pushstring(L, dst);
        lua_pushnil(L);
    } else {
        lua_pushnil(L);
        lua_pushstring(L, "error");
    }
    return 2;
}

static int script_get_env_list(lua_State *L)
{
    lua_newtable(L); // table
    for (int n = 0; environ && environ[n]; n++) {
        lua_pushstring(L, environ[n]); // table str
        lua_rawseti(L, -2, n + 1); // table
    }
    return 1;
}

#define FN_ENTRY(name) {#name, script_ ## name, 0}
#define AF_ENTRY(name) {#name, 0, script_ ## name}
struct fn_entry {
    const char *name;
    int (*fn)(lua_State *L);  // lua_CFunction
    int (*af)(lua_State *L, void *);  // af_CFunction
};

static const struct fn_entry main_fns[] = {
    FN_ENTRY(log),
    FN_ENTRY(suspend),
    FN_ENTRY(resume),
    FN_ENTRY(resume_all),
    AF_ENTRY(raw_wait_event),
    FN_ENTRY(request_event),
    FN_ENTRY(find_config_file),
    FN_ENTRY(get_script_directory),
    FN_ENTRY(command),
    FN_ENTRY(commandv),
    AF_ENTRY(command_native),
    AF_ENTRY(raw_command_native_async),
    FN_ENTRY(raw_abort_async_command),
    AF_ENTRY(get_property),
    AF_ENTRY(get_property_osd),
    FN_ENTRY(get_property_bool),
    FN_ENTRY(get_property_number),
    AF_ENTRY(get_property_native),
    FN_ENTRY(set_property),
    FN_ENTRY(set_property_bool),
    FN_ENTRY(set_property_number),
    AF_ENTRY(set_property_native),
    FN_ENTRY(raw_observe_property),
    FN_ENTRY(raw_unobserve_property),
    FN_ENTRY(get_time),
    FN_ENTRY(input_set_section_mouse_area),
    FN_ENTRY(format_time),
    FN_ENTRY(enable_messages),
    FN_ENTRY(get_wakeup_pipe),
    FN_ENTRY(raw_hook_add),
    FN_ENTRY(raw_hook_continue),
    {0}
};

static const struct fn_entry utils_fns[] = {
    AF_ENTRY(readdir),
    FN_ENTRY(file_info),
    FN_ENTRY(split_path),
    FN_ENTRY(join_path),
    AF_ENTRY(parse_json),
    AF_ENTRY(format_json),
    FN_ENTRY(get_env_list),
    {0}
};

typedef struct autofree_data {
    af_CFunction target;
    void *ctx;
} autofree_data;

/* runs the target autofree script_* function with the ctx argument */
static int script_autofree_call(lua_State *L)
{
    // n*args &data
    autofree_data *data = lua_touserdata(L, -1);
    lua_pop(L, 1);  // n*args
    assert(data && data->target && data->ctx);
    return data->target(L, data->ctx);
}

static int script_autofree_trampoline(lua_State *L)
{
    // n*args
    autofree_data data = {
        .target = lua_touserdata(L, lua_upvalueindex(2)),  // fn
        .ctx = NULL,
    };
    assert(data.target);

    lua_pushvalue(L, lua_upvalueindex(1));  // n*args autofree_call (closure)
    lua_insert(L, 1);  // autofree_call n*args
    lua_pushlightuserdata(L, &data);  // autofree_call n*args &data

    data.ctx = talloc_new(NULL);
    int r = lua_pcall(L, lua_gettop(L) - 1, LUA_MULTRET, 0);  // m*retvals
    talloc_free(data.ctx);

    if (r)
        lua_error(L);

    return lua_gettop(L);  // m (retvals)
}

static void af_pushcclosure(lua_State *L, af_CFunction fn, int n)
{
    // Instead of pushing a direct closure of fn with n upvalues, we push an
    // autofree_trampoline closure with two upvalues:
    //   1: autofree_call closure with the n upvalues given here.
    //   2: fn
    //
    // when called the autofree_trampoline closure will pcall the autofree_call
    // closure with the current lua call arguments and an additional argument
    // which holds ctx and fn. the autofree_call closure (with the n upvalues
    // given here) calls fn directly and provides it with the ctx C argument,
    // so that fn sees the exact n upvalues and lua call arguments as intended,
    // wrapped with ctx init/cleanup.

    lua_pushcclosure(L, script_autofree_call, n);
    lua_pushlightuserdata(L, fn);
    lua_pushcclosure(L, script_autofree_trampoline, 2);
}

static void register_package_fns(lua_State *L, char *module,
                                 const struct fn_entry *e)
{
    push_module_table(L, module); // modtable
    for (int n = 0; e[n].name; n++) {
        if (e[n].af) {
            af_pushcclosure(L, e[n].af, 0); // modtable fn
        } else {
            lua_pushcclosure(L, e[n].fn, 0); // modtable fn
        }
        lua_setfield(L, -2, e[n].name); // modtable
    }
    lua_pop(L, 1); // -
}

static void add_functions(struct script_ctx *ctx)
{
    lua_State *L = ctx->state;

    register_package_fns(L, "mp", main_fns);
    register_package_fns(L, "mp.utils", utils_fns);
}

const struct mp_scripting mp_scripting_lua = {
    .name = "lua script",
    .file_ext = "lua",
    .load = load_lua,
};
