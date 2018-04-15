#ifndef MP_DISPATCH_H_
#define MP_DISPATCH_H_

#include <stdint.h>

typedef void (*mp_dispatch_fn)(void *data);
struct mp_dispatch_queue;

struct mp_dispatch_queue *mp_dispatch_create(void *talloc_parent);
void mp_dispatch_set_wakeup_fn(struct mp_dispatch_queue *queue,
                               void (*wakeup_fn)(void *wakeup_ctx),
                               void *wakeup_ctx);
void mp_dispatch_enqueue(struct mp_dispatch_queue *queue,
                         mp_dispatch_fn fn, void *fn_data);
void mp_dispatch_enqueue_autofree(struct mp_dispatch_queue *queue,
                                  mp_dispatch_fn fn, void *fn_data);
void mp_dispatch_enqueue_notify(struct mp_dispatch_queue *queue,
                                mp_dispatch_fn fn, void *fn_data);
void mp_dispatch_cancel_fn(struct mp_dispatch_queue *queue,
                           mp_dispatch_fn fn, void *fn_data);
void mp_dispatch_run(struct mp_dispatch_queue *queue,
                     mp_dispatch_fn fn, void *fn_data);
void mp_dispatch_queue_process(struct mp_dispatch_queue *queue, double timeout);
void mp_dispatch_interrupt(struct mp_dispatch_queue *queue);
void mp_dispatch_adjust_timeout(struct mp_dispatch_queue *queue, int64_t until);
void mp_dispatch_lock(struct mp_dispatch_queue *queue);
void mp_dispatch_unlock(struct mp_dispatch_queue *queue);

#endif
