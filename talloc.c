/* 
   Samba Unix SMB/CIFS implementation.

   Samba trivial allocation library - new interface

   NOTE: Please read talloc_guide.txt for full documentation

   Copyright (C) Andrew Tridgell 2004
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

/*
  inspired by http://swapped.cc/halloc/
*/

// Hardcode these for MPlayer assuming a working system.
// Original used autoconf detection with workarounds for broken systems.
#define HAVE_VA_COPY
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define strnlen rep_strnlen
static size_t rep_strnlen(const char *s, size_t max)
{
        size_t len;
  
        for (len = 0; len < max; len++) {
                if (s[len] == '\0') {
                        break;
                }
        }
        return len;  
}



#ifdef _SAMBA_BUILD_
#include "version.h"
#if (SAMBA_VERSION_MAJOR<4)
#include "includes.h"
/* This is to circumvent SAMBA3's paranoid malloc checker. Here in this file
 * we trust ourselves... */
#ifdef malloc
#undef malloc
#endif
#ifdef realloc
#undef realloc
#endif
#define _TALLOC_SAMBA3
#endif /* (SAMBA_VERSION_MAJOR<4) */
#endif /* _SAMBA_BUILD_ */

#ifndef _TALLOC_SAMBA3
// Workarounds for missing standard features, not used in MPlayer
// #include "replace.h"
#include "talloc.h"
#endif /* not _TALLOC_SAMBA3 */

/* use this to force every realloc to change the pointer, to stress test
   code that might not cope */
#define ALWAYS_REALLOC 0


#define MAX_TALLOC_SIZE 0x10000000
#define TALLOC_MAGIC 0xe814ec70
#define TALLOC_FLAG_FREE 0x01
#define TALLOC_FLAG_LOOP 0x02
#define TALLOC_FLAG_POOL 0x04		/* This is a talloc pool */
#define TALLOC_FLAG_POOLMEM 0x08	/* This is allocated in a pool */
#define TALLOC_MAGIC_REFERENCE ((const char *)1)

/* by default we abort when given a bad pointer (such as when talloc_free() is called 
   on a pointer that came from malloc() */
#ifndef TALLOC_ABORT
#define TALLOC_ABORT(reason) abort()
#endif

#ifndef discard_const_p
#if defined(__intptr_t_defined) || defined(HAVE_INTPTR_T)
# define discard_const_p(type, ptr) ((type *)((intptr_t)(ptr)))
#else
# define discard_const_p(type, ptr) ((type *)(ptr))
#endif
#endif

/* these macros gain us a few percent of speed on gcc */
#if (__GNUC__ >= 3)
/* the strange !! is to ensure that __builtin_expect() takes either 0 or 1
   as its first argument */
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#else
#ifndef likely
#define likely(x) (x)
#endif
#ifndef unlikely
#define unlikely(x) (x)
#endif
#endif

/* this null_context is only used if talloc_enable_leak_report() or
   talloc_enable_leak_report_full() is called, otherwise it remains
   NULL
*/
static void *null_context;
static void *autofree_context;

struct talloc_reference_handle {
	struct talloc_reference_handle *next, *prev;
	void *ptr;
};

typedef int (*talloc_destructor_t)(void *);

struct talloc_chunk {
	struct talloc_chunk *next, *prev;
	struct talloc_chunk *parent, *child;
	struct talloc_reference_handle *refs;
	talloc_destructor_t destructor;
	const char *name;
	size_t size;
	unsigned flags;

	/*
	 * "pool" has dual use:
	 *
	 * For the talloc pool itself (i.e. TALLOC_FLAG_POOL is set), "pool"
	 * marks the end of the currently allocated area.
	 *
	 * For members of the pool (i.e. TALLOC_FLAG_POOLMEM is set), "pool"
	 * is a pointer to the struct talloc_chunk of the pool that it was
	 * allocated from. This way children can quickly find the pool to chew
	 * from.
	 */
	void *pool;
};

/* 16 byte alignment seems to keep everyone happy */
#define TC_HDR_SIZE ((sizeof(struct talloc_chunk)+15)&~15)
#define TC_PTR_FROM_CHUNK(tc) ((void *)(TC_HDR_SIZE + (char*)tc))

static void talloc_abort_double_free(void)
{
	TALLOC_ABORT("Bad talloc magic value - double free"); 
}

static void talloc_abort_unknown_value(void)
{
	TALLOC_ABORT("Bad talloc magic value - unknown value"); 
}

/* panic if we get a bad magic value */
static inline struct talloc_chunk *talloc_chunk_from_ptr(const void *ptr)
{
	const char *pp = (const char *)ptr;
	struct talloc_chunk *tc = discard_const_p(struct talloc_chunk, pp - TC_HDR_SIZE);
	if (unlikely((tc->flags & (TALLOC_FLAG_FREE | ~0xF)) != TALLOC_MAGIC)) { 
		if (tc->flags & TALLOC_FLAG_FREE) {
			talloc_abort_double_free();
		} else {
			talloc_abort_unknown_value();
		}
	}
	return tc;
}

/* hook into the front of the list */
#define _TLIST_ADD(list, p) \
do { \
        if (!(list)) { \
		(list) = (p); \
		(p)->next = (p)->prev = NULL; \
	} else { \
		(list)->prev = (p); \
		(p)->next = (list); \
		(p)->prev = NULL; \
		(list) = (p); \
	}\
} while (0)

/* remove an element from a list - element doesn't have to be in list. */
#define _TLIST_REMOVE(list, p) \
do { \
	if ((p) == (list)) { \
		(list) = (p)->next; \
		if (list) (list)->prev = NULL; \
	} else { \
		if ((p)->prev) (p)->prev->next = (p)->next; \
		if ((p)->next) (p)->next->prev = (p)->prev; \
	} \
	if ((p) && ((p) != (list))) (p)->next = (p)->prev = NULL; \
} while (0)


/*
  return the parent chunk of a pointer
*/
static inline struct talloc_chunk *talloc_parent_chunk(const void *ptr)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(ptr);
	while (tc->prev) tc=tc->prev;

	return tc->parent;
}

void *talloc_parent(const void *ptr)
{
	struct talloc_chunk *tc = talloc_parent_chunk(ptr);
	return tc? TC_PTR_FROM_CHUNK(tc) : NULL;
}

/*
  find parents name
*/
const char *talloc_parent_name(const void *ptr)
{
	struct talloc_chunk *tc = talloc_parent_chunk(ptr);
	return tc? tc->name : NULL;
}

/*
  A pool carries an in-pool object count count in the first 16 bytes.
  bytes. This is done to support talloc_steal() to a parent outside of the
  pool. The count includes the pool itself, so a talloc_free() on a pool will
  only destroy the pool if the count has dropped to zero. A talloc_free() of a
  pool member will reduce the count, and eventually also call free(3) on the
  pool memory.

  The object count is not put into "struct talloc_chunk" because it is only
  relevant for talloc pools and the alignment to 16 bytes would increase the
  memory footprint of each talloc chunk by those 16 bytes.
*/

#define TALLOC_POOL_HDR_SIZE 16

static unsigned int *talloc_pool_objectcount(struct talloc_chunk *tc)
{
	return (unsigned int *)((char *)tc + sizeof(struct talloc_chunk));
}

/*
  Allocate from a pool
*/

static struct talloc_chunk *talloc_alloc_pool(struct talloc_chunk *parent,
					      size_t size)
{
	struct talloc_chunk *pool_ctx = NULL;
	size_t space_left;
	struct talloc_chunk *result;
	size_t chunk_size;

	if (parent == NULL) {
		return NULL;
	}

	if (parent->flags & TALLOC_FLAG_POOL) {
		pool_ctx = parent;
	}
	else if (parent->flags & TALLOC_FLAG_POOLMEM) {
		pool_ctx = (struct talloc_chunk *)parent->pool;
	}

	if (pool_ctx == NULL) {
		return NULL;
	}

	space_left = ((char *)pool_ctx + TC_HDR_SIZE + pool_ctx->size)
		- ((char *)pool_ctx->pool);

	/*
	 * Align size to 16 bytes
	 */
	chunk_size = ((size + 15) & ~15);

	if (space_left < chunk_size) {
		return NULL;
	}

	result = (struct talloc_chunk *)pool_ctx->pool;

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
	VALGRIND_MAKE_MEM_UNDEFINED(result, size);
#endif

	pool_ctx->pool = (void *)((char *)result + chunk_size);

	result->flags = TALLOC_MAGIC | TALLOC_FLAG_POOLMEM;
	result->pool = pool_ctx;

	*talloc_pool_objectcount(pool_ctx) += 1;

	return result;
}

/* 
   Allocate a bit of memory as a child of an existing pointer
*/
static inline void *__talloc(const void *context, size_t size)
{
	struct talloc_chunk *tc = NULL;

	if (unlikely(context == NULL)) {
		context = null_context;
	}

	if (unlikely(size >= MAX_TALLOC_SIZE)) {
		abort(); // return NULL;
	}

	if (context != NULL) {
		tc = talloc_alloc_pool(talloc_chunk_from_ptr(context),
				       TC_HDR_SIZE+size);
	}

	if (tc == NULL) {
		tc = (struct talloc_chunk *)malloc(TC_HDR_SIZE+size);
		if (unlikely(tc == NULL)) abort(); // return NULL;
		tc->flags = TALLOC_MAGIC;
		tc->pool  = NULL;
	}

	tc->size = size;
	tc->destructor = NULL;
	tc->child = NULL;
	tc->name = NULL;
	tc->refs = NULL;

	if (likely(context)) {
		struct talloc_chunk *parent = talloc_chunk_from_ptr(context);

		if (parent->child) {
			parent->child->parent = NULL;
			tc->next = parent->child;
			tc->next->prev = tc;
		} else {
			tc->next = NULL;
		}
		tc->parent = parent;
		tc->prev = NULL;
		parent->child = tc;
	} else {
		tc->next = tc->prev = tc->parent = NULL;
	}

	return TC_PTR_FROM_CHUNK(tc);
}

/*
 * Create a talloc pool
 */

void *talloc_pool(const void *context, size_t size)
{
	void *result = __talloc(context, size + TALLOC_POOL_HDR_SIZE);
	struct talloc_chunk *tc;

	if (unlikely(result == NULL)) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(result);

	tc->flags |= TALLOC_FLAG_POOL;
	tc->pool = (char *)result + TALLOC_POOL_HDR_SIZE;

	*talloc_pool_objectcount(tc) = 1;

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_NOACCESS)
	VALGRIND_MAKE_MEM_NOACCESS(tc->pool, size);
#endif

	return result;
}

/*
  setup a destructor to be called on free of a pointer
  the destructor should return 0 on success, or -1 on failure.
  if the destructor fails then the free is failed, and the memory can
  be continued to be used
*/
void _talloc_set_destructor(const void *ptr, int (*destructor)(void *))
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	tc->destructor = destructor;
}

/*
  increase the reference count on a piece of memory. 
*/
int talloc_increase_ref_count(const void *ptr)
{
	if (unlikely(!talloc_reference(null_context, ptr))) {
		return -1;
	}
	return 0;
}

/*
  helper for talloc_reference()

  this is referenced by a function pointer and should not be inline
*/
static int talloc_reference_destructor(void *ptr)
{
	struct talloc_reference_handle *handle = ptr;
	struct talloc_chunk *ptr_tc = talloc_chunk_from_ptr(handle->ptr);
	_TLIST_REMOVE(ptr_tc->refs, handle);
	return 0;
}

/*
   more efficient way to add a name to a pointer - the name must point to a 
   true string constant
*/
static inline void _talloc_set_name_const(const void *ptr, const char *name)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	tc->name = name;
}

/*
  internal talloc_named_const()
*/
static inline void *_talloc_named_const(const void *context, size_t size, const char *name)
{
	void *ptr;

	ptr = __talloc(context, size);
	if (unlikely(ptr == NULL)) {
		return NULL;
	}

	_talloc_set_name_const(ptr, name);

	return ptr;
}

/*
  make a secondary reference to a pointer, hanging off the given context.
  the pointer remains valid until both the original caller and this given
  context are freed.
  
  the major use for this is when two different structures need to reference the 
  same underlying data, and you want to be able to free the two instances separately,
  and in either order
*/
void *_talloc_reference(const void *context, const void *ptr)
{
	struct talloc_chunk *tc;
	struct talloc_reference_handle *handle;
	if (unlikely(ptr == NULL)) return NULL;

	tc = talloc_chunk_from_ptr(ptr);
	handle = (struct talloc_reference_handle *)_talloc_named_const(context,
						   sizeof(struct talloc_reference_handle),
						   TALLOC_MAGIC_REFERENCE);
	if (unlikely(handle == NULL)) return NULL;

	/* note that we hang the destructor off the handle, not the
	   main context as that allows the caller to still setup their
	   own destructor on the context if they want to */
	talloc_set_destructor(handle, talloc_reference_destructor);
	handle->ptr = discard_const_p(void, ptr);
	_TLIST_ADD(tc->refs, handle);
	return handle->ptr;
}


/* 
   internal talloc_free call
*/
static inline int _talloc_free(void *ptr)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return -1;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (unlikely(tc->refs)) {
		int is_child;
		/* check this is a reference from a child or grantchild
		 * back to it's parent or grantparent
		 *
		 * in that case we need to remove the reference and
		 * call another instance of talloc_free() on the current
		 * pointer.
		 */
		is_child = talloc_is_parent(tc->refs, ptr);
		_talloc_free(tc->refs);
		if (is_child) {
			return _talloc_free(ptr);
		}
		return -1;
	}

	if (unlikely(tc->flags & TALLOC_FLAG_LOOP)) {
		/* we have a free loop - stop looping */
		return 0;
	}

	if (unlikely(tc->destructor)) {
		talloc_destructor_t d = tc->destructor;
		if (d == (talloc_destructor_t)-1) {
			return -1;
		}
		tc->destructor = (talloc_destructor_t)-1;
		if (d(ptr) == -1) {
			tc->destructor = d;
			return -1;
		}
		tc->destructor = NULL;
	}

	if (tc->parent) {
		_TLIST_REMOVE(tc->parent->child, tc);
		if (tc->parent->child) {
			tc->parent->child->parent = tc->parent;
		}
	} else {
		if (tc->prev) tc->prev->next = tc->next;
		if (tc->next) tc->next->prev = tc->prev;
	}

	tc->flags |= TALLOC_FLAG_LOOP;

	while (tc->child) {
		/* we need to work out who will own an abandoned child
		   if it cannot be freed. In priority order, the first
		   choice is owner of any remaining reference to this
		   pointer, the second choice is our parent, and the
		   final choice is the null context. */
		void *child = TC_PTR_FROM_CHUNK(tc->child);
		const void *new_parent = null_context;
		if (unlikely(tc->child->refs)) {
			struct talloc_chunk *p = talloc_parent_chunk(tc->child->refs);
			if (p) new_parent = TC_PTR_FROM_CHUNK(p);
		}
		if (unlikely(_talloc_free(child) == -1)) {
			if (new_parent == null_context) {
				struct talloc_chunk *p = talloc_parent_chunk(ptr);
				if (p) new_parent = TC_PTR_FROM_CHUNK(p);
			}
			talloc_steal(new_parent, child);
		}
	}

	tc->flags |= TALLOC_FLAG_FREE;

	if (tc->flags & (TALLOC_FLAG_POOL|TALLOC_FLAG_POOLMEM)) {
		struct talloc_chunk *pool;
		unsigned int *pool_object_count;

		pool = (tc->flags & TALLOC_FLAG_POOL)
			? tc : (struct talloc_chunk *)tc->pool;

		pool_object_count = talloc_pool_objectcount(pool);

		if (*pool_object_count == 0) {
			TALLOC_ABORT("Pool object count zero!");
		}

		*pool_object_count -= 1;

		if (*pool_object_count == 0) {
			free(pool);
		}
	}
	else {
		free(tc);
	}
	return 0;
}

/* 
   move a lump of memory from one talloc context to another return the
   ptr on success, or NULL if it could not be transferred.
   passing NULL as ptr will always return NULL with no side effects.
*/
void *_talloc_steal(const void *new_ctx, const void *ptr)
{
	struct talloc_chunk *tc, *new_tc;

	if (unlikely(!ptr)) {
		return NULL;
	}

	if (unlikely(new_ctx == NULL)) {
		new_ctx = null_context;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (unlikely(new_ctx == NULL)) {
		if (tc->parent) {
			_TLIST_REMOVE(tc->parent->child, tc);
			if (tc->parent->child) {
				tc->parent->child->parent = tc->parent;
			}
		} else {
			if (tc->prev) tc->prev->next = tc->next;
			if (tc->next) tc->next->prev = tc->prev;
		}
		
		tc->parent = tc->next = tc->prev = NULL;
		return discard_const_p(void, ptr);
	}

	new_tc = talloc_chunk_from_ptr(new_ctx);

	if (unlikely(tc == new_tc || tc->parent == new_tc)) {
		return discard_const_p(void, ptr);
	}

	if (tc->parent) {
		_TLIST_REMOVE(tc->parent->child, tc);
		if (tc->parent->child) {
			tc->parent->child->parent = tc->parent;
		}
	} else {
		if (tc->prev) tc->prev->next = tc->next;
		if (tc->next) tc->next->prev = tc->prev;
	}

	tc->parent = new_tc;
	if (new_tc->child) new_tc->child->parent = NULL;
	_TLIST_ADD(new_tc->child, tc);

	return discard_const_p(void, ptr);
}



/*
  remove a secondary reference to a pointer. This undo's what
  talloc_reference() has done. The context and pointer arguments
  must match those given to a talloc_reference()
*/
static inline int talloc_unreference(const void *context, const void *ptr)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	struct talloc_reference_handle *h;

	if (unlikely(context == NULL)) {
		context = null_context;
	}

	for (h=tc->refs;h;h=h->next) {
		struct talloc_chunk *p = talloc_parent_chunk(h);
		if (p == NULL) {
			if (context == NULL) break;
		} else if (TC_PTR_FROM_CHUNK(p) == context) {
			break;
		}
	}
	if (h == NULL) {
		return -1;
	}

	return _talloc_free(h);
}

/*
  remove a specific parent context from a pointer. This is a more
  controlled varient of talloc_free()
*/
int talloc_unlink(const void *context, void *ptr)
{
	struct talloc_chunk *tc_p, *new_p;
	void *new_parent;

	if (ptr == NULL) {
		return -1;
	}

	if (context == NULL) {
		context = null_context;
	}

	if (talloc_unreference(context, ptr) == 0) {
		return 0;
	}

	if (context == NULL) {
		if (talloc_parent_chunk(ptr) != NULL) {
			return -1;
		}
	} else {
		if (talloc_chunk_from_ptr(context) != talloc_parent_chunk(ptr)) {
			return -1;
		}
	}
	
	tc_p = talloc_chunk_from_ptr(ptr);

	if (tc_p->refs == NULL) {
		return _talloc_free(ptr);
	}

	new_p = talloc_parent_chunk(tc_p->refs);
	if (new_p) {
		new_parent = TC_PTR_FROM_CHUNK(new_p);
	} else {
		new_parent = NULL;
	}

	if (talloc_unreference(new_parent, ptr) != 0) {
		return -1;
	}

	talloc_steal(new_parent, ptr);

	return 0;
}

/*
  add a name to an existing pointer - va_list version
*/
static inline const char *talloc_set_name_v(const void *ptr, const char *fmt, va_list ap) PRINTF_ATTRIBUTE(2,0);

static inline const char *talloc_set_name_v(const void *ptr, const char *fmt, va_list ap)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	tc->name = talloc_vasprintf(ptr, fmt, ap);
	if (likely(tc->name)) {
		_talloc_set_name_const(tc->name, ".name");
	}
	return tc->name;
}

/*
  add a name to an existing pointer
*/
const char *talloc_set_name(const void *ptr, const char *fmt, ...)
{
	const char *name;
	va_list ap;
	va_start(ap, fmt);
	name = talloc_set_name_v(ptr, fmt, ap);
	va_end(ap);
	return name;
}


/*
  create a named talloc pointer. Any talloc pointer can be named, and
  talloc_named() operates just like talloc() except that it allows you
  to name the pointer.
*/
void *talloc_named(const void *context, size_t size, const char *fmt, ...)
{
	va_list ap;
	void *ptr;
	const char *name;

	ptr = __talloc(context, size);
	if (unlikely(ptr == NULL)) return NULL;

	va_start(ap, fmt);
	name = talloc_set_name_v(ptr, fmt, ap);
	va_end(ap);

	if (unlikely(name == NULL)) {
		_talloc_free(ptr);
		return NULL;
	}

	return ptr;
}

/*
  return the name of a talloc ptr, or "UNNAMED"
*/
const char *talloc_get_name(const void *ptr)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	if (unlikely(tc->name == TALLOC_MAGIC_REFERENCE)) {
		return ".reference";
	}
	if (likely(tc->name)) {
		return tc->name;
	}
	return "UNNAMED";
}


/*
  check if a pointer has the given name. If it does, return the pointer,
  otherwise return NULL
*/
void *talloc_check_name(const void *ptr, const char *name)
{
	const char *pname;
	if (unlikely(ptr == NULL)) return NULL;
	pname = talloc_get_name(ptr);
	if (likely(pname == name || strcmp(pname, name) == 0)) {
		return discard_const_p(void, ptr);
	}
	return NULL;
}


/*
  this is for compatibility with older versions of talloc
*/
void *talloc_init(const char *fmt, ...)
{
	va_list ap;
	void *ptr;
	const char *name;

	/*
	 * samba3 expects talloc_report_depth_cb(NULL, ...)
	 * reports all talloc'ed memory, so we need to enable
	 * null_tracking
	 */
	talloc_enable_null_tracking();

	ptr = __talloc(NULL, 0);
	if (unlikely(ptr == NULL)) return NULL;

	va_start(ap, fmt);
	name = talloc_set_name_v(ptr, fmt, ap);
	va_end(ap);

	if (unlikely(name == NULL)) {
		_talloc_free(ptr);
		return NULL;
	}

	return ptr;
}

/*
  this is a replacement for the Samba3 talloc_destroy_pool functionality. It
  should probably not be used in new code. It's in here to keep the talloc
  code consistent across Samba 3 and 4.
*/
void talloc_free_children(void *ptr)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return;
	}

	tc = talloc_chunk_from_ptr(ptr);

	while (tc->child) {
		/* we need to work out who will own an abandoned child
		   if it cannot be freed. In priority order, the first
		   choice is owner of any remaining reference to this
		   pointer, the second choice is our parent, and the
		   final choice is the null context. */
		void *child = TC_PTR_FROM_CHUNK(tc->child);
		const void *new_parent = null_context;
		if (unlikely(tc->child->refs)) {
			struct talloc_chunk *p = talloc_parent_chunk(tc->child->refs);
			if (p) new_parent = TC_PTR_FROM_CHUNK(p);
		}
		if (unlikely(_talloc_free(child) == -1)) {
			if (new_parent == null_context) {
				struct talloc_chunk *p = talloc_parent_chunk(ptr);
				if (p) new_parent = TC_PTR_FROM_CHUNK(p);
			}
			talloc_steal(new_parent, child);
		}
	}

	if ((tc->flags & TALLOC_FLAG_POOL)
	    && (*talloc_pool_objectcount(tc) == 1)) {
		tc->pool = ((char *)tc + TC_HDR_SIZE + TALLOC_POOL_HDR_SIZE);
#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_NOACCESS)
		VALGRIND_MAKE_MEM_NOACCESS(
			tc->pool, tc->size - TALLOC_POOL_HDR_SIZE);
#endif
	}
}

/* 
   Allocate a bit of memory as a child of an existing pointer
*/
void *_talloc(const void *context, size_t size)
{
	return __talloc(context, size);
}

/*
  externally callable talloc_set_name_const()
*/
void talloc_set_name_const(const void *ptr, const char *name)
{
	_talloc_set_name_const(ptr, name);
}

/*
  create a named talloc pointer. Any talloc pointer can be named, and
  talloc_named() operates just like talloc() except that it allows you
  to name the pointer.
*/
void *talloc_named_const(const void *context, size_t size, const char *name)
{
	return _talloc_named_const(context, size, name);
}

/* 
   free a talloc pointer. This also frees all child pointers of this 
   pointer recursively

   return 0 if the memory is actually freed, otherwise -1. The memory
   will not be freed if the ref_count is > 1 or the destructor (if
   any) returns non-zero
*/
int talloc_free(void *ptr)
{
	return _talloc_free(ptr);
}



/*
  A talloc version of realloc. The context argument is only used if
  ptr is NULL
*/
void *_talloc_realloc(const void *context, void *ptr, size_t size, const char *name)
{
	struct talloc_chunk *tc;
	void *new_ptr;
	bool malloced = false;

	/* size zero is equivalent to free() */
	if (unlikely(size == 0)) {
		_talloc_free(ptr);
		return NULL;
	}

	if (unlikely(size >= MAX_TALLOC_SIZE)) {
		abort(); // return NULL;
	}

	/* realloc(NULL) is equivalent to malloc() */
	if (ptr == NULL) {
		return _talloc_named_const(context, size, name);
	}

	tc = talloc_chunk_from_ptr(ptr);

	/* don't allow realloc on referenced pointers */
	if (unlikely(tc->refs)) {
		abort(); // return NULL;
	}

	/* don't shrink if we have less than 1k to gain */
	if ((size < tc->size) && ((tc->size - size) < 1024)) {
		tc->size = size;
		return ptr;
	}

	/* by resetting magic we catch users of the old memory */
	tc->flags |= TALLOC_FLAG_FREE;

#if ALWAYS_REALLOC
	new_ptr = malloc(size + TC_HDR_SIZE);
	if (new_ptr) {
		memcpy(new_ptr, tc, tc->size + TC_HDR_SIZE);
		free(tc);
	}
#else
	if (tc->flags & TALLOC_FLAG_POOLMEM) {

		new_ptr = talloc_alloc_pool(tc, size + TC_HDR_SIZE);
		*talloc_pool_objectcount((struct talloc_chunk *)
					 (tc->pool)) -= 1;

		if (new_ptr == NULL) {
			new_ptr = malloc(TC_HDR_SIZE+size);
			malloced = true;
		}

		if (new_ptr) {
			memcpy(new_ptr, tc, MIN(tc->size,size) + TC_HDR_SIZE);
		}
	}
	else {
		new_ptr = realloc(tc, size + TC_HDR_SIZE);
	}
#endif
	if (unlikely(!new_ptr)) {	
		tc->flags &= ~TALLOC_FLAG_FREE; 
		abort(); // return NULL; 
	}

	tc = (struct talloc_chunk *)new_ptr;
	tc->flags &= ~TALLOC_FLAG_FREE;
	if (malloced) {
		tc->flags &= ~TALLOC_FLAG_POOLMEM;
	}
	if (tc->parent) {
		tc->parent->child = tc;
	}
	if (tc->child) {
		tc->child->parent = tc;
	}

	if (tc->prev) {
		tc->prev->next = tc;
	}
	if (tc->next) {
		tc->next->prev = tc;
	}

	tc->size = size;
	_talloc_set_name_const(TC_PTR_FROM_CHUNK(tc), name);

	return TC_PTR_FROM_CHUNK(tc);
}

/*
  a wrapper around talloc_steal() for situations where you are moving a pointer
  between two structures, and want the old pointer to be set to NULL
*/
void *_talloc_move(const void *new_ctx, const void *_pptr)
{
	const void **pptr = discard_const_p(const void *,_pptr);
	void *ret = _talloc_steal(new_ctx, *pptr);
	(*pptr) = NULL;
	return ret;
}

/*
  return the total size of a talloc pool (subtree)
*/
size_t talloc_total_size(const void *ptr)
{
	size_t total = 0;
	struct talloc_chunk *c, *tc;

	if (ptr == NULL) {
		ptr = null_context;
	}
	if (ptr == NULL) {
		return 0;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (tc->flags & TALLOC_FLAG_LOOP) {
		return 0;
	}

	tc->flags |= TALLOC_FLAG_LOOP;

	total = tc->size;
	for (c=tc->child;c;c=c->next) {
		total += talloc_total_size(TC_PTR_FROM_CHUNK(c));
	}

	tc->flags &= ~TALLOC_FLAG_LOOP;

	return total;
}

/*
  return the total number of blocks in a talloc pool (subtree)
*/
size_t talloc_total_blocks(const void *ptr)
{
	size_t total = 0;
	struct talloc_chunk *c, *tc = talloc_chunk_from_ptr(ptr);

	if (tc->flags & TALLOC_FLAG_LOOP) {
		return 0;
	}

	tc->flags |= TALLOC_FLAG_LOOP;

	total++;
	for (c=tc->child;c;c=c->next) {
		total += talloc_total_blocks(TC_PTR_FROM_CHUNK(c));
	}

	tc->flags &= ~TALLOC_FLAG_LOOP;

	return total;
}

/*
  return the number of external references to a pointer
*/
size_t talloc_reference_count(const void *ptr)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	struct talloc_reference_handle *h;
	size_t ret = 0;

	for (h=tc->refs;h;h=h->next) {
		ret++;
	}
	return ret;
}

/*
  report on memory usage by all children of a pointer, giving a full tree view
*/
void talloc_report_depth_cb(const void *ptr, int depth, int max_depth,
			    void (*callback)(const void *ptr,
			  		     int depth, int max_depth,
					     int is_ref,
					     void *private_data),
			    void *private_data)
{
	struct talloc_chunk *c, *tc;

	if (ptr == NULL) {
		ptr = null_context;
	}
	if (ptr == NULL) return;

	tc = talloc_chunk_from_ptr(ptr);

	if (tc->flags & TALLOC_FLAG_LOOP) {
		return;
	}

	callback(ptr, depth, max_depth, 0, private_data);

	if (max_depth >= 0 && depth >= max_depth) {
		return;
	}

	tc->flags |= TALLOC_FLAG_LOOP;
	for (c=tc->child;c;c=c->next) {
		if (c->name == TALLOC_MAGIC_REFERENCE) {
			struct talloc_reference_handle *h = (struct talloc_reference_handle *)TC_PTR_FROM_CHUNK(c);
			callback(h->ptr, depth + 1, max_depth, 1, private_data);
		} else {
			talloc_report_depth_cb(TC_PTR_FROM_CHUNK(c), depth + 1, max_depth, callback, private_data);
		}
	}
	tc->flags &= ~TALLOC_FLAG_LOOP;
}

static void talloc_report_depth_FILE_helper(const void *ptr, int depth, int max_depth, int is_ref, void *_f)
{
	const char *name = talloc_get_name(ptr);
	FILE *f = (FILE *)_f;

	if (is_ref) {
		fprintf(f, "%*sreference to: %s\n", depth*4, "", name);
		return;
	}

	if (depth == 0) {
		fprintf(f,"%stalloc report on '%s' (total %6lu bytes in %3lu blocks)\n", 
			(max_depth < 0 ? "full " :""), name,
			(unsigned long)talloc_total_size(ptr),
			(unsigned long)talloc_total_blocks(ptr));
		return;
	}

	fprintf(f, "%*s%-30s contains %6lu bytes in %3lu blocks (ref %d) %p\n", 
		depth*4, "",
		name,
		(unsigned long)talloc_total_size(ptr),
		(unsigned long)talloc_total_blocks(ptr),
		(int)talloc_reference_count(ptr), ptr);

#if 0
	fprintf(f, "content: ");
	if (talloc_total_size(ptr)) {
		int tot = talloc_total_size(ptr);
		int i;

		for (i = 0; i < tot; i++) {
			if ((((char *)ptr)[i] > 31) && (((char *)ptr)[i] < 126)) {
				fprintf(f, "%c", ((char *)ptr)[i]);
			} else {
				fprintf(f, "~%02x", ((char *)ptr)[i]);
			}
		}
	}
	fprintf(f, "\n");
#endif
}

/*
  report on memory usage by all children of a pointer, giving a full tree view
*/
void talloc_report_depth_file(const void *ptr, int depth, int max_depth, FILE *f)
{
	talloc_report_depth_cb(ptr, depth, max_depth, talloc_report_depth_FILE_helper, f);
	fflush(f);
}

/*
  report on memory usage by all children of a pointer, giving a full tree view
*/
void talloc_report_full(const void *ptr, FILE *f)
{
	talloc_report_depth_file(ptr, 0, -1, f);
}

/*
  report on memory usage by all children of a pointer
*/
void talloc_report(const void *ptr, FILE *f)
{
	talloc_report_depth_file(ptr, 0, 1, f);
}

/*
  report on any memory hanging off the null context
*/
static void talloc_report_null(void)
{
	if (talloc_total_size(null_context) != 0) {
		talloc_report(null_context, stderr);
	}
}

/*
  report on any memory hanging off the null context
*/
static void talloc_report_null_full(void)
{
	if (talloc_total_size(null_context) != 0) {
		talloc_report_full(null_context, stderr);
	}
}

/*
  enable tracking of the NULL context
*/
void talloc_enable_null_tracking(void)
{
	if (null_context == NULL) {
		null_context = _talloc_named_const(NULL, 0, "null_context");
	}
}

/*
  disable tracking of the NULL context
*/
void talloc_disable_null_tracking(void)
{
	_talloc_free(null_context);
	null_context = NULL;
}

/*
  enable leak reporting on exit
*/
void talloc_enable_leak_report(void)
{
	talloc_enable_null_tracking();
	atexit(talloc_report_null);
}

/*
  enable full leak reporting on exit
*/
void talloc_enable_leak_report_full(void)
{
	talloc_enable_null_tracking();
	atexit(talloc_report_null_full);
}

/* 
   talloc and zero memory. 
*/
void *_talloc_zero(const void *ctx, size_t size, const char *name)
{
	void *p = _talloc_named_const(ctx, size, name);

	if (p) {
		memset(p, '\0', size);
	}

	return p;
}

/*
  memdup with a talloc. 
*/
void *_talloc_memdup(const void *t, const void *p, size_t size, const char *name)
{
	void *newp = _talloc_named_const(t, size, name);

	if (likely(newp)) {
		memcpy(newp, p, size);
	}

	return newp;
}

static inline char *__talloc_strlendup(const void *t, const char *p, size_t len)
{
	char *ret;

	ret = (char *)__talloc(t, len + 1);
	if (unlikely(!ret)) return NULL;

	memcpy(ret, p, len);
	ret[len] = 0;

	_talloc_set_name_const(ret, ret);
	return ret;
}

/*
  strdup with a talloc
*/
char *talloc_strdup(const void *t, const char *p)
{
	if (unlikely(!p)) return NULL;
	return __talloc_strlendup(t, p, strlen(p));
}

/*
  strndup with a talloc
*/
char *talloc_strndup(const void *t, const char *p, size_t n)
{
	if (unlikely(!p)) return NULL;
	return __talloc_strlendup(t, p, strnlen(p, n));
}

static inline char *__talloc_strlendup_append(char *s, size_t slen,
					      const char *a, size_t alen)
{
	char *ret;

	ret = talloc_realloc(NULL, s, char, slen + alen + 1);
	if (unlikely(!ret)) return NULL;

	/* append the string and the trailing \0 */
	memcpy(&ret[slen], a, alen);
	ret[slen+alen] = 0;

	_talloc_set_name_const(ret, ret);
	return ret;
}

/*
 * Appends at the end of the string.
 */
char *talloc_strdup_append(char *s, const char *a)
{
	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	return __talloc_strlendup_append(s, strlen(s), a, strlen(a));
}

/*
 * Appends at the end of the talloc'ed buffer,
 * not the end of the string.
 */
char *talloc_strdup_append_buffer(char *s, const char *a)
{
	size_t slen;

	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	slen = talloc_get_size(s);
	if (likely(slen > 0)) {
		slen--;
	}

	return __talloc_strlendup_append(s, slen, a, strlen(a));
}

/*
 * Appends at the end of the string.
 */
char *talloc_strndup_append(char *s, const char *a, size_t n)
{
	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	return __talloc_strlendup_append(s, strlen(s), a, strnlen(a, n));
}

/*
 * Appends at the end of the talloc'ed buffer,
 * not the end of the string.
 */
char *talloc_strndup_append_buffer(char *s, const char *a, size_t n)
{
	size_t slen;

	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	slen = talloc_get_size(s);
	if (likely(slen > 0)) {
		slen--;
	}

	return __talloc_strlendup_append(s, slen, a, strnlen(a, n));
}

#ifndef HAVE_VA_COPY
#ifdef HAVE___VA_COPY
#define va_copy(dest, src) __va_copy(dest, src)
#else
#define va_copy(dest, src) (dest) = (src)
#endif
#endif

char *talloc_vasprintf(const void *t, const char *fmt, va_list ap)
{
	int len;
	char *ret;
	va_list ap2;
	char c;

	/* this call looks strange, but it makes it work on older solaris boxes */
	va_copy(ap2, ap);
	len = vsnprintf(&c, 1, fmt, ap2);
	va_end(ap2);
	if (unlikely(len < 0)) {
		abort(); // return NULL;
	}

	ret = (char *)__talloc(t, len+1);
	if (unlikely(!ret)) return NULL;

	va_copy(ap2, ap);
	vsnprintf(ret, len+1, fmt, ap2);
	va_end(ap2);

	_talloc_set_name_const(ret, ret);
	return ret;
}


/*
  Perform string formatting, and return a pointer to newly allocated
  memory holding the result, inside a memory pool.
 */
char *talloc_asprintf(const void *t, const char *fmt, ...)
{
	va_list ap;
	char *ret;

	va_start(ap, fmt);
	ret = talloc_vasprintf(t, fmt, ap);
	va_end(ap);
	return ret;
}

static inline char *__talloc_vaslenprintf_append(char *s, size_t slen,
						 const char *fmt, va_list ap)
						 PRINTF_ATTRIBUTE(3,0);

static inline char *__talloc_vaslenprintf_append(char *s, size_t slen,
						 const char *fmt, va_list ap)
{
	ssize_t alen;
	va_list ap2;
	char c;

	va_copy(ap2, ap);
	alen = vsnprintf(&c, 1, fmt, ap2);
	va_end(ap2);

	if (alen <= 0) {
		/* Either the vsnprintf failed or the format resulted in
		 * no characters being formatted. In the former case, we
		 * ought to return NULL, in the latter we ought to return
		 * the original string. Most current callers of this
		 * function expect it to never return NULL.
		 */
		return s;
	}

	s = talloc_realloc(NULL, s, char, slen + alen + 1);
	if (!s) return NULL;

	va_copy(ap2, ap);
	vsnprintf(s + slen, alen + 1, fmt, ap2);
	va_end(ap2);

	_talloc_set_name_const(s, s);
	return s;
}

/**
 * Realloc @p s to append the formatted result of @p fmt and @p ap,
 * and return @p s, which may have moved.  Good for gradually
 * accumulating output into a string buffer. Appends at the end
 * of the string.
 **/
char *talloc_vasprintf_append(char *s, const char *fmt, va_list ap)
{
	if (unlikely(!s)) {
		return talloc_vasprintf(NULL, fmt, ap);
	}

	return __talloc_vaslenprintf_append(s, strlen(s), fmt, ap);
}

/**
 * Realloc @p s to append the formatted result of @p fmt and @p ap,
 * and return @p s, which may have moved. Always appends at the
 * end of the talloc'ed buffer, not the end of the string.
 **/
char *talloc_vasprintf_append_buffer(char *s, const char *fmt, va_list ap)
{
	size_t slen;

	if (unlikely(!s)) {
		return talloc_vasprintf(NULL, fmt, ap);
	}

	slen = talloc_get_size(s);
	if (likely(slen > 0)) {
		slen--;
	}

	return __talloc_vaslenprintf_append(s, slen, fmt, ap);
}

/*
  Realloc @p s to append the formatted result of @p fmt and return @p
  s, which may have moved.  Good for gradually accumulating output
  into a string buffer.
 */
char *talloc_asprintf_append(char *s, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	s = talloc_vasprintf_append(s, fmt, ap);
	va_end(ap);
	return s;
}

/*
  Realloc @p s to append the formatted result of @p fmt and return @p
  s, which may have moved.  Good for gradually accumulating output
  into a buffer.
 */
char *talloc_asprintf_append_buffer(char *s, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	s = talloc_vasprintf_append_buffer(s, fmt, ap);
	va_end(ap);
	return s;
}

/*
  alloc an array, checking for integer overflow in the array size
*/
void *_talloc_array(const void *ctx, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		abort(); // return NULL;
	}
	return _talloc_named_const(ctx, el_size * count, name);
}

/*
  alloc an zero array, checking for integer overflow in the array size
*/
void *_talloc_zero_array(const void *ctx, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		abort(); // return NULL;
	}
	return _talloc_zero(ctx, el_size * count, name);
}

/*
  realloc an array, checking for integer overflow in the array size
*/
void *_talloc_realloc_array(const void *ctx, void *ptr, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		abort(); // return NULL;
	}
	return _talloc_realloc(ctx, ptr, el_size * count, name);
}

/*
  a function version of talloc_realloc(), so it can be passed as a function pointer
  to libraries that want a realloc function (a realloc function encapsulates
  all the basic capabilities of an allocation library, which is why this is useful)
*/
void *talloc_realloc_fn(const void *context, void *ptr, size_t size)
{
	return _talloc_realloc(context, ptr, size, NULL);
}


static int talloc_autofree_destructor(void *ptr)
{
	autofree_context = NULL;
	return 0;
}

static void talloc_autofree(void)
{
	_talloc_free(autofree_context);
}

/*
  return a context which will be auto-freed on exit
  this is useful for reducing the noise in leak reports
*/
void *talloc_autofree_context(void)
{
	if (autofree_context == NULL) {
		autofree_context = _talloc_named_const(NULL, 0, "autofree_context");
		talloc_set_destructor(autofree_context, talloc_autofree_destructor);
		atexit(talloc_autofree);
	}
	return autofree_context;
}

size_t talloc_get_size(const void *context)
{
	struct talloc_chunk *tc;

	if (context == NULL)
		return 0;

	tc = talloc_chunk_from_ptr(context);

	return tc->size;
}

/*
  find a parent of this context that has the given name, if any
*/
void *talloc_find_parent_byname(const void *context, const char *name)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(context);
	while (tc) {
		if (tc->name && strcmp(tc->name, name) == 0) {
			return TC_PTR_FROM_CHUNK(tc);
		}
		while (tc && tc->prev) tc = tc->prev;
		if (tc) {
			tc = tc->parent;
		}
	}
	return NULL;
}

/*
  show the parentage of a context
*/
void talloc_show_parents(const void *context, FILE *file)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		fprintf(file, "talloc no parents for NULL\n");
		return;
	}

	tc = talloc_chunk_from_ptr(context);
	fprintf(file, "talloc parents of '%s'\n", talloc_get_name(context));
	while (tc) {
		fprintf(file, "\t'%s'\n", talloc_get_name(TC_PTR_FROM_CHUNK(tc)));
		while (tc && tc->prev) tc = tc->prev;
		if (tc) {
			tc = tc->parent;
		}
	}
	fflush(file);
}

/*
  return 1 if ptr is a parent of context
*/
int talloc_is_parent(const void *context, const void *ptr)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		return 0;
	}

	tc = talloc_chunk_from_ptr(context);
	while (tc) {
		if (TC_PTR_FROM_CHUNK(tc) == ptr) return 1;
		while (tc && tc->prev) tc = tc->prev;
		if (tc) {
			tc = tc->parent;
		}
	}
	return 0;
}
