#include "audio/aframe.h"
#include "audio/out/ao.h"
#include "common/global.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "video/out/vo.h"

#include "filter_internal.h"

#include "f_autoconvert.h"
#include "f_auto_filters.h"
#include "f_lavfi.h"
#include "f_output_chain.h"
#include "f_utils.h"
#include "user_filters.h"

struct chain {
    struct mp_filter *f;
    struct mp_log *log;

    enum mp_output_chain_type type;

    // Expected media type.
    enum mp_frame_type frame_type;

    struct mp_stream_info stream_info;

    struct mp_user_filter **pre_filters;
    int num_pre_filters;
    struct mp_user_filter **post_filters;
    int num_post_filters;

    struct mp_user_filter **user_filters;
    int num_user_filters;

    // Concatentated list of pre+user+post filters.
    struct mp_user_filter **all_filters;
    int num_all_filters;
    // First input/last output of all_filters[].
    struct mp_pin *filters_in, *filters_out;

    struct mp_user_filter *input, *output;
    struct mp_autoconvert *convert;

    struct vo *vo;
    struct ao *ao;

    struct mp_frame pending_input;

    // Some chain types (MP_OUTPUT_CHAIN_AUDIO) require draining the entire
    // filter chain on format changes and further complex actions:
    //  0: normal filtering
    //  1: input changed, flushing out remaining frames from current filters
    //  2: flushing finished
    //  3: sent new frame through chain for format probing
    //  4: sent EOF through chain for format probing
    //  5: received format probing frame; now waiting for API user to call
    //     mp_output_chain_set_ao().
    int format_change_phase;
    // True if it's a second run trying to see if downmix can be moved up.
    bool format_change_second_try;

    struct mp_output_chain public;
};

// This wraps each individual "actual" filter for:
//  - isolating against its failure (logging it and disabling the filter)
//  - tracking its output format (mostly for logging)
//  - store extra per-filter information like the filter label
struct mp_user_filter {
    struct chain *p;

    struct mp_filter *wrapper; // parent filter for f
    struct mp_filter *f; // the actual user filter
    struct m_obj_settings *args; // NULL, or list of 1 item with creation args
    char *label;
    bool generated_label;
    char *name;
    bool is_output_converter;
    bool is_input;
    bool is_channelremix;

    struct mp_image_params last_out_params;
    struct mp_aframe *last_out_aformat;

    int64_t last_in_pts, last_out_pts;

    bool failed;
    bool error_eof_sent;
};

static void recheck_channelremix_filter(struct chain *p);
static void remove_channelremix_filter(struct chain *p);

static void update_output_caps(struct chain *p)
{
    if (p->type != MP_OUTPUT_CHAIN_VIDEO)
        return;

    mp_autoconvert_clear(p->convert);

    if (p->vo) {
        uint8_t allowed_output_formats[IMGFMT_END - IMGFMT_START] = {0};
        vo_query_formats(p->vo, allowed_output_formats);

        for (int n = 0; n < MP_ARRAY_SIZE(allowed_output_formats); n++) {
            if (allowed_output_formats[n])
                mp_autoconvert_add_imgfmt(p->convert, IMGFMT_START + n, 0);
        }

        if (p->vo->hwdec_devs)
            mp_autoconvert_add_vo_hwdec_subfmts(p->convert, p->vo->hwdec_devs);
    }
}

static bool check_out_format_change(struct mp_user_filter *u,
                                    struct mp_frame frame)
{
    struct chain *p = u->p;
    bool changed = false;

    if (frame.type == MP_FRAME_VIDEO) {
        struct mp_image *img = frame.data;

        if (!mp_image_params_equal(&img->params, &u->last_out_params)) {
            MP_VERBOSE(p, "[%s] %s\n", u->name,
                       mp_image_params_to_str(&img->params));
            u->last_out_params = img->params;

            if (u->is_input) {
                p->public.input_params = img->params;

                // Unfortunately there's no good place to update these.
                // But a common case is enabling HW decoding, which
                // might init some support of them in the VO, and update
                // the VO's format list.
                update_output_caps(p);
            } else if (u->is_output_converter) {
                p->public.output_params = img->params;
            }

            p->public.reconfig_happened = true;
            changed = true;
        }
    }

    if (frame.type == MP_FRAME_AUDIO) {
        struct mp_aframe *aframe = frame.data;

        if (!mp_aframe_config_equals(aframe, u->last_out_aformat)) {
            MP_VERBOSE(p, "[%s] %s\n", u->name,
                       mp_aframe_format_str(aframe));
            mp_aframe_config_copy(u->last_out_aformat, aframe);

            if (u->is_input) {
                mp_aframe_config_copy(p->public.input_aformat, aframe);
            } else if (u->is_output_converter) {
                mp_aframe_config_copy(p->public.output_aformat, aframe);
            }

            p->public.reconfig_happened = true;
            changed = true;
        }
    }

    return changed;
}

static void process_user(struct mp_filter *f)
{
    struct mp_user_filter *u = f->priv;
    struct chain *p = u->p;

    mp_filter_set_error_handler(u->f, f);
    const char *name = u->label ? u->label : u->name;
    assert(u->name);

    if (!u->failed && mp_filter_has_failed(u->f)) {
        if (u->is_output_converter) {
            // This is a fuckup we can't ignore.
            MP_FATAL(p, "Cannot convert decoder/filter output to any format "
                     "supported by the output.\n");
            p->public.failed_output_conversion = true;
            p->format_change_phase = 0;
            mp_filter_wakeup(p->f);
        } else {
            MP_ERR(p, "Disabling filter %s because it has failed.\n", name);
            mp_filter_reset(u->f); // clear out staled buffered data
        }
        u->failed = true;
        if (p->format_change_phase)
            p->format_change_phase = 2; // redo without it
    }

    if (u->failed) {
        if (u->is_output_converter) {
            if (mp_pin_in_needs_data(f->ppins[1])) {
                if (!u->error_eof_sent)
                    mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
                u->error_eof_sent = true;
            }
            return;
        }

        mp_pin_transfer_data(f->ppins[1], f->ppins[0]);
        return;
    }

    if (mp_pin_can_transfer_data(u->f->pins[0], f->ppins[0])) {
        struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

        double pts = mp_frame_get_pts(frame);
        if (pts != MP_NOPTS_VALUE)
            u->last_in_pts = pts;

        mp_pin_in_write(u->f->pins[0], frame);
    }

    if (mp_pin_can_transfer_data(f->ppins[1], u->f->pins[1])) {
        struct mp_frame frame = mp_pin_out_read(u->f->pins[1]);

        bool changed = check_out_format_change(u, frame);
        if (p->type == MP_OUTPUT_CHAIN_AUDIO && (!p->ao || changed) &&
            u->is_input && !p->format_change_phase)
        {
            // Format changed -> block filtering, start draining current filters.
            MP_VERBOSE(p, "format changed, draining filter chain\n");
            mp_frame_unref(&p->pending_input);
            p->pending_input = frame;
            p->format_change_phase = 1;
            mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
            return;
        }

        double pts = mp_frame_get_pts(frame);
        if (pts != MP_NOPTS_VALUE)
            u->last_out_pts = pts;

        mp_pin_in_write(f->ppins[1], frame);
    }
}

static void reset_user(struct mp_filter *f)
{
    struct mp_user_filter *u = f->priv;

    u->error_eof_sent = false;
    u->last_in_pts = u->last_out_pts = MP_NOPTS_VALUE;
}

static void destroy_user(struct mp_filter *f)
{
    struct mp_user_filter *u = f->priv;

    struct m_option dummy = {.type = &m_option_type_obj_settings_list};
    m_option_free(&dummy, &u->args);

    mp_filter_free_children(f);
}

static const struct mp_filter_info user_wrapper_filter = {
    .name = "user_filter_wrapper",
    .priv_size = sizeof(struct mp_user_filter),
    .process = process_user,
    .reset = reset_user,
    .destroy = destroy_user,
};

static struct mp_user_filter *create_wrapper_filter(struct chain *p)
{
    struct mp_filter *f = mp_filter_create(p->f, &user_wrapper_filter);
    if (!f)
        abort();
    struct mp_user_filter *wrapper = f->priv;
    wrapper->wrapper = f;
    wrapper->p = p;
    wrapper->last_out_aformat = talloc_steal(wrapper, mp_aframe_create());
    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");
    return wrapper;
}

// Rebuild p->all_filters and relink the filters. Non-destructive if no change.
static void relink_filter_list(struct chain *p)
{
    struct mp_user_filter **all_filters[3] =
        {p->pre_filters, p->user_filters, p->post_filters};
    int all_filters_num[3] =
        {p->num_pre_filters, p->num_user_filters, p->num_post_filters};
    p->num_all_filters = 0;
    for (int n = 0; n < 3; n++) {
        struct mp_user_filter **filters = all_filters[n];
        int filters_num = all_filters_num[n];
        for (int i = 0; i < filters_num; i++)
            MP_TARRAY_APPEND(p, p->all_filters, p->num_all_filters, filters[i]);
    }

    assert(p->num_all_filters > 0);

    p->filters_in = NULL;
    p->filters_out = NULL;
    for (int n = 0; n < p->num_all_filters; n++) {
        struct mp_filter *f = p->all_filters[n]->wrapper;
        if (n == 0)
            p->filters_in = f->pins[0];
        if (p->filters_out)
            mp_pin_connect(f->pins[0], p->filters_out);
        p->filters_out = f->pins[1];
    }
}

// Special logic for draining on format changes (for audio). Never used or
// initiated video.
static void process_format_change(struct mp_filter *f)
{
    struct chain *p = f->priv;

    if (mp_pin_in_needs_data(p->filters_in)) {
        if (p->format_change_phase == 2) {
            MP_VERBOSE(p, "probing new format\n");
            // Clear any old state.
            if (!p->format_change_second_try) {
                mp_autoconvert_clear(p->convert);
                remove_channelremix_filter(p);
            }
            for (int n = 0; n < p->num_all_filters; n++)
                mp_filter_reset(p->all_filters[n]->f);
            // Filter a copy of the new input frame to see what comes out.
            struct mp_frame frame = mp_frame_ref(p->pending_input);
            if (!frame.type)
                abort();
            mp_pin_in_write(p->filters_in, frame);
            mp_pin_out_request_data(p->filters_out);
            p->format_change_phase = 3;
        } else if (p->format_change_phase == 3) {
            MP_VERBOSE(p, "probing new format (drain)\n");
            mp_pin_in_write(p->filters_in, MP_EOF_FRAME);
            p->format_change_phase = 4;
        }
    }

    if (mp_pin_can_transfer_data(f->ppins[1], p->filters_out)) {
        struct mp_frame frame = mp_pin_out_read(p->filters_out);

        if (frame.type == MP_FRAME_EOF) {
            // We're apparently draining for a format change, and we got EOF
            // from the chain, which means we're done draining.
            if (p->format_change_phase == 1) {
                MP_VERBOSE(p, "done format change draining\n");
                // Then we need to start probing the new format.
                p->format_change_phase = 2;
                mp_pin_out_request_data(p->filters_out);
            } else if (!p->public.failed_output_conversion) {
                MP_ERR(p, "we didn't get an output frame? (broken filter?)\n");
            }
            mp_filter_internal_mark_progress(f);
            return;
        }

        if (p->format_change_phase >= 2) {
            // We were filtering a "test" frame to probe the format. Now
            // that we have it (apparently), just discard it, and make the
            // user aware of the previously grabbed format.
            MP_VERBOSE(p, "got output format from probing\n");
            mp_frame_unref(&frame);
            for (int n = 0; n < p->num_all_filters; n++)
                mp_filter_reset(p->all_filters[n]->f);
            if (p->format_change_second_try) {
                p->format_change_second_try = false;
                p->format_change_phase = 0;
                recheck_channelremix_filter(p);
            } else {
                p->ao = NULL;
                p->public.ao_needs_update = true;
                p->format_change_phase = 5;
            }
            // Do something silly to ensure the f_output_chain user gets
            // notified properly.
            mp_filter_wakeup(f);
            return;
        }

        // Draining remaining data.
        mp_pin_in_write(f->ppins[1], frame);
    }
}


static void process(struct mp_filter *f)
{
    struct chain *p = f->priv;

    if (p->format_change_phase) {
        process_format_change(f);
        return;
    }

    // Send remaining input from previous format change.
    if (p->pending_input.type) {
        if (mp_pin_in_needs_data(p->filters_in)) {
            mp_pin_in_write(p->filters_in, p->pending_input);
            p->pending_input = MP_NO_FRAME;
        }
    }

    if (mp_pin_can_transfer_data(p->filters_in, f->ppins[0])) {
        struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

        if (frame.type == MP_FRAME_EOF)
            MP_VERBOSE(p, "filter input EOF\n");

        if (frame.type == MP_FRAME_VIDEO && p->public.update_subtitles) {
            p->public.update_subtitles(p->public.update_subtitles_ctx,
                                       mp_frame_get_pts(frame));
        }

        mp_pin_in_write(p->filters_in, frame);
    }

    if (mp_pin_can_transfer_data(f->ppins[1], p->filters_out)) {
        struct mp_frame frame = mp_pin_out_read(p->filters_out);

        p->public.got_output_eof = frame.type == MP_FRAME_EOF;
        if (p->public.got_output_eof)
            MP_VERBOSE(p, "filter output EOF\n");

        mp_pin_in_write(f->ppins[1], frame);
    }
}

static void reset(struct mp_filter *f)
{
    struct chain *p = f->priv;

    // (if format initialization was in progress, this can be repeated next time)
    mp_frame_unref(&p->pending_input);
    p->format_change_phase = 0;
    if (p->format_change_second_try)
        remove_channelremix_filter(p);
    p->format_change_second_try = false;
    p->public.ao_needs_update = false;

    p->public.got_output_eof = false;
}

void mp_output_chain_reset_harder(struct mp_output_chain *c)
{
    struct chain *p = c->f->priv;

    mp_filter_reset(p->f);

    p->public.failed_output_conversion = false;
    for (int n = 0; n < p->num_all_filters; n++) {
        struct mp_user_filter *u = p->all_filters[n];

        u->failed = false;
        u->last_out_params = (struct mp_image_params){0};
        mp_aframe_reset(u->last_out_aformat);
    }
}

static void destroy(struct mp_filter *f)
{
    reset(f);
}

static const struct mp_filter_info output_chain_filter = {
    .name = "output_chain",
    .priv_size = sizeof(struct chain),
    .process = process,
    .reset = reset,
    .destroy = destroy,
};

static double get_display_fps(struct mp_stream_info *i)
{
    struct chain *p = i->priv;
    double res = 0;
    if (p->vo)
        vo_control(p->vo, VOCTRL_GET_DISPLAY_FPS, &res);
    return res;
}

static double get_container_fps(struct mp_stream_info *i)
{
    struct chain *p = i->priv;
    return p->public.container_fps;
}

void mp_output_chain_set_vo(struct mp_output_chain *c, struct vo *vo)
{
    struct chain *p = c->f->priv;

    p->stream_info.hwdec_devs = vo ? vo->hwdec_devs : NULL;
    p->stream_info.osd = vo ? vo->osd : NULL;
    p->stream_info.rotate90 = vo ? vo->driver->caps & VO_CAP_ROTATE90 : false;
    p->stream_info.dr_vo = vo;
    p->vo = vo;
    update_output_caps(p);
}

// If there are any user filters, and they don't affect the channel config,
// then move upmix/downmix to the start of the chain.
static void maybe_move_up_channelremix(struct chain *p, struct mp_chmap *final)
{
    assert(p->num_all_filters >= 2); // at least in/convert filters
    struct mp_user_filter *first = p->all_filters[0]; // "in" pseudo filter
    struct mp_chmap in = {0};
    mp_aframe_get_chmap(first->last_out_aformat, &in);
    if (mp_chmap_is_unknown(&in))
        return;
    mp_chmap_reorder_to_lavc(&in);
    if (!mp_chmap_is_valid(&in) || mp_chmap_equals_reordered(&in, final))
        return;
    for (int n = 0; n < p->num_all_filters; n++) {
        struct mp_user_filter *u = p->all_filters[n];
        struct mp_chmap chmap = {0};
        mp_aframe_get_chmap(u->last_out_aformat, &chmap);
        if (!mp_chmap_equals_reordered(&in, &chmap))
            return; // some remix going in
    }

    if (!p->num_user_filters)
        return; // would be a NOP

    MP_VERBOSE(p, "trying with channel remixing moved to start of chain\n");

    struct mp_user_filter *remix = create_wrapper_filter(p);
    struct mp_autoconvert *convert = mp_autoconvert_create(remix->wrapper);
    if (!convert)
        abort();
    mp_autoconvert_add_chmap(convert, final);
    remix->name = "channelremix";
    remix->f = convert->f;
    remix->is_channelremix = true;
    MP_TARRAY_APPEND(p, p->pre_filters, p->num_pre_filters, remix);
    relink_filter_list(p);

    // now run the scary state machine again in order to see what filters do
    // with the remixed channel data and if it was a good idea
    p->format_change_phase = 2;
    p->format_change_second_try = true;
}

static void remove_channelremix_filter(struct chain *p)
{
    for (int n = 0; n < p->num_pre_filters; n++) {
        struct mp_user_filter *u = p->pre_filters[n];
        if (u->is_channelremix) {
            MP_TARRAY_REMOVE_AT(p->pre_filters, p->num_pre_filters, n);
            talloc_free(u->wrapper);
            relink_filter_list(p);
            break;
        }
    }
}

static void recheck_channelremix_filter(struct chain *p)
{
    struct mp_chmap in = {0};
    int start = -1;
    for (int n = 0; n < p->num_all_filters; n++) {
        struct mp_user_filter *u = p->all_filters[n];
        if (u->is_channelremix) {
            mp_aframe_get_chmap(u->last_out_aformat, &in);
            start = n;
            break;
        }
    }

    if (start < 0 || !mp_chmap_is_valid(&in))
        goto remove;

    for (int n = start; n < p->num_all_filters; n++) {
        struct mp_user_filter *u = p->all_filters[n];
        struct mp_chmap chmap = {0};
        mp_aframe_get_chmap(u->last_out_aformat, &chmap);
        if (!mp_chmap_equals_reordered(&in, &chmap))
            goto remove;
    }

    return;
remove:
    MP_VERBOSE(p, "reverting moved up channel remixing\n");
    remove_channelremix_filter(p);
}

void mp_output_chain_set_ao(struct mp_output_chain *c, struct ao *ao)
{
    struct chain *p = c->f->priv;

    assert(p->public.ao_needs_update); // can't just call it any time
    assert(p->format_change_phase == 5);
    assert(!p->format_change_second_try);

    p->public.ao_needs_update = false;
    p->format_change_phase = 0;

    p->ao = ao;

    int out_format = 0;
    int out_rate = 0;
    struct mp_chmap out_channels = {0};
    ao_get_format(p->ao, &out_rate, &out_format, &out_channels);

    mp_autoconvert_clear(p->convert);
    mp_autoconvert_add_afmt(p->convert, out_format);
    mp_autoconvert_add_srate(p->convert, out_rate);
    mp_autoconvert_add_chmap(p->convert, &out_channels);

    maybe_move_up_channelremix(p, &out_channels);

    mp_filter_wakeup(p->f);
}

static struct mp_user_filter *find_by_label(struct chain *p, const char *label)
{
    for (int n = 0; n < p->num_user_filters; n++) {
        struct mp_user_filter *u = p->user_filters[n];
        if (label && u->label && strcmp(label, u->label) == 0)
            return u;
    }
    return NULL;
}

bool mp_output_chain_command(struct mp_output_chain *c, const char *target,
                             struct mp_filter_command *cmd)
{
    struct chain *p = c->f->priv;

    if (!target || !target[0])
        return false;

    if (strcmp(target, "all") == 0 && cmd->type == MP_FILTER_COMMAND_TEXT) {
        // (Following old semantics.)
        for (int n = 0; n < p->num_user_filters; n++)
            mp_filter_command(p->user_filters[n]->f, cmd);
        return true;
    }

    struct mp_user_filter *f = find_by_label(p, target);
    if (!f)
        return false;

    return mp_filter_command(f->f, cmd);
}

// Set the speed on the last filter in the chain that supports it. If a filter
// supports it, reset *speed, then keep setting the speed on the other filters.
// The purpose of this is to make sure only 1 filter changes speed.
static void set_speed_any(struct mp_user_filter **filters, int num_filters,
                          bool resample, double *speed)
{
    for (int n = num_filters - 1; n >= 0; n--) {
        assert(*speed);
        struct mp_filter_command cmd = {
            .type = resample ? MP_FILTER_COMMAND_SET_SPEED_RESAMPLE
                             : MP_FILTER_COMMAND_SET_SPEED,
            .speed = *speed,
        };
        if (mp_filter_command(filters[n]->f, &cmd))
            *speed = 1.0;
    }
}

void mp_output_chain_set_audio_speed(struct mp_output_chain *c,
                                     double speed, double resample)
{
    struct chain *p = c->f->priv;

    // We always resample with the final libavresample instance.
    set_speed_any(p->post_filters, p->num_post_filters, true, &resample);

    // If users have filters like "scaletempo" insert anywhere, use that,
    // otherwise use the builtin ones.
    set_speed_any(p->user_filters, p->num_user_filters, false, &speed);
    set_speed_any(p->post_filters, p->num_post_filters, false, &speed);
}

double mp_output_get_measured_total_delay(struct mp_output_chain *c)
{
    struct chain *p = c->f->priv;

    double delay = 0;

    for (int n = 0; n < p->num_all_filters; n++) {
        struct mp_user_filter *u = p->all_filters[n];

        if (u->last_in_pts != MP_NOPTS_VALUE &&
            u->last_out_pts != MP_NOPTS_VALUE)
        {
            delay += u->last_in_pts - u->last_out_pts;
        }
    }

    return delay;
}

static bool compare_filter(struct m_obj_settings *a, struct m_obj_settings *b)
{
    if (a == b || !a || !b)
        return a == b;

    if (!a->name || !b->name)
        return a->name == b->name;

    if (!!a->label != !!b->label || (a->label && strcmp(a->label, b->label) != 0))
        return false;

    if (a->enabled != b->enabled)
        return false;

    if (!a->attribs || !a->attribs[0])
        return !b->attribs || !b->attribs[0];

    for (int n = 0; a->attribs[n] || b->attribs[n]; n++) {
        if (!a->attribs[n] || !b->attribs[n])
            return false;
        if (strcmp(a->attribs[n], b->attribs[n]) != 0)
            return false;
    }

    return true;
}

bool mp_output_chain_update_filters(struct mp_output_chain *c,
                                    struct m_obj_settings *list)
{
    struct chain *p = c->f->priv;

    struct mp_user_filter **add = NULL;      // new filters
    int num_add = 0;
    struct mp_user_filter **res = NULL;      // new final list
    int num_res = 0;
    bool *used = talloc_zero_array(NULL, bool, p->num_user_filters);

    for (int n = 0; list && list[n].name; n++) {
        struct m_obj_settings *entry = &list[n];

        if (!entry->enabled)
            continue;

        struct mp_user_filter *u = NULL;

        for (int i = 0; i < p->num_user_filters; i++) {
            if (!used[i] && compare_filter(entry, p->user_filters[i]->args)) {
                u = p->user_filters[i];
                used[i] = true;
                break;
            }
        }

        if (!u) {
            u = create_wrapper_filter(p);
            u->name = talloc_strdup(u, entry->name);
            u->label = talloc_strdup(u, entry->label);
            u->f = mp_create_user_filter(u->wrapper, p->type, entry->name,
                                         entry->attribs);
            if (!u->f) {
                talloc_free(u->wrapper);
                goto error;
            }

            struct m_obj_settings *args = (struct m_obj_settings[2]){*entry, {0}};

            struct m_option dummy = {.type = &m_option_type_obj_settings_list};
            m_option_copy(&dummy, &u->args, &args);

            MP_TARRAY_APPEND(NULL, add, num_add, u);
        }

        MP_TARRAY_APPEND(p, res, num_res, u);
    }

    // At this point we definitely know we'll use the new list, so clean up.

    for (int n = 0; n < p->num_user_filters; n++) {
        if (!used[n])
            talloc_free(p->user_filters[n]->wrapper);
    }

    talloc_free(p->user_filters);
    p->user_filters = res;
    p->num_user_filters = num_res;

    relink_filter_list(p);

    for (int n = 0; n < p->num_user_filters; n++) {
        struct mp_user_filter *u = p->user_filters[n];
        if (u->generated_label)
            TA_FREEP(&u->label);
        if (!u->label) {
            for (int i = 0; i < 100; i++) {
                char *label = mp_tprintf(80, "%s.%02d", u->name, i);
                if (!find_by_label(p, label)) {
                    u->label = talloc_strdup(u, label);
                    u->generated_label = true;
                    break;
                }
            }
        }
    }

    MP_VERBOSE(p, "User filter list:\n");
    for (int n = 0; n < p->num_user_filters; n++) {
        struct mp_user_filter *u = p->user_filters[n];
        MP_VERBOSE(p, "  %s (%s)\n", u->name, u->label ? u->label : "-");
    }
    if (!p->num_user_filters)
        MP_VERBOSE(p, "  (empty)\n");

    // Filters can load hwdec interops, which might add new formats.
    update_output_caps(p);

    mp_filter_wakeup(p->f);

    talloc_free(add);
    talloc_free(used);
    return true;

error:
    for (int n = 0; n < num_add; n++)
        talloc_free(add[n]);
    talloc_free(add);
    talloc_free(used);
    return false;
}

static void create_video_things(struct chain *p)
{
    p->frame_type = MP_FRAME_VIDEO;

    p->stream_info.priv = p;
    p->stream_info.get_display_fps = get_display_fps;
    p->stream_info.get_container_fps = get_container_fps;

    p->f->stream_info = &p->stream_info;

    struct mp_user_filter *f = create_wrapper_filter(p);
    f->name = "userdeint";
    f->f = mp_deint_create(f->wrapper);
    if (!f->f)
        abort();
    MP_TARRAY_APPEND(p, p->pre_filters, p->num_pre_filters, f);

    f = create_wrapper_filter(p);
    f->name = "autorotate";
    f->f = mp_autorotate_create(f->wrapper);
    if (!f->f)
        abort();
    MP_TARRAY_APPEND(p, p->post_filters, p->num_post_filters, f);
}

static void create_audio_things(struct chain *p)
{
    p->frame_type = MP_FRAME_AUDIO;

    struct mp_user_filter *f = create_wrapper_filter(p);
    f->name = "userspeed";
    f->f = mp_autoaspeed_create(f->wrapper);
    if (!f->f)
        abort();
    MP_TARRAY_APPEND(p, p->post_filters, p->num_post_filters, f);
}

struct mp_output_chain *mp_output_chain_create(struct mp_filter *parent,
                                               enum mp_output_chain_type type)
{
    struct mp_filter *f = mp_filter_create(parent, &output_chain_filter);
    if (!f)
        return NULL;

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    const char *log_name = NULL;
    switch (type) {
    case MP_OUTPUT_CHAIN_VIDEO: log_name = "!vf"; break;
    case MP_OUTPUT_CHAIN_AUDIO: log_name = "!af"; break;
    }
    if (log_name)
        f->log = mp_log_new(f, parent->global->log, log_name);

    struct chain *p = f->priv;
    p->f = f;
    p->log = f->log;
    p->type = type;

    struct mp_output_chain *c = &p->public;
    c->f = f;
    c->input_aformat = talloc_steal(p, mp_aframe_create());
    c->output_aformat = talloc_steal(p, mp_aframe_create());

    // Dummy filter for reporting and logging the input format.
    p->input = create_wrapper_filter(p);
    p->input->f = mp_bidir_nop_filter_create(p->input->wrapper);
    if (!p->input->f)
        abort();
    p->input->name = "in";
    p->input->is_input = true;
    MP_TARRAY_APPEND(p, p->pre_filters, p->num_pre_filters, p->input);

    switch (type) {
    case MP_OUTPUT_CHAIN_VIDEO: create_video_things(p); break;
    case MP_OUTPUT_CHAIN_AUDIO: create_audio_things(p); break;
    }

    p->output = create_wrapper_filter(p);
    p->convert = mp_autoconvert_create(p->output->wrapper);
    if (!p->convert)
        abort();
    p->output->name = "convert";
    p->output->is_output_converter = true;
    p->output->f = p->convert->f;
    MP_TARRAY_APPEND(p, p->post_filters, p->num_post_filters, p->output);

    relink_filter_list(p);

    reset(f);

    return c;
}
