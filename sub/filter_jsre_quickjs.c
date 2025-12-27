#include <sys/types.h>

#include <quickjs.h>

#include "common/common.h"
#include "common/msg.h"
#include "misc/bstr.h"
#include "options/options.h"
#include "sd.h"

struct priv {
  JSRuntime *rt;
  JSContext *ctx;
  JSValue regexes;
  int num_regexes;
  int offset;
};

static void destruct_priv(void *p) {
  struct priv *priv = p;

  if (priv->ctx && !JS_IsUndefined(priv->regexes))
    JS_FreeValue(priv->ctx, priv->regexes);
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

static int p_regcomp(JSContext *ctx, JSValue regexes, int n, const char *str) {
  JSValue global = JS_GetGlobalObject(ctx);
  if (JS_IsException(global))
    return 1;

  JSValue ctor = JS_GetPropertyStr(ctx, global, "RegExp");
  JS_FreeValue(ctx, global);
  if (JS_IsException(ctor))
    return 1;

  JSValue args[2] = {JS_NewString(ctx, str), JS_NewString(ctx, "im")};
  if (JS_IsException(args[0]) || JS_IsException(args[1])) {
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, ctor);
    return 1;
  }

  JSValue regex = JS_CallConstructor(ctx, ctor, MP_ARRAY_SIZE(args), args);
  JS_FreeValue(ctx, ctor);
  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  if (JS_IsException(regex))
    return 1;

  if (JS_SetPropertyUint32(ctx, regexes, n, regex) < 0)
    return 1;

  return 0;
}

static int p_regexec(JSContext *ctx, JSValueConst regexes, int n,
                     const char *text, int *found) {
  JSValue regex = JS_GetPropertyUint32(ctx, regexes, n);
  if (JS_IsException(regex))
    return 1;
  if (JS_IsUndefined(regex)) {
    JS_FreeValue(ctx, regex);
    *found = 0;
    return 0;
  }

  JSValue test = JS_GetPropertyStr(ctx, regex, "test");
  if (JS_IsException(test)) {
    JS_FreeValue(ctx, regex);
    return 1;
  }
  if (!JS_IsFunction(ctx, test)) {
    JS_FreeValue(ctx, test);
    JS_FreeValue(ctx, regex);
    return 1;
  }

  JSValue args[1] = {JS_NewString(ctx, text)};
  if (JS_IsException(args[0])) {
    JS_FreeValue(ctx, test);
    JS_FreeValue(ctx, regex);
    return 1;
  }

  JSValue ret = JS_Call(ctx, test, regex, MP_ARRAY_SIZE(args), args);
  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, test);
  JS_FreeValue(ctx, regex);
  if (JS_IsException(ret))
    return 1;

  int res = JS_ToBool(ctx, ret);
  JS_FreeValue(ctx, ret);
  if (res < 0)
    return 1;

  *found = res;
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

  p->regexes = JS_NewArray(p->ctx);
  if (JS_IsException(p->regexes)) {
    char *msg = get_err(ft, p->ctx);
    MP_ERR(ft, "jsre: %s\n", msg);
    talloc_free(msg);
    return false;
  }

  for (int n = 0; ft->opts->jsre_items[n]; n++) {
    char *item = ft->opts->jsre_items[n];

    int err = p_regcomp(p->ctx, p->regexes, p->num_regexes, item);
    if (err) {
      char *msg = get_err(ft, p->ctx);
      MP_ERR(ft, "jsre: %s -- '%s'\n", msg, item);
      talloc_free(msg);
      continue;
    }

    p->num_regexes += 1;
  }

  if (!p->num_regexes)
    return false;

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

  for (int n = 0; n < p->num_regexes; n++) {
    int found, err = p_regexec(p->ctx, p->regexes, n, text, &found);
    if (err == 0 && found) {
      int level = ft->opts->rf_warn ? MSGL_WARN : MSGL_V;
      MP_MSG(ft, level, "jsre: regex %d => drop: '%s'\n", n, text);
      drop = true;
      break;
    } else if (err) {
      char *msg = get_err(ft, p->ctx);
      MP_WARN(ft, "jsre: test regex %d: %s.\n", n, msg);
      talloc_free(msg);
    }
  }

  talloc_free(text);
  return drop ? NULL : pkt;
}

const struct sd_filter_functions sd_filter_jsre = {
    .init = jsre_init,
    .filter = jsre_filter,
};
