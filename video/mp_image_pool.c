/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>

#include "talloc.h"

#include "common/common.h"
#include "video/mp_image.h"

#include "mp_image_pool.h"

static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
#define pool_lock() pthread_mutex_lock(&pool_mutex)
#define pool_unlock() pthread_mutex_unlock(&pool_mutex)

// Thread-safety: the pool itself is not thread-safe, but pool-allocated images
// can be referenced and unreferenced from other threads. (As long as the image
// destructors are thread-safe.)

struct mp_image_pool {
    int max_count;

    struct mp_image **images;
    int num_images;

    mp_image_allocator allocator;
    void *allocator_ctx;

    bool use_lru;
    unsigned int lru_counter;
};

// Used to gracefully handle the case when the pool is freed while image
// references allocated from the image pool are still held by someone.
struct image_flags {
    // If both of these are false, the image must be freed.
    bool referenced;            // outside mp_image reference exists
    bool pool_alive;            // the mp_image_pool references this
    unsigned int order;         // for LRU allocation (basically a timestamp)
};

static void image_pool_destructor(void *ptr)
{
    struct mp_image_pool *pool = ptr;
    mp_image_pool_clear(pool);
}

struct mp_image_pool *mp_image_pool_new(int max_count)
{
    struct mp_image_pool *pool = talloc_ptrtype(NULL, pool);
    talloc_set_destructor(pool, image_pool_destructor);
    *pool = (struct mp_image_pool) {
        .max_count = max_count,
    };
    return pool;
}

void mp_image_pool_clear(struct mp_image_pool *pool)
{
    for (int n = 0; n < pool->num_images; n++) {
        struct mp_image *img = pool->images[n];
        struct image_flags *it = img->priv;
        bool referenced;
        pool_lock();
        assert(it->pool_alive);
        it->pool_alive = false;
        referenced = it->referenced;
        pool_unlock();
        if (!referenced)
            talloc_free(img);
    }
    pool->num_images = 0;
}

// This is the only function that is allowed to run in a different thread.
// (Consider passing an image to another thread, which frees it.)
static void unref_image(void *ptr)
{
    struct mp_image *img = ptr;
    struct image_flags *it = img->priv;
    bool alive;
    pool_lock();
    assert(it->referenced);
    it->referenced = false;
    alive = it->pool_alive;
    pool_unlock();
    if (!alive)
        talloc_free(img);
}

// Return a new image of given format/size. Unlike mp_image_pool_get(), this
// returns NULL if there is no free image of this format/size.
struct mp_image *mp_image_pool_get_no_alloc(struct mp_image_pool *pool, int fmt,
                                            int w, int h)
{
    struct mp_image *new = NULL;
    pool_lock();
    for (int n = 0; n < pool->num_images; n++) {
        struct mp_image *img = pool->images[n];
        struct image_flags *img_it = img->priv;
        assert(img_it->pool_alive);
        if (!img_it->referenced) {
            if (img->imgfmt == fmt && img->w == w && img->h == h) {
                if (pool->use_lru) {
                    struct image_flags *new_it = new ? new->priv : NULL;
                    if (!new_it || new_it->order > img_it->order)
                        new = img;
                } else {
                    new = img;
                    break;
                }
            }
        }
    }
    pool_unlock();
    if (!new)
        return NULL;
    struct image_flags *it = new->priv;
    assert(!it->referenced && it->pool_alive);
    it->referenced = true;
    it->order = ++pool->lru_counter;
    return mp_image_new_custom_ref(new, new, unref_image);
}

// Return a new image of given format/size. The only difference to
// mp_image_alloc() is that there is a transparent mechanism to recycle image
// data allocations through this pool.
// If pool==NULL, mp_image_alloc() is called (for convenience).
// The image can be free'd with talloc_free().
// Returns NULL on OOM.
struct mp_image *mp_image_pool_get(struct mp_image_pool *pool, int fmt,
                                   int w, int h)
{
    if (!pool)
        return mp_image_alloc(fmt, w, h);
    struct mp_image *new = mp_image_pool_get_no_alloc(pool, fmt, w, h);
    if (!new) {
        if (pool->num_images >= pool->max_count)
            mp_image_pool_clear(pool);
        if (pool->allocator) {
            new = pool->allocator(pool->allocator_ctx, fmt, w, h);
        } else {
            new = mp_image_alloc(fmt, w, h);
        }
        if (!new)
            return NULL;
        struct image_flags *it = talloc_ptrtype(new, it);
        *it = (struct image_flags) { .pool_alive = true };
        new->priv = it;
        MP_TARRAY_APPEND(pool, pool->images, pool->num_images, new);
        new = mp_image_pool_get_no_alloc(pool, fmt, w, h);
    }
    return new;
}

// Like mp_image_new_copy(), but allocate the image out of the pool.
// If pool==NULL, a plain copy is made (for convenience).
// Returns NULL on OOM.
struct mp_image *mp_image_pool_new_copy(struct mp_image_pool *pool,
                                        struct mp_image *img)
{
    struct mp_image *new = mp_image_pool_get(pool, img->imgfmt, img->w, img->h);
    if (new) {
        mp_image_copy(new, img);
        mp_image_copy_attributes(new, img);
    }
    return new;
}

// Like mp_image_make_writeable(), but if a copy has to be made, allocate it
// out of the pool.
// If pool==NULL, mp_image_make_writeable() is called (for convenience).
// Returns false on failure (see mp_image_make_writeable()).
bool mp_image_pool_make_writeable(struct mp_image_pool *pool,
                                  struct mp_image *img)
{
    if (mp_image_is_writeable(img))
        return true;
    struct mp_image *new = mp_image_pool_new_copy(pool, img);
    if (!new)
        return false;
    mp_image_steal_data(img, new);
    assert(mp_image_is_writeable(img));
    return true;
}

void mp_image_pool_set_allocator(struct mp_image_pool *pool,
                                 mp_image_allocator cb, void  *cb_data)
{
    pool->allocator = cb;
    pool->allocator_ctx = cb_data;
}

// Put into LRU mode. (Likely better for hwaccel surfaces, but worse for memory.)
void mp_image_pool_set_lru(struct mp_image_pool *pool)
{
    pool->use_lru = true;
}
