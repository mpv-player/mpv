#ifndef MPV_MP_IMAGE_POOL_H
#define MPV_MP_IMAGE_POOL_H

#include <stdbool.h>

struct mp_image_pool;

struct mp_image_pool *mp_image_pool_new(int max_count);
struct mp_image *mp_image_pool_get(struct mp_image_pool *pool, int fmt,
                                   int w, int h);
void mp_image_pool_clear(struct mp_image_pool *pool);

void mp_image_pool_set_lru(struct mp_image_pool *pool);

struct mp_image *mp_image_pool_get_no_alloc(struct mp_image_pool *pool, int fmt,
                                            int w, int h);

typedef struct mp_image *(*mp_image_allocator)(void *data, int fmt, int w, int h);
void mp_image_pool_set_allocator(struct mp_image_pool *pool,
                                 mp_image_allocator cb, void  *cb_data);

struct mp_image *mp_image_pool_new_copy(struct mp_image_pool *pool,
                                        struct mp_image *img);
bool mp_image_pool_make_writeable(struct mp_image_pool *pool,
                                  struct mp_image *img);

#endif
