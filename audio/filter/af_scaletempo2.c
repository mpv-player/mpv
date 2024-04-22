#include "audio/aframe.h"
#include "audio/filter/af_scaletempo2_internals.h"
#include "audio/format.h"
#include "common/common.h"
#include "filters/f_autoconvert.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_option.h"

struct priv {
    struct mp_scaletempo2 *data;
    struct mp_pin *in_pin;
    struct mp_aframe *cur_format;
    struct mp_aframe_pool *out_pool;
    bool sent_final;
    struct mp_aframe *pending;
    bool initialized;
    float speed;
};

static bool init_scaletempo2(struct mp_filter *f);
static void af_scaletempo2_reset(struct mp_filter *f);

static void af_scaletempo2_process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    while (!p->initialized || !p->pending ||
           !mp_scaletempo2_frames_available(p->data, p->speed))
    {
        bool eof = false;
        if (!p->pending || !mp_aframe_get_size(p->pending)) {
            struct mp_frame frame = mp_pin_out_read(p->in_pin);
            if (frame.type == MP_FRAME_AUDIO) {
                TA_FREEP(&p->pending);
                p->pending = frame.data;
            } else if (frame.type == MP_FRAME_EOF) {
                eof = true;
            } else if (frame.type) {
                MP_ERR(f, "unexpected frame type\n");
                goto error;
            } else {
                return; // no new data yet
            }
        }
        assert(p->pending || eof);

        if (!p->initialized) {
            if (!p->pending) {
                mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
                return;
            }
            if (!init_scaletempo2(f))
                goto error;
        }

        bool format_change =
            p->pending && !mp_aframe_config_equals(p->pending, p->cur_format);

        bool final = format_change || eof;
        if (p->pending && !format_change && !p->sent_final) {
            int frame_size = mp_aframe_get_size(p->pending);
            uint8_t **planes = mp_aframe_get_data_ro(p->pending);
            int read = mp_scaletempo2_fill_input_buffer(p->data,
                planes, frame_size, p->speed);
            mp_aframe_skip_samples(p->pending, read);
        }
        if (final && p->pending && !p->sent_final) {
            mp_scaletempo2_set_final(p->data);
            p->sent_final = true;
        }

        if (mp_scaletempo2_frames_available(p->data, p->speed)) {
            if (eof) {
                mp_pin_out_repeat_eof(p->in_pin); // drain more next time
            }
        } else if (final) {
            p->initialized = false;
            p->sent_final = false;
            if (eof) {
                mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
                return;
            }
            // for format change go on with proper reinit on the next iteration
        }
    }

    assert(p->pending);
    if (mp_scaletempo2_frames_available(p->data, p->speed)) {
        struct mp_aframe *out = mp_aframe_new_ref(p->cur_format);
        int out_samples = p->data->ola_hop_size;
        if (mp_aframe_pool_allocate(p->out_pool, out, out_samples) < 0) {
            talloc_free(out);
            goto error;
        }

        mp_aframe_copy_attributes(out, p->pending);

        uint8_t **planes = mp_aframe_get_data_rw(out);
        assert(planes);
        assert(mp_aframe_get_planes(out) == p->data->channels);

        out_samples = mp_scaletempo2_fill_buffer(p->data,
            (float**)planes, out_samples, p->speed);

        double pts = mp_aframe_get_pts(p->pending);
        if (pts != MP_NOPTS_VALUE) {
            double frame_delay = mp_scaletempo2_get_latency(p->data, p->speed)
                + out_samples * p->speed;
            mp_aframe_set_pts(out, pts - frame_delay / mp_aframe_get_effective_rate(out));

            if (p->sent_final) {
                double remain_pts = pts - mp_aframe_get_pts(out);
                double rate = mp_aframe_get_effective_rate(out) / p->speed;
                int max_samples = MPMAX(0, (int) (remain_pts * rate));
                // truncate final packet to expected length
                if (out_samples >= max_samples) {
                    out_samples = max_samples;

                    // reset the filter to ensure it stops generating audio
                    // and mp_scaletempo2_frames_available returns false
                    mp_scaletempo2_reset(p->data);
                }
            }
        }

        mp_aframe_set_size(out, out_samples);
        mp_aframe_mul_speed(out, p->speed);
        mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_AUDIO, out));
    }

    return;
error:
    mp_filter_internal_mark_failed(f);
}

static bool init_scaletempo2(struct mp_filter *f)
{
    struct priv *p = f->priv;
    assert(p->pending);

    if (mp_aframe_get_format(p->pending) != AF_FORMAT_FLOATP)
        return false;

    mp_aframe_reset(p->cur_format);
    p->initialized = true;
    p->sent_final = false;
    mp_aframe_config_copy(p->cur_format, p->pending);

    mp_scaletempo2_init(p->data, mp_aframe_get_channels(p->pending),
        mp_aframe_get_rate(p->pending));

    return true;
}

static bool af_scaletempo2_command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct priv *p = f->priv;

    switch (cmd->type) {
    case MP_FILTER_COMMAND_SET_SPEED:
        p->speed = cmd->speed;
        return true;
    }

    return false;
}

static void af_scaletempo2_reset(struct mp_filter *f)
{
    struct priv *p = f->priv;
    mp_scaletempo2_reset(p->data);
    p->initialized = false;
    TA_FREEP(&p->pending);
}

static void af_scaletempo2_destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;
    TA_FREEP(&p->data);
    TA_FREEP(&p->pending);
}

static const struct mp_filter_info af_scaletempo2_filter = {
    .name = "scaletempo2",
    .priv_size = sizeof(struct priv),
    .process = af_scaletempo2_process,
    .command = af_scaletempo2_command,
    .reset = af_scaletempo2_reset,
    .destroy = af_scaletempo2_destroy,
};

static struct mp_filter *af_scaletempo2_create(
    struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &af_scaletempo2_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->data = talloc_zero(p, struct mp_scaletempo2);
    p->data->opts = talloc_steal(p, options);
    p->speed = 1.0;
    p->cur_format = talloc_steal(p, mp_aframe_create());
    p->out_pool = mp_aframe_pool_create(p);
    p->pending = NULL;
    p->initialized = false;

    struct mp_autoconvert *conv = mp_autoconvert_create(f);
    if (!conv)
        abort();

    mp_autoconvert_add_afmt(conv, AF_FORMAT_FLOATP);

    mp_pin_connect(conv->f->pins[0], f->ppins[0]);
    p->in_pin = conv->f->pins[1];

    return f;
}

#define OPT_BASE_STRUCT struct mp_scaletempo2_opts
const struct mp_user_filter_entry af_scaletempo2 = {
    .desc = {
        .description = "Scale audio tempo while maintaining pitch"
            " (filter ported from chromium)",
        .name = "scaletempo2",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .min_playback_rate = 0.25,
            .max_playback_rate = 8.0,
            .ola_window_size_ms = 12,
            .wsola_search_interval_ms = 40,
        },
        .options = (const struct m_option[]) {
            {"search-interval",
                OPT_FLOAT(wsola_search_interval_ms), M_RANGE(1, 1000)},
            {"window-size",
                OPT_FLOAT(ola_window_size_ms), M_RANGE(1, 1000)},
            {"min-speed",
                OPT_FLOAT(min_playback_rate), M_RANGE(0, FLT_MAX)},
            {"max-speed",
                OPT_FLOAT(max_playback_rate), M_RANGE(0, FLT_MAX)},
            {0}
        }
    },
    .create = af_scaletempo2_create,
};
