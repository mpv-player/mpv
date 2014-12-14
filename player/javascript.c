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

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <math.h>

#include "osdep/io.h"
#include "osdep/atomics.h"

#include <mujs.h>

#include "talloc.h"

#include "common/common.h"
#include "options/m_property.h"
#include "common/msg.h"
#include "common/msg_control.h"
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

#define MAX_LENGTH_COMMANDV 50

// List of builtin modules and their contents as strings.
// All these are generated from player/javascript/*.js
static const char * const builtin_files[][3] = {
    {"@defaults.js",
#   include "player/javascript/defaults.js.inc"
    },
    {0}
};

// Represents a loaded script. Each has its own js state.
struct script_ctx {
    const char *name;
    const char *filename;
    js_State *jsstate;
    struct mp_log *log;
    struct mpv_handle *client;
    struct MPContext *mpctx;
    int suspended;
};

static struct script_ctx *get_ctx(js_State *J)
{
    return (struct script_ctx *)js_getcontext(J);
}

static struct MPContext *get_mpctx(js_State *J)
{
    return get_ctx(J)->mpctx;
}

static mpv_handle *client_js(js_State *J) {
    return get_ctx(J)->client;
}


/**********************************************************************
 *  error handling
 *********************************************************************/
// sets mp.last_error_string from str, or if doesn't exist, from err
static void set_last_error(js_State *J, int err, const char *str)
{
    js_getglobal(J, "mp");
    js_pushstring(J, str ? str : mpv_error_string(err));
    js_setproperty(J, -2, "last_error_string");
    js_pop(J, 1);
}

// if err < 0: sets mp.last_error_string, pushes undefined and returns 1
// else: does nothing and returns 0
static int handledAsError(js_State *J, int err) {
    if (err >= 0)
        return 0;

    set_last_error(J, err, NULL);
    js_pushundefined(J);
    return 1;
}

// assumes idx 2 exists on the stack (even if undefined).
// identical to handledAsError if idx 2 is undefined. otherwise:
// - always sets mp.last_error_string ("success" on success)
// - on error, pushes idx 2 as the result.
static int handledAsErrDef(js_State *J, int err) {
    if (js_isundefined(J, 2))
      return handledAsError(J, err);

    set_last_error(J, err, NULL);
    if (err >= 0)
        return 0;

    js_copy(J, 2);
    return 1;
}

// pushes true/false and handles error if required
static void pushStatus(js_State *J, int err) {
    if (!handledAsError(J, err))
        js_pushboolean(J, 1);
}


/**********************************************************************
 *  Initialization and files reading/loading/running
 *********************************************************************/

enum FILE_COMMAND {
    FILE_READ, // pushes the file's content as (null terminated) string
    FILE_LOAD, // compiles the file to js and pushes it as a js function
    FILE_RUN   // compiles + runs the file and pushes the result
};

static void push_file_result(js_State *J, enum FILE_COMMAND command, const char *filename);

static void script_read_file(js_State *J)
{
    push_file_result(J, FILE_READ, js_tostring(J, 1));
}

static void script_load_file(js_State *J)
{
    push_file_result(J, FILE_LOAD, js_tostring(J, 1));
}

static void script_run_file(js_State *J)
{
    push_file_result(J, FILE_RUN, js_tostring(J, 1));
}

/// file functions helper:
static const char *get_builtin_file(const char *name)
{
    for (int n = 0; builtin_files[n][0]; n++)
        if (!strcmp(builtin_files[n][0], name))
            return builtin_files[n][1];
    return NULL;
}

// executes the command and pushes the result or throws an error
// filename is taken as is, unless it starts with '@', at which case it's read from builtin_files.
static void push_file_result(js_State *J, enum FILE_COMMAND command, const char *filename)
{
  if (!filename || !filename[0])
      js_error(J, "invalid file name");

  FILE *f;
  char *s = NULL;
  const char *data;

  if (filename[0] == '@') {
      data = get_builtin_file(filename);
      if (!data)
          js_error(J, "cannot find built in file '%s'", filename);

  } else {
      int n, t;

      f = fopen(filename, "rb");
      if (!f)
          js_error(J, "cannot open file: '%s'", filename);

      if (fseek(f, 0, SEEK_END) < 0) {
          fclose(f);
          js_error(J, "cannot seek in file: '%s'", filename);
      }
      n = ftell(f);
      fseek(f, 0, SEEK_SET);

      s = talloc_array(NULL, char, n + 1);
      if (!s) {
          fclose(f);
          js_error(J, "cannot allocate storage for file contents: '%s'", filename);
      }

      t = fread(s, 1, n, f);
      if (t != n) {
          talloc_free(s);
          fclose(f);
          js_error(J, "cannot read data from file: '%s'", filename);
      }

      s[n] = 0;
      data = s;
  }

  int err;
  switch (command) {
      case FILE_READ:
          js_pushstring (J, data);
          err = 0;
          break;
      case FILE_LOAD:
          err = js_ploadstring(J, filename, data);
          break;
      case FILE_RUN :
          if (err = js_ploadstring(J, filename, data))
            break;
          js_pushglobal(J);
          err = js_pcall(J, 0);
          break;
      default:
          js_newerror(J, "unknown file command");
          err = -1;
  }

  if (s) {
      talloc_free(s);
      fclose(f);
  }

  if (err)
      js_throw(J);
}

static void add_functions(struct script_ctx *ctx);

// called directly, doesn't modify stack's depth, runs the file or throws an error
static void run_file(js_State *J, const char *fname)
{
    struct script_ctx *ctx = get_ctx(J);
    char *res_name = mp_get_user_path(NULL, ctx->mpctx->global, fname);
    const char *name = fname[0] == '@' ? fname : res_name;
    MP_VERBOSE(ctx, "loading file %s\n", name);

    js_newcfunction(J, script_run_file, "run_file", 1);
    js_pushglobal(J);
    js_pushstring(J, name);
    int r = js_pcall(J, 1);
    talloc_free(res_name); // careful to not leak this on errors
    if (r)
        js_throw(J); // the error object is at the top of the stack
    js_pop(J, 1);
}

// called as script, leaves result on stack or throws
static void script_run_scripts(js_State *J)
{
    add_functions(get_ctx(J));
    run_file(J, "@defaults.js");
    //run_file(J, "player/javascript/defaults.js"); // useful for development to read from the filesystem instead of embedded.
    run_file(J,  get_ctx(J)->filename); // the main file for this script

    js_getglobal(J, "mp_event_loop"); // fn
    if (!js_iscallable(J, -1))
        js_error(J, "no event loop function");

    js_pushglobal(J);
    js_call(J, 0); // mp_event_loop
}

// load_javascript - entry point for mpv to run this script, and exit point for any uncaught js errors beyond it.
// script_run_scripts - loads the built in functions into the vm
//                   - runs the default file[s] and the main script file
//                   - calls mp_event_loop, returns on shutdown/error/script-exit.
// run_file        - loads and runs a single js file
// - names starting with script_ are js functions (take arguments from the vm's stack and push the result back)
// - names starting with push take c args and push a value to the js stack
// - js calle's stack index 0 is "this", and the rest (1, 2, 3, ...) are the args
static int load_javascript(struct mpv_handle *client, const char *fname)
{
    struct script_ctx ctx = (struct script_ctx) {
        .mpctx = mp_client_get_core(client),
        .client = client,
        .name = mpv_client_name(client),
        .log = mp_client_get_log(client),
        .filename = fname,
    };

    int r = -1;
    js_State *J = ctx.jsstate = js_newstate(NULL, NULL);
    if (!J)
        goto error_out;

    // store ctx as the vm's context
    // later used by functions called from js with get_ctx(J);
    js_setcontext(J, &ctx);

    js_newcfunction(J, script_run_scripts, "run_scripts", 0);
    js_pushglobal(J);
    if (js_pcall(J, 0)) {
        MP_FATAL(&ctx, "JS error: %s\n", js_tostring(J, -1));
        goto error_out;
    }

    r = 0;

error_out:
    if (ctx.suspended)
        mpv_resume(ctx.client);
    if (J)
        js_freestate(J);
    return r;
}

/**********************************************************************
 *  functions exposed to javascript and helpers
 *********************************************************************/
static int check_loglevel(js_State *J, int idx)
{
    const char *level = js_tostring(J, idx);
    for (int n = 0; n < MSGL_MAX; n++) {
        if (mp_log_levels[n] && strcasecmp(mp_log_levels[n], level) == 0)
            return n;
    }
    js_error(J, "Invalid log level '%s'", level);
}

// assumes fromIdx is not negative
static void finalize_log(int msgl, js_State *J, int fromIdx)
{
    struct script_ctx *ctx = get_ctx(J);
    int last = js_gettop(J) - 1;
    for (int i = fromIdx; i <= last; i++)
        mp_msg(ctx->log, msgl, "%s%s", (i > fromIdx ? " " : ""), js_tostring(J, i));

    mp_msg(ctx->log, msgl, "\n");
    pushStatus(J, 1);
}

// All the log functions are at mp.msg

// args: level as string and the rest are strings to log
static void script_log(js_State *J) { finalize_log(check_loglevel(J, 1), J, 2); }

// args: strings to log
static void script_fatal(js_State *J)   { finalize_log(MSGL_FATAL, J, 1); }
static void script_error(js_State *J)   { finalize_log(MSGL_ERR,   J, 1); }
static void script_warn(js_State *J)    { finalize_log(MSGL_WARN,  J, 1); }
static void script_info(js_State *J)    { finalize_log(MSGL_INFO,  J, 1); }
static void script_verbose(js_State *J) { finalize_log(MSGL_V,     J, 1); }
static void script_debug(js_State *J)   { finalize_log(MSGL_DEBUG, J, 1); }

static void script_find_config_file(js_State *J)
{
    struct MPContext *mpctx = get_mpctx(J);
    const char *s = js_tostring(J, 1);
    char *path = mp_find_config_file(NULL, mpctx->global, s);
    if (path)
        js_pushstring(J, path);
    else
        js_pushnull(J);

    talloc_free(path);
}

static void script_suspend(js_State *J)
{
    struct script_ctx *ctx = get_ctx(J);
    if (!ctx->suspended)
        mpv_suspend(ctx->client);
    ctx->suspended++;
    js_pushundefined(J);
}

static void script_resume(js_State *J)
{
    struct script_ctx *ctx = get_ctx(J);
    if (ctx->suspended < 1)
        js_error(J, "trying to resume, but core is not suspended");
    ctx->suspended--;
    if (!ctx->suspended)
        mpv_resume(ctx->client);
    js_pushundefined(J);
}

static void resume_all(struct script_ctx *ctx)
{
    if (ctx->suspended)
        mpv_resume(ctx->client);
    ctx->suspended = 0;
}

static void script_resume_all(js_State *J)
{
    resume_all(get_ctx(J));
}

static void pushnode(js_State *J, mpv_node *node);

static void script_wait_event(js_State *J)
{
    struct script_ctx *ctx = get_ctx(J);
    int top = js_gettop(J);
    double timeout = -1;
    if (!js_isundefined(J, -1))
      timeout = js_tonumber(J, -1);
    if (timeout < 0)
      timeout = 1e20;

    // This will almost surely lead to a deadlock. (Polling is still ok.)
    if (ctx->suspended && timeout > 0)
        js_error(J, "attempting to wait while core is suspended");

    mpv_event *event = mpv_wait_event(ctx->client, timeout);

    js_newobject(J); // reply
    js_pushstring(J, mpv_event_name(event->event_id)); // event name
    js_setproperty(J, -2, "event"); // event

    if (event->reply_userdata) {
        js_pushnumber(J, event->reply_userdata);
        js_setproperty(J, -2, "id");
    }

    if (event->error < 0) {
        // TODO: untested
        js_pushstring(J, mpv_error_string(event->error)); // event err
        js_setproperty(J, -2, "error"); // event
    }

    switch (event->event_id) {
    case MPV_EVENT_LOG_MESSAGE: {
        // TODO: untested
        mpv_event_log_message *msg = event->data;

        js_pushstring(J, msg->prefix); // event s
        js_setproperty(J, -2, "prefix"); // event
        js_pushstring(J, msg->level); // event s
        js_setproperty(J, -2, "level"); // event
        js_pushstring(J, msg->text); // event s
        js_setproperty(J, -2, "text"); // event
        break;
    }

    case MPV_EVENT_CLIENT_MESSAGE: {
        mpv_event_client_message *msg = event->data;

        js_newarray(J); // event args
        js_setlength(J, -1, msg->num_args);
        for (int n = 0; n < msg->num_args; n++) {
            js_pushstring(J, msg->args[n]); // event args N val
            js_setindex(J, -2, n);
        }
        js_setproperty(J, -2, "args"); // reply
        break;
    }

    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = event->data;

        js_pushstring(J, prop->name);
        js_setproperty(J, -2, "name");

        switch (prop->format) {
        case MPV_FORMAT_NODE:   pushnode(J, prop->data);                  break;
        case MPV_FORMAT_DOUBLE: js_pushnumber(J, *(double *)prop->data);  break;
        case MPV_FORMAT_INT64:  js_pushnumber(J, *(int64_t *)prop->data); break;
        case MPV_FORMAT_FLAG:   js_pushboolean(J, *(int *)prop->data);    break;
        case MPV_FORMAT_STRING: js_pushstring(J, *(char **)prop->data);   break;
        default:
            MP_WARN(ctx, "unknown property type: %d\n", prop->format);
            js_pushundefined(J);
        }
        js_setproperty(J, -2, "data");
        break;
    }
    default: ;
    }

    // return event
    assert(top == js_gettop(J) - 1);
    return;
}

static void script__request_event(js_State *J)
{
    struct script_ctx *ctx = get_ctx(J);
    const char *event = js_tostring(J, 1);
    bool enable = js_toboolean(J, 2);
    // brute force event name -> id; stops working for events > assumed max
    int event_id = -1;
    for (int n = 0; n < 256; n++) {
        const char *name = mpv_event_name(n);
        if (name && strcmp(name, event) == 0) {
            event_id = n;
            break;
        }
    }
    pushStatus(J, mpv_request_event(ctx->client, event_id, enable));
}

// TODO: untested
static void script_enable_messages(js_State *J)
{
    struct script_ctx *ctx = get_ctx(J);
    check_loglevel(J, 1);
    const char *level = js_tostring(J, 1);
    pushStatus(J, mpv_request_log_messages(ctx->client, level));
}

//args - command [with arguments] as string
static void script_command(js_State *J)
{
    pushStatus(J, mpv_command_string(client_js(J), js_tostring(J, 1)));
}

//args: strings of command and then variable number of arguments
static void script_commandv(js_State *J)
{
    const char *args[MAX_LENGTH_COMMANDV + 1];
    unsigned int length = js_gettop(J);
    if (!length || length > MAX_LENGTH_COMMANDV)
        js_error(J, "Invalid number of arguments. Allowed: 1 - %d", MAX_LENGTH_COMMANDV);

    unsigned int i;
    for (i = 0; i < length - 1; i++)
        args[i] = js_tostring(J, i + 1);
    args[i] = NULL;

    pushStatus(J, mpv_command(client_js(J), args));
}

//args: name, string value
static void script_set_property(js_State *J)
{
    pushStatus(J, mpv_set_property_string(client_js(J),
                                          js_tostring(J, 1),
                                          js_tostring(J, 2)));
}

// args: name, boolean
static void script_set_property_bool(js_State *J)
{
    int v = js_toboolean(J, 2);
    pushStatus(J, mpv_set_property(client_js(J),
                                   js_tostring(J, 1),
                                   MPV_FORMAT_FLAG, &v));
}

static bool is_int(double d)
{
    int64_t v = d;
    return d == (double)v;
}

//args: name [,def]
static void script_get_property_number(js_State *J)
{
    double result;
    if (!handledAsErrDef(J, mpv_get_property(client_js(J),
                                             js_tostring(J, 1),
                                             MPV_FORMAT_DOUBLE, &result)))
        js_pushnumber(J, result);
}

// for the object at stack index idx, extract the property names into keys array
// and return the number of keys.
static int get_object_properties(void *ta_ctx, char ***keys, js_State *J, int idx)
{
    idx = idx >= 0 ? idx : js_gettop(J) + idx;
    int length = 0;
    js_pushiterator(J, idx, 1);
    // the vm probably doesn't allocate the strings for us, so iterating them is cheap
    // and saves reallocs for *keys
    while (js_nextiterator(J, -1))
        length++;
    js_pop(J, 1);

    js_pushiterator(J, idx, 1);
    *keys = talloc_array(ta_ctx, char*, length);
    for (int n = 0; n < length; n++)
        (*keys)[n] = talloc_strdup(ta_ctx, js_nextiterator(J, -1));
    js_pop(J, 1); // the iterator

    return length;
}

// from the js stack object at index idx
static void makenode(void *ta_ctx, mpv_node *dst, js_State *J, int idx)
{
    if (js_isundefined(J, idx) || js_isnull(J, idx)) {
        dst->format = MPV_FORMAT_NONE;

    } else if (js_isboolean(J, idx)) {
        dst->format = MPV_FORMAT_FLAG;
        dst->u.flag = js_toboolean(J, idx);

    } else if (js_isnumber(J, idx)) {
        double val = js_tonumber(J, idx);
        if (is_int(val)) {
            dst->format = MPV_FORMAT_INT64;
            dst->u.int64 = val;
        } else {
            dst->format = MPV_FORMAT_DOUBLE;
            dst->u.double_ = val;
        }

    } else if (js_isstring(J, idx)) {
        dst->format = MPV_FORMAT_STRING;
        dst->u.string = talloc_strdup(ta_ctx, js_tostring(J, idx));

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

        int length = get_object_properties(ta_ctx, &dst->u.list->keys, J, idx);
        dst->u.list->num = length;
        dst->u.list->values = talloc_array(ta_ctx, mpv_node, length);
        for (int n = 0; n < length; n++) {
            js_getproperty(J, idx, dst->u.list->keys[n]);
            makenode(ta_ctx, &dst->u.list->values[n], J, -1);
            js_pop(J, 1);
        }

    } else {
        dst->format = MPV_FORMAT_NONE; // unknown data type
    }
}

//args: name, native value
static void script_set_property_native(js_State *J)
{
    mpv_node node;
    void *tmp = talloc_new(NULL);
    makenode(tmp, &node, J, 2);
    //debug_node(&node);
    int err = mpv_set_property(client_js(J), js_tostring(J, 1), MPV_FORMAT_NODE, &node);
    talloc_free(tmp);
    pushStatus(J, err);
}

//args: name [,def]
static void script_get_property(js_State *J)
{
    char *result = NULL;
    if (!handledAsErrDef(J, mpv_get_property(client_js(J), js_tostring(J, 1), MPV_FORMAT_STRING, &result))) {
        js_pushstring(J, result);
        talloc_free(result);
    }
}

//args: name [,def]
static void script_get_property_bool(js_State *J)
{
    int result;
    if (!handledAsErrDef(J, mpv_get_property(client_js(J), js_tostring(J, 1), MPV_FORMAT_FLAG, &result)))
        js_pushboolean(J, result);
}

//args: name, number
static void script_set_property_number(js_State *J)
{
    double v = js_tonumber(J, 2);
    pushStatus(J, mpv_set_property(client_js(J), js_tostring(J, 1), MPV_FORMAT_DOUBLE, &v));
}

static void pushnode(js_State *J, mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_NONE:   js_pushnull   (J);                  break;
    case MPV_FORMAT_STRING: js_pushstring (J, node->u.string);  break;
    case MPV_FORMAT_INT64:  js_pushnumber (J, node->u.int64);   break;
    case MPV_FORMAT_DOUBLE: js_pushnumber (J, node->u.double_); break;
    case MPV_FORMAT_FLAG:   js_pushboolean(J, node->u.flag);    break;
    case MPV_FORMAT_NODE_ARRAY:
                            js_newarray(J);
                            js_setlength(J, -1, node->u.list->num);
                            for (int n = 0; n < node->u.list->num; n++) {
                                pushnode(J, &node->u.list->values[n]);
                                js_setindex(J, -2, n);
                            }
                            break;
    case MPV_FORMAT_NODE_MAP:
                            js_newobject(J);
                            for (int n = 0; n < node->u.list->num; n++) {
                                pushnode(J, &node->u.list->values[n]);
                                js_setproperty(J, -2, node->u.list->keys[n]);
                            }
                            break;
    default:
        js_pushstring(J, "[UNKNOWN_VALUE_FORMAT]");
        break;
    }
}

//args: name [,def]
static void script_get_property_native(js_State *J)
{
    mpv_node result;
    if (!handledAsErrDef(J, mpv_get_property(client_js(J),
                                             js_tostring(J, 1),
                                             MPV_FORMAT_NODE, &result))) {
        pushnode(J, &result);
        mpv_free_node_contents(&result);
    }
}

//args: name [,def]
static void script_get_property_osd(js_State *J)
{
    char *result = NULL;
    if (!handledAsErrDef(J, mpv_get_property(client_js(J), js_tostring(J, 1), MPV_FORMAT_OSD_STRING, &result))) {
        js_pushstring(J, result);
        talloc_free(result);
    }
}

//args: id, name, type
static void script__observe_property(js_State *J)
{
    pushStatus(J, mpv_observe_property(client_js(J),
                                       js_tonumber(J, 1),
                                       js_tostring(J, 2),
                                       js_tonumber(J, 3)));
}

//args: id
static void script__unobserve_property(js_State *J)
{
    pushStatus(J, mpv_unobserve_property(client_js(J), js_tonumber(J, 1)));
}

//args: native (node)
static void script_command_native(js_State *J) {
  mpv_node cmd;
  mpv_node result;
  void *tmp = talloc_new(NULL);
  makenode(tmp, &cmd, J, 1);
  if (!handledAsErrDef(J, mpv_command_node(client_js(J), &cmd, &result))) {
      pushnode(tmp, &result);
      mpv_free_node_contents(&result);
  }
  talloc_free(tmp);
}

// TODO: untested
static void script_set_osd_ass(js_State *J)
{
    struct MPContext *mpctx = get_mpctx(J);
    int res_x = js_tonumber(J, 1);
    int res_y = js_tonumber(J, 2);
    const char *text = js_tostring(J, 3);
    osd_set_external(mpctx->osd, res_x, res_y, (char *)text);
    mp_input_wakeup(mpctx->input);
}

// args: none, return: object with w and h properties
static void script_get_osd_resolution(js_State *J)
{
    struct MPContext *mpctx = get_mpctx(J);
    int w, h;
    osd_object_get_resolution(mpctx->osd, OSDTYPE_EXTERNAL, &w, &h);
    js_newobject(J);
    js_pushnumber(J, w);
    js_setproperty(J, -2, "w");
    js_pushnumber(J, h);
    js_setproperty(J, -2, "h");
}

// args: none, return: object with w, h and aspect properties
static void script_get_screen_size(js_State *J)
{
    struct MPContext *mpctx = get_mpctx(J);
    struct mp_osd_res vo_res = osd_get_vo_res(mpctx->osd, OSDTYPE_EXTERNAL);
    double aspect = 1.0 * vo_res.w / MPMAX(vo_res.h, 1) /
                    (vo_res.display_par ? vo_res.display_par : 1);
    js_newobject(J);
    js_pushnumber(J, vo_res.w);
    js_setproperty(J, -2, "w");
    js_pushnumber(J, vo_res.h);
    js_setproperty(J, -2, "h");
    js_pushnumber(J, aspect);
    js_setproperty(J, -2, "aspect");
}

// args: none, return: object with x and y properties
static void script_get_mouse_pos(js_State *J)
{
    struct MPContext *mpctx = get_mpctx(J);
    int px, py;
    mp_input_get_mouse_pos(mpctx->input, &px, &py);
    double sw, sh;
    osd_object_get_scale_factor(mpctx->osd, OSDTYPE_EXTERNAL, &sw, &sh);
    js_newobject(J);
    js_pushnumber(J, px * sw);
    js_setproperty(J, -2, "x");
    js_pushnumber(J, py * sh);
    js_setproperty(J, -2, "y");
}

//args: none, result in seconds
static void script_get_time(js_State *J)
{
    js_pushnumber(J, mpv_get_time_us(client_js(J)) / (double)(1000 * 1000));
}

//args: none, result in millisec
static void script_get_time_ms(js_State *J)
{
    js_pushnumber(J, mpv_get_time_us(client_js(J)) / (double)(1000));
}

// args: section, content [,flags]
static void script_input_define_section(js_State *J)
{
    struct MPContext *mpctx = get_mpctx(J);
    char *section = (char *)js_tostring(J, 1);
    char *contents = (char *)js_tostring(J, 2);
    char *flags = (char *)(js_isundefined(J, 3) ? "" : js_tostring(J, 3));
    bool builtin = true;
    if (strcmp(flags, "default") == 0) {
        builtin = true;
    } else if (strcmp(flags, "force") == 0) {
        builtin = false;
    } else if (strcmp(flags, "") == 0) {
        //pass
    } else {
        js_error(J, "invalid flags: '%s'", flags);
    }
    mp_input_define_section(mpctx->input, section, "<script>", contents, builtin);
}

// args: section [,flags]
static void script_input_enable_section(js_State *J)
{
    struct MPContext *mpctx = get_mpctx(J);
    char *section = (char *)js_tostring(J, 1);
    char *sflags = (char *)(js_isundefined(J, 2) ? "" : js_tostring(J, 2));
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
            js_error(J, "invalid flag");
        }
    }
    mp_input_enable_section(mpctx->input, section, flags);
}

// args: section
static void script_input_disable_section(js_State *J)
{
    struct MPContext *mpctx = get_mpctx(J);
    char *section = (char *)js_tostring(J, 1);
    mp_input_disable_section(mpctx->input, section);
}

// TODO: untested. args: section, x0, y0, x1, y1
static void script_input_set_section_mouse_area(js_State *J)
{
    struct MPContext *mpctx = get_mpctx(J);

    double sw, sh;
    osd_object_get_scale_factor(mpctx->osd, OSDTYPE_EXTERNAL, &sw, &sh);

    char *section = (char *)js_tostring(J, 1);
    int x0 = sw ? js_tonumber(J, 2) / sw : 0;
    int y0 = sh ? js_tonumber(J, 3) / sh : 0;
    int x1 = sw ? js_tonumber(J, 4) / sw : 0;
    int y1 = sh ? js_tonumber(J, 5) / sh : 0;
    mp_input_set_section_mouse_area(mpctx->input, section, x0, y0, x1, y1);
}

static void script_format_time(js_State *J)
{
    double t = js_tonumber(J, 1);
    const char *fmt = js_isundefined(J, 2) ? "%H:%M:%S" : js_tostring(J, 2);
    char *r = mp_format_time_fmt(fmt, t);
    if (!r)
        js_error(J, "Invalid time format string '%s'", fmt);
    js_pushstring(J, r);
    talloc_free(r);
}

// TODO: untested
static void script_get_wakeup_pipe(js_State *J)
{
    struct script_ctx *ctx = get_ctx(J);
    js_pushnumber(J, mpv_get_wakeup_pipe(ctx->client));
}

static void script_getcwd(js_State *J)
{
    char *cwd = mp_getcwd(NULL);
    if (!cwd) {
        js_pushundefined(J);
        set_last_error(J, 0, "Error");
        return;
    }
    js_pushstring(J, cwd);
    talloc_free(cwd);
}

static int checkoption(js_State *J, int idx, const char *def, const char *const opts[]) {
  const char *opt;
  if (js_isstring(J, idx)) {
      opt = js_tostring(J, idx);
  } else {
      if (def)
          opt = def;
      else
          js_error(J, "Not a string");
  }

  for (int i = 0; opts[i]; i++)
      if (!strcmp(opt, opts[i]))
          return i;

  js_error(J, "Unknown option");
}

static void script_readdir(js_State *J)
{
    //                    0      1        2       3
    const char *fmts[] = {"all", "files", "dirs", "normal", NULL};
    const char *path = js_isstring(J, 1) ? js_tostring(J, 1) : ".";
    int t = checkoption(J, 2, "normal", fmts);
    DIR *dir = opendir(path);
    if (!dir) {
        js_pushundefined(J);
        set_last_error(J, 0, "Cannot open dir");
        return;
    }
    js_newarray(J); // list
    char *fullpath = NULL;
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
        js_pushstring(J, name); // list index name
        js_setindex(J, -2, n++);
    }
    talloc_free(fullpath);
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

static void script_join_path(js_State *J)
{
    const char *p1 = js_tostring(J, 1);
    const char *p2 = js_tostring(J, 2);
    char *r = mp_path_join(NULL, bstr0(p1), bstr0(p2));
    js_pushstring(J, r);
    talloc_free(r);
}

#if HAVE_POSIX_SPAWN || defined(__MINGW32__)
struct subprocess_cb_ctx {
    struct mp_log *log;
    void* talloc_ctx;
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

//args: client invocation args object, and a userdata object with talloc context to be used
static void script_subprocess_exec(js_State *J)
{
    struct script_ctx *ctx = get_ctx(J);
    if (!js_isobject(J, 1))
      js_error(J, "argument must be an object");

    void *tmp = js_touserdata(J, 2, "talloc_ctx");

    resume_all(ctx);

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
        args[n] = talloc_strdup(tmp, js_tostring(J, -1));
        js_pop(J, 1); // args
    }
    args[num_args] = NULL;
    js_pop(J, 1); // -

    js_getproperty(J, 1, "cancellable"); // c
    struct mp_cancel *cancel = NULL;
    if (js_isundefined(J, -1) ? true : js_toboolean(J, -1))
        cancel = ctx->mpctx->playback_abort;
    js_pop(J, 1); // -

    js_getproperty(J, 1, "max_size"); // m
    int64_t max_size = js_isundefined(J, -1) ? 16 * 1024 * 1024 : (int64_t)js_tointeger(J, -1);

    struct subprocess_cb_ctx cb_ctx = {
        .log = ctx->log,
        .talloc_ctx = tmp,
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
}

// since subprocess_exec can fail in several places, we allocate the memory in advance
// and pcall it, then release the data regardless if succeeded or failed.
static void script_subprocess(js_State *J)
{
    void *tmp = talloc_new(NULL);
    js_newcfunction(J, script_subprocess_exec, "subprocess_exec", 2);
    js_copy(J, 0);
    js_copy(J, 1);
    js_pushnull(J);
    js_newuserdata(J, "talloc_ctx", tmp);
    int err = js_pcall(J, 2);
    talloc_free(tmp);
    if (err)
        js_throw(J);
}
#else
static void script_subprocess(js_State *J)
{
    js_error("unimplemented");
}
#endif

//args: number - print
static void script_gc(js_State *J)
{
    js_gc(J, js_tonumber(J, 1));
    js_pushundefined(J);
}

#define FN_ENTRY(name, length) {#name, script_ ## name, length}
struct fn_entry {
    const char *name;
    void (*fn)(js_State *J);
    int length;
};

// functions starting with _ are wrapped at the js code.
static const struct fn_entry main_fns[] = {
    FN_ENTRY(suspend, 0),
    FN_ENTRY(resume, 0),
    FN_ENTRY(resume_all, 0),
    FN_ENTRY(wait_event, 1),
    FN_ENTRY(_request_event, 2),
    FN_ENTRY(find_config_file, 1),
    FN_ENTRY(command, 1),
    FN_ENTRY(commandv, 1),
    FN_ENTRY(command_native, 1),
    FN_ENTRY(get_property_bool, 2),
    FN_ENTRY(get_property_number, 2),
    FN_ENTRY(get_property_native, 2),
    FN_ENTRY(get_property, 2),
    FN_ENTRY(get_property_osd, 2),
    FN_ENTRY(set_property, 2),
    FN_ENTRY(set_property_bool, 2),
    FN_ENTRY(set_property_number, 2),
    FN_ENTRY(set_property_native, 2),
    FN_ENTRY(_observe_property, 3),
    FN_ENTRY(_unobserve_property, 1),
    FN_ENTRY(set_osd_ass, 3),
    FN_ENTRY(get_osd_resolution, 0),
    FN_ENTRY(get_screen_size, 0),
    FN_ENTRY(get_mouse_pos, 0),
    FN_ENTRY(get_time, 0),
    FN_ENTRY(get_time_ms, 0),
    FN_ENTRY(input_define_section, 3),
    FN_ENTRY(input_enable_section, 2),
    FN_ENTRY(input_disable_section, 1),
    FN_ENTRY(input_set_section_mouse_area, 5),
    FN_ENTRY(format_time, 2),
    FN_ENTRY(enable_messages, 1),
    FN_ENTRY(get_wakeup_pipe, 0),
    {0}
};

static const struct fn_entry utils_fns[] = {
    FN_ENTRY(getcwd, 0),
    FN_ENTRY(readdir, 2),
    FN_ENTRY(split_path, 1),
    FN_ENTRY(join_path, 2),
    FN_ENTRY(subprocess, 1),

    FN_ENTRY(read_file, 1),
    FN_ENTRY(load_file, 1),
    FN_ENTRY(run_file, 1),
    FN_ENTRY(gc, 1),
    {0}
};

static const struct fn_entry msg_fns[] = {
    FN_ENTRY(log, 1),
    FN_ENTRY(fatal, 0),
    FN_ENTRY(error, 0),
    FN_ENTRY(warn, 0),
    FN_ENTRY(info, 0),
    FN_ENTRY(verbose, 0),
    FN_ENTRY(debug, 0),
    {0}
};

// adds an object <module> with the functions at e to the current object on stack
static void register_package_fns(js_State *J, const char *module, const struct fn_entry *e)
{
    js_newobject(J);
    for (int n = 0; e[n].name; n++) {
        js_newcfunction(J, e[n].fn, e[n].name, e[n].length);
        js_setproperty(J, -2, e[n].name);
    }
    js_setproperty(J, -2, module);
}

static void add_functions(struct script_ctx *ctx)
{
    js_State *J = ctx->jsstate;

    js_pushglobal(J);
    register_package_fns(J, "mp", main_fns);

    js_getproperty(J, -1, "mp");

      js_pushstring(J, ctx->name);
      js_setproperty(J, -2, "script_name");

      char *res_name = mp_get_user_path(NULL, ctx->mpctx->global, get_ctx(J)->filename);
      js_pushstring(J, res_name);
      js_setproperty(J, -2, "script_path");
      talloc_free(res_name);

      register_package_fns(J, "msg", msg_fns);
      register_package_fns(J, "utils", utils_fns);

      js_newobject(J); // mp._formats
        js_pushnumber(J, MPV_FORMAT_NONE);
        js_setproperty(J, -2, "none");
        js_pushnumber(J, MPV_FORMAT_STRING);
        js_setproperty(J, -2, "string");
        js_pushnumber(J, MPV_FORMAT_FLAG);
        js_setproperty(J, -2, "bool");
        js_pushnumber(J, MPV_FORMAT_DOUBLE);
        js_setproperty(J, -2, "number");
        js_pushnumber(J, MPV_FORMAT_NODE);
        js_setproperty(J, -2, "native");
        js_pushnumber(J, MPV_FORMAT_OSD_STRING); // currently unused
        js_setproperty(J, -2, "osd");            //
      js_setproperty(J, -2, "_formats");

    js_pop(J, 1);
}

const struct mp_scripting mp_scripting_js = {
    .file_ext = "js",
    .load = load_javascript,
};

////////////// alternative implementations and debug ////////////
/*
// printf the node as json
static void debug_node (mpv_node *node)
{
    char *str = talloc_strdup(NULL, "");
    json_write(&str, node);
    printf("--node:--\n%s\n", str);
    talloc_free(str);
}

// alternative implementation using JSON: push a json string, then use js to parse it into an object
static void pushnode(js_State *J, mpv_node *node)
{
    // convert the node to json
    char *str = talloc_strdup(NULL, "");
    json_write(&str, node);

    // then parse it in js
    js_getglobal(J, "JSON");
    js_getproperty(J, -1, "parse");
    js_remove(J, -2); // JSON
    js_pushglobal(J);
    js_pushstring(J, str);
    js_call(J, 1);

    talloc_free(str);
}

// alternative implementation using json, where we use js to stringify the object on stack
// and then json_parse locally to create a node of it
static void makenode(void *ta_ctx, mpv_node *dst, js_State *J, int idx)
{
    if (idx < 0) idx += js_gettop(J);
    js_getglobal(J, "JSON");
    js_getproperty(J, -1, "stringify");
    js_remove(J, -2); // JSON
    js_pushglobal(J);
    js_copy(J, idx);
    js_call(J, 1);
    char *str = talloc_strdup(ta_ctx, js_tostring(J, -1));
    js_pop(J, 1);

    if (json_parse(ta_ctx, dst, &str, 10) < 0)
        dst->format = MPV_FORMAT_NONE;
}
*/
