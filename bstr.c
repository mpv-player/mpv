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

#include <string.h>
#include <libavutil/avutil.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>

#include "talloc.h"

#include "bstr.h"

int bstrcmp(struct bstr str1, struct bstr str2)
{
    int ret = memcmp(str1.start, str2.start, FFMIN(str1.len, str2.len));

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
    int ret = strncasecmp(str1.start, str2.start, FFMIN(str1.len, str2.len));

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

int bstr_find(struct bstr haystack, struct bstr needle)
{
    for (int i = 0; i < haystack.len; i++)
        if (bstr_startswith(bstr_splice(haystack, i, haystack.len), needle))
            return i;
    return -1;
}

struct bstr bstr_lstrip(struct bstr str)
{
    while (str.len && isspace(*str.start)) {
        str.start++;
        str.len--;
    }
    return str;
}

struct bstr bstr_strip(struct bstr str)
{
    str = bstr_lstrip(str);
    while (str.len && isspace(str.start[str.len - 1]))
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

struct bstr bstr_getline(struct bstr str, struct bstr *rest)
{
    int pos = bstrchr(str, '\n');
    if (pos < 0)
        pos = str.len;
    if (rest)
        *rest = bstr_cut(str, pos + 1);
    str.len = pos;
    if (str.len > 0 && str.start[str.len - 1] == '\r')
        str.len -= 1;
    return str;
}

void bstr_lower(struct bstr str)
{
    for (int i = 0; i < str.len; i++)
        str.start[i] = tolower(str.start[i]);
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
        if (bytes < 0 || s.len < bytes - 1)
            return -1;
        codepoint &= 127 >> bytes;
        for (int n = 1; n < bytes; n++) {
            int tmp = s.start[0];
            if ((tmp & 0xC0) != 0x80)
                return -1;
            codepoint = (codepoint << 6) | (tmp & ~0xC0);
            s.start++; s.len--;
        }
    }
    if (out_next)
        *out_next = s;
    return codepoint;
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
