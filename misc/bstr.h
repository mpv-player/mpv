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

#ifndef MPLAYER_BSTR_H
#define MPLAYER_BSTR_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include "mpv_talloc.h"
#include "osdep/compiler.h"

/* NOTE: 'len' is size_t, but most string-handling functions below assume
 * that input size has been sanity checked and len fits in an int.
 */
typedef struct bstr {
    unsigned char *start;
    size_t len;
} bstr;

// If str.start is NULL, return NULL.
static inline char *bstrdup0(void *talloc_ctx, struct bstr str)
{
    return talloc_strndup(talloc_ctx, (char *)str.start, str.len);
}

// Like bstrdup0(), but always return a valid C-string.
static inline char *bstrto0(void *talloc_ctx, struct bstr str)
{
    return str.start ? bstrdup0(talloc_ctx, str) : talloc_strdup(talloc_ctx, "");
}

// Return start = NULL iff that is true for the original.
static inline struct bstr bstrdup(void *talloc_ctx, struct bstr str)
{
    struct bstr r = { NULL, str.len };
    if (str.start)
        r.start = (unsigned char *)talloc_memdup(talloc_ctx, str.start, str.len);
    return r;
}

static inline struct bstr bstr0(const char *s)
{
    return (struct bstr){(unsigned char *)s, s ? strlen(s) : 0};
}

int bstrcmp(struct bstr str1, struct bstr str2);
int bstrcasecmp(struct bstr str1, struct bstr str2);
int bstrchr(struct bstr str, int c);
int bstrrchr(struct bstr str, int c);
int bstrspn(struct bstr str, const char *accept);
int bstrcspn(struct bstr str, const char *reject);

int bstr_find(struct bstr haystack, struct bstr needle);
struct bstr bstr_lstrip(struct bstr str);
struct bstr bstr_strip(struct bstr str);
struct bstr bstr_split(struct bstr str, const char *sep, struct bstr *rest);
bool bstr_split_tok(bstr str, const char *tok, bstr *out_left, bstr *out_right);
struct bstr bstr_splice(struct bstr str, int start, int end);
long long bstrtoll(struct bstr str, struct bstr *rest, int base);
double bstrtod(struct bstr str, struct bstr *rest);
void bstr_lower(struct bstr str);
int bstr_sscanf(struct bstr str, const char *format, ...);

// Decode a string containing hexadecimal data. All whitespace will be silently
// ignored. When successful, this allocates a new array to store the output.
bool bstr_decode_hex(void *talloc_ctx, struct bstr hex, struct bstr *out);

// Decode the UTF-8 code point at the start of the string, and return the
// character.
// After calling this function, *out_next will point to the next character.
// out_next can be NULL.
// On error, -1 is returned, and *out_next is not modified.
int bstr_decode_utf8(struct bstr str, struct bstr *out_next);

// Return the UTF-8 code point at the start of the string.
// After calling this function, *out_next will point to the next character.
// out_next can be NULL.
// On error, an empty string is returned, and *out_next is not modified.
struct bstr bstr_split_utf8(struct bstr str, struct bstr *out_next);

// Return the length of the UTF-8 sequence that starts with the given byte.
// Given a string char *s, the next UTF-8 code point is to be expected at
//      s + bstr_parse_utf8_code_length(s[0])
// On error, -1 is returned. On success, it returns a value in the range [1, 4].
int bstr_parse_utf8_code_length(unsigned char b);

// Return >= 0 if the string is valid UTF-8, otherwise negative error code.
// Embedded \0 bytes are considered valid.
// This returns -N if the UTF-8 string was likely just cut-off in the middle of
// an UTF-8 sequence: -1 means 1 byte was missing, -5 5 bytes missing.
// If the string was likely not cut off, -8 is returned.
// Use (return_value > -8) to check whether the string is valid UTF-8 or valid
// but cut-off UTF-8.
int bstr_validate_utf8(struct bstr s);

// Force the input string to valid UTF-8. If invalid UTF-8 encoding is
// encountered, the invalid bytes are interpreted as Latin-1.
// Embedded \0 bytes are considered valid.
// If replacement happens, a newly allocated string is returned (with a \0
// byte added past its end for convenience). The string is allocated via
// talloc, with talloc_ctx as parent.
struct bstr bstr_sanitize_utf8_latin1(void *talloc_ctx, struct bstr s);

// Return the text before the occurrence of a character, and return it. Change
// *rest to point to the text following this character. (rest can be NULL.)
struct bstr bstr_splitchar(struct bstr str, struct bstr *rest, const char c);

// Like bstr_splitchar. Trailing newlines are not stripped.
static inline struct bstr bstr_getline(struct bstr str, struct bstr *rest)
{
    return bstr_splitchar(str, rest, '\n');
}

// Strip one trailing line break. This is intended for use with bstr_getline,
// and will remove the trailing \n or \r\n sequence.
struct bstr bstr_strip_linebreaks(struct bstr str);

void bstr_xappend(void *talloc_ctx, bstr *s, bstr append);
void bstr_xappend_asprintf(void *talloc_ctx, bstr *s, const char *fmt, ...)
    PRINTF_ATTRIBUTE(3, 4);
void bstr_xappend_vasprintf(void *talloc_ctx, bstr *s, const char *fmt, va_list va)
    PRINTF_ATTRIBUTE(3, 0);

// If s starts/ends with prefix, return true and return the rest of the string
// in s.
bool bstr_eatstart(struct bstr *s, struct bstr prefix);
bool bstr_eatend(struct bstr *s, struct bstr prefix);

bool bstr_case_startswith(struct bstr s, struct bstr prefix);
bool bstr_case_endswith(struct bstr s, struct bstr suffix);
struct bstr bstr_strip_ext(struct bstr str);
struct bstr bstr_get_ext(struct bstr s);

static inline struct bstr bstr_cut(struct bstr str, int n)
{
    if (n < 0) {
        n += str.len;
        if (n < 0)
            n = 0;
    }
    if (((size_t)n) > str.len)
        n = str.len;
    return (struct bstr){str.start + n, str.len - n};
}

static inline bool bstr_startswith(struct bstr str, struct bstr prefix)
{
    if (str.len < prefix.len)
        return false;
    return !memcmp(str.start, prefix.start, prefix.len);
}

static inline bool bstr_startswith0(struct bstr str, const char *prefix)
{
    return bstr_startswith(str, bstr0(prefix));
}

static inline bool bstr_endswith(struct bstr str, struct bstr suffix)
{
    if (str.len < suffix.len)
        return false;
    return !memcmp(str.start + str.len - suffix.len, suffix.start, suffix.len);
}

static inline bool bstr_endswith0(struct bstr str, const char *suffix)
{
    return bstr_endswith(str, bstr0(suffix));
}

static inline int bstrcmp0(struct bstr str1, const char *str2)
{
    return bstrcmp(str1, bstr0(str2));
}

static inline bool bstr_equals(struct bstr str1, struct bstr str2)
{
    if (str1.len != str2.len)
        return false;

    return str1.start == str2.start || bstrcmp(str1, str2) == 0;
}

static inline bool bstr_equals0(struct bstr str1, const char *str2)
{
    return bstr_equals(str1, bstr0(str2));
}

static inline int bstrcasecmp0(struct bstr str1, const char *str2)
{
    return bstrcasecmp(str1, bstr0(str2));
}

static inline int bstr_find0(struct bstr haystack, const char *needle)
{
    return bstr_find(haystack, bstr0(needle));
}

static inline bool bstr_eatstart0(struct bstr *s, const char *prefix)
{
    return bstr_eatstart(s, bstr0(prefix));
}

static inline bool bstr_eatend0(struct bstr *s, const char *prefix)
{
    return bstr_eatend(s, bstr0(prefix));
}

// create a pair (not single value!) for "%.*s" printf syntax
#define BSTR_P(bstr) (int)((bstr).len), ((bstr).start ? (char*)(bstr).start : "")

#define WHITESPACE " \f\n\r\t\v"

#endif /* MPLAYER_BSTR_H */
