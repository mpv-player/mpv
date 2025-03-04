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

/// \file
/// \ingroup Options

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

#include <libavutil/common.h>

#include "mpv/client.h"
#include "player/client.h"

#include "mpv_talloc.h"
#include "common/common.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "misc/json.h"
#include "misc/node.h"
#include "m_option.h"
#include "m_config_frontend.h"

#if HAVE_DOS_PATHS
#define OPTION_PATH_SEPARATOR ';'
#else
#define OPTION_PATH_SEPARATOR ':'
#endif

const char m_option_path_separator = OPTION_PATH_SEPARATOR;

// For integer types: since min/max are floats and may not be able to represent
// the real min/max, and since opt.min/.max may use +/-INFINITY, some care has
// to be taken. (Also tricky rounding.)
#define OPT_INT_MIN(opt, T, Tm) ((opt)->min < (opt)->max \
    ? ((opt)->min <= (double)(Tm) ? (Tm) : (T)((opt)->min)) : (Tm))
#define OPT_INT_MAX(opt, T, Tm) ((opt)->min < (opt)->max \
    ? ((opt)->max >= (double)(Tm) ? (Tm) : (T)((opt)->max)) : (Tm))

int m_option_parse(struct mp_log *log, const m_option_t *opt,
                   struct bstr name, struct bstr param, void *dst)
{
    int r = M_OPT_INVALID;
    if (bstr_equals0(param, "help") && opt->help) {
        r = opt->help(log, opt, name);
        if (r < 0)
            return r;
    }

    r = opt->type->parse(log, opt, name, param, dst);
    if (r < 0)
        return r;

    if (opt->validate) {
        r = opt->validate(log, opt, name, dst);
        if (r < 0) {
            if (opt->type->free)
                opt->type->free(dst);
            return r;
        }
    }
    return 1;
}

char *m_option_strerror(int code)
{
    switch (code) {
    case M_OPT_UNKNOWN:
        return "option not found";
    case M_OPT_MISSING_PARAM:
        return "option requires parameter";
    case M_OPT_INVALID:
        return "option parameter could not be parsed";
    case M_OPT_OUT_OF_RANGE:
        return "parameter is outside values allowed for option";
    case M_OPT_DISALLOW_PARAM:
        return "option doesn't take a parameter";
    default:
        return "parser error";
    }
}

int m_option_required_params(const m_option_t *opt)
{
    if (opt->type->flags & M_OPT_TYPE_OPTIONAL_PARAM)
        return 0;
    if (opt->flags & M_OPT_OPTIONAL_PARAM)
        return 0;
    if (opt->type == &m_option_type_choice) {
        const struct m_opt_choice_alternatives *alt;
        for (alt = opt->priv; alt->name; alt++) {
            if (strcmp(alt->name, "yes") == 0)
                return 0;
        }
    }
    return 1;
}

int m_option_set_node_or_string(struct mp_log *log, const m_option_t *opt,
                                const char *name, void *dst, struct mpv_node *src)
{
    if (src->format == MPV_FORMAT_STRING) {
        // The af and vf option unfortunately require this, because the
        // option name includes the "action".
        bstr optname = bstr0(name), a, b;
        if (bstr_split_tok(optname, "/", &a, &b))
            optname = b;
        return m_option_parse(log, opt, optname, bstr0(src->u.string), dst);
    } else {
        return m_option_set_node(opt, dst, src);
    }
}

// Default function that just does a memcpy

static void copy_opt(const m_option_t *opt, void *dst, const void *src)
{
    if (dst && src)
        memcpy(dst, src, opt->type->size);
}

// Bool

#define VAL(x) (*(bool *)(x))

static int parse_bool(struct mp_log *log, const m_option_t *opt,
                      struct bstr name, struct bstr param, void *dst)
{
    if (bstr_equals0(param, "yes") || !param.len) {
        if (dst)
            VAL(dst) = 1;
        return 1;
    }
    if (bstr_equals0(param, "no")) {
        if (dst)
            VAL(dst) = 0;
        return 1;
    }
    bool is_help = bstr_equals0(param, "help");
    if (is_help) {
        mp_info(log, "Valid values for %.*s flag are:\n", BSTR_P(name));
    } else {
        mp_fatal(log, "Invalid parameter for %.*s flag: %.*s\n",
                 BSTR_P(name), BSTR_P(param));
        mp_info(log, "Valid values are:\n");
    }
    mp_info(log, "    yes\n");
    mp_info(log, "    no\n");
    mp_info(log, "    (passing nothing)\n");
    return is_help ? M_OPT_EXIT : M_OPT_INVALID;
}

static char *print_bool(const m_option_t *opt, const void *val)
{
    return talloc_strdup(NULL, VAL(val) ? "yes" : "no");
}

static void add_bool(const m_option_t *opt, void *val, double add, bool wrap)
{
    if (fabs(add) < 0.5)
        return;
    bool state = !!VAL(val);
    state = wrap ? !state : add > 0;
    VAL(val) = state ? 1 : 0;
}

static int bool_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    if (src->format != MPV_FORMAT_FLAG)
        return M_OPT_UNKNOWN;
    VAL(dst) = !!src->u.flag;
    return 1;
}

static int bool_get(const m_option_t *opt, void *ta_parent,
                    struct mpv_node *dst, void *src)
{
    dst->format = MPV_FORMAT_FLAG;
    dst->u.flag = !!VAL(src);
    return 1;
}

static bool bool_equal(const m_option_t *opt, void *a, void *b)
{
    return VAL(a) == VAL(b);
}

const m_option_type_t m_option_type_bool = {
    .name  = "Flag", // same as m_option_type_flag; transparent to user
    .size  = sizeof(bool),
    .flags = M_OPT_TYPE_OPTIONAL_PARAM | M_OPT_TYPE_CHOICE,
    .parse = parse_bool,
    .print = print_bool,
    .copy  = copy_opt,
    .add   = add_bool,
    .set   = bool_set,
    .get   = bool_get,
    .equal = bool_equal,
};

#undef VAL

// Flag

#define VAL(x) (*(int *)(x))

static int parse_flag(struct mp_log *log, const m_option_t *opt,
                      struct bstr name, struct bstr param, void *dst)
{
    bool bdst = false;
    int r = parse_bool(log, opt, name, param, &bdst);
    if (dst)
        VAL(dst) = bdst;
    return r;
}

static char *print_flag(const m_option_t *opt, const void *val)
{
    return print_bool(opt, &(bool){VAL(val)});
}

static void add_flag(const m_option_t *opt, void *val, double add, bool wrap)
{
    bool bval = VAL(val);
    add_bool(opt, &bval, add, wrap);
    VAL(val) = bval;
}

static int flag_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    bool bdst = false;
    int r = bool_set(opt, &bdst, src);
    if (r >= 0)
        VAL(dst) = bdst;
    return r;
}

static int flag_get(const m_option_t *opt, void *ta_parent,
                    struct mpv_node *dst, void *src)
{
    return bool_get(opt, ta_parent, dst, &(bool){VAL(src)});
}

static bool flag_equal(const m_option_t *opt, void *a, void *b)
{
    return VAL(a) == VAL(b);
}

// Only exists for libmpv interopability and should not be used anywhere.
const m_option_type_t m_option_type_flag = {
    // need yes or no in config files
    .name  = "Flag",
    .size  = sizeof(int),
    .flags = M_OPT_TYPE_OPTIONAL_PARAM | M_OPT_TYPE_CHOICE,
    .parse = parse_flag,
    .print = print_flag,
    .copy  = copy_opt,
    .add   = add_flag,
    .set   = flag_set,
    .get   = flag_get,
    .equal = flag_equal,
};

// Integer

#undef VAL

static int clamp_longlong(const m_option_t *opt, long long i_min, long long i_max,
                          void *val)
{
    long long v = *(long long *)val;
    int r = 0;
    long long min = OPT_INT_MIN(opt, long long, i_min);
    long long max = OPT_INT_MAX(opt, long long, i_max);
    if (v > max) {
        v = max;
        r = M_OPT_OUT_OF_RANGE;
    }
    if (v < min) {
        v = min;
        r = M_OPT_OUT_OF_RANGE;
    }
    *(long long *)val = v;
    return r;
}

static int parse_longlong(struct mp_log *log, const m_option_t *opt,
                          long long i_min, long long i_max,
                          struct bstr name, struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct bstr rest;
    long long tmp_int = bstrtoll(param, &rest, 10);
    if (rest.len)
        tmp_int = bstrtoll(param, &rest, 0);
    if (rest.len) {
        mp_err(log, "The %.*s option must be an integer: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    long long min = OPT_INT_MIN(opt, long long, i_min);
    if (tmp_int < min) {
        mp_err(log, "The %.*s option must be >= %lld: %.*s\n",
               BSTR_P(name), min, BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    long long max = OPT_INT_MAX(opt, long long, i_max);
    if (tmp_int > max) {
        mp_err(log, "The %.*s option must be <= %lld: %.*s\n",
               BSTR_P(name), max, BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    if (dst)
        *(long long *)dst = tmp_int;

    return 1;
}

static int clamp_int64(const m_option_t *opt, void *val)
{
    long long tmp = *(int64_t *)val;
    int r = clamp_longlong(opt, INT64_MIN, INT64_MAX, &tmp);
    *(int64_t *)val = tmp;
    return r;
}

static int parse_int(struct mp_log *log, const m_option_t *opt,
                     struct bstr name, struct bstr param, void *dst)
{
    long long tmp;
    int r = parse_longlong(log, opt, INT_MIN, INT_MAX, name, param, &tmp);
    if (r >= 0 && dst)
        *(int *)dst = tmp;
    return r;
}

static int parse_int64(struct mp_log *log, const m_option_t *opt,
                       struct bstr name, struct bstr param, void *dst)
{
    long long tmp;
    int r = parse_longlong(log, opt, INT64_MIN, INT64_MAX, name, param, &tmp);
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

    clamp_int64(opt, &v);

    v = v + add;

    bool is64 = opt->type->size == sizeof(int64_t);
    int64_t nmin = is64 ? INT64_MIN : INT_MIN;
    int64_t nmax = is64 ? INT64_MAX : INT_MAX;

    int64_t min = OPT_INT_MIN(opt, int64_t, nmin);
    int64_t max = OPT_INT_MAX(opt, int64_t, nmax);

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

static void multiply_int64(const m_option_t *opt, void *val, double f)
{
    double v = *(int64_t *)val * f;
    int64_t iv = v;
    if (v < INT64_MIN)
        iv = INT64_MIN;
    if (v >= (double)INT64_MAX)
        iv = INT64_MAX;
    *(int64_t *)val = iv;
    clamp_int64(opt, val);
}

static void multiply_int(const m_option_t *opt, void *val, double f)
{
    int64_t tmp = *(int *)val;
    multiply_int64(opt, &tmp, f);
    *(int *)val = MPCLAMP(tmp, INT_MIN, INT_MAX);
}

static int int64_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    if (src->format != MPV_FORMAT_INT64)
        return M_OPT_UNKNOWN;
    int64_t val = src->u.int64;
    if (val < OPT_INT_MIN(opt, int64_t, INT64_MIN))
        return M_OPT_OUT_OF_RANGE;
    if (val > OPT_INT_MAX(opt, int64_t, INT64_MAX))
        return M_OPT_OUT_OF_RANGE;
    *(int64_t *)dst = val;
    return 1;
}

static int int_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    int64_t val;
    int r = int64_set(opt, &val, src);
    if (r >= 0) {
        if (val < INT_MIN || val > INT_MAX)
            return M_OPT_OUT_OF_RANGE;
        *(int *)dst = val;
    }
    return r;
}

static int int64_get(const m_option_t *opt, void *ta_parent,
                     struct mpv_node *dst, void *src)
{
    dst->format = MPV_FORMAT_INT64;
    dst->u.int64 = *(int64_t *)src;
    return 1;
}

static int int_get(const m_option_t *opt, void *ta_parent,
                   struct mpv_node *dst, void *src)
{
    dst->format = MPV_FORMAT_INT64;
    dst->u.int64 = *(int *)src;
    return 1;
}

static bool int_equal(const m_option_t *opt, void *a, void *b)
{
    return *(int *)a == *(int *)b;
}

static bool int64_equal(const m_option_t *opt, void *a, void *b)
{
    return *(int64_t *)a == *(int64_t *)b;
}

const m_option_type_t m_option_type_int = {
    .name  = "Integer",
    .flags = M_OPT_TYPE_USES_RANGE,
    .size  = sizeof(int),
    .parse = parse_int,
    .print = print_int,
    .copy  = copy_opt,
    .add = add_int,
    .multiply = multiply_int,
    .set   = int_set,
    .get   = int_get,
    .equal = int_equal,
};

const m_option_type_t m_option_type_int64 = {
    .name  = "Integer64",
    .flags = M_OPT_TYPE_USES_RANGE,
    .size  = sizeof(int64_t),
    .parse = parse_int64,
    .print = print_int,
    .copy  = copy_opt,
    .add = add_int64,
    .multiply = multiply_int64,
    .set   = int64_set,
    .get   = int64_get,
    .equal = int64_equal,
};

static int parse_byte_size(struct mp_log *log, const m_option_t *opt,
                           struct bstr name, struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct bstr r;
    long long tmp_int = bstrtoll(param, &r, 0);
    int64_t unit = 1;
    if (r.len) {
        if (bstrcasecmp0(r, "b") == 0) {
            unit = 1;
        } else if (bstrcasecmp0(r, "kib") == 0 || bstrcasecmp0(r, "k") == 0) {
            unit = 1024;
        } else if (bstrcasecmp0(r, "mib") == 0 || bstrcasecmp0(r, "m") == 0) {
            unit = 1024 * 1024;
        } else if (bstrcasecmp0(r, "gib") == 0 || bstrcasecmp0(r, "g") == 0) {
            unit = 1024 * 1024 * 1024;
        } else if (bstrcasecmp0(r, "tib") == 0 || bstrcasecmp0(r, "t") == 0) {
            unit = 1024 * 1024 * 1024 * 1024LL;
        } else {
            mp_err(log, "The %.*s option must be an integer: %.*s\n",
                   BSTR_P(name), BSTR_P(param));
            mp_err(log, "The following suffixes are also allowed: "
                   "KiB, MiB, GiB, TiB, B, K, M, G, T.\n");
            return M_OPT_INVALID;
        }
    }

    if (tmp_int < 0) {
        mp_err(log, "The %.*s option does not support negative numbers: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    if (INT64_MAX / unit < tmp_int) {
        mp_err(log, "The %.*s option overflows: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    tmp_int *= unit;

    int64_t min = OPT_INT_MIN(opt, int64_t, INT64_MIN);
    if (tmp_int < min) {
        mp_err(log, "The %.*s option must be >= %"PRId64": %.*s\n",
               BSTR_P(name), min, BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    int64_t max = OPT_INT_MAX(opt, int64_t, INT64_MAX);
    if (tmp_int > max) {
        mp_err(log, "The %.*s option must be <= %"PRId64": %.*s\n",
               BSTR_P(name), max, BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    if (dst)
        *(int64_t *)dst = tmp_int;

    return 1;
}

char *format_file_size(int64_t size)
{
    double s = size;
    if (size < 1024)
        return talloc_asprintf(NULL, "%.0f B", s);

    if (size < (1024 * 1024))
        return talloc_asprintf(NULL, "%.3f KiB", s / (1024.0));

    if (size < (1024 * 1024 * 1024))
        return talloc_asprintf(NULL, "%.3f MiB", s / (1024.0 * 1024.0));

    if (size < (1024LL * 1024LL * 1024LL * 1024LL))
        return talloc_asprintf(NULL, "%.3f GiB", s / (1024.0 * 1024.0 * 1024.0));

    return talloc_asprintf(NULL, "%.3f TiB", s / (1024.0 * 1024.0 * 1024.0 * 1024.0));
}

static char *pretty_print_byte_size(const m_option_t *opt, const void *val)
{
    return format_file_size(*(int64_t *)val);
}

const m_option_type_t m_option_type_byte_size = {
    .name  = "ByteSize",
    .flags = M_OPT_TYPE_USES_RANGE,
    .size  = sizeof(int64_t),
    .parse = parse_byte_size,
    .print = print_int,
    .pretty_print = pretty_print_byte_size,
    .copy  = copy_opt,
    .add = add_int64,
    .multiply = multiply_int64,
    .set   = int64_set,
    .get   = int64_get,
    .equal = int64_equal,
};

const char *m_opt_choice_str(const struct m_opt_choice_alternatives *choices,
                             int value)
{
    for (const struct m_opt_choice_alternatives *c = choices; c->name; c++) {
        if (c->value == value)
            return c->name;
    }
    return NULL;
}

static void print_choice_values(struct mp_log *log, const struct m_option *opt)
{
    const struct m_opt_choice_alternatives *alt = opt->priv;
    for ( ; alt->name; alt++)
        mp_info(log, "    %s\n", alt->name[0] ? alt->name : "(passing nothing)");
    if (opt->min < opt->max)
        mp_info(log, "    %g-%g (integer range)\n", opt->min, opt->max);
}

static int parse_choice(struct mp_log *log, const struct m_option *opt,
                        struct bstr name, struct bstr param, void *dst)
{
    const struct m_opt_choice_alternatives *alt = opt->priv;
    for ( ; alt->name; alt++) {
        if (!bstrcmp0(param, alt->name))
            break;
    }
    if (!alt->name && param.len == 0) {
        // allow flag-style options, e.g. "--mute" implies "--mute=yes"
        for (alt = opt->priv; alt->name; alt++) {
            if (!strcmp("yes", alt->name))
                break;
        }
    }
    if (!alt->name) {
        if (!bstrcmp0(param, "help")) {
            mp_info(log, "Valid values for option %.*s are:\n", BSTR_P(name));
            print_choice_values(log, opt);
            return M_OPT_EXIT;
        }
        if (param.len == 0)
            return M_OPT_MISSING_PARAM;
        if (opt->min < opt->max) {
            long long val;
            if (parse_longlong(mp_null_log, opt, INT_MIN, INT_MAX, name, param,
                               &val) == 1)
            {
                if (dst)
                    *(int *)dst = val;
                return 1;
            }
        }
        mp_fatal(log, "Invalid value for option %.*s: %.*s\n",
                 BSTR_P(name), BSTR_P(param));
        mp_info(log, "Valid values are:\n");
        print_choice_values(log, opt);
        return M_OPT_INVALID;
    }
    if (dst)
        *(int *)dst = alt->value;

    return 1;
}

static void choice_get_min_max(const struct m_option *opt, int *min, int *max)
{
    mp_assert(opt->type == &m_option_type_choice);
    *min = INT_MAX;
    *max = INT_MIN;
    for (const struct m_opt_choice_alternatives *alt = opt->priv; alt->name; alt++) {
        *min = MPMIN(*min, alt->value);
        *max = MPMAX(*max, alt->value);
    }
    if (opt->min < opt->max) {
        *min = MPMIN(*min, opt->min);
        *max = MPMAX(*max, opt->max);
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
    mp_assert(opt->type == &m_option_type_choice);
    int dir = add > 0 ? +1 : -1;
    bool found = false;
    int ival = *(int *)val;
    int best = 0; // init. value unused

    if (fabs(add) < 0.5)
        return;

    if (opt->min < opt->max) {
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

    for (const struct m_opt_choice_alternatives *alt = opt->priv; alt->name; alt++)
        check_choice(dir, ival, &found, &best, alt->value);

    if (!found) {
        int min, max;
        choice_get_min_max(opt, &min, &max);
        best = (dir == -1) ^ wrap ? min : max;
    }

    *(int *)val = best;
}

static int choice_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    char buf[80];
    char *src_str = NULL;
    if (src->format == MPV_FORMAT_INT64) {
        snprintf(buf, sizeof(buf), "%" PRId64, src->u.int64);
        src_str = buf;
    } else if (src->format == MPV_FORMAT_STRING) {
        src_str = src->u.string;
    } else if (src->format == MPV_FORMAT_FLAG) {
        src_str = src->u.flag ? "yes" : "no";
    }
    if (!src_str)
        return M_OPT_UNKNOWN;
    int val = 0;
    int r = parse_choice(mp_null_log, opt, (bstr){0}, bstr0(src_str), &val);
    if (r >= 0)
        *(int *)dst = val;
    return r;
}

static const struct m_opt_choice_alternatives *get_choice(const m_option_t *opt,
                                                          const void *val,
                                                          int *out_val)
{
    int v = *(int *)val;
    const struct m_opt_choice_alternatives *alt;
    for (alt = opt->priv; alt->name; alt++) {
        if (alt->value == v)
            return alt;
    }
    if (opt->min < opt->max) {
        if (v >= opt->min && v <= opt->max) {
            *out_val = v;
            return NULL;
        }
    }
    MP_ASSERT_UNREACHABLE();
}

static int choice_get(const m_option_t *opt, void *ta_parent,
                      struct mpv_node *dst, void *src)
{
    int ival = 0;
    const struct m_opt_choice_alternatives *alt = get_choice(opt, src, &ival);
    // If a choice string looks like a number, return it as number
    if (alt) {
        char *end = NULL;
        ival = strtol(alt->name, &end, 10);
        if (end && !end[0])
            alt = NULL;
    }
    if (alt) {
        int b = -1;
        if (strcmp(alt->name, "yes") == 0) {
            b = 1;
        } else if (strcmp(alt->name, "no") == 0) {
            b = 0;
        }
        if (b >= 0) {
            dst->format = MPV_FORMAT_FLAG;
            dst->u.flag = b;
        } else {
            dst->format = MPV_FORMAT_STRING;
            dst->u.string = talloc_strdup(ta_parent, alt->name);
        }
    } else {
        dst->format = MPV_FORMAT_INT64;
        dst->u.int64 = ival;
    }
    return 1;
}

static char *print_choice(const m_option_t *opt, const void *val)
{
    int ival = 0;
    const struct m_opt_choice_alternatives *alt = get_choice(opt, val, &ival);
    return alt ? talloc_strdup(NULL, alt->name)
               : talloc_asprintf(NULL, "%d", ival);
}

const struct m_option_type m_option_type_choice = {
    .name  = "Choice",
    .size  = sizeof(int),
    .flags = M_OPT_TYPE_CHOICE | M_OPT_TYPE_USES_RANGE,
    .parse = parse_choice,
    .print = print_choice,
    .copy  = copy_opt,
    .add   = add_choice,
    .set   = choice_set,
    .get   = choice_get,
    .equal = int_equal,
};

static int apply_flag(const struct m_option *opt, int *val, bstr flag)
{
    const struct m_opt_choice_alternatives *alt;
    for (alt = opt->priv; alt->name; alt++) {
        if (bstr_equals0(flag, alt->name)) {
            if (*val & alt->value)
                return M_OPT_INVALID;
            *val |= alt->value;
            return 0;
        }
    }
    return M_OPT_UNKNOWN;
}

static const char *find_next_flag(const struct m_option *opt, int *val)
{
    const struct m_opt_choice_alternatives *best = NULL;
    const struct m_opt_choice_alternatives *alt;
    for (alt = opt->priv; alt->name; alt++) {
        if (alt->value && (alt->value & (*val)) == alt->value) {
            if (!best || av_popcount64(alt->value) > av_popcount64(best->value))
                best = alt;
        }
    }
    if (best) {
        *val = *val & ~(unsigned)best->value;
        return best->name;
    }
    *val = 0; // if there are still flags left, there's not much we can do
    return NULL;
}

static int parse_flags(struct mp_log *log, const struct m_option *opt,
                       struct bstr name, struct bstr param, void *dst)
{
    int value = 0;
    while (param.len) {
        bstr flag;
        bstr_split_tok(param, "+", &flag, &param);
        int r = apply_flag(opt, &value, flag);
        if (r == M_OPT_UNKNOWN) {
            mp_fatal(log, "Invalid flag for option %.*s: %.*s\n",
                     BSTR_P(name), BSTR_P(flag));
            mp_info(log, "Valid flags are:\n");
            const struct m_opt_choice_alternatives *alt;
            for (alt = opt->priv; alt->name; alt++)
                mp_info(log, "    %s\n", alt->name);
            mp_info(log, "Flags can usually be combined with '+'.\n");
            return M_OPT_INVALID;
        } else if (r < 0) {
            mp_fatal(log, "Option %.*s: flag '%.*s' conflicts with a previous "
                     "flag value.\n", BSTR_P(name), BSTR_P(flag));
            return M_OPT_INVALID;
        }
    }
    if (dst)
        *(int *)dst = value;
    return 1;
}

static int flags_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    int value = 0;
    if (src->format != MPV_FORMAT_NODE_ARRAY)
        return M_OPT_UNKNOWN;
    struct mpv_node_list *srclist = src->u.list;
    for (int n = 0; n < srclist->num; n++) {
        if (srclist->values[n].format != MPV_FORMAT_STRING)
            return M_OPT_INVALID;
        if (apply_flag(opt, &value, bstr0(srclist->values[n].u.string)) < 0)
            return M_OPT_INVALID;
    }
    *(int *)dst = value;
    return 0;
}

static int flags_get(const m_option_t *opt, void *ta_parent,
                     struct mpv_node *dst, void *src)
{
    int value = *(int *)src;

    dst->format = MPV_FORMAT_NODE_ARRAY;
    dst->u.list = talloc_zero(ta_parent, struct mpv_node_list);
    struct mpv_node_list *list = dst->u.list;
    while (1) {
        const char *flag = find_next_flag(opt, &value);
        if (!flag)
            break;

        struct mpv_node node;
        node.format = MPV_FORMAT_STRING;
        node.u.string = (char *)flag;
        MP_TARRAY_APPEND(list, list->values, list->num, node);
    }

    return 1;
}

static char *print_flags(const m_option_t *opt, const void *val)
{
    int value = *(int *)val;
    char *res = talloc_strdup(NULL, "");
    while (1) {
        const char *flag = find_next_flag(opt, &value);
        if (!flag)
            break;

        res = talloc_asprintf_append_buffer(res, "%s%s", res[0] ? "+" : "", flag);
    }
    return res;
}

const struct m_option_type m_option_type_flags = {
    .name  = "Flags",
    .size  = sizeof(int),
    .parse = parse_flags,
    .print = print_flags,
    .copy  = copy_opt,
    .set   = flags_set,
    .get   = flags_get,
    .equal = int_equal,
};

// Float

#undef VAL
#define VAL(x) (*(double *)(x))

static int clamp_double(const m_option_t *opt, void *val)
{
    double v = VAL(val);
    int r = 0;
    if (opt->min < opt->max) {
        if (v > opt->max) {
            v = opt->max;
            r = M_OPT_OUT_OF_RANGE;
        }
        if (v < opt->min) {
            v = opt->min;
            r = M_OPT_OUT_OF_RANGE;
        }
    }
    // (setting max/min to INFINITY/-INFINITY is allowed)
    if (!isfinite(v) && v != opt->max && v != opt->min) {
        v = opt->min;
        r = M_OPT_OUT_OF_RANGE;
    }
    VAL(val) = v;
    return r;
}

static int parse_double(struct mp_log *log, const m_option_t *opt,
                        struct bstr name, struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    struct bstr rest;
    double tmp_float = bstrtod(param, &rest);

    if (bstr_eatstart0(&rest, ":") || bstr_eatstart0(&rest, "/"))
        tmp_float /= bstrtod(rest, &rest);

    if ((opt->flags & M_OPT_DEFAULT_NAN) && bstr_equals0(param, "default")) {
        tmp_float = NAN;
        goto done;
    }

    if (rest.len) {
        mp_err(log, "The %.*s option must be a floating point number or a "
               "ratio (numerator[:/]denominator): %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    if (clamp_double(opt, &tmp_float) < 0) {
        mp_err(log, "The %.*s option is out of range: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

done:
    if (dst)
        VAL(dst) = tmp_float;
    return 1;
}

static char *print_double(const m_option_t *opt, const void *val)
{
    double f = VAL(val);
    if (isnan(f) && (opt->flags & M_OPT_DEFAULT_NAN))
        return talloc_strdup(NULL, "default");
    return talloc_asprintf(NULL, "%f", f);
}

static char *pretty_print_double(const m_option_t *opt, const void *val)
{
    double f = VAL(val);
    if (isnan(f))
        return print_double(opt, val);
    return mp_format_double(NULL, f, 4, false, false, !(opt->flags & M_OPT_FIXED_LEN_PRINT));
}

static void add_double(const m_option_t *opt, void *val, double add, bool wrap)
{
    double v = VAL(val);

    v = v + add;

    double min = opt->min < opt->max ? opt->min : -INFINITY;
    double max = opt->min < opt->max ? opt->max : +INFINITY;

    if (v < min)
        v = wrap ? max : min;
    if (v > max)
        v = wrap ? min : max;

    VAL(val) = v;
}

static void multiply_double(const m_option_t *opt, void *val, double f)
{
    VAL(val) *= f;
    clamp_double(opt, val);
}

static int double_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    double val;
    if (src->format == MPV_FORMAT_INT64) {
        // Can't always be represented exactly, but don't care.
        val = src->u.int64;
    } else if (src->format == MPV_FORMAT_DOUBLE) {
        val = src->u.double_;
    } else {
        return M_OPT_UNKNOWN;
    }
    if (clamp_double(opt, &val) < 0)
        return M_OPT_OUT_OF_RANGE;
    VAL(dst) = val;
    return 1;
}

static int double_get(const m_option_t *opt, void *ta_parent,
                      struct mpv_node *dst, void *src)
{
    double f = VAL(src);
    if (isnan(f) && (opt->flags & M_OPT_DEFAULT_NAN)) {
        dst->format = MPV_FORMAT_STRING;
        dst->u.string = talloc_strdup(ta_parent, "default");
    } else {
        dst->format = MPV_FORMAT_DOUBLE;
        dst->u.double_ = f;
    }
    return 1;
}

static bool double_equal(const m_option_t *opt, void *a, void *b)
{
    double fa = VAL(a), fb = VAL(b);
    if (isnan(fa) || isnan(fb))
        return isnan(fa) == isnan(fb);
    return fa == fb;
}

const m_option_type_t m_option_type_double = {
    // double precision float or ratio (numerator[:/]denominator)
    .name  = "Double",
    .flags = M_OPT_TYPE_USES_RANGE,
    .size  = sizeof(double),
    .parse = parse_double,
    .print = print_double,
    .pretty_print = pretty_print_double,
    .copy  = copy_opt,
    .add = add_double,
    .multiply = multiply_double,
    .set   = double_set,
    .get   = double_get,
    .equal = double_equal,
};

static int parse_double_aspect(struct mp_log *log, const m_option_t *opt,
                               struct bstr name, struct bstr param, void *dst)
{
    if (bstr_equals0(param, "no")) {
        if (dst)
            VAL(dst) = -2.0;
        return 1;
    }
    return parse_double(log, opt, name, param, dst);
}

const m_option_type_t m_option_type_aspect = {
    .name  = "Aspect",
    .size  = sizeof(double),
    .flags = M_OPT_TYPE_CHOICE | M_OPT_TYPE_USES_RANGE,
    .parse = parse_double_aspect,
    .print = print_double,
    .pretty_print = pretty_print_double,
    .copy  = copy_opt,
    .add = add_double,
    .multiply = multiply_double,
    .set   = double_set,
    .get   = double_get,
    .equal = double_equal,
};

#undef VAL
#define VAL(x) (*(float *)(x))

static int clamp_float(const m_option_t *opt, double *val)
{
    double v = *val;
    int r = clamp_double(opt, &v);
    // Handle the case where range is not set and v is finite
    // but overflows the float range.
    if (isfinite(v) && v > FLT_MAX) {
        v = FLT_MAX;
        r = M_OPT_OUT_OF_RANGE;
    }
    if (isfinite(v) && v < -FLT_MAX) {
        v = -FLT_MAX;
        r = M_OPT_OUT_OF_RANGE;
    }
    *val = v;
    return r;
}

static int parse_float(struct mp_log *log, const m_option_t *opt,
                       struct bstr name, struct bstr param, void *dst)
{
    double tmp;
    int r = parse_double(log, opt, name, param, &tmp);

    if (r == 1 && clamp_float(opt, &tmp) < 0) {
        mp_err(log, "The %.*s option is out of range: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_OUT_OF_RANGE;
    }

    if (r == 1 && dst)
        VAL(dst) = tmp;
    return r;
}

static char *print_float(const m_option_t *opt, const void *val)
{
    double tmp = VAL(val);
    return print_double(opt, &tmp);
}

static char *pretty_print_float(const m_option_t *opt, const void *val)
{
    double tmp = VAL(val);
    return pretty_print_double(opt, &tmp);
}

static void add_float(const m_option_t *opt, void *val, double add, bool wrap)
{
    double tmp = VAL(val);
    add_double(opt, &tmp, add, wrap);
    clamp_float(opt, &tmp);
    VAL(val) = tmp;
}

static void multiply_float(const m_option_t *opt, void *val, double f)
{
    double tmp = VAL(val);
    multiply_double(opt, &tmp, f);
    clamp_float(opt, &tmp);
    VAL(val) = tmp;
}

static int float_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    double tmp;
    int r = double_set(opt, &tmp, src);
    if (r >= 0 && clamp_double(opt, &tmp) < 0)
        return M_OPT_OUT_OF_RANGE;
    if (r >= 0)
        VAL(dst) = tmp;
    return r;
}

static int float_get(const m_option_t *opt, void *ta_parent,
                     struct mpv_node *dst, void *src)
{
    double tmp = VAL(src);
    return double_get(opt, ta_parent, dst, &tmp);
}

static bool float_equal(const m_option_t *opt, void *a, void *b)
{
    return double_equal(opt, &(double){VAL(a)}, &(double){VAL(b)});
}

const m_option_type_t m_option_type_float = {
    // floating point number or ratio (numerator[:/]denominator)
    .name  = "Float",
    .flags = M_OPT_TYPE_USES_RANGE,
    .size  = sizeof(float),
    .parse = parse_float,
    .print = print_float,
    .pretty_print = pretty_print_float,
    .copy  = copy_opt,
    .add = add_float,
    .multiply = multiply_float,
    .set   = float_set,
    .get   = float_get,
    .equal = float_equal,
};

///////////// String

#undef VAL
#define VAL(x) (*(char **)(x))

static int parse_str(struct mp_log *log, const m_option_t *opt,
                     struct bstr name, struct bstr param, void *dst)
{
    if (dst) {
        talloc_free(VAL(dst));
        VAL(dst) = bstrdup0(NULL, param);
    }

    return 0;
}

static char *print_str(const m_option_t *opt, const void *val)
{
    return talloc_strdup(NULL, VAL(val) ? VAL(val) : "");
}

static void copy_str(const m_option_t *opt, void *dst, const void *src)
{
    if (dst && src)
        talloc_replace(NULL, VAL(dst), VAL(src));
}

static int str_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    if (src->format != MPV_FORMAT_STRING)
        return M_OPT_UNKNOWN;
    char *s = src->u.string;
    int r = s ? 0 : M_OPT_INVALID;
    if (r >= 0)
        copy_str(opt, dst, &s);
    return r;
}

static int str_get(const m_option_t *opt, void *ta_parent,
                   struct mpv_node *dst, void *src)
{
    dst->format = MPV_FORMAT_STRING;
    dst->u.string = talloc_strdup(ta_parent, VAL(src) ? VAL(src) : "");
    return 1;
}

static bool str_equal(const m_option_t *opt, void *a, void *b)
{
    return bstr_equals(bstr0(VAL(a)), bstr0(VAL(b)));
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
    .parse = parse_str,
    .print = print_str,
    .copy  = copy_str,
    .free  = free_str,
    .set   = str_set,
    .get   = str_get,
    .equal = str_equal,
};

//////////// String list

#undef VAL
#define VAL(x) (*(char ***)(x))

#define OP_NONE 0
#define OP_ADD 1
#define OP_PRE 2
#define OP_CLR 3
#define OP_DEL 4
#define OP_TOGGLE 5
#define OP_APPEND 6
#define OP_REMOVE 7

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
    for (ln = 0; lst && lst[ln]; ln++)
        /**/;

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (ln >= 100) {
        while (n--)
            talloc_free(add[n]);
        talloc_free(add);
        return 0;
    }
#endif

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

static struct bstr get_nextsep(struct bstr *ptr, char sep, bool modify)
{
    struct bstr str = *ptr;
    struct bstr orig = str;
    for (;;) {
        int idx = sep ? bstrchr(str, sep) : -1;
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

static int find_list_bstr(char **list, bstr item)
{
    for (int n = 0; list && list[n]; n++) {
        if (bstr_equals0(item, list[n]))
            return n;
    }
    return -1;
}

static char **separate_input_param(const m_option_t *opt, bstr param,
                                   int *len, int op)
{
    char separator = opt->priv ? *(char *)opt->priv : OPTION_LIST_SEPARATOR;
    if (op == OP_APPEND || op == OP_REMOVE)
        separator = 0; // specially handled
    struct bstr str = param;
    int n = *len;
    while (str.len) {
        get_nextsep(&str, separator, 0);
        str = bstr_cut(str, 1);
        n++;
    }
    if (n == 0 && op != OP_NONE)
        return NULL;

    char **list = talloc_array(NULL, char *, n + 2);
    str = bstrdup(NULL, param);
    char *ptr = str.start;
    n = 0;

    while (1) {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        if (n >= 100)
            break;
#endif
        struct bstr el = get_nextsep(&str, separator, 1);
        list[n] = bstrdup0(NULL, el);
        n++;
        if (!str.len)
            break;
        str = bstr_cut(str, 1);
    }
    list[n] = NULL;
    *len = n;
    talloc_free(ptr);
    return list;
}

static int str_list_remove(char **remove, int n, void *dst)
{
    bool found = false;
    char **list = VAL(dst);
    for (int i = 0; i < n; i++) {
        int index = 0;
        do {
            index = find_list_bstr(list, bstr0(remove[i]));
            if (index >= 0) {
                found = true;
                char *old = list[index];
                for (int j = index; list[j]; j++)
                    list[j] = list[j + 1];
                talloc_free(old);
            }
        } while (index >= 0);
        talloc_free(remove[i]);
    }
    talloc_free(remove);
    return found;
}

static int parse_str_list_impl(struct mp_log *log, const m_option_t *opt,
                               struct bstr name, struct bstr param, void *dst,
                               int default_op)
{
    char **res;
    int op = default_op;

    if (bstr_endswith0(name, "-add")) {
        op = OP_ADD;
    } else if (bstr_endswith0(name, "-append")) {
        op = OP_APPEND;
    } else if (bstr_endswith0(name, "-pre")) {
        op = OP_PRE;
    } else if (bstr_endswith0(name, "-clr")) {
        op = OP_CLR;
    } else if (bstr_endswith0(name, "-del")) {
        op = OP_DEL;
    } else if (bstr_endswith0(name, "-set")) {
        op = OP_NONE;
    } else if (bstr_endswith0(name, "-toggle")) {
        op = OP_TOGGLE;
    } else if (bstr_endswith0(name, "-remove")) {
        op = OP_REMOVE;
    }

    if (op == OP_TOGGLE || op == OP_REMOVE) {
        if (dst) {
            res = talloc_array(NULL, char *, 2);
            res[0] = bstrdup0(res, param);
            res[1] = NULL;
            bool found = str_list_remove(res, 2, dst);
            if (found)
                return 1;
        }
        if (op == OP_REMOVE)
            return 1; // ignore if not found
        op = OP_APPEND;
    }

    // Clear the list ??
    if (op == OP_CLR) {
        if (dst)
            free_str_list(dst);
        return 0;
    }

    // All other ops need a param
    if (param.len == 0 && op != OP_NONE)
        return M_OPT_MISSING_PARAM;

    if (!dst)
        return 1;

    int n = 0;
    res = separate_input_param(opt, param, &n, op);
    if (!res)
        return M_OPT_INVALID;

    switch (op) {
    case OP_ADD:
    case OP_APPEND:
        return str_list_add(res, n, dst, 0);
    case OP_PRE:
        return str_list_add(res, n, dst, 1);
    case OP_DEL:
        return str_list_remove(res, n, dst);
    }

    if (VAL(dst))
        free_str_list(dst);
    VAL(dst) = res;

    if (!res[0])
        free_str_list(dst);

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
    const char sep = opt->priv ? *(char *)opt->priv : OPTION_LIST_SEPARATOR;

    if (!(src && VAL(src)))
        return talloc_strdup(NULL, "");
    lst = VAL(src);

    for (int i = 0; lst[i]; i++) {
        if (ret)
            ret = talloc_strndup_append_buffer(ret, &sep, 1);
        ret = talloc_strdup_append_buffer(ret, lst[i]);
    }
    return ret;
}

static int str_list_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    if (src->format != MPV_FORMAT_NODE_ARRAY)
        return M_OPT_UNKNOWN;
    struct mpv_node_list *srclist = src->u.list;
    for (int n = 0; n < srclist->num; n++) {
        if (srclist->values[n].format != MPV_FORMAT_STRING)
            return M_OPT_INVALID;
    }
    free_str_list(dst);
    if (srclist->num > 0) {
        VAL(dst) = talloc_array(NULL, char*, srclist->num + 1);
        for (int n = 0; n < srclist->num; n++)
            VAL(dst)[n] = talloc_strdup(NULL, srclist->values[n].u.string);
        VAL(dst)[srclist->num] = NULL;
    }
    return 1;
}

static int str_list_get(const m_option_t *opt, void *ta_parent,
                        struct mpv_node *dst, void *src)
{
    dst->format = MPV_FORMAT_NODE_ARRAY;
    dst->u.list = talloc_zero(ta_parent, struct mpv_node_list);
    struct mpv_node_list *list = dst->u.list;
    for (int n = 0; VAL(src) && VAL(src)[n]; n++) {
        struct mpv_node node;
        node.format = MPV_FORMAT_STRING;
        node.u.string = talloc_strdup(list, VAL(src)[n]);
        MP_TARRAY_APPEND(list, list->values, list->num, node);
    }
    return 1;
}

static int parse_str_list(struct mp_log *log, const m_option_t *opt,
                          struct bstr name, struct bstr param, void *dst)
{
    return parse_str_list_impl(log, opt, name, param, dst, OP_NONE);
}

static bool str_list_equal(const m_option_t *opt, void *a, void *b)
{
    char **la = VAL(a);
    char **lb = VAL(b);

    bool a_empty = !la || !la[0];
    bool b_empty = !lb || !lb[0];
    if (a_empty || b_empty)
        return a_empty == b_empty;

    for (int n = 0; la[n] || lb[n]; n++) {
        if (!la[n] || !lb[n])
            return false;
        if (strcmp(la[n], lb[n]) != 0)
            return false;
    }

    return true;
}

const m_option_type_t m_option_type_string_list = {
    .name  = "String list",
    .size  = sizeof(char **),
    .parse = parse_str_list,
    .print = print_str_list,
    .copy  = copy_str_list,
    .free  = free_str_list,
    .get   = str_list_get,
    .set   = str_list_set,
    .equal = str_list_equal,
    .actions = (const struct m_option_action[]){
        {"add"},
        {"append"},
        {"clr",         M_OPT_TYPE_OPTIONAL_PARAM},
        {"del"},
        {"pre"},
        {"set"},
        {"toggle"},
        {"remove"},
        {0}
    },
};

static int read_subparam(struct mp_log *log, bstr optname, char *termset,
                         bstr *str, bstr *out_subparam);

static int keyvalue_list_find_key(char **lst, bstr str)
{
    for (int n = 0; lst && lst[n] && lst[n + 1]; n += 2) {
        if (bstr_equals0(str, lst[n]))
            return n / 2;
    }
    return -1;
}

static void keyvalue_list_del_key(char **lst, int index)
{
    int count = 0;
    for (int n = 0; lst && lst[n]; n++)
        count++;
    mp_assert(index * 2 + 1 < count);
    count += 1; // terminating item
    talloc_free(lst[index * 2 + 0]);
    talloc_free(lst[index * 2 + 1]);
    MP_TARRAY_REMOVE_AT(lst, count, index * 2 + 1);
    MP_TARRAY_REMOVE_AT(lst, count, index * 2 + 0);
}

static int parse_keyvalue_list(struct mp_log *log, const m_option_t *opt,
                               struct bstr name, struct bstr param, void *dst)
{
    char **lst = NULL;
    int num = 0;
    int r = 0;
    bool append = false;
    bool full_value = false;

    if ((opt->flags & M_OPT_HAVE_HELP) && bstr_equals0(param, "help"))
        param = bstr0("help=");

    int op = 0;
    if (bstr_endswith0(name, "-del")) {
        op = OP_DEL;
    } else if (bstr_endswith0(name, "-remove")) {
        op = OP_REMOVE;
    }

    if (bstr_endswith0(name, "-add")) {
        append = true;
    } else if (bstr_endswith0(name, "-append")) {
        append = full_value = true;
    } else if (bstr_endswith0(name, "-clr")) {
        if (dst)
            free_str_list(dst);
        return 0;
    } else if (op == OP_DEL || op == OP_REMOVE) {
        int n = 0;
        char **res = separate_input_param(opt, param, &n, op);
        if (!res)
            return M_OPT_INVALID;
        lst = dst ? VAL(dst) : NULL;
        for (int i = 0; i < n; i++) {
            int index = dst ? keyvalue_list_find_key(lst, bstr0(res[i])) : -1;
            if (index >= 0) {
                keyvalue_list_del_key(lst, index);
                VAL(dst) = lst;
            }
            talloc_free(res[i]);
        }
        talloc_free(res);
        return 1;
    }

    if (append && dst) {
        lst = VAL(dst);
        for (int n = 0; lst && lst[n]; n++)
            num++;
    }

    while (param.len) {
        bstr key, val;
        r = read_subparam(log, name, "=", &param, &key);
        if (r < 0)
            break;
        if (!bstr_eatstart0(&param, "=")) {
            mp_err(log, "Expected '=' and a value.\n");
            r = M_OPT_INVALID;
            break;
        }
        if (full_value) {
            val = param;
            param.len = 0;
        } else {
            r = read_subparam(log, name, ",", &param, &val);
            if (r < 0)
                break;
        }
        if (dst) {
            int index = keyvalue_list_find_key(lst, key);
            if (index >= 0) {
                keyvalue_list_del_key(lst, index);
                num -= 2;
            }
            MP_TARRAY_APPEND(NULL, lst, num, bstrto0(NULL, key));
            MP_TARRAY_APPEND(NULL, lst, num, bstrto0(NULL, val));
            MP_TARRAY_APPEND(NULL, lst, num, NULL);
            num -= 1;
        }

        if (!bstr_eatstart0(&param, ",") && !bstr_eatstart0(&param, ":"))
            break;
    }

    if (param.len) {
        mp_err(log, "Unparsable garbage at end of option value: '%.*s'\n",
               BSTR_P(param));
        r = M_OPT_INVALID;
    }

    if (dst) {
        if (!append)
            free_str_list(dst);
        VAL(dst) = lst;
        if (r < 0)
            free_str_list(dst);
    } else {
        free_str_list(&lst);
    }
    return r;
}

static char *print_keyvalue_list(const m_option_t *opt, const void *src)
{
    char **lst = VAL(src);
    char *ret = talloc_strdup(NULL, "");
    for (int n = 0; lst && lst[n] && lst[n + 1]; n += 2) {
        if (ret[0])
            ret = talloc_strdup_append(ret, ",");
        ret = talloc_asprintf_append(ret, "%s=%s", lst[n], lst[n + 1]);
    }
    return ret;
}

static int keyvalue_list_set(const m_option_t *opt, void *dst,
                             struct mpv_node *src)
{
    if (src->format != MPV_FORMAT_NODE_MAP)
        return M_OPT_UNKNOWN;
    struct mpv_node_list *srclist = src->u.list;
    for (int n = 0; n < srclist->num; n++) {
        if (srclist->values[n].format != MPV_FORMAT_STRING)
            return M_OPT_INVALID;
    }
    free_str_list(dst);
    if (srclist->num > 0) {
        VAL(dst) = talloc_array(NULL, char*, (srclist->num + 1) * 2);
        for (int n = 0; n < srclist->num; n++) {
            VAL(dst)[n * 2 + 0] = talloc_strdup(NULL, srclist->keys[n]);
            VAL(dst)[n * 2 + 1] = talloc_strdup(NULL, srclist->values[n].u.string);
        }
        VAL(dst)[srclist->num * 2 + 0] = NULL;
        VAL(dst)[srclist->num * 2 + 1] = NULL;
    }
    return 1;
}

static int keyvalue_list_get(const m_option_t *opt, void *ta_parent,
                             struct mpv_node *dst, void *src)
{
    dst->format = MPV_FORMAT_NODE_MAP;
    dst->u.list = talloc_zero(ta_parent, struct mpv_node_list);
    struct mpv_node_list *list = dst->u.list;
    for (int n = 0; VAL(src) && VAL(src)[n * 2 + 0]; n++) {
        MP_TARRAY_GROW(list, list->values, list->num);
        MP_TARRAY_GROW(list, list->keys, list->num);
        list->keys[list->num] = talloc_strdup(list, VAL(src)[n * 2 + 0]);
        list->values[list->num] = (struct mpv_node){
            .format = MPV_FORMAT_STRING,
            .u.string = talloc_strdup(list, VAL(src)[n * 2 + 1]),
        };
        list->num++;
    }
    return 1;
}

const m_option_type_t m_option_type_keyvalue_list = {
    .name  = "Key/value list",
    .size  = sizeof(char **),
    .parse = parse_keyvalue_list,
    .print = print_keyvalue_list,
    .copy  = copy_str_list,
    .free  = free_str_list,
    .get   = keyvalue_list_get,
    .set   = keyvalue_list_set,
    .equal = str_list_equal,
    .actions = (const struct m_option_action[]){
        {"add"},
        {"append"},
        {"clr",         M_OPT_TYPE_OPTIONAL_PARAM},
        {"del"},
        {"set"},
        {"remove"},
        {0}
    },
};


#undef VAL
#define VAL(x) (*(char **)(x))

static int check_msg_levels(struct mp_log *log, char **list)
{
    for (int n = 0; list && list[n * 2 + 0]; n++) {
        char *level = list[n * 2 + 1];
        if (mp_msg_find_level(level) < 0 && strcmp(level, "no") != 0) {
            mp_err(log, "Invalid message level '%s'\n", level);
            return M_OPT_INVALID;
        }
    }
    return 1;
}

static int parse_msglevels(struct mp_log *log, const m_option_t *opt,
                           struct bstr name, struct bstr param, void *dst)
{
    if (bstr_equals0(param, "help")) {
        mp_info(log, "Syntax:\n\n   --msg-level=module1=level,module2=level,...\n\n"
                     "'module' is output prefix as shown with -v, or a prefix\n"
                     "of it. level is one of:\n\n"
                     "  fatal error warn info status v debug trace\n\n"
                     "The level specifies the minimum log level a message\n"
                     "must have to be printed.\n"
                     "The special module name 'all' affects all modules.\n");
        return M_OPT_EXIT;
    }

    char **dst_copy = NULL;
    int r = m_option_type_keyvalue_list.parse(log, opt, name, param, &dst_copy);
    if (r >= 0)
        r = check_msg_levels(log, dst_copy);

    if (r >= 0)
        m_option_type_keyvalue_list.copy(opt, dst, &dst_copy);
    m_option_type_keyvalue_list.free(&dst_copy);
    return r;
}

static int set_msglevels(const m_option_t *opt, void *dst,
                             struct mpv_node *src)
{
    char **dst_copy = NULL;
    int r = m_option_type_keyvalue_list.set(opt, &dst_copy, src);
    if (r >= 0)
        r = check_msg_levels(mp_null_log, dst_copy);

    if (r >= 0)
        m_option_type_keyvalue_list.copy(opt, dst, &dst_copy);
    m_option_type_keyvalue_list.free(&dst_copy);
    return r;
}

const m_option_type_t m_option_type_msglevels = {
    .name = "Output verbosity levels",
    .size  = sizeof(char **),
    .parse = parse_msglevels,
    .print = print_keyvalue_list,
    .copy  = copy_str_list,
    .free  = free_str_list,
    .get   = keyvalue_list_get,
    .set   = set_msglevels,
    .equal = str_list_equal,
};

static int parse_print(struct mp_log *log, const m_option_t *opt,
                       struct bstr name, struct bstr param, void *dst)
{
    ((m_opt_print_fn) opt->priv)(log);
    return M_OPT_EXIT;
}

const m_option_type_t m_option_type_print_fn = {
    .name  = "Print",
    .flags = M_OPT_TYPE_OPTIONAL_PARAM,
    .parse = parse_print,
};

static int parse_dummy_flag(struct mp_log *log, const m_option_t *opt,
                            struct bstr name, struct bstr param, void *dst)
{
    if (param.len) {
        mp_err(log, "Invalid parameter for %.*s flag: %.*s\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_DISALLOW_PARAM;
    }
    return 0;
}

const m_option_type_t m_option_type_dummy_flag = {
    // can only be activated
    .name  = "Flag",
    .flags = M_OPT_TYPE_OPTIONAL_PARAM,
    .parse = parse_dummy_flag,
};

#undef VAL

// Read s sub-option name, or a positional sub-opt value.
// termset is a string containing the set of chars that terminate an option.
// Return 0 on success, M_OPT_ error code otherwise.
// optname is for error reporting.
static int read_subparam(struct mp_log *log, bstr optname, char *termset,
                         bstr *str, bstr *out_subparam)
{
    bstr p = *str;
    bstr subparam = {0};

    if (bstr_eatstart0(&p, "\"")) {
        int optlen = bstrcspn(p, "\"");
        subparam = bstr_splice(p, 0, optlen);
        p = bstr_cut(p, optlen);
        if (!bstr_startswith0(p, "\"")) {
            mp_err(log, "Terminating '\"' missing for '%.*s'\n",
                   BSTR_P(optname));
            return M_OPT_INVALID;
        }
        p = bstr_cut(p, 1);
    } else if (bstr_eatstart0(&p, "[")) {
        bstr s = p;
        int balance = 1;
        while (p.len && balance > 0) {
            if (p.start[0] == '[') {
                balance++;
            } else if (p.start[0] == ']') {
                balance--;
            }
            p = bstr_cut(p, 1);
        }
        if (balance != 0) {
            mp_err(log, "Terminating ']' missing for '%.*s'\n",
                   BSTR_P(optname));
            return M_OPT_INVALID;
        }
        subparam = bstr_splice(s, 0, s.len - p.len - 1);
    } else if (bstr_eatstart0(&p, "%")) {
        int optlen = bstrtoll(p, &p, 0);
        if (!bstr_startswith0(p, "%") || (optlen > p.len - 1)) {
            mp_err(log, "Invalid length %d for '%.*s'\n",
                   optlen, BSTR_P(optname));
            return M_OPT_INVALID;
        }
        subparam = bstr_splice(p, 1, optlen + 1);
        p = bstr_cut(p, optlen + 1);
    } else {
        // Skip until the next character that could possibly be a meta
        // character in option parsing.
        int optlen = bstrcspn(p, termset);
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
static int split_subconf(struct mp_log *log, bstr optname, bstr *str,
                         bstr *out_name, bstr *out_val)
{
    bstr p = *str;
    bstr subparam = {0};
    bstr subopt;
    int r = read_subparam(log, optname, ":=,\\%\"'[]", &p, &subopt);
    if (r < 0)
        return r;
    if (bstr_eatstart0(&p, "=")) {
        r = read_subparam(log, subopt, ":=,\\%\"'[]", &p, &subparam);
        if (r < 0)
            return r;
    }
    *str = p;
    *out_name = subopt;
    *out_val = subparam;
    return 0;
}

#undef VAL

// Split the string on the given split character.
// out_arr is at least max entries long.
// Return number of out_arr entries filled.
static int split_char(bstr str, unsigned char split, int max, bstr *out_arr)
{
    if (max < 1)
        return 0;

    int count = 0;
    while (1) {
        int next = bstrchr(str, split);
        if (next >= 0 && max - count > 1) {
            out_arr[count++] = bstr_splice(str, 0, next);
            str = bstr_cut(str, next + 1);
        } else {
            out_arr[count++] = str;
            break;
        }
    }
    return count;
}

static int parse_color(struct mp_log *log, const m_option_t *opt,
                       struct bstr name, struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    bool is_help = bstr_equals0(param, "help");
    if (is_help)
        goto exit;

    bstr val = param;
    struct m_color color = {0};

    if (bstr_eatstart0(&val, "#")) {
        // #[AA]RRGGBB
        if (val.len != 6 && val.len != 8)
            goto exit;
        bool has_alpha = val.len == 8;
        uint32_t c = bstrtoll(val, &val, 16);
        if (val.len)
            goto exit;
        color = (struct m_color) {
            (c >> 16) & 0xFF,
            (c >> 8) & 0xFF,
            c & 0xFF,
            has_alpha ? (c >> 24) & 0xFF : 0xFF,
        };
    } else {
        bstr comp_str[5];
        int num = split_char(param, '/', 5, comp_str);
        if (num < 1 || num > 4)
            goto exit;
        double comp[4] = {0, 0, 0, 1};
        for (int n = 0; n < num; n++) {
            bstr rest;
            double d = bstrtod(comp_str[n], &rest);
            if (rest.len || !comp_str[n].len || d < 0 || d > 1 || !isfinite(d))
                goto exit;
            comp[n] = d;
        }
        if (num == 2)
            comp[3] = comp[1];
        if (num < 3)
            comp[2] = comp[1] = comp[0];
        color = (struct m_color) { comp[0] * 0xFF, comp[1] * 0xFF,
                                   comp[2] * 0xFF, comp[3] * 0xFF };
    }

    if (dst)
        *((struct m_color *)dst) = color;

    return 1;

exit:
    if (!is_help) {
        mp_err(log, "Option %.*s: invalid color: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
    }
    mp_info(log, "Valid colors must be in the form #RRGGBB or #AARRGGBB (in hex)\n"
            "or in the form 'r/g/b/a', where each component is a value in the\n"
            "range 0.0-1.0. (Also allowed: 'gray', 'gray/a', 'r/g/b').\n");
    return is_help ? M_OPT_EXIT : M_OPT_INVALID;
}

static char *print_color(const m_option_t *opt, const void *val)
{
    const struct m_color *c = val;
    return talloc_asprintf(NULL, "#%02X%02X%02X%02X", c->a, c->r, c->g, c->b);
}

static bool color_equal(const m_option_t *opt, void *a, void *b)
{
    struct m_color *ca = a;
    struct m_color *cb = b;
    return ca->a == cb->a && ca->r == cb->r && ca->g == cb->g && ca->b == cb->b;
}

const m_option_type_t m_option_type_color = {
    .name  = "Color",
    .size  = sizeof(struct m_color),
    .parse = parse_color,
    .print = print_color,
    .copy  = copy_opt,
    .equal = color_equal,
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
    // [[W][xH]][{+-}X{+-}Y][/WS] | [X:Y]
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
            if (!bstr_startswith0(s, "x"))
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
        if (bstr_eatstart0(&s, "/")) {
            bstr rest;
            long long v = bstrtoll(s, &rest, 10);
            if (s.len == rest.len || v < 1 || v > INT_MAX)
                goto error;
            s = rest;
            gm->ws = v;
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

#define APPEND_PER(F, F_PER) \
    res = talloc_asprintf_append(res, "%d%s", gm->F, gm->F_PER ? "%" : "")

static char *print_geometry(const m_option_t *opt, const void *val)
{
    const struct m_geometry *gm = val;
    char *res = talloc_strdup(NULL, "");
    if (gm->wh_valid || gm->xy_valid) {
        if (gm->wh_valid) {
            APPEND_PER(w, w_per);
            res = talloc_asprintf_append(res, "x");
            APPEND_PER(h, h_per);
        }
        if (gm->xy_valid) {
            res = talloc_asprintf_append(res, gm->x_sign ? "-" : "+");
            APPEND_PER(x, x_per);
            res = talloc_asprintf_append(res, gm->y_sign ? "-" : "+");
            APPEND_PER(y, y_per);
        }
        if (gm->ws > 0)
            res = talloc_asprintf_append(res, "/%d", gm->ws);
    }
    return res;
}

#undef APPEND_PER

// xpos,ypos: position of the left upper corner
// widw,widh: width and height of the window
// scrw,scrh: width and height of the current screen
// The input parameters should be set to a centered window (default fallbacks).
void m_geometry_apply(int *xpos, int *ypos, int *widw, int *widh,
                      int scrw, int scrh, bool center, struct m_geometry *gm)
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
        if (center) {
            *xpos += prew / 2 - *widw / 2;
            *ypos += preh / 2 - *widh / 2;
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

static int parse_geometry(struct mp_log *log, const m_option_t *opt,
                          struct bstr name, struct bstr param, void *dst)
{
    bool is_help = bstr_equals0(param, "help");
    if (is_help)
        goto exit;

    struct m_geometry gm;
    if (!parse_geometry_str(&gm, param))
        goto exit;

    if (dst)
        *((struct m_geometry *)dst) = gm;

    return 1;

exit:
    if (!is_help) {
        mp_err(log, "Option %.*s: invalid geometry: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
    }
    mp_info(log,
         "Valid format: [W[%%][xH[%%]]][{+-}X[%%]{+-}Y[%%]] | [X[%%]:Y[%%]]\n");
    return is_help ? M_OPT_EXIT : M_OPT_INVALID;
}

static bool geometry_equal(const m_option_t *opt, void *a, void *b)
{
    struct m_geometry *ga = a;
    struct m_geometry *gb = b;
    return ga->x == gb->x && ga->y == gb->y && ga->w == gb->w && ga->h == gb->h &&
           ga->xy_valid == gb->xy_valid && ga->wh_valid == gb->wh_valid &&
           ga->w_per == gb->w_per && ga->h_per == gb->h_per &&
           ga->x_per == gb->x_per && ga->y_per == gb->y_per &&
           ga->x_sign == gb->x_sign && ga->y_sign == gb->y_sign &&
           ga->ws == gb->ws;
}

const m_option_type_t m_option_type_geometry = {
    .name  = "Window geometry",
    .size  = sizeof(struct m_geometry),
    .parse = parse_geometry,
    .print = print_geometry,
    .copy  = copy_opt,
    .equal = geometry_equal,
};

static int parse_size_box(struct mp_log *log, const m_option_t *opt,
                          struct bstr name, struct bstr param, void *dst)
{
    bool is_help = bstr_equals0(param, "help");
    if (is_help)
        goto exit;

    struct m_geometry gm;
    if (!parse_geometry_str(&gm, param))
        goto exit;

    if (gm.xy_valid)
        goto exit;

    if (dst)
        *((struct m_geometry *)dst) = gm;

    return 1;

exit:
    if (!is_help) {
        mp_err(log, "Option %.*s: invalid size: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
    }
    mp_info(log, "Valid format: W[%%][xH[%%]] or empty string\n");
    return is_help ? M_OPT_EXIT : M_OPT_INVALID;
}

const m_option_type_t m_option_type_size_box = {
    .name  = "Window size",
    .size  = sizeof(struct m_geometry),
    .parse = parse_size_box,
    .print = print_geometry,
    .copy  = copy_opt,
    .equal = geometry_equal,
};

void m_rect_apply(struct mp_rect *rc, int w, int h, struct m_geometry *gm)
{
    *rc = (struct mp_rect){0, 0, w, h};
    if (!w || !h)
        return;
    m_geometry_apply(&rc->x0, &rc->y0, &rc->x1, &rc->y1, w, h, true, gm);
    if (!gm->xy_valid && gm->wh_valid && rc->x1 == 0 && rc->y1 == 0)
        return;
    if (!gm->wh_valid || rc->x1 == 0 || rc->x1 == INT_MIN)
        rc->x1 = w - rc->x0;
    if (!gm->wh_valid || rc->y1 == 0 || rc->y1 == INT_MIN)
        rc->y1 = h - rc->y0;
    if (gm->wh_valid && (gm->w || gm->h))
        rc->x1 += rc->x0;
    if (gm->wh_valid && (gm->w || gm->h))
        rc->y1 += rc->y0;
}

static int parse_rect(struct mp_log *log, const m_option_t *opt,
                      struct bstr name, struct bstr param, void *dst)
{
    bool is_help = bstr_equals0(param, "help");
    if (is_help)
        goto exit;

    struct m_geometry gm;
    if (!parse_geometry_str(&gm, param))
        goto exit;

    bool invalid = gm.x_sign || gm.y_sign || gm.ws;
    invalid |= gm.wh_valid && (gm.w < 0 || gm.h < 0);
    invalid |= gm.wh_valid && !gm.xy_valid && gm.w <= 0 && gm.h <= 0;

    if (invalid)
        goto exit;

    if (dst)
        *((struct m_geometry *)dst) = gm;

    return 1;

exit:
    if (!is_help) {
        mp_err(log, "Option %.*s: invalid rect: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
    }
    mp_info(log, "Valid format: W[%%][xH[%%]][+x+y]\n");
    return is_help ? M_OPT_EXIT : M_OPT_INVALID;
}

const m_option_type_t m_option_type_rect = {
    .name  = "Video rect",
    .size  = sizeof(struct m_geometry),
    .parse = parse_rect,
    .print = print_geometry,
    .copy  = copy_opt,
    .equal = geometry_equal,
};

#include "video/img_format.h"

static int parse_imgfmt(struct mp_log *log, const m_option_t *opt,
                        struct bstr name, struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    if (!bstrcmp0(param, "help")) {
        mp_info(log, "Available formats:");
        char **list = mp_imgfmt_name_list();
        for (int i = 0; list[i]; i++)
            mp_info(log, " %s", list[i]);
        mp_info(log, " no");
        mp_info(log, "\n");
        talloc_free(list);
        return M_OPT_EXIT;
    }

    unsigned int fmt = mp_imgfmt_from_name(param);
    if (!fmt && !bstr_equals0(param, "no")) {
        mp_err(log, "Option %.*s: unknown format name: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    if (dst)
        *((int *)dst) = fmt;

    return 1;
}

static char *print_imgfmt(const m_option_t *opt, const void *val)
{
    int fmt = *(int *)val;
    return talloc_strdup(NULL, fmt ? mp_imgfmt_to_name(fmt) : "no");
}

const m_option_type_t m_option_type_imgfmt = {
    .name  = "Image format",
    .size  = sizeof(int),
    .parse = parse_imgfmt,
    .print = print_imgfmt,
    .copy  = copy_opt,
    .equal = int_equal,
};

static int parse_fourcc(struct mp_log *log, const m_option_t *opt,
                        struct bstr name, struct bstr param, void *dst)
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
            mp_err(log, "Option %.*s: invalid FourCC: '%.*s'\n",
                   BSTR_P(name), BSTR_P(param));
            return M_OPT_INVALID;
        }
    }

    if (dst)
        *((unsigned int *)dst) = value;

    return 1;
}

static char *print_fourcc(const m_option_t *opt, const void *val)
{
    unsigned int fourcc = *(unsigned int *)val;
    return talloc_asprintf(NULL, "%08x", fourcc);
}

const m_option_type_t m_option_type_fourcc = {
    .name  = "FourCC",
    .size  = sizeof(unsigned int),
    .parse = parse_fourcc,
    .print = print_fourcc,
    .copy  = copy_opt,
    .equal = int_equal,
};

#include "audio/format.h"

static int parse_afmt(struct mp_log *log, const m_option_t *opt,
                      struct bstr name, struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    if (!bstrcmp0(param, "help")) {
        mp_info(log, "Available formats:");
        for (int i = 1; i < AF_FORMAT_COUNT; i++)
            mp_info(log, " %s", af_fmt_to_str(i));
        mp_info(log, "\n");
        return M_OPT_EXIT;
    }

    int fmt = 0;
    for (int i = 1; i < AF_FORMAT_COUNT; i++) {
        if (bstr_equals0(param, af_fmt_to_str(i)))
            fmt = i;
    }
    if (!fmt) {
        mp_err(log, "Option %.*s: unknown format name: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    if (dst)
        *((int *)dst) = fmt;

    return 1;
}

static char *print_afmt(const m_option_t *opt, const void *val)
{
    int fmt = *(int *)val;
    return talloc_strdup(NULL, fmt ? af_fmt_to_str(fmt) : "no");
}

const m_option_type_t m_option_type_afmt = {
    .name  = "Audio format",
    .size  = sizeof(int),
    .parse = parse_afmt,
    .print = print_afmt,
    .copy  = copy_opt,
    .equal = int_equal,
};

#include "audio/chmap.h"

static int parse_channels(struct mp_log *log, const m_option_t *opt,
                          struct bstr name, struct bstr param, void *dst)
{
    bool limited = opt->flags & M_OPT_CHANNELS_LIMITED;

    struct m_channels res = {0};

    if (bstr_equals0(param, "help")) {
        mp_chmap_print_help(log);
        if (!limited) {
            mp_info(log, "\nOther values:\n"
                         "    auto-safe\n");
        }
        return M_OPT_EXIT;
    }

    bool auto_safe = bstr_equals0(param, "auto-safe");
    if (bstr_equals0(param, "auto") || bstr_equals0(param, "empty") || auto_safe) {
        if (limited) {
            mp_err(log, "Disallowed parameter.\n");
            return M_OPT_INVALID;
        }
        param.len = 0;
        res.set = true;
        res.auto_safe = auto_safe;
    }

    while (param.len) {
        bstr item;
        if (limited) {
            item = param;
            param.len = 0;
        } else {
            bstr_split_tok(param, ",", &item, &param);
        }

        struct mp_chmap map = {0};
        if (!mp_chmap_from_str(&map, item) || !mp_chmap_is_valid(&map)) {
            mp_err(log, "Invalid channel layout: %.*s\n", BSTR_P(item));
            talloc_free(res.chmaps);
            return M_OPT_INVALID;
        }

        MP_TARRAY_APPEND(NULL, res.chmaps, res.num_chmaps, map);
        res.set = true;
    }

    if (dst) {
        opt->type->free(dst);
        *(struct m_channels *)dst = res;
    } else {
        talloc_free(res.chmaps);
    }

    return 1;
}

static char *print_channels(const m_option_t *opt, const void *val)
{
    const struct m_channels *ch = val;
    if (!ch->set)
        return talloc_strdup(NULL, "");
    if (ch->auto_safe)
        return talloc_strdup(NULL, "auto-safe");
    if (ch->num_chmaps > 0) {
        char *res = talloc_strdup(NULL, "");
        for (int n = 0; n < ch->num_chmaps; n++) {
            if (n > 0)
                res = talloc_strdup_append(res, ",");
            res = talloc_strdup_append(res, mp_chmap_to_str(&ch->chmaps[n]));
        }
        return res;
    }
    return talloc_strdup(NULL, "auto");
}

static void free_channels(void *src)
{
    if (!src)
        return;

    struct m_channels *ch = src;
    talloc_free(ch->chmaps);
    *ch = (struct m_channels){0};
}

static void copy_channels(const m_option_t *opt, void *dst, const void *src)
{
    if (!(dst && src))
        return;

    struct m_channels *ch = dst;
    free_channels(dst);
    *ch = *(struct m_channels *)src;
    ch->chmaps =
        talloc_memdup(NULL, ch->chmaps, sizeof(ch->chmaps[0]) * ch->num_chmaps);
}

static bool channels_equal(const m_option_t *opt, void *a, void *b)
{
    struct m_channels *ca = a;
    struct m_channels *cb = b;

    if (ca->set         != cb->set ||
        ca->auto_safe   != cb->auto_safe ||
        ca->num_chmaps  != cb->num_chmaps)
        return false;

    for (int n = 0; n < ca->num_chmaps; n++) {
        if (!mp_chmap_equals(&ca->chmaps[n], &cb->chmaps[n]))
            return false;
    }

    return true;
}

const m_option_type_t m_option_type_channels = {
    .name  = "Audio channels or channel map",
    .size  = sizeof(struct m_channels),
    .parse = parse_channels,
    .print = print_channels,
    .copy  = copy_channels,
    .free  = free_channels,
    .equal = channels_equal,
};

static int parse_timestring(struct bstr str, double *time, char endchar)
{
    int len;
    unsigned h, m;
    double s;
    *time = 0; /* ensure initialization for error cases */
    bool neg = bstr_eatstart0(&str, "-");
    if (!neg)
        bstr_eatstart0(&str, "+");
    bool sci = bstr_find0(str, "e-") >= 0 || bstr_find0(str, "e+") >= 0;
    /* non-scientific notation timestamps shouldn't contain anymore +/- after this point */
    if (!sci && (bstrchr(str, '-') >= 0 || bstrchr(str, '+') >= 0))
        return 0;
    if (bstr_sscanf(str, "%u:%u:%lf%n", &h, &m, &s, &len) >= 3) {
        if (m >= 60 || s >= 60)
            return 0; /* minutes or seconds are out of range */
        *time = 3600.0 * h + 60 * m + s;
    } else if (bstr_sscanf(str, "%u:%lf%n", &m, &s, &len) >= 2) {
        if (s >= 60)
            return 0; /* seconds are out of range */
        *time = 60.0 * m + s;
    } else if (bstr_sscanf(str, "%lf%n", &s, &len) >= 1) {
        *time = s;
    } else {
        return 0;  /* unsupported time format */
    }
    if (len < str.len && str.start[len] != endchar)
        return 0;  /* invalid extra characters at the end */
    if (!isfinite(*time))
        return 0;
    if (neg)
        *time = -*time;
    return len;
}

#define HAS_NOPTS(opt) ((opt)->flags & M_OPT_ALLOW_NO)

static int parse_time(struct mp_log *log, const m_option_t *opt,
                      struct bstr name, struct bstr param, void *dst)
{
    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    double time = MP_NOPTS_VALUE;
    if (HAS_NOPTS(opt) && bstr_equals0(param, "no")) {
        // nothing
    } else if (!parse_timestring(param, &time, 0)) {
        mp_err(log, "Option %.*s: invalid time: '%.*s'\n",
               BSTR_P(name), BSTR_P(param));
        return M_OPT_INVALID;
    }

    if (dst)
        *(double *)dst = time;
    return 1;
}

static char *print_time(const m_option_t *opt, const void *val)
{
    double pts = *(double *)val;
    if (pts == MP_NOPTS_VALUE && HAS_NOPTS(opt))
        return talloc_strdup(NULL, "no"); // symmetry with parsing
    return talloc_asprintf(NULL, "%f", pts);
}

static char *pretty_print_time(const m_option_t *opt, const void *val)
{
    double pts = *(double *)val;
    if (pts == MP_NOPTS_VALUE && HAS_NOPTS(opt))
        return talloc_strdup(NULL, "no"); // symmetry with parsing
    return mp_format_time(pts, false);
}

static int time_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    if (HAS_NOPTS(opt) && src->format == MPV_FORMAT_STRING) {
        if (strcmp(src->u.string, "no") == 0) {
            *(double *)dst = MP_NOPTS_VALUE;
            return 1;
        }
    }
    return double_set(opt, dst, src);
}

static int time_get(const m_option_t *opt, void *ta_parent,
                      struct mpv_node *dst, void *src)
{
    if (HAS_NOPTS(opt) && *(double *)src == MP_NOPTS_VALUE) {
        dst->format = MPV_FORMAT_STRING;
        dst->u.string = talloc_strdup(ta_parent, "no");
        return 1;
    }
    return double_get(opt, ta_parent, dst, src);
}

const m_option_type_t m_option_type_time = {
    .name  = "Time",
    .size  = sizeof(double),
    .parse = parse_time,
    .print = print_time,
    .pretty_print = pretty_print_time,
    .copy  = copy_opt,
    .add   = add_double,
    .set   = time_set,
    .get   = time_get,
    .equal = double_equal,
};


// Relative time

static int parse_rel_time(struct mp_log *log, const m_option_t *opt,
                          struct bstr name, struct bstr param, void *dst)
{
    struct m_rel_time t = {0};

    if (param.len == 0)
        return M_OPT_MISSING_PARAM;

    if (bstr_equals0(param, "none")) {
        t.type = REL_TIME_NONE;
        goto out;
    }

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

    double time;
    if (parse_timestring(param, &time, 0)) {
        if (bstr_startswith0(param, "+") || bstr_startswith0(param, "-")) {
            t.type = REL_TIME_RELATIVE;
        } else {
            t.type = REL_TIME_ABSOLUTE;
        }
        t.pos = time;
        goto out;
    }

    mp_err(log, "Option %.*s: invalid time or position: '%.*s'\n",
           BSTR_P(name), BSTR_P(param));
    return M_OPT_INVALID;

out:
    if (dst)
        *(struct m_rel_time *)dst = t;
    return 1;
}

static char *print_rel_time(const m_option_t *opt, const void *val)
{
    const struct m_rel_time *t = val;
    switch(t->type) {
    case REL_TIME_ABSOLUTE:
        return talloc_asprintf(NULL, "%g", t->pos);
    case REL_TIME_RELATIVE:
        return talloc_asprintf(NULL, "%+g", t->pos);
    case REL_TIME_CHAPTER:
        return talloc_asprintf(NULL, "#%g", t->pos + 1);
    case REL_TIME_PERCENT:
        return talloc_asprintf(NULL, "%g%%", t->pos);
    }
    return talloc_strdup(NULL, "none");
}

static bool rel_time_equal(const m_option_t *opt, void *a, void *b)
{
    struct m_rel_time *ta = a;
    struct m_rel_time *tb = b;
    return ta->type == tb->type && ta->pos == tb->pos;
}

const m_option_type_t m_option_type_rel_time = {
    .name  = "Relative time or percent position",
    .size  = sizeof(struct m_rel_time),
    .parse = parse_rel_time,
    .print = print_rel_time,
    .copy  = copy_opt,
    .equal = rel_time_equal,
};


//// Objects (i.e. filters, etc) settings

#undef VAL
#define VAL(x) (*(m_obj_settings_t **)(x))

bool m_obj_list_find(struct m_obj_desc *dst, const struct m_obj_list *l,
                     bstr name)
{
    for (int i = 0; ; i++) {
        if (!l->get_desc(dst, i))
            break;
        if (bstr_equals0(name, dst->name))
            return true;
    }
    for (int i = 0; l->aliases[i][0]; i++) {
        const char *aname = l->aliases[i][0];
        const char *alias = l->aliases[i][1];
        if (bstr_equals0(name, aname) && m_obj_list_find(dst, l, bstr0(alias)))
        {
            dst->replaced_name = aname;
            return true;
        }
    }
    return false;
}

static void obj_setting_free(m_obj_settings_t *item)
{
    talloc_free(item->name);
    talloc_free(item->label);
    free_str_list(&(item->attribs));
}

// If at least one item has a label, compare labels only - otherwise ignore them.
static bool obj_setting_match(m_obj_settings_t *a, m_obj_settings_t *b)
{
    bstr la = bstr0(a->label), lb = bstr0(b->label);
    if (la.len || lb.len)
        return bstr_equals(la, lb);

    return m_obj_settings_equal(a, b);
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

    mp_assert(idx >= 0 && idx < num);

    obj_setting_free(&obj_list[idx]);

    // Note: the NULL-terminating element is moved down as part of this
    memmove(&obj_list[idx], &obj_list[idx + 1],
            sizeof(m_obj_settings_t) * (num - idx));

    *p_obj_list = talloc_realloc(NULL, obj_list, struct m_obj_settings, num);
}

// Insert such that *p_obj_list[idx] is set to item.
// If idx < 0, set idx = count + idx + 1 (i.e. -1 inserts it as last element).
// Memory referenced by *item is not copied.
static bool obj_settings_list_insert_at(struct mp_log *log,
                                        m_obj_settings_t **p_obj_list, int idx,
                                        m_obj_settings_t *item)
{
    int num = obj_settings_list_num_items(*p_obj_list);
    // Limit list entries to 100. obj_settings_list is not designed to hold more
    // items, and it quickly starts taking ages to add all items.
    if (num > 100) {
        mp_warn(log, "Object settings list capacity exceeded: "
                     "a maximum of 100 elements is allowed.");
        return false;
    }
    if (idx < 0)
        idx = num + idx + 1;
    mp_assert(idx >= 0 && idx <= num);
    *p_obj_list = talloc_realloc(NULL, *p_obj_list, struct m_obj_settings,
                                 num + 2);
    memmove(*p_obj_list + idx + 1, *p_obj_list + idx,
            (num - idx) * sizeof(m_obj_settings_t));
    (*p_obj_list)[idx] = *item;
    (*p_obj_list)[num + 1] = (m_obj_settings_t){0};
    return true;
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
        if (obj_setting_match(&obj_list[n], item))
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
        d[n].enabled = s[n].enabled;
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
static int get_obj_param(struct mp_log *log, bstr opt_name, bstr obj_name,
                         struct m_config *config, bstr name, bstr val,
                         int flags, bool nopos,
                         int *nold, bstr *out_name, bstr *out_val,
                         char *tmp, size_t tmp_size)
{
    int r;

    if (!config) {
        // Duplicates the logic below, but with unknown parameter types/names.
        if (val.start || nopos) {
            *out_name = name;
            *out_val = val;
        } else {
            val = name;
            // positional fields
            if (val.len == 0) { // Empty field, count it and go on
                (*nold)++;
                return 0;
            }
            // Positional naming convention for/followed by mp_set_avopts().
            snprintf(tmp, tmp_size, "@%d", *nold);
            *out_name = bstr0(tmp);
            *out_val = val;
            (*nold)++;
        }
        return 1;
    }

    // val.start != NULL => of the form name=val (not positional)
    // If it's just "name", and the associated option exists and is a flag,
    // don't accept it as positional argument.
    if (val.start || m_config_option_requires_param(config, name) == 0 || nopos) {
        r = m_config_set_option_cli(config, name, val, flags);
        if (r < 0) {
            if (r == M_OPT_UNKNOWN) {
                mp_err(log, "Option %.*s: %.*s doesn't have a %.*s parameter.\n",
                       BSTR_P(opt_name), BSTR_P(obj_name), BSTR_P(name));
                return M_OPT_UNKNOWN;
            }
            if (r != M_OPT_EXIT)
                mp_err(log, "Option %.*s: "
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
        const char *opt = m_config_get_positional_option(config, *nold);
        if (!opt) {
            mp_err(log, "Option %.*s: %.*s has only %d "
                   "params, so you can't give more than %d unnamed params.\n",
                   BSTR_P(opt_name), BSTR_P(obj_name), *nold, *nold);
            return M_OPT_OUT_OF_RANGE;
        }
        r = m_config_set_option_cli(config, bstr0(opt), val, flags);
        if (r < 0) {
            if (r != M_OPT_EXIT)
                mp_err(log, "Option %.*s: "
                       "Error while parsing %.*s parameter %s (%.*s)\n",
                       BSTR_P(opt_name), BSTR_P(obj_name), opt, BSTR_P(val));
            return r;
        }
        *out_name = bstr0(opt);
        *out_val = val;
        (*nold)++;
        return 1;
    }
}

// Consider -vf a=b:c:d. This parses "b:c:d" into name/value pairs, stored as
// linear array in *_ret. In particular, config contains what options a the
// object takes, and verifies the option values as well.
// If config is NULL, all parameters are accepted without checking.
// _ret set to NULL can be used for checking-only.
// flags can contain any M_SETOPT_* flag.
// desc is optional.
static int m_obj_parse_sub_config(struct mp_log *log, struct bstr opt_name,
                                  struct bstr name, struct bstr *pstr,
                                  struct m_config *config, int flags, bool nopos,
                                  struct m_obj_desc *desc,
                                  const struct m_obj_list *list, char ***ret)
{
    int nold = 0;
    char **args = NULL;
    int num_args = 0;
    int r = 1;
    char tmp[80];

    if (ret) {
        args = *ret;
        while (args && args[num_args])
            num_args++;
    }

    while (pstr->len > 0) {
        bstr fname, fval;
        r = split_subconf(log, opt_name, pstr, &fname, &fval);
        if (r < 0)
            goto exit;

        if (list->use_global_options) {
            mp_err(log, "Option %.*s: this option does not accept sub-options.\n",
                   BSTR_P(opt_name));
            mp_err(log, "Sub-options for --vo and --ao were removed from mpv in "
                   "release 0.23.0.\nSee https://0x0.st/uM for details.\n");
            r = M_OPT_INVALID;
            goto exit;
        }

        if (bstr_equals0(fname, "help"))
            goto print_help;
        r = get_obj_param(log, opt_name, name, config, fname, fval, flags,
                          nopos, &nold, &fname, &fval, tmp, sizeof(tmp));
        if (r < 0)
            goto exit;

        if (r > 0 && ret) {
            MP_TARRAY_APPEND(NULL, args, num_args, bstrto0(NULL, fname));
            MP_TARRAY_APPEND(NULL, args, num_args, bstrto0(NULL, fval));
            MP_TARRAY_APPEND(NULL, args, num_args, NULL);
            MP_TARRAY_APPEND(NULL, args, num_args, NULL);
            num_args -= 2;
        }

        if (!bstr_eatstart0(pstr, ":"))
            break;
    }

    if (ret) {
        if (num_args > 0) {
            *ret = args;
            args = NULL;
        } else {
            *ret = NULL;
        }
    }

    goto exit;

print_help: ;
    if (config) {
        if (desc->print_help)
            desc->print_help(log);
        m_config_print_option_list(config, "*");
    } else if (list->print_unknown_entry_help) {
        list->print_unknown_entry_help(log, mp_tprintf(80, "%.*s", BSTR_P(name)));
    } else {
        mp_warn(log, "Option %.*s: item '%.*s' isn't supported.\n",
               BSTR_P(opt_name), BSTR_P(name));
    }
    r = M_OPT_EXIT;

exit:
    free_str_list(&args);
    return r;
}

// Characters which may appear in a filter name
#define NAMECH "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"

// Parse one item, e.g. -vf a=b:c:d,e=f:g => parse a=b:c:d into "a" and "b:c:d"
static int parse_obj_settings(struct mp_log *log, struct bstr opt, int op,
                              struct bstr *pstr, const struct m_obj_list *list,
                              m_obj_settings_t **_ret)
{
    int r;
    char **plist = NULL;
    struct m_obj_desc desc;
    bstr str = {0};
    bstr label = {0};
    bool nopos = list->disallow_positional_parameters;
    bool enabled = true;

    if (bstr_eatstart0(pstr, "@")) {
        bstr rest;
        if (!bstr_split_tok(*pstr, ":", &label, &rest)) {
            // "@labelname" is the special enable/disable toggle syntax
            if (op == OP_TOGGLE) {
                int idx = bstrspn(*pstr, NAMECH);
                label = bstr_splice(*pstr, 0, idx);
                if (label.len) {
                    *pstr = bstr_cut(*pstr, idx);
                    goto done;
                }
            }
            mp_err(log, "Option %.*s: ':' expected after label.\n", BSTR_P(opt));
            return M_OPT_INVALID;
        }
        *pstr = rest;
        if (label.len == 0) {
            mp_err(log, "Option %.*s: label name expected.\n", BSTR_P(opt));
            return M_OPT_INVALID;
        }
    }

    if (list->allow_disable_entries && bstr_eatstart0(pstr, "!"))
        enabled = false;

    bool has_param = false;
    int idx = bstrspn(*pstr, NAMECH);
    str = bstr_splice(*pstr, 0, idx);
    if (!str.len) {
        mp_err(log, "Option %.*s: item name expected.\n", BSTR_P(opt));
        return M_OPT_INVALID;
    }
    *pstr = bstr_cut(*pstr, idx);
    // video filters use "=", VOs use ":"
    if (bstr_eatstart0(pstr, "=") || bstr_eatstart0(pstr, ":"))
        has_param = true;

    bool skip = false;
    if (m_obj_list_find(&desc, list, str)) {
        if (desc.replaced_name)
            mp_warn(log, "Driver '%s' has been replaced with '%s'!\n",
                   desc.replaced_name, desc.name);
    } else {
        char name[80];
        snprintf(name, sizeof(name), "%.*s", BSTR_P(str));
        if (list->check_unknown_entry && !list->check_unknown_entry(name)) {
            mp_err(log, "Option %.*s: '%.*s' isn't supported.\n",
                   BSTR_P(opt), BSTR_P(str));
            return M_OPT_INVALID;
        }
        desc = (struct m_obj_desc){0};
        skip = true;
    }

    if (has_param) {
        struct m_config *config = NULL;
        if (!skip)
            config = m_config_from_obj_desc_noalloc(NULL, log, &desc);
        r = m_obj_parse_sub_config(log, opt, str, pstr, config,
                                   M_SETOPT_CHECK_ONLY, nopos, &desc, list,
                                   _ret ? &plist : NULL);
        talloc_free(config);
        if (r < 0)
            return r;
    }
    if (!_ret)
        return 1;

done: ;
    m_obj_settings_t item = {
        .name = bstrto0(NULL, str),
        .label = bstrdup0(NULL, label),
        .enabled = enabled,
        .attribs = plist,
    };
    if (!obj_settings_list_insert_at(log, _ret, -1, &item))
        obj_setting_free(&item);
    return 1;
}

// Parse a single entry for -vf-remove (return 0 if not applicable)
// mark_del is bounded by the number of items in dst
static int parse_obj_settings_del(struct mp_log *log, struct bstr opt_name,
                                  struct bstr *param, void *dst, bool *mark_del)
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
            int label_index = 0;
            label_index = obj_settings_list_find_by_label(VAL(dst), label);
            if (label_index >= 0) {
                mark_del[label_index] = true;
            } else {
                mp_warn(log, "Option %.*s: item label @%.*s not found.\n",
                        BSTR_P(opt_name), BSTR_P(label));
            }
        }
        *param = s;
        return 1;
    }
    return 0;
}

static int parse_obj_settings_list(struct mp_log *log, const m_option_t *opt,
                                   struct bstr name, struct bstr param, void *dst)
{
    m_obj_settings_t *res = NULL;
    int op = OP_NONE;
    bool *mark_del = NULL;
    int num_items = obj_settings_list_num_items(dst ? VAL(dst) : 0);
    const struct m_obj_list *ol = opt->priv;
    int ret = 1;

    mp_assert(opt->priv);

    if (bstr_endswith0(name, "-add")) {
        op = OP_ADD;
    } else if (bstr_endswith0(name, "-append")) {
        op = OP_APPEND;
    } else if (bstr_endswith0(name, "-set")) {
        op = OP_NONE;
    } else if (bstr_endswith0(name, "-pre")) {
        op = OP_PRE;
    } else if (bstr_endswith0(name, "-remove")) {
        op = OP_REMOVE;
    } else if (bstr_endswith0(name, "-clr")) {
        op = OP_CLR;
    } else if (bstr_endswith0(name, "-toggle")) {
        op = OP_TOGGLE;
    } else if (bstr_endswith0(name, "-help")) {
        mp_err(log, "Option %s:\n"
                "Supported operations are:\n"
                "  %s-set\n"
                " Overwrite the old list with the given list\n\n"
                "  %s-append\n"
                " Append the given item to the current list\n\n"
                "  %s-add\n"
                " Append the given list to the current list\n\n"
                "  %s-pre\n"
                " Prepend the given list to the current list\n\n"
                "  %s-remove\n"
                " Remove the given item from the current list\n\n"
                "  %s-toggle\n"
                " Add the item to the list, or remove it if it's already added.\n\n"
                "  %s-clr\n"
                " Clear the current list.\n\n",
                opt->name, opt->name, opt->name, opt->name, opt->name,
                opt->name, opt->name, opt->name);

        ret = M_OPT_EXIT;
        goto done;
    }

    if (!bstrcmp0(param, "help")) {
        mp_info(log, "Available %s:\n", ol->description);
        for (int n = 0; ; n++) {
            struct m_obj_desc desc;
            if (!ol->get_desc(&desc, n))
                break;
            if (!desc.hidden) {
                mp_info(log, "  %-16s %s\n",
                       desc.name, desc.description);
            }
        }
        mp_info(log, "\n");
        if (ol->print_help_list)
            ol->print_help_list(log);
        if (!ol->use_global_options) {
            mp_info(log, "Get help on individual entries via: --%s=entry=help\n",
                    opt->name);
        }
        ret = M_OPT_EXIT;
        goto done;
    }

    if (op == OP_CLR) {
        if (param.len) {
            mp_err(log, "Option %.*s: -clr does not take an argument.\n",
                   BSTR_P(name));
            ret = M_OPT_INVALID;
            goto done;
        }
        if (dst)
            free_obj_settings_list(dst);
        ret = 0;
        goto done;
    } else if (op == OP_REMOVE) {
        mark_del = talloc_zero_array(NULL, bool, num_items + 1);
    }

    if (op != OP_NONE && param.len == 0) {
        ret = M_OPT_MISSING_PARAM;
        goto done;
    }

    while (param.len > 0) {
        int r = 0;
        if (op == OP_REMOVE)
            r = parse_obj_settings_del(log, name, &param, dst, mark_del);
        if (r == 0) {
            r = parse_obj_settings(log, name, op, &param, ol, dst ? &res : NULL);
        }
        if (r < 0) {
            free_obj_settings_list(&res);
            ret = r;
            goto done;
        }
        if (param.len > 0) {
            const char sep[2] = {OPTION_LIST_SEPARATOR, 0};
            if (!bstr_eatstart0(&param, sep)) {
                free_obj_settings_list(&res);
                ret = M_OPT_INVALID;
                goto done;
            }
            if (param.len == 0) {
                if (!ol->allow_trailer) {
                    free_obj_settings_list(&res);
                    ret = M_OPT_INVALID;
                    goto done;
                }
                if (dst) {
                    m_obj_settings_t item = {
                        .name = talloc_strdup(NULL, ""),
                    };
                    if (!obj_settings_list_insert_at(log, &res, -1, &item))
                        obj_setting_free(&item);
                }
            }
        }
    }

    if (op != OP_NONE && res && res[0].name && res[1].name) {
        if (op == OP_APPEND) {
            mp_err(log, "Option %.*s: -append takes only 1 item (no ',').\n",
                   BSTR_P(name));
            free_obj_settings_list(&res);
            ret = M_OPT_INVALID;
            goto done;
        }
    }

    if (dst) {
        m_obj_settings_t *list = VAL(dst);
        if (op == OP_PRE) {
            int prepend_counter = 0;
            for (int n = 0; res && res[n].name; n++) {
                int label = obj_settings_list_find_by_label0(list, res[n].label);
                if (label < 0) {
                    if (!obj_settings_list_insert_at(log, &list, prepend_counter, &res[n]))
                        obj_setting_free(&res[n]);
                    prepend_counter++;
                } else {
                    // Prefer replacement semantics, instead of actually
                    // prepending.
                    obj_setting_free(&list[label]);
                    list[label] = res[n];
                }
            }
            talloc_free(res);
        } else if (op == OP_ADD || op == OP_APPEND) {
            for (int n = 0; res && res[n].name; n++) {
                int label = obj_settings_list_find_by_label0(list, res[n].label);
                if (label < 0) {
                    if (!obj_settings_list_insert_at(log, &list, -1, &res[n]))
                        obj_setting_free(&res[n]);
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
                if (res[n].label && !res[n].name[0]) {
                    // Toggle enable/disable special case.
                    int found =
                        obj_settings_list_find_by_label0(list, res[n].label);
                    if (found < 0) {
                        mp_warn(log, "Option %.*s: Label %s not found\n",
                                BSTR_P(name), res[n].label);
                    } else {
                        list[found].enabled = !list[found].enabled;
                    }
                    obj_setting_free(&res[n]);
                } else {
                    int found = obj_settings_find_by_content(list, &res[n]);
                    if (found < 0) {
                        if (!obj_settings_list_insert_at(log, &list, -1, &res[n]))
                            obj_setting_free(&res[n]);
                    } else {
                        obj_settings_list_del_at(&list, found);
                        obj_setting_free(&res[n]);
                    }
                }
            }
            talloc_free(res);
        } else if (op == OP_REMOVE) {
            for (int n = num_items - 1; n >= 0; n--) {
                if (mark_del[n])
                    obj_settings_list_del_at(&list, n);
            }
            for (int n = 0; res && res[n].name; n++) {
                int found = obj_settings_find_by_content(list, &res[n]);
                if (found >= 0)
                    obj_settings_list_del_at(&list, found);
            }
            free_obj_settings_list(&res);
        } else {
            mp_assert(op == OP_NONE);
            free_obj_settings_list(&list);
            list = res;
        }
        VAL(dst) = list;
    }

done:
    talloc_free(mark_del);
    return ret;
}

static void append_param(char **res, char *param)
{
    if (strspn(param, NAMECH) == strlen(param)) {
        *res = talloc_strdup_append(*res, param);
    } else {
        // Simple escaping: %BYTECOUNT%STRING
        *res = talloc_asprintf_append(*res, "%%%zd%%%s", strlen(param), param);
    }
}

static char *print_obj_settings_list(const m_option_t *opt, const void *val)
{
    m_obj_settings_t *list = VAL(val);
    char *res = talloc_strdup(NULL, "");
    for (int n = 0; list && list[n].name; n++) {
        m_obj_settings_t *entry = &list[n];
        if (n > 0)
            res = talloc_strdup_append(res, ",");
        // Assume labels and names don't need escaping
        if (entry->label && entry->label[0])
            res = talloc_asprintf_append(res, "@%s:", entry->label);
        if (!entry->enabled)
            res = talloc_strdup_append(res, "!");
        res = talloc_strdup_append(res, entry->name);
        if (entry->attribs && entry->attribs[0]) {
            res = talloc_strdup_append(res, "=");
            for (int i = 0; entry->attribs[i * 2 + 0]; i++) {
                if (i > 0)
                    res = talloc_strdup_append(res, ":");
                append_param(&res, entry->attribs[i * 2 + 0]);
                res = talloc_strdup_append(res, "=");
                append_param(&res, entry->attribs[i * 2 + 1]);
            }
        }
    }
    return res;
}

static int set_obj_settings_list(const m_option_t *opt, void *dst,
                                 struct mpv_node *src)
{
    if (src->format != MPV_FORMAT_NODE_ARRAY)
        return M_OPT_INVALID;
    m_obj_settings_t *entries =
        talloc_zero_array(NULL, m_obj_settings_t, src->u.list->num + 1);
    for (int n = 0; n < src->u.list->num; n++) {
        m_obj_settings_t *entry = &entries[n];
        entry->enabled = true;
        if (src->u.list->values[n].format != MPV_FORMAT_NODE_MAP)
            goto error;
        struct mpv_node_list *src_entry = src->u.list->values[n].u.list;
        for (int i = 0; i < src_entry->num; i++) {
            const char *key = src_entry->keys[i];
            struct mpv_node *val = &src_entry->values[i];
            if (strcmp(key, "name") == 0) {
                if (val->format != MPV_FORMAT_STRING)
                    goto error;
                entry->name = talloc_strdup(NULL, val->u.string);
            } else if (strcmp(key, "label") == 0) {
                if (val->format != MPV_FORMAT_STRING)
                    goto error;
                entry->label = talloc_strdup(NULL, val->u.string);
            } else if (strcmp(key, "enabled") == 0) {
                if (val->format != MPV_FORMAT_FLAG)
                    goto error;
                entry->enabled = val->u.flag;
            } else if (strcmp(key, "params") == 0) {
                if (val->format != MPV_FORMAT_NODE_MAP)
                    goto error;
                struct mpv_node_list *src_params = val->u.list;
                entry->attribs =
                    talloc_zero_array(NULL, char*, (src_params->num + 1) * 2);
                for (int x = 0; x < src_params->num; x++) {
                    if (src_params->values[x].format != MPV_FORMAT_STRING)
                        goto error;
                    entry->attribs[x * 2 + 0] =
                        talloc_strdup(NULL, src_params->keys[x]);
                    entry->attribs[x * 2 + 1] =
                        talloc_strdup(NULL, src_params->values[x].u.string);
                }
            }
        }
    }
    free_obj_settings_list(dst);
    VAL(dst) = entries;
    return 0;
error:
    free_obj_settings_list(&entries);
    return M_OPT_INVALID;
}

static struct mpv_node *add_array_entry(struct mpv_node *dst)
{
    struct mpv_node_list *list = dst->u.list;
    mp_assert(dst->format == MPV_FORMAT_NODE_ARRAY&& dst->u.list);
    MP_TARRAY_GROW(list, list->values, list->num);
    return &list->values[list->num++];
}

static struct mpv_node *add_map_entry(struct mpv_node *dst, const char *key)
{
    struct mpv_node_list *list = dst->u.list;
    mp_assert(dst->format == MPV_FORMAT_NODE_MAP && dst->u.list);
    MP_TARRAY_GROW(list, list->values, list->num);
    MP_TARRAY_GROW(list, list->keys, list->num);
    list->keys[list->num] = talloc_strdup(list, key);
    return &list->values[list->num++];
}

static void add_map_string(struct mpv_node *dst, const char *key, const char *val)
{
    struct mpv_node *entry = add_map_entry(dst, key);
    entry->format = MPV_FORMAT_STRING;
    entry->u.string = talloc_strdup(dst->u.list, val);
}

static int get_obj_settings_list(const m_option_t *opt, void *ta_parent,
                                 struct mpv_node *dst, void *val)
{
    m_obj_settings_t *list = VAL(val);
    dst->format = MPV_FORMAT_NODE_ARRAY;
    dst->u.list = talloc_zero(ta_parent, struct mpv_node_list);
    ta_parent = dst->u.list;
    for (int n = 0; list && list[n].name; n++) {
        m_obj_settings_t *entry = &list[n];
        struct mpv_node *nentry = add_array_entry(dst);
        nentry->format = MPV_FORMAT_NODE_MAP;
        nentry->u.list = talloc_zero(ta_parent, struct mpv_node_list);
        add_map_string(nentry, "name", entry->name);
        if (entry->label && entry->label[0])
            add_map_string(nentry, "label", entry->label);
        struct mpv_node *enabled = add_map_entry(nentry, "enabled");
        enabled->format = MPV_FORMAT_FLAG;
        enabled->u.flag = entry->enabled;
        struct mpv_node *params = add_map_entry(nentry, "params");
        params->format = MPV_FORMAT_NODE_MAP;
        params->u.list = talloc_zero(ta_parent, struct mpv_node_list);
        for (int i = 0; entry->attribs && entry->attribs[i * 2 + 0]; i++) {
            add_map_string(params, entry->attribs[i * 2 + 0],
                                   entry->attribs[i * 2 + 1]);
        }
    }
    return 1;
}

static bool obj_settings_list_equal(const m_option_t *opt, void *pa, void *pb)
{
    struct m_obj_settings *a = VAL(pa);
    struct m_obj_settings *b = VAL(pb);

    if (a == b || !a || !b)
        return a == b || (!a && !b[0].name) || (!b && !a[0].name);

    for (int n = 0; a[n].name || b[n].name; n++) {
        if (!a[n].name || !b[n].name)
            return false;
        if (!m_obj_settings_equal(&a[n], &b[n]))
            return false;
    }

    return true;
}

bool m_obj_settings_equal(struct m_obj_settings *a, struct m_obj_settings *b)
{
    if (!str_equal(NULL, &a->name, &b->name))
        return false;

    if (!str_equal(NULL, &a->label, &b->label))
        return false;

    if (a->enabled != b->enabled)
        return false;

    return str_list_equal(NULL, &a->attribs, &b->attribs);
}

const m_option_type_t m_option_type_obj_settings_list = {
    .name  = "Object settings list",
    .size  = sizeof(m_obj_settings_t *),
    .parse = parse_obj_settings_list,
    .print = print_obj_settings_list,
    .copy  = copy_obj_settings_list,
    .free  = free_obj_settings_list,
    .set   = set_obj_settings_list,
    .get   = get_obj_settings_list,
    .equal = obj_settings_list_equal,
    .actions = (const struct m_option_action[]){
        {"add"},
        {"append"},
        {"clr",     M_OPT_TYPE_OPTIONAL_PARAM},
        {"help",    M_OPT_TYPE_OPTIONAL_PARAM},
        {"pre"},
        {"set"},
        {"toggle"},
        {"remove"},
        {0}
    },
};

#undef VAL
#define VAL(x) (*(struct mpv_node *)(x))

static int parse_node(struct mp_log *log, const m_option_t *opt,
                      struct bstr name, struct bstr param, void *dst)
{
    // Maybe use JSON?
    mp_err(log, "option type doesn't accept strings");
    return M_OPT_INVALID;
}

static char *print_node(const m_option_t *opt, const void *val)
{
    char *t = talloc_strdup(NULL, "");
    if (json_write(&t, &VAL(val)) < 0) {
        talloc_free(t);
        t = NULL;
    }
    return t;
}

static char *pretty_print_node(const m_option_t *opt, const void *val)
{
    char *t = talloc_strdup(NULL, "");
    if (json_write_pretty(&t, &VAL(val)) < 0) {
        talloc_free(t);
        t = NULL;
    }
    return t;
}

static void dup_node(void *ta_parent, struct mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_STRING:
        node->u.string = talloc_strdup(ta_parent, node->u.string);
        break;
    case MPV_FORMAT_NODE_ARRAY:
    case MPV_FORMAT_NODE_MAP: {
        struct mpv_node_list *oldlist = node->u.list;
        struct mpv_node_list *new = talloc_zero(ta_parent, struct mpv_node_list);
        node->u.list = new;
        if (oldlist->num > 0) {
            *new = *oldlist;
            new->values = talloc_array(new, struct mpv_node, new->num);
            for (int n = 0; n < new->num; n++) {
                new->values[n] = oldlist->values[n];
                dup_node(new, &new->values[n]);
            }
            if (node->format == MPV_FORMAT_NODE_MAP) {
                new->keys = talloc_array(new, char*, new->num);
                for (int n = 0; n < new->num; n++)
                    new->keys[n] = talloc_strdup(new, oldlist->keys[n]);
            }
        }
        break;
    }
    case MPV_FORMAT_BYTE_ARRAY: {
        struct mpv_byte_array *old = node->u.ba;
        struct mpv_byte_array *new = talloc_zero(ta_parent, struct mpv_byte_array);
        node->u.ba = new;
        if (old->size > 0) {
            *new = *old;
            new->data = talloc_memdup(new, old->data, old->size);
        }
        break;
    }
    case MPV_FORMAT_NONE:
    case MPV_FORMAT_FLAG:
    case MPV_FORMAT_INT64:
    case MPV_FORMAT_DOUBLE:
        break;
    default:
        // unknown entry - mark as invalid
        node->format = (mpv_format)-1;
    }
}

static void copy_node(const m_option_t *opt, void *dst, const void *src)
{
    static_assert(sizeof(struct mpv_node) <= sizeof(union m_option_value), "");

    if (!(dst && src))
        return;

    opt->type->free(dst);
    VAL(dst) = VAL(src);
    dup_node(NULL, &VAL(dst));
}

void *node_get_alloc(struct mpv_node *node)
{
    // Assume it was allocated with copy_node(), which allocates all
    // sub-nodes with the parent node as talloc parent.
    switch (node->format) {
    case MPV_FORMAT_STRING:
        return node->u.string;
    case MPV_FORMAT_NODE_ARRAY:
    case MPV_FORMAT_NODE_MAP:
        return node->u.list;
    default:
        return NULL;
    }
}

static void free_node(void *src)
{
    if (src) {
        struct mpv_node *node = &VAL(src);
        talloc_free(node_get_alloc(node));
        *node = (struct mpv_node){{0}};
    }
}

// idempotent functions for convenience
static int node_set(const m_option_t *opt, void *dst, struct mpv_node *src)
{
    copy_node(opt, dst, src);
    return 1;
}

static int node_get(const m_option_t *opt, void *ta_parent,
                    struct mpv_node *dst, void *src)
{
    *dst = VAL(src);
    dup_node(ta_parent, dst);
    return 1;
}

static bool node_equal(const m_option_t *opt, void *a, void *b)
{
    return equal_mpv_node(&VAL(a), &VAL(b));
}

const m_option_type_t m_option_type_node = {
    .name  = "Complex",
    .size  = sizeof(struct mpv_node),
    .parse = parse_node,
    .print = print_node,
    .pretty_print = pretty_print_node,
    .copy  = copy_node,
    .free  = free_node,
    .set   = node_set,
    .get   = node_get,
    .equal = node_equal,
};

static int parse_cycle_dir(struct mp_log *log, const struct m_option *opt,
                           struct bstr name, struct bstr param, void *dst)
{
    double val;
    if (bstrcmp0(param, "up") == 0) {
        val = +1;
    } else if (bstrcmp0(param, "down") == 0) {
        val = -1;
    } else {
        return m_option_type_double.parse(log, opt, name, param, dst);
    }
    *(double *)dst = val;
    return 1;
}

static char *print_cycle_dir(const m_option_t *opt, const void *val)
{
    return talloc_asprintf(NULL, "%f", *(double *)val);
}

const m_option_type_t m_option_type_cycle_dir = {
    .name = "up|down",
    .parse = parse_cycle_dir,
    .print = print_cycle_dir,
    .copy = copy_opt,
    .size = sizeof(double),
};

// Special-cased by m_config.c.
const m_option_type_t m_option_type_alias = {
    .name  = "alias",
};
const m_option_type_t m_option_type_cli_alias = {
    .name  = "alias",
};
const m_option_type_t m_option_type_removed = {
    .name  = "removed",
};
const m_option_type_t m_option_type_subconfig = {
    .name = "Subconfig",
};
