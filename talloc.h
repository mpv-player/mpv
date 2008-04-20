#ifndef _TALLOC_H_
#define _TALLOC_H_
/* 
   Unix SMB/CIFS implementation.
   Samba temporary memory allocation functions

   Copyright (C) Andrew Tridgell 2004-2005
   Copyright (C) Stefan Metzmacher 2006
   
     ** NOTE! The following LGPL license applies to the talloc
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/* this is only needed for compatibility with the old talloc */
typedef void TALLOC_CTX;

/*
  this uses a little trick to allow __LINE__ to be stringified
*/
#ifndef __location__
#define __TALLOC_STRING_LINE1__(s)    #s
#define __TALLOC_STRING_LINE2__(s)   __TALLOC_STRING_LINE1__(s)
#define __TALLOC_STRING_LINE3__  __TALLOC_STRING_LINE2__(__LINE__)
#define __location__ __FILE__ ":" __TALLOC_STRING_LINE3__
#endif

#ifndef TALLOC_DEPRECATED
#define TALLOC_DEPRECATED 0
#endif

#ifndef PRINTF_ATTRIBUTE
#if (__GNUC__ >= 3)
/** Use gcc attribute to check printf fns.  a1 is the 1-based index of
 * the parameter containing the format, and a2 the index of the first
 * argument. Note that some gcc 2.x versions don't handle this
 * properly **/
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif
#endif

/* try to make talloc_set_destructor() and talloc_steal() type safe,
   if we have a recent gcc */
#if (__GNUC__ >= 3)
#define _TALLOC_TYPEOF(ptr) __typeof__(ptr)
#define talloc_set_destructor(ptr, function)				      \
	do {								      \
		int (*_talloc_destructor_fn)(_TALLOC_TYPEOF(ptr)) = (function);	      \
		_talloc_set_destructor((ptr), (int (*)(void *))_talloc_destructor_fn); \
	} while(0)
/* this extremely strange macro is to avoid some braindamaged warning
   stupidity in gcc 4.1.x */
#define talloc_steal(ctx, ptr) ({ _TALLOC_TYPEOF(ptr) __talloc_steal_ret = (_TALLOC_TYPEOF(ptr))_talloc_steal((ctx),(ptr)); __talloc_steal_ret; })
#else
#define talloc_set_destructor(ptr, function) \
	_talloc_set_destructor((ptr), (int (*)(void *))(function))
#define _TALLOC_TYPEOF(ptr) void *
#define talloc_steal(ctx, ptr) (_TALLOC_TYPEOF(ptr))_talloc_steal((ctx),(ptr))
#endif

#define talloc_reference(ctx, ptr) (_TALLOC_TYPEOF(ptr))_talloc_reference((ctx),(ptr))
#define talloc_move(ctx, ptr) (_TALLOC_TYPEOF(*(ptr)))_talloc_move((ctx),(void *)(ptr))

/* useful macros for creating type checked pointers */
#define talloc(ctx, type) (type *)talloc_named_const(ctx, sizeof(type), #type)
#define talloc_size(ctx, size) talloc_named_const(ctx, size, __location__)
#define talloc_ptrtype(ctx, ptr) (_TALLOC_TYPEOF(ptr))talloc_size(ctx, sizeof(*(ptr)))

#define talloc_new(ctx) talloc_named_const(ctx, 0, "talloc_new: " __location__)

#define talloc_zero(ctx, type) (type *)_talloc_zero(ctx, sizeof(type), #type)
#define talloc_zero_size(ctx, size) _talloc_zero(ctx, size, __location__)

#define talloc_zero_array(ctx, type, count) (type *)_talloc_zero_array(ctx, sizeof(type), count, #type)
#define talloc_array(ctx, type, count) (type *)_talloc_array(ctx, sizeof(type), count, #type)
#define talloc_array_size(ctx, size, count) _talloc_array(ctx, size, count, __location__)
#define talloc_array_ptrtype(ctx, ptr, count) (_TALLOC_TYPEOF(ptr))talloc_array_size(ctx, sizeof(*(ptr)), count)

#define talloc_realloc(ctx, p, type, count) (type *)_talloc_realloc_array(ctx, p, sizeof(type), count, #type)
#define talloc_realloc_size(ctx, ptr, size) _talloc_realloc(ctx, ptr, size, __location__)

#define talloc_memdup(t, p, size) _talloc_memdup(t, p, size, __location__)

#define talloc_set_type(ptr, type) talloc_set_name_const(ptr, #type)
#define talloc_get_type(ptr, type) (type *)talloc_check_name(ptr, #type)

#define talloc_find_parent_bytype(ptr, type) (type *)talloc_find_parent_byname(ptr, #type)

#if TALLOC_DEPRECATED
#define talloc_zero_p(ctx, type) talloc_zero(ctx, type)
#define talloc_p(ctx, type) talloc(ctx, type)
#define talloc_array_p(ctx, type, count) talloc_array(ctx, type, count)
#define talloc_realloc_p(ctx, p, type, count) talloc_realloc(ctx, p, type, count)
#define talloc_destroy(ctx) talloc_free(ctx)
#define talloc_append_string(c, s, a) (s?talloc_strdup_append(s,a):talloc_strdup(c, a))
#endif

/* The following definitions come from talloc.c  */
void *_talloc(const void *context, size_t size);
void *talloc_pool(const void *context, size_t size);
void _talloc_set_destructor(const void *ptr, int (*destructor)(void *));
int talloc_increase_ref_count(const void *ptr);
size_t talloc_reference_count(const void *ptr);
void *_talloc_reference(const void *context, const void *ptr);
int talloc_unlink(const void *context, void *ptr);
const char *talloc_set_name(const void *ptr, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);
void talloc_set_name_const(const void *ptr, const char *name);
void *talloc_named(const void *context, size_t size, 
		   const char *fmt, ...) PRINTF_ATTRIBUTE(3,4);
void *talloc_named_const(const void *context, size_t size, const char *name);
const char *talloc_get_name(const void *ptr);
void *talloc_check_name(const void *ptr, const char *name);
void *talloc_parent(const void *ptr);
const char *talloc_parent_name(const void *ptr);
void *talloc_init(const char *fmt, ...) PRINTF_ATTRIBUTE(1,2);
int talloc_free(void *ptr);
void talloc_free_children(void *ptr);
void *_talloc_realloc(const void *context, void *ptr, size_t size, const char *name);
void *_talloc_steal(const void *new_ctx, const void *ptr);
void *_talloc_move(const void *new_ctx, const void *pptr);
size_t talloc_total_size(const void *ptr);
size_t talloc_total_blocks(const void *ptr);
void talloc_report_depth_cb(const void *ptr, int depth, int max_depth,
			    void (*callback)(const void *ptr,
			  		     int depth, int max_depth,
					     int is_ref,
					     void *private_data),
			    void *private_data);
void talloc_report_depth_file(const void *ptr, int depth, int max_depth, FILE *f);
void talloc_report_full(const void *ptr, FILE *f);
void talloc_report(const void *ptr, FILE *f);
void talloc_enable_null_tracking(void);
void talloc_disable_null_tracking(void);
void talloc_enable_leak_report(void);
void talloc_enable_leak_report_full(void);
void *_talloc_zero(const void *ctx, size_t size, const char *name);
void *_talloc_memdup(const void *t, const void *p, size_t size, const char *name);
void *_talloc_array(const void *ctx, size_t el_size, unsigned count, const char *name);
void *_talloc_zero_array(const void *ctx, size_t el_size, unsigned count, const char *name);
void *_talloc_realloc_array(const void *ctx, void *ptr, size_t el_size, unsigned count, const char *name);
void *talloc_realloc_fn(const void *context, void *ptr, size_t size);
void *talloc_autofree_context(void);
size_t talloc_get_size(const void *ctx);
void *talloc_find_parent_byname(const void *ctx, const char *name);
void talloc_show_parents(const void *context, FILE *file);
int talloc_is_parent(const void *context, const void *ptr);

char *talloc_strdup(const void *t, const char *p);
char *talloc_strdup_append(char *s, const char *a);
char *talloc_strdup_append_buffer(char *s, const char *a);

char *talloc_strndup(const void *t, const char *p, size_t n);
char *talloc_strndup_append(char *s, const char *a, size_t n);
char *talloc_strndup_append_buffer(char *s, const char *a, size_t n);

char *talloc_vasprintf(const void *t, const char *fmt, va_list ap) PRINTF_ATTRIBUTE(2,0);
char *talloc_vasprintf_append(char *s, const char *fmt, va_list ap) PRINTF_ATTRIBUTE(2,0);
char *talloc_vasprintf_append_buffer(char *s, const char *fmt, va_list ap) PRINTF_ATTRIBUTE(2,0);

char *talloc_asprintf(const void *t, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);
char *talloc_asprintf_append(char *s, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);
char *talloc_asprintf_append_buffer(char *s, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);

#endif
