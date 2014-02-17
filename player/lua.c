#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "osdep/io.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "talloc.h"

#include "common/common.h"
#include "options/m_property.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "options/m_option.h"
#include "input/input.h"
#include "options/path.h"
#include "bstr/bstr.h"
#include "osdep/timer.h"
#include "osdep/threads.h"
#include "sub/osd.h"
#include "core.h"
#include "command.h"
#include "client.h"
#include "libmpv/client.h"
#include "lua.h"

// List of builtin modules and their contents as strings.
// All these are generated from player/lua/*.lua
static const char *builtin_lua_scripts[][2] = {
    {"mp.defaults",
#   include "player/lua/defaults.inc"
    },
    {"mp.assdraw",
#   include "player/lua/assdraw.inc"
    },
    {"@osc",
#   include "player/lua/osc.inc"
    },
    {0}
};

// Represents a loaded script. Each has its own Lua state.
struct script_ctx {
    const char *name;
    lua_State *state;
    struct mp_log *log;
    struct mpv_handle *client;
    struct MPContext *mpctx;
    int suspended;
};

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

static int wrap_cpcall(lua_State *L)
{
    lua_CFunction fn = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return fn(L);
}

// Call the given function fn under a Lua error handler (similar to lua_cpcall).
// Pass the given number of args from the Lua stack to fn.
// Returns 0 (and empty stack) on success.
// Returns LUA_ERR[RUN|MEM|ERR] otherwise, with the error value on the stack.
static int mp_cpcall(lua_State *L, lua_CFunction fn, int args)
{
    // Don't use lua_pushcfunction() - it allocates memory on Lua 5.1.
    // Instead, emulate C closures by making wrap_cpcall call fn.
    lua_pushlightuserdata(L, fn); // args... fn
    // Will always succeed if mp_lua_init() set it up correctly.
    lua_getfield(L, LUA_REGISTRYINDEX, "wrap_cpcall"); // args... fn wrap_cpcall
    lua_insert(L, -(args + 2)); // wrap_cpcall args... fn
    return lua_pcall(L, args + 1, 0, 0);
}

static void report_error(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *err = lua_tostring(L, -1);
    MP_WARN(ctx, "Error: %s\n", err ? err : "[unknown]");
    lua_pop(L, 1);
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

static int run_event_loop(lua_State *L)
{
    lua_getglobal(L, "mp_event_loop");
    if (lua_isnil(L, -1))
        luaL_error(L, "no event loop function\n");
    lua_call(L, 0, 0);
    return 0;
}

static void add_functions(struct script_ctx *ctx);

static char *script_name_from_filename(void *talloc_ctx, const char *fname)
{
    fname = mp_basename(fname);
    if (fname[0] == '@')
        fname += 1;
    char *name = talloc_strdup(talloc_ctx, fname);
    // Drop .lua extension
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
    return talloc_asprintf(talloc_ctx, "lua/%s", name);
}

static int load_file(struct script_ctx *ctx, const char *fname)
{
    int r = 0;
    lua_State *L = ctx->state;
    char *res_name = mp_get_user_path(NULL, ctx->mpctx->global, fname);
    MP_VERBOSE(ctx, "loading file %s\n", res_name);
    if (luaL_loadfile(L, res_name) || lua_pcall(L, 0, 0, 0)) {
        report_error(L);
        r = -1;
    }
    assert(lua_gettop(L) == 0);
    talloc_free(res_name);
    return r;
}

static int load_builtin(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    for (int n = 0; builtin_lua_scripts[n][0]; n++) {
        if (strcmp(name, builtin_lua_scripts[n][0]) == 0) {
            if (luaL_loadstring(L, builtin_lua_scripts[n][1]))
                lua_error(L);
            lua_call(L, 0, 1);
            return 1;
        }
    }
    return 0;
}

// Execute "require " .. name
static bool require(lua_State *L, const char *name)
{
    struct script_ctx *ctx = get_ctx(L);
    MP_VERBOSE(ctx, "loading %s\n", name);
    // Lazy, but better than calling the "require" function manually
    char buf[80];
    snprintf(buf, sizeof(buf), "require '%s'", name);
    if (luaL_loadstring(L, buf) || lua_pcall(L, 0, 0, 0)) {
        report_error(L);
        return false;
    }
    return true;
}

struct thread_arg {
    struct MPContext *mpctx;
    mpv_handle *client;
    const char *fname;
};

static void *lua_thread(void *p)
{
    pthread_detach(pthread_self());

    struct thread_arg *arg = p;
    struct MPContext *mpctx = arg->mpctx;
    const char *fname = arg->fname;
    mpv_handle *client = arg->client;

    struct script_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct script_ctx) {
        .mpctx = mpctx,
        .client = client,
        .name = mpv_client_name(client),
        .log = mp_client_get_log(client),
    };

    lua_State *L = ctx->state = luaL_newstate();
    if (!L)
        goto error_out;

    // used by get_ctx()
    lua_pushlightuserdata(L, ctx); // ctx
    lua_setfield(L, LUA_REGISTRYINDEX, "ctx"); // -

    lua_pushcfunction(L, wrap_cpcall); // closure
    lua_setfield(L, LUA_REGISTRYINDEX, "wrap_cpcall"); // -

    luaL_openlibs(L);

    lua_newtable(L); // mp
    lua_pushvalue(L, -1); // mp mp
    lua_setglobal(L, "mp"); // mp

    add_functions(ctx); // mp

    lua_pushstring(L, ctx->name); // mp name
    lua_setfield(L, -2, "script_name"); // mp

    lua_pop(L, 1); // -

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

    if (!require(L, "mp.defaults")) {
        report_error(L);
        goto error_out;
    }

    assert(lua_gettop(L) == 0);

    if (fname[0] == '@') {
        if (!require(L, fname))
            goto error_out;
    } else {
        if (load_file(ctx, fname) < 0)
            goto error_out;
    }

    // Call the script's event loop runs until the script terminates and unloads.
    if (mp_cpcall(L, run_event_loop, 0) != 0)
        report_error(L);

error_out:
    MP_VERBOSE(ctx, "exiting.\n");
    if (ctx->suspended)
        mpv_resume(ctx->client);
    if (ctx->state)
        lua_close(ctx->state);
    mpv_destroy(ctx->client);
    talloc_free(ctx);
    talloc_free(arg);
    return NULL;
}

static void mp_lua_load_script(struct MPContext *mpctx, const char *fname)
{
    struct thread_arg *arg = talloc_ptrtype(NULL, arg);
    char *name = script_name_from_filename(arg, fname);
    *arg = (struct thread_arg){
        .mpctx = mpctx,
        .fname = talloc_strdup(arg, fname),
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
    if (pthread_create(&thread, NULL, lua_thread, arg))
        talloc_free(arg);

    return;
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
    char *path = mp_find_user_config_file(NULL, mpctx->global, s);
    if (path) {
        lua_pushstring(L, path);
    } else {
        lua_pushnil(L);
    }
    talloc_free(path);
    return 1;
}

static int script_suspend(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    if (!ctx->suspended)
        mpv_suspend(ctx->client);
    ctx->suspended++;
    return 0;
}

static int script_resume(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    if (ctx->suspended < 1)
        luaL_error(L, "trying to resume, but core is not suspended");
    ctx->suspended--;
    if (!ctx->suspended)
        mpv_resume(ctx->client);
    return 0;
}

static int script_resume_all(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    if (ctx->suspended)
        mpv_resume(ctx->client);
    ctx->suspended = 0;
    return 0;
}

static int script_wait_event(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);

    double timeout = luaL_optnumber(L, 1, 1e20);

    // This will almost surely lead to a deadlock. (Polling is still ok.)
    if (ctx->suspended && timeout > 0)
        luaL_error(L, "attempting to wait while core is suspended");

    mpv_event *event = mpv_wait_event(ctx->client, timeout);

    lua_newtable(L); // event
    lua_pushstring(L, mpv_event_name(event->event_id)); // event name
    lua_setfield(L, -2, "event"); // event

    if (event->error < 0) {
        lua_pushstring(L, mpv_error_string(event->error)); // event err
        lua_setfield(L, -2, "error"); // event
    }

    switch (event->event_id) {
    case MPV_EVENT_LOG_MESSAGE: {
        mpv_event_log_message *msg = event->data;

        lua_pushstring(L, msg->prefix); // event s
        lua_setfield(L, -2, "prefix"); // event
        lua_pushstring(L, msg->level); // event s
        lua_setfield(L, -2, "level"); // event
        lua_pushstring(L, msg->text); // event s
        lua_setfield(L, -2, "text"); // event
        break;
    }
    case MPV_EVENT_SCRIPT_INPUT_DISPATCH: {
        mpv_event_script_input_dispatch *msg = event->data;

        lua_pushinteger(L, msg->arg0); // event i
        lua_setfield(L, -2, "arg0"); // event
        lua_pushstring(L, msg->type); // event s
        lua_setfield(L, -2, "type"); // event
        break;
    }
    case MPV_EVENT_CLIENT_MESSAGE: {
        mpv_event_client_message *msg = event->data;

        lua_newtable(L); // event args
        for (int n = 0; n < msg->num_args; n++) {
            lua_pushinteger(L, n + 1); // event args N
            lua_pushstring(L, msg->args[n]); // event args N val
            lua_settable(L, -3); // event args
        }
        lua_setfield(L, -2, "args"); // event
        break;
    }
    default: ;
    }

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
    check_loglevel(L, 1);
    const char *level = luaL_checkstring(L, 1);
    return check_error(L, mpv_request_log_messages(ctx->client, level));
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

static int script_property_list(lua_State *L)
{
    const struct m_option *props = mp_get_property_list();
    lua_newtable(L);
    for (int i = 0; props[i].name; i++) {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, props[i].name);
        lua_settable(L, -3);
    }
    return 1;
}

static int script_get_property(lua_State *L)
{
    struct script_ctx *ctx = get_ctx(L);
    const char *name = luaL_checkstring(L, 1);
    int type = lua_tointeger(L, lua_upvalueindex(1))
               ? MPV_FORMAT_OSD_STRING : MPV_FORMAT_STRING;
    char *def_fallback = type == MPV_FORMAT_OSD_STRING ? "" : NULL;
    char *def = (char *)luaL_optstring(L, 2, def_fallback);

    char *result = NULL;
    int err = mpv_get_property(ctx->client, name, type, &result);
    if (err >= 0) {
        lua_pushstring(L, result);
        talloc_free(result);
        return 1;
    }
    if (def) {
        lua_pushstring(L, def);
        lua_pushstring(L, mpv_error_string(err));
        return 2;
    }
    return check_error(L, err);
}

static int script_set_osd_ass(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    int res_x = luaL_checkinteger(L, 1);
    int res_y = luaL_checkinteger(L, 2);
    const char *text = luaL_checkstring(L, 3);
    osd_set_external(mpctx->osd, res_x, res_y, (char *)text);
    return 0;
}

static int script_get_osd_resolution(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    int w, h;
    osd_object_get_resolution(mpctx->osd, OSDTYPE_EXTERNAL, &w, &h);
    lua_pushnumber(L, w);
    lua_pushnumber(L, h);
    return 2;
}

static int script_get_screen_size(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    struct mp_osd_res vo_res = osd_get_vo_res(mpctx->osd, OSDTYPE_EXTERNAL);
    double aspect = 1.0 * vo_res.w / MPMAX(vo_res.h, 1) /
                    vo_res.display_par;
    lua_pushnumber(L, vo_res.w);
    lua_pushnumber(L, vo_res.h);
    lua_pushnumber(L, aspect);
    return 3;
}

static int script_get_mouse_pos(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    int px, py;
    mp_input_get_mouse_pos(mpctx->input, &px, &py);
    double sw, sh;
    osd_object_get_scale_factor(mpctx->osd, OSDTYPE_EXTERNAL, &sw, &sh);
    lua_pushnumber(L, px * sw);
    lua_pushnumber(L, py * sh);
    return 2;
}

static int script_get_time(lua_State *L)
{
    lua_pushnumber(L, mp_time_sec());
    return 1;
}

static int script_get_chapter_list(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    mp_dispatch_lock(mpctx->dispatch);
    lua_newtable(L); // list
    int num = get_chapter_count(mpctx);
    for (int n = 0; n < num; n++) {
        double time = chapter_start_time(mpctx, n);
        char *name = chapter_display_name(mpctx, n);
        lua_newtable(L); // list ch
        lua_pushnumber(L, time); // list ch time
        lua_setfield(L, -2, "time"); // list ch
        lua_pushstring(L, name); // list ch name
        lua_setfield(L, -2, "name"); // list ch
        lua_pushinteger(L, n + 1); // list ch n1
        lua_insert(L, -2); // list n1 ch
        lua_settable(L, -3); // list
        talloc_free(name);
    }
    mp_dispatch_unlock(mpctx->dispatch);
    return 1;
}

static const char *stream_type(enum stream_type t)
{
    switch (t) {
    case STREAM_VIDEO: return "video";
    case STREAM_AUDIO: return "audio";
    case STREAM_SUB:   return "sub";
    default:           return "unknown";
    }
}

static int script_get_track_list(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    mp_dispatch_lock(mpctx->dispatch);
    lua_newtable(L); // list
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        lua_newtable(L); // list track

        lua_pushstring(L, stream_type(track->type));
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, track->user_tid);
        lua_setfield(L, -2, "id");
        lua_pushboolean(L, track->default_track);
        lua_setfield(L, -2, "default");
        lua_pushboolean(L, track->attached_picture);
        lua_setfield(L, -2, "attached_picture");
        if (track->lang) {
            lua_pushstring(L, track->lang);
            lua_setfield(L, -2, "language");
        }
        if (track->title) {
            lua_pushstring(L, track->title);
            lua_setfield(L, -2, "title");
        }
        lua_pushboolean(L, track->is_external);
        lua_setfield(L, -2, "external");
        if (track->external_filename) {
            lua_pushstring(L, track->external_filename);
            lua_setfield(L, -2, "external_filename");
        }
        lua_pushboolean(L, track->auto_loaded);
        lua_setfield(L, -2, "auto_loaded");

        lua_pushinteger(L, n + 1); // list track n1
        lua_insert(L, -2); // list n1 track
        lua_settable(L, -3); // list
    }
    mp_dispatch_unlock(mpctx->dispatch);
    return 1;
}

static int script_input_define_section(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    char *section = (char *)luaL_checkstring(L, 1);
    char *contents = (char *)luaL_checkstring(L, 2);
    char *flags = (char *)luaL_optstring(L, 3, "");
    bool builtin = true;
    if (strcmp(flags, "builtin") == 0) {
        builtin = true;
    } else if (strcmp(flags, "default") == 0) {
        builtin = false;
    } else if (strcmp(flags, "") == 0) {
        //pass
    } else {
        luaL_error(L, "invalid flags: '%*'", flags);
    }
    mp_input_define_section(mpctx->input, section, "<script>", contents, builtin);
    return 0;
}

static int script_input_enable_section(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    char *section = (char *)luaL_checkstring(L, 1);
    char *sflags = (char *)luaL_optstring(L, 2, "");
    bstr bflags = bstr0(sflags);
    int flags = 0;
    while (bflags.len) {
        bstr val;
        bstr_split_tok(bflags, "|", &val, &bflags);
        if (bstr_equals0(val, "allow-hide-cursor")) {
            flags |= MP_INPUT_ALLOW_HIDE_CURSOR;
        } else if (bstr_equals0(val, "allow-vo-dragging")) {
            flags |= MP_INPUT_ALLOW_VO_DRAGGING;
        } else if (bstr_equals0(val, "exclusive")) {
            flags |= MP_INPUT_EXCLUSIVE;
        } else {
            luaL_error(L, "invalid flag: '%.*s'", BSTR_P(val));
        }
    }
    mp_input_enable_section(mpctx->input, section, flags);
    return 0;
}

static int script_input_disable_section(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);
    char *section = (char *)luaL_checkstring(L, 1);
    mp_input_disable_section(mpctx->input, section);
    return 0;
}

static int script_input_set_section_mouse_area(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);

    double sw, sh;
    osd_object_get_scale_factor(mpctx->osd, OSDTYPE_EXTERNAL, &sw, &sh);

    char *section = (char *)luaL_checkstring(L, 1);
    int x0 = luaL_checkinteger(L, 2) / sw;
    int y0 = luaL_checkinteger(L, 3) / sh;
    int x1 = luaL_checkinteger(L, 4) / sw;
    int y1 = luaL_checkinteger(L, 5) / sh;
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

static int script_get_opt(lua_State *L)
{
    struct MPContext *mpctx = get_mpctx(L);

    mp_dispatch_lock(mpctx->dispatch);

    char **opts = mpctx->opts->lua_opts;
    const char *name = luaL_checkstring(L, 1);
    int r = 0;

    for (int n = 0; opts && opts[n] && opts[n + 1]; n++) {
        if (strcmp(opts[n], name) == 0) {
            lua_pushstring(L, opts[n + 1]);
            r = 1;
            break;
        }
    }

    mp_dispatch_unlock(mpctx->dispatch);

    return r;
}

struct fn_entry {
    const char *name;
    int (*fn)(lua_State *L);
};

#define FN_ENTRY(name) {#name, script_ ## name}

static struct fn_entry fn_list[] = {
    FN_ENTRY(log),
    FN_ENTRY(suspend),
    FN_ENTRY(resume),
    FN_ENTRY(resume_all),
    FN_ENTRY(wait_event),
    FN_ENTRY(request_event),
    FN_ENTRY(find_config_file),
    FN_ENTRY(command),
    FN_ENTRY(commandv),
    FN_ENTRY(set_property),
    FN_ENTRY(property_list),
    FN_ENTRY(set_osd_ass),
    FN_ENTRY(get_osd_resolution),
    FN_ENTRY(get_screen_size),
    FN_ENTRY(get_mouse_pos),
    FN_ENTRY(get_time),
    FN_ENTRY(get_chapter_list),
    FN_ENTRY(get_track_list),
    FN_ENTRY(input_define_section),
    FN_ENTRY(input_enable_section),
    FN_ENTRY(input_disable_section),
    FN_ENTRY(input_set_section_mouse_area),
    FN_ENTRY(format_time),
    FN_ENTRY(enable_messages),
    FN_ENTRY(get_opt),
};

// On stack: mp table
static void add_functions(struct script_ctx *ctx)
{
    lua_State *L = ctx->state;

    for (int n = 0; n < MP_ARRAY_SIZE(fn_list); n++) {
        lua_pushcfunction(L, fn_list[n].fn);
        lua_setfield(L, -2, fn_list[n].name);
    }

    lua_pushinteger(L, 0);
    lua_pushcclosure(L, script_get_property, 1);
    lua_setfield(L, -2, "get_property");

    lua_pushinteger(L, 1);
    lua_pushcclosure(L, script_get_property, 1);
    lua_setfield(L, -2, "get_property_osd");
}

static int compare_filename(const void *pa, const void *pb)
{
    char *a = (char *)pa;
    char *b = (char *)pb;
    return strcmp(a, b);
}

static char **list_lua_files(void *talloc_ctx, char *path)
{
    char **files = NULL;
    int count = 0;
    DIR *dp = opendir(path);
    if (!dp)
        return NULL;
    struct dirent *ep;
    while ((ep = readdir(dp))) {
        char *ext = mp_splitext(ep->d_name, NULL);
        if (!ext || strcasecmp(ext, "lua") != 0)
            continue;
        char *fname = mp_path_join(talloc_ctx, bstr0(path), bstr0(ep->d_name));
        MP_TARRAY_APPEND(talloc_ctx, files, count, fname);
    }
    closedir(dp);
    qsort(files, count, sizeof(char *), compare_filename);
    MP_TARRAY_APPEND(talloc_ctx, files, count, NULL);
    return files;
}

void mp_lua_init(struct MPContext *mpctx)
{
    // Load scripts from options
    if (mpctx->opts->lua_load_osc)
        mp_lua_load_script(mpctx, "@osc");
    char **files = mpctx->opts->lua_files;
    for (int n = 0; files && files[n]; n++) {
        if (files[n][0])
            mp_lua_load_script(mpctx, files[n]);
    }
    // Load ~/.mpv/lua/*
    void *tmp = talloc_new(NULL);
    char *lua_path = mp_find_user_config_file(tmp, mpctx->global, "lua");
    if (lua_path) {
        files = list_lua_files(tmp, lua_path);
        for (int n = 0; files && files[n]; n++)
            mp_lua_load_script(mpctx, files[n]);
    }
    talloc_free(tmp);
}
