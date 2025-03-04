#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

#include <libavutil/buffer.h>

#include "misc/dispatch.h"
#include "mpv_talloc.h"
#include "osdep/threads.h"
#include "video/mp_image.h"

#include "dr_helper.h"

struct dr_helper {
    mp_mutex thread_lock;
    mp_thread_id thread_id;
    bool thread_valid; // (POSIX defines no "unset" mp_thread value yet)

    struct mp_dispatch_queue *dispatch;
    atomic_ullong dr_in_flight;

    struct mp_image *(*get_image)(void *ctx, int imgfmt, int w, int h,
                                  int stride_align, int flags);
    void *get_image_ctx;
};

static void dr_helper_destroy(void *ptr)
{
    struct dr_helper *dr = ptr;

    // All references must have been freed on destruction, or we'll have
    // dangling pointers.
    mp_assert(atomic_load(&dr->dr_in_flight) == 0);

    mp_mutex_destroy(&dr->thread_lock);
}

struct dr_helper *dr_helper_create(struct mp_dispatch_queue *dispatch,
            struct mp_image *(*get_image)(void *ctx, int imgfmt, int w, int h,
                                          int stride_align, int flags),
            void *get_image_ctx)
{
    struct dr_helper *dr = talloc_ptrtype(NULL, dr);
    talloc_set_destructor(dr, dr_helper_destroy);
    *dr = (struct dr_helper){
        .dispatch = dispatch,
        .dr_in_flight = 0,
        .get_image = get_image,
        .get_image_ctx = get_image_ctx,
    };
    mp_mutex_init(&dr->thread_lock);
    return dr;
}

void dr_helper_acquire_thread(struct dr_helper *dr)
{
    mp_mutex_lock(&dr->thread_lock);
    mp_assert(!dr->thread_valid); // fails on API user errors
    dr->thread_valid = true;
    dr->thread_id = mp_thread_current_id();
    mp_mutex_unlock(&dr->thread_lock);
}

void dr_helper_release_thread(struct dr_helper *dr)
{
    mp_mutex_lock(&dr->thread_lock);
    // Fails on API user errors.
    mp_assert(dr->thread_valid);
    mp_assert(mp_thread_id_equal(dr->thread_id, mp_thread_current_id()));
    dr->thread_valid = false;
    mp_mutex_unlock(&dr->thread_lock);
}

struct free_dr_context {
    struct dr_helper *dr;
    AVBufferRef *ref;
};

static void dr_thread_free(void *ptr)
{
    struct free_dr_context *ctx = ptr;

    unsigned long long v = atomic_fetch_add(&ctx->dr->dr_in_flight, -1);
    mp_assert(v); // value before sub is 0 - unexpected underflow.

    av_buffer_unref(&ctx->ref);
    talloc_free(ctx);
}

static void free_dr_buffer_on_dr_thread(void *opaque, uint8_t *data)
{
    struct free_dr_context *ctx = opaque;
    struct dr_helper *dr = ctx->dr;

    mp_mutex_lock(&dr->thread_lock);
    bool on_this_thread =
        dr->thread_valid && mp_thread_id_equal(ctx->dr->thread_id, mp_thread_current_id());
    mp_mutex_unlock(&dr->thread_lock);

    // The image could be unreffed even on the DR thread. In practice, this
    // matters most on DR destruction.
    if (on_this_thread) {
        dr_thread_free(ctx);
    } else {
        mp_dispatch_enqueue(dr->dispatch, dr_thread_free, ctx);
    }
}

struct get_image_cmd {
    struct dr_helper *dr;
    int imgfmt, w, h, stride_align, flags;
    struct mp_image *res;
};

static void sync_get_image(void *ptr)
{
    struct get_image_cmd *cmd = ptr;
    struct dr_helper *dr = cmd->dr;

    cmd->res = dr->get_image(dr->get_image_ctx, cmd->imgfmt, cmd->w, cmd->h,
                             cmd->stride_align, cmd->flags);
    if (!cmd->res)
        return;

    // We require exactly 1 AVBufferRef.
    mp_assert(cmd->res->bufs[0]);
    mp_assert(!cmd->res->bufs[1]);

    // Apply some magic to get it free'd on the DR thread as well. For this to
    // work, we create a dummy-ref that aliases the original ref, which is why
    // the original ref must be writable in the first place. (A newly allocated
    // image should be always writable of course.)
    mp_assert(mp_image_is_writeable(cmd->res));

    struct free_dr_context *ctx = talloc_zero(NULL, struct free_dr_context);
    *ctx = (struct free_dr_context){
        .dr = dr,
        .ref = cmd->res->bufs[0],
    };

    AVBufferRef *new_ref = av_buffer_create(ctx->ref->data, ctx->ref->size,
                                            free_dr_buffer_on_dr_thread, ctx, 0);
    MP_HANDLE_OOM(new_ref);

    cmd->res->bufs[0] = new_ref;

    atomic_fetch_add(&dr->dr_in_flight, 1);
}

struct mp_image *dr_helper_get_image(struct dr_helper *dr, int imgfmt,
                                     int w, int h, int stride_align, int flags)
{
    struct get_image_cmd cmd = {
        .dr = dr,
        .imgfmt = imgfmt,
        .w = w, .h = h,
        .stride_align = stride_align,
        .flags = flags,
    };
    mp_dispatch_run(dr->dispatch, sync_get_image, &cmd);
    return cmd.res;
}
