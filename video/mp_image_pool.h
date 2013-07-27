#ifndef MPV_MP_IMAGE_POOL_H
#define MPV_MP_IMAGE_POOL_H

struct mp_image_pool;

struct mp_image_pool *mp_image_pool_new(int max_count);
struct mp_image *mp_image_pool_get(struct mp_image_pool *pool, unsigned int fmt,
                                   int w, int h);
void mp_image_pool_clear(struct mp_image_pool *pool);

struct mp_image *mp_image_pool_new_copy(struct mp_image_pool *pool,
                                        struct mp_image *img);
void mp_image_pool_make_writeable(struct mp_image_pool *pool,
                                  struct mp_image *img);

#endif
