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

#include <assert.h>

#include "talloc.h"

#include "core/mp_common.h"
#include "video/mp_image.h"

#include "mp_image_pool.h"

struct pool_trampoline {
    struct mp_image_pool *pool;
    int refcount;
};

static int image_pool_destructor(void *ptr)
{
    struct mp_image_pool *pool = ptr;
    mp_image_pool_clear(pool);
    pool->trampoline->pool = NULL;
    if (!pool->trampoline->refcount)
        talloc_free(pool->trampoline);
    return 0;
}

struct mp_image_pool *mp_image_pool_new(int max_count)
{
    struct mp_image_pool *pool = talloc_ptrtype(NULL, pool);
    talloc_set_destructor(pool, image_pool_destructor);
    *pool = (struct mp_image_pool) {
        .max_count = max_count,
        .trampoline = talloc_struct(NULL, struct pool_trampoline, {
            .pool = pool,
        }),
    };
    return pool;
}

void mp_image_pool_clear(struct mp_image_pool *pool)
{
    for (int n = 0; n < pool->num_images; n++) {
        struct mp_image *img = pool->images[n];
        if (img->priv) {
            // Image data is being used - detach from the pool, and make it
            // free itself when pool_free_image() is called
            img->priv = talloc_struct(NULL, struct pool_trampoline, {
                .refcount = 1,
            });
        } else {
            talloc_free(img);
        }
    }
    pool->num_images = 0;
    pool->trampoline->refcount = 0;
}

static void pool_free_image(void *ptr)
{
    struct mp_image *img = ptr;
    struct pool_trampoline *trampoline = img->priv;
    trampoline->refcount--;
    img->priv = NULL;
    if (!trampoline->pool) {
        // pool was free'd while image reference was still held
        if (trampoline->refcount == 0)
            talloc_free(trampoline);
        talloc_free(img);
    }
}

// Return a new image of given format/size. The only difference to
// mp_image_alloc() is that there is a transparent mechanism to recycle image
// data allocations through this pool.
// The image can be free'd with talloc_free().
struct mp_image *mp_image_pool_get(struct mp_image_pool *pool, unsigned int fmt,
                                   int w, int h)
{
    struct mp_image *new = NULL;
    for (int n = 0; n < pool->num_images; n++) {
        struct mp_image *img = pool->images[n];
        if (!img->priv && img->imgfmt == fmt && img->w == w && img->h == h) {
            new = img;
            break;
        }
    }
    if (!new) {
        if (pool->num_images >= pool->max_count)
            mp_image_pool_clear(pool);
        new = mp_image_alloc(fmt, w, h);
        MP_TARRAY_APPEND(pool, pool->images, pool->num_images, new);
    }
    pool->trampoline->refcount++;
    new->priv = pool->trampoline;
    return mp_image_new_custom_ref(new, new, pool_free_image);
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
