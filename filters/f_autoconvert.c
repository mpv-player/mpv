#include "config.h"

#include "audio/aframe.h"
#include "audio/chmap_sel.h"
#include "audio/format.h"
#include "common/common.h"
#include "common/msg.h"
#include "video/hwdec.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

#include "f_autoconvert.h"
#include "f_hwtransfer.h"
#include "f_swresample.h"
#include "f_swscale.h"
#include "f_utils.h"
#include "filter.h"
#include "filter_internal.h"

struct priv {
    struct mp_log *log;

    struct mp_subfilter sub;

    bool force_update;

    int *imgfmts;
    int *subfmts;
    int num_imgfmts;
    struct mp_image_params imgparams;
    bool imgparams_set;

    // Enable special conversion for the final stage before the VO.
    bool vo_convert;

    // sws state
    int in_imgfmt, in_subfmt;

    int *afmts;
    int num_afmts;
    int *srates;
    int num_srates;
    struct mp_chmap_sel chmaps;

    int in_afmt, in_srate;
    struct mp_chmap in_chmap;

    double audio_speed;
    bool resampling_forced;

    bool format_change_blocked;
    bool format_change_cont;

    struct mp_autoconvert public;
};

// Dummy filter for bundling sub-conversion filters.
static const struct mp_filter_info convert_filter = {
    .name = "convert",
};

void mp_autoconvert_clear(struct mp_autoconvert *c)
{
    struct priv *p = c->f->priv;

    p->num_imgfmts = 0;
    p->imgparams_set = false;
    p->num_afmts = 0;
    p->num_srates = 0;
    p->chmaps = (struct mp_chmap_sel){0};
    p->force_update = true;
}

void mp_autoconvert_add_imgfmt(struct mp_autoconvert *c, int imgfmt, int subfmt)
{
    struct priv *p = c->f->priv;

    MP_TARRAY_GROW(p, p->imgfmts, p->num_imgfmts);
    MP_TARRAY_GROW(p, p->subfmts, p->num_imgfmts);

    p->imgfmts[p->num_imgfmts] = imgfmt;
    p->subfmts[p->num_imgfmts] = subfmt;

    p->num_imgfmts += 1;
    p->force_update = true;
}

void mp_autoconvert_set_target_image_params(struct mp_autoconvert *c,
                                            struct mp_image_params *par)
{
    struct priv *p = c->f->priv;

    if (p->imgparams_set && mp_image_params_equal(&p->imgparams, par) &&
        p->num_imgfmts == 1 && p->imgfmts[0] == par->imgfmt &&
        p->subfmts[0] == par->hw_subfmt)
        return;

    p->imgparams = *par;
    p->imgparams_set = true;

    p->num_imgfmts = 0;
    mp_autoconvert_add_imgfmt(c, par->imgfmt, par->hw_subfmt);
}

void mp_autoconvert_add_all_sw_imgfmts(struct mp_autoconvert *c)
{
    for (int n = IMGFMT_START; n < IMGFMT_END; n++) {
        if (!IMGFMT_IS_HWACCEL(n))
            mp_autoconvert_add_imgfmt(c, n, 0);
    }
}

void mp_autoconvert_add_afmt(struct mp_autoconvert *c, int afmt)
{
    struct priv *p = c->f->priv;

    MP_TARRAY_APPEND(p, p->afmts, p->num_afmts, afmt);
    p->force_update = true;
}

void mp_autoconvert_add_chmap(struct mp_autoconvert *c, struct mp_chmap *chmap)
{
    struct priv *p = c->f->priv;

    mp_chmap_sel_add_map(&p->chmaps, chmap);
    p->force_update = true;
}

void mp_autoconvert_add_srate(struct mp_autoconvert *c, int rate)
{
    struct priv *p = c->f->priv;

    MP_TARRAY_APPEND(p, p->srates, p->num_srates, rate);
    // Some other API we call expects a 0-terminated sample rates array.
    MP_TARRAY_GROW(p, p->srates, p->num_srates);
    p->srates[p->num_srates] = 0;
    p->force_update = true;
}

// If this returns true, and *out==NULL, no conversion is necessary.
static bool build_image_converter(struct mp_autoconvert *c, struct mp_log *log,
                                  struct mp_image *img, struct mp_filter **f_out)
{
    struct mp_filter *f = c->f;
    struct priv *p = f->priv;

    *f_out = NULL;

    if (!p->num_imgfmts)
        return true;

    for (int n = 0; n < p->num_imgfmts; n++) {
        bool samefmt = img->params.imgfmt == p->imgfmts[n];
        bool samesubffmt = img->params.hw_subfmt == p->subfmts[n];
        if (samefmt && (samesubffmt || !p->subfmts[n])) {
            if (p->imgparams_set) {
                if (!mp_image_params_equal(&p->imgparams, &img->params))
                    break;
            }
            return true;
        }
    }

    struct mp_filter *conv = mp_filter_create(f, &convert_filter);
    mp_filter_add_pin(conv, MP_PIN_IN, "in");
    mp_filter_add_pin(conv, MP_PIN_OUT, "out");

    // 0: hw->sw download
    // 1: swscale
    // 2: sw->hw upload
    struct mp_filter *filters[3] = {0};
    bool need_sws = true;
    bool force_sws_params = false;
    struct mp_image_params imgpar = img->params;

    int *fmts = p->imgfmts;
    int num_fmts = p->num_imgfmts;
    int hwupload_fmt = 0;

    bool imgfmt_is_sw = !IMGFMT_IS_HWACCEL(img->imgfmt);

    // This should not happen. But not enough guarantee to make it an assert().
    if (imgfmt_is_sw != !img->hwctx)
        mp_warn(log, "Unexpected AVFrame/imgfmt hardware context mismatch.\n");

    bool dst_all_hw = true;
    bool dst_have_sw = false;
    for (int n = 0; n < num_fmts; n++) {
        bool is_hw = IMGFMT_IS_HWACCEL(fmts[n]);
        dst_all_hw &= is_hw;
        dst_have_sw |= !is_hw;
    }

    // Source is sw, all targets are hw -> try to upload.
    bool sw_to_hw = imgfmt_is_sw && dst_all_hw;
    // Source is hw, some targets are sw -> try to download.
    bool hw_to_sw = !imgfmt_is_sw && dst_have_sw;

    if (sw_to_hw && num_fmts > 0) {
        // We can probably use this! Very lazy and very approximate.
        struct mp_hwupload *upload = mp_hwupload_create(conv, fmts[0]);
        if (upload) {
            mp_info(log, "HW-uploading to %s\n", mp_imgfmt_to_name(fmts[0]));
            filters[2] = upload->f;
            hwupload_fmt = mp_hwupload_find_upload_format(upload, img->imgfmt);
            fmts = &hwupload_fmt;
            num_fmts = hwupload_fmt ? 1 : 0;
            hw_to_sw = false;
        }
    }

    int src_fmt = img->imgfmt;
    if (hw_to_sw) {
        mp_info(log, "HW-downloading from %s\n", mp_imgfmt_to_name(img->imgfmt));
        int res_fmt = mp_image_hw_download_get_sw_format(img);
        if (!res_fmt) {
            mp_err(log, "cannot copy surface of this format to CPU memory\n");
            goto fail;
        }
        struct mp_hwdownload *hwd = mp_hwdownload_create(conv);
        if (hwd) {
            filters[0] = hwd->f;
            src_fmt = res_fmt;
            // Downloading from hw will obviously change the parameters. We
            // stupidly don't know the result parameters, but if it's
            // sufficiently sane, it will only do the following.
            imgpar.imgfmt = src_fmt;
            imgpar.hw_subfmt = 0;
            // Try to compensate for in-sane cases.
            mp_image_params_guess_csp(&imgpar);
        }
    }

    if (p->imgparams_set) {
        force_sws_params |= !mp_image_params_equal(&imgpar, &p->imgparams);
        need_sws |= force_sws_params;
    }

    if (need_sws) {
        // Create a new conversion filter.
        struct mp_sws_filter *sws = mp_sws_filter_create(conv);
        if (!sws) {
            mp_err(log, "error creating conversion filter\n");
            goto fail;
        }

        sws->force_scaler = c->force_scaler;

        int out = mp_sws_find_best_out_format(sws, src_fmt, fmts, num_fmts);
        if (!out) {
            mp_err(log, "can't find video conversion for %s\n",
                   mp_imgfmt_to_name(src_fmt));
            goto fail;
        }

        if (out == src_fmt && !force_sws_params) {
            // Can happen if hwupload goes to same format.
            talloc_free(sws->f);
        } else {
            sws->out_format = out;
            sws->out_params = p->imgparams;
            sws->use_out_params = force_sws_params;
            mp_info(log, "Converting %s -> %s\n", mp_imgfmt_to_name(src_fmt),
                    mp_imgfmt_to_name(sws->out_format));
            filters[1] = sws->f;
        }
    }

    mp_chain_filters(conv->ppins[0], conv->ppins[1], filters, 3);

    *f_out = conv;
    return true;

fail:
    talloc_free(conv);
    return false;
}

bool mp_autoconvert_probe_input_video(struct mp_autoconvert *c,
                                      struct mp_image *img)
{
    struct mp_filter *conv = NULL;
    bool res = build_image_converter(c, mp_null_log, img, &conv);
    talloc_free(conv);
    return res;
}

static void handle_video_frame(struct mp_filter *f)
{
    struct priv *p = f->priv;

    struct mp_image *img = p->sub.frame.data;

    if (p->force_update)
        p->in_imgfmt = p->in_subfmt = 0;

    if (img->imgfmt == p->in_imgfmt && img->params.hw_subfmt == p->in_subfmt) {
        mp_subfilter_continue(&p->sub);
        return;
    }

    if (!mp_subfilter_drain_destroy(&p->sub)) {
        p->in_imgfmt = p->in_subfmt = 0;
        return;
    }

    p->in_imgfmt = img->params.imgfmt;
    p->in_subfmt = img->params.hw_subfmt;
    p->force_update = false;

    struct mp_filter *conv = NULL;
    if (build_image_converter(&p->public, p->log, img, &conv)) {
        p->sub.filter = conv;
        mp_subfilter_continue(&p->sub);
    } else {
        mp_filter_internal_mark_failed(f);
    }
}

static void handle_audio_frame(struct mp_filter *f)
{
    struct priv *p = f->priv;

    struct mp_aframe *aframe = p->sub.frame.data;

    int afmt = mp_aframe_get_format(aframe);
    int srate = mp_aframe_get_rate(aframe);
    struct mp_chmap chmap = {0};
    mp_aframe_get_chmap(aframe, &chmap);

    if (p->resampling_forced && !af_fmt_is_pcm(afmt)) {
        MP_WARN(p, "ignoring request to resample non-PCM audio for speed change\n");
        p->resampling_forced = false;
    }

    bool format_change = afmt != p->in_afmt ||
                         srate != p->in_srate ||
                         !mp_chmap_equals(&chmap, &p->in_chmap) ||
                         p->force_update;

    if (!format_change && (!p->resampling_forced || p->sub.filter))
        goto cont;

    if (!mp_subfilter_drain_destroy(&p->sub))
        return;

    if (format_change && p->public.on_audio_format_change) {
        if (p->format_change_blocked)
            return;

        if (!p->format_change_cont) {
            p->format_change_blocked = true;
            p->public.
                on_audio_format_change(p->public.on_audio_format_change_opaque);
            return;
        }
        p->format_change_cont = false;
    }

    p->in_afmt = afmt;
    p->in_srate = srate;
    p->in_chmap = chmap;
    p->force_update = false;

    int out_afmt = 0;
    int best_score = 0;
    for (int n = 0; n < p->num_afmts; n++) {
        int score = af_format_conversion_score(p->afmts[n], afmt);
        if (!out_afmt || score > best_score) {
            best_score = score;
            out_afmt = p->afmts[n];
        }
    }
    if (!out_afmt)
        out_afmt = afmt;

    // (The p->srates array is 0-terminated already.)
    int out_srate = af_select_best_samplerate(srate, p->srates);
    if (out_srate <= 0)
        out_srate = p->num_srates ? p->srates[0] : srate;

    struct mp_chmap out_chmap = chmap;
    if (p->chmaps.num_chmaps) {
        if (!mp_chmap_sel_adjust(&p->chmaps, &out_chmap))
            out_chmap = p->chmaps.chmaps[0]; // violently force fallback
    }

    if (out_afmt == p->in_afmt && out_srate == p->in_srate &&
        mp_chmap_equals(&out_chmap, &p->in_chmap) && !p->resampling_forced)
    {
        goto cont;
    }

    MP_VERBOSE(p, "inserting resampler\n");

    struct mp_swresample *s = mp_swresample_create(f, NULL);
    if (!s)
        abort();

    s->out_format = out_afmt;
    s->out_rate = out_srate;
    s->out_channels = out_chmap;

    p->sub.filter = s->f;

cont:

    if (p->sub.filter) {
        struct mp_filter_command cmd = {
            .type = MP_FILTER_COMMAND_SET_SPEED_RESAMPLE,
            .speed = p->audio_speed,
        };
        mp_filter_command(p->sub.filter, &cmd);
    }

    mp_subfilter_continue(&p->sub);
}

static void process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_subfilter_read(&p->sub))
        return;

    if (p->sub.frame.type == MP_FRAME_VIDEO) {
        handle_video_frame(f);
        return;
    }

    if (p->sub.frame.type == MP_FRAME_AUDIO) {
        handle_audio_frame(f);
        return;
    }

    mp_subfilter_continue(&p->sub);
}

void mp_autoconvert_format_change_continue(struct mp_autoconvert *c)
{
    struct priv *p = c->f->priv;

    if (p->format_change_blocked) {
        p->format_change_cont = true;
        p->format_change_blocked = false;
        mp_filter_wakeup(c->f);
    }
}

static bool command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct priv *p = f->priv;

    if (cmd->type == MP_FILTER_COMMAND_SET_SPEED_RESAMPLE) {
        p->audio_speed = cmd->speed;
        // If we needed resampling once, keep forcing resampling, as it might be
        // quickly changing between 1.0 and other values for A/V compensation.
        if (p->audio_speed != 1.0)
            p->resampling_forced = true;
        return true;
    }

    if (cmd->type == MP_FILTER_COMMAND_IS_ACTIVE) {
        cmd->is_active = !!p->sub.filter;
        return true;
    }

    return false;
}

static void reset(struct mp_filter *f)
{
    struct priv *p = f->priv;

    mp_subfilter_reset(&p->sub);

    p->format_change_cont = false;
    p->format_change_blocked = false;
}

static void destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;

    mp_subfilter_reset(&p->sub);
    TA_FREEP(&p->sub.filter);
}

static const struct mp_filter_info autoconvert_filter = {
    .name = "autoconvert",
    .priv_size = sizeof(struct priv),
    .process = process,
    .command = command,
    .reset = reset,
    .destroy = destroy,
};

struct mp_autoconvert *mp_autoconvert_create(struct mp_filter *parent)
{
    struct mp_filter *f = mp_filter_create(parent, &autoconvert_filter);
    if (!f)
        return NULL;

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->public.f = f;
    p->log = f->log;
    p->audio_speed = 1.0;
    p->sub.in = f->ppins[0];
    p->sub.out = f->ppins[1];

    return &p->public;
}
