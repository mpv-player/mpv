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
#include <limits.h>
#include <inttypes.h>
#include <unistd.h>
#include <assert.h>

#include <libavutil/common.h>
#include <libavutil/avstring.h>

#include "talloc.h"
#include "core/mp_common.h"
#include "core/m_option.h"
#include "core/mp_msg.h"
#include "stream/url.h"

char *m_option_strerror(int code)
{
    switch (code) {
    case M_OPT_UNKNOWN:
        return mp_gtext("option not found");
    case M_OPT_MISSING_PARAM:
        return mp_gtext("option requires parameter");
    case M_OPT_INVALID:
        return mp_gtext("option parameter could not be parsed");
    case M_OPT_OUT_OF_RANGE:
        return mp_gtext("parameter is outside values allowed for option");
    case M_OPT_DISALLOW_PARAM:
        return mp_gtext("option doesn't take a parameter");
    case M_OPT_PARSER_ERR:
    default:
        return mp_gtext("parser error");
    }
}

int m_option_required_params(const m_option_t *opt)
{
    if (((opt->flags & M_OPT_OPTIONAL_PARAM) ||
            (opt->type->flags & M_OPT_TYPE_OPTIONAL_PARAM)))
        return 0;
    return 1;
}

static const struct m_option *m_option_list_findb(const struct m_option *list,
                                                  struct bstr name)
{
    for (int i = 0; list[i].name; i++) {
        struct bstr lname = bstr0(list[i].name);
        if ((list[i].type->flags & M_OPT_TYPE_ALLOW_WILDCARD)
                && bstr_endswith0(lname, "*")) {
            lname.len--;
            if (bstrcmp(bstr_splice(name, 0, lname.len), lname) == 0)
                return &list[i];
        } else if (bstrcmp(lname, name) == 0)
            return &list[i];
    }
    return NULL;
}

const m_option_t *m_option_list_find(const m_option_t *list, const char *name)
{
    return m_option_list_findb(list, bstr0(name));
}

// Default function that just does a memcpy

static void copy_opt(const m_option_t *opt, void *dst, const void *src)
{
    if (dst && src)
        memcpy(dst, src, opt->type->size);
}

// Flag

#define VAL(x) (*(int *)(x))

static int clamp_flag(const m_option_t *opt, void *val)
{
    if (VAL(val) == opt->min || VAL(val) == opt->max)
        return 0;
    VAL(val) = opt->min;
    return M_OPT_OUT_OF_RANGE;
}

static int parse_flag(const m_option_t *opt, struct bstr name,
                      struct bstr param, void *dst)
{
    if (param.len) {
        if (!bstrcmp0(param, "yes")) {
            if (dst)
                VAL(dst) = opt->max;
            return 1;
        }
        if (!bstrcmp0(param, "no")) {
            if (dst)
                VAL(dst) = opt->min;
            return 1;
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

static void add_flag(const m_option_t *opt, void *val, double add, bool wrap)
{
    if (fabs(add) < 0.5)
        return;
    bool state = VAL(val) != opt->min;
    state = wrap ? !state : add > 0;
    VAL(val) = state ? opt->max : opt->min;
}

const m_option_type_t m_option_type_flag = {
    // need yes or no in config files
    .name  = "Flag",
    .size  = sizeof(int),
    .flags = M_OPT_TYPE_OPTIONAL_PARAM,
    .parse = parse_flag,
    .print = print_flag,
    .copy  = copy_opt,
    .add = add_flag,
    .clamp = clamp_flag,
};

// Single-value, write-only flag

static int parse_store(const m_option_t *opt, struct bstr name,
                       struct bstr param, void *dst)
{
    if (param.len == 0 || bstrcmp0(param, "yes") == 0) {
        if (dst)
            VAL(dst) = opt->max;
        return 0;
    } else {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Invalid parameter for %.*s flag: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_DISALLOW_PARAM;
    }
}

const m_option_type_t m_option_type_store = {
    // can only be activated
    .name  = "Flag",
    .size  = sizeof(int),
    .flags = M_OPT_TYPE_OPTIONAL_PARAM,
    .parse = parse_store,
};

// Same for float types

#undef VAL
#define VAL(x) (*(float *)(x))

static int parse_store_float(const m_option_t *opt, struct bstr name,
                             struct bstr param, void *dst)
{
    if (param.len == 0 || bstrcmp0(param, "yes") == 0) {
        if (dst)
            VAL(dst) = opt->max;
        return 0;
    } else {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Invalid parameter for %.*s flag: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_DISALLOW_PARAM;
    }
}

const m_option_type_t m_option_type_float_store = {
    // can only be activated
    .name  = "Flag",
    .size  = sizeof(float),
    .flags = M_OPT_TYPE_OPTIONAL_PARAM,
    .parse = parse_store_float,
};

// Integer

#undef VAL

static int clamp_longlong(const m_option_t *opt, void *val)
{
    long long v = *(long long *)val;
    int r = 0;
    if ((opt->flags & M_OPT_MAX) && (v > opt->max)) {
        v = opt->max;
        r = M_OPT_OUT_OF_RANGE;
    }
    if ((opt->flags & M_OPT_MIN) && (v < opt->min)) {
        v = opt->min;
        r = M_OPT_OUT_OF_RANGE;
    }
    *(long long *)val = v;
    return r;
}

static int parse_longlong(const m_option_t *opt, struct bstr name,
                          struct bstr param, void *dst)
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

static int clamp_int(const m_option_t *opt, void *val)
{
    long long tmp = *(int *)val;
    int r = clamp_longlong(opt, &tmp);
    *(int *)val = tmp;
    return r;
}

static int clamp_int64(const m_option_t *opt, void *val)
{
    long long tmp = *(int64_t *)val;
    int r = clamp_longlong(opt, &tmp);
    *(int64_t *)val = tmp;
    return r;
}

static int parse_int(const m_option_t *opt, struct bstr name,
                     struct bstr param, void *dst)
{
    long long tmp;
    int r = parse_longlong(opt, name, param, &tmp);
    if (r >= 0 && dst)
        *(int *)dst = tmp;
    return r;
}

static int parse_int64(const m_option_t *opt, struct bstr name,
                       struct bstr param, void *dst)
{
    long long tmp;
    int r = parse_longlong(opt, name, param, &tmp);
    if (r >= 0 && dst)
        *(int64_t *)dst = tmp;
    return r;
}

static char *print_int(const m_option_t *opt, const void *val)
{
    if (opt->type->size == sizeof(int64_t))
        return talloc_asprintf(NULL, "%"PRId64, *(const int64_t *)val);
    return talloc_asprintf(NULL, "%d", *(const int *)val);
}

static void add_int64(const m_option_t *opt, void *val, double add, bool wrap)
{
    int64_t v = *(int64_t *)val;

    v = v + add;

    bool is64 = opt->type->size == sizeof(int64_t);
    int64_t nmin = is64 ? INT64_MIN : INT_MIN;
    int64_t nmax = is64 ? INT64_MAX : INT_MAX;

    int64_t min = (opt->flags & M_OPT_MIN) ? opt->min : nmin;
    int64_t max = (opt->flags & M_OPT_MAX) ? opt->max : nmax;

    if (v < min)
        v = wrap ? max : min;
    if (v > max)
        v = wrap ? min : max;

    *(int64_t *)val = v;
}

static void add_int(const m_option_t *opt, void *val, double add, bool wrap)
{
    int64_t tmp = *(int *)val;
    add_int64(opt, &tmp, add, wrap);
    *(int *)val = tmp;
}

const m_option_type_t m_option_type_int = {
    .name  = "Integer",
    .size  = sizeof(int),
    .parse = parse_int,
    .print = print_int,
    .copy  = copy_opt,
    .add = add_int,
    .clamp = clamp_int,
};

const m_option_type_t m_option_type_int64 = {
    .name  = "Integer64",
    .size  = sizeof(int64_t),
    .parse = parse_int64,
    .print = print_int,
    .copy  = copy_opt,
    .add = add_int64,
    .clamp = clamp_int64,
};

static int parse_intpair(const struct m_option *opt, struct bstr name,
                         struct bstr param, void *dst)
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
    .name  = "Int[-Int]",
    .size  = sizeof(int[2]),
    .parse = parse_intpair,
    .copy  = copy_opt,
};

static int clamp_choice(const m_option_t *opt, void *val)
{
    int v = *(int *)val;
    if ((opt->flags & M_OPT_MIN) && (opt->flags & M_OPT_MAX)) {
        if (v >= opt->min && v <= opt->max)
            return 0;
    }
    ;
    for (struct m_opt_choice_alternatives *alt = opt->priv; alt->name; alt++) {
        if (alt->value == v)
            return 0;
    }
    return M_OPT_INVALID;
}

static int parse_choice(const struct m_option *opt, struct bstr name,
                        struct bstr param, void *dst)
{
    struct m_opt_choice_alternatives *alt = opt->priv;
    for ( ; alt->name; alt++)
        if (!bstrcmp0(param, alt->name))
            break;
    if (!alt->name) {
        if (param.len == 0)
            return M_OPT_MISSING_PARAM;
        if ((opt->flags & M_OPT_MIN) && (opt->flags & M_OPT_MAX)) {
            long long val;
            if (parse_longlong(opt, name, param, &val) == 1) {
                if (dst)
                    *(int *)dst = val;
                return 1;
            }
        }
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Invalid value for option %.*s: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Valid values are:");
        for (alt = opt->priv; alt->name; alt++)
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, " %s", alt->name);
        if ((opt->flags & M_OPT_MIN) && (opt->flags & M_OPT_MAX))
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, " %g-%g", opt->min, opt->max);
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
    if ((opt->flags & M_OPT_MIN) && (opt->flags & M_OPT_MAX)) {
        if (v >= opt->min && v <= opt->max)
            return talloc_asprintf(NULL, "%d", v);
    }
    abort();
}

static void choice_get_min_max(const struct m_option *opt, int *min, int *max)
{
    assert(opt->type == &m_option_type_choice);
    *min = INT_MAX;
    *max = INT_MIN;
    for (struct m_opt_choice_alternatives *alt = opt->priv; alt->name; alt++) {
        *min = FFMIN(*min, alt->value);
        *max = FFMAX(*max, alt->value);
    }
    if ((opt->flags & M_OPT_MIN) && (opt->flags & M_OPT_MAX)) {
        *min = FFMIN(*min, opt->min);
        *max = FFMAX(*max, opt->max);
    }
}

static void check_choice(int dir, int val, bool *found, int *best, int choice)
{
    if ((dir == -1 && (!(*found) || choice > (*best)) && choice < val) ||
        (dir == +1 && (!(*found) || choice < (*best)) && choice > val))
    {
        *found = true;
        *best = choice;
    }
}

static void add_choice(const m_option_t *opt, void *val, double add, bool wrap)
{
    assert(opt->type == &m_option_type_choice);
    int dir = add > 0 ? +1 : -1;
    bool found = false;
    int ival = *(int *)val;
    int best = 0; // init. value unused

    if (fabs(add) < 0.5)
        return;

    if ((opt->flags & M_OPT_MIN) && (opt->flags & M_OPT_MAX)) {
        int newval = ival + add;
        if (ival >= opt->min && ival <= opt->max &&
            newval >= opt->min && newval <= opt->max)
        {
            found = true;
            best = newval;
        } else {
            check_choice(dir, ival, &found, &best, opt->min);
            check_choice(dir, ival, &found, &best, opt->max);
        }
    }

    for (struct m_opt_choice_alternatives *alt = opt->priv; alt->name; alt++)
        check_choice(dir, ival, &found, &best, alt->value);

    if (!found) {
        int min, max;
        choice_get_min_max(opt, &min, &max);
        best = (dir == -1) ^ wrap ? min : max;
    }

    *(int *)val = best;
}

const struct m_option_type m_option_type_choice = {
    .name  = "String",  // same as arbitrary strings in option list for now
    .size  = sizeof(int),
    .parse = parse_choice,
    .print = print_choice,
    .copy  = copy_opt,
    .add = add_choice,
    .clamp = clamp_choice,
};

// Float

#undef VAL
#define VAL(x) (*(double *)(x))

static int clamp_double(const m_option_t *opt, void *val)
{
    double v = VAL(val);
    int r = 0;
    if ((opt->flags & M_OPT_MAX) && (v > opt->max)) {
        v = opt->max;
        r = M_OPT_OUT_OF_RANGE;
    }
    if ((opt->flags & M_OPT_MIN) && (v < opt->min)) {
        v = opt->min;
        r = M_OPT_OUT_OF_RANGE;
    }
    if (!isfinite(v)) {
        v = opt->min;
        r = M_OPT_OUT_OF_RANGE;
    }
    VAL(val) = v;
    return r;
}

static int parse_double(const m_option_t *opt, struct bstr name,
                        struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct bstr rest;
    double tmp_float = bstrtod(param, &rest);

    if (bstr_eatstart0(&rest, ":") || bstr_eatstart0(&rest, "/"))
        tmp_float /= bstrtod(rest, &rest);

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

    if (!isfinite(tmp_float)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "The %.*s option must be a finite number: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    if (dst)
        VAL(dst) = tmp_float;
    return 1;
}

static char *print_double(const m_option_t *opt, const void *val)
{
    return talloc_asprintf(NULL, "%f", VAL(val));
}

static char *print_double_f3(const m_option_t *opt, const void *val)
{
    return talloc_asprintf(NULL, "%.3f", VAL(val));
}

static void add_double(const m_option_t *opt, void *val, double add, bool wrap)
{
    double v = VAL(val);

    v = v + add;

    double min = (opt->flags & M_OPT_MIN) ? opt->min : -INFINITY;
    double max = (opt->flags & M_OPT_MAX) ? opt->max : +INFINITY;

    if (v < min)
        v = wrap ? max : min;
    if (v > max)
        v = wrap ? min : max;

    VAL(val) = v;
}

const m_option_type_t m_option_type_double = {
    // double precision float or ratio (numerator[:/]denominator)
    .name  = "Double",
    .size  = sizeof(double),
    .parse = parse_double,
    .print = print_double,
    .pretty_print = print_double_f3,
    .copy  = copy_opt,
    .clamp = clamp_double,
};

#undef VAL
#define VAL(x) (*(float *)(x))

static int clamp_float(const m_option_t *opt, void *val)
{
    double tmp = VAL(val);
    int r = clamp_double(opt, &tmp);
    VAL(val) = tmp;
    return r;
}

static int parse_float(const m_option_t *opt, struct bstr name,
                       struct bstr param, void *dst)
{
    double tmp;
    int r = parse_double(opt, name, param, &tmp);
    if (r == 1 && dst)
        VAL(dst) = tmp;
    return r;
}

static char *print_float(const m_option_t *opt, const void *val)
{
    return talloc_asprintf(NULL, "%f", VAL(val));
}

static char *print_float_f3(const m_option_t *opt, const void *val)
{
    return talloc_asprintf(NULL, "%.3f", VAL(val));
}

static void add_float(const m_option_t *opt, void *val, double add, bool wrap)
{
    double tmp = VAL(val);
    add_double(opt, &tmp, add, wrap);
    VAL(val) = tmp;
}

const m_option_type_t m_option_type_float = {
    // floating point number or ratio (numerator[:/]denominator)
    .name  = "Float",
    .size  = sizeof(float),
    .parse = parse_float,
    .print = print_float,
    .pretty_print = print_float_f3,
    .copy  = copy_opt,
    .add = add_float,
    .clamp = clamp_float,
};

///////////// String

#undef VAL
#define VAL(x) (*(char **)(x))

static char *unescape_string(void *talloc_ctx, bstr str)
{
    char *res = talloc_strdup(talloc_ctx, "");
    while (str.len) {
        bstr rest;
        bool esc = bstr_split_tok(str, "\\", &str, &rest);
        res = talloc_strndup_append_buffer(res, str.start, str.len);
        if (esc) {
            if (!mp_parse_escape(&rest, &res)) {
                talloc_free(res);
                return NULL;
            }
        }
        str = rest;
    }
    return res;
}

static char *escape_string(char *str0)
{
    char *res = talloc_strdup(NULL, "");
    bstr str = bstr0(str0);
    while (str.len) {
        bstr rest;
        bool esc = bstr_split_tok(str, "\\", &str, &rest);
        res = talloc_strndup_append_buffer(res, str.start, str.len);
        if (esc)
            res = talloc_strdup_append_buffer(res, "\\\\");
        str = rest;
    }
    return res;
}

static int clamp_str(const m_option_t *opt, void *val)
{
    char *v = VAL(val);
    int len = v ? strlen(v) : 0;
    if ((opt->flags & M_OPT_MIN) && (len < opt->min))
        return M_OPT_OUT_OF_RANGE;
    if ((opt->flags & M_OPT_MAX) && (len > opt->max))
        return M_OPT_OUT_OF_RANGE;
    return 0;
}

static int parse_str(const m_option_t *opt, struct bstr name,
                     struct bstr param, void *dst)
{
    int r = 1;
    void *tmp = talloc_new(NULL);

    if (param.start == NULL) {
        r = M_OPT_MISSING_PARAM;
        goto exit;
    }

    if (opt->flags & M_OPT_PARSE_ESCAPES) {
        char *res = unescape_string(tmp, param);
        if (!res) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Parameter has broken escapes: %.*s\n", BSTR_P(param));
            r = M_OPT_INVALID;
            goto exit;
        }
        param = bstr0(res);
    }

    if ((opt->flags & M_OPT_MIN) && (param.len < opt->min)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Parameter must be >= %d chars: %.*s\n",
               (int) opt->min, BSTR_P(param));
        r = M_OPT_OUT_OF_RANGE;
        goto exit;
    }

    if ((opt->flags & M_OPT_MAX) && (param.len > opt->max)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Parameter must be <= %d chars: %.*s\n",
               (int) opt->max, BSTR_P(param));
        r = M_OPT_OUT_OF_RANGE;
        goto exit;
    }

    if (dst) {
        talloc_free(VAL(dst));
        VAL(dst) = bstrdup0(NULL, param);
    }

exit:
    talloc_free(tmp);
    return r;
}

static char *print_str(const m_option_t *opt, const void *val)
{
    bool need_escape = opt->flags & M_OPT_PARSE_ESCAPES;
    char *s = val ? VAL(val) : NULL;
    return s ? (need_escape ? escape_string(s) : talloc_strdup(NULL, s)) : NULL;
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
    .name  = "String",
    .size  = sizeof(char *),
    .flags = M_OPT_TYPE_DYNAMIC,
    .parse = parse_str,
    .print = print_str,
    .copy  = copy_str,
    .free  = free_str,
    .clamp = clamp_str,
};

//////////// String list

#undef VAL
#define VAL(x) (*(char ***)(x))

#define OP_NONE 0
#define OP_ADD 1
#define OP_PRE 2
#define OP_DEL 3
#define OP_CLR 4
#define OP_TOGGLE 5

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
    if (!dst)
        return M_OPT_PARSER_ERR;
    char **lst = VAL(dst);

    int ln;
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
                          struct bstr param, void *dst)
{
    char **res;
    int op = OP_NONE;
    int len = strlen(opt->name);
    if (opt->name[len - 1] == '*' && (name.len > len - 1)) {
        struct bstr suffix = bstr_cut(name, len - 1);
        if (bstrcmp0(suffix, "-add") == 0)
            op = OP_ADD;
        else if (bstrcmp0(suffix, "-pre") == 0)
            op = OP_PRE;
        else if (bstrcmp0(suffix, "-del") == 0)
            op = OP_DEL;
        else if (bstrcmp0(suffix, "-clr") == 0)
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
    /* A list of strings separated by ','.
     * Option with a name ending in '*' permits using the following suffixes:
     *     -add: Add the given parameters at the end of the list.
     *     -pre: Add the given parameters at the beginning of the list.
     *     -del: Remove the entry at the given indices.
     *     -clr: Clear the list.
     * e.g: -vf-add flip,mirror -vf-del 2,5
     */
    .name  = "String list",
    .size  = sizeof(char **),
    .flags = M_OPT_TYPE_DYNAMIC | M_OPT_TYPE_ALLOW_WILDCARD,
    .parse = parse_str_list,
    .print = print_str_list,
    .copy  = copy_str_list,
    .free  = free_str_list,
};


/////////////////// Print

static int parse_print(const m_option_t *opt, struct bstr name,
                       struct bstr param, void *dst)
{
    if (opt->type == CONF_TYPE_PRINT) {
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "%s", mp_gtext(opt->p));
    } else {
        char *name0 = bstrdup0(NULL, name);
        char *param0 = bstrdup0(NULL, param);
        int r = ((m_opt_func_full_t) opt->p)(opt, name0, param0);
        talloc_free(name0);
        talloc_free(param0);
        return r;
    }

    if (opt->priv == NULL)
        return M_OPT_EXIT;
    return 0;
}

const m_option_type_t m_option_type_print = {
    .name  = "Print",
    .flags = M_OPT_TYPE_OPTIONAL_PARAM,
    .parse = parse_print,
};

const m_option_type_t m_option_type_print_func_param = {
    .name  = "Print",
    .flags = M_OPT_TYPE_ALLOW_WILDCARD,
    .parse = parse_print,
};

const m_option_type_t m_option_type_print_func = {
    .name  = "Print",
    .flags = M_OPT_TYPE_ALLOW_WILDCARD | M_OPT_TYPE_OPTIONAL_PARAM,
    .parse = parse_print,
};


/////////////////////// Subconfig
#undef VAL
#define VAL(x) (*(char ***)(x))

// Read s sub-option name, or a positional sub-opt value.
// Return 0 on succes, M_OPT_ error code otherwise.
// optname is for error reporting.
static int read_subparam(bstr optname, bstr *str, bstr *out_subparam)
{
    bstr p = *str;
    bstr subparam = {0};

    if (bstr_eatstart0(&p, "\"")) {
        int optlen = bstrcspn(p, "\"");
        subparam = bstr_splice(p, 0, optlen);
        p = bstr_cut(p, optlen);
        if (!bstr_startswith0(p, "\"")) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Terminating '\"' missing for '%.*s'\n",
                   BSTR_P(optname));
            return M_OPT_INVALID;
        }
        p = bstr_cut(p, 1);
    } else if (bstr_eatstart0(&p, "[")) {
        if (!bstr_split_tok(p, "]", &subparam, &p)) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Terminating ']' missing for '%.*s'\n",
                   BSTR_P(optname));
            return M_OPT_INVALID;
        }
    } else if (bstr_eatstart0(&p, "%")) {
        int optlen = bstrtoll(p, &p, 0);
        if (!bstr_startswith0(p, "%") || (optlen > p.len - 1)) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Invalid length %d for '%.*s'\n",
                   optlen, BSTR_P(optname));
            return M_OPT_INVALID;
        }
        subparam = bstr_splice(p, 1, optlen + 1);
        p = bstr_cut(p, optlen + 1);
    } else {
        // Skip until the next character that could possibly be a meta
        // character in option parsing.
        int optlen = bstrcspn(p, ":=,\\%\"'[]");
        subparam = bstr_splice(p, 0, optlen);
        p = bstr_cut(p, optlen);
    }

    *str = p;
    *out_subparam = subparam;
    return 0;
}

// Return 0 on success, otherwise error code
// On success, set *out_name and *out_val, and advance *str
// out_val.start is NULL if there was no parameter.
// optname is for error reporting.
static int split_subconf(bstr optname, bstr *str, bstr *out_name, bstr *out_val)
{
    bstr p = *str;
    bstr subparam = {0};
    bstr subopt;
    int r = read_subparam(optname, &p, &subopt);
    if (r < 0)
        return r;
    if (bstr_eatstart0(&p, "=")) {
        r = read_subparam(subopt, &p, &subparam);
        if (r < 0)
            return r;
    }
    *str = p;
    *out_name = subopt;
    *out_val = subparam;
    return 0;
}

static int parse_subconf(const m_option_t *opt, struct bstr name,
                         struct bstr param, void *dst)
{
    int nr = 0;
    char **lst = NULL;

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct bstr p = param;

    while (p.len) {
        bstr subopt, subparam;
        int r = split_subconf(name, &p, &subopt, &subparam);
        if (r < 0)
            return r;
        if (bstr_startswith0(p, ":"))
            p = bstr_cut(p, 1);
        else if (p.len > 0) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Incorrect termination for '%.*s'\n", BSTR_P(subopt));
            return M_OPT_INVALID;
        }

        if (dst) {
            lst = talloc_realloc(NULL, lst, char *, 2 * (nr + 2));
            lst[2 * nr] = bstrto0(lst, subopt);
            lst[2 * nr + 1] = bstrto0(lst, subparam);
            memset(&lst[2 * (nr + 1)], 0, 2 * sizeof(char *));
            nr++;
        }
    }

    if (dst)
        VAL(dst) = lst;

    return 1;
}

const m_option_type_t m_option_type_subconfig = {
    // The syntax is -option opt1=foo:flag:opt2=blah
    .name = "Subconfig",
    .flags = M_OPT_TYPE_HAS_CHILD,
    .parse = parse_subconf,
};

const m_option_type_t m_option_type_subconfig_struct = {
    .name = "Subconfig",
    .flags = M_OPT_TYPE_HAS_CHILD | M_OPT_TYPE_USE_SUBSTRUCT,
    .parse = parse_subconf,
};

static int parse_color(const m_option_t *opt, struct bstr name,
                        struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    bstr val = param;
    struct m_color color = {0};

    if (bstr_eatstart0(&val, "#")) {
        // #[AA]RRGGBB
        if (val.len != 6 && val.len != 8)
            goto error;
        bool has_alpha = val.len == 8;
        uint32_t c = bstrtoll(val, &val, 16);
        if (val.len)
            goto error;
        color = (struct m_color) {
            (c >> 16) & 0xFF,
            (c >> 8) & 0xFF,
            c & 0xFF,
            has_alpha ? (c >> 24) & 0xFF : 0xFF,
        };
    } else {
        goto error;
    }

    if (dst)
        *((struct m_color *)dst) = color;

    return 1;

error:
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
           "Option %.*s: invalid color: '%.*s'\n",
           BSTR_P(name), BSTR_P(param));
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
           "Valid colors must be in the form #RRRGGBB or #AARRGGBB (in hex)\n");
    return M_OPT_INVALID;
}

const m_option_type_t m_option_type_color = {
    .name  = "Color",
    .size  = sizeof(struct m_color),
    .parse = parse_color,
};


// Parse a >=0 number starting at s. Set s to the string following the number.
// If the number ends with '%', eat that and set *out_per to true, but only
// if the number is between 0-100; if not, don't eat anything, even the number.
static bool eat_num_per(bstr *s, int *out_num, bool *out_per)
{
    bstr rest;
    long long v = bstrtoll(*s, &rest, 10);
    if (s->len == rest.len || v < INT_MIN || v > INT_MAX)
        return false;
    *out_num = v;
    *out_per = false;
    *s = rest;
    if (bstr_eatstart0(&rest, "%") && v >= 0 && v <= 100) {
        *out_per = true;
        *s = rest;
    }
    return true;
}

static bool parse_geometry_str(struct m_geometry *gm, bstr s)
{
    *gm = (struct m_geometry) { .x = INT_MIN, .y = INT_MIN };
    if (s.len == 0)
        return true;
    // Approximate grammar:
    // [W[xH]][{+-}X{+-}Y] | [X:Y]
    // (meaning: [optional] {one character of} one|alternative)
    // Every number can be followed by '%'
    int num;
    bool per;

#define READ_NUM(F, F_PER) do {         \
    if (!eat_num_per(&s, &num, &per))   \
        goto error;                     \
    gm->F = num;                        \
    gm->F_PER = per;                    \
} while(0)

#define READ_SIGN(F) do {               \
    if (bstr_eatstart0(&s, "+")) {      \
        gm->F = false;                  \
    } else if (bstr_eatstart0(&s, "-")) {\
        gm->F = true;                   \
    } else goto error;                  \
} while(0)

    if (bstrchr(s, ':') < 0) {
        gm->wh_valid = true;
        if (!bstr_startswith0(s, "+") && !bstr_startswith0(s, "-")) {
            READ_NUM(w, w_per);
            if (bstr_eatstart0(&s, "x"))
                READ_NUM(h, h_per);
        }
        if (s.len > 0) {
            gm->xy_valid = true;
            READ_SIGN(x_sign);
            READ_NUM(x, x_per);
            READ_SIGN(y_sign);
            READ_NUM(y, y_per);
        }
    } else {
        gm->xy_valid = true;
        READ_NUM(x, x_per);
        if (!bstr_eatstart0(&s, ":"))
            goto error;
        READ_NUM(y, y_per);
    }

    return s.len == 0;

error:
    return false;
}

#undef READ_NUM
#undef READ_SIGN

// xpos,ypos: position of the left upper corner
// widw,widh: width and height of the window
// scrw,scrh: width and height of the current screen
// The input parameters should be set to a centered window (default fallbacks).
void m_geometry_apply(int *xpos, int *ypos, int *widw, int *widh,
                      int scrw, int scrh, struct m_geometry *gm)
{
    if (gm->wh_valid) {
        int prew = *widw, preh = *widh;
        if (gm->w > 0)
            *widw = gm->w_per ? scrw * (gm->w / 100.0) : gm->w;
        if (gm->h > 0)
            *widh = gm->h_per ? scrh * (gm->h / 100.0) : gm->h;
        // keep aspect if the other value is not set
        double asp = (double)prew / preh;
        if (gm->w > 0 && !(gm->h > 0)) {
            *widh = *widw / asp;
        } else if (!(gm->w > 0) && gm->h > 0) {
            *widw = *widh * asp;
        }
    }

    if (gm->xy_valid) {
        if (gm->x != INT_MIN) {
            *xpos = gm->x;
            if (gm->x_per)
                *xpos = (scrw - *widw) * (*xpos / 100.0);
            if (gm->x_sign)
                *xpos = scrw - *widw - *xpos;
        }
        if (gm->y != INT_MIN) {
            *ypos = gm->y;
            if (gm->y_per)
                *ypos = (scrh - *widh) * (*ypos / 100.0);
            if (gm->y_sign)
                *ypos = scrh - *widh - *ypos;
        }
    }
}

static int parse_geometry(const m_option_t *opt, struct bstr name,
                          struct bstr param, void *dst)
{
    struct m_geometry gm;
    if (!parse_geometry_str(&gm, param))
        goto error;

    if (dst)
        *((struct m_geometry *)dst) = gm;

    return 1;

error:
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: invalid geometry: '%.*s'\n",
           BSTR_P(name), BSTR_P(param));
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
           "Valid format: [W[%%][xH[%%]]][{+-}X[%%]{+-}Y[%%]] | [X[%%]:Y[%%]]\n");
    return M_OPT_INVALID;
}

const m_option_type_t m_option_type_geometry = {
    .name  = "Window geometry",
    .size  = sizeof(struct m_geometry),
    .parse = parse_geometry,
};

static int parse_size_box(const m_option_t *opt, struct bstr name,
                          struct bstr param, void *dst)
{
    struct m_geometry gm;
    if (!parse_geometry_str(&gm, param))
        goto error;

    if (gm.xy_valid)
        goto error;

    if (dst)
        *((struct m_geometry *)dst) = gm;

    return 1;

error:
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: invalid size: '%.*s'\n",
           BSTR_P(name), BSTR_P(param));
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
           "Valid format: W[%%][xH[%%]] or empty string\n");
    return M_OPT_INVALID;
}

const m_option_type_t m_option_type_size_box = {
    .name  = "Window size",
    .size  = sizeof(struct m_geometry),
    .parse = parse_size_box,
};


#include "video/img_format.h"

static int parse_imgfmt(const m_option_t *opt, struct bstr name,
                        struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    if (!bstrcmp0(param, "help")) {
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "Available formats:");
        for (int i = 0; mp_imgfmt_list[i].name; i++)
            mp_msg(MSGT_CFGPARSER, MSGL_INFO, " %s", mp_imgfmt_list[i].name);
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
        return M_OPT_EXIT - 1;
    }

    unsigned int fmt = mp_imgfmt_from_name(param, false);
    if (!fmt) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Option %.*s: unknown format name: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    if (dst)
        *((uint32_t *)dst) = fmt;

    return 1;
}

const m_option_type_t m_option_type_imgfmt = {
    // Please report any missing colorspaces
    .name  = "Image format",
    .size  = sizeof(uint32_t),
    .parse = parse_imgfmt,
    .copy  = copy_opt,
};

static int parse_fourcc(const m_option_t *opt, struct bstr name,
                        struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    unsigned int value;

    if (param.len == 4) {
        uint8_t *s = param.start;
        value = s[0] | (s[1] << 8) | (s[2] << 16) | (s[3] << 24);
    } else {
        bstr rest;
        value = bstrtoll(param, &rest, 16);
        if (rest.len != 0) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: invalid FourCC: '%.*s'\n",
                   BSTR_P(name), BSTR_P(param));
            return M_OPT_INVALID;
        }
    }

    if (dst)
        *((unsigned int *)dst) = value;

    return 1;
}

const m_option_type_t m_option_type_fourcc = {
    .name  = "FourCC",
    .size  = sizeof(unsigned int),
    .parse = parse_fourcc,
    .copy  = copy_opt,
};

#include "audio/format.h"

static int parse_afmt(const m_option_t *opt, struct bstr name,
                      struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    if (!bstrcmp0(param, "help")) {
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "Available formats:");
        for (int i = 0; af_fmtstr_table[i].name; i++)
            mp_msg(MSGT_CFGPARSER, MSGL_INFO, " %s", af_fmtstr_table[i].name);
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
        return M_OPT_EXIT - 1;
    }

    int fmt = af_str2fmt_short(param);
    if (fmt == -1) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Option %.*s: unknown format name: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    if (dst)
        *((uint32_t *)dst) = fmt;

    return 1;
}

const m_option_type_t m_option_type_afmt = {
    // Please report any missing formats
    .name  = "Audio format",
    .size  = sizeof(uint32_t),
    .parse = parse_afmt,
    .copy  = copy_opt,
};

#include "audio/chmap.h"

static int parse_chmap(const m_option_t *opt, struct bstr name,
                       struct bstr param, void *dst)
{
    // min>0: at least min channels, min=0: empty ok, min=-1: invalid ok
    int min_ch = (opt->flags & M_OPT_MIN) ? opt->min : 1;

    if (bstr_equals0(param, "help")) {
        mp_chmap_print_help(MSGT_CFGPARSER, MSGL_INFO);
        return M_OPT_EXIT - 1;
    }

    if (param.len == 0 && min_ch >= 1)
        return M_OPT_MISSING_PARAM;

    struct mp_chmap res = {0};
    if (!mp_chmap_from_str(&res, param)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Error parsing channel layout: %.*s\n", BSTR_P(param));
        return M_OPT_INVALID;
    }

    if ((min_ch > 0 && !mp_chmap_is_valid(&res)) ||
        (min_ch >= 0 && mp_chmap_is_empty(&res)))
    {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               "Invalid channel layout: %.*s\n", BSTR_P(param));
        return M_OPT_INVALID;
    }

    if (dst)
        *(struct mp_chmap *)dst = res;

    return 1;
}

const m_option_type_t m_option_type_chmap = {
    .name  = "Audio channels or channel map",
    .size  = sizeof(struct mp_chmap *),
    .parse = parse_chmap,
    .copy  = copy_opt,
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
    if (!isfinite(*time))
        return 0;
    return len;
}


static int parse_time(const m_option_t *opt, struct bstr name,
                      struct bstr param, void *dst)
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

static char *pretty_print_time(const m_option_t *opt, const void *val)
{
    return mp_format_time(*(double *)val, false);
}

const m_option_type_t m_option_type_time = {
    .name  = "Time",
    .size  = sizeof(double),
    .parse = parse_time,
    .print = print_double,
    .pretty_print = pretty_print_time,
    .copy  = copy_opt,
    .add = add_double,
    .clamp = clamp_double,
};


// Relative time

static int parse_rel_time(const m_option_t *opt, struct bstr name,
                          struct bstr param, void *dst)
{
    struct m_rel_time t = {0};

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    // Percent pos
    if (bstr_endswith0(param, "%")) {
        double percent = bstrtod(bstr_splice(param, 0, -1), &param);
        if (param.len == 0 && percent >= 0 && percent <= 100) {
            t.type = REL_TIME_PERCENT;
            t.pos = percent;
            goto out;
        }
    }

    // Chapter pos
    if (bstr_startswith0(param, "#")) {
        int chapter = bstrtoll(bstr_cut(param, 1), &param, 10);
        if (param.len == 0 && chapter >= 1) {
            t.type = REL_TIME_CHAPTER;
            t.pos = chapter - 1;
            goto out;
        }
    }

    bool sign = bstr_eatstart0(&param, "-");
    double time;
    if (parse_timestring(param, &time, 0)) {
        t.type = sign ? REL_TIME_NEGATIVE : REL_TIME_ABSOLUTE;
        t.pos = time;
        goto out;
    }

    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
           "Option %.*s: invalid time or position: '%.*s'\n",
           BSTR_P(name), BSTR_P(param));
    return M_OPT_INVALID;

out:
    if (dst)
        *(struct m_rel_time *)dst = t;
    return 1;
}

const m_option_type_t m_option_type_rel_time = {
    .name  = "Relative time or percent position",
    .size  = sizeof(struct m_rel_time),
    .parse = parse_rel_time,
    .copy  = copy_opt,
};


//// Objects (i.e. filters, etc) settings

#include "core/m_struct.h"

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

static void obj_setting_free(m_obj_settings_t *item)
{
    talloc_free(item->name);
    talloc_free(item->label);
    free_str_list(&(item->attribs));
}

// If at least one item has a label, compare labels only - otherwise ignore them.
static bool obj_setting_equals(m_obj_settings_t *a, m_obj_settings_t *b)
{
    bstr la = bstr0(a->label), lb = bstr0(b->label);
    if (la.len || lb.len)
        return bstr_equals(la, lb);
    if (strcmp(a->name, b->name) != 0)
        return false;

    int a_attr_count = 0;
    while (a->attribs && a->attribs[a_attr_count])
        a_attr_count++;
    int b_attr_count = 0;
    while (b->attribs && b->attribs[b_attr_count])
        b_attr_count++;
    if (a_attr_count != b_attr_count)
        return false;
    for (int n = 0; n < a_attr_count; n++) {
        if (strcmp(a->attribs[n], b->attribs[n]) != 0)
            return false;
    }
    return true;
}

static int obj_settings_list_num_items(m_obj_settings_t *obj_list)
{
    int num = 0;
    while (obj_list && obj_list[num].name)
        num++;
    return num;
}

static void obj_settings_list_del_at(m_obj_settings_t **p_obj_list, int idx)
{
    m_obj_settings_t *obj_list = *p_obj_list;
    int num = obj_settings_list_num_items(obj_list);

    assert(idx >= 0 && idx < num);

    obj_setting_free(&obj_list[idx]);

    // Note: the NULL-terminating element is moved down as part of this
    memmove(&obj_list[idx], &obj_list[idx + 1],
            sizeof(m_obj_settings_t) * (num - idx));

    *p_obj_list = talloc_realloc(NULL, obj_list, struct m_obj_settings, num);
}

// Insert such that *p_obj_list[idx] is set to item.
// If idx < 0, set idx = count + idx + 1 (i.e. -1 inserts it as last element).
// Memory referenced by *item is not copied.
static void obj_settings_list_insert_at(m_obj_settings_t **p_obj_list, int idx,
                                        m_obj_settings_t *item)
{
    int num = obj_settings_list_num_items(*p_obj_list);
    if (idx < 0)
        idx = num + idx + 1;
    assert(idx >= 0 && idx <= num);
    *p_obj_list = talloc_realloc(NULL, *p_obj_list, struct m_obj_settings,
                                 num + 2);
    memmove(*p_obj_list + idx + 1, *p_obj_list + idx,
            (num - idx) * sizeof(m_obj_settings_t));
    (*p_obj_list)[idx] = *item;
    (*p_obj_list)[num + 1] = (m_obj_settings_t){0};
}

static int obj_settings_list_find_by_label(m_obj_settings_t *obj_list,
                                           bstr label)
{
    for (int n = 0; obj_list && obj_list[n].name; n++) {
        if (label.len && bstr_equals0(label, obj_list[n].label))
            return n;
    }
    return -1;
}

static int obj_settings_list_find_by_label0(m_obj_settings_t *obj_list,
                                            const char *label)
{
    return obj_settings_list_find_by_label(obj_list, bstr0(label));
}

static int obj_settings_find_by_content(m_obj_settings_t *obj_list,
                                        m_obj_settings_t *item)
{
    for (int n = 0; obj_list && obj_list[n].name; n++) {
        if (obj_setting_equals(&obj_list[n], item))
            return n;
    }
    return -1;
}

static void free_obj_settings_list(void *dst)
{
    int n;
    m_obj_settings_t *d;

    if (!dst || !VAL(dst))
        return;

    d = VAL(dst);
    for (n = 0; d[n].name; n++)
        obj_setting_free(&d[n]);
    talloc_free(d);
    VAL(dst) = NULL;
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
        d[n].label = talloc_strdup(NULL, s[n].label);
        d[n].attribs = NULL;
        copy_str_list(NULL, &(d[n].attribs), &(s[n].attribs));
    }
    d[n].name = NULL;
    d[n].label = NULL;
    d[n].attribs = NULL;
    VAL(dst) = d;
}

// Consider -vf a=b=c:d=e. This verifies "b"="c" and "d"="e" and that the
// option names/values are correct. Try to determine whether an option
// without '=' sets a flag, or whether it's a positional argument.
static int get_obj_param(bstr opt_name, bstr obj_name, const m_struct_t *desc,
                         bstr name, bstr val, int *nold, int oldmax,
                         bstr *out_name, bstr *out_val)
{
    const m_option_t *opt = m_option_list_findb(desc->fields, name);
    int r;

    // va.start != NULL => of the form name=val (not positional)
    // If it's just "name", and the associated option exists and is a flag,
    // don't accept it as positional argument.
    if (val.start || (opt && m_option_required_params(opt) == 0)) {
        if (!opt) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: %.*s doesn't have a %.*s parameter.\n",
                   BSTR_P(opt_name), BSTR_P(obj_name), BSTR_P(name));
            return M_OPT_UNKNOWN;
        }
        r = m_option_parse(opt, name, val, NULL);
        if (r < 0) {
            if (r > M_OPT_EXIT)
                mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: "
                       "Error while parsing %.*s parameter %.*s (%.*s)\n",
                       BSTR_P(opt_name), BSTR_P(obj_name), BSTR_P(name),
                       BSTR_P(val));
            return r;
        }
        *out_name = name;
        *out_val = val;
        return 1;
    } else {
        val = name;
        // positional fields
        if (val.len == 0) { // Empty field, count it and go on
            (*nold)++;
            return 0;
        }
        if ((*nold) >= oldmax) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: %.*s has only %d params, so you can't give more than %d unnamed params.\n",
                   BSTR_P(opt_name), BSTR_P(obj_name), oldmax, oldmax);
            return M_OPT_OUT_OF_RANGE;
        }
        opt = &desc->fields[(*nold)];
        r = m_option_parse(opt, bstr0(opt->name), val, NULL);
        if (r < 0) {
            if (r > M_OPT_EXIT)
                mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: "
                       "Error while parsing %.*s parameter %s (%.*s)\n",
                       BSTR_P(opt_name), BSTR_P(obj_name), opt->name,
                       BSTR_P(val));
            return r;
        }
        *out_name = bstr0(opt->name);
        *out_val = val;
        (*nold)++;
        return 1;
    }
}

// Consider -vf a=b:c:d. This parses "b:c:d" into name/value pairs, stored as
// linear array in *_ret. In particular, desc contains what options a the
// object takes, and verifies the option values as well.
static int get_obj_params(struct bstr opt_name, struct bstr name,
                          struct bstr *pstr, const m_struct_t *desc,
                          char ***ret)
{
    int n = 0, nold = 0, nopts;
    char **args = NULL;
    int num_args = 0;
    int r = 1;

    for (nopts = 0; desc->fields[nopts].name; nopts++)
        /* NOP */;

    while (pstr->len > 0) {
        bstr fname, fval;
        r = split_subconf(opt_name, pstr, &fname, &fval);
        if (r < 0)
            goto exit;
        if (bstr_equals0(fname, "help"))
            goto print_help;
        r = get_obj_param(opt_name, name, desc, fname, fval, &nold, nopts,
                          &fname, &fval);
        if (r < 0)
            goto exit;

        if (r > 0 && ret) {
            MP_TARRAY_APPEND(NULL, args, num_args, bstrto0(NULL, fname));
            MP_TARRAY_APPEND(NULL, args, num_args, bstrto0(NULL, fval));
        }

        if (!bstr_eatstart0(pstr, ":"))
            break;
    }

    if (ret) {
        if (num_args > 0) {
            for (int n = 0; n < 2; n++)
                MP_TARRAY_APPEND(NULL, args, num_args, NULL);
            *ret = args;
            args = NULL;
        } else {
            *ret = NULL;
        }
    }

exit:
    return r;

print_help: ;
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

// Characters which may appear in a filter name
#define NAMECH "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"

// Parse one item, e.g. -vf a=b:c:d,e=f:g => parse a=b:c:d into "a" and "b:c:d"
static int parse_obj_settings(struct bstr opt, struct bstr *pstr,
                              const m_obj_list_t *list,
                              m_obj_settings_t **_ret)
{
    int r;
    char **plist = NULL;
    const m_struct_t *desc;
    bstr label = {0};

    if (bstr_eatstart0(pstr, "@")) {
        if (!bstr_split_tok(*pstr, ":", &label, pstr)) {
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: ':' expected after label.\n", BSTR_P(opt));
            return M_OPT_INVALID;
        }
    }

    bool has_param = false;
    int idx = bstrspn(*pstr, NAMECH);
    bstr str = bstr_splice(*pstr, 0, idx);
    *pstr = bstr_cut(*pstr, idx);
    if (bstr_eatstart0(pstr, "="))
        has_param = true;

    if (!find_obj_desc(str, list, &desc)) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %.*s: %.*s doesn't exist.\n",
               BSTR_P(opt), BSTR_P(str));
        return M_OPT_INVALID;
    }

    if (has_param) {
        if (!desc) {
            // Should perhaps be parsed as escape-able string. But this is a
            // compatibility path, so it's not worth the trouble.
            int next = bstrcspn(*pstr, ",");
            bstr param = bstr_splice(*pstr, 0, next);
            *pstr = bstr_cut(*pstr, next);
            if (!bstrcmp0(param, "help")) {
                mp_msg(MSGT_CFGPARSER, MSGL_INFO,
                       "Option %.*s: %.*s have no option description.\n",
                       BSTR_P(opt), BSTR_P(str));
                return M_OPT_EXIT - 1;
            }
            if (_ret) {
                plist = talloc_zero_array(NULL, char *, 4);
                plist[0] = talloc_strdup(NULL, "_oldargs_");
                plist[1] = bstrto0(NULL, param);
            }
        } else if (desc) {
            r = get_obj_params(opt, str, pstr, desc, _ret ? &plist : NULL);
            if (r < 0)
                return r;
        }
    }
    if (!_ret)
        return 1;

    m_obj_settings_t item = {
        .name = bstrto0(NULL, str),
        .label = bstrdup0(NULL, label),
        .attribs = plist,
    };
    obj_settings_list_insert_at(_ret, -1, &item);
    return 1;
}

// Parse a single entry for -vf-del (return 0 if not applicable)
// mark_del is bounded by the number of items in dst
static int parse_obj_settings_del(struct bstr opt_name, struct bstr *param,
                                  void *dst, bool *mark_del)
{
    bstr s = *param;
    if (bstr_eatstart0(&s, "@")) {
        // '@name:' -> parse as normal filter entry
        // '@name,' or '@name<end>' -> parse here
        int idx = bstrspn(s, NAMECH);
        bstr label = bstr_splice(s, 0, idx);
        s = bstr_cut(s, idx);
        if (bstr_startswith0(s, ":"))
            return 0;
        if (dst) {
            int label_index = obj_settings_list_find_by_label(VAL(dst), label);
            if (label_index >= 0) {
                mark_del[label_index] = true;
            } else {
                mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                        "Option %.*s: item label @%.*s not found.\n",
                        BSTR_P(opt_name), BSTR_P(label));
            }
        }
        *param = s;
        return 1;
    }

    bstr rest;
    long long id = bstrtoll(s, &rest, 0);
    if (rest.len == s.len)
        return 0;

    if (dst) {
        int num = obj_settings_list_num_items(VAL(dst));
        if (id < 0)
            id = num + id;

        if (id >= 0 && id < num) {
            mark_del[id] = true;
        } else {
            mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                    "Option %.*s: Index %lld is out of range.\n",
                    BSTR_P(opt_name), id);
        }
    }

    *param = rest;
    return 1;
}

static int parse_obj_settings_list(const m_option_t *opt, struct bstr name,
                                   struct bstr param, void *dst)
{
    int len = strlen(opt->name);
    m_obj_settings_t *res = NULL;
    int op = OP_NONE;
    bool *mark_del = NULL;
    int num_items = obj_settings_list_num_items(dst ? VAL(dst) : 0);

    assert(opt->priv);

    if (opt->name[len - 1] == '*' && (name.len > len - 1)) {
        struct bstr suffix = bstr_cut(name, len - 1);
        if (bstrcmp0(suffix, "-add") == 0)
            op = OP_ADD;
        else if (bstrcmp0(suffix, "-set") == 0)
            op = OP_NONE;
        else if (bstrcmp0(suffix, "-pre") == 0)
            op = OP_PRE;
        else if (bstrcmp0(suffix, "-del") == 0)
            op = OP_DEL;
        else if (bstrcmp0(suffix, "-clr") == 0)
            op = OP_CLR;
        else if (bstrcmp0(suffix, "-toggle") == 0)
            op = OP_TOGGLE;
        else {
            char prefix[len];
            strncpy(prefix, opt->name, len - 1);
            prefix[len - 1] = '\0';
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "Option %.*s: unknown postfix %.*s\n"
                   "Supported postfixes are:\n"
                   "  %s-set\n"
                   " Overwrite the old list with the given list\n\n"
                   "  %s-add\n"
                   " Append the given list to the current list\n\n"
                   "  %s-pre\n"
                   " Prepend the given list to the current list\n\n"
                   "  %s-del x,y,...\n"
                   " Remove the given elements. Take the list element index (starting from 0).\n"
                   " Negative index can be used (i.e. -1 is the last element).\n"
                   " Filter names work as well.\n\n"
                   "  %s-clr\n"
                   " Clear the current list.\n",
                   BSTR_P(name), BSTR_P(suffix), prefix, prefix, prefix, prefix, prefix);

            return M_OPT_UNKNOWN;
        }
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

    if (op == OP_CLR) {
        if (dst)
            free_obj_settings_list(dst);
        return 0;
    } else if (op == OP_DEL) {
        mark_del = talloc_zero_array(NULL, bool, num_items + 1);
    }

    if (op != OP_NONE && param.len == 0)
        return M_OPT_MISSING_PARAM;

    while (param.len > 0) {
        int r = 0;
        if (op == OP_DEL)
            r = parse_obj_settings_del(name, &param, dst, mark_del);
        if (r == 0) {
            r = parse_obj_settings(name, &param, opt->priv,
                                   dst ? &res : NULL);
        }
        if (r < 0)
            return r;
        const char sep[2] = {OPTION_LIST_SEPARATOR, 0};
        if (param.len > 0 && !bstr_eatstart0(&param, sep))
            return M_OPT_INVALID;
    }

    if (dst) {
        m_obj_settings_t *list = VAL(dst);
        if (op == OP_PRE) {
            int prepend_counter = 0;
            for (int n = 0; res && res[n].name; n++) {
                int label = obj_settings_list_find_by_label0(list, res[n].label);
                if (label < 0) {
                    obj_settings_list_insert_at(&list, prepend_counter, &res[n]);
                    prepend_counter++;
                } else {
                    // Prefer replacement semantics, instead of actually
                    // prepending.
                    obj_setting_free(&list[label]);
                    list[label] = res[n];
                }
            }
            talloc_free(res);
        } else if (op == OP_ADD) {
            for (int n = 0; res && res[n].name; n++) {
                int label = obj_settings_list_find_by_label0(list, res[n].label);
                if (label < 0) {
                    obj_settings_list_insert_at(&list, -1, &res[n]);
                } else {
                    // Prefer replacement semantics, instead of actually
                    // appending.
                    obj_setting_free(&list[label]);
                    list[label] = res[n];
                }
            }
            talloc_free(res);
        } else if (op == OP_TOGGLE) {
            for (int n = 0; res && res[n].name; n++) {
                int found = obj_settings_find_by_content(list, &res[n]);
                if (found < 0) {
                    obj_settings_list_insert_at(&list, -1, &res[n]);
                } else {
                    obj_settings_list_del_at(&list, found);
                    obj_setting_free(&res[n]);
                }
            }
            talloc_free(res);
        } else if (op == OP_DEL) {
            for (int n = num_items - 1; n >= 0; n--) {
                if (mark_del[n])
                    obj_settings_list_del_at(&list, n);
            }
            for (int n = 0; res && res[n].name; n++) {
                int found = obj_settings_find_by_content(list, &res[n]);
                if (found < 0) {
                    mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                           "Option %.*s: Item bot found\n", BSTR_P(name));
                } else {
                    obj_settings_list_del_at(&list, found);
                }
            }
            free_obj_settings_list(&res);
        } else {
            assert(op == OP_NONE);
            free_obj_settings_list(&list);
            list = res;
        }
        VAL(dst) = list;
    }

    talloc_free(mark_del);
    return 1;
}

const m_option_type_t m_option_type_obj_settings_list = {
    .name  = "Object settings list",
    .size  = sizeof(m_obj_settings_t *),
    .flags = M_OPT_TYPE_DYNAMIC | M_OPT_TYPE_ALLOW_WILDCARD,
    .parse = parse_obj_settings_list,
    .copy  = copy_obj_settings_list,
    .free  = free_obj_settings_list,
};


static int parse_custom_url(const m_option_t *opt, struct bstr name,
                            struct bstr url, void *dst)
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
        struct bstr userpass = bstr_splice(hostpart, 0, idx);
        hostpart = bstr_cut(hostpart, idx + 1);
        // We got something, at least a username...
        if (!m_option_list_find(desc->fields, "username")) {
            mp_msg(MSGT_CFGPARSER, MSGL_WARN,
                   "Option %.*s: This URL doesn't have a username part.\n",
                   BSTR_P(name));
            // skip
        } else {
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
                r = m_struct_set(desc, dst, "port", bstr0(tmp));
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
                r = m_struct_set(desc, dst, "filename", bstr0(fname));
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
    .name  = "Custom URL",
    .parse = parse_custom_url,
};
