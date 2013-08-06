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

#include <libavutil/common.h>

#include "talloc.h"
#include "core/bstr.h"
#include "core/mp_common.h"

char *mp_format_time(double time, bool fractions)
{
    if (time == MP_NOPTS_VALUE)
        return talloc_strdup(NULL, "unknown");
    char sign[2] = {0};
    if (time < 0) {
        time = -time;
        sign[0] = '-';
    }
    long long int itime = time;
    long long int h, m, s;
    s = itime;
    h = s / 3600;
    s -= h * 3600;
    m = s / 60;
    s -= m * 60;
    char *res = talloc_asprintf(NULL, "%s%02lld:%02lld:%02lld", sign, h, m, s);
    if (fractions)
        res = talloc_asprintf_append(res, ".%03d",
                                     (int)((time - itime) * 1000));
    return res;
}

// Set rc to the union of rc and rc2
void mp_rect_union(struct mp_rect *rc, const struct mp_rect *rc2)
{
    rc->x0 = FFMIN(rc->x0, rc2->x0);
    rc->y0 = FFMIN(rc->y0, rc2->y0);
    rc->x1 = FFMAX(rc->x1, rc2->x1);
    rc->y1 = FFMAX(rc->y1, rc2->y1);
}

// Set rc to the intersection of rc and src.
// Return false if the result is empty.
bool mp_rect_intersection(struct mp_rect *rc, const struct mp_rect *rc2)
{
    rc->x0 = FFMAX(rc->x0, rc2->x0);
    rc->y0 = FFMAX(rc->y0, rc2->y0);
    rc->x1 = FFMIN(rc->x1, rc2->x1);
    rc->y1 = FFMIN(rc->y1, rc2->y1);

    return rc->x1 > rc->x0 && rc->y1 > rc->y0;
}

// Encode the unicode codepoint as UTF-8, and append to the end of the
// talloc'ed buffer.
char *mp_append_utf8_buffer(char *buffer, uint32_t codepoint)
{
    char data[8];
    uint8_t tmp;
    char *output = data;
    PUT_UTF8(codepoint, tmp, *output++ = tmp;);
    return talloc_strndup_append_buffer(buffer, data, output - data);
}

// Parse a C-style escape beginning at code, and append the result to *str
// using talloc. The input string (*code) must point to the first character
// after the initial '\', and after parsing *code is set to the first character
// after the current escape.
// On error, false is returned, and all input remains unchanged.
bool mp_parse_escape(bstr *code, char **str)
{
    if (code->len < 1)
        return false;
    char replace = 0;
    switch (code->start[0]) {
    case '"':  replace = '"';  break;
    case '\\': replace = '\\'; break;
    case 'b':  replace = '\b'; break;
    case 'f':  replace = '\f'; break;
    case 'n':  replace = '\n'; break;
    case 'r':  replace = '\r'; break;
    case 't':  replace = '\t'; break;
    case 'e':  replace = '\x1b'; break;
    case '\'': replace = '\''; break;
    }
    if (replace) {
        *str = talloc_strndup_append_buffer(*str, &replace, 1);
        *code = bstr_cut(*code, 1);
        return true;
    }
    if (code->start[0] == 'x' && code->len >= 3) {
        bstr num = bstr_splice(*code, 1, 3);
        char c = bstrtoll(num, &num, 16);
        if (!num.len)
            return false;
        *str = talloc_strndup_append_buffer(*str, &c, 1);
        *code = bstr_cut(*code, 3);
        return true;
    }
    if (code->start[0] == 'u' && code->len >= 5) {
        bstr num = bstr_splice(*code, 1, 5);
        int c = bstrtoll(num, &num, 16);
        if (num.len)
            return false;
        *str = mp_append_utf8_buffer(*str, c);
        *code = bstr_cut(*code, 5);
        return true;
    }
    return false;
}
