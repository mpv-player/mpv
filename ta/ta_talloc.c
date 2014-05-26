/* Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "ta_talloc.h"

char *ta_talloc_strdup_append(char *s, const char *a)
{
    ta_xstrdup_append(&s, a);
    return s;
}

char *ta_talloc_strdup_append_buffer(char *s, const char *a)
{
    ta_xstrdup_append_buffer(&s, a);
    return s;
}

char *ta_talloc_strndup_append(char *s, const char *a, size_t n)
{
    ta_xstrndup_append(&s, a, n);
    return s;
}

char *ta_talloc_strndup_append_buffer(char *s, const char *a, size_t n)
{
    ta_xstrndup_append_buffer(&s, a, n);
    return s;
}

char *ta_talloc_vasprintf_append(char *s, const char *fmt, va_list ap)
{
    ta_xvasprintf_append(&s, fmt, ap);
    return s;
}

char *ta_talloc_vasprintf_append_buffer(char *s, const char *fmt, va_list ap)
{
    ta_xvasprintf_append_buffer(&s, fmt, ap);
    return s;
}

char *ta_talloc_asprintf_append(char *s, const char *fmt, ...)
{
    char *res;
    va_list ap;
    va_start(ap, fmt);
    res = talloc_vasprintf_append(s, fmt, ap);
    va_end(ap);
    return res;
}

char *ta_talloc_asprintf_append_buffer(char *s, const char *fmt, ...)
{
    char *res;
    va_list ap;
    va_start(ap, fmt);
    res = talloc_vasprintf_append_buffer(s, fmt, ap);
    va_end(ap);
    return res;
}
