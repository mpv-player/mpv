#pragma once

// This is a helper for implementing thread-safety for DR callbacks. These need
// to allocate GPU buffers on the GPU thread (e.g. OpenGL with its forced TLS),
// and the buffers also need to be freed on the GPU thread.
// This is not a helpful "Dr.", rather it represents Satan in form of C code.
struct dr_helper;

struct mp_image;
struct mp_dispatch_queue;

// dr_helper_get_image() calls will use the dispatch queue to run get_image on
// a target thread, which processes the dispatch queue.
// Note: the dispatch queue must process outstanding async. work before the
//       dr_helper instance can be destroyed.
struct dr_helper *dr_helper_create(struct mp_dispatch_queue *dispatch,
            struct mp_image *(*get_image)(void *ctx, int imgfmt, int w, int h,
                                          int stride_align),
            void *get_image_ctx);

// Make DR release calls (freeing images) reentrant if they are called on this
// (pthread_self()) thread. That means any free call will directly release the
// image as allocated with get_image().
// Only 1 thread can use this at a time. Note that it would make no sense to
// call this on more than 1 thread, as get_image is assumed not thread-safe.
void dr_helper_acquire_thread(struct dr_helper *dr);

// This _must_ be called on the same thread as dr_helper_acquire_thread() was
// called. Every release call must be paired with an acquire call.
void dr_helper_release_thread(struct dr_helper *dr);

// Allocate an image by running the get_image callback on the target thread.
// Always blocks on dispatch queue processing. This implies there is no way to
// allocate a DR'ed image on the render thread (at least not in a way which
// actually works if you want foreign threads to be able to free them).
struct mp_image *dr_helper_get_image(struct dr_helper *dr, int imgfmt,
                                     int w, int h, int stride_align);
