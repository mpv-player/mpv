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
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#include "talloc.h"

#include "mpvcore/mp_common.h"
#include "video/mp_image.h"

#include "mp_image_pool.h"

#if HAVE_PTHREADS
#include <pthread.h>
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
#define pool_lock() pthread_mutex_lock(&pool_mutex)
#define pool_unlock() pthread_mutex_unlock(&pool_mutex)
#else
#define pool_lock() 0
#define pool_unlock() 0
#endif

// Thread-safety: the pool itself is not thread-safe, but pool-allocated images
// can be referenced and unreferenced from other threads. (As long as compiled
// with pthreads, and the image destructors are thread-safe.)

struct mp_image_pool {
    int max_count;

    struct mp_image **images;
    int num_images;
};

// Used to gracefully handle the case when the pool is freed while image
// references allocated from the image pool are still held by someone.
struct image_flags {
    // If both of these are false, the image must be freed.
    bool referenced;            // outside mp_image reference exists
    bool pool_alive;            // the mp_image_pool references this
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

// Return a new image of given format/size. The only difference to
// mp_image_alloc() is that there is a transparent mechanism to recycle image
// data allocations through this pool.
// The image can be free'd with talloc_free().
struct mp_image *mp_image_pool_get(struct mp_image_pool *pool, unsigned int fmt,
                                   int w, int h)
{
    struct mp_image *new = NULL;

    pool_lock();
    for (int n = 0; n < pool->num_images; n++) {
        struct mp_image *img = pool->images[n];
        struct image_flags *it = img->priv;
        assert(it->pool_alive);
        if (!it->referenced) {
            if (img->imgfmt == fmt && img->w == w && img->h == h) {
                new = img;
                break;
            }
        }
    }
    pool_unlock();

    if (!new) {
        if (pool->num_images >= pool->max_count)
            mp_image_pool_clear(pool);
        new = mp_image_alloc(fmt, w, h);
        struct image_flags *it = talloc_ptrtype(new, it);
        *it = (struct image_flags) { .pool_alive = true };
        new->priv = it;
        MP_TARRAY_APPEND(pool, pool->images, pool->num_images, new);
    }

    struct image_flags *it = new->priv;
    assert(!it->referenced && it->pool_alive);
    it->referenced = true;
    return mp_image_new_custom_ref(new, new, unref_image);
}

// Like mp_image_new_copy(), but allocate the image out of the pool.
struct mp_image *mp_image_pool_new_copy(struct mp_image_pool *pool,
                                        struct mp_image *img)
{
    struct mp_image *new = mp_image_pool_get(pool, img->imgfmt, img->w, img->h);
    mp_image_copy(new, img);
    mp_image_copy_attributes(new, img);
    return new;
}

// Like mp_image_make_writeable(), but if a copy has to be made, allocate it
// out of the pool.
void mp_image_pool_make_writeable(struct mp_image_pool *pool,
                                  struct mp_image *img)
{
    if (mp_image_is_writeable(img))
        return;
    mp_image_steal_data(img, mp_image_pool_new_copy(pool, img));
    assert(mp_image_is_writeable(img));
}
