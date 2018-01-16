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

    struct mp_image_params last_out_params;

    bool failed;
    bool error_eof_sent;
};

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
            mp_filter_wakeup(p->f);
        } else {
            MP_ERR(p, "Disabling filter %s because it has failed.\n", name);
            mp_filter_reset(u->f); // clear out staled buffered data
        }
        u->failed = true;
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

    mp_pin_transfer_data(u->f->pins[0], f->ppins[0]);

    if (mp_pin_can_transfer_data(f->ppins[1], u->f->pins[1])) {
        struct mp_frame frame = mp_pin_out_read(u->f->pins[1]);

        check_out_format_change(u, frame);

        mp_pin_in_write(f->ppins[1], frame);
    }
}

static void reset_user(struct mp_filter *f)
{
    struct mp_user_filter *u = f->priv;

    u->error_eof_sent = false;
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

static void process(struct mp_filter *f)
{
    struct chain *p = f->priv;

    if (mp_pin_can_transfer_data(p->filters_in, f->ppins[0])) {
        struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

        if (frame.type == MP_FRAME_EOF) {
            MP_VERBOSE(p, "filter input EOF\n");
            p->public.got_input_eof = true;
        }

        mp_pin_in_write(p->filters_in, frame);
    }

    if (mp_pin_can_transfer_data(f->ppins[1], p->filters_out)) {
        struct mp_frame frame = mp_pin_out_read(p->filters_out);

        if (frame.type == MP_FRAME_EOF) {
            MP_VERBOSE(p, "filter output EOF\n");
            p->public.got_output_eof = true;
        }

        mp_pin_in_write(f->ppins[1], frame);
    }
}

static void reset(struct mp_filter *f)
{
    struct chain *p = f->priv;

    p->public.got_input_eof = false;
    p->public.got_output_eof = false;
}

void mp_output_chain_reset_harder(struct mp_output_chain *c)
{
    struct chain *p = c->f->priv;

    mp_filter_reset(p->f);

    p->public.failed_output_conversion = false;
    for (int n = 0; n < p->num_all_filters; n++)
        p->all_filters[n]->failed = false;
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
    p->vo = vo;
    update_output_caps(p);
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
    }
    if (log_name)
        f->log = mp_log_new(f, parent->global->log, log_name);

    struct chain *p = f->priv;
    p->f = f;
    p->log = f->log;
    p->type = type;

    struct mp_output_chain *c = &p->public;
    c->f = f;

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
