#include "common/common.h"
#include "demux/demux.h"
#include "demux/packet.h"

#include "f_demux_in.h"
#include "filter_internal.h"

struct priv {
    struct sh_stream *src;
    bool eof_returned;
};

static void wakeup(void *ctx)
{
    struct mp_filter *f = ctx;

    mp_filter_wakeup(f);
}

static void process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_pin_in_needs_data(f->ppins[0]))
        return;

    struct demux_packet *pkt = NULL;
    if (demux_read_packet_async(p->src, &pkt) == 0)
        return; // wait

    struct mp_frame frame = {MP_FRAME_PACKET, pkt};
    if (pkt) {
        p->eof_returned = false;
    } else {
        frame.type = MP_FRAME_EOF;

        // While the demuxer will repeat EOFs, filters never do that.
        if (p->eof_returned)
            return;
        p->eof_returned = true;
    }

    mp_pin_in_write(f->ppins[0], frame);
}

static void reset(struct mp_filter *f)
{
    struct priv *p = f->priv;

    p->eof_returned = false;
}

static void destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;

    demux_set_stream_wakeup_cb(p->src, NULL, NULL);
}

static const struct mp_filter_info demux_filter = {
    .name = "demux_in",
    .priv_size = sizeof(struct priv),
    .process = process,
    .reset = reset,
    .destroy = destroy,
};

struct mp_filter *mp_demux_in_create(struct mp_filter *parent,
                                     struct sh_stream *src)
{
    struct mp_filter *f = mp_filter_create(parent, &demux_filter);
    if (!f)
        return NULL;

    struct priv *p = f->priv;
    p->src = src;

    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    demux_set_stream_wakeup_cb(p->src, wakeup, f);

    return f;
}
