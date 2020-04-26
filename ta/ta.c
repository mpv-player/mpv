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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define TA_NO_WRAPPERS
#include "ta.h"

// Note: the actual minimum alignment is dictated by malloc(). It doesn't
//       make sense to set this value higher than malloc's alignment.
#define MIN_ALIGN 16

#if !defined(TA_MEMORY_DEBUGGING)
    #if !defined(NDEBUG)
        #define TA_MEMORY_DEBUGGING 1
    #else
        #define TA_MEMORY_DEBUGGING 0
    #endif
#endif

struct ta_header {
    size_t size;                // size of the user allocation
    // Invariant: parent!=NULL => prev==NULL
    struct ta_header *prev;     // siblings list (by destructor order)
    struct ta_header *next;
    // Invariant: parent==NULL || parent->child==this
    struct ta_header *child;    // points to first child
    struct ta_header *parent;   // set for _first_ child only, NULL otherwise
    void (*destructor)(void *);
#if TA_MEMORY_DEBUGGING
    unsigned int canary;
    struct ta_header *leak_next;
    struct ta_header *leak_prev;
    const char *name;
#endif
};

#define CANARY 0xD3ADB3EF

union aligned_header {
    struct ta_header ta;
    // Make sure to satisfy typical alignment requirements
    void *align_ptr;
    int align_int;
    double align_d;
    long long align_ll;
    char align_min[(sizeof(struct ta_header) + MIN_ALIGN - 1) & ~(MIN_ALIGN - 1)];
};

#define PTR_TO_HEADER(ptr) (&((union aligned_header *)(ptr) - 1)->ta)
#define PTR_FROM_HEADER(h) ((void *)((union aligned_header *)(h) + 1))

#define MAX_ALLOC (((size_t)-1) - sizeof(union aligned_header))

static void ta_dbg_add(struct ta_header *h);
static void ta_dbg_check_header(struct ta_header *h);
static void ta_dbg_remove(struct ta_header *h);

static struct ta_header *get_header(void *ptr)
{
    struct ta_header *h = ptr ? PTR_TO_HEADER(ptr) : NULL;
    ta_dbg_check_header(h);
    return h;
}

/* Set the parent allocation of ptr. If parent==NULL, remove the parent.
 * Setting parent==NULL (with ptr!=NULL) unsets the parent of ptr.
 * With ptr==NULL, the function does nothing.
 *
 * Warning: if ta_parent is a direct or indirect child of ptr, things will go
 *          wrong. The function will apparently succeed, but creates circular
 *          parent links, which are not allowed.
 */
void ta_set_parent(void *ptr, void *ta_parent)
{
    struct ta_header *ch = get_header(ptr);
    if (!ch)
        return;
    struct ta_header *new_parent = get_header(ta_parent);
    // Unlink from previous parent
    if (ch->prev)
        ch->prev->next = ch->next;
    if (ch->next)
        ch->next->prev = ch->prev;
    // If ch was the first child, change child link of old parent
    if (ch->parent) {
        assert(ch->parent->child == ch);
        ch->parent->child = ch->next;
        if (ch->parent->child) {
            assert(!ch->parent->child->parent);
            ch->parent->child->parent = ch->parent;
        }
    }
    ch->next = ch->prev = ch->parent = NULL;
    // Link to new parent - insert at start of list (LIFO destructor order)
    if (new_parent) {
        ch->next = new_parent->child;
        if (ch->next) {
            ch->next->prev = ch;
            ch->next->parent = NULL;
        }
        new_parent->child = ch;
        ch->parent = new_parent;
    }
}

/* Return the parent allocation, or NULL if none or if ptr==NULL.
 *
 * Warning: do not use this for program logic, or I'll be sad.
 */
void *ta_get_parent(void *ptr)
{
    struct ta_header *ch = get_header(ptr);
    return ch ? ch->parent : NULL;
}

/* Allocate size bytes of memory. If ta_parent is not NULL, this is used as
 * parent allocation (if ta_parent is freed, this allocation is automatically
 * freed as well). size==0 allocates a block of size 0 (i.e. returns non-NULL).
 * Returns NULL on OOM.
 */
void *ta_alloc_size(void *ta_parent, size_t size)
{
    if (size >= MAX_ALLOC)
        return NULL;
    struct ta_header *h = malloc(sizeof(union aligned_header) + size);
    if (!h)
        return NULL;
    *h = (struct ta_header) {.size = size};
    ta_dbg_add(h);
    void *ptr = PTR_FROM_HEADER(h);
    ta_set_parent(ptr, ta_parent);
    return ptr;
}

/* Exactly the same as ta_alloc_size(), but the returned memory block is
 * initialized to 0.
 */
void *ta_zalloc_size(void *ta_parent, size_t size)
{
    if (size >= MAX_ALLOC)
        return NULL;
    struct ta_header *h = calloc(1, sizeof(union aligned_header) + size);
    if (!h)
        return NULL;
    *h = (struct ta_header) {.size = size};
    ta_dbg_add(h);
    void *ptr = PTR_FROM_HEADER(h);
    ta_set_parent(ptr, ta_parent);
    return ptr;
}

/* Reallocate the allocation given by ptr and return a new pointer. Much like
 * realloc(), the returned pointer can be different, and on OOM, NULL is
 * returned.
 *
 * size==0 is equivalent to ta_free(ptr).
 * ptr==NULL is equivalent to ta_alloc_size(ta_parent, size).
 *
 * ta_parent is used only in the ptr==NULL case.
 *
 * Returns NULL if the operation failed.
 * NULL is also returned if size==0.
 */
void *ta_realloc_size(void *ta_parent, void *ptr, size_t size)
{
    if (size >= MAX_ALLOC)
        return NULL;
    if (!size) {
        ta_free(ptr);
        return NULL;
    }
    if (!ptr)
        return ta_alloc_size(ta_parent, size);
    struct ta_header *h = get_header(ptr);
    struct ta_header *old_h = h;
    if (h->size == size)
        return ptr;
    ta_dbg_remove(h);
    h = realloc(h, sizeof(union aligned_header) + size);
    ta_dbg_add(h ? h : old_h);
    if (!h)
        return NULL;
    h->size = size;
    if (h != old_h) {
        // Relink parent
        if (h->parent)
            h->parent->child = h;
        // Relink siblings
        if (h->next)
            h->next->prev = h;
        if (h->prev)
            h->prev->next = h;
        // Relink children
        if (h->child)
            h->child->parent = h;
    }
    return PTR_FROM_HEADER(h);
}

/* Return the allocated size of ptr. This returns the size parameter of the
 * most recent ta_alloc.../ta_realloc... call.
 * If ptr==NULL, return 0.
 */
size_t ta_get_size(void *ptr)
{
    struct ta_header *h = get_header(ptr);
    return h ? h->size : 0;
}

/* Free all allocations that (recursively) have ptr as parent allocation, but
 * do not free ptr itself.
 */
void ta_free_children(void *ptr)
{
    struct ta_header *h = get_header(ptr);
    while (h && h->child)
        ta_free(PTR_FROM_HEADER(h->child));
}

/* Free the given allocation, and all of its direct and indirect children.
 */
void ta_free(void *ptr)
{
    struct ta_header *h = get_header(ptr);
    if (!h)
        return;
    if (h->destructor)
        h->destructor(ptr);
    ta_free_children(ptr);
    ta_set_parent(ptr, NULL);
    ta_dbg_remove(h);
    free(h);
}

/* Set a destructor that is to be called when the given allocation is freed.
 * (Whether the allocation is directly freed with ta_free() or indirectly by
 * freeing its parent does not matter.) There is only one destructor. If an
 * destructor was already set, it's overwritten.
 *
 * The destructor will be called with ptr as argument. The destructor can do
 * almost anything, but it must not attempt to free or realloc ptr. The
 * destructor is run before the allocation's children are freed (also, before
 * their destructors are run).
 */
void ta_set_destructor(void *ptr, void (*destructor)(void *))
{
    struct ta_header *h = get_header(ptr);
    if (h)
        h->destructor = destructor;
}

#if TA_MEMORY_DEBUGGING

#include <pthread.h>

static pthread_mutex_t ta_dbg_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool enable_leak_check; // pretty much constant
static struct ta_header leak_node;
static char allocation_is_string;

static void ta_dbg_add(struct ta_header *h)
{
    h->canary = CANARY;
    if (enable_leak_check) {
        pthread_mutex_lock(&ta_dbg_mutex);
        h->leak_next = &leak_node;
        h->leak_prev = leak_node.leak_prev;
        leak_node.leak_prev->leak_next = h;
        leak_node.leak_prev = h;
        pthread_mutex_unlock(&ta_dbg_mutex);
    }
}

static void ta_dbg_check_header(struct ta_header *h)
{
    if (h) {
        assert(h->canary == CANARY);
        if (h->parent) {
            assert(!h->prev);
            assert(h->parent->child == h);
        }
    }
}

static void ta_dbg_remove(struct ta_header *h)
{
    ta_dbg_check_header(h);
    if (h->leak_next) { // assume checking for !=NULL invariant ok without lock
        pthread_mutex_lock(&ta_dbg_mutex);
        h->leak_next->leak_prev = h->leak_prev;
        h->leak_prev->leak_next = h->leak_next;
        pthread_mutex_unlock(&ta_dbg_mutex);
        h->leak_next = h->leak_prev = NULL;
    }
    h->canary = 0;
}

static size_t get_children_size(struct ta_header *h)
{
    size_t size = 0;
    for (struct ta_header *s = h->child; s; s = s->next)
        size += s->size + get_children_size(s);
    return size;
}

static void print_leak_report(void)
{
    pthread_mutex_lock(&ta_dbg_mutex);
    if (leak_node.leak_next && leak_node.leak_next != &leak_node) {
        size_t size = 0;
        size_t num_blocks = 0;
        fprintf(stderr, "Blocks not freed:\n");
        fprintf(stderr, "  %-20s %10s %10s  %s\n",
                "Ptr", "Bytes", "C. Bytes", "Name");
        while (leak_node.leak_next != &leak_node) {
            struct ta_header *cur = leak_node.leak_next;
            // Don't list those with parent; logically, only parents are listed
            if (!cur->next) {
                size_t c_size = get_children_size(cur);
                char name[30] = {0};
                if (cur->name)
                    snprintf(name, sizeof(name), "%s", cur->name);
                if (cur->name == &allocation_is_string) {
                    snprintf(name, sizeof(name), "'%.*s'",
                             (int)cur->size, (char *)PTR_FROM_HEADER(cur));
                }
                for (int n = 0; n < sizeof(name); n++) {
                    if (name[n] && name[n] < 0x20)
                        name[n] = '.';
                }
                fprintf(stderr, "  %-20p %10zu %10zu  %s\n",
                        cur, cur->size, c_size, name);
            }
            size += cur->size;
            num_blocks += 1;
            // Unlink, and don't confuse valgrind by leaving live pointers.
            cur->leak_next->leak_prev = cur->leak_prev;
            cur->leak_prev->leak_next = cur->leak_next;
            cur->leak_next = cur->leak_prev = NULL;
        }
        fprintf(stderr, "%zu bytes in %zu blocks.\n", size, num_blocks);
    }
    pthread_mutex_unlock(&ta_dbg_mutex);
}

void ta_enable_leak_report(void)
{
    pthread_mutex_lock(&ta_dbg_mutex);
    enable_leak_check = true;
    if (!leak_node.leak_prev && !leak_node.leak_next) {
        leak_node.leak_prev = &leak_node;
        leak_node.leak_next = &leak_node;
        atexit(print_leak_report);
    }
    pthread_mutex_unlock(&ta_dbg_mutex);
}

/* Set a (static) string that will be printed if the memory allocation in ptr
 * shows up on the leak report. The string must stay valid until ptr is freed.
 * Calling it on ptr==NULL does nothing.
 * Typically used to set location info.
 * Always returns ptr (useful for chaining function calls).
 */
void *ta_dbg_set_loc(void *ptr, const char *loc)
{
    struct ta_header *h = get_header(ptr);
    if (h)
        h->name = loc;
    return ptr;
}

/* Mark the allocation as string. The leak report will print it literally.
 */
void *ta_dbg_mark_as_string(void *ptr)
{
    // Specially handled by leak report code.
    return ta_dbg_set_loc(ptr, &allocation_is_string);
}

#else

static void ta_dbg_add(struct ta_header *h){}
static void ta_dbg_check_header(struct ta_header *h){}
static void ta_dbg_remove(struct ta_header *h){}

void ta_enable_leak_report(void){}
void *ta_dbg_set_loc(void *ptr, const char *loc){return ptr;}
void *ta_dbg_mark_as_string(void *ptr){return ptr;}

#endif
