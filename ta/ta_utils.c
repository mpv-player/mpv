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

#define TA_NO_WRAPPERS
#include "ta.h"

// Return element_size * count. If it overflows, return (size_t)-1 (SIZE_MAX).
// I.e. this returns the equivalent of: MIN(element_size * count, SIZE_MAX).
// The idea is that every real memory allocator will reject (size_t)-1, thus
// this is a valid way to handle too large array allocation requests.
size_t ta_calc_array_size(size_t element_size, size_t count)
{
    if (count > (((size_t)-1) / element_size))
        return (size_t)-1;
    return element_size * count;
}

// This is used when an array has to be enlarged for appending new elements.
// Return a "good" size for the new array (in number of elements). This returns
// a value > nextidx, unless the calculation overflows, in which case SIZE_MAX
// is returned.
size_t ta_calc_prealloc_elems(size_t nextidx)
{
    if (nextidx >= ((size_t)-1) / 2 - 1)
        return (size_t)-1;
    return (nextidx + 1) * 2;
}

static void dummy_dtor(void *p){}

/* Create an empty (size 0) TA allocation, which is prepared in a way such that
 * using it as parent with ta_set_parent() always succeed. Calling
 * ta_set_destructor() on it will always succeed as well.
 */
void *ta_new_context(void *ta_parent)
{
    void *new = ta_alloc_size(ta_parent, 0);
    // Force it to allocate an extended header.
    if (!ta_set_destructor(new, dummy_dtor)) {
        ta_free(new);
        new = NULL;
    }
    return new;
}

/* Set parent of ptr to ta_parent, return the ptr.
 * Note that ta_parent==NULL will simply unset the current parent of ptr.
 * If the operation fails (on OOM), return NULL. (That's pretty bad behavior,
 * but the only way to signal failure.)
 */
void *ta_steal_(void *ta_parent, void *ptr)
{
    if (!ta_set_parent(ptr, ta_parent))
        return NULL;
    return ptr;
}

/* Duplicate the memory at ptr with the given size.
 */
void *ta_memdup(void *ta_parent, void *ptr, size_t size)
{
    if (!ptr) {
        assert(!size);
        return NULL;
    }
    void *res = ta_alloc_size(ta_parent, size);
    if (!res)
        return NULL;
    memcpy(res, ptr, size);
    return res;
}

// *str = *str[0..at] + append[0..append_len]
// (append_len being a maximum length; shorter if embedded \0s are encountered)
static bool strndup_append_at(char **str, size_t at, const char *append,
                              size_t append_len)
{
    assert(ta_get_size(*str) >= at);

    if (!*str && !append)
        return true; // stays NULL, but not an OOM condition

    size_t real_len = append ? strnlen(append, append_len) : 0;
    if (append_len > real_len)
        append_len = real_len;

    if (ta_get_size(*str) < at + append_len + 1) {
        char *t = ta_realloc_size(NULL, *str, at + append_len + 1);
        if (!t)
            return false;
        *str = t;
    }

    if (append_len)
        memcpy(*str + at, append, append_len);

    (*str)[at + append_len] = '\0';

    ta_dbg_mark_as_string(*str);

    return true;
}

/* Return a copy of str.
 * Returns NULL on OOM.
 */
char *ta_strdup(void *ta_parent, const char *str)
{
    return ta_strndup(ta_parent, str, str ? strlen(str) : 0);
}

/* Return a copy of str. If the string is longer than n, copy only n characters
 * (the returned allocation will be n+1 bytes and contain a terminating '\0').
 * The returned string will have the length MIN(strlen(str), n)
 * If str==NULL, return NULL. Returns NULL on OOM as well.
 */
char *ta_strndup(void *ta_parent, const char *str, size_t n)
{
    if (!str)
        return NULL;
    char *new = NULL;
    strndup_append_at(&new, 0, str, n);
    if (!ta_set_parent(new, ta_parent)) {
        ta_free(new);
        new = NULL;
    }
    return new;
}

/* Append a to *str. If *str is NULL, the string is newly allocated, otherwise
 * ta_realloc() is used on *str as needed.
 * Return success or failure (it can fail due to OOM only).
 */
bool ta_strdup_append(char **str, const char *a)
{
    return strndup_append_at(str, *str ? strlen(*str) : 0, a, (size_t)-1);
}

/* Like ta_strdup_append(), but use ta_get_size(*str)-1 instead of strlen(*str).
 * (See also: ta_asprintf_append_buffer())
 */
bool ta_strdup_append_buffer(char **str, const char *a)
{
    size_t size = ta_get_size(*str);
    if (size > 0)
        size -= 1;
    return strndup_append_at(str, size, a, (size_t)-1);
}

/* Like ta_strdup_append(), but limit the length of a with n.
 * (See also: ta_strndup())
 */
bool ta_strndup_append(char **str, const char *a, size_t n)
{
    return strndup_append_at(str, *str ? strlen(*str) : 0, a, n);
}

/* Like ta_strdup_append_buffer(), but limit the length of a with n.
 * (See also: ta_strndup())
 */
bool ta_strndup_append_buffer(char **str, const char *a, size_t n)
{
    size_t size = ta_get_size(*str);
    if (size > 0)
        size -= 1;
    return strndup_append_at(str, size, a, n);
}

static bool ta_vasprintf_append_at(char **str, size_t at, const char *fmt,
                                   va_list ap)
{
    assert(ta_get_size(*str) >= at);

    int size;
    va_list copy;
    va_copy(copy, ap);
    char c;
    size = vsnprintf(&c, 1, fmt, copy);
    va_end(copy);

    if (size < 0)
        return false;

    if (ta_get_size(*str) < at + size + 1) {
        char *t = ta_realloc_size(NULL, *str, at + size + 1);
        if (!t)
            return false;
        *str = t;
    }
    vsnprintf(*str + at, size + 1, fmt, ap);

    ta_dbg_mark_as_string(*str);

    return true;
}

/* Like snprintf(); returns the formatted string as allocation (or NULL on OOM
 * or snprintf() errors).
 */
char *ta_asprintf(void *ta_parent, const char *fmt, ...)
{
    char *res;
    va_list ap;
    va_start(ap, fmt);
    res = ta_vasprintf(ta_parent, fmt, ap);
    va_end(ap);
    return res;
}

char *ta_vasprintf(void *ta_parent, const char *fmt, va_list ap)
{
    char *res = NULL;
    ta_vasprintf_append_at(&res, 0, fmt, ap);
    if (!res || !ta_set_parent(res, ta_parent)) {
        ta_free(res);
        return NULL;
    }
    return res;
}

/* Append the formatted string to *str (after strlen(*str)). The allocation is
 * ta_realloced if needed.
 * Returns false on OOM or snprintf() errors, with *str left untouched.
 */
bool ta_asprintf_append(char **str, const char *fmt, ...)
{
    bool res;
    va_list ap;
    va_start(ap, fmt);
    res = ta_vasprintf_append(str, fmt, ap);
    va_end(ap);
    return res;
}

bool ta_vasprintf_append(char **str, const char *fmt, va_list ap)
{
    return ta_vasprintf_append_at(str, *str ? strlen(*str) : 0, fmt, ap);
}

/* Append the formatted string at the end of the allocation of *str. It
 * overwrites the last byte of the allocation too (which is assumed to be the
 * '\0' terminating the string). Compared to ta_asprintf_append(), this is
 * useful if you know that the string ends with the allocation, so that the
 * extra strlen() can be avoided for better performance.
 * Returns false on OOM or snprintf() errors, with *str left untouched.
 */
bool ta_asprintf_append_buffer(char **str, const char *fmt, ...)
{
    bool res;
    va_list ap;
    va_start(ap, fmt);
    res = ta_vasprintf_append_buffer(str, fmt, ap);
    va_end(ap);
    return res;
}

bool ta_vasprintf_append_buffer(char **str, const char *fmt, va_list ap)
{
    size_t size = ta_get_size(*str);
    if (size > 0)
        size -= 1;
    return ta_vasprintf_append_at(str, size, fmt, ap);
}


void *ta_oom_p(void *p)
{
    if (!p)
        abort();
    return p;
}

void ta_oom_b(bool b)
{
    if (!b)
        abort();
}

char *ta_oom_s(char *s)
{
    if (!s)
        abort();
    return s;
}

void *ta_xsteal_(void *ta_parent, void *ptr)
{
    ta_oom_b(ta_set_parent(ptr, ta_parent));
    return ptr;
}

void *ta_xmemdup(void *ta_parent, void *ptr, size_t size)
{
    void *new = ta_memdup(ta_parent, ptr, size);
    ta_oom_b(new || !ptr);
    return new;
}

void *ta_xrealloc_size(void *ta_parent, void *ptr, size_t size)
{
    ptr = ta_realloc_size(ta_parent, ptr, size);
    ta_oom_b(ptr || !size);
    return ptr;
}

char *ta_xstrdup(void *ta_parent, const char *str)
{
    char *res = ta_strdup(ta_parent, str);
    ta_oom_b(res || !str);
    return res;
}

char *ta_xstrndup(void *ta_parent, const char *str, size_t n)
{
    char *res = ta_strndup(ta_parent, str, n);
    ta_oom_b(res || !str);
    return res;
}
