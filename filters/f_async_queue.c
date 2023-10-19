#include <limits.h>
#include <pthread.h>

#include "audio/aframe.h"
#include "common/common.h"
#include "common/msg.h"
#include <stdatomic.h>

#include "f_async_queue.h"
#include "filter_internal.h"

struct mp_async_queue {
    // This is just a wrapper, so the API user can talloc_free() it, instead of
    // having to call a special unref function.
    struct async_queue *q;
};

struct async_queue {
    _Atomic uint64_t refcount;

    pthread_mutex_t lock;

    // -- protected by lock
    struct mp_async_queue_config cfg;
    bool active; // queue was resumed; consumer may request frames
    bool reading; // data flow: reading => consumer has requested frames
    int64_t samples_size; // queue size in the cfg.sample_unit
    size_t byte_size; // queue size in bytes (using approx. frame sizes)
    int num_frames;
    struct mp_frame *frames;
    int eof_count; // number of MP_FRAME_EOF in frames[], for draining
    struct mp_filter *conn[2]; // filters: in (0), out (1)
};

static void reset_queue(struct async_queue *q)
{
    pthread_mutex_lock(&q->lock);
    q->active = q->reading = false;
    for (int n = 0; n < q->num_frames; n++)
        mp_frame_unref(&q->frames[n]);
    q->num_frames = 0;
    q->eof_count = 0;
    q->samples_size = 0;
    q->byte_size = 0;
    for (int n = 0; n < 2; n++) {
        if (q->conn[n])
            mp_filter_wakeup(q->conn[n]);
    }
    pthread_mutex_unlock(&q->lock);
}

static void unref_queue(struct async_queue *q)
{
    if (!q)
        return;
    int count = atomic_fetch_add(&q->refcount, -1) - 1;
    assert(count >= 0);
    if (count == 0) {
        reset_queue(q);
        pthread_mutex_destroy(&q->lock);
        talloc_free(q);
    }
}

static void on_free_queue(void *p)
{
    struct mp_async_queue *q = p;
    unref_queue(q->q);
}

struct mp_async_queue *mp_async_queue_create(void)
{
    struct mp_async_queue *r = talloc_zero(NULL, struct mp_async_queue);
    r->q = talloc_zero(NULL, struct async_queue);
    *r->q = (struct async_queue){
        .refcount = ATOMIC_VAR_INIT(1),
    };
    pthread_mutex_init(&r->q->lock, NULL);
    talloc_set_destructor(r, on_free_queue);
    mp_async_queue_set_config(r, (struct mp_async_queue_config){0});
    return r;
}

static int64_t frame_get_samples(struct async_queue *q, struct mp_frame frame)
{
    int64_t res = 1;
    if (frame.type == MP_FRAME_AUDIO && q->cfg.sample_unit == AQUEUE_UNIT_SAMPLES) {
        struct mp_aframe *aframe = frame.data;
        res = mp_aframe_get_size(aframe);
    }
    if (mp_frame_is_signaling(frame))
        return 0;
    return res;
}

static bool is_full(struct async_queue *q)
{
    if (q->samples_size >= q->cfg.max_samples || q->byte_size >= q->cfg.max_bytes)
        return true;
    if (q->num_frames >= 2 && q->cfg.max_duration > 0) {
        double pts1 = mp_frame_get_pts(q->frames[q->num_frames - 1]);
        double pts2 = mp_frame_get_pts(q->frames[0]);
        if (pts1 != MP_NOPTS_VALUE && pts2 != MP_NOPTS_VALUE &&
            pts2 - pts1 >= q->cfg.max_duration)
            return true;
    }
    return false;
}

// Add or remove a frame from the accounted queue size.
//  dir==1: add, dir==-1: remove
static void account_frame(struct async_queue *q, struct mp_frame frame,
                          int dir)
{
    assert(dir == 1 || dir == -1);

    q->samples_size += dir * frame_get_samples(q, frame);
    q->byte_size += dir * mp_frame_approx_size(frame);

    if (frame.type == MP_FRAME_EOF)
        q->eof_count += dir;
}

static void recompute_sizes(struct async_queue *q)
{
    q->eof_count = 0;
    q->samples_size = 0;
    q->byte_size = 0;
    for (int n = 0; n < q->num_frames; n++)
        account_frame(q, q->frames[n], 1);
}

void mp_async_queue_set_config(struct mp_async_queue *queue,
                               struct mp_async_queue_config cfg)
{
    struct async_queue *q = queue->q;

    cfg.max_bytes = MPCLAMP(cfg.max_bytes, 1, (size_t)-1 / 2);

    assert(cfg.sample_unit == AQUEUE_UNIT_FRAME ||
           cfg.sample_unit == AQUEUE_UNIT_SAMPLES);

    cfg.max_samples = MPMAX(cfg.max_samples, 1);

    pthread_mutex_lock(&q->lock);
    bool recompute = q->cfg.sample_unit != cfg.sample_unit;
    q->cfg = cfg;
    if (recompute)
        recompute_sizes(q);
    pthread_mutex_unlock(&q->lock);
}

void mp_async_queue_reset(struct mp_async_queue *queue)
{
    reset_queue(queue->q);
}

bool mp_async_queue_is_active(struct mp_async_queue *queue)
{
    struct async_queue *q = queue->q;
    pthread_mutex_lock(&q->lock);
    bool res = q->active;
    pthread_mutex_unlock(&q->lock);
    return res;
}

bool mp_async_queue_is_full(struct mp_async_queue *queue)
{
    struct async_queue *q = queue->q;
    pthread_mutex_lock(&q->lock);
    bool res = is_full(q);
    pthread_mutex_unlock(&q->lock);
    return res;
}

void mp_async_queue_resume(struct mp_async_queue *queue)
{
    struct async_queue *q = queue->q;

    pthread_mutex_lock(&q->lock);
    if (!q->active) {
        q->active = true;
        // Possibly make the consumer request new frames.
        if (q->conn[1])
            mp_filter_wakeup(q->conn[1]);
    }
    pthread_mutex_unlock(&q->lock);
}

void mp_async_queue_resume_reading(struct mp_async_queue *queue)
{
    struct async_queue *q = queue->q;

    pthread_mutex_lock(&q->lock);
    if (!q->active || !q->reading) {
        q->active = true;
        q->reading = true;
        // Possibly start producer/consumer.
        for (int n = 0; n < 2; n++) {
            if (q->conn[n])
                mp_filter_wakeup(q->conn[n]);
        }
    }
    pthread_mutex_unlock(&q->lock);
}

int64_t mp_async_queue_get_samples(struct mp_async_queue *queue)
{
    struct async_queue *q = queue->q;
    pthread_mutex_lock(&q->lock);
    int64_t res = q->samples_size;
    pthread_mutex_unlock(&q->lock);
    return res;
}

int mp_async_queue_get_frames(struct mp_async_queue *queue)
{
    struct async_queue *q = queue->q;
    pthread_mutex_lock(&q->lock);
    int res = q->num_frames;
    pthread_mutex_unlock(&q->lock);
    return res;
}

struct priv {
    struct async_queue *q;
    struct mp_filter *notify;
};

static void destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;
    struct async_queue *q = p->q;

    pthread_mutex_lock(&q->lock);
    for (int n = 0; n < 2; n++) {
        if (q->conn[n] == f)
            q->conn[n] = NULL;
    }
    pthread_mutex_unlock(&q->lock);

    unref_queue(q);
}

static void process_in(struct mp_filter *f)
{
    struct priv *p = f->priv;
    struct async_queue *q = p->q;
    assert(q->conn[0] == f);

    pthread_mutex_lock(&q->lock);
    if (!q->reading) {
        // mp_async_queue_reset()/reset_queue() is usually called asynchronously,
        // so we might have requested a frame earlier, and now can't use it.
        // Discard it; the expectation is that this is a benign logical race
        // condition, and the filter graph will be reset anyway.
        if (mp_pin_out_has_data(f->ppins[0])) {
            struct mp_frame frame = mp_pin_out_read(f->ppins[0]);
            mp_frame_unref(&frame);
            MP_DBG(f, "discarding frame due to async reset\n");
        }
    } else if (!is_full(q) && mp_pin_out_request_data(f->ppins[0])) {
        struct mp_frame frame = mp_pin_out_read(f->ppins[0]);
        account_frame(q, frame, 1);
        MP_TARRAY_INSERT_AT(q, q->frames, q->num_frames, 0, frame);
        // Notify reader that we have new frames.
        if (q->conn[1])
            mp_filter_wakeup(q->conn[1]);
        bool full = is_full(q);
        if (!full)
            mp_pin_out_request_data_next(f->ppins[0]);
        if (p->notify && full)
            mp_filter_wakeup(p->notify);
    }
    if (p->notify && !q->num_frames)
        mp_filter_wakeup(p->notify);
    pthread_mutex_unlock(&q->lock);
}

static void process_out(struct mp_filter *f)
{
    struct priv *p = f->priv;
    struct async_queue *q = p->q;
    assert(q->conn[1] == f);

    if (!mp_pin_in_needs_data(f->ppins[0]))
        return;

    pthread_mutex_lock(&q->lock);
    if (q->active && !q->reading) {
        q->reading = true;
        mp_filter_wakeup(q->conn[0]);
    }
    if (q->active && q->num_frames) {
        struct mp_frame frame = q->frames[q->num_frames - 1];
        q->num_frames -= 1;
        account_frame(q, frame, -1);
        assert(q->samples_size >= 0);
        mp_pin_in_write(f->ppins[0], frame);
        // Notify writer that we need new frames.
        if (q->conn[0])
            mp_filter_wakeup(q->conn[0]);
    }
    pthread_mutex_unlock(&q->lock);
}

static void reset(struct mp_filter *f)
{
    struct priv *p = f->priv;
    struct async_queue *q = p->q;

    pthread_mutex_lock(&q->lock);
    // If the queue is in reading state, it is logical that it should request
    // input immediately.
    if (mp_pin_get_dir(f->pins[0]) == MP_PIN_IN && q->reading)
        mp_filter_wakeup(f);
    pthread_mutex_unlock(&q->lock);
}

// producer
static const struct mp_filter_info info_in = {
    .name = "async_queue_in",
    .priv_size = sizeof(struct priv),
    .destroy = destroy,
    .process = process_in,
    .reset = reset,
};

// consumer
static const struct mp_filter_info info_out = {
    .name = "async_queue_out",
    .priv_size = sizeof(struct priv),
    .destroy = destroy,
    .process = process_out,
};

void mp_async_queue_set_notifier(struct mp_filter *f, struct mp_filter *notify)
{
    assert(mp_filter_get_info(f) == &info_in);
    struct priv *p = f->priv;
    if (p->notify != notify) {
        p->notify = notify;
        if (notify)
            mp_filter_wakeup(notify);
    }
}

struct mp_filter *mp_async_queue_create_filter(struct mp_filter *parent,
                                               enum mp_pin_dir dir,
                                               struct mp_async_queue *queue)
{
    bool is_in = dir == MP_PIN_IN;
    assert(queue);

    struct mp_filter *f = mp_filter_create(parent, is_in ? &info_in : &info_out);
    if (!f)
        return NULL;

    struct priv *p = f->priv;

    struct async_queue *q = queue->q;

    mp_filter_add_pin(f, dir, is_in ? "in" : "out");

    atomic_fetch_add(&q->refcount, 1);
    p->q = q;

    pthread_mutex_lock(&q->lock);
    int slot = is_in ? 0 : 1;
    assert(!q->conn[slot]); // fails if already connected on this end
    q->conn[slot] = f;
    pthread_mutex_unlock(&q->lock);

    return f;
}
