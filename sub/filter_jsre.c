#include <stdio.h>
#include <sys/types.h>

#include <mujs.h>

#include "common/common.h"
#include "common/msg.h"
#include "misc/bstr.h"
#include "options/options.h"
#include "sd.h"


// p_NAME are protected functions (never throw) which interact with the JS VM.
// return 0 on successful interaction, not-0 on (caught) js-error.
// on error: stack is the same as on entry + an error value

// js: global[n] = new RegExp(str, flags)
static int p_regcomp(js_State *J, int n, const char *str, int flags)
{
    if (js_try(J))
        return 1;

    js_pushnumber(J, n);  // n
    js_newregexp(J, str, flags);  // n regex
    js_setglobal(J, js_tostring(J, -2));  // n  (and global[n] is the regex)
    js_pop(J, 1);

    js_endtry(J);
    return 0;
}

// js: found = global[n].test(text)
static int p_regexec(js_State *J, int n, const char *text, int *found)
{
    if (js_try(J))
        return 1;

    js_pushnumber(J, n);  // n
    js_getglobal(J, js_tostring(J, -1));  // n global[n]
    js_getproperty(J, -1, "test");  // n global[n] global[n].test
    js_rot2(J);  // n global[n].test global[n]   (n, test(), and its `this')
    js_pushstring(J, text); // n global[n].test global[n] text
    js_call(J, 1);  // n test-result
    *found = js_toboolean(J, -1);
    js_pop(J, 2);  // the result and n

    js_endtry(J);
    return 0;
}

// protected. caller should pop the error after using the result string.
static const char *get_err(js_State *J)
{
    return js_trystring(J, -1, "unknown error");
}


struct priv {
    js_State *J;
    int num_regexes;
    int offset;
};

static void destruct_priv(void *p)
{
    js_freestate(((struct priv *)p)->J);
}

static bool jsre_init(struct sd_filter *ft)
{
    if (strcmp(ft->codec, "ass") != 0)
        return false;

    if (!ft->opts->rf_enable)
        return false;

    if (!(ft->opts->jsre_items && ft->opts->jsre_items[0]))
        return false;

    struct priv *p = talloc_zero(ft, struct priv);
    ft->priv = p;

    p->J = js_newstate(0, 0, JS_STRICT);
    if (!p->J) {
        MP_ERR(ft, "jsre: VM init error\n");
        return false;
    }
    talloc_set_destructor(p, destruct_priv);

    for (int n = 0; ft->opts->jsre_items[n]; n++) {
        char *item = ft->opts->jsre_items[n];

        int err = p_regcomp(p->J, p->num_regexes, item, JS_REGEXP_I | JS_REGEXP_M);
        if (err) {
            MP_ERR(ft, "jsre: %s -- '%s'\n", get_err(p->J), item);
            js_pop(p->J, 1);
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
                                      struct demux_packet *pkt)
{
    struct priv *p = ft->priv;
    char *text = bstrto0(NULL, sd_ass_pkt_text(ft, pkt, p->offset));
    bool drop = false;

    if (ft->opts->rf_plain)
        sd_ass_to_plaintext(text, strlen(text), text);

    for (int n = 0; n < p->num_regexes; n++) {
        int found, err = p_regexec(p->J, n, text, &found);
        if (err == 0 && found) {
            int level = ft->opts->rf_warn ? MSGL_WARN : MSGL_V;
            MP_MSG(ft, level, "jsre: regex %d => drop: '%s'\n", n, text);
            drop = true;
            break;
        } else if (err) {
            MP_WARN(ft, "jsre: test regex %d: %s.\n", n, get_err(p->J));
            js_pop(p->J, 1);
        }
    }

    talloc_free(text);
    return drop ? NULL : pkt;
}

const struct sd_filter_functions sd_filter_jsre = {
    .init   = jsre_init,
    .filter = jsre_filter,
};
