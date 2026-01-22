#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <quickjs-libc.h>
#include <quickjs.h>

#include "../client.h"
#include "../command.h"
#include "../common/common.h"
#include "../common/msg.h"
#include "../common/msg_control.h"
#include "../common/stats.h"
#include "../core.h"
#include "../input/input.h"
#include "../misc/bstr.h"
#include "../mpv_talloc.h"
#include "../options/m_option.h"
#include "../options/m_property.h"
#include "../options/path.h"
#include "../osdep/io.h"
#include "../stream/stream.h"
#include "mpv/client.h"
#include "ta/ta.h"

// List of builtin modules and their contents as strings.
// All these are generated from player/javascript/*.js
static const char *const builtin_files[][3] = {{
                                                   "@/defaults.js",
#include "player/javascript/defaults.js.inc"
                                               },
                                               {0}};

struct script_ctx {
    const char *filename;
    const char *path; // NULL if single file
    struct mpv_handle *client;
    struct MPContext *mpctx;
    struct mp_log *log;
    char *last_error_str;
    struct stats_ctx *stats;
    JSRuntime *rt;
    JSContext *qctx;
};

static struct script_ctx *jctx(JSContext *ctx) {
    return JS_GetContextOpaque(ctx);
}

static mpv_handle *jclient(JSContext *ctx) { return jctx(ctx)->client; }

static JSValue node_to_js(JSContext *ctx, const mpv_node *node);
static int js_to_node(void *ta_ctx, mpv_node *dst, JSContext *ctx,
                      JSValueConst val);

// Utilities ---------------------------------------------------------------
static void set_last_error(struct script_ctx *ctx, bool iserr,
                           const char *str) {
    ctx->last_error_str[0] = 0;
    if (!iserr)
        return;
    if (!str || !str[0])
        str = "Error";
    ctx->last_error_str = talloc_strdup_append(ctx->last_error_str, str);
}

static JSValue push_success(JSContext *ctx) {
    set_last_error(jctx(ctx), 0, NULL);
    return JS_NewBool(ctx, true);
}

static JSValue push_failure(JSContext *ctx, const char *str) {
    set_last_error(jctx(ctx), 1, str);
    return JS_UNDEFINED;
}

static JSValue push_status(JSContext *ctx, int err) {
    if (err >= 0)
        return push_success(ctx);
    return push_failure(ctx, mpv_error_string(err));
}

static bool pushed_error(JSContext *ctx, int err, JSValueConst def,
                         JSValue *out) {
    bool iserr = err < 0;
    set_last_error(jctx(ctx), iserr, iserr ? mpv_error_string(err) : NULL);
    if (!iserr)
        return false;
    *out = JS_DupValue(ctx, def);
    return true;
}

static const char *js_to_cstring(JSContext *ctx, JSValueConst v) {
    const char *s = JS_ToCString(ctx, v);
    if (!s) {
        JS_ThrowTypeError(ctx, "expected string");
    }
    return s;
}

static double js_to_number(JSContext *ctx, JSValueConst v) {
    double d;
    if (JS_ToFloat64(ctx, &d, v) < 0)
        return NAN;
    return d;
}

static int64_t js_to_int64_checked(JSContext *ctx, JSValueConst v, int idx) {
    double d = js_to_number(ctx, v);
    if (!(d >= INT64_MIN && d <= (double)INT64_MAX)) {
        JS_ThrowRangeError(ctx, "int out of range at index %d", idx);
    }
    return (int64_t)d;
}

static uint64_t js_to_uint64_checked(JSContext *ctx, JSValueConst v, int idx) {
    double d = js_to_number(ctx, v);
    if (!(d >= 0 && d <= (double)UINT64_MAX))
        JS_ThrowRangeError(ctx, "uint64 out of range at index %d", idx);
    return (uint64_t)d;
}

static bool js_is_truthy(JSContext *ctx, JSValueConst v) {
    return JS_ToBool(ctx, v) == 1;
}

static inline int js_get_tag(JSValueConst v) {
    return JS_VALUE_GET_NORM_TAG(v);
}

// Builtin file lookup -----------------------------------------------------
static const char *get_builtin_file(const char *name) {
    for (int n = 0; builtin_files[n][0]; n++) {
        if (strcmp(builtin_files[n][0], name) == 0)
            return builtin_files[n][1];
    }
    return NULL;
}

static JSValue read_file_limit(JSContext *ctx, const char *fname, int limit) {
    void *af = talloc_new(NULL);
    JSValue ret = JS_UNDEFINED;

    char *filename = mp_get_user_path(af, jctx(ctx)->mpctx->global, fname);
    MP_VERBOSE(jctx(ctx), "Reading file '%s'\n", filename);
    if (limit < 0) {
        limit = INT_MAX - 1;
    }

    const char *builtin = get_builtin_file(filename);
    if (builtin) {
        ret = JS_NewStringLen(ctx, builtin, MPMIN(limit, strlen(builtin)));
        goto out;
    }

    int flags = STREAM_READ_FILE_FLAGS_DEFAULT | STREAM_ALLOW_PARTIAL_READ |
                STREAM_SILENT;
    bstr data =
        stream_read_file2(filename, af, flags, jctx(ctx)->mpctx->global, limit);
    if (data.start) {
        ret = JS_NewStringLen(ctx, data.start, data.len);
    } else {
        ret = JS_ThrowInternalError(ctx, "cannot open file: '%s'", filename);
    }

out:
    talloc_free(af);
    return ret;
}

// Logging ----------------------------------------------------------------
// TS: (level: string, ...message: any[]) => boolean
static JSValue js_log(JSContext *ctx, JSValueConst this_val, int argc,
                      JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "log requires level");
    const char *level = js_to_cstring(ctx, argv[0]);
    int msgl = mp_msg_find_level(level);
    if (msgl < 0) {
        JS_FreeCString(ctx, level);
        return JS_ThrowRangeError(ctx, "Invalid log level '%s'", level);
    }

    struct mp_log *log = jctx(ctx)->log;
    for (int i = 1; i < argc; i++) {
        const char *s = JS_ToCString(ctx, argv[i]);
        if (!s)
            s = "<invalid>";
        mp_msg(log, msgl, (i == 1 ? "%s" : " %s"), s);
        JS_FreeCString(ctx, s);
    }
    mp_msg(log, msgl, "\n");
    JS_FreeCString(ctx, level);
    return push_success(ctx);
}

// Basic helpers ----------------------------------------------------------
static int checkopt(JSContext *ctx, JSValueConst val, const char *def,
                    const char *opts[], const char *desc) {
    const char *opt;
    if (JS_IsUndefined(val)) {
        opt = def;
    } else {
        opt = js_to_cstring(ctx, val);
    }
    for (int i = 0; opts[i]; i++) {
        if (strcmp(opt, opts[i]) == 0) {
            if (!JS_IsUndefined(val))
                JS_FreeCString(ctx, opt);
            return i;
        }
    }
    JS_ThrowRangeError(ctx, "Invalid %s '%s'", desc, opt);
    if (!JS_IsUndefined(val))
        JS_FreeCString(ctx, opt);
    return -1;
}

// mpv property and command helpers --------------------------------------
// TS: (cmd: string) => boolean
static JSValue js_command(JSContext *ctx, JSValueConst this_val, int argc,
                          JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "command expects a string");
    const char *cmd = js_to_cstring(ctx, argv[0]);
    int r = mpv_command_string(jclient(ctx), cmd);
    JS_FreeCString(ctx, cmd);
    return push_status(ctx, r);
}

// TS: (...args: string[]) => boolean
static JSValue js_commandv(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
    const char *cargv[MP_CMD_MAX_ARGS + 1];
    if (argc >= MP_ARRAY_SIZE(cargv))
        return JS_ThrowRangeError(ctx, "Too many arguments");

    for (int i = 0; i < argc; i++)
        cargv[i] = js_to_cstring(ctx, argv[i]);
    cargv[argc] = NULL;
    int r = mpv_command(jclient(ctx), cargv);
    for (int i = 0; i < argc; i++)
        JS_FreeCString(ctx, cargv[i]);
    return push_status(ctx, r);
}

// TS: (name: string, value: string) => boolean
static JSValue js_set_property(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "set_property expects name, value");
    const char *name = js_to_cstring(ctx, argv[0]);
    const char *val = js_to_cstring(ctx, argv[1]);
    int r = mpv_set_property_string(jclient(ctx), name, val);
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, val);
    return push_status(ctx, r);
}

// TS: (name: string, value: boolean) => boolean
static JSValue js_set_property_bool(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "set_property_bool expects 2 args");
    const char *name = js_to_cstring(ctx, argv[0]);
    int v = js_is_truthy(ctx, argv[1]);
    int r = mpv_set_property(jclient(ctx), name, MPV_FORMAT_FLAG, &v);
    JS_FreeCString(ctx, name);
    return push_status(ctx, r);
}

// TS: (name: string, value: number) => boolean
static JSValue js_set_property_number(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "set_property_number expects 2 args");
    const char *name = js_to_cstring(ctx, argv[0]);
    int tag = JS_VALUE_GET_NORM_TAG(argv[1]);
    mpv_handle *h = jclient(ctx);
    int r;
    switch (tag) {
    case JS_TAG_INT: {
        int64_t iv = 0;
        if (JS_ToInt64(ctx, &iv, argv[1]) < 0) {
            JS_FreeCString(ctx, name);
            return JS_EXCEPTION;
        }
        r = mpv_set_property(h, name, MPV_FORMAT_INT64, &iv);
        break;
    }
    case JS_TAG_FLOAT64: {
        double dv = JS_VALUE_GET_FLOAT64(argv[1]);
        r = mpv_set_property(h, name, MPV_FORMAT_DOUBLE, &dv);
        break;
    }
    default: {
        double v = js_to_number(ctx, argv[1]);
        r = mpv_set_property(h, name, MPV_FORMAT_DOUBLE, &v);
        break;
    }
    }
    JS_FreeCString(ctx, name);
    return push_status(ctx, r);
}

// TS: (name: string, def?: any) => string | undefined
static JSValue js_get_property_string(JSContext *ctx, const char *name,
                                      JSValueConst def) {
    char *res = NULL;
    int r = mpv_get_property(jclient(ctx), name, MPV_FORMAT_STRING, &res);
    if (r < 0) {
        JSValue out;
        if (pushed_error(ctx, r, def, &out))
            return out;
    }
    JSValue ret = JS_NewString(ctx, res ? res : "");
    if (res)
        mpv_free(res);
    return ret;
}

// TS: (name: string, def?: any) => string | undefined
static JSValue js_get_property(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "get_property expects name");
    const char *name = js_to_cstring(ctx, argv[0]);
    JSValue def = argc >= 2 ? argv[1] : JS_UNDEFINED;
    JSValue ret = js_get_property_string(ctx, name, def);
    JS_FreeCString(ctx, name);
    return ret;
}

// TS: (name: string, def?: any) => boolean | undefined
static JSValue js_get_property_bool(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "get_property_bool expects name");
    const char *name = js_to_cstring(ctx, argv[0]);
    JSValue def = argc >= 2 ? argv[1] : JS_UNDEFINED;
    int result = 0;
    int r = mpv_get_property(jclient(ctx), name, MPV_FORMAT_FLAG, &result);
    JS_FreeCString(ctx, name);
    if (r < 0) {
        JSValue out;
        if (pushed_error(ctx, r, def, &out))
            return out;
    }
    return JS_NewBool(ctx, result);
}

// TS: (name: string, def?: any) => number | undefined
static JSValue js_get_property_number(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "get_property_number expects name");
    const char *name = js_to_cstring(ctx, argv[0]);
    JSValue def = argc >= 2 ? argv[1] : JS_UNDEFINED;
    double result = 0;
    int r = mpv_get_property(jclient(ctx), name, MPV_FORMAT_DOUBLE, &result);
    JS_FreeCString(ctx, name);
    if (r < 0) {
        JSValue out;
        if (pushed_error(ctx, r, def, &out))
            return out;
    }
    return JS_NewFloat64(ctx, result);
}

// TS: (name: string) => boolean
static JSValue js_del_property(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "del_property expects name");
    const char *name = js_to_cstring(ctx, argv[0]);
    int r = mpv_del_property(jclient(ctx), name);
    JS_FreeCString(ctx, name);
    return push_status(ctx, r);
}

// Node conversions -------------------------------------------------------
static JSValue node_to_js(JSContext *ctx, const mpv_node *node) {
    switch (node->format) {
    case MPV_FORMAT_NONE:
        return JS_NULL;
    case MPV_FORMAT_STRING:
        return JS_NewString(ctx, node->u.string);
    case MPV_FORMAT_INT64:
        return JS_NewInt64(ctx, node->u.int64);
    case MPV_FORMAT_DOUBLE:
        return JS_NewFloat64(ctx, node->u.double_);
    case MPV_FORMAT_FLAG:
        return JS_NewBool(ctx, node->u.flag);
    case MPV_FORMAT_BYTE_ARRAY:
        return JS_NewStringLen(ctx, (const char *)node->u.ba->data,
                               node->u.ba->size);
    case MPV_FORMAT_NODE_ARRAY: {
        JSValue arr = JS_NewArray(ctx);
        int len = node->u.list->num;
        for (int n = 0; n < len; n++) {
            JS_SetPropertyUint32(ctx, arr, n,
                                 node_to_js(ctx, &node->u.list->values[n]));
        }
        return arr;
    }
    case MPV_FORMAT_NODE_MAP: {
        JSValue obj = JS_NewObject(ctx);
        int len = node->u.list->num;
        for (int n = 0; n < len; n++) {
            JS_SetPropertyStr(ctx, obj, node->u.list->keys[n],
                              node_to_js(ctx, &node->u.list->values[n]));
        }
        return obj;
    }
    default:
        return JS_ThrowTypeError(ctx, "Unsupported mpv node format");
    }
}

// Returns 1 if buffer found, 0 if not a buffer, -1 on error
static int js_try_get_buffer(JSContext *ctx, JSValueConst val, uint8_t **data,
                             size_t *size) {
    if (JS_IsArrayBuffer(val)) {
        *data = JS_GetArrayBuffer(ctx, size, val);
        return *data ? 1 : -1;
    }

    size_t off = 0, len = 0;
    JSValue buffer = JS_GetTypedArrayBuffer(ctx, val, &off, &len, NULL);
    if (JS_IsException(buffer)) {
        JSValue exc = JS_GetException(ctx); // swallow type errors for non-TA
        JS_FreeValue(ctx, exc);
        return 0;
    }
    if (JS_IsArrayBuffer(buffer)) {
        *data = JS_GetArrayBuffer(ctx, size, buffer);
        JS_FreeValue(ctx, buffer);
        if (!*data)
            return -1;
        *data += off;
        *size = len;
        return 1;
    }
    JS_FreeValue(ctx, buffer);
    return 0;
}

static int get_obj_properties(void *ta_ctx, JSContext *ctx, JSValueConst obj,
                              char ***keys) {
    JSPropertyEnum *props = NULL;
    uint32_t len = 0;
    if (JS_GetOwnPropertyNames(ctx, &props, &len, obj,
                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
        return -1;
    }

    *keys = talloc_array(ta_ctx, char *, len);
    for (uint32_t i = 0; i < len; i++) {
        const char *name = JS_AtomToCString(ctx, props[i].atom);
        *keys[i] = talloc_strdup(ta_ctx, name);
        JS_FreeCString(ctx, name);
    }
    JS_FreePropertyEnum(ctx, props, len);
    return len;
}

// Converts a JS value to an mpv_node. Returns 0 on success, -1 on error
static int js_to_node(void *ta_ctx, mpv_node *dst, JSContext *ctx,
                      JSValueConst val) {
    int tag = js_get_tag(val);

    switch (tag) {
    case JS_TAG_UNDEFINED:
    case JS_TAG_NULL:
        dst->format = MPV_FORMAT_NONE;
        return 0;
    case JS_TAG_BOOL: {
        int b = JS_ToBool(ctx, val);
        if (b < 0)
            return -1;
        dst->format = MPV_FORMAT_FLAG;
        dst->u.flag = b;
        return 0;
    }
    case JS_TAG_INT: {
        int64_t i = 0;
        if (JS_ToInt64(ctx, &i, val) < 0)
            return -1;
        dst->format = MPV_FORMAT_INT64;
        dst->u.int64 = i;
        return 0;
    }
    case JS_TAG_FLOAT64:
        dst->format = MPV_FORMAT_DOUBLE;
        dst->u.double_ = JS_VALUE_GET_FLOAT64(val);
        return 0;
    case JS_TAG_STRING: {
        const char *s = JS_ToCString(ctx, val);
        if (!s)
            return -1;
        dst->format = MPV_FORMAT_STRING;
        dst->u.string = talloc_strdup(ta_ctx, s);
        JS_FreeCString(ctx, s);
        return 0;
    }
    case JS_TAG_OBJECT: {
        uint8_t *buf = NULL;
        size_t buf_len = 0;
        int buf_res = js_try_get_buffer(ctx, val, &buf, &buf_len);
        if (buf_res < 0)
            return -1;
        if (buf_res > 0) {
            dst->format = MPV_FORMAT_BYTE_ARRAY;
            mpv_byte_array *ba = talloc(ta_ctx, mpv_byte_array);
            ba->data = talloc_memdup(ba, buf, buf_len);
            ba->size = buf_len;
            dst->u.ba = ba;
            return 0;
        }

        if (JS_IsArray(val)) {
            int64_t len = 0;
            if (JS_GetLength(ctx, val, &len) < 0)
                return -1;

            dst->format = MPV_FORMAT_NODE_ARRAY;
            dst->u.list = talloc(ta_ctx, struct mpv_node_list);
            dst->u.list->keys = NULL;
            dst->u.list->num = len;
            dst->u.list->values = talloc_array(ta_ctx, mpv_node, len);
            for (uint32_t i = 0; i < len; i++) {
                JSValue v = JS_GetPropertyUint32(ctx, val, i);
                if (JS_IsException(v)) {
                    JS_FreeValue(ctx, v);
                    return -1;
                }
                if (js_to_node(ta_ctx, &dst->u.list->values[i], ctx, v) < 0) {
                    JS_FreeValue(ctx, v);
                    return -1;
                }
                JS_FreeValue(ctx, v);
            }
            return 0;
        }

        dst->format = MPV_FORMAT_NODE_MAP;
        dst->u.list = talloc(ta_ctx, struct mpv_node_list);
        int len = get_obj_properties(ta_ctx, ctx, val, &dst->u.list->keys);
        if (len < 0) {
            return -1;
        }
        dst->u.list->num = len;
        dst->u.list->values = talloc_array(ta_ctx, mpv_node, len);
        for (int i = 0; i < len; i++) {
            JSValue v = JS_GetPropertyStr(ctx, val, dst->u.list->keys[i]);
            if (JS_IsException(v)) {
                JS_FreeValue(ctx, v);
                return -1;
            }
            if (js_to_node(ta_ctx, &dst->u.list->values[i], ctx, v) < 0) {
                JS_FreeValue(ctx, v);
                return -1;
            }
            JS_FreeValue(ctx, v);
        }
        return 0;
    }
    default: {
        return -1;
    }
    }
}

// Property native conversions -------------------------------------------
// TS: (name: string, value: any) => boolean
static JSValue js_set_property_native(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "set_property_native expects 2 args");
    const char *name = js_to_cstring(ctx, argv[0]);
    void *af = talloc_new(NULL);
    mpv_node node = {0};
    if (js_to_node(af, &node, ctx, argv[1]) < 0) {
        JS_FreeCString(ctx, name);
        talloc_free(af);
        return JS_EXCEPTION;
    }
    int r = mpv_set_property(jclient(ctx), name, MPV_FORMAT_NODE, &node);
    JS_FreeCString(ctx, name);
    mpv_free_node_contents(&node);
    talloc_free(af);
    return push_status(ctx, r);
}

// TS: (name: string, def?: any) => any | undefined
static JSValue js_get_property_native(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "get_property_native expects name");
    const char *name = js_to_cstring(ctx, argv[0]);
    JSValue def = argc >= 2 ? argv[1] : JS_UNDEFINED;
    void *af = talloc_new(NULL);
    mpv_node node = {0};
    int r = mpv_get_property(jclient(ctx), name, MPV_FORMAT_NODE, &node);
    JSValue ret;
    if (r < 0) {
        if (pushed_error(ctx, r, def, &ret)) {
            JS_FreeCString(ctx, name);
            talloc_free(af);
            return ret;
        }
    }
    ret = node_to_js(ctx, &node);
    mpv_free_node_contents(&node);
    JS_FreeCString(ctx, name);
    talloc_free(af);
    return ret;
}

// TS: (name: string, def?: any) => string | undefined
static JSValue js_get_property_osd(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "get_property_osd expects name");
    const char *name = js_to_cstring(ctx, argv[0]);
    JSValue def = argc >= 2 ? argv[1] : JS_UNDEFINED;
    char *res = NULL;
    int r = mpv_get_property(jclient(ctx), name, MPV_FORMAT_OSD_STRING, &res);
    JSValue ret;
    if (r < 0) {
        if (pushed_error(ctx, r, def, &ret)) {
            JS_FreeCString(ctx, name);
            return ret;
        }
    }
    ret = JS_NewString(ctx, res ? res : "");
    if (res)
        mpv_free(res);
    JS_FreeCString(ctx, name);
    return ret;
}

// Events, observe, commands ---------------------------------------------
// TS: (event: string, enable: boolean) => boolean
static JSValue js_request_event(JSContext *ctx, JSValueConst this_val, int argc,
                                JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "_request_event expects name, enable");
    const char *event = js_to_cstring(ctx, argv[0]);
    bool enable = js_is_truthy(ctx, argv[1]);

    for (int n = 0; n < 256; n++) {
        const char *name = mpv_event_name(n);
        if (name && strcmp(name, event) == 0) {
            int r = mpv_request_event(jclient(ctx), n, enable);
            JS_FreeCString(ctx, event);
            return push_status(ctx, r);
        }
    }
    JS_FreeCString(ctx, event);
    return push_failure(ctx, "Unknown event name");
}

// TS: (level: string) => boolean
static JSValue js_enable_messages(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "enable_messages expects level");
    const char *level = js_to_cstring(ctx, argv[0]);
    int r = mpv_request_log_messages(jclient(ctx), level);
    if (r == MPV_ERROR_INVALID_PARAMETER) {
        JS_FreeCString(ctx, level);
        return JS_ThrowRangeError(ctx, "Invalid log level '%s'", level);
    }
    JS_FreeCString(ctx, level);
    return push_status(ctx, r);
}

// TS: (id: number, name: string, fmt: "none" | "native" | "bool" | "string" |
// "number") => boolean
static JSValue js_observe_property(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    if (argc < 3)
        return JS_ThrowTypeError(ctx,
                                 "_observe_property expects id, name, fmt");
    const char *fmts[] = {"none", "native", "bool", "string", "number", NULL};
    const mpv_format mf[] = {MPV_FORMAT_NONE, MPV_FORMAT_NODE, MPV_FORMAT_FLAG,
                             MPV_FORMAT_STRING, MPV_FORMAT_DOUBLE};
    uint64_t id = js_to_uint64_checked(ctx, argv[0], 1);
    const char *name = js_to_cstring(ctx, argv[1]);
    int fidx = checkopt(ctx, argv[2], "none", fmts, "observe type");
    if (fidx < 0) {
        JS_FreeCString(ctx, name);
        return JS_EXCEPTION;
    }
    int r = mpv_observe_property(jclient(ctx), id, name, mf[fidx]);
    JS_FreeCString(ctx, name);
    return push_status(ctx, r);
}

// TS: (id: number) => boolean
static JSValue js_unobserve_property(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "_unobserve_property expects id");
    uint64_t id = js_to_uint64_checked(ctx, argv[0], 1);
    return push_status(ctx, mpv_unobserve_property(jclient(ctx), id));
}

// TS: (cmd: any, def?: any) => any
static JSValue js_command_native(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "command_native expects native node");
    }
    JSValue def = argc >= 2 ? argv[1] : JS_UNDEFINED;
    void *af = talloc_new(NULL);
    mpv_node cmd = {0};
    if (js_to_node(af, &cmd, ctx, argv[0]) < 0) {
        talloc_free(af);
        return JS_EXCEPTION;
    }
    mpv_node result = {0};
    int r = mpv_command_node(jclient(ctx), &cmd, &result);
    JSValue ret;
    if (r < 0) {
        pushed_error(ctx, r, def, &ret);
        mpv_free_node_contents(&cmd);
        talloc_free(af);
        return ret;
    } else {
        ret = node_to_js(ctx, &result);
    }
    mpv_free_node_contents(&result);
    mpv_free_node_contents(&cmd);
    talloc_free(af);
    return ret;
}

// TS: (id: number, cmd: any) => boolean
static JSValue js_command_native_async(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "_command_native_async expects id, cmd");
    uint64_t id = js_to_uint64_checked(ctx, argv[0], 1);
    void *af = talloc_new(NULL);
    mpv_node cmd = {0};
    if (js_to_node(af, &cmd, ctx, argv[1]) < 0) {
        talloc_free(af);
        return JS_EXCEPTION;
    }
    int r = mpv_command_node_async(jclient(ctx), id, &cmd);
    mpv_free_node_contents(&cmd);
    talloc_free(af);
    return push_status(ctx, r);
}

// TS: (id: number) => boolean
static JSValue js_abort_async(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "_abort_async_command expects id");
    uint64_t id = js_to_uint64_checked(ctx, argv[0], 1);
    mpv_abort_async_command(jclient(ctx), id);
    return push_success(ctx);
}

// Time and input ---------------------------------------------------------
// TS: () => number
static JSValue js_get_time_ms(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv) {
    return JS_NewFloat64(ctx, mpv_get_time_us(jclient(ctx)) / 1000.0);
}

// TS: (section: string, x0: number, y0: number, x1: number, y1: number) =>
// boolean
static JSValue js_input_set_section_mouse_area(JSContext *ctx,
                                               JSValueConst this_val, int argc,
                                               JSValueConst *argv) {
    if (argc < 5) {
        return JS_ThrowTypeError(ctx,
                                 "input_set_section_mouse_area expects 5 args");
    }
    const char *section = js_to_cstring(ctx, argv[0]);
    mp_input_set_section_mouse_area(jctx(ctx)->mpctx->input, (char *)section,
                                    js_to_int64_checked(ctx, argv[1], 2),
                                    js_to_int64_checked(ctx, argv[2], 3),
                                    js_to_int64_checked(ctx, argv[3], 4),
                                    js_to_int64_checked(ctx, argv[4], 5));
    JS_FreeCString(ctx, section);
    return push_success(ctx);
}

// TS: (timeSec: number, fmt?: string) => string
static JSValue js_format_time(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "format_time expects time");
    double t = js_to_number(ctx, argv[0]);
    const char *fmt = argc >= 2 && !JS_IsUndefined(argv[1])
                          ? js_to_cstring(ctx, argv[1])
                          : "%H:%M:%S";
    char *r = mp_format_time_fmt(fmt, t);
    if (!r) {
        if (argc >= 2 && !JS_IsUndefined(argv[1]))
            JS_FreeCString(ctx, fmt);
        return JS_ThrowRangeError(ctx, "Invalid time format string");
    }
    JSValue ret = JS_NewString(ctx, r);
    talloc_free(r);
    if (argc >= 2 && !JS_IsUndefined(argv[1]))
        JS_FreeCString(ctx, fmt);
    return ret;
}

// TS: () => number
static JSValue js_get_wakeup_pipe(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    return JS_NewInt32(ctx, mpv_get_wakeup_pipe(jclient(ctx)));
}

// TS: (name: string, priority: number, id: number) => boolean
static JSValue js_hook_add(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
    if (argc < 3)
        return JS_ThrowTypeError(ctx, "_hook_add expects name, pri, id");
    const char *name = js_to_cstring(ctx, argv[0]);
    int pri = js_to_int64_checked(ctx, argv[1], 2);
    uint64_t id = js_to_uint64_checked(ctx, argv[2], 3);
    int r = mpv_hook_add(jclient(ctx), id, name, pri);
    JS_FreeCString(ctx, name);
    return push_status(ctx, r);
}

// TS: (id: number) => boolean
static JSValue js_hook_continue(JSContext *ctx, JSValueConst this_val, int argc,
                                JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "_hook_continue expects id");
    uint64_t id = js_to_uint64_checked(ctx, argv[0], 1);
    return push_status(ctx, mpv_hook_continue(jclient(ctx), id));
}

// Utils ------------------------------------------------------------------
// TS: (path?: string, filter?: "all" | "files" | "dirs" | "normal") => string[]
static JSValue js_readdir(JSContext *ctx, JSValueConst this_val, int argc,
                          JSValueConst *argv) {
    const char *filters[] = {"all", "files", "dirs", "normal", NULL};
    const char *path = (argc < 1 || JS_IsUndefined(argv[0]))
                           ? "."
                           : js_to_cstring(ctx, argv[0]);
    int t = checkopt(ctx, argc >= 2 ? argv[1] : JS_UNDEFINED, "normal", filters,
                     "listing filter");
    if (t < 0) {
        if (!(argc < 1 || JS_IsUndefined(argv[0])))
            JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        if (!(argc < 1 || JS_IsUndefined(argv[0])))
            JS_FreeCString(ctx, path);
        return push_failure(ctx, "Cannot open dir");
    }
    JSValue arr = JS_NewArray(ctx);
    char *fullpath = talloc_strdup(NULL, "");
    struct dirent *e;
    int n = 0;
    while ((e = readdir(dir))) {
        char *name = e->d_name;
        if (t) {
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;
            fullpath[0] = '\0';
            fullpath = talloc_asprintf_append(fullpath, "%s/%s", path, name);
            struct stat st;
            if (stat(fullpath, &st))
                continue;
            if (!(((t & 1) && S_ISREG(st.st_mode)) ||
                  ((t & 2) && S_ISDIR(st.st_mode))))
                continue;
        }
        JS_SetPropertyUint32(ctx, arr, n++, JS_NewString(ctx, name));
    }
    talloc_free(fullpath);
    closedir(dir);
    if (!(argc < 1 || JS_IsUndefined(argv[0])))
        JS_FreeCString(ctx, path);
    set_last_error(jctx(ctx), 0, NULL);
    return arr;
}

// TS: (file: string) => {mode: number; size: number; atime: number; mtime:
// number; ctime: number; is_file: boolean; is_dir: boolean}
static JSValue js_file_info(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "file_info expects path");
    const char *path = js_to_cstring(ctx, argv[0]);
    struct stat st;
    if (stat(path, &st) != 0) {
        JS_FreeCString(ctx, path);
        return push_failure(ctx, "Cannot stat path");
    }
    JS_FreeCString(ctx, path);
    set_last_error(jctx(ctx), 0, NULL);

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "mode", JS_NewInt64(ctx, st.st_mode));
    JS_SetPropertyStr(ctx, obj, "size", JS_NewInt64(ctx, st.st_size));
    JS_SetPropertyStr(ctx, obj, "atime", JS_NewInt64(ctx, st.st_atime));
    JS_SetPropertyStr(ctx, obj, "mtime", JS_NewInt64(ctx, st.st_mtime));
    JS_SetPropertyStr(ctx, obj, "ctime", JS_NewInt64(ctx, st.st_ctime));
    JS_SetPropertyStr(ctx, obj, "is_file",
                      JS_NewBool(ctx, S_ISREG(st.st_mode)));
    JS_SetPropertyStr(ctx, obj, "is_dir", JS_NewBool(ctx, S_ISDIR(st.st_mode)));
    return obj;
}

// TS: (path: string) => [string, string]
static JSValue js_split_path(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "split_path expects path");
    const char *p = js_to_cstring(ctx, argv[0]);
    bstr dir = mp_dirname(p);
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewStringLen(ctx, dir.start, dir.len));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewString(ctx, mp_basename(p)));
    JS_FreeCString(ctx, p);
    return arr;
}

// TS: (a: string, b: string) => string
static JSValue js_join_path(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "join_path expects 2 args");
    const char *a = js_to_cstring(ctx, argv[0]);
    const char *b = js_to_cstring(ctx, argv[1]);
    void *tmp = talloc_new(NULL);
    char *joined = mp_path_join(tmp, a, b);
    JSValue ret = JS_NewString(ctx, joined);
    talloc_free(tmp);
    JS_FreeCString(ctx, a);
    JS_FreeCString(ctx, b);
    return ret;
}

// TS: (append: boolean, name: string, data: string) => boolean
static JSValue js_write_file(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    if (argc < 3) {
        return JS_ThrowTypeError(ctx, "_write_file expects append, name, data");
    }
    bool append = js_is_truthy(ctx, argv[0]);
    const char *fname_js = js_to_cstring(ctx, argv[1]);
    const char *data_js = js_to_cstring(ctx, argv[2]);
    static const char *prefix = "file://";
    const char *opstr = append ? "append" : "write";
    if (strstr(fname_js, prefix) != fname_js) {
        JS_FreeCString(ctx, fname_js);
        JS_FreeCString(ctx, data_js);
        return JS_ThrowRangeError(ctx, "File name must be prefixed with '%s'",
                                  prefix);
    }
    char *fname = mp_get_user_path(NULL, jctx(ctx)->mpctx->global,
                                   fname_js + strlen(prefix));
    JS_FreeCString(ctx, fname_js);
    MP_VERBOSE(jctx(ctx), "%s file '%s'\n", opstr, fname);

    FILE *f = fopen(fname, append ? "ab" : "wb");
    if (!f) {
        JS_FreeCString(ctx, data_js);
        talloc_free(fname);
        return JS_ThrowInternalError(ctx, "Cannot open (%s) file: '%s'", opstr,
                                     fname);
    }

    int len = strlen(data_js);
    int wrote = fwrite(data_js, 1, len, f);
    fclose(f);
    JS_FreeCString(ctx, data_js);
    talloc_free(fname);
    if (len != wrote) {
        return JS_ThrowInternalError(ctx, "Cannot %s to file", opstr);
    }
    return JS_NewBool(ctx, true);
}

// TS: (name: string, limit?: number) => string
static JSValue js_read_file(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "read_file expects filename");
    }
    int limit = -1;
    if (argc >= 2 && !JS_IsUndefined(argv[1])) {
        limit = (int)js_to_int64_checked(ctx, argv[1], 2);
    }
    const char *fname = js_to_cstring(ctx, argv[0]);
    JSValue ret = read_file_limit(ctx, fname, limit);
    JS_FreeCString(ctx, fname);
    return ret;
}

// TS: (name: string) => string | undefined
static JSValue js_getenv(JSContext *ctx, JSValueConst this_val, int argc,
                         JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "getenv expects name");
    const char *name = js_to_cstring(ctx, argv[0]);
    const char *v = getenv(name);
    JS_FreeCString(ctx, name);
    if (v)
        return JS_NewString(ctx, v);
    return JS_UNDEFINED;
}

// TS: () => string[]
static JSValue js_get_env_list(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
    JSValue arr = JS_NewArray(ctx);
    for (int n = 0; environ && environ[n]; n++)
        JS_SetPropertyUint32(ctx, arr, n, JS_NewString(ctx, environ[n]));
    return arr;
}

// TS: (filename: string, code: string) => Function
static JSValue js_compile_js(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "compile_js expects filename, code");

    const char *code = js_to_cstring(ctx, argv[1]);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_GetPropertyStr(ctx, global, "Function");
    if (!JS_IsFunction(ctx, ctor)) {
        JS_FreeValue(ctx, ctor);
        JS_FreeValue(ctx, global);
        JS_FreeCString(ctx, code);
        return JS_ThrowInternalError(ctx, "global.Function is not callable");
    }

    JSValue arg = JS_NewString(ctx, code);
    JSValue ret = JS_CallConstructor(ctx, ctor, 1, &arg);
    JS_FreeValue(ctx, arg);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, global);
    JS_FreeCString(ctx, code);
    return ret;
}

// TS: () => boolean
static JSValue js_gc(JSContext *ctx, JSValueConst this_val, int argc,
                     JSValueConst *argv) {
    JS_RunGC(JS_GetRuntime(ctx));
    return push_success(ctx);
}

// Events waiting ---------------------------------------------------------
// TS: (timeoutSec?: number) => any
static JSValue js_wait_event(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    double timeout =
        (argc >= 1 && JS_IsNumber(argv[0])) ? js_to_number(ctx, argv[0]) : -1;
    mpv_event *event = mpv_wait_event(jclient(ctx), timeout);
    mpv_node node = {0};
    mpv_event_to_node(&node, event);
    JSValue ret = node_to_js(ctx, &node);
    mpv_free_node_contents(&node);
    return ret;
}

// Misc helpers -----------------------------------------------------------
// TS: (name: string) => string | undefined
static JSValue js_find_config_file(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "find_config_file expects name");
    const char *fname = js_to_cstring(ctx, argv[0]);
    char *path = mp_find_config_file(NULL, jctx(ctx)->mpctx->global, fname);
    JS_FreeCString(ctx, fname);
    if (path) {
        JSValue ret = JS_NewString(ctx, path);
        talloc_free(path);
        return ret;
    }
    return push_failure(ctx, "not found");
}

// TS: () => string
static JSValue js_last_error(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    return JS_NewString(ctx, jctx(ctx)->last_error_str);
}

// TS: (message: string) => void
static JSValue js_set_last_error(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "_set_last_error expects string");
    const char *e = js_to_cstring(ctx, argv[0]);
    set_last_error(jctx(ctx), e[0], e);
    JS_FreeCString(ctx, e);
    return JS_UNDEFINED;
}

// Function registration --------------------------------------------------
struct fn_entry {
    const char *name;
    int length;
    JSValue (*fn)(JSContext *, JSValueConst, int, JSValueConst *);
};

static const struct fn_entry main_fns[] = {
    {"log", 1, js_log},
    {"wait_event", 1, js_wait_event},
    {"_request_event", 2, js_request_event},
    {"find_config_file", 1, js_find_config_file},
    {"command", 1, js_command},
    {"commandv", 0, js_commandv},
    {"command_native", 2, js_command_native},
    {"_command_native_async", 2, js_command_native_async},
    {"_abort_async_command", 1, js_abort_async},
    {"del_property", 1, js_del_property},
    {"get_property_bool", 2, js_get_property_bool},
    {"get_property_number", 2, js_get_property_number},
    {"get_property_native", 2, js_get_property_native},
    {"get_property", 2, js_get_property},
    {"get_property_osd", 2, js_get_property_osd},
    {"set_property", 2, js_set_property},
    {"set_property_bool", 2, js_set_property_bool},
    {"set_property_number", 2, js_set_property_number},
    {"set_property_native", 2, js_set_property_native},
    {"_observe_property", 3, js_observe_property},
    {"_unobserve_property", 1, js_unobserve_property},
    {"get_time_ms", 0, js_get_time_ms},
    {"format_time", 2, js_format_time},
    {"enable_messages", 1, js_enable_messages},
    {"get_wakeup_pipe", 0, js_get_wakeup_pipe},
    {"_hook_add", 3, js_hook_add},
    {"_hook_continue", 1, js_hook_continue},
    {"input_set_section_mouse_area", 5, js_input_set_section_mouse_area},
    {"last_error", 0, js_last_error},
    {"_set_last_error", 1, js_set_last_error},
    {0},
};

static const struct fn_entry utils_fns[] = {
    {"readdir", 2, js_readdir},
    {"file_info", 1, js_file_info},
    {"split_path", 1, js_split_path},
    {"join_path", 2, js_join_path},
    {"get_env_list", 0, js_get_env_list},
    {"read_file", 2, js_read_file},
    {"_write_file", 3, js_write_file},
    {"getenv", 1, js_getenv},
    {"compile_js", 2, js_compile_js},
    {"_gc", 1, js_gc},
    {0},
};

static void add_package_fns(JSContext *ctx, JSValue obj,
                            const struct fn_entry *e) {
    for (int n = 0; e[n].name; n++) {
        JS_SetPropertyStr(
            ctx, obj, e[n].name,
            JS_NewCFunction(ctx, e[n].fn, e[n].name, e[n].length));
    }
}

static int setup_functions(JSContext *ctx, struct script_ctx *sctx) {
    JSValue global = JS_GetGlobalObject(ctx);

    JSValue mp = JS_NewObject(ctx);

    add_package_fns(ctx, mp, main_fns);

    JS_SetPropertyStr(ctx, mp, "script_name",
                      JS_NewString(ctx, mpv_client_name(sctx->client)));

    JS_SetPropertyStr(ctx, mp, "script_file",
                      JS_NewString(ctx, sctx->filename));

    if (sctx->path) {
        JS_SetPropertyStr(ctx, mp, "script_path",
                          JS_NewString(ctx, sctx->path));
    }

    JSValue utils = JS_NewObject(ctx);
    add_package_fns(ctx, utils, utils_fns);
    // take ownership, no need to free
    JS_SetPropertyStr(ctx, mp, "utils", utils);

    // take ownership, no need to free
    JS_SetPropertyStr(ctx, global, "mp", mp);

    JS_FreeValue(ctx, global);
    return 0;
}

// Script loading ---------------------------------------------------------
static JSValue eval_string(JSContext *ctx, const char *code, const char *name) {
    return JS_Eval(ctx, code, strlen(code), name, JS_EVAL_TYPE_GLOBAL);
}

static int run_script(JSContext *ctx, struct script_ctx *sctx) {
    js_std_add_helpers(ctx, 0, 0);

    if (setup_functions(ctx, sctx)) {
        return -1;
    }

    JSValue r = JS_UNDEFINED;

    const char *def = builtin_files[0][1];
    r = eval_string(ctx, def, "@/defaults.js");
    if (JS_IsException(r)) {
        goto error;
    }
    JS_FreeValue(ctx, r);

    JSValue main_code = read_file_limit(ctx, sctx->filename, -1);
    if (JS_IsException(main_code)) {
        goto error;
    }

    size_t len;
    const char *src = JS_ToCStringLen(ctx, &len, main_code);
    r = JS_Eval(ctx, src, len, sctx->filename, JS_EVAL_TYPE_GLOBAL);
    JS_FreeCString(ctx, src);
    JS_FreeValue(ctx, main_code);
    if (JS_IsException(r)) {
        goto error;
    }
    JS_FreeValue(ctx, r);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "mp_event_loop");
    if (!JS_IsFunction(ctx, fn)) {
        JS_FreeValue(ctx, fn);
        JS_FreeValue(ctx, global);
        return -1;
    }
    r = JS_Call(ctx, fn, global, 0, NULL);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    if (JS_IsException(r))
        goto error;
    JS_FreeValue(ctx, r);
    return 0;

error: {
    JSValue exc = JS_GetException(ctx);
    JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
    const char *str = JS_ToCString(ctx, JS_IsUndefined(stack) ? exc : stack);
    set_last_error(sctx, 1, str ? str : "unknown error");
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, exc);
}
    return -1;
}

static int load_quickjs(struct mp_script_args *args) {
    struct script_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct script_ctx){
        .client = args->client,
        .mpctx = args->mpctx,
        .log = args->log,
        .last_error_str = talloc_strdup(NULL, "Cannot initialize JavaScript"),
        .filename = args->filename,
        .path = args->path,
        .stats = stats_ctx_create(
            ctx, args->mpctx->global,
            mp_tprintf(80, "script/%s", mpv_client_name(args->client))),
    };
    stats_register_thread_cputime(ctx->stats, "cpu");

    ctx->rt = JS_NewRuntime();
    if (!ctx->rt)
        goto error_out;
    ctx->qctx = JS_NewContext(ctx->rt);
    if (!ctx->qctx)
        goto error_out;

    JS_SetContextOpaque(ctx->qctx, ctx);

    set_last_error(ctx, 0, NULL);

    if (run_script(ctx->qctx, ctx)) {
        goto error_out;
    }

    JS_FreeContext(ctx->qctx);
    JS_FreeRuntime(ctx->rt);
    talloc_free(ctx->last_error_str);
    talloc_free(ctx);
    return 0;

error_out:
    MP_FATAL(ctx, "%s\n", ctx->last_error_str);
    if (ctx->qctx)
        JS_FreeContext(ctx->qctx);
    if (ctx->rt)
        JS_FreeRuntime(ctx->rt);
    talloc_free(ctx->last_error_str);
    talloc_free(ctx);
    return -1;
}

// main export of this file, used by cplayer to load js scripts
const struct mp_scripting mp_scripting_js = {
    .name = "js",
    .file_ext = "js",
    .load = load_quickjs,
};
