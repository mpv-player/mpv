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

int bstr_find(struct bstr haystack, struct bstr needle)
{
    for (int i = 0; i < haystack.len; i++)
        if (bstr_startswith(bstr_splice(haystack, i, haystack.len), needle))
            return i;
    return -1;
}

struct bstr bstr_strip(struct bstr str)
{
    while (str.len && isspace(*str.start)) {
        str.start++;
        str.len--;
    }
    while (str.len && isspace(str.start[str.len - 1]))
        str.len--;
    return str;
}

struct bstr bstr_split(struct bstr str, char *sep, struct bstr *rest)
{
    int start, end;
    for (start = 0; start < str.len; start++)
        if (!strchr(sep, str.start[start]))
            break;
    for (end = start; end < str.len; end++)
        if (strchr(sep, str.start[end]))
            break;
    if (rest) {
        *rest = bstr_cut(str, end);
    }
    str.start += start;
    str.len = end - start;
    return str;
}


struct bstr bstr_splice(struct bstr str, int start, int end)
{
    if (start < 0)
        start += str.len;
    if (end < 0)
        end += str.len;
    end = FFMIN(end, str.len);
    start = FFMAX(start, 0);
    if (start >= end)
        return (struct bstr){NULL, 0};
    str.start += start;
    str.len = end - start;
    return str;
}

long long bstrtoll(struct bstr str, struct bstr *rest, int base)
{
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

void bstr_lower(struct bstr str)
{
    for (int i = 0; i < str.len; i++)
        str.start[i] = tolower(str.start[i]);
}
