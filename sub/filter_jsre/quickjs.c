#include <string.h>
#include <sys/types.h>

#include <quickjs.h>

#include "../common/common.h"
#include "../common/msg.h"
#include "../misc/bstr.h"
#include "../options/options.h"
#include "../sd.h"

struct priv {
    JSRuntime *rt;
    JSContext *ctx;
    JSValue regexes;
    JSValue test_fn;
    int num_regexes;
    int offset;
};

static void destruct_priv(void *p) {
    struct priv *priv = p;

    if (priv->ctx && !JS_IsUndefined(priv->regexes))
        JS_FreeValue(priv->ctx, priv->regexes);
    if (priv->ctx && !JS_IsUndefined(priv->test_fn))
        JS_FreeValue(priv->ctx, priv->test_fn);
    if (priv->ctx)
        JS_FreeContext(priv->ctx);
    if (priv->rt)
        JS_FreeRuntime(priv->rt);
}

static char *get_err(struct sd_filter *ft, JSContext *ctx) {
    JSValue exc = JS_GetException(ctx);
    JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
    JSValue msg_val = JS_IsUndefined(stack) ? exc : stack;
    const char *str = JS_ToCString(ctx, msg_val);
    char *msg = talloc_strdup(ft, str ? str : "unknown error");

    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, exc);
    return msg;
}

static int test_any(struct priv *p, const char *text, int *match_index) {
    JSContext *ctx = p->ctx;

    JSValue args[2] = {JS_DupValue(ctx, p->regexes), JS_NewString(ctx, text)};
    if (JS_IsException(args[0]) || JS_IsException(args[1])) {
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        return 1;
    }

    JSValue ret =
        JS_Call(ctx, p->test_fn, JS_UNDEFINED, MP_ARRAY_SIZE(args), args);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, args[0]);
    if (JS_IsException(ret))
        return 1;

    int idx = -1;
    if (JS_ToInt32(ctx, &idx, ret) < 0) {
        JS_FreeValue(ctx, ret);
        return 1;
    }
    JS_FreeValue(ctx, ret);

    *match_index = idx;
    return 0;
}

static bool jsre_init(struct sd_filter *ft) {
    if (strcmp(ft->codec, "ass") != 0)
        return false;

    if (!ft->opts->rf_enable)
        return false;

    if (!(ft->opts->jsre_items && ft->opts->jsre_items[0]))
        return false;

    struct priv *p = talloc_zero(ft, struct priv);
    p->regexes = JS_UNDEFINED;
    p->test_fn = JS_UNDEFINED;
    ft->priv = p;
    talloc_set_destructor(p, destruct_priv);

    p->rt = JS_NewRuntime();
    if (!p->rt) {
        MP_ERR(ft, "jsre: VM init error\n");
        return false;
    }

    p->ctx = JS_NewContext(p->rt);
    if (!p->ctx) {
        MP_ERR(ft, "jsre: VM init error\n");
        return false;
    }

    const char *compile_src =
        "(function (arr) {"
        "  if (!Array.isArray(arr)) return [];"
        "  return arr.map(function (s) { return new RegExp(s, 'im'); });"
        "})";
    JSValue compile_fn =
        JS_Eval(p->ctx, compile_src, strlen(compile_src), "<jsre>",
                JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);
    if (JS_IsException(compile_fn) || !JS_IsFunction(p->ctx, compile_fn)) {
        char *msg = get_err(ft, p->ctx);
        MP_ERR(ft, "jsre: %s\n", msg);
        talloc_free(msg);
        JS_FreeValue(p->ctx, compile_fn);
        return false;
    }

    const char *test_src =
        "(function (regs, t) {"
        "  if (!Array.isArray(regs)) return -1;"
        "  for (let i = 0; i < regs.length; i++) {"
        "    const r = regs[i];"
        "    if (r && typeof r.test === 'function' && r.test(t)) return i;"
        "  }"
        "  return -1;"
        "})";
    p->test_fn = JS_Eval(p->ctx, test_src, strlen(test_src), "<jsre>",
                         JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);
    if (JS_IsException(p->test_fn) || !JS_IsFunction(p->ctx, p->test_fn)) {
        char *msg = get_err(ft, p->ctx);
        MP_ERR(ft, "jsre: %s\n", msg);
        talloc_free(msg);
        return false;
    }

    JSValue patterns = JS_NewArray(p->ctx);
    if (JS_IsException(patterns)) {
        char *msg = get_err(ft, p->ctx);
        MP_ERR(ft, "jsre: %s\n", msg);
        talloc_free(msg);
        return false;
    }

    for (int n = 0; ft->opts->jsre_items[n]; n++) {
        char *item = ft->opts->jsre_items[n];

        JSValue pat = JS_NewString(p->ctx, item);
        if (JS_IsException(pat)) {
            JS_FreeValue(p->ctx, patterns);
            char *msg = get_err(ft, p->ctx);
            MP_ERR(ft, "jsre: %s -- '%s'\n", msg, item);
            talloc_free(msg);
            return false;
        }

        if (JS_SetPropertyUint32(p->ctx, patterns, n, pat) != 0) {
            JS_FreeValue(p->ctx, pat);
            JS_FreeValue(p->ctx, patterns);
            char *msg = get_err(ft, p->ctx);
            MP_ERR(ft, "jsre: %s -- '%s'\n", msg, item);
            talloc_free(msg);
            return false;
        }

        p->num_regexes += 1;
    }

    if (!p->num_regexes) {
        JS_FreeValue(p->ctx, patterns);
        return false;
    }

    JSValue compiled = JS_Call(p->ctx, compile_fn, JS_UNDEFINED, 1, &patterns);
    JS_FreeValue(p->ctx, compile_fn);
    JS_FreeValue(p->ctx, patterns);
    if (JS_IsException(compiled)) {
        char *msg = get_err(ft, p->ctx);
        MP_ERR(ft, "jsre: %s\n", msg);
        talloc_free(msg);
        return false;
    }

    p->regexes = compiled;

    p->offset = sd_ass_fmt_offset(ft->event_format);
    return true;
}

static struct demux_packet *jsre_filter(struct sd_filter *ft,
                                        struct demux_packet *pkt) {

    struct priv *p = ft->priv;
    char *text = bstrto0(NULL, sd_ass_pkt_text(ft, pkt, p->offset));
    bool drop = false;

    if (ft->opts->rf_plain)
        sd_ass_to_plaintext(&text, text);

    int match_index = -1;
    int err = test_any(p, text, &match_index);
    if (err == 0 && match_index >= 0) {
        int level = ft->opts->rf_warn ? MSGL_WARN : MSGL_V;
        MP_MSG(ft, level, "jsre: regex %d => drop: '%s'\n", match_index, text);
        drop = true;
    } else if (err) {
        char *msg = get_err(ft, p->ctx);
        MP_WARN(ft, "jsre: test regex: %s.\n", msg);
        talloc_free(msg);
    }

    return drop ? NULL : pkt;
}

const struct sd_filter_functions sd_filter_jsre = {
    .init = jsre_init,
    .filter = jsre_filter,
};
