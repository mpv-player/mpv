#include "audio/aframe.h"
#include "video/mp_image.h"

#include "f_utils.h"
#include "filter_internal.h"

struct frame_duration_priv {
    struct mp_image *buffered;
};

static void frame_duration_process(struct mp_filter *f)
{
    struct frame_duration_priv *p = f->priv;

    if (!mp_pin_can_transfer_data(f->ppins[1], f->ppins[0]))
        return;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

    if (frame.type == MP_FRAME_EOF && p->buffered) {
        mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_VIDEO, p->buffered));
        p->buffered = NULL;
        // Pass through the actual EOF in the next iteration.
        mp_pin_out_repeat_eof(f->ppins[0]);
    } else if (frame.type == MP_FRAME_VIDEO) {
        struct mp_image *next = frame.data;
        if (p->buffered) {
            if (p->buffered->pts != MP_NOPTS_VALUE &&
                next->pts != MP_NOPTS_VALUE &&
                next->pts >= p->buffered->pts)
            {
                p->buffered->pkt_duration = next->pts - p->buffered->pts;
            }
            mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_VIDEO, p->buffered));
        } else {
            mp_pin_out_request_data(f->ppins[0]);
        }
        p->buffered = next;
    } else {
        mp_pin_in_write(f->ppins[1], frame);
    }
}

static void frame_duration_reset(struct mp_filter *f)
{
    struct frame_duration_priv *p = f->priv;

    mp_image_unrefp(&p->buffered);
}

static const struct mp_filter_info frame_duration_filter = {
    .name = "frame_duration",
    .priv_size = sizeof(struct frame_duration_priv),
    .process = frame_duration_process,
    .reset = frame_duration_reset,
    .destroy = frame_duration_reset,
};

struct mp_filter *mp_compute_frame_duration_create(struct mp_filter *parent)
{
    struct mp_filter *f = mp_filter_create(parent, &frame_duration_filter);
    if (!f)
        return NULL;

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    return f;
}

void mp_chain_filters(struct mp_pin *in, struct mp_pin *out,
                      struct mp_filter **filters, int num_filters)
{
    for (int n = 0; n < num_filters; n++) {
        if (!filters[n])
            continue;
        assert(filters[n]->num_pins == 2);
        mp_pin_connect(filters[n]->pins[0], in);
        in = filters[n]->pins[1];
    }
    mp_pin_connect(out, in);
}

// Make it repeat process().
static void mark_progress(struct mp_subfilter *sub)
{
    // f == NULL is not really allowed, but at least don't crash.
    struct mp_filter *f = mp_pin_get_manual_connection(sub->in);
    if (f)
        mp_filter_internal_mark_progress(f);
}

bool mp_subfilter_read(struct mp_subfilter *sub)
{
    if (sub->filter) {
        if (mp_pin_can_transfer_data(sub->out, sub->filter->pins[1])) {
            struct mp_frame frame = mp_pin_out_read(sub->filter->pins[1]);
            if (sub->draining && frame.type == MP_FRAME_EOF) {
                sub->draining = false;
                TA_FREEP(&sub->filter);
                mark_progress(sub);
                return false;
            }
            mp_pin_in_write(sub->out, frame);
            return false;
        }
        if (sub->draining)
            return false;
    }

    struct mp_pin *out = sub->filter ? sub->filter->pins[0] : sub->out;

    if (sub->frame.type)
        return mp_pin_in_needs_data(out);

    if (!mp_pin_can_transfer_data(out, sub->in))
        return false;

    sub->frame = mp_pin_out_read(sub->in);
    return true;
}

void mp_subfilter_reset(struct mp_subfilter *sub)
{
    if (sub->filter && sub->draining)
        TA_FREEP(&sub->filter);
    sub->draining = false;
    mp_frame_unref(&sub->frame);
}

void mp_subfilter_continue(struct mp_subfilter *sub)
{
    struct mp_pin *out = sub->filter ? sub->filter->pins[0] : sub->out;
    // It was made sure earlier that the pin is writable, unless the filter
    // was newly created, or a previously existing filter (which was going to
    // accept input) was destroyed. In those cases, essentially restart
    // data flow.
    if (!mp_pin_in_needs_data(out)) {
        mark_progress(sub);
        return;
    }
    mp_pin_in_write(out, sub->frame);
    sub->frame = MP_NO_FRAME;
}

void mp_subfilter_destroy(struct mp_subfilter *sub)
{
    TA_FREEP(&sub->filter);
    sub->draining = false;
}

bool mp_subfilter_drain_destroy(struct mp_subfilter *sub)
{
    if (!sub->draining && sub->filter) {
        // We know the filter is writable (unless the user created a new filter
        // and immediately called this function, which is invalid).
        mp_pin_in_write(sub->filter->pins[0], MP_EOF_FRAME);
        sub->draining = true;
    }
    return !sub->filter;
}

static const struct mp_filter_info bidir_nop_filter = {
    .name = "nop",
};

struct mp_filter *mp_bidir_nop_filter_create(struct mp_filter *parent)
{
    struct mp_filter *f = mp_filter_create(parent, &bidir_nop_filter);
    if (!f)
        return NULL;

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    mp_pin_connect(f->ppins[1], f->ppins[0]);

    return f;
}

struct fixed_aframe_size_priv {
    int samples;
    bool pad_silence;
    struct mp_aframe *in;
    struct mp_aframe *out;
    int out_written; // valid samples in out
    struct mp_aframe_pool *pool;
};

static void fixed_aframe_size_process(struct mp_filter *f)
{
    struct fixed_aframe_size_priv *p = f->priv;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    if (p->in && !mp_aframe_get_size(p->in))
        TA_FREEP(&p->in);

    if (!p->in) {
        struct mp_frame frame = mp_pin_out_read(f->ppins[0]);
        if (frame.type == MP_FRAME_EOF) {
            if (!p->out) {
                mp_pin_in_write(f->ppins[1], frame);
                return;
            }
            mp_pin_out_repeat_eof(f->ppins[0]);
        } else if (frame.type == MP_FRAME_AUDIO) {
            p->in = frame.data;
            if (p->out && !mp_aframe_config_equals(p->out, p->in)) {
                mp_pin_out_unread(f->ppins[0], frame);
                p->in = NULL;
            }
        } else if (frame.type) {
            MP_ERR(f, "unsupported frame type\n");
            mp_filter_internal_mark_failed(f);
            return;
        } else {
            return; // no new data yet
        }
    }

    if (p->in) {
        if (!p->out) {
            p->out = mp_aframe_create();
            mp_aframe_config_copy(p->out, p->in);
            mp_aframe_copy_attributes(p->out, p->in);
            if (mp_aframe_pool_allocate(p->pool, p->out, p->samples) < 0) {
                mp_filter_internal_mark_failed(f);
                return;
            }
            p->out_written = 0;
        }
        int in_samples = mp_aframe_get_size(p->in);
        int copy = MPMIN(in_samples, p->samples - p->out_written);
        if (!mp_aframe_copy_samples(p->out, p->out_written, p->in, 0, copy))
            assert(0);
        mp_aframe_skip_samples(p->in, copy);
        p->out_written += copy;
    }

    // p->in not set means draining for EOF or format change
    if ((!p->in && p->out_written) || p->out_written == p->samples) {
        int missing = p->samples - p->out_written;
        assert(missing >= 0);
        if (missing) {
            mp_aframe_set_silence(p->out, p->out_written, missing);
            if (!p->pad_silence)
                mp_aframe_set_size(p->out, p->out_written);
        }
        mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_AUDIO, p->out));
        p->out = NULL;
        p->out_written = 0;
    } else {
        mp_pin_out_request_data_next(f->ppins[0]);
    }
}

static void fixed_aframe_size_reset(struct mp_filter *f)
{
    struct fixed_aframe_size_priv *p = f->priv;

    TA_FREEP(&p->in);
    TA_FREEP(&p->out);
    p->out_written = 0;
}

static const struct mp_filter_info fixed_aframe_size_filter = {
    .name = "fixed_aframe_size",
    .priv_size = sizeof(struct fixed_aframe_size_priv),
    .process = fixed_aframe_size_process,
    .reset = fixed_aframe_size_reset,
    .destroy = fixed_aframe_size_reset,
};

struct mp_filter *mp_fixed_aframe_size_create(struct mp_filter *parent,
                                              int samples, bool pad_silence)
{
    if (samples < 1)
        return NULL;

    struct mp_filter *f = mp_filter_create(parent, &fixed_aframe_size_filter);
    if (!f)
        return NULL;

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct fixed_aframe_size_priv *p = f->priv;
    p->samples = samples;
    p->pad_silence = pad_silence;
    p->pool = mp_aframe_pool_create(p);

    return f;
}
