#pragma once

// This is a helper for implementing thread-safety for DR callbacks. These need
// to allocate GPU buffers on the GPU thread (e.g. OpenGL with its forced TLS),
// and the buffers also need to be freed on the GPU thread.
struct dr_helper;

struct mp_image;
struct mp_dispatch_queue;

// This MUST be called on the "target" thread (it will call pthread_self()).
// dr_helper_get_image() calls will use the dispatch queue to run get_image on
// the target thread too.
struct dr_helper *dr_helper_create(struct mp_dispatch_queue *dispatch,
            struct mp_image *(*get_image)(void *ctx, int imgfmt, int w, int h,
                                          int stride_align),
            void *get_image_ctx);

struct mp_image *dr_helper_get_image(struct dr_helper *dr, int imgfmt,
                                     int w, int h, int stride_align);
