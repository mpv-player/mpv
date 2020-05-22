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

#include <stdarg.h>
#include <math.h>
#include <assert.h>

#include <libavutil/common.h>
#include <libavutil/error.h>

#include "mpv_talloc.h"
#include "misc/bstr.h"
#include "misc/ctype.h"
#include "common/common.h"
#include "osdep/strnlen.h"

#define appendf(ptr, ...) \
    do {(*(ptr)) = talloc_asprintf_append_buffer(*(ptr), __VA_ARGS__);} while(0)

// Return a talloc'ed string formatted according to the format string in fmt.
// On error, return NULL.
// Valid formats:
// %H, %h: hour (%H is padded with 0 to two digits)
// %M: minutes from 00-59 (hours are subtracted)
// %m: total minutes (includes hours, unlike %M)
// %S: seconds from 00-59 (minutes and hours are subtracted)
// %s: total seconds (includes hours and minutes)
// %f: like %s, but as float
// %T: milliseconds (000-999)
char *mp_format_time_fmt(const char *fmt, double time)
{
    if (time == MP_NOPTS_VALUE)
        return talloc_strdup(NULL, "unknown");
    char *sign = time < 0 ? "-" : "";
    time = time < 0 ? -time : time;
    long long int itime = time;
    long long int h, m, tm, s;
    int ms = lrint((time - itime) * 1000);
    if (ms >= 1000) {
        ms -= 1000;
        itime += 1;
    }
    s = itime;
    tm = s / 60;
    h = s / 3600;
    s -= h * 3600;
    m = s / 60;
    s -= m * 60;
    char *res = talloc_strdup(NULL, "");
    while (*fmt) {
        if (fmt[0] == '%') {
            fmt++;
            switch (fmt[0]) {
            case 'h': appendf(&res, "%s%lld", sign, h); break;
            case 'H': appendf(&res, "%s%02lld", sign, h); break;
            case 'm': appendf(&res, "%s%lld", sign, tm); break;
            case 'M': appendf(&res, "%02lld", m); break;
            case 's': appendf(&res, "%s%lld", sign, itime); break;
            case 'S': appendf(&res, "%02lld", s); break;
            case 'T': appendf(&res, "%03d", ms); break;
            case 'f': appendf(&res, "%f", time); break;
            case '%': appendf(&res, "%s", "%"); break;
            default: goto error;
            }
            fmt++;
        } else {
            appendf(&res, "%c", *fmt);
            fmt++;
        }
    }
    return res;
error:
    talloc_free(res);
    return NULL;
}

char *mp_format_time(double time, bool fractions)
{
    return mp_format_time_fmt(fractions ? "%H:%M:%S.%T" : "%H:%M:%S", time);
}

// Set rc to the union of rc and rc2
void mp_rect_union(struct mp_rect *rc, const struct mp_rect *rc2)
{
    rc->x0 = MPMIN(rc->x0, rc2->x0);
    rc->y0 = MPMIN(rc->y0, rc2->y0);
    rc->x1 = MPMAX(rc->x1, rc2->x1);
    rc->y1 = MPMAX(rc->y1, rc2->y1);
}

// Returns whether or not a point is contained by rc
bool mp_rect_contains(struct mp_rect *rc, int x, int y)
{
    return rc->x0 <= x && x < rc->x1 && rc->y0 <= y && y < rc->y1;
}

// Set rc to the intersection of rc and src.
// Return false if the result is empty.
bool mp_rect_intersection(struct mp_rect *rc, const struct mp_rect *rc2)
{
    rc->x0 = MPMAX(rc->x0, rc2->x0);
    rc->y0 = MPMAX(rc->y0, rc2->y0);
    rc->x1 = MPMIN(rc->x1, rc2->x1);
    rc->y1 = MPMIN(rc->y1, rc2->y1);

    return rc->x1 > rc->x0 && rc->y1 > rc->y0;
}

bool mp_rect_equals(struct mp_rect *rc1, struct mp_rect *rc2)
{
    return rc1->x0 == rc2->x0 && rc1->y0 == rc2->y0 &&
           rc1->x1 == rc2->x1 && rc1->y1 == rc2->y1;
}

// Compute rc1-rc2, put result in res_array, return number of rectangles in
// res_array. In the worst case, there are 4 rectangles, so res_array must
// provide that much storage space.
int mp_rect_subtract(const struct mp_rect *rc1, const struct mp_rect *rc2,
                     struct mp_rect res[4])
{
    struct mp_rect rc = *rc1;
    if (!mp_rect_intersection(&rc, rc2))
        return 0;

    int cnt = 0;
    if (rc1->y0 < rc.y0)
        res[cnt++] = (struct mp_rect){rc1->x0, rc1->y0, rc1->x1, rc.y0};
    if (rc1->x0 < rc.x0)
        res[cnt++] = (struct mp_rect){rc1->x0, rc.y0,   rc.x0,   rc.y1};
    if (rc1->x1 > rc.x1)
        res[cnt++] = (struct mp_rect){rc.x1,   rc.y0,   rc1->x1, rc.y1};
    if (rc1->y1 > rc.y1)
        res[cnt++] = (struct mp_rect){rc1->x0, rc.y1,   rc1->x1, rc1->y1};
    return cnt;
}

// This works like snprintf(), except that it starts writing the first output
// character to str[strlen(str)]. This returns the number of characters the
// string would have *appended* assuming a large enough buffer, will make sure
// str is null-terminated, and will never write to str[size] or past.
// Example:
//  int example(char *buf, size_t buf_size, double num, char *str) {
//      int n = 0;
//      n += mp_snprintf_cat(buf, size, "%f", num);
//      n += mp_snprintf_cat(buf, size, "%s", str);
//      return n; }
// Note how this can be chained with functions similar in style.
int mp_snprintf_cat(char *str, size_t size, const char *format, ...)
{
    size_t len = strnlen(str, size);
    assert(!size || len < size); // str with no 0-termination is not allowed
    int r;
    va_list ap;
    va_start(ap, format);
    r = vsnprintf(str + len, size - len, format, ap);
    va_end(ap);
    return r;
}

// Encode the unicode codepoint as UTF-8, and append to the end of the
// talloc'ed buffer. All guarantees bstr_xappend() give applies, such as
// implicit \0-termination for convenience.
void mp_append_utf8_bstr(void *talloc_ctx, struct bstr *buf, uint32_t codepoint)
{
    char data[8];
    uint8_t tmp;
    char *output = data;
    PUT_UTF8(codepoint, tmp, *output++ = tmp;);
    bstr_xappend(talloc_ctx, buf, (bstr){data, output - data});
}

// Parse a C/JSON-style escape beginning at code, and append the result to *str
// using talloc. The input string (*code) must point to the first character
// after the initial '\', and after parsing *code is set to the first character
// after the current escape.
// On error, false is returned, and all input remains unchanged.
static bool mp_parse_escape(void *talloc_ctx, bstr *dst, bstr *code)
{
    if (code->len < 1)
        return false;
    char replace = 0;
    switch (code->start[0]) {
    case '"':  replace = '"';  break;
    case '\\': replace = '\\'; break;
    case '/':  replace = '/'; break;
    case 'b':  replace = '\b'; break;
    case 'f':  replace = '\f'; break;
    case 'n':  replace = '\n'; break;
    case 'r':  replace = '\r'; break;
    case 't':  replace = '\t'; break;
    case 'e':  replace = '\x1b'; break;
    case '\'': replace = '\''; break;
    }
    if (replace) {
        bstr_xappend(talloc_ctx, dst, (bstr){&replace, 1});
        *code = bstr_cut(*code, 1);
        return true;
    }
    if (code->start[0] == 'x' && code->len >= 3) {
        bstr num = bstr_splice(*code, 1, 3);
        char c = bstrtoll(num, &num, 16);
        if (num.len)
            return false;
        bstr_xappend(talloc_ctx, dst, (bstr){&c, 1});
        *code = bstr_cut(*code, 3);
        return true;
    }
    if (code->start[0] == 'u' && code->len >= 5) {
        bstr num = bstr_splice(*code, 1, 5);
        uint32_t c = bstrtoll(num, &num, 16);
        if (num.len)
            return false;
        if (c >= 0xd800 && c <= 0xdbff) {
            if (code->len < 5 + 6 // udddd + \udddd
                || code->start[5] != '\\' || code->start[6] != 'u')
                return false;
            *code = bstr_cut(*code, 5 + 1);
            bstr num2 = bstr_splice(*code, 1, 5);
            uint32_t c2 = bstrtoll(num2, &num2, 16);
            if (num2.len || c2 < 0xdc00 || c2 > 0xdfff)
                return false;
            c = ((c - 0xd800) << 10) + 0x10000 + (c2 - 0xdc00);
        }
        mp_append_utf8_bstr(talloc_ctx, dst, c);
        *code = bstr_cut(*code, 5);
        return true;
    }
    return false;
}

// Like mp_append_escaped_string, but set *dst to sliced *src if no escape
// sequences have to be parsed (i.e. no memory allocation is required), and
// if dst->start was NULL on function entry.
bool mp_append_escaped_string_noalloc(void *talloc_ctx, bstr *dst, bstr *src)
{
    bstr t = *src;
    int cur = 0;
    while (1) {
        if (cur >= t.len || t.start[cur] == '"') {
            *src = bstr_cut(t, cur);
            t = bstr_splice(t, 0, cur);
            if (dst->start == NULL) {
                *dst = t;
            } else {
                bstr_xappend(talloc_ctx, dst, t);
            }
            return true;
        } else if (t.start[cur] == '\\') {
            bstr_xappend(talloc_ctx, dst, bstr_splice(t, 0, cur));
            t = bstr_cut(t, cur + 1);
            cur = 0;
            if (!mp_parse_escape(talloc_ctx, dst, &t))
                goto error;
        } else {
            cur++;
        }
    }
error:
    return false;
}

// src is expected to point to a C-style string literal, *src pointing to the
// first char after the starting '"'. It will append the contents of the literal
// to *dst (using talloc_ctx) until the first '"' or the end of *str is found.
// See bstr_xappend() how data is appended to *dst.
// On success, *src will either start with '"', or be empty.
// On error, return false, and *dst will contain the string until the first
// error, *src is not changed.
// Note that dst->start will be implicitly \0-terminated on successful return,
// and if it was NULL or \0-terminated before calling the function.
// As mentioned above, the caller is responsible for skipping the '"' chars.
bool mp_append_escaped_string(void *talloc_ctx, bstr *dst, bstr *src)
{
    if (mp_append_escaped_string_noalloc(talloc_ctx, dst, src)) {
        // Guarantee copy (or allocation).
        if (!dst->start || dst->start == src->start) {
            bstr res = *dst;
            *dst = (bstr){0};
            bstr_xappend(talloc_ctx, dst, res);
        }
        return true;
    }
    return false;
}

// Behaves like strerror()/strerror_r(), but is thread- and GNU-safe.
char *mp_strerror_buf(char *buf, size_t buf_size, int errnum)
{
    // This handles the nasty details of calling the right function for us.
    av_strerror(AVERROR(errnum), buf, buf_size);
    return buf;
}

char *mp_tag_str_buf(char *buf, size_t buf_size, uint32_t tag)
{
    if (buf_size < 1)
        return buf;
    buf[0] = '\0';
    for (int n = 0; n < 4; n++) {
        uint8_t val = (tag >> (n * 8)) & 0xFF;
        if (mp_isalnum(val) || val == '_' || val == ' ') {
            mp_snprintf_cat(buf, buf_size, "%c", val);
        } else {
            mp_snprintf_cat(buf, buf_size, "[%d]", val);
        }
    }
    return buf;
}

char *mp_tprintf_buf(char *buf, size_t buf_size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, buf_size, format, ap);
    va_end(ap);
    return buf;
}

char **mp_dup_str_array(void *tctx, char **s)
{
    char **r = NULL;
    int num_r = 0;
    for (int n = 0; s && s[n]; n++)
        MP_TARRAY_APPEND(tctx, r, num_r, talloc_strdup(tctx, s[n]));
    if (r)
        MP_TARRAY_APPEND(tctx, r, num_r, NULL);
    return r;
}

// Return rounded down integer log 2 of v, i.e. position of highest set bit.
//  mp_log2(0)  == 0
//  mp_log2(1)  == 0
//  mp_log2(31) == 4
//  mp_log2(32) == 5
unsigned int mp_log2(uint32_t v)
{
#if defined(__GNUC__) && __GNUC__ >= 4
    return v ? 31 - __builtin_clz(v) : 0;
#else
    for (int x = 31; x >= 0; x--) {
        if (v & (((uint32_t)1) << x))
            return x;
    }
    return 0;
#endif
}

// If a power of 2, return it, otherwise return the next highest one, or 0.
//  mp_round_next_power_of_2(65)            == 128
//  mp_round_next_power_of_2(64)            == 64
//  mp_round_next_power_of_2(0)             == 1
//  mp_round_next_power_of_2(UINT32_MAX)    == 0
uint32_t mp_round_next_power_of_2(uint32_t v)
{
    if (!v)
        return 1;
    if (!(v & (v - 1)))
        return v;
    int l = mp_log2(v) + 1;
    return l == 32 ? 0 : (uint32_t)1 << l;
}
