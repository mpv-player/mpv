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

#ifndef TA_H_
#define TA_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __GNUC__
#define TA_PRF(a1, a2) __attribute__ ((format(printf, a1, a2)))
#define TA_TYPEOF(t) __typeof__(t)
#else
#define TA_PRF(a1, a2)
#define TA_TYPEOF(t) void *
#endif

// Broken crap with __USE_MINGW_ANSI_STDIO
#if defined(__MINGW32__) && defined(__GNUC__) && !defined(__clang__)
#undef TA_PRF
#define TA_PRF(a1, a2) __attribute__ ((format (gnu_printf, a1, a2)))
#endif

#define TA_STRINGIFY_(x) # x
#define TA_STRINGIFY(x) TA_STRINGIFY_(x)

#ifdef NDEBUG
#define TA_LOC ""
#else
#define TA_LOC __FILE__ ":" TA_STRINGIFY(__LINE__)
#endif

// Core functions
void *ta_alloc_size(void *ta_parent, size_t size);
void *ta_zalloc_size(void *ta_parent, size_t size);
void *ta_realloc_size(void *ta_parent, void *ptr, size_t size);
size_t ta_get_size(void *ptr);
void ta_free(void *ptr);
void ta_free_children(void *ptr);
bool ta_set_destructor(void *ptr, void (*destructor)(void *));
bool ta_set_parent(void *ptr, void *ta_parent);
void *ta_find_parent(void *ptr);

// Utility functions
size_t ta_calc_array_size(size_t element_size, size_t count);
size_t ta_calc_prealloc_elems(size_t nextidx);
void *ta_new_context(void *ta_parent);
void *ta_steal_(void *ta_parent, void *ptr);
void *ta_memdup(void *ta_parent, void *ptr, size_t size);
char *ta_strdup(void *ta_parent, const char *str);
bool ta_strdup_append(char **str, const char *a);
bool ta_strdup_append_buffer(char **str, const char *a);
char *ta_strndup(void *ta_parent, const char *str, size_t n);
bool ta_strndup_append(char **str, const char *a, size_t n);
bool ta_strndup_append_buffer(char **str, const char *a, size_t n);
char *ta_asprintf(void *ta_parent, const char *fmt, ...) TA_PRF(2, 3);
char *ta_vasprintf(void *ta_parent, const char *fmt, va_list ap) TA_PRF(2, 0);
bool ta_asprintf_append(char **str, const char *fmt, ...) TA_PRF(2, 3);
bool ta_vasprintf_append(char **str, const char *fmt, va_list ap) TA_PRF(2, 0);
bool ta_asprintf_append_buffer(char **str, const char *fmt, ...) TA_PRF(2, 3);
bool ta_vasprintf_append_buffer(char **str, const char *fmt, va_list ap) TA_PRF(2, 0);

#define ta_new(ta_parent, type)  (type *)ta_alloc_size(ta_parent, sizeof(type))
#define ta_znew(ta_parent, type) (type *)ta_zalloc_size(ta_parent, sizeof(type))

#define ta_new_array(ta_parent, type, count) \
    (type *)ta_alloc_size(ta_parent, ta_calc_array_size(sizeof(type), count))

#define ta_znew_array(ta_parent, type, count) \
    (type *)ta_zalloc_size(ta_parent, ta_calc_array_size(sizeof(type), count))

#define ta_new_array_size(ta_parent, element_size, count) \
    ta_alloc_size(ta_parent, ta_calc_array_size(element_size, count))

#define ta_realloc(ta_parent, ptr, type, count) \
    (type *)ta_realloc_size(ta_parent, ptr, ta_calc_array_size(sizeof(type), count))

#define ta_new_ptrtype(ta_parent, ptr) \
    (TA_TYPEOF(ptr))ta_alloc_size(ta_parent, sizeof(*ptr))

#define ta_new_array_ptrtype(ta_parent, ptr, count) \
    (TA_TYPEOF(ptr))ta_new_array_size(ta_parent, sizeof(*(ptr)), count)

#define ta_steal(ta_parent, ptr) (TA_TYPEOF(ptr))ta_steal_(ta_parent, ptr)

#define ta_dup(ta_parent, ptr) \
    (TA_TYPEOF(ptr))ta_memdup(ta_parent, ptr, sizeof(*(ptr)))

// Ugly macros that crash on OOM.
// All of these mirror real functions (with a 'x' added after the 'ta_'
// prefix), and the only difference is that they will call abort() on allocation
// failures (such as out of memory conditions), instead of returning an error
// code.
#define ta_xalloc_size(...)             ta_oom_p(ta_alloc_size(__VA_ARGS__))
#define ta_xzalloc_size(...)            ta_oom_p(ta_zalloc_size(__VA_ARGS__))
#define ta_xset_destructor(...)         ta_oom_b(ta_set_destructor(__VA_ARGS__))
#define ta_xset_parent(...)             ta_oom_b(ta_set_parent(__VA_ARGS__))
#define ta_xnew_context(...)            ta_oom_p(ta_new_context(__VA_ARGS__))
#define ta_xstrdup_append(...)          ta_oom_b(ta_strdup_append(__VA_ARGS__))
#define ta_xstrdup_append_buffer(...)   ta_oom_b(ta_strdup_append_buffer(__VA_ARGS__))
#define ta_xstrndup_append(...)         ta_oom_b(ta_strndup_append(__VA_ARGS__))
#define ta_xstrndup_append_buffer(...)  ta_oom_b(ta_strndup_append_buffer(__VA_ARGS__))
#define ta_xasprintf(...)               ta_oom_s(ta_asprintf(__VA_ARGS__))
#define ta_xvasprintf(...)              ta_oom_s(ta_vasprintf(__VA_ARGS__))
#define ta_xasprintf_append(...)        ta_oom_b(ta_asprintf_append(__VA_ARGS__))
#define ta_xvasprintf_append(...)       ta_oom_b(ta_vasprintf_append(__VA_ARGS__))
#define ta_xasprintf_append_buffer(...) ta_oom_b(ta_asprintf_append_buffer(__VA_ARGS__))
#define ta_xvasprintf_append_buffer(...) ta_oom_b(ta_vasprintf_append_buffer(__VA_ARGS__))
#define ta_xnew(...)                    ta_oom_g(ta_new(__VA_ARGS__))
#define ta_xznew(...)                   ta_oom_g(ta_znew(__VA_ARGS__))
#define ta_xnew_array(...)              ta_oom_g(ta_new_array(__VA_ARGS__))
#define ta_xznew_array(...)             ta_oom_g(ta_znew_array(__VA_ARGS__))
#define ta_xnew_array_size(...)         ta_oom_p(ta_new_array_size(__VA_ARGS__))
#define ta_xnew_ptrtype(...)            ta_oom_g(ta_new_ptrtype(__VA_ARGS__))
#define ta_xnew_array_ptrtype(...)      ta_oom_g(ta_new_array_ptrtype(__VA_ARGS__))
#define ta_xdup(...)                    ta_oom_g(ta_dup(__VA_ARGS__))

#define ta_xsteal(ta_parent, ptr) (TA_TYPEOF(ptr))ta_xsteal_(ta_parent, ptr)
#define ta_xrealloc(ta_parent, ptr, type, count) \
    (type *)ta_xrealloc_size(ta_parent, ptr, ta_calc_array_size(sizeof(type), count))

// Can't be macros, because the OOM logic is slightly less trivial.
char *ta_xstrdup(void *ta_parent, const char *str);
char *ta_xstrndup(void *ta_parent, const char *str, size_t n);
void *ta_xsteal_(void *ta_parent, void *ptr);
void *ta_xmemdup(void *ta_parent, void *ptr, size_t size);
void *ta_xrealloc_size(void *ta_parent, void *ptr, size_t size);

#ifndef TA_NO_WRAPPERS
#define ta_alloc_size(...)      ta_dbg_set_loc(ta_alloc_size(__VA_ARGS__), TA_LOC)
#define ta_zalloc_size(...)     ta_dbg_set_loc(ta_zalloc_size(__VA_ARGS__), TA_LOC)
#define ta_realloc_size(...)    ta_dbg_set_loc(ta_realloc_size(__VA_ARGS__), TA_LOC)
#define ta_memdup(...)          ta_dbg_set_loc(ta_memdup(__VA_ARGS__), TA_LOC)
#define ta_xmemdup(...)         ta_dbg_set_loc(ta_xmemdup(__VA_ARGS__), TA_LOC)
#define ta_xrealloc_size(...)   ta_dbg_set_loc(ta_xrealloc_size(__VA_ARGS__), TA_LOC)
#endif

void ta_oom_b(bool b);
char *ta_oom_s(char *s);
void *ta_oom_p(void *p);
// Generic pointer
#define ta_oom_g(ptr) (TA_TYPEOF(ptr))ta_oom_p(ptr)

void ta_enable_leak_report(void);
void *ta_dbg_set_loc(void *ptr, const char *name);
void *ta_dbg_mark_as_string(void *ptr);

#endif
