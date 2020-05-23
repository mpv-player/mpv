#include "audio/aframe.h"
#include "audio/format.h"
#include "common/common.h"
#include "filters/f_autoconvert.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"

struct priv {
    double speed;
    double diff; // amount of too many additional samples in normal speed
    struct mp_aframe *last; // for repeating
};

static void process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    struct mp_frame frame = {0};

    double last_dur = p->last ? mp_aframe_duration(p->last) : 0;
    if (p->last && p->diff < 0 && -p->diff > last_dur / 2) {
        MP_VERBOSE(f, "repeat\n");
        frame = MAKE_FRAME(MP_FRAME_AUDIO, p->last);
        p->last = NULL;
    } else {
        frame = mp_pin_out_read(f->ppins[0]);

        if (frame.type == MP_FRAME_AUDIO) {
            last_dur = mp_aframe_duration(frame.data);
            p->diff -= last_dur;
            if (p->diff > last_dur / 2) {
                MP_VERBOSE(f, "drop\n");
                mp_frame_unref(&frame);
                mp_filter_internal_mark_progress(f);
            }
        }
    }

    if (frame.type == MP_FRAME_AUDIO) {
        struct mp_aframe *fr = frame.data;
        talloc_free(p->last);
        p->last = mp_aframe_new_ref(fr);
        mp_aframe_mul_speed(fr, p->speed);
        p->diff += mp_aframe_duration(fr);
        mp_aframe_set_pts(p->last, mp_aframe_end_pts(fr));
    } else if (frame.type == MP_FRAME_EOF) {
        TA_FREEP(&p->last);
    }
    mp_pin_in_write(f->ppins[1], frame);
}

static bool command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct priv *p = f->priv;

    switch (cmd->type) {
    case MP_FILTER_COMMAND_SET_SPEED:
        p->speed = cmd->speed;
        return true;
    }

    return false;
}

static void reset(struct mp_filter *f)
{
    struct priv *p = f->priv;

    TA_FREEP(&p->last);
    p->diff = 0;
}

static void destroy(struct mp_filter *f)
{
    reset(f);
}

static const struct mp_filter_info af_drop_filter = {
    .name = "drop",
    .priv_size = sizeof(struct priv),
    .process = process,
    .command = command,
    .reset = reset,
    .destroy = destroy,
};

static struct mp_filter *af_drop_create(struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &af_drop_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->speed = 1.0;

    return f;
}

const struct mp_user_filter_entry af_drop = {
    .desc = {
        .description = "Change audio speed by dropping/repeating frames",
        .name = "drop",
        .priv_size = sizeof(struct priv),
    },
    .create = af_drop_create,
};
