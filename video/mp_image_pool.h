#ifndef MPV_MP_IMAGE_POOL_H
#define MPV_MP_IMAGE_POOL_H

#include <stdbool.h>

struct mp_image_pool;

struct mp_image_pool *mp_image_pool_new(void *tparent);
struct mp_image *mp_image_pool_get(struct mp_image_pool *pool, int fmt,
                                   int w, int h);
// the reference to "new" is transferred to the pool
void mp_image_pool_add(struct mp_image_pool *pool, struct mp_image *new);
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

struct mp_image *mp_image_hw_download(struct mp_image *img,
                                      struct mp_image_pool *swpool);

int mp_image_hw_download_get_sw_format(struct mp_image *img);

bool mp_image_hw_upload(struct mp_image *hw_img, struct mp_image *src);

struct AVBufferRef;
bool mp_update_av_hw_frames_pool(struct AVBufferRef **hw_frames_ctx,
                                 struct AVBufferRef *hw_device_ctx,
                                 int imgfmt, int sw_imgfmt, int w, int h);

struct mp_image *mp_av_pool_image_hw_upload(struct AVBufferRef *hw_frames_ctx,
                                            struct mp_image *src);

#endif
