/* Copyright (C) 2017 the mpv developers
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#ifndef TA_TALLOC_H_
#define TA_TALLOC_H_

#include <string.h>

#include "ta.h"

// Note: all talloc wrappers are wired to the "x" functions, which abort on OOM.
//       libtalloc doesn't do that, but the mplayer2 internal copy of it did.

#define talloc                          ta_xnew
#define talloc_zero                     ta_xznew

#define talloc_array                    ta_xnew_array
#define talloc_zero_array               ta_xznew_array

#define talloc_array_size               ta_xnew_array_size
#define talloc_realloc                  ta_xrealloc
#define talloc_ptrtype                  ta_xnew_ptrtype
#define talloc_zero_ptrtype             ta_xznew_ptrtype
#define talloc_array_ptrtype            ta_xnew_array_ptrtype

#define talloc_steal                    ta_steal
#define talloc_realloc_size             ta_xrealloc_size
#define talloc_new                      ta_xnew_context
#define talloc_set_destructor           ta_set_destructor
#define talloc_enable_leak_report       ta_enable_leak_report
#define talloc_size                     ta_xalloc_size
#define talloc_zero_size                ta_xzalloc_size
#define talloc_get_size                 ta_get_size
#define talloc_free_children            ta_free_children
#define talloc_free                     ta_free
#define talloc_dup                      ta_xdup
#define talloc_memdup                   ta_xmemdup
#define talloc_strdup                   ta_xstrdup
#define talloc_strndup                  ta_xstrndup
#define talloc_asprintf                 ta_xasprintf
#define talloc_vasprintf                ta_xvasprintf
#define talloc_replace                  ta_replace

// Don't define linker-level symbols, as that would clash with real libtalloc.
#define talloc_strdup_append            ta_talloc_strdup_append
#define talloc_strdup_append_buffer     ta_talloc_strdup_append_buffer
#define talloc_strndup_append           ta_talloc_strndup_append
#define talloc_strndup_append_buffer    ta_talloc_strndup_append_buffer
#define talloc_vasprintf_append         ta_talloc_vasprintf_append
#define talloc_vasprintf_append_buffer  ta_talloc_vasprintf_append_buffer
#define talloc_asprintf_append          ta_talloc_asprintf_append
#define talloc_asprintf_append_buffer   ta_talloc_asprintf_append_buffer

char *ta_talloc_strdup_append(char *s, const char *a);
char *ta_talloc_strdup_append_buffer(char *s, const char *a);

char *ta_talloc_strndup(void *t, const char *p, size_t n);
char *ta_talloc_strndup_append(char *s, const char *a, size_t n);
char *ta_talloc_strndup_append_buffer(char *s, const char *a, size_t n);

char *ta_talloc_vasprintf_append(char *s, const char *fmt, va_list ap) TA_PRF(2, 0);
char *ta_talloc_vasprintf_append_buffer(char *s, const char *fmt, va_list ap) TA_PRF(2, 0);

char *ta_talloc_asprintf_append(char *s, const char *fmt, ...) TA_PRF(2, 3);
char *ta_talloc_asprintf_append_buffer(char *s, const char *fmt, ...) TA_PRF(2, 3);

// mpv specific stuff - should be made part of proper TA API

#define TA_FREEP(pctx) do {talloc_free(*(pctx)); *(pctx) = NULL;} while(0)

// Return number of allocated entries in typed array p[].
#define MP_TALLOC_AVAIL(p) (talloc_get_size(p) / sizeof((p)[0]))

// Resize array p so that p[count-1] is the last valid entry. ctx as ta parent.
#define MP_RESIZE_ARRAY(ctx, p, count)                          \
    do {                                                        \
        (p) = ta_xrealloc_size(ctx, p,                          \
                    ta_calc_array_size(sizeof((p)[0]), count)); \
    } while (0)

// Resize array p so that p[nextidx] is accessible. Preallocate additional
// space to make appending more efficient, never shrink. ctx as ta parent.
#define MP_TARRAY_GROW(ctx, p, nextidx)             \
    do {                                            \
        size_t nextidx_ = (nextidx);                \
        if (nextidx_ >= MP_TALLOC_AVAIL(p))         \
            MP_RESIZE_ARRAY(ctx, p, ta_calc_prealloc_elems(nextidx_)); \
    } while (0)

// Append the last argument to array p (with count idxvar), basically:
// p[idxvar++] = ...; ctx as ta parent.
#define MP_TARRAY_APPEND(ctx, p, idxvar, ...)       \
    do {                                            \
        MP_TARRAY_GROW(ctx, p, idxvar);             \
        (p)[(idxvar)] = (__VA_ARGS__);              \
        (idxvar)++;                                 \
    } while (0)

// Insert the last argument at p[at] (array p with count idxvar), basically:
// for(idxvar-1 down to at) p[n+1] = p[n]; p[at] = ...; idxvar++;
// ctx as ta parent. Required: at >= 0 && at <= idxvar.
#define MP_TARRAY_INSERT_AT(ctx, p, idxvar, at, ...)\
    do {                                            \
        size_t at_ = (at);                          \
        assert(at_ <= (idxvar));                    \
        MP_TARRAY_GROW(ctx, p, idxvar);             \
        memmove((p) + at_ + 1, (p) + at_,           \
                ((idxvar) - at_) * sizeof((p)[0])); \
        (idxvar)++;                                 \
        (p)[at_] = (__VA_ARGS__);                   \
    } while (0)

// Given an array p with count idxvar, insert c elements at p[at], so that
// p[at] to p[at+c-1] can be accessed. The elements at p[at] and following
// are shifted up by c before insertion. The new entries are uninitialized.
// ctx as ta parent. Required: at >= 0 && at <= idxvar.
#define MP_TARRAY_INSERT_N_AT(ctx, p, idxvar, at, c)\
    do {                                            \
        size_t at_ = (at);                          \
        assert(at_ <= (idxvar));                    \
        size_t c_ = (c);                            \
        MP_TARRAY_GROW(ctx, p, (idxvar) + c_);      \
        memmove((p) + at_ + c_, (p) + at_,          \
                ((idxvar) - at_) * sizeof((p)[0])); \
        (idxvar) += c_;                             \
    } while (0)

// Remove p[at] from array p with count idxvar (inverse of MP_TARRAY_INSERT_AT()).
// Doesn't actually free any memory, or do any other talloc calls.
#define MP_TARRAY_REMOVE_AT(p, idxvar, at)          \
    do {                                            \
        size_t at_ = (at);                          \
        assert(at_ <= (idxvar));                    \
        memmove((p) + at_, (p) + at_ + 1,           \
                ((idxvar) - at_ - 1) * sizeof((p)[0])); \
        (idxvar)--;                                 \
    } while (0)

// Returns whether or not there was any element to pop.
#define MP_TARRAY_POP(p, idxvar, out)               \
    ((idxvar) > 0                                   \
        ? (*(out) = (p)[--(idxvar)], true)          \
        : false                                     \
    )

#endif
