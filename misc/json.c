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

/* JSON parser:
 *
 * Unlike standard JSON, \u escapes don't allow you to specify UTF-16 surrogate
 * pairs. There may be some differences how numbers are parsed (this parser
 * doesn't verify what's passed to strtod(), and also prefers parsing numbers
 * as integers with stroll() if possible).
 *
 * It has some non-standard extensions which shouldn't conflict with JSON:
 *  - a list or object item can have a trailing ","
 *  - object syntax accepts "=" in addition of ":"
 *  - object keys can be unquoted, if they start with a character in [A-Za-z_]
 *    and contain only characters in [A-Za-z0-9_]
 *  - byte escapes with "\xAB" are allowed (with AB being a 2 digit hex number)
 *
 * Also see: http://tools.ietf.org/html/rfc8259
 *
 * JSON writer:
 *
 * Doesn't insert whitespace. It's literally a waste of space.
 *
 * Can output invalid UTF-8, if input is invalid UTF-8. Consumers are supposed
 * to deal with somehow: either by using byte-strings for JSON, or by running
 * a "fixup" pass on the input data. The latter could for example change
 * invalid UTF-8 sequences to replacement characters.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>

#include <mpv/client.h>

#include "common/common.h"
#include "misc/bstr.h"
#include "misc/ctype.h"

#include "json.h"

static bool eat_c(char **s, char c)
{
    if (**s == c) {
        *s += 1;
        return true;
    }
    return false;
}

static void eat_ws(char **src)
{
    while (1) {
        char c = **src;
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            return;
        *src += 1;
    }
}

void json_skip_whitespace(char **src)
{
    eat_ws(src);
}

static int read_id(void *ta_parent, struct mpv_node *dst, char **src)
{
    char *start = *src;
    if (!mp_isalpha(**src) && **src != '_')
        return -1;
    while (mp_isalnum(**src) || **src == '_')
        *src += 1;
    if (**src == ' ') {
        **src = '\0'; // we're allowed to mutate it => can avoid the strndup
        *src += 1;
    } else {
        start = talloc_strndup(ta_parent, start, *src - start);
    }
    dst->format = MPV_FORMAT_STRING;
    dst->u.string = start;
    return 0;
}

static int read_str(void *ta_parent, struct mpv_node *dst, char **src)
{
    if (!eat_c(src, '"'))
        return -1; // not a string
    char *str = *src;
    char *cur = str;
    bool has_escapes = false;
    while (cur[0] && cur[0] != '"') {
        if (cur[0] == '\\') {
            has_escapes = true;
            // skip >\"< and >\\< (latter to handle >\\"< correctly)
            if (cur[1] == '"' || cur[1] == '\\')
                cur++;
        }
        cur++;
    }
    if (cur[0] != '"')
        return -1; // invalid termination
    // Mutate input string so we have a null-terminated string to the literal.
    // This is a stupid micro-optimization, so we can avoid allocation.
    cur[0] = '\0';
    *src = cur + 1;
    if (has_escapes) {
        bstr unescaped = {0};
        bstr r = bstr0(str);
        if (!mp_append_escaped_string(ta_parent, &unescaped, &r))
            return -1; // broken escapes
        str = unescaped.start; // the function guarantees null-termination
    }
    dst->format = MPV_FORMAT_STRING;
    dst->u.string = str;
    return 0;
}

static int read_sub(void *ta_parent, struct mpv_node *dst, char **src,
                    int max_depth)
{
    bool is_arr = eat_c(src, '[');
    bool is_obj = !is_arr && eat_c(src, '{');
    if (!is_arr && !is_obj)
        return -1; // not an array or object
    char term = is_obj ? '}' : ']';
    struct mpv_node_list *list = talloc_zero(ta_parent, struct mpv_node_list);
    while (1) {
        eat_ws(src);
        if (eat_c(src, term))
            break;
        if (list->num > 0 && !eat_c(src, ','))
            return -1; // missing ','
        eat_ws(src);
        // non-standard extension: allow a trailing ","
        if (eat_c(src, term))
            break;
        if (is_obj) {
            struct mpv_node keynode;
            // non-standard extension: allow unquoted strings as keys
            if (read_id(list, &keynode, src) < 0 &&
                read_str(list, &keynode, src) < 0)
                return -1; // key is not a string
            eat_ws(src);
            // non-standard extension: allow "=" instead of ":"
            if (!eat_c(src, ':') && !eat_c(src, '='))
                return -1; // ':' missing
            eat_ws(src);
            MP_TARRAY_GROW(list, list->keys, list->num);
            list->keys[list->num] = keynode.u.string;
        }
        MP_TARRAY_GROW(list, list->values, list->num);
        if (json_parse(ta_parent, &list->values[list->num], src, max_depth) < 0)
            return -1;
        list->num++;
    }
    dst->format = is_obj ? MPV_FORMAT_NODE_MAP : MPV_FORMAT_NODE_ARRAY;
    dst->u.list = list;
    return 0;
}

/* Parse the string in *src as JSON, and write the result into *dst.
 * max_depth limits the recursion and JSON tree depth.
 * Warning: this overwrites the input string (what *src points to)!
 * Returns:
 *   0: success, *dst is valid, *src points to the end (the caller must check
 *      whether *src really terminates)
 *  -1: failure, *dst is invalid, there may be dead allocs under ta_parent
 *      (ta_free_children(ta_parent) is the only way to free them)
 * The input string can be mutated in both cases. *dst might contain string
 * elements, which point into the (mutated) input string.
 */
int json_parse(void *ta_parent, struct mpv_node *dst, char **src, int max_depth)
{
    max_depth -= 1;
    if (max_depth < 0)
        return -1;

    eat_ws(src);

    char c = **src;
    if (!c)
        return -1; // early EOF
    if (c == 'n' && strncmp(*src, "null", 4) == 0) {
        *src += 4;
        dst->format = MPV_FORMAT_NONE;
        return 0;
    } else if (c == 't' && strncmp(*src, "true", 4) == 0) {
        *src += 4;
        dst->format = MPV_FORMAT_FLAG;
        dst->u.flag = 1;
        return 0;
    } else if (c == 'f' && strncmp(*src, "false", 5) == 0) {
        *src += 5;
        dst->format = MPV_FORMAT_FLAG;
        dst->u.flag = 0;
        return 0;
    } else if (c == '"') {
        return read_str(ta_parent, dst, src);
    } else if (c == '[' || c == '{') {
        return read_sub(ta_parent, dst, src, max_depth);
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        // The number could be either a float or an int. JSON doesn't make a
        // difference, but the client API does.
        char *nsrci = *src, *nsrcf = *src;
        errno = 0;
        long long int numi = strtoll(*src, &nsrci, 0);
        if (errno)
            nsrci = *src;
        errno = 0;
        double numf = strtod(*src, &nsrcf);
        if (errno)
            nsrcf = *src;
        if (nsrci >= nsrcf) {
            *src = nsrci;
            dst->format = MPV_FORMAT_INT64; // long long is usually 64 bits
            dst->u.int64 = numi;
            return 0;
        }
        if (nsrcf > *src && isfinite(numf)) {
            *src = nsrcf;
            dst->format = MPV_FORMAT_DOUBLE;
            dst->u.double_ = numf;
            return 0;
        }
        return -1;
    }
    return -1; // character doesn't start a valid token
}


#define APPEND(b, s) bstr_xappend(NULL, (b), bstr0(s))

static const char special_escape[] = {
    ['\b'] = 'b',
    ['\f'] = 'f',
    ['\n'] = 'n',
    ['\r'] = 'r',
    ['\t'] = 't',
};

static void write_json_str(bstr *b, unsigned char *str)
{
    mp_assert(str);

    APPEND(b, "\"");
    while (1) {
        unsigned char *cur = str;
        while (cur[0] >= 32 && cur[0] != '"' && cur[0] != '\\')
            cur++;
        if (!cur[0])
            break;
        bstr_xappend(NULL, b, (bstr){str, cur - str});
        if (cur[0] == '\"') {
            bstr_xappend(NULL, b, (bstr){"\\\"", 2});
        } else if (cur[0] == '\\') {
            bstr_xappend(NULL, b, (bstr){"\\\\", 2});
        } else if (cur[0] < sizeof(special_escape) && special_escape[cur[0]]) {
            bstr_xappend_asprintf(NULL, b, "\\%c", special_escape[cur[0]]);
        } else {
            bstr_xappend_asprintf(NULL, b, "\\u%04x", (unsigned char)cur[0]);
        }
        str = cur + 1;
    }
    APPEND(b, str);
    APPEND(b, "\"");
}

static void add_indent(bstr *b, int indent)
{
    if (indent < 0)
        return;
    bstr_xappend(NULL, b, bstr0("\n"));
    for (int n = 0; n < indent; n++)
        bstr_xappend(NULL, b, bstr0(" "));
}

int json_append(bstr *b, const struct mpv_node *src, int indent)
{
    switch (src->format) {
    case MPV_FORMAT_NONE:
        APPEND(b, "null");
        return 0;
    case MPV_FORMAT_FLAG:
        APPEND(b, src->u.flag ? "true" : "false");
        return 0;
    case MPV_FORMAT_INT64:
        bstr_xappend_asprintf(NULL, b, "%"PRId64, src->u.int64);
        return 0;
    case MPV_FORMAT_DOUBLE: {
        const char *px = (isfinite(src->u.double_) || indent == 0) ? "" : "\"";
        bstr_xappend_asprintf(NULL, b, "%s%f%s", px, src->u.double_, px);
        return 0;
    }
    case MPV_FORMAT_STRING:
        if (indent == 0)
            APPEND(b, src->u.string);
        else
            write_json_str(b, src->u.string);
        return 0;
    case MPV_FORMAT_NODE_ARRAY:
    case MPV_FORMAT_NODE_MAP: {
        struct mpv_node_list *list = src->u.list;
        bool is_obj = src->format == MPV_FORMAT_NODE_MAP;
        APPEND(b, is_obj ? "{" : "[");
        int next_indent = indent >= 0 ? indent + 1 : -1;
        for (int n = 0; n < list->num; n++) {
            if (n)
                APPEND(b, ",");
            add_indent(b, next_indent);
            if (is_obj) {
                write_json_str(b, list->keys[n]);
                APPEND(b, ":");
            }
            json_append(b, &list->values[n], next_indent);
        }
        add_indent(b, indent);
        APPEND(b, is_obj ? "}" : "]");
        return 0;
    }
    }
    return -1; // unknown format
}

static int json_append_str(char **dst, struct mpv_node *src, int indent)
{
    bstr buffer = bstr0(*dst);
    int r = json_append(&buffer, src, indent);
    *dst = buffer.start;
    return r;
}

/* Write the contents of *src as JSON, and append the JSON string to *dst.
 * This will use strlen() to determine the start offset, and ta_get_size()
 * and ta_realloc() to extend the memory allocation of *dst.
 * Returns: 0 on success, <0 on failure.
 */
int json_write(char **dst, struct mpv_node *src)
{
    return json_append_str(dst, src, -1);
}

// Same as json_write(), but add whitespace to make it readable.
int json_write_pretty(char **dst, struct mpv_node *src)
{
    return json_append_str(dst, src, 0);
}
