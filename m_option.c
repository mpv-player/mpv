/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/// \file
/// \ingroup Options

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>
#include <assert.h>

#include "talloc.h"
#include "m_option.h"
#include "mp_msg.h"
#include "stream/url.h"
#include "libavutil/avstring.h"

char *m_option_strerror(int code)
{
    switch (code) {
    case M_OPT_UNKNOWN:
        return mp_gtext("Unrecognized option name");
    case M_OPT_MISSING_PARAM:
        return mp_gtext("Required parameter for option missing");
    case M_OPT_INVALID:
        return mp_gtext("Option parameter could not be parsed");
    case M_OPT_OUT_OF_RANGE:
        return mp_gtext("Parameter is outside values allowed for option");
    case M_OPT_PARSER_ERR:
        return mp_gtext("Parser error");
    default:
        return NULL;
    }
}

static const struct m_option *m_option_list_findb(const struct m_option *list,
                                                  struct bstr name)
{
    for (int i = 0; list[i].name; i++) {
        struct bstr lname = bstr(list[i].name);
        if ((list[i].type->flags & M_OPT_TYPE_ALLOW_WILDCARD)
                && bstr_endswith0(lname, "*")) {
            lname.len--;
            if (bstrcasecmp(bstr_splice(name, 0, lname.len), lname) == 0)
                return &list[i];
        } else if (bstrcasecmp(lname, name) == 0)
            return &list[i];
    }
    return NULL;
}

const m_option_t *m_option_list_find(const m_option_t *list, const char *name)
{
    return m_option_list_findb(list, bstr(name));
}

// Default function that just does a memcpy

static void copy_opt(const m_option_t *opt, void *dst, const void *src)
{
    if (dst && src)
        memcpy(dst, src, opt->type->size);
}

// Flag

#define VAL(x) (*(int *)(x))

static int parse_flag(const m_option_t *opt, struct bstr name,
                      struct bstr param, bool ambiguous_param, void *dst)
{
    if (param.len && !ambiguous_param) {
        char * const enable[] = { "yes", "on", "ja", "si", "igen", "y", "j",
                                  "i", "tak", "ja", "true", "1" };
        for (int i = 0; i < sizeof(enable) / sizeof(enable[0]); i++) {
            if (!bstrcasecmp0(param, enable[i])) {
                if (dst)
                    VAL(dst) = opt->max;
                return 1;
            }
        }
        char * const disable[] = { "no", "off", "nein", "nicht", "nem", "n",
                                   "nie", "nej", "false", "0" };
        for (int i = 0; i < sizeof(disable) / sizeof(disable[0]); i++) {
            if (!bstrcasecmp0(param, disable[i])) {
                if (dst)
                    VAL(dst) = opt->min;
                return 1;
            }
        }
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Invalid parameter for %.*s flag: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    } else {
        if (dst)
            VAL(dst) = opt->max;
        return 0;
    }
}

static char *print_flag(const m_option_t *opt, const void *val)
{
    if (VAL(val) == opt->min)
        return talloc_strdup(NULL, "no");
    else
        return talloc_strdup(NULL, "yes");
}

const m_option_type_t m_option_type_flag = {
    "Flag",
    "need yes or no in config files",
    sizeof(int),
    0,
    parse_flag,
    print_flag,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};

// Integer

static int parse_longlong(const m_option_t *opt, struct bstr name,
                          struct bstr param, bool ambiguous_param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct bstr rest;
    long long tmp_int = bstrtoll(param, &rest, 10);
    if (rest.len)
        tmp_int = bstrtoll(param, &rest, 0);
    if (rest.len) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "The %.*s option must be an integer: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    if ((opt->flags & M_OPT_MIN) && (tmp_int < opt->min)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "The %.*s option must be >= %d: %.*s\n",
               BSTR_P(name), (int) opt->min, BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    if ((opt->flags & M_OPT_MAX) && (tmp_int > opt->max)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "The %.*s option must be <= %d: %.*s\n",
               BSTR_P(name), (int) opt->max, BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    if (dst)
        *(long long *)dst = tmp_int;

    return 1;
}

static int parse_int(const m_option_t *opt, struct bstr name,
                     struct bstr param, bool ambiguous_param, void *dst)
{
    long long tmp;
    int r = parse_longlong(opt, name, param, false, &tmp);
    if (r >= 0 && dst)
        *(int *)dst = tmp;
    return r;
}

static int parse_int64(const m_option_t *opt, struct bstr name,
                       struct bstr param, bool ambiguous_param, void *dst)
{
    long long tmp;
    int r = parse_longlong(opt, name, param, false, &tmp);
    if (r >= 0 && dst)
        *(int64_t *)dst = tmp;
    return r;
}


static char *print_int(const m_option_t *opt, const void *val)
{
    if (opt->type->size == sizeof(int64_t))
        return talloc_asprintf(NULL, "%"PRId64, *(const int64_t *)val);
    return talloc_asprintf(NULL, "%d", VAL(val));
}

const m_option_type_t m_option_type_int = {
    "Integer",
    "",
    sizeof(int),
    0,
    parse_int,
    print_int,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};

const m_option_type_t m_option_type_int64 = {
    "Integer64",
    "",
    sizeof(int64_t),
    0,
    parse_int64,
    print_int,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};

static int parse_intpair(const struct m_option *opt, struct bstr name,
                         struct bstr param, bool ambiguous_param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct bstr s = param;
    int end = -1;
    int start = bstrtoll(s, &s, 10);
    if (s.len == param.len)
        goto bad;
    if (s.len > 0) {
        if (!bstr_startswith0(s, "-"))
            goto bad;
        s = bstr_cut(s, 1);
    }
    if (s.len > 0)
        end = bstrtoll(s, &s, 10);
    if (s.len > 0)
        goto bad;

    if (dst) {
        int *p = dst;
        p[0] = start;
        p[1] = end;
    }

    return 1;

bad:
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Invalid integer range "
           "specification for option %.*s: %.*s\n",
           BSTR_P(name), BSTR_P(param));
    return M_OPT_INVALID;
}

const struct m_option_type m_option_type_intpair = {
    .name = "Int[-Int]",
    .size = sizeof(int[2]),
    .parse = parse_intpair,
    .save = copy_opt,
    .set = copy_opt,
};

static int parse_choice(const struct m_option *opt, struct bstr name,
                        struct bstr param, bool ambiguous_param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct m_opt_choice_alternatives *alt;
    for (alt = opt->priv; alt->name; alt++)
        if (!bstrcasecmp0(param, alt->name))
            break;
    if (!alt->name) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Invalid value for option %.*s: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Valid values are:");
        for (alt = opt->priv; alt->name; alt++)
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, " %s", alt->name);
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "\n");
        return M_OPT_INVALID;
    }
    if (dst)
        *(int *)dst = alt->value;

    return 1;
}

static char *print_choice(const m_option_t *opt, const void *val)
{
    int v = *(int *)val;
    struct m_opt_choice_alternatives *alt;
    for (alt = opt->priv; alt->name; alt++)
        if (alt->value == v)
            return talloc_strdup(NULL, alt->name);
    abort();
}

const struct m_option_type m_option_type_choice = {
    .name = "String",  // same as arbitrary strings in option list for now
    .size = sizeof(int),
    .parse = parse_choice,
    .print = print_choice,
    .save = copy_opt,
    .set = copy_opt,
};

// Float

#undef VAL
#define VAL(x) (*(double *)(x))

static int parse_double(const m_option_t *opt, struct bstr name,
                        struct bstr param, bool ambiguous_param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct bstr rest;
    double tmp_float = bstrtod(param, &rest);

    switch (rest.len ? rest.start[0] : 0) {
    case ':':
    case '/':
        tmp_float /= bstrtod(bstr_cut(rest, 1), &rest);
        break;
    case '.':
    case ',':
        /* we also handle floats specified with
         * non-locale decimal point ::atmos
         */
        rest = bstr_cut(rest, 1);
        if (tmp_float < 0)
            tmp_float -= 1.0 / pow(10, rest.len) * bstrtod(rest, &rest);
        else
            tmp_float += 1.0 / pow(10, rest.len) * bstrtod(rest, &rest);
        break;
    }

    if (rest.len) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "The %.*s option must be a floating point number or a "
               "ratio (numerator[:/]denominator): %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    if (opt->flags & M_OPT_MIN)
        if (tmp_float < opt->min) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "The %.*s option must be >= %f: %.*s\n",
                   BSTR_P(name), opt->min, BSTR_P(param));
            return M_OPT_OUT_OF_RANGE;
        }

    if (opt->flags & M_OPT_MAX)
        if (tmp_float > opt->max) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "The %.*s option must be <= %f: %.*s\n",
                   BSTR_P(name), opt->max, BSTR_P(param));
            return M_OPT_OUT_OF_RANGE;
        }

    if (dst)
        VAL(dst) = tmp_float;
    return 1;
}

static char *print_double(const m_option_t *opt, const void *val)
{
    opt = NULL;
    return talloc_asprintf(NULL, "%f", VAL(val));
}

const m_option_type_t m_option_type_double = {
    "Double",
    "double precision floating point number or ratio (numerator[:/]denominator)",
    sizeof(double),
    0,
    parse_double,
    print_double,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};

#undef VAL
#define VAL(x) (*(float *)(x))

static int parse_float(const m_option_t *opt, struct bstr name,
                       struct bstr param, bool ambiguous_param, void *dst)
{
    double tmp;
    int r = parse_double(opt, name, param, false, &tmp);
    if (r == 1 && dst)
        VAL(dst) = tmp;
    return r;
}

static char *print_float(const m_option_t *opt, const void *val)
{
    opt = NULL;
    return talloc_asprintf(NULL, "%f", VAL(val));
}

const m_option_type_t m_option_type_float = {
    "Float",
    "floating point number or ratio (numerator[:/]denominator)",
    sizeof(float),
    0,
    parse_float,
    print_float,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};

///////////// Position
#undef VAL
#define VAL(x) (*(off_t *)(x))

static int parse_position(const m_option_t *opt, struct bstr name,
                          struct bstr param, bool ambiguous_param, void *dst)
{
    long long tmp;
    int r = parse_longlong(opt, name, param, false, &tmp);
    if (r >= 0 && dst)
        *(off_t *)dst = tmp;
    return r;
}

static char *print_position(const m_option_t *opt, const void *val)
{
    return talloc_asprintf(NULL, "%"PRId64, (int64_t)VAL(val));
}

const m_option_type_t m_option_type_position = {
    "Position",
    "Integer (off_t)",
    sizeof(off_t),
    0,
    parse_position,
    print_position,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};


///////////// String

#undef VAL
#define VAL(x) (*(char **)(x))

static int parse_str(const m_option_t *opt, struct bstr name,
                     struct bstr param, bool ambiguous_param, void *dst)
{
    if ((opt->flags & M_OPT_MIN) && (param.len < opt->min)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Parameter must be >= %d chars: %.*s\n",
               (int) opt->min, BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    if ((opt->flags & M_OPT_MAX) && (param.len > opt->max)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Parameter must be <= %d chars: %.*s\n",
               (int) opt->max, BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    if (dst) {
        talloc_free(VAL(dst));
        VAL(dst) = bstrdup0(NULL, param);
    }

    return 1;

}

static char *print_str(const m_option_t *opt, const void *val)
{
    return (val && VAL(val)) ? talloc_strdup(NULL, VAL(val)) : NULL;
}

static void copy_str(const m_option_t *opt, void *dst, const void *src)
{
    if (dst && src) {
        talloc_free(VAL(dst));
        VAL(dst) = talloc_strdup(NULL, VAL(src));
    }
}

static void free_str(void *src)
{
    if (src && VAL(src)) {
        talloc_free(VAL(src));
        VAL(src) = NULL;
    }
}

const m_option_type_t m_option_type_string = {
    "String",
    "",
    sizeof(char *),
    M_OPT_TYPE_DYNAMIC,
    parse_str,
    print_str,
    copy_str,
    copy_str,
    copy_str,
    free_str
};

//////////// String list

#undef VAL
#define VAL(x) (*(char ***)(x))

#define OP_NONE 0
#define OP_ADD 1
#define OP_PRE 2
#define OP_DEL 3
#define OP_CLR 4

static void free_str_list(void *dst)
{
    char **d;
    int i;

    if (!dst || !VAL(dst))
        return;
    d = VAL(dst);

    for (i = 0; d[i] != NULL; i++)
        talloc_free(d[i]);
    talloc_free(d);
    VAL(dst) = NULL;
}

static int str_list_add(char **add, int n, void *dst, int pre)
{
    char **lst = VAL(dst);
    int ln;

    if (!dst)
        return M_OPT_PARSER_ERR;
    lst = VAL(dst);

    for (ln = 0; lst && lst[ln]; ln++)
        /**/;

    lst = talloc_realloc(NULL, lst, char *, n + ln + 1);

    if (pre) {
        memmove(&lst[n], lst, ln * sizeof(char *));
        memcpy(lst, add, n * sizeof(char *));
    } else
        memcpy(&lst[ln], add, n * sizeof(char *));
    // (re-)add NULL-termination
    lst[ln + n] = NULL;

    talloc_free(add);

    VAL(dst) = lst;

    return 1;
}

static int str_list_del(char **del, int n, void *dst)
{
    char **lst, *ep;
    int i, ln, s;
    long idx;

    if (!dst)
        return M_OPT_PARSER_ERR;
    lst = VAL(dst);

    for (ln = 0; lst && lst[ln]; ln++)
        /**/;
    s = ln;

    for (i = 0; del[i] != NULL; i++) {
        idx = strtol(del[i], &ep, 0);
        if (*ep) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Invalid index: %s\n", del[i]);
            talloc_free(del[i]);
            continue;
        }
        talloc_free(del[i]);
        if (idx < 0 || idx >= ln) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Index %ld is out of range.\n", idx);
            continue;
        } else if (!lst[idx])
            continue;
        talloc_free(lst[idx]);
        lst[idx] = NULL;
        s--;
    }
    talloc_free(del);

    if (s == 0) {
        talloc_free(lst);
        VAL(dst) = NULL;
        return 1;
    }

    // Don't bother shrinking the list allocation
    for (i = 0, n = 0; i < ln; i++) {
        if (!lst[i])
            continue;
        lst[n] = lst[i];
        n++;
    }
    lst[s] = NULL;

    return 1;
}

static struct bstr get_nextsep(struct bstr *ptr, char sep, bool modify)
{
    struct bstr str = *ptr;
    struct bstr orig = str;
    for (;;) {
        int idx = bstrchr(str, sep);
        if (idx > 0 && str.start[idx - 1] == '\\') {
            if (modify) {
                memmove(str.start + idx - 1, str.start + idx, str.len - idx);
                str.len--;
                str = bstr_cut(str, idx);
            } else
                str = bstr_cut(str, idx + 1);
        } else {
            str = bstr_cut(str, idx < 0 ? str.len : idx);
            break;
        }
    }
    *ptr = str;
    return bstr_splice(orig, 0, str.start - orig.start);
}

static int parse_str_list(const m_option_t *opt, struct bstr name,
                          struct bstr param, bool ambiguous_param, void *dst)
{
    char **res;
    int op = OP_NONE;
    int len = strlen(opt->name);
    if (opt->name[len - 1] == '*' && (name.len > len - 1)) {
        struct bstr suffix = bstr_cut(name, len - 1);
        if (bstrcasecmp0(suffix, "-add") == 0)
            op = OP_ADD;
        else if (bstrcasecmp0(suffix, "-pre") == 0)
            op = OP_PRE;
        else if (bstrcasecmp0(suffix, "-del") == 0)
            op = OP_DEL;
        else if (bstrcasecmp0(suffix, "-clr") == 0)
            op = OP_CLR;
        else
            return M_OPT_UNKNOWN;
    }

    // Clear the list ??
    if (op == OP_CLR) {
        if (dst)
            free_str_list(dst);
        return 0;
    }

    // All other ops need a param
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    // custom type for "profile" calls this but uses ->priv for something else
    char separator = opt->type == &m_option_type_string_list && opt->priv ?
                     *(char *)opt->priv : OPTION_LIST_SEPARATOR;
    int n = 0;
    struct bstr str = param;
    while (str.len) {
        get_nextsep(&str, separator, 0);
        str = bstr_cut(str, 1);
        n++;
    }
    if (n == 0)
        return M_OPT_INVALID;
    if (((opt->flags & M_OPT_MIN) && (n < opt->min)) ||
        ((opt->flags & M_OPT_MAX) && (n > opt->max)))
        return M_OPT_OUT_OF_RANGE;

    if (!dst)
        return 1;

    res = talloc_array(NULL, char *, n + 2);
    str = bstrdup(NULL, param);
    char *ptr = str.start;
    n = 0;

    while (1) {
        struct bstr el = get_nextsep(&str, separator, 1);
        res[n] = bstrdup0(NULL, el);
        n++;
        if (!str.len)
            break;
        str = bstr_cut(str, 1);
    }
    res[n] = NULL;
    talloc_free(ptr);

    switch (op) {
    case OP_ADD:
        return str_list_add(res, n, dst, 0);
    case OP_PRE:
        return str_list_add(res, n, dst, 1);
    case OP_DEL:
        return str_list_del(res, n, dst);
    }

    if (VAL(dst))
        free_str_list(dst);
    VAL(dst) = res;

    return 1;
}

static void copy_str_list(const m_option_t *opt, void *dst, const void *src)
{
    int n;
    char **d, **s;

    if (!(dst && src))
        return;
    s = VAL(src);

    if (VAL(dst))
        free_str_list(dst);

    if (!s) {
        VAL(dst) = NULL;
        return;
    }

    for (n = 0; s[n] != NULL; n++)
        /* NOTHING */;
    d = talloc_array(NULL, char *, n + 1);
    for (; n >= 0; n--)
        d[n] = talloc_strdup(NULL, s[n]);

    VAL(dst) = d;
}

static char *print_str_list(const m_option_t *opt, const void *src)
{
    char **lst = NULL;
    char *ret = NULL;

    if (!(src && VAL(src)))
        return NULL;
    lst = VAL(src);

    for (int i = 0; lst[i]; i++) {
        if (ret)
            ret = talloc_strdup_append_buffer(ret, ",");
        ret = talloc_strdup_append_buffer(ret, lst[i]);
    }
    return ret;
}

const m_option_type_t m_option_type_string_list = {
    "String list",
    "A list of strings separated by ','\n"
    "Option with a name ending in an * permits using the following suffix: \n"
    "\t-add: Add the given parameters at the end of the list.\n"
    "\t-pre: Add the given parameters at the beginning of the list.\n"
    "\t-del: Remove the entry at the given indices.\n"
    "\t-clr: Clear the list.\n"
    "e.g: -vf-add flip,mirror -vf-del 2,5\n",
    sizeof(char **),
    M_OPT_TYPE_DYNAMIC | M_OPT_TYPE_ALLOW_WILDCARD,
    parse_str_list,
    print_str_list,
    copy_str_list,
    copy_str_list,
    copy_str_list,
    free_str_list
};


///////////////////  Func based options

// A chained list to save the various calls for func_param
struct m_func_save {
    struct m_func_save *next;
    char *name;
    char *param;
};

#undef VAL
#define VAL(x) (*(struct m_func_save **)(x))

static void free_func_pf(void *src)
{
    struct m_func_save *s, *n;

    if (!src)
        return;

    s = VAL(src);

    while (s) {
        n = s->next;
        talloc_free(s->name);
        talloc_free(s->param);
        talloc_free(s);
        s = n;
    }
    VAL(src) = NULL;
}

// Parser for func_param
static int parse_func_pf(const m_option_t *opt, struct bstr name,
                         struct bstr param, bool ambiguous_param, void *dst)
{
    struct m_func_save *s, *p;

    if (!dst)
        return 1;

    s = talloc_zero(NULL, struct m_func_save);
    s->name = bstrdup0(NULL, name);
    s->param = bstrdup0(NULL, param);

    p = VAL(dst);
    if (p) {
        for (; p->next != NULL; p = p->next)
            /**/;
        p->next = s;
    } else
        VAL(dst) = s;

    return 1;
}

static void copy_func_pf(const m_option_t *opt, void *dst, const void *src)
{
    struct m_func_save *d = NULL, *s, *last = NULL;

    if (!(dst && src))
        return;
    s = VAL(src);

    if (VAL(dst))
        free_func_pf(dst);

    while (s) {
        d = talloc_zero(NULL, struct m_func_save);
        d->name = talloc_strdup(NULL, s->name);
        d->param = talloc_strdup(NULL, s->param);
        if (last)
            last->next = d;
        else
            VAL(dst) = d;
        last = d;
        s = s->next;
    }


}

/////////////////// Func_param

static void set_func_param(const m_option_t *opt, void *dst, const void *src)
{
    struct m_func_save *s;

    if (!src)
        return;
    s = VAL(src);

    if (!s)
        return;

    for (; s != NULL; s = s->next)
        ((m_opt_func_param_t) opt->p)(opt, s->param);
}

const m_option_type_t m_option_type_func_param = {
    "Func param",
    "",
    sizeof(struct m_func_save *),
    M_OPT_TYPE_INDIRECT,
    parse_func_pf,
    NULL,
    NULL, // Nothing to do on save
    set_func_param,
    copy_func_pf,
    free_func_pf
};

/////////////// Func

#undef VAL

static int parse_func(const m_option_t *opt, struct bstr name,
                      struct bstr param, bool ambiguous_param, void *dst)
{
    return 0;
}

static void set_func(const m_option_t *opt, void *dst, const void *src)
{
    ((m_opt_func_t) opt->p)(opt);
}

const m_option_type_t m_option_type_func = {
    "Func",
    "",
    sizeof(int),
    M_OPT_TYPE_INDIRECT,
    parse_func,
    NULL,
    NULL, // Nothing to do on save
    set_func,
    NULL,
    NULL
};

/////////////////// Print

static int parse_print(const m_option_t *opt, struct bstr name,
                       struct bstr param, bool ambiguous_param, void *dst)
{
    if (opt->type == CONF_TYPE_PRINT_INDIRECT)
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "%s", *(char **) opt->p);
    else if (opt->type == CONF_TYPE_PRINT_FUNC) {
        char *name0 = bstrdup0(NULL, name);
        char *param0 = bstrdup0(NULL, param);
        int r = ((m_opt_func_full_t) opt->p)(opt, name0, param0);
        talloc_free(name0);
        talloc_free(param0);
        return r;
    } else
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "%s", mp_gtext(opt->p));

    if (opt->priv == NULL)
        return M_OPT_EXIT;
    return 0;
}

const m_option_type_t m_option_type_print = {
    "Print",
    "",
    0,
    0,
    parse_print,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

const m_option_type_t m_option_type_print_indirect = {
    "Print",
    "",
    0,
    0,
    parse_print,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

const m_option_type_t m_option_type_print_func = {
    "Print",
    "",
    0,
    M_OPT_TYPE_ALLOW_WILDCARD,
    parse_print,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};


/////////////////////// Subconfig
#undef VAL
#define VAL(x) (*(char ***)(x))

static int parse_subconf(const m_option_t *opt, struct bstr name,
                         struct bstr param, bool ambiguous_param, void *dst)
{
    int nr = 0, i;
    char **lst = NULL;

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct bstr p = param;
    const struct m_option *subopts = opt->p;

    while (p.len) {
        int optlen = bstrcspn(p, ":=");
        struct bstr subopt = bstr_splice(p, 0, optlen);
        struct bstr subparam = bstr(NULL);
        p = bstr_cut(p, optlen);
        if (bstr_startswith0(p, "=")) {
            p = bstr_cut(p, 1);
            if (bstr_startswith0(p, "\"")) {
                p = bstr_cut(p, 1);
                optlen = bstrcspn(p, "\"");
                subparam = bstr_splice(p, 0, optlen);
                p = bstr_cut(p, optlen);
                if (!bstr_startswith0(p, "\"")) {
                    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                           "Terminating '\"' missing for '%.*s'\n",
                           BSTR_P(subopt));
                    return M_OPT_INVALID;
                }
                p = bstr_cut(p, 1);
            } else if (bstr_startswith0(p, "%")) {
                p = bstr_cut(p, 1);
                optlen = bstrtoll(p, &p, 0);
                if (!bstr_startswith0(p, "%") || (optlen > p.len - 1)) {
                    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                           "Invalid length %d for '%.*s'\n",
                           optlen, BSTR_P(subopt));
                    return M_OPT_INVALID;
                }
                subparam = bstr_splice(p, 1, optlen + 1);
                p = bstr_cut(p, optlen + 1);
            } else {
                optlen = bstrcspn(p, ":");
                subparam = bstr_splice(p, 0, optlen);
                p = bstr_cut(p, optlen);
            }
        }
        if (bstr_startswith0(p, ":"))
            p = bstr_cut(p, 1);
        else if (p.len > 0) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Incorrect termination for '%.*s'\n", BSTR_P(subopt));
            return M_OPT_INVALID;
        }

        for (i = 0; subopts[i].name; i++)
            if (!bstrcmp0(subopt, subopts[i].name))
                break;
        if (!subopts[i].name) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: Unknown suboption %.*s\n",
                   BSTR_P(name), BSTR_P(subopt));
            return M_OPT_UNKNOWN;
        }
        int r = m_option_parse(&subopts[i], subopt, subparam, false, NULL);
        if (r < 0)
            return r;
        if (dst) {
            lst = talloc_realloc(NULL, lst, char *, 2 * (nr + 2));
            lst[2 * nr] = bstrdup0(NULL, subopt);
            lst[2 * nr + 1] = subparam.len == 0 ? NULL :
                bstrdup0(NULL, subparam);
            memset(&lst[2 * (nr + 1)], 0, 2 * sizeof(char *));
            nr++;
        }
    }

    if (dst)
        VAL(dst) = lst;

    return 1;
}

const m_option_type_t m_option_type_subconfig = {
    "Subconfig",
    "The syntax is -option opt1=foo:flag:opt2=blah",
    sizeof(int),
    M_OPT_TYPE_HAS_CHILD,
    parse_subconf,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

#include "libmpcodecs/img_format.h"

/* FIXME: snyc with img_format.h */
static struct {
    const char *name;
    unsigned int fmt;
} mp_imgfmt_list[] = {
    {"444p16le", IMGFMT_444P16_LE},
    {"444p16be", IMGFMT_444P16_BE},
    {"444p10le", IMGFMT_444P10_LE},
    {"444p10be", IMGFMT_444P10_BE},
    {"444p9le", IMGFMT_444P9_LE},
    {"444p9be", IMGFMT_444P9_BE},
    {"422p16le", IMGFMT_422P16_LE},
    {"422p16be", IMGFMT_422P16_BE},
    {"422p10le", IMGFMT_422P10_LE},
    {"422p10be", IMGFMT_422P10_BE},
    {"420p16le", IMGFMT_420P16_LE},
    {"420p16be", IMGFMT_420P16_BE},
    {"420p10le", IMGFMT_420P10_LE},
    {"420p10be", IMGFMT_420P10_BE},
    {"420p9le", IMGFMT_420P9_LE},
    {"420p9be", IMGFMT_420P9_BE},
    {"444p16", IMGFMT_444P16},
    {"444p10", IMGFMT_444P10},
    {"444p9", IMGFMT_444P9},
    {"422p16", IMGFMT_422P16},
    {"422p10", IMGFMT_422P10},
    {"420p10", IMGFMT_420P10},
    {"420p9", IMGFMT_420P9},
    {"420p16", IMGFMT_420P16},
    {"420a", IMGFMT_420A},
    {"444p", IMGFMT_444P},
    {"422p", IMGFMT_422P},
    {"411p", IMGFMT_411P},
    {"440p", IMGFMT_440P},
    {"yuy2", IMGFMT_YUY2},
    {"yvyu", IMGFMT_YVYU},
    {"uyvy", IMGFMT_UYVY},
    {"yvu9", IMGFMT_YVU9},
    {"if09", IMGFMT_IF09},
    {"yv12", IMGFMT_YV12},
    {"i420", IMGFMT_I420},
    {"iyuv", IMGFMT_IYUV},
    {"clpl", IMGFMT_CLPL},
    {"hm12", IMGFMT_HM12},
    {"y800", IMGFMT_Y800},
    {"y8", IMGFMT_Y8},
    {"nv12", IMGFMT_NV12},
    {"nv21", IMGFMT_NV21},
    {"bgr24", IMGFMT_BGR24},
    {"bgr32", IMGFMT_BGR32},
    {"bgr16", IMGFMT_BGR16},
    {"bgr15", IMGFMT_BGR15},
    {"bgr12", IMGFMT_BGR12},
    {"bgr8", IMGFMT_BGR8},
    {"bgr4", IMGFMT_BGR4},
    {"bg4b", IMGFMT_BG4B},
    {"bgr1", IMGFMT_BGR1},
    {"rgb48be", IMGFMT_RGB48BE},
    {"rgb48le", IMGFMT_RGB48LE},
    {"rgb48ne", IMGFMT_RGB48NE},
    {"rgb24", IMGFMT_RGB24},
    {"rgb32", IMGFMT_RGB32},
    {"rgb16", IMGFMT_RGB16},
    {"rgb15", IMGFMT_RGB15},
    {"rgb12", IMGFMT_RGB12},
    {"rgb8", IMGFMT_RGB8},
    {"rgb4", IMGFMT_RGB4},
    {"rg4b", IMGFMT_RG4B},
    {"rgb1", IMGFMT_RGB1},
    {"rgba", IMGFMT_RGBA},
    {"argb", IMGFMT_ARGB},
    {"bgra", IMGFMT_BGRA},
    {"abgr", IMGFMT_ABGR},
    {"mjpeg", IMGFMT_MJPEG},
    {"mjpg", IMGFMT_MJPEG},
    { NULL, 0 }
};

static int parse_imgfmt(const m_option_t *opt, struct bstr name,
                        struct bstr param, bool ambiguous_param, void *dst)
{
    uint32_t fmt = 0;
    int i;

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    if (!bstrcmp0(param, "help")) {
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "Available formats:");
        for (i = 0; mp_imgfmt_list[i].name; i++)
            mp_msg(MSGT_CFGPARSER, MSGL_INFO, " %s", mp_imgfmt_list[i].name);
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
        return M_OPT_EXIT - 1;
    }

    if (bstr_startswith0(param, "0x"))
        fmt = bstrtoll(param, NULL, 16);
    else {
        for (i = 0; mp_imgfmt_list[i].name; i++) {
            if (!bstrcasecmp0(param, mp_imgfmt_list[i].name)) {
                fmt = mp_imgfmt_list[i].fmt;
                break;
            }
        }
        if (!mp_imgfmt_list[i].name) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: unknown format name: '%.*s'\n",
                   BSTR_P(name), BSTR_P(param));
            return M_OPT_INVALID;
        }
    }

    if (dst)
        *((uint32_t *)dst) = fmt;

    return 1;
}

const m_option_type_t m_option_type_imgfmt = {
    "Image format",
    "Please report any missing colorspaces.",
    sizeof(uint32_t),
    0,
    parse_imgfmt,
    NULL,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};

#include "libaf/af_format.h"

/* FIXME: snyc with af_format.h */
static struct {
    const char *name;
    unsigned int fmt;
} mp_afmt_list[] = {
    // SPECIAL
    {"mulaw", AF_FORMAT_MU_LAW},
    {"alaw", AF_FORMAT_A_LAW},
    {"mpeg2", AF_FORMAT_MPEG2},
    {"ac3le", AF_FORMAT_AC3_LE},
    {"ac3be", AF_FORMAT_AC3_BE},
    {"ac3ne", AF_FORMAT_AC3_NE},
    {"imaadpcm", AF_FORMAT_IMA_ADPCM},
    // ORDINARY
    {"u8", AF_FORMAT_U8},
    {"s8", AF_FORMAT_S8},
    {"u16le", AF_FORMAT_U16_LE},
    {"u16be", AF_FORMAT_U16_BE},
    {"u16ne", AF_FORMAT_U16_NE},
    {"s16le", AF_FORMAT_S16_LE},
    {"s16be", AF_FORMAT_S16_BE},
    {"s16ne", AF_FORMAT_S16_NE},
    {"u24le", AF_FORMAT_U24_LE},
    {"u24be", AF_FORMAT_U24_BE},
    {"u24ne", AF_FORMAT_U24_NE},
    {"s24le", AF_FORMAT_S24_LE},
    {"s24be", AF_FORMAT_S24_BE},
    {"s24ne", AF_FORMAT_S24_NE},
    {"u32le", AF_FORMAT_U32_LE},
    {"u32be", AF_FORMAT_U32_BE},
    {"u32ne", AF_FORMAT_U32_NE},
    {"s32le", AF_FORMAT_S32_LE},
    {"s32be", AF_FORMAT_S32_BE},
    {"s32ne", AF_FORMAT_S32_NE},
    {"floatle", AF_FORMAT_FLOAT_LE},
    {"floatbe", AF_FORMAT_FLOAT_BE},
    {"floatne", AF_FORMAT_FLOAT_NE},
    { NULL, 0 }
};

static int parse_afmt(const m_option_t *opt, struct bstr name,
                      struct bstr param, bool ambiguous_param, void *dst)
{
    uint32_t fmt = 0;
    int i;

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    if (!bstrcmp0(param, "help")) {
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "Available formats:");
        for (i = 0; mp_afmt_list[i].name; i++)
            mp_msg(MSGT_CFGPARSER, MSGL_INFO, " %s", mp_afmt_list[i].name);
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
        return M_OPT_EXIT - 1;
    }

    if (bstr_startswith0(param, "0x"))
        fmt = bstrtoll(param, NULL, 16);
    else {
        for (i = 0; mp_afmt_list[i].name; i++) {
            if (!bstrcasecmp0(param, mp_afmt_list[i].name)) {
                fmt = mp_afmt_list[i].fmt;
                break;
            }
        }
        if (!mp_afmt_list[i].name) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: unknown format name: '%.*s'\n",
                   BSTR_P(name), BSTR_P(param));
            return M_OPT_INVALID;
        }
    }

    if (dst)
        *((uint32_t *)dst) = fmt;

    return 1;
}

const m_option_type_t m_option_type_afmt = {
    "Audio format",
    "Please report any missing formats.",
    sizeof(uint32_t),
    0,
    parse_afmt,
    NULL,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};


static int parse_timestring(struct bstr str, double *time, char endchar)
{
    int a, b, len;
    double d;
    *time = 0; /* ensure initialization for error cases */
    if (bstr_sscanf(str, "%d:%d:%lf%n", &a, &b, &d, &len) >= 3)
        *time = 3600 * a + 60 * b + d;
    else if (bstr_sscanf(str, "%d:%lf%n", &a, &d, &len) >= 2)
        *time = 60 * a + d;
    else if (bstr_sscanf(str, "%lf%n", &d, &len) >= 1)
        *time = d;
    else
        return 0;  /* unsupported time format */
    if (len < str.len && str.start[len] != endchar)
        return 0;  /* invalid extra characters at the end */
    return len;
}


static int parse_time(const m_option_t *opt, struct bstr name,
                      struct bstr param, bool ambiguous_param, void *dst)
{
    double time;

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    if (!parse_timestring(param, &time, 0)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: invalid time: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    if (dst)
        *(double *)dst = time;
    return 1;
}

const m_option_type_t m_option_type_time = {
    "Time",
    "",
    sizeof(double),
    0,
    parse_time,
    print_double,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};


// Time or size (-endpos)

static int parse_time_size(const m_option_t *opt, struct bstr name,
                           struct bstr param, bool ambiguous_param, void *dst)
{
    m_time_size_t ts;
    char unit[4];
    double end_at;

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    ts.pos = 0;
    /* End at size parsing */
    if (bstr_sscanf(param, "%lf%3s", &end_at, unit) == 2) {
        ts.type = END_AT_SIZE;
        if (!strcasecmp(unit, "b"))
            ;
        else if (!strcasecmp(unit, "kb"))
            end_at *= 1024;
        else if (!strcasecmp(unit, "mb"))
            end_at *= 1024 * 1024;
        else if (!strcasecmp(unit, "gb"))
            end_at *= 1024 * 1024 * 1024;
        else
            ts.type = END_AT_NONE;

        if (ts.type == END_AT_SIZE) {
            ts.pos  = end_at;
            goto out;
        }
    }

    /* End at time parsing. This has to be last because the parsing accepts
     * even a number followed by garbage */
    if (!parse_timestring(param, &end_at, 0)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Option %.*s: invalid time or size: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    ts.type = END_AT_TIME;
    ts.pos  = end_at;
out:
    if (dst)
        *(m_time_size_t *)dst = ts;
    return 1;
}

const m_option_type_t m_option_type_time_size = {
    "Time or size",
    "",
    sizeof(m_time_size_t),
    0,
    parse_time_size,
    NULL,
    copy_opt,
    copy_opt,
    NULL,
    NULL
};


//// Objects (i.e. filters, etc) settings

#include "m_struct.h"

#undef VAL
#define VAL(x) (*(m_obj_settings_t **)(x))

static int find_obj_desc(struct bstr name, const m_obj_list_t *l,
                         const m_struct_t **ret)
{
    int i;
    char *n;

    for (i = 0; l->list[i]; i++) {
        n = M_ST_MB(char *, l->list[i], l->name_off);
        if (!bstrcmp0(name, n)) {
            *ret = M_ST_MB(m_struct_t *, l->list[i], l->desc_off);
            return 1;
        }
    }
    return 0;
}

static int get_obj_param(struct bstr opt_name, struct bstr obj_name,
                         const m_struct_t *desc, struct bstr str, int *nold,
                         int oldmax, char **dst)
{
    const m_option_t *opt;
    int r;

    int eq = bstrchr(str, '=');

    if (eq > 0) {   // eq == 0  ignored
        struct bstr p = bstr_cut(str, eq + 1);
        str = bstr_splice(str, 0, eq);
        opt = m_option_list_findb(desc->fields, str);
        if (!opt) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: %.*s doesn't have a %.*s parameter.\n",
                   BSTR_P(opt_name), BSTR_P(obj_name), BSTR_P(str));
            return M_OPT_UNKNOWN;
        }
        r = m_option_parse(opt, str, p, false, NULL);
        if (r < 0) {
            if (r > M_OPT_EXIT)
                mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: "
                       "Error while parsing %.*s parameter %.*s (%.*s)\n",
                       BSTR_P(opt_name), BSTR_P(obj_name), BSTR_P(str),
                       BSTR_P(p));
            return r;
        }
        if (dst) {
            dst[0] = bstrdup0(NULL, str);
            dst[1] = bstrdup0(NULL, p);
        }
    } else {
        if ((*nold) >= oldmax) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: %.*s has only %d params, so you can't give more than %d unnamed params.\n",
                   BSTR_P(opt_name), BSTR_P(obj_name), oldmax, oldmax);
            return M_OPT_OUT_OF_RANGE;
        }
        opt = &desc->fields[(*nold)];
        r = m_option_parse(opt, bstr(opt->name), str, false, NULL);
        if (r < 0) {
            if (r > M_OPT_EXIT)
                mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: "
                       "Error while parsing %.*s parameter %s (%.*s)\n",
                       BSTR_P(opt_name), BSTR_P(obj_name), opt->name,
                       BSTR_P(str));
            return r;
        }
        if (dst) {
            dst[0] = talloc_strdup(NULL, opt->name);
            dst[1] = bstrdup0(NULL, str);
        }
        (*nold)++;
    }
    return 1;
}

static int get_obj_params(struct bstr opt_name, struct bstr name,
                          struct bstr params, const m_struct_t *desc,
                          char separator, char ***_ret)
{
    int n = 0, nold = 0, nopts;
    char **ret;

    if (!bstrcmp0(params, "help")) { // Help
        char min[50], max[50];
        if (!desc->fields) {
            mp_msg(MSGT_CFGPARSER, MSGL_INFO,
                   "%.*s doesn't have any options.\n\n", BSTR_P(name));
            return M_OPT_EXIT - 1;
        }
        mp_msg(MSGT_CFGPARSER, MSGL_INFO,
               "\n Name                 Type            Min        Max\n\n");
        for (n = 0; desc->fields[n].name; n++) {
            const m_option_t *opt = &desc->fields[n];
            if (opt->type->flags & M_OPT_TYPE_HAS_CHILD)
                continue;
            if (opt->flags & M_OPT_MIN)
                sprintf(min, "%-8.0f", opt->min);
            else
                strcpy(min, "No");
            if (opt->flags & M_OPT_MAX)
                sprintf(max, "%-8.0f", opt->max);
            else
                strcpy(max, "No");
            mp_msg(MSGT_CFGPARSER, MSGL_INFO,
                   " %-20.20s %-15.15s %-10.10s %-10.10s\n",
                   opt->name, opt->type->name, min, max);
        }
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
        return M_OPT_EXIT - 1;
    }

    for (nopts = 0; desc->fields[nopts].name; nopts++)
        /* NOP */;

    // TODO : Check that each opt can be parsed
    struct bstr s = params;
    while (1) {
        bool end = false;
        int idx = bstrchr(s, separator);
        if (idx < 0) {
            idx = s.len;
            end = true;
        }
        struct bstr field = bstr_splice(s, 0, idx);
        s = bstr_cut(s, idx + 1);
        if (field.len == 0) { // Empty field, count it and go on
            nold++;
        } else {
            int r = get_obj_param(opt_name, name, desc, field, &nold, nopts,
                                  NULL);
            if (r < 0)
                return r;
            n++;
        }
        if (end)
            break;
    }
    if (nold > nopts) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Too many options for %.*s\n",
               BSTR_P(name));
        return M_OPT_OUT_OF_RANGE;
    }
    if (!_ret) // Just test
        return 1;
    if (n == 0) // No options or only empty options
        return 1;

    ret = talloc_array(NULL, char *, (n + 2) * 2);
    n = nold = 0;
    s = params;

    while (s.len > 0) {
        int idx = bstrchr(s, separator);
        if (idx < 0)
            idx = s.len;
        struct bstr field = bstr_splice(s, 0, idx);
        s = bstr_cut(s, idx + 1);
        if (field.len == 0) { // Empty field, count it and go on
            nold++;
        } else {
            get_obj_param(opt_name, name, desc, field, &nold, nopts,
                          &ret[n * 2]);
            n++;
        }
    }
    ret[n * 2] = ret[n * 2 + 1] = NULL;
    *_ret = ret;

    return 1;
}

static int parse_obj_params(const m_option_t *opt, struct bstr name,
                            struct bstr param, bool ambiguous_param, void *dst)
{
    char **opts;
    int r;
    m_obj_params_t *p = opt->priv;
    const m_struct_t *desc;

    // We need the object desc
    if (!p)
        return M_OPT_INVALID;

    desc = p->desc;
    r = get_obj_params(name, bstr(desc->name), param, desc, p->separator,
                       dst ? &opts : NULL);
    if (r < 0)
        return r;
    if (!dst)
        return 1;
    if (!opts) // no arguments given
        return 1;

    for (r = 0; opts[r]; r += 2)
        m_struct_set(desc, dst, opts[r], bstr(opts[r + 1]));

    return 1;
}


const m_option_type_t m_option_type_obj_params = {
    "Object params",
    "",
    0,
    0,
    parse_obj_params,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/// Some predefined types as a definition would be quite lengthy

/// Span arguments
static const m_span_t m_span_params_dflts = {
    -1, -1
};
static const m_option_t m_span_params_fields[] = {
    {"start", M_ST_OFF(m_span_t, start), CONF_TYPE_INT, M_OPT_MIN, 1, 0, NULL},
    {"end", M_ST_OFF(m_span_t, end), CONF_TYPE_INT, M_OPT_MIN, 1, 0, NULL},
    { NULL, NULL, 0, 0, 0, 0, NULL }
};
static const struct m_struct_st m_span_opts = {
    "m_span",
    sizeof(m_span_t),
    &m_span_params_dflts,
    m_span_params_fields
};
const m_obj_params_t m_span_params_def = {
    &m_span_opts,
    '-'
};

static int parse_obj_settings(struct bstr opt, struct bstr str,
                              const m_obj_list_t *list,
                              m_obj_settings_t **_ret, int ret_n)
{
    int r;
    char **plist = NULL;
    const m_struct_t *desc;
    m_obj_settings_t *ret = _ret ? *_ret : NULL;

    struct bstr param = bstr(NULL);
    int idx = bstrchr(str, '=');
    if (idx >= 0) {
        param = bstr_cut(str, idx + 1);
        str = bstr_splice(str, 0, idx);
    }

    if (!find_obj_desc(str, list, &desc)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: %.*s doesn't exist.\n",
               BSTR_P(opt), BSTR_P(str));
        return M_OPT_INVALID;
    }

    if (param.start) {
        if (!desc && _ret) {
            if (!bstrcmp0(param, "help")) {
                mp_msg(MSGT_CFGPARSER, MSGL_INFO,
                       "Option %.*s: %.*s have no option description.\n",
                       BSTR_P(opt), BSTR_P(str));
                return M_OPT_EXIT - 1;
            }
            plist = talloc_zero_array(NULL, char *, 4);
            plist[0] = talloc_strdup(NULL, "_oldargs_");
            plist[1] = bstrdup0(NULL, param);
        } else if (desc) {
            r = get_obj_params(opt, str, param, desc, ':',
                               _ret ? &plist : NULL);
            if (r < 0)
                return r;
        }
    }
    if (!_ret)
        return 1;

    ret = talloc_realloc(NULL, ret, struct m_obj_settings, ret_n + 2);
    memset(&ret[ret_n], 0, 2 * sizeof(m_obj_settings_t));
    ret[ret_n].name = bstrdup0(NULL, str);
    ret[ret_n].attribs = plist;

    *_ret = ret;
    return 1;
}

static int obj_settings_list_del(struct bstr opt_name, struct bstr param,
                                 bool ambiguous_param, void *dst)
{
    char **str_list = NULL;
    int r, i, idx_max = 0;
    char *rem_id = "_removed_marker_";
    char name[100];
    assert(opt_name.len < 100);
    memcpy(name, opt_name.start, opt_name.len);
    name[opt_name.len] = 0;
    const m_option_t list_opt = {
        name, NULL, CONF_TYPE_STRING_LIST,
        0, 0, 0, NULL
    };
    m_obj_settings_t *obj_list = dst ? VAL(dst) : NULL;

    if (dst && !obj_list) {
        mp_msg(MSGT_CFGPARSER, MSGL_WARN, "Option %.*s: the list is empty.\n",
               BSTR_P(opt_name));
        return 1;
    } else if (obj_list) {
        for (idx_max = 0; obj_list[idx_max].name != NULL; idx_max++)
            /* NOP */;
    }

    r = m_option_parse(&list_opt, opt_name, param, false, &str_list);
    if (r < 0 || !str_list)
        return r;

    for (r = 0; str_list[r]; r++) {
        int id;
        char *endptr;
        id = strtol(str_list[r], &endptr, 0);
        if (endptr == str_list[r]) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: invalid parameter. We need a list of integers which are the indices of the elements to remove.\n", BSTR_P(opt_name));
            m_option_free(&list_opt, &str_list);
            return M_OPT_INVALID;
        }
        if (!obj_list)
            continue;
        if (id >= idx_max || id < -idx_max) {
            mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                   "Option %.*s: Index %d is out of range.\n",
                   BSTR_P(opt_name), id);
            continue;
        }
        if (id < 0)
            id = idx_max + id;
        talloc_free(obj_list[id].name);
        free_str_list(&(obj_list[id].attribs));
        obj_list[id].name = rem_id;
    }

    if (!dst) {
        m_option_free(&list_opt, &str_list);
        return 1;
    }

    for (i = 0; obj_list[i].name; i++) {
        while (obj_list[i].name == rem_id) {
            memmove(&obj_list[i], &obj_list[i + 1],
                    sizeof(m_obj_settings_t) * (idx_max - i));
            idx_max--;
        }
    }
    obj_list = talloc_realloc(NULL, obj_list, struct m_obj_settings,
                              idx_max + 1);
    VAL(dst) = obj_list;

    return 1;
}

static void free_obj_settings_list(void *dst)
{
    int n;
    m_obj_settings_t *d;

    if (!dst || !VAL(dst))
        return;

    d = VAL(dst);
    for (n = 0; d[n].name; n++) {
        talloc_free(d[n].name);
        free_str_list(&(d[n].attribs));
    }
    talloc_free(d);
    VAL(dst) = NULL;
}

static int parse_obj_settings_list(const m_option_t *opt, struct bstr name,
                                   struct bstr param, bool ambiguous_param,
                                   void *dst)
{
    int len = strlen(opt->name);
    m_obj_settings_t *res = NULL, *queue = NULL, *head = NULL;
    int op = OP_NONE;

    // We need the objects list
    if (!opt->priv)
        return M_OPT_INVALID;

    if (opt->name[len - 1] == '*' && (name.len > len - 1)) {
        struct bstr suffix = bstr_cut(name, len - 1);
        if (bstrcasecmp0(suffix, "-add") == 0)
            op = OP_ADD;
        else if (bstrcasecmp0(suffix, "-pre") == 0)
            op = OP_PRE;
        else if (bstrcasecmp0(suffix, "-del") == 0)
            op = OP_DEL;
        else if (bstrcasecmp0(suffix, "-clr") == 0)
            op = OP_CLR;
        else {
            char prefix[len];
            strncpy(prefix, opt->name, len - 1);
            prefix[len - 1] = '\0';
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: unknown postfix %.*s\n"
                   "Supported postfixes are:\n"
                   "  %s-add\n"
                   " Append the given list to the current list\n\n"
                   "  %s-pre\n"
                   " Prepend the given list to the current list\n\n"
                   "  %s-del x,y,...\n"
                   " Remove the given elements. Take the list element index (starting from 0).\n"
                   " Negative index can be used (i.e. -1 is the last element)\n\n"
                   "  %s-clr\n"
                   " Clear the current list.\n",
                   BSTR_P(name), BSTR_P(suffix), prefix, prefix, prefix, prefix);

            return M_OPT_UNKNOWN;
        }
    }

    // Clear the list ??
    if (op == OP_CLR) {
        if (dst)
            free_obj_settings_list(dst);
        return 0;
    }

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    switch (op) {
    case OP_ADD:
        if (dst)
            head = VAL(dst);
        break;
    case OP_PRE:
        if (dst)
            queue = VAL(dst);
        break;
    case OP_DEL:
        return obj_settings_list_del(name, param, false, dst);
    case OP_NONE:
        if (dst && VAL(dst))
            free_obj_settings_list(dst);
        break;
    default:
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: FIXME\n", BSTR_P(name));
        return M_OPT_UNKNOWN;
    }

    if (!bstrcmp0(param, "help")) {
        m_obj_list_t *ol = opt->priv;
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "Available video filters:\n");
        for (int n = 0; ol->list[n]; n++)
            mp_msg(MSGT_CFGPARSER, MSGL_INFO, "  %-15s: %s\n",
                   M_ST_MB(char *, ol->list[n], ol->name_off),
                   M_ST_MB(char *, ol->list[n], ol->info_off));
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
        return M_OPT_EXIT - 1;
    }

    struct bstr s = bstrdup(NULL, param);
    char *allocptr = s.start;
    int n = 0;
    while (s.len > 0) {
        struct bstr el = get_nextsep(&s, OPTION_LIST_SEPARATOR, 1);
        int r = parse_obj_settings(name, el, opt->priv, dst ? &res : NULL, n);
        if (r < 0) {
            talloc_free(allocptr);
            return r;
        }
        s = bstr_cut(s, 1);
        n++;
    }
    talloc_free(allocptr);
    if (n == 0)
        return M_OPT_INVALID;

    if (((opt->flags & M_OPT_MIN) && (n < opt->min)) ||
        ((opt->flags & M_OPT_MAX) && (n > opt->max)))
        return M_OPT_OUT_OF_RANGE;

    if (dst) {
        if (queue) {
            int qsize;
            for (qsize = 0; queue[qsize].name; qsize++)
                /* NOP */;
            res = talloc_realloc(NULL, res, struct m_obj_settings,
                                 qsize + n + 1);
            memcpy(&res[n], queue, (qsize + 1) * sizeof(m_obj_settings_t));
            n += qsize;
            talloc_free(queue);
        }
        if (head) {
            int hsize;
            for (hsize = 0; head[hsize].name; hsize++)
                /* NOP */;
            head = talloc_realloc(NULL, head, struct m_obj_settings,
                                  hsize + n + 1);
            memcpy(&head[hsize], res, (n + 1) * sizeof(m_obj_settings_t));
            talloc_free(res);
            res = head;
        }
        VAL(dst) = res;
    }
    return 1;
}

static void copy_obj_settings_list(const m_option_t *opt, void *dst,
                                   const void *src)
{
    m_obj_settings_t *d, *s;
    int n;

    if (!(dst && src))
        return;

    s = VAL(src);

    if (VAL(dst))
        free_obj_settings_list(dst);
    if (!s)
        return;



    for (n = 0; s[n].name; n++)
        /* NOP */;
    d = talloc_array(NULL, struct m_obj_settings, n + 1);
    for (n = 0; s[n].name; n++) {
        d[n].name = talloc_strdup(NULL, s[n].name);
        d[n].attribs = NULL;
        copy_str_list(NULL, &(d[n].attribs), &(s[n].attribs));
    }
    d[n].name = NULL;
    d[n].attribs = NULL;
    VAL(dst) = d;
}

const m_option_type_t m_option_type_obj_settings_list = {
    "Object settings list",
    "",
    sizeof(m_obj_settings_t *),
    M_OPT_TYPE_DYNAMIC | M_OPT_TYPE_ALLOW_WILDCARD,
    parse_obj_settings_list,
    NULL,
    copy_obj_settings_list,
    copy_obj_settings_list,
    copy_obj_settings_list,
    free_obj_settings_list,
};



static int parse_obj_presets(const m_option_t *opt, struct bstr name,
                             struct bstr param, bool ambiguous_param,
                             void *dst)
{
    m_obj_presets_t *obj_p = (m_obj_presets_t *)opt->priv;
    const m_struct_t *in_desc;
    const m_struct_t *out_desc;
    int s, i;
    const unsigned char *pre;
    char *pre_name = NULL;

    if (!obj_p) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: Presets need a "
               "pointer to a m_obj_presets_t in the priv field.\n",
               BSTR_P(name));
        return M_OPT_PARSER_ERR;
    }

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    pre = obj_p->presets;
    in_desc = obj_p->in_desc;
    out_desc = obj_p->out_desc ? obj_p->out_desc : obj_p->in_desc;
    s = in_desc->size;

    if (!bstrcmp0(param, "help")) {
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "Available presets for %s->%.*s:",
               out_desc->name, BSTR_P(name));
        for (pre = obj_p->presets;
             (pre_name = M_ST_MB(char *, pre, obj_p->name_off)); pre +=  s)
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, " %s", pre_name);
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "\n");
        return M_OPT_EXIT - 1;
    }

    for (pre_name = M_ST_MB(char *, pre, obj_p->name_off); pre_name;
         pre +=  s, pre_name = M_ST_MB(char *, pre, obj_p->name_off))
        if (!bstrcmp0(param, pre_name))
            break;
    if (!pre_name) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Option %.*s: There is no preset named %.*s\n"
               "Available presets are:", BSTR_P(name), BSTR_P(param));
        for (pre = obj_p->presets;
             (pre_name = M_ST_MB(char *, pre, obj_p->name_off)); pre +=  s)
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, " %s", pre_name);
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "\n");
        return M_OPT_INVALID;
    }

    if (!dst)
        return 1;

    for (i = 0; in_desc->fields[i].name; i++) {
        const m_option_t *out_opt = m_option_list_find(out_desc->fields,
                                                       in_desc->fields[i].name);
        if (!out_opt) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                "Option %.*s: Unable to find the target option for field %s.\n"
                "Please report this to the developers.\n",
                BSTR_P(name), in_desc->fields[i].name);
            return M_OPT_PARSER_ERR;
        }
        m_option_copy(out_opt, M_ST_MB_P(dst, out_opt->p),
                      M_ST_MB_P(pre, in_desc->fields[i].p));
    }
    return 1;
}


const m_option_type_t m_option_type_obj_presets = {
    "Object presets",
    "",
    0,
    0,
    parse_obj_presets,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static int parse_custom_url(const m_option_t *opt, struct bstr name,
                            struct bstr url, bool ambiguous_param, void *dst)
{
    int r;
    m_struct_t *desc = opt->priv;

    if (!desc) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: Custom URL needs "
               "a pointer to a m_struct_t in the priv field.\n", BSTR_P(name));
        return M_OPT_PARSER_ERR;
    }

    // extract the protocol
    int idx = bstr_find0(url, "://");
    if (idx < 0) {
        // Filename only
        if (m_option_list_find(desc->fields, "filename")) {
            m_struct_set(desc, dst, "filename", url);
            return 1;
        }
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Option %.*s: URL doesn't have a valid protocol!\n",
               BSTR_P(name));
        return M_OPT_INVALID;
    }
    struct bstr ptr1 = bstr_cut(url, idx + 3);
    if (m_option_list_find(desc->fields, "string")) {
        if (ptr1.len > 0) {
            m_struct_set(desc, dst, "string", ptr1);
            return 1;
        }
    }
    if (dst && m_option_list_find(desc->fields, "protocol")) {
        r = m_struct_set(desc, dst, "protocol", bstr_splice(url, 0, idx));
        if (r < 0) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: Error while setting protocol.\n",
                   BSTR_P(name));
            return r;
        }
    }

    // check if a username:password is given
    idx = bstrchr(ptr1, '/');
    if (idx < 0)
        idx = ptr1.len;
    struct bstr hostpart = bstr_splice(ptr1, 0, idx);
    struct bstr path = bstr_cut(ptr1, idx);
    idx = bstrchr(hostpart, '@');
    if (idx >= 0) {
        // We got something, at least a username...
        if (!m_option_list_find(desc->fields, "username")) {
            mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                   "Option %.*s: This URL doesn't have a username part.\n",
                   BSTR_P(name));
            // skip
        } else {
            struct bstr userpass = bstr_splice(hostpart, 0, idx);
            idx = bstrchr(userpass, ':');
            if (idx >= 0) {
                // We also have a password
                if (!m_option_list_find(desc->fields, "password")) {
                    mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                       "Option %.*s: This URL doesn't have a password part.\n",
                       BSTR_P(name));
                    // skip
                } else { // Username and password
                    if (dst) {
                        r = m_struct_set(desc, dst, "username",
                                         bstr_splice(userpass, 0, idx));
                        if (r < 0) {
                            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                                "Option %.*s: Error while setting username.\n",
                                BSTR_P(name));
                            return r;
                        }
                        r = m_struct_set(desc, dst, "password",
                                         bstr_splice(userpass, idx+1,
                                                     userpass.len));
                        if (r < 0) {
                            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                                "Option %.*s: Error while setting password.\n",
                                BSTR_P(name));
                            return r;
                        }
                    }
                }
            } else { // User name only
                r = m_struct_set(desc, dst, "username", userpass);
                if (r < 0) {
                    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                           "Option %.*s: Error while setting username.\n",
                           BSTR_P(name));
                    return r;
                }
            }
        }
        hostpart = bstr_cut(hostpart, idx + 1);
    }

    // Before looking for a port number check if we have an IPv6 type
    // numeric address.
    // In an IPv6 URL the numeric address should be inside square braces.
    int idx1 = bstrchr(hostpart, '[');
    int idx2 = bstrchr(hostpart, ']');
    struct bstr portstr = hostpart;
    bool v6addr = false;
    if (idx1 >= 0 && idx2 >= 0 && idx1 < idx2) {
        // we have an IPv6 numeric address
        portstr = bstr_cut(hostpart, idx2);
        hostpart = bstr_splice(hostpart, idx1 + 1, idx2);
        v6addr = true;
    }

    idx = bstrchr(portstr, ':');
    if (idx >= 0) {
        if (!v6addr)
            hostpart = bstr_splice(hostpart, 0, idx);
        // We have an URL beginning like http://www.hostname.com:1212
        // Get the port number
        if (!m_option_list_find(desc->fields, "port")) {
            mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                   "Option %.*s: This URL doesn't have a port part.\n",
                   BSTR_P(name));
            // skip
        } else {
            if (dst) {
                int p = bstrtoll(bstr_cut(portstr, idx + 1), NULL, 0);
                char tmp[100];
                snprintf(tmp, 99, "%d", p);
                r = m_struct_set(desc, dst, "port", bstr(tmp));
                if (r < 0) {
                    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                           "Option %.*s: Error while setting port.\n",
                           BSTR_P(name));
                    return r;
                }
            }
        }
    }
    // Get the hostname
    if (hostpart.len > 0) {
        if (!m_option_list_find(desc->fields, "hostname")) {
            mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                   "Option %.*s: This URL doesn't have a hostname part.\n",
                   BSTR_P(name));
            // skip
        } else {
            r = m_struct_set(desc, dst, "hostname", hostpart);
            if (r < 0) {
                mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                       "Option %.*s: Error while setting hostname.\n",
                       BSTR_P(name));
                return r;
            }
        }
    }
    // Look if a path is given
    if (path.len > 1) {   // not just "/"
        // copy the path/filename in the URL container
        if (!m_option_list_find(desc->fields, "filename")) {
            mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                   "Option %.*s: This URL doesn't have a filename part.\n",
                   BSTR_P(name));
            // skip
        } else {
            if (dst) {
                char *fname = bstrdup0(NULL, bstr_cut(path, 1));
                url_unescape_string(fname, fname);
                r = m_struct_set(desc, dst, "filename", bstr(fname));
                talloc_free(fname);
                if (r < 0) {
                    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                           "Option %.*s: Error while setting filename.\n",
                           BSTR_P(name));
                    return r;
                }
            }
        }
    }
    return 1;
}

/// TODO : Write the other needed funcs for 'normal' options
const m_option_type_t m_option_type_custom_url = {
    "Custom URL",
    "",
    0,
    0,
    parse_custom_url,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};
