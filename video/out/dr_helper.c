#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include <libavutil/buffer.h>

#include "mpv_talloc.h"
#include "misc/dispatch.h"
#include "osdep/atomic.h"
#include "video/mp_image.h"

#include "dr_helper.h"

struct dr_helper {
    pthread_t thread;
    struct mp_dispatch_queue *dispatch;
    atomic_ullong dr_in_flight;

    struct mp_image *(*get_image)(void *ctx, int imgfmt, int w, int h,
                                  int stride_align);
    void *get_image_ctx;
};

static void dr_helper_destroy(void *ptr)
{
    struct dr_helper *dr = ptr;

    // All references must have been freed on destruction, or we'll have
    // dangling pointers.
    assert(atomic_load(&dr->dr_in_flight) == 0);
}

struct dr_helper *dr_helper_create(struct mp_dispatch_queue *dispatch,
            struct mp_image *(*get_image)(void *ctx, int imgfmt, int w, int h,
                                          int stride_align),
            void *get_image_ctx)
{
    struct dr_helper *dr = talloc_ptrtype(NULL, dr);
    talloc_set_destructor(dr, dr_helper_destroy);
    *dr = (struct dr_helper){
        .thread = pthread_self(),
        .dispatch = dispatch,
        .dr_in_flight = ATOMIC_VAR_INIT(0),
        .get_image = get_image,
        .get_image_ctx = get_image_ctx,
    };
    return dr;
}

struct free_dr_context {
    struct dr_helper *dr;
    AVBufferRef *ref;
};

static void dr_thread_free(void *ptr)
{
    struct free_dr_context *ctx = ptr;

    unsigned long long v = atomic_fetch_add(&ctx->dr->dr_in_flight, -1);
    assert(v); // value before sub is 0 - unexpected underflow.

    av_buffer_unref(&ctx->ref);
    talloc_free(ctx);
}

static void free_dr_buffer_on_dr_thread(void *opaque, uint8_t *data)
{
    struct free_dr_context *ctx = opaque;

    // The image could be unreffed even on the DR thread. In practice, this
    // matters most on DR destruction.
    if (pthread_equal(ctx->dr->thread, pthread_self())) {
        dr_thread_free(ctx);
    } else {
        mp_dispatch_run(ctx->dr->dispatch, dr_thread_free, ctx);
    }
}

struct get_image_cmd {
    struct dr_helper *dr;
    int imgfmt, w, h, stride_align;
    struct mp_image *res;
};

static void sync_get_image(void *ptr)
{
    struct get_image_cmd *cmd = ptr;
    struct dr_helper *dr = cmd->dr;

    cmd->res = dr->get_image(dr->get_image_ctx, cmd->imgfmt, cmd->w, cmd->h,
                             cmd->stride_align);
    if (!cmd->res)
        return;

    // We require exactly 1 AVBufferRef.
    assert(cmd->res->bufs[0]);
    assert(!cmd->res->bufs[1]);

    // Apply some magic to get it free'd on the DR thread as well. For this to
    // work, we create a dummy-ref that aliases the original ref, which is why
    // the original ref must be writable in the first place. (A newly allocated
    // image should be always writable of course.)
    assert(mp_image_is_writeable(cmd->res));

    struct free_dr_context *ctx = talloc_zero(NULL, struct free_dr_context);
    *ctx = (struct free_dr_context){
        .dr = dr,
        .ref = cmd->res->bufs[0],
    };

    AVBufferRef *new_ref = av_buffer_create(ctx->ref->data, ctx->ref->size,
                                            free_dr_buffer_on_dr_thread, ctx, 0);
    if (!new_ref)
        abort(); // tiny malloc OOM

    cmd->res->bufs[0] = new_ref;

    atomic_fetch_add(&dr->dr_in_flight, 1);
}

struct mp_image *dr_helper_get_image(struct dr_helper *dr, int imgfmt,
                                     int w, int h, int stride_align)
{
    struct get_image_cmd cmd = {
        .dr = dr,
        .imgfmt = imgfmt, .w = w, .h = h, .stride_align = stride_align,
    };
    mp_dispatch_run(dr->dispatch, sync_get_image, &cmd);
    return cmd.res;
}
