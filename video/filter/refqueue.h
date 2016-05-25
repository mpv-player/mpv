#ifndef MP_REFQUEUE_H_
#define MP_REFQUEUE_H_

#include <stdbool.h>

// A helper for deinterlacers which require past/future reference frames.

struct mp_refqueue;

struct mp_refqueue *mp_refqueue_alloc(void);
void mp_refqueue_free(struct mp_refqueue *q);

void mp_refqueue_set_refs(struct mp_refqueue *q, int past, int future);
void mp_refqueue_flush(struct mp_refqueue *q);
void mp_refqueue_add_input(struct mp_refqueue *q, struct mp_image *img);
bool mp_refqueue_need_input(struct mp_refqueue *q);
bool mp_refqueue_has_output(struct mp_refqueue *q);
void mp_refqueue_next(struct mp_refqueue *q);
struct mp_image *mp_refqueue_get(struct mp_refqueue *q, int pos);
double mp_refqueue_get_field_pts(struct mp_refqueue *q, int field);

#endif
