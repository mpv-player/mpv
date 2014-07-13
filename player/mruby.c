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

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/compile.h>
#include <mruby/error.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include "common/msg.h"
#include "common/msg_control.h"
#include "options/m_property.h"
#include "options/path.h"
#include "player/command.h"
#include "player/core.h"
#include "player/client.h"
#include "libmpv/client.h"
#include "mpv_talloc.h"

static const char * const mruby_scripts[][2] = {
    {"mpv/reply.mrb",
#   include "player/mruby/reply.inc"
    },
    {"mpv/logging.mrb",
#   include "player/mruby/logging.inc"
    },
    {"mpv/events.mrb",
#   include "player/mruby/events.inc"
    },
    {0}
};

struct script_ctx {
    mrb_state *state;

    const char *name;
    const char *filename;
    struct mp_log *log;
    struct mpv_handle *client;
    struct MPContext *mpctx;
};

static struct script_ctx *get_ctx(mrb_state *mrb)
{
    mrb_sym sym = mrb_intern_cstr(mrb, "mpctx");
    mrb_value mrbctx = mrb_vm_const_get(mrb, sym);;
    return mrb_cptr(mrbctx);
}

static int get_loglevel(char *level)
{
    for (int n = 0; n < MSGL_MAX; n++) {
        if (mp_log_levels[n] && strcasecmp(mp_log_levels[n], level) == 0)
            return n;
    }
    abort();
}

static mrb_value api_return(mrb_state *mrb, int err, mrb_value value)
{
    const char* status = mpv_error_string(err);
    struct RClass *M = mrb_module_get(mrb, "M");
    struct RClass *c = mrb_class_get_under(mrb, M, "Reply");
    mrb_value init_args[2] = { value, mrb_str_new_cstr(mrb, status) };
    return mrb_obj_new(mrb, c, MP_ARRAY_SIZE(init_args), init_args);
}

#define api_return_bool(mrb, err) api_return(mrb, err, mrb_bool_value(err >= 0))
#define api_return_val(mrb, err, val) \
    api_return(mrb, err, err >= 0 ? val : mrb_nil_value())

static mrb_value _log(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    char *string;
    char *level;
    mrb_get_args(mrb, "zz", &level, &string);
    mp_msg(ctx->log, get_loglevel(level), "%s", string);
    return mrb_nil_value();
}

static mrb_value _find_config_file(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    char *s;
    mrb_get_args(mrb, "z", &s);
    char *path = mp_find_config_file(NULL, ctx->mpctx->global, s);
    mrb_value r = path ? mrb_str_new_cstr(mrb, path) : mrb_nil_value();
    talloc_free(path);
    return api_return_val(mrb, 0, r);
}

static mrb_value mpv_to_mrb_root(mrb_state *mrb, mpv_node node, bool root)
{
    switch (node.format) {
    case MPV_FORMAT_STRING:
        return mrb_str_new_cstr(mrb, node.u.string);
    case MPV_FORMAT_FLAG:
        return mrb_bool_value(node.u.flag > 0);
    case MPV_FORMAT_INT64:
        return mrb_fixnum_value(node.u.int64);
    case MPV_FORMAT_DOUBLE:
        return mrb_float_value(mrb, node.u.double_);
    case MPV_FORMAT_NODE_ARRAY: {
        mrb_value ary = mrb_ary_new(mrb);
        int ai = mrb_gc_arena_save(mrb);
        for (int n = 0; n < node.u.list->num; n++) {
            mrb_value item = mpv_to_mrb_root(mrb, node.u.list->values[n], false);
            mrb_ary_push(mrb, ary, item);
        }
        if (root)
            mrb_gc_arena_restore(mrb, ai);
        return ary;
    }
    case MPV_FORMAT_NODE_MAP: {
        mrb_value hash = mrb_hash_new(mrb);
        int ai = mrb_gc_arena_save(mrb);
        for (int n = 0; n < node.u.list->num; n++) {
            mrb_value key = mrb_str_new_cstr(mrb, node.u.list->keys[n]);
            mrb_value val = mpv_to_mrb_root(mrb, node.u.list->values[n], false);
            mrb_hash_set(mrb, hash, key, val);
        }
        if (root)
            mrb_gc_arena_restore(mrb, ai);
        return hash;
    }
    default: {
        struct script_ctx *ctx = get_ctx(mrb);
        MP_ERR(ctx, "mpv_node mapping failed (format: %d).\n", node.format);
        return mrb_nil_value();
    }
    }
}

#define mpv_to_mrb(mrb, node) mpv_to_mrb_root(mrb, node, true)

static mrb_value _get_property(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    char *name;
    mrb_get_args(mrb, "z", &name);
    mpv_node node;
    int err = mpv_get_property(ctx->client, name, MPV_FORMAT_NODE, &node);
    return api_return_val(mrb, err, mpv_to_mrb(mrb, node));
}

static mpv_node mrb_to_mpv(void *ta_ctx, mrb_state *mrb, mrb_value value)
{
    mpv_node res;
    switch (mrb_type(value)) {
    case MRB_TT_TRUE:
        res.format  = MPV_FORMAT_FLAG;
        res.u.flag  = 1;
        break;
    case MRB_TT_FALSE: {
        // MRB_TT_FALSE is used for both `nil` and `false`
        if (mrb_nil_p(value)) {
            res.format = MPV_FORMAT_NONE;
        } else {
            res.format = MPV_FORMAT_FLAG;
            res.u.flag = 0;
        }
        break;
    }
    case MRB_TT_FIXNUM:
        res.format  = MPV_FORMAT_INT64;
        res.u.int64 = mrb_fixnum(value);
        break;
    case MRB_TT_FLOAT:
        res.format    = MPV_FORMAT_DOUBLE;
        res.u.double_ = mrb_float(value);
        break;
    case MRB_TT_STRING:
        res.format = MPV_FORMAT_STRING;
        res.u.string = talloc_strdup(ta_ctx, RSTRING_PTR(value));
        break;
    case MRB_TT_ARRAY: {
        mpv_node_list *list = talloc_zero(ta_ctx, mpv_node_list);
        res.format = MPV_FORMAT_NODE_ARRAY;
        res.u.list = list;
        mrb_int len = mrb_ary_len(mrb, value);
        for (int i = 0; i < len; i++) {
            MP_TARRAY_GROW(ta_ctx, list->values, list->num);
            mrb_value item  = mrb_ary_entry(value, i);
            list->values[i] = mrb_to_mpv(ta_ctx, mrb, item);
            list->num++;
        }
        break;
    }
    case MRB_TT_HASH: {
        mpv_node_list *list = talloc_zero(ta_ctx, mpv_node_list);
        res.format = MPV_FORMAT_NODE_MAP;
        res.u.list = list;

        mrb_value keys = mrb_hash_keys(mrb, value);
        mrb_int len    = mrb_ary_len(mrb, mrb_hash_keys(mrb, value));
        for (int i = 0; i < len; i++) {
            MP_TARRAY_GROW(ta_ctx, list->keys,   list->num);
            MP_TARRAY_GROW(ta_ctx, list->values, list->num);
            mrb_value key   = mrb_ary_entry(keys, i);
            mrb_value skey  = mrb_funcall(mrb, key, "to_s", 0);
            mrb_value item  = mrb_hash_get(mrb, value, key);
            list->keys[i]   = talloc_strdup(ta_ctx, RSTRING_PTR(skey));
            list->values[i] = mrb_to_mpv(ta_ctx, mrb, item);
            list->num++;
        }
        break;
    }
    default: {
        struct script_ctx *ctx = get_ctx(mrb);
        MP_ERR(ctx, "mrb_value mapping failed (class: %s).\n",
               mrb_obj_classname(mrb, value));
    }
    }
    return res;
}

static mrb_value _set_property(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    char *key;
    mrb_value value;
    mrb_get_args(mrb, "zo", &key, &value);

    void *ta_ctx = talloc_new(NULL);
    mpv_node node = mrb_to_mpv(ta_ctx, mrb, value);
    int res = mpv_set_property(ctx->client, key, MPV_FORMAT_NODE, &node);
    talloc_free(ta_ctx);
    return api_return_bool(mrb, res);
}

#define mrb_hash_set_str(h, k, v) \
    mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, k), mrb_str_new_cstr(mrb, v))

static mrb_value _wait_event(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    mrb_float timeout;
    mrb_get_args(mrb, "f", &timeout);
    mpv_event *event = mpv_wait_event(ctx->client, timeout);

    struct RClass *M = mrb_module_get(mrb, "M");
    struct RClass *c = mrb_class_get_under(mrb, M, "Event");

    mrb_value data = mrb_hash_new(mrb);

    switch (event->event_id) {
    case MPV_EVENT_LOG_MESSAGE: {
        mpv_event_log_message *msg = event->data;
        mrb_hash_set_str(data, "prefix", msg->prefix);
        mrb_hash_set_str(data, "level",  msg->level);
        mrb_hash_set_str(data, "text",   msg->text);
        break;
    }
    case MPV_EVENT_SCRIPT_INPUT_DISPATCH: {
        mpv_event_script_input_dispatch *msg = event->data;
        mrb_value arg0 = mrb_fixnum_value(msg->arg0);
        mrb_hash_set(mrb, data, mrb_str_new_cstr(mrb, "arg0"), arg0);
        mrb_hash_set_str(data, "type", msg->type);
        break;
    }
    case MPV_EVENT_CLIENT_MESSAGE: {
        mpv_event_client_message *msg = event->data;
        mrb_value args = mrb_ary_new(mrb);
        for (int n = 0; n < msg->num_args; n++)
            mrb_ary_push(mrb, args, mrb_str_new_cstr(mrb, msg->args[n]));
        mrb_hash_set(mrb, data, mrb_str_new_cstr(mrb, "args"), args);
        break;
    }
    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = event->data;
        mrb_hash_set_str(data, "name", prop->name);
        mpv_node node;

        if (prop->format == MPV_FORMAT_NODE) {
            node = *(mpv_node*)prop->data;
        } else {
            node = (mpv_node) { .format = MPV_FORMAT_NONE };
        }

        mrb_value value = mpv_to_mrb(mrb, node);
        mrb_hash_set(mrb, data, mrb_str_new_cstr(mrb, "value"), value);
    }
    default: ;
    }

    mrb_value init_args[4] = {
        mrb_fixnum_value(event->reply_userdata),
        mrb_str_new_cstr(mrb, mpv_event_name(event->event_id)),
        mrb_str_new_cstr(mrb, mpv_error_string(event->error)),
        data
    };

    return mrb_obj_new(mrb, c, MP_ARRAY_SIZE(init_args), init_args);
}

static mrb_value _observe_property_raw(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    mrb_int id;
    char *name;
    mrb_get_args(mrb, "iz", &id, &name);
    int err = mpv_observe_property(ctx->client, id, name, MPV_FORMAT_NODE);
    return api_return_val(mrb, err, mrb_fixnum_value(id));
}

static mrb_value _unobserve_property_raw(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    mrb_int id;
    mrb_get_args(mrb, "i", &id);
    int err = mpv_unobserve_property(ctx->client, id);
    return api_return_bool(mrb, err);
}

static mrb_value _request_event(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    char *event;
    mrb_bool enable;
    mrb_get_args(mrb, "zb", &event, &enable);

    int event_id = -1;
    for (int n = 0; n < 256; n++) {
        const char *name = mpv_event_name(n);
        if (name && strcmp(name, event) == 0) {
            event_id = n;
            break;
        }
    }

    int err = mpv_request_event(ctx->client, event_id, enable);
    return api_return_bool(mrb, err);
}

static mrb_value _command(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    char *command;
    mrb_get_args(mrb, "z", &command);
    return api_return_bool(mrb, mpv_command_string(ctx->client, command));
}

static mrb_value _commandv(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    mrb_value *commands;
    mrb_int size;
    mrb_get_args(mrb, "*", &commands, &size);
    char **args = talloc_zero_array(NULL, char *, size + 1);
    for (int i = 0; i < size; i++)
        args[i] = talloc_strdup(args, RSTRING_PTR(commands[i]));
    int err = mpv_command(ctx->client, (const char **)args);
    talloc_free(args);
    return api_return_bool(mrb, err);
}

static mrb_value _get_time(mrb_state *mrb, mrb_value self)
{
    struct script_ctx *ctx = get_ctx(mrb);
    const double secs = mpv_get_time_us(ctx->client) / (double)(1e6);
    return mrb_float_value(mrb, secs);
}

#define MRB_FN(a,b) mrb_define_module_function(mrb, mod, #a, _ ## a, (b))
static void define_module(mrb_state *mrb)
{
    struct RClass *mod = mrb_define_module(mrb, "M");
    MRB_FN(log,                     MRB_ARGS_REQ(1));
    MRB_FN(find_config_file,        MRB_ARGS_REQ(1));
    MRB_FN(get_property,            MRB_ARGS_REQ(1));
    MRB_FN(set_property,            MRB_ARGS_REQ(2));
    MRB_FN(wait_event,              MRB_ARGS_REQ(1));
    MRB_FN(observe_property_raw,    MRB_ARGS_REQ(2));
    MRB_FN(unobserve_property_raw,  MRB_ARGS_REQ(1));
    MRB_FN(request_event,           MRB_ARGS_REQ(2));
    MRB_FN(command,                 MRB_ARGS_REQ(1));
    MRB_FN(commandv,                MRB_ARGS_ANY());
    MRB_FN(get_time,                MRB_ARGS_NONE());
}
#undef MRB_FN

static bool print_backtrace(mrb_state *mrb)
{
    if (!mrb->exc)
        return true;

    mrb_value exc = mrb_obj_value(mrb->exc);
    mrb_value bt  = mrb_exc_backtrace(mrb, exc);

    int ai = mrb_gc_arena_save(mrb);

    char *err = talloc_strdup(NULL, "");
    mrb_value exc_str = mrb_inspect(mrb, exc);
    err = talloc_asprintf_append(err, "%s\n", RSTRING_PTR(exc_str));

    mrb_int bt_len = mrb_ary_len(mrb, bt);
    err = talloc_asprintf_append(err, "backtrace:\n");
    for (int i = 0; i < bt_len; i++) {
        mrb_value s = mrb_ary_entry(bt, i);
        err = talloc_asprintf_append(err, "\t[%d] => %s\n", i, RSTRING_PTR(s));
    }

    mrb_gc_arena_restore(mrb, ai);

    struct script_ctx *ctx = get_ctx(mrb);
    MP_ERR(ctx, "%s", err);
    talloc_free(err);
    return false;
}

typedef mrb_value (*runner)(mrb_state *, const void*, mrbc_context *);

static bool run_script(mrb_state *mrb, runner runner,
                       const void *runee, const char *name)
{
    mrbc_context *mrb_ctx = mrbc_context_new(mrb);
    mrbc_filename(mrb, mrb_ctx, name);
    runner(mrb, runee, mrb_ctx);
    bool err = print_backtrace(mrb);
    mrbc_context_free(mrb, mrb_ctx);
    return err;
}

static bool load_environment(mrb_state *mrb)
{
    for (int n = 0; mruby_scripts[n][0]; n++) {
        const char *script = mruby_scripts[n][1];
        const char *fname  = mruby_scripts[n][0];
        if (!run_script(mrb, (runner) mrb_load_string_cxt, script, fname))
            return false;
    }
    return true;
}

static bool load_script(mrb_state *mrb, const char *fname)
{
    struct script_ctx *ctx = get_ctx(mrb);
    char *file_path = mp_get_user_path(NULL, ctx->mpctx->global, fname);
    FILE *fp = fopen(file_path, "r");
    bool result = run_script(mrb, (runner) mrb_load_file_cxt, fp, fname);
    fclose(fp);
    talloc_free(file_path);
    return result;
}

static int load_mruby(struct mpv_handle *client, const char *fname)
{
    struct MPContext *mpctx = mp_client_get_core(client);
    int r = -1;

    struct script_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct script_ctx) {
        .name     = mpv_client_name(client),
        .filename = fname,
        .log      = mp_client_get_log(client),
        .client   = client,
        .mpctx    = mpctx,
    };

    mrb_state *mrb = ctx->state = mrb_open();
    mrb_sym sym = mrb_intern_cstr(mrb, "mpctx");
    mrb_vm_const_set(mrb, sym, mrb_cptr_value(mrb, ctx));
    define_module(mrb);

    if (!mrb)
        goto err_out;

    if (!load_environment(mrb))
        goto err_out;

    if (!load_script(mrb, fname))
        goto err_out;

    if (!run_script(mrb, (runner) mrb_load_string_cxt, "M.run", "event_loop"))
        goto err_out;

    r = 0;

err_out:
    if (ctx->state)
        mrb_close(ctx->state);
    talloc_free(ctx);
    return r;
}

const struct mp_scripting mp_scripting_mruby = {
    .file_ext = "mrb",
    .load = load_mruby,
};
