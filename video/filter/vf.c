/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <libavutil/common.h>
#include <libavutil/mem.h>

#include "config.h"

#include "common/global.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "options/m_config.h"

#include "options/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"
#include "vf.h"

#include "video/memcpy_pic.h"

extern const vf_info_t vf_info_crop;
extern const vf_info_t vf_info_expand;
extern const vf_info_t vf_info_pp;
extern const vf_info_t vf_info_scale;
extern const vf_info_t vf_info_format;
extern const vf_info_t vf_info_noformat;
extern const vf_info_t vf_info_flip;
extern const vf_info_t vf_info_rotate;
extern const vf_info_t vf_info_mirror;
extern const vf_info_t vf_info_noise;
extern const vf_info_t vf_info_eq;
extern const vf_info_t vf_info_gradfun;
extern const vf_info_t vf_info_unsharp;
extern const vf_info_t vf_info_swapuv;
extern const vf_info_t vf_info_hqdn3d;
extern const vf_info_t vf_info_ilpack;
extern const vf_info_t vf_info_dsize;
extern const vf_info_t vf_info_softpulldown;
extern const vf_info_t vf_info_pullup;
extern const vf_info_t vf_info_delogo;
extern const vf_info_t vf_info_phase;
extern const vf_info_t vf_info_divtc;
extern const vf_info_t vf_info_screenshot;
extern const vf_info_t vf_info_sub;
extern const vf_info_t vf_info_yadif;
extern const vf_info_t vf_info_stereo3d;
extern const vf_info_t vf_info_dlopen;
extern const vf_info_t vf_info_lavfi;
extern const vf_info_t vf_info_vaapi;
extern const vf_info_t vf_info_vapoursynth;

// list of available filters:
static const vf_info_t *const filter_list[] = {
    &vf_info_crop,
    &vf_info_expand,
    &vf_info_scale,
    &vf_info_format,
    &vf_info_noformat,
    &vf_info_flip,
    &vf_info_rotate,
    &vf_info_mirror,

#if HAVE_LIBPOSTPROC
    &vf_info_pp,
#endif
#if HAVE_LIBAVFILTER
    &vf_info_lavfi,
#endif

    &vf_info_screenshot,

    &vf_info_noise,
    &vf_info_eq,
    &vf_info_gradfun,
    &vf_info_unsharp,
    &vf_info_swapuv,
    &vf_info_hqdn3d,
    &vf_info_ilpack,
    &vf_info_dsize,
    &vf_info_softpulldown,
    &vf_info_pullup,
    &vf_info_delogo,
    &vf_info_phase,
    &vf_info_divtc,
    &vf_info_sub,
    &vf_info_yadif,
    &vf_info_stereo3d,
#if HAVE_DLOPEN
    &vf_info_dlopen,
#endif
#if HAVE_VAPOURSYNTH
    &vf_info_vapoursynth,
#endif
#if HAVE_VAAPI_VPP
    &vf_info_vaapi,
#endif
    NULL
};

static bool get_desc(struct m_obj_desc *dst, int index)
{
    if (index >= MP_ARRAY_SIZE(filter_list) - 1)
        return false;
    const vf_info_t *vf = filter_list[index];
    *dst = (struct m_obj_desc) {
        .name = vf->name,
        .description = vf->description,
        .priv_size = vf->priv_size,
        .priv_defaults = vf->priv_defaults,
        .options = vf->options,
        .p = vf,
        .print_help = vf->print_help,
    };
    return true;
}

// For the vf option
const struct m_obj_list vf_obj_list = {
    .get_desc = get_desc,
    .description = "video filters",
};

// Try the cmd on each filter (starting with the first), and stop at the first
// filter which does not return CONTROL_UNKNOWN for it.
int vf_control_any(struct vf_chain *c, int cmd, void *arg)
{
    for (struct vf_instance *cur = c->first; cur; cur = cur->next) {
        if (cur->control) {
            int r = cur->control(cur, cmd, arg);
            if (r != CONTROL_UNKNOWN)
                return r;
        }
    }
    return CONTROL_UNKNOWN;
}

static void vf_fix_img_params(struct mp_image *img, struct mp_image_params *p)
{
    // Filters must absolutely set these correctly.
    assert(img->w == p->w && img->h == p->h);
    assert(img->imgfmt == p->imgfmt);
    // Too many things don't set this correctly.
    // If --colormatrix is used, decoder and filter chain disagree too.
    // In general, it's probably more convenient to force these here,
    // instead of requiring filters to set these correctly.
    img->colorspace = p->colorspace;
    img->levels = p->colorlevels;
    img->chroma_location = p->chroma_location;
    mp_image_set_display_size(img, p->d_w, p->d_h);
}

// Get a new image for filter output, with size and pixel format according to
// the last vf_config call.
struct mp_image *vf_alloc_out_image(struct vf_instance *vf)
{
    struct mp_image_params *p = &vf->fmt_out;
    assert(p->imgfmt);
    struct mp_image *img = mp_image_pool_get(vf->out_pool, p->imgfmt, p->w, p->h);
    vf_fix_img_params(img, p);
    return img;
}

void vf_make_out_image_writeable(struct vf_instance *vf, struct mp_image *img)
{
    struct mp_image_params *p = &vf->fmt_out;
    assert(p->imgfmt);
    assert(p->imgfmt == img->imgfmt);
    assert(p->w == img->w && p->h == img->h);
    mp_image_pool_make_writeable(vf->out_pool, img);
}

//============================================================================

// The default callback assumes all formats are passed through.
static int vf_default_query_format(struct vf_instance *vf, unsigned int fmt)
{
    return vf_next_query_format(vf, fmt);
}

static void print_fmt(struct mp_log *log, int msglevel, struct mp_image_params *p)
{
    if (p && p->imgfmt) {
        mp_msg(log, msglevel, "%dx%d", p->w, p->h);
        if (p->w != p->d_w || p->h != p->d_h)
            mp_msg(log, msglevel, "->%dx%d", p->d_w, p->d_h);
        mp_msg(log, msglevel, " %s", mp_imgfmt_to_name(p->imgfmt));
        mp_msg(log, msglevel, " %s/%s", mp_csp_names[p->colorspace],
                   mp_csp_levels_names[p->colorlevels]);
    } else {
        mp_msg(log, msglevel, "???");
    }
}

void vf_print_filter_chain(struct vf_chain *c, int msglevel)
{
    if (!mp_msg_test(c->log, msglevel))
        return;

    for (vf_instance_t *f = c->first; f; f = f->next) {
        mp_msg(c->log, msglevel, " [%s] ", f->info->name);
        print_fmt(c->log, msglevel, &f->fmt_out);
        mp_msg(c->log, msglevel, "\n");
    }
}

static struct vf_instance *vf_open(struct vf_chain *c, const char *name,
                                   char **args)
{
    struct m_obj_desc desc;
    if (!m_obj_list_find(&desc, &vf_obj_list, bstr0(name))) {
        MP_ERR(c, "Couldn't find video filter '%s'.\n", name);
        return NULL;
    }
    vf_instance_t *vf = talloc_zero(NULL, struct vf_instance);
    *vf = (vf_instance_t) {
        .info = desc.p,
        .log = mp_log_new(vf, c->log, name),
        .hwdec = c->hwdec,
        .query_format = vf_default_query_format,
        .out_pool = talloc_steal(vf, mp_image_pool_new(16)),
    };
    struct m_config *config = m_config_from_obj_desc(vf, vf->log, &desc);
    if (m_config_apply_defaults(config, name, c->opts->vf_defs) < 0)
        goto error;
    if (m_config_set_obj_params(config, args) < 0)
        goto error;
    vf->priv = config->optstruct;
    int retcode = vf->info->open(vf);
    if (retcode < 1)
        goto error;
    return vf;

error:
    MP_ERR(c, "Creating filter '%s' failed.\n", name);
    talloc_free(vf);
    return NULL;
}

static vf_instance_t *vf_open_filter(struct vf_chain *c, const char *name,
                                     char **args)
{
    int i, l = 0;
    for (i = 0; args && args[2 * i]; i++)
        l += 1 + strlen(args[2 * i]) + 1 + strlen(args[2 * i + 1]);
    l += strlen(name);
    char str[l + 1];
    char *p = str;
    p += sprintf(str, "%s", name);
    for (i = 0; args && args[2 * i]; i++)
        p += sprintf(p, " %s=%s", args[2 * i], args[2 * i + 1]);
    MP_INFO(c, "Opening video filter: [%s]\n", str);
    return vf_open(c, name, args);
}

struct vf_instance *vf_append_filter(struct vf_chain *c, const char *name,
                                     char **args)
{
    struct vf_instance *vf = vf_open_filter(c, name, args);
    if (vf) {
        // Insert it before the last filter, which is the "out" pseudo-filter
        // (But after the "in" pseudo-filter)
        struct vf_instance **pprev = &c->first->next;
        while (*pprev && (*pprev)->next)
            pprev = &(*pprev)->next;
        vf->next = *pprev ? *pprev : NULL;
        *pprev = vf;
    }
    return vf;
}

int vf_append_filter_list(struct vf_chain *c, struct m_obj_settings *list)
{
    for (int n = 0; list && list[n].name; n++) {
        struct vf_instance *vf =
            vf_append_filter(c, list[n].name, list[n].attribs);
        if (vf) {
            if (list[n].label)
                vf->label = talloc_strdup(vf, list[n].label);
        }
    }
    return 0;
}

// Used by filters to add a filtered frame to the output queue.
// Ownership of img is transferred from caller to the filter chain.
void vf_add_output_frame(struct vf_instance *vf, struct mp_image *img)
{
    if (img) {
        vf_fix_img_params(img, &vf->fmt_out);
        MP_TARRAY_APPEND(vf, vf->out_queued, vf->num_out_queued, img);
    }
}

static struct mp_image *vf_dequeue_output_frame(struct vf_instance *vf)
{
    struct mp_image *res = NULL;
    if (vf->num_out_queued) {
        res = vf->out_queued[0];
        MP_TARRAY_REMOVE_AT(vf->out_queued, vf->num_out_queued, 0);
    }
    return res;
}

static int vf_do_filter(struct vf_instance *vf, struct mp_image *img)
{
    assert(vf->fmt_in.imgfmt);
    vf_fix_img_params(img, &vf->fmt_in);

    if (vf->filter_ext) {
        return vf->filter_ext(vf, img);
    } else {
        if (vf->filter)
            img = vf->filter(vf, img);
        vf_add_output_frame(vf, img);
        return 0;
    }
}

// Input a frame into the filter chain. Ownership of img is transferred.
// Return >= 0 on success, < 0 on failure (even if output frames were produced)
int vf_filter_frame(struct vf_chain *c, struct mp_image *img)
{
    if (c->initialized < 1) {
        talloc_free(img);
        return -1;
    }
    return vf_do_filter(c->first, img);
}

// Output the next queued image (if any) from the full filter chain.
struct mp_image *vf_output_queued_frame(struct vf_chain *c)
{
    if (c->initialized < 1)
        return NULL;
    while (1) {
        struct vf_instance *last = NULL;
        for (struct vf_instance * cur = c->first; cur; cur = cur->next) {
            if (cur->num_out_queued)
                last = cur;
        }
        if (!last)
            return NULL;
        struct mp_image *img = vf_dequeue_output_frame(last);
        if (!last->next)
            return img;
        vf_do_filter(last->next, img);
    }
}

static void vf_forget_frames(struct vf_instance *vf)
{
    for (int n = 0; n < vf->num_out_queued; n++)
        talloc_free(vf->out_queued[n]);
    vf->num_out_queued = 0;
}

void vf_seek_reset(struct vf_chain *c)
{
    for (struct vf_instance *cur = c->first; cur; cur = cur->next) {
        if (cur->control)
            cur->control(cur, VFCTRL_SEEK_RESET, NULL);
        vf_forget_frames(cur);
    }
}

int vf_next_config(struct vf_instance *vf,
                   int width, int height, int d_width, int d_height,
                   unsigned int voflags, unsigned int outfmt)
{
    vf->fmt_out = (struct mp_image_params) {
        .imgfmt = outfmt,
        .w = width,
        .h = height,
        .d_w = d_width,
        .d_h = d_height,
        .colorspace = vf->fmt_in.colorspace,
        .colorlevels = vf->fmt_in.colorlevels,
        .chroma_location = vf->fmt_in.chroma_location,
        .outputlevels = vf->fmt_in.outputlevels,
    };
    return 1;
}

int vf_next_query_format(struct vf_instance *vf, unsigned int fmt)
{
    return fmt >= IMGFMT_START && fmt < IMGFMT_END
           ? vf->last_outfmts[fmt - IMGFMT_START] : 0;
}

// Mark accepted input formats in fmts[]. Note that ->query_format will
// typically (but not always) call vf_next_query_format() to check whether
// an output format is supported.
static void query_formats(uint8_t *fmts, struct vf_instance *vf)
{
    for (int n = IMGFMT_START; n < IMGFMT_END; n++)
        fmts[n - IMGFMT_START] = vf->query_format(vf, n);
}

static bool is_conv_filter(struct vf_instance *vf)
{
    return vf && strcmp(vf->info->name, "scale") == 0;
}

static void update_formats(struct vf_chain *c, struct vf_instance *vf,
                           uint8_t *fmts)
{
    if (vf->next)
        update_formats(c, vf->next, vf->last_outfmts);
    query_formats(fmts, vf);
    bool has_in = false, has_out = false;
    for (int n = IMGFMT_START; n < IMGFMT_END; n++) {
        has_in |= !!fmts[n - IMGFMT_START];
        has_out |= !!vf->last_outfmts[n - IMGFMT_START];
    }
    if (has_out && !has_in && !is_conv_filter(vf) &&
        !is_conv_filter(vf->next))
    {
        // If there are output formats, but no input formats (meaning the
        // filters after vf work, but vf can't output any format the filters
        // after it accept), try to insert a conversion filter.
        MP_INFO(c, "Using conversion filter.\n");
        struct vf_instance *conv = vf_open(c, "scale", NULL);
        if (conv) {
            conv->next = vf->next;
            vf->next = conv;
            update_formats(c, conv, vf->last_outfmts);
            query_formats(fmts, vf);
        }
    }
    for (int n = IMGFMT_START; n < IMGFMT_END; n++)
        has_in |= !!fmts[n - IMGFMT_START];
    if (!has_in) {
        // Pretend all out formats work. All this does it getting better
        // error messages in some cases, so we can configure all filter
        // until it fails, which will be visible in vf_print_filter_chain().
        for (int n = IMGFMT_START; n < IMGFMT_END; n++)
            vf->last_outfmts[n - IMGFMT_START] = VFCAP_CSP_SUPPORTED;
        query_formats(fmts, vf);
    }
}

static int vf_reconfig_wrapper(struct vf_instance *vf,
                               const struct mp_image_params *p)
{
    vf_forget_frames(vf);
    if (vf->out_pool)
        mp_image_pool_clear(vf->out_pool);

    if (!vf->query_format(vf, p->imgfmt))
        return -2;

    vf->fmt_out = vf->fmt_in = *p;

    int r;
    if (vf->reconfig) {
        r = vf->reconfig(vf, &vf->fmt_in, &vf->fmt_out);
    } else if (vf->config) {
        r = vf->config(vf, p->w, p->h, p->d_w, p->d_h, 0, p->imgfmt) ? 0 : -1;
    } else {
        r = 0;
    }

    if (!mp_image_params_equals(&vf->fmt_in, p))
        r = -2;

    // Fix csp in case of pixel format change
    if (r >= 0)
        mp_image_params_guess_csp(&vf->fmt_out);

    return r;
}

int vf_reconfig(struct vf_chain *c, const struct mp_image_params *params)
{
    struct mp_image_params cur = *params;
    int r = 0;
    c->first->fmt_in = *params;
    uint8_t unused[IMGFMT_END - IMGFMT_START];
    update_formats(c, c->first, unused);
    for (struct vf_instance *vf = c->first; vf; vf = vf->next) {
        r = vf_reconfig_wrapper(vf, &cur);
        if (r < 0)
            break;
        cur = vf->fmt_out;
    }
    if (r >= 0)
        c->output_params = cur;
    c->initialized = r < 0 ? -1 : 1;
    int loglevel = r < 0 ? MSGL_WARN : MSGL_V;
    if (r == -2)
        MP_ERR(c, "Image formats incompatible.\n");
    mp_msg(c->log, loglevel, "Video filter chain:\n");
    vf_print_filter_chain(c, loglevel);
    return r;
}

struct vf_instance *vf_find_by_label(struct vf_chain *c, const char *label)
{
    struct vf_instance *vf = c->first;
    while (vf) {
        if (vf->label && label && strcmp(vf->label, label) == 0)
            return vf;
        vf = vf->next;
    }
    return NULL;
}

static void vf_uninit_filter(vf_instance_t *vf)
{
    if (vf->uninit)
        vf->uninit(vf);
    vf_forget_frames(vf);
    talloc_free(vf);
}

static int input_query_format(struct vf_instance *vf, unsigned int fmt)
{
    // Setting fmt_in is guaranteed by vf_reconfig().
    if (fmt == vf->fmt_in.imgfmt)
        return vf_next_query_format(vf, fmt);
    return 0;
}

static int output_query_format(struct vf_instance *vf, unsigned int fmt)
{
    struct vf_chain *c = (void *)vf->priv;
    if (fmt >= IMGFMT_START && fmt < IMGFMT_END)
        return c->allowed_output_formats[fmt - IMGFMT_START];
    return 0;
}

struct vf_chain *vf_new(struct mpv_global *global)
{
    struct vf_chain *c = talloc_ptrtype(NULL, c);
    *c = (struct vf_chain){
        .opts = global->opts,
        .log = mp_log_new(c, global->log, "!vf"),
        .global = global,
    };
    static const struct vf_info in = { .name = "in" };
    c->first = talloc(c, struct vf_instance);
    *c->first = (struct vf_instance) {
        .info = &in,
        .query_format = input_query_format,
    };
    static const struct vf_info out = { .name = "out" };
    c->first->next = talloc(c, struct vf_instance);
    *c->first->next = (struct vf_instance) {
        .info = &out,
        .query_format = output_query_format,
        .priv = (void *)c,
    };
    return c;
}

void vf_destroy(struct vf_chain *c)
{
    if (!c)
        return;
    while (c->first) {
        vf_instance_t *vf = c->first;
        c->first = vf->next;
        vf_uninit_filter(vf);
    }
    talloc_free(c);
}

// When changing the size of an image that had old_w/old_h with
// DAR *d_width/*d_height to the new size new_w/new_h, adjust
// *d_width/*d_height such that the new image has the same pixel aspect ratio.
void vf_rescale_dsize(int *d_width, int *d_height, int old_w, int old_h,
                      int new_w, int new_h)
{
    *d_width  = *d_width  * new_w / old_w;
    *d_height = *d_height * new_h / old_h;
}

// Set *d_width/*d_height to display aspect ratio with the givem source size
void vf_set_dar(int *d_w, int *d_h, int w, int h, double dar)
{
    *d_w = w;
    *d_h = h;
    if (dar > 0.01) {
        *d_w = h * dar + 0.5;
        // we don't like horizontal downscale
        if (*d_w < w) {
            *d_w = w;
            *d_h = w / dar + 0.5;
        }
    }
}

void vf_detc_init_pts_buf(struct vf_detc_pts_buf *p)
{
    p->inpts_prev = MP_NOPTS_VALUE;
    p->outpts_prev = MP_NOPTS_VALUE;
    p->lastdelta = 0;
}

static double vf_detc_adjust_pts_internal(struct vf_detc_pts_buf *p,
                                          double pts, bool reset_pattern,
                                          bool skip_frame, double delta,
                                          double boundfactor_minus,
                                          double increasefactor,
                                          double boundfactor_plus)
{
    double newpts;

    if (pts == MP_NOPTS_VALUE)
        return pts;

    if (delta <= 0) {
        if (p->inpts_prev == MP_NOPTS_VALUE)
            delta = 0;
        else if (pts == p->inpts_prev)
            delta = p->lastdelta;
        else
            delta = pts - p->inpts_prev;
    }
    p->inpts_prev = pts;
    p->lastdelta = delta;

    if (skip_frame)
        return MP_NOPTS_VALUE;

    /* detect bogus deltas and then passthru pts (possibly caused by seeking,
     * or bad input) */
    if (p->outpts_prev == MP_NOPTS_VALUE || reset_pattern || delta <= 0.0 ||
            delta >= 0.5)
        newpts = pts;
    else {
        // turn 5 frames into 4
        newpts = p->outpts_prev + delta * increasefactor;

        // bound to input pts in a sensible way; these numbers come because we
        // map frames the following way when ivtc'ing:
        // 0/30 -> 0/24   diff=0
        // 1/30 -> 1/24   diff=1/120
        // 2/30 -> -
        // 3/30 -> 2/24   diff=-1/60
        // 4/30 -> 3/24   diff=-1/120
        if (newpts < pts - delta * boundfactor_minus)
            newpts = pts - delta * boundfactor_minus;
        if (newpts > pts + delta * boundfactor_plus)
            newpts = pts + delta * boundfactor_plus;
        if (newpts < p->outpts_prev)
            newpts = p->outpts_prev;  // damage control
    }
    p->outpts_prev = newpts;

    return newpts;
}

double vf_detc_adjust_pts(struct vf_detc_pts_buf *p, double pts,
                          bool reset_pattern, bool skip_frame)
{
    // standard telecine (see above)
    return vf_detc_adjust_pts_internal(p, pts, reset_pattern, skip_frame,
                                       0, 0.5, 1.25, 0.25);
}

double vf_softpulldown_adjust_pts(struct vf_detc_pts_buf *p, double pts,
                                  bool reset_pattern, bool skip_frame,
                                  int last_frame_duration)
{
    // for the softpulldown filter we get:
    // 0/60 -> 0/30
    // 2/60 -> 1/30
    // 5/60 -> 2/30
    // 7/60 -> 3/30, 4/30
    return vf_detc_adjust_pts_internal(p, pts, reset_pattern, skip_frame,
                                       0, 1.0 / last_frame_duration,
                                       2.0 / last_frame_duration,
                                       1.0 / last_frame_duration);
}
