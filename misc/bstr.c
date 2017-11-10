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

#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include <libavutil/common.h>

#include "mpv_talloc.h"

#include "common/common.h"
#include "misc/ctype.h"
#include "bstr.h"

int bstrcmp(struct bstr str1, struct bstr str2)
{
    int ret = 0;
    if (str1.len && str2.len)
        ret = memcmp(str1.start, str2.start, FFMIN(str1.len, str2.len));

    if (!ret) {
        if (str1.len == str2.len)
            return 0;
        else if (str1.len > str2.len)
            return 1;
        else
            return -1;
    }
    return ret;
}

int bstrcasecmp(struct bstr str1, struct bstr str2)
{
    int ret = 0;
    if (str1.len && str2.len)
        ret = strncasecmp(str1.start, str2.start, FFMIN(str1.len, str2.len));

    if (!ret) {
        if (str1.len == str2.len)
            return 0;
        else if (str1.len > str2.len)
            return 1;
        else
            return -1;
    }
    return ret;
}

int bstrchr(struct bstr str, int c)
{
    for (int i = 0; i < str.len; i++)
        if (str.start[i] == c)
            return i;
    return -1;
}

int bstrrchr(struct bstr str, int c)
{
    for (int i = str.len - 1; i >= 0; i--)
        if (str.start[i] == c)
            return i;
    return -1;
}

int bstrcspn(struct bstr str, const char *reject)
{
    int i;
    for (i = 0; i < str.len; i++)
        if (strchr(reject, str.start[i]))
            break;
    return i;
}

int bstrspn(struct bstr str, const char *accept)
{
    int i;
    for (i = 0; i < str.len; i++)
        if (!strchr(accept, str.start[i]))
            break;
    return i;
}

int bstr_find(struct bstr haystack, struct bstr needle)
{
    for (int i = 0; i < haystack.len; i++)
        if (bstr_startswith(bstr_splice(haystack, i, haystack.len), needle))
            return i;
    return -1;
}

struct bstr bstr_lstrip(struct bstr str)
{
    while (str.len && mp_isspace(*str.start)) {
        str.start++;
        str.len--;
    }
    return str;
}

struct bstr bstr_strip(struct bstr str)
{
    str = bstr_lstrip(str);
    while (str.len && mp_isspace(str.start[str.len - 1]))
        str.len--;
    return str;
}

struct bstr bstr_split(struct bstr str, const char *sep, struct bstr *rest)
{
    int start;
    for (start = 0; start < str.len; start++)
        if (!strchr(sep, str.start[start]))
            break;
    str = bstr_cut(str, start);
    int end = bstrcspn(str, sep);
    if (rest) {
        *rest = bstr_cut(str, end);
    }
    return bstr_splice(str, 0, end);
}

// Unlike with bstr_split(), tok is a string, and not a set of char.
// If tok is in str, return true, and: concat(out_left, tok, out_right) == str
// Otherwise, return false, and set out_left==str, out_right==""
bool bstr_split_tok(bstr str, const char *tok, bstr *out_left, bstr *out_right)
{
    bstr bsep = bstr0(tok);
    int pos = bstr_find(str, bsep);
    if (pos < 0)
        pos = str.len;
    *out_left = bstr_splice(str, 0, pos);
    *out_right = bstr_cut(str, pos + bsep.len);
    return pos != str.len;
}

struct bstr bstr_splice(struct bstr str, int start, int end)
{
    if (start < 0)
        start += str.len;
    if (end < 0)
        end += str.len;
    end = FFMIN(end, str.len);
    start = FFMAX(start, 0);
    end = FFMAX(end, start);
    str.start += start;
    str.len = end - start;
    return str;
}

long long bstrtoll(struct bstr str, struct bstr *rest, int base)
{
    str = bstr_lstrip(str);
    char buf[51];
    int len = FFMIN(str.len, 50);
    memcpy(buf, str.start, len);
    buf[len] = 0;
    char *endptr;
    long long r = strtoll(buf, &endptr, base);
    if (rest)
        *rest = bstr_cut(str, endptr - buf);
    return r;
}

double bstrtod(struct bstr str, struct bstr *rest)
{
    str = bstr_lstrip(str);
    char buf[101];
    int len = FFMIN(str.len, 100);
    memcpy(buf, str.start, len);
    buf[len] = 0;
    char *endptr;
    double r = strtod(buf, &endptr);
    if (rest)
        *rest = bstr_cut(str, endptr - buf);
    return r;
}

struct bstr *bstr_splitlines(void *talloc_ctx, struct bstr str)
{
    if (str.len == 0)
        return NULL;
    int count = 0;
    for (int i = 0; i < str.len; i++)
        if (str.start[i] == '\n')
            count++;
    if (str.start[str.len - 1] != '\n')
        count++;
    struct bstr *r = talloc_array_ptrtype(talloc_ctx, r, count);
    unsigned char *p = str.start;
    for (int i = 0; i < count - 1; i++) {
        r[i].start = p;
        while (*p++ != '\n');
        r[i].len = p - r[i].start;
    }
    r[count - 1].start = p;
    r[count - 1].len = str.start + str.len - p;
    return r;
}

struct bstr bstr_splitchar(struct bstr str, struct bstr *rest, const char c)
{
    int pos = bstrchr(str, c);
    if (pos < 0)
        pos = str.len;
    if (rest)
        *rest = bstr_cut(str, pos + 1);
    return bstr_splice(str, 0, pos + 1);
}

struct bstr bstr_strip_linebreaks(struct bstr str)
{
    if (bstr_endswith0(str, "\r\n")) {
        str = bstr_splice(str, 0, str.len - 2);
    } else if (bstr_endswith0(str, "\n")) {
        str = bstr_splice(str, 0, str.len - 1);
    }
    return str;
}

bool bstr_eatstart(struct bstr *s, struct bstr prefix)
{
    if (!bstr_startswith(*s, prefix))
        return false;
    *s = bstr_cut(*s, prefix.len);
    return true;
}

bool bstr_eatend(struct bstr *s, struct bstr prefix)
{
    if (!bstr_endswith(*s, prefix))
        return false;
    s->len -= prefix.len;
    return true;
}

void bstr_lower(struct bstr str)
{
    for (int i = 0; i < str.len; i++)
        str.start[i] = mp_tolower(str.start[i]);
}

int bstr_sscanf(struct bstr str, const char *format, ...)
{
    char *ptr = bstrdup0(NULL, str);
    va_list va;
    va_start(va, format);
    int ret = vsscanf(ptr, format, va);
    va_end(va);
    talloc_free(ptr);
    return ret;
}

int bstr_parse_utf8_code_length(unsigned char b)
{
    if (b < 128)
        return 1;
    int bytes = 7 - av_log2(b ^ 255);
    return (bytes >= 2 && bytes <= 4) ? bytes : -1;
}

int bstr_decode_utf8(struct bstr s, struct bstr *out_next)
{
    if (s.len == 0)
        return -1;
    unsigned int codepoint = s.start[0];
    s.start++; s.len--;
    if (codepoint >= 128) {
        int bytes = bstr_parse_utf8_code_length(codepoint);
        if (bytes < 1 || s.len < bytes - 1)
            return -1;
        codepoint &= 127 >> bytes;
        for (int n = 1; n < bytes; n++) {
            int tmp = (unsigned char)s.start[0];
            if ((tmp & 0xC0) != 0x80)
                return -1;
            codepoint = (codepoint << 6) | (tmp & ~0xC0);
            s.start++; s.len--;
        }
        if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
            return -1;
        // Overlong sequences - check taken from libavcodec.
        // (The only reason we even bother with this is to make libavcodec's
        //  retarded subtitle utf-8 check happy.)
        unsigned int min = bytes == 2 ? 0x80 : 1 << (5 * bytes - 4);
        if (codepoint < min)
            return -1;
    }
    if (out_next)
        *out_next = s;
    return codepoint;
}

struct bstr bstr_split_utf8(struct bstr str, struct bstr *out_next)
{
    bstr rest;
    int code = bstr_decode_utf8(str, &rest);
    if (code < 0)
        return (bstr){0};
    if (out_next)
        *out_next = rest;
    return bstr_splice(str, 0, str.len - rest.len);
}

int bstr_validate_utf8(struct bstr s)
{
    while (s.len) {
        if (bstr_decode_utf8(s, &s) < 0) {
            // Try to guess whether the sequence was just cut-off.
            unsigned int codepoint = (unsigned char)s.start[0];
            int bytes = bstr_parse_utf8_code_length(codepoint);
            if (bytes > 1 && s.len < 6) {
                // Manually check validity of left bytes
                for (int n = 1; n < bytes; n++) {
                    if (n >= s.len) {
                        // Everything valid until now - just cut off.
                        return -(bytes - s.len);
                    }
                    int tmp = (unsigned char)s.start[n];
                    if ((tmp & 0xC0) != 0x80)
                        break;
                }
            }
            return -8;
        }
    }
    return 0;
}

struct bstr bstr_sanitize_utf8_latin1(void *talloc_ctx, struct bstr s)
{
    bstr new = {0};
    bstr left = s;
    unsigned char *first_ok = s.start;
    while (left.len) {
        int r = bstr_decode_utf8(left, &left);
        if (r < 0) {
            bstr_xappend(talloc_ctx, &new, (bstr){first_ok, left.start - first_ok});
            mp_append_utf8_bstr(talloc_ctx, &new, (unsigned char)left.start[0]);
            left.start += 1;
            left.len -= 1;
            first_ok = left.start;
        }
    }
    if (!new.start)
        return s;
    if (first_ok != left.start)
        bstr_xappend(talloc_ctx, &new, (bstr){first_ok, left.start - first_ok});
    return new;
}

static void resize_append(void *talloc_ctx, bstr *s, size_t append_min)
{
    size_t size = talloc_get_size(s->start);
    assert(s->len <= size);
    if (append_min > size - s->len) {
        if (append_min < size)
            append_min = size; // preallocate in power of 2s
        if (size >= SIZE_MAX / 2 || append_min >= SIZE_MAX / 2)
            abort(); // oom
        s->start = talloc_realloc_size(talloc_ctx, s->start, size + append_min);
    }
}

// Append the string, so that *s = *s + append. s->start is expected to be
// a talloc allocation (which can be realloced) or NULL.
// This function will always implicitly append a \0 after the new string for
// convenience.
// talloc_ctx will be used as parent context, if s->start is NULL.
void bstr_xappend(void *talloc_ctx, bstr *s, bstr append)
{
    if (!append.len)
        return;
    resize_append(talloc_ctx, s, append.len + 1);
    memcpy(s->start + s->len, append.start, append.len);
    s->len += append.len;
    s->start[s->len] = '\0';
}

void bstr_xappend_asprintf(void *talloc_ctx, bstr *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bstr_xappend_vasprintf(talloc_ctx, s, fmt, ap);
    va_end(ap);
}

// Exactly as bstr_xappend(), but with a formatted string.
void bstr_xappend_vasprintf(void *talloc_ctx, bstr *s, const char *fmt,
                            va_list ap)
{
    int size;
    va_list copy;
    va_copy(copy, ap);
    size_t avail = talloc_get_size(s->start) - s->len;
    char *dest = s->start ? s->start + s->len : NULL;
    char c;
    if (avail < 1)
        dest = &c;
    size = vsnprintf(dest, MPMAX(avail, 1), fmt, copy);
    va_end(copy);

    if (size < 0)
        abort();

    if (avail < 1 || size + 1 > avail) {
        resize_append(talloc_ctx, s, size + 1);
        vsnprintf(s->start + s->len, size + 1, fmt, ap);
    }
    s->len += size;
}

bool bstr_case_startswith(struct bstr s, struct bstr prefix)
{
    struct bstr start = bstr_splice(s, 0, prefix.len);
    return start.len == prefix.len && bstrcasecmp(start, prefix) == 0;
}

bool bstr_case_endswith(struct bstr s, struct bstr suffix)
{
    struct bstr end = bstr_cut(s, -suffix.len);
    return end.len == suffix.len && bstrcasecmp(end, suffix) == 0;
}

struct bstr bstr_strip_ext(struct bstr str)
{
    int dotpos = bstrrchr(str, '.');
    if (dotpos < 0)
        return str;
    return (struct bstr){str.start, dotpos};
}

struct bstr bstr_get_ext(struct bstr s)
{
    int dotpos = bstrrchr(s, '.');
    if (dotpos < 0)
        return (struct bstr){NULL, 0};
    return bstr_splice(s, dotpos + 1, s.len);
}

static int h_to_i(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1; // invalid char
}

bool bstr_decode_hex(void *talloc_ctx, struct bstr hex, struct bstr *out)
{
    if (!out)
        return false;

    char *arr = talloc_array(talloc_ctx, char, hex.len / 2);
    int len = 0;

    while (hex.len >= 2) {
        int a = h_to_i(hex.start[0]);
        int b = h_to_i(hex.start[1]);
        hex = bstr_splice(hex, 2, hex.len);

        if (a < 0 || b < 0) {
            talloc_free(arr);
            return false;
        }

        arr[len++] = (a << 4) | b;
    }

    *out = (struct bstr){ .start = arr, .len = len };
    return true;
}
