#include "libmpv/render_gl.h"
#include "libmpv.h"
#include "sub/osd.h"
#include "video/sws_utils.h"

struct priv {
    struct libmpv_gpu_context *context;

    struct mp_sws_context *sws;
    struct osd_state *osd;

    struct mp_image_params src_params, dst_params;
    struct mp_rect src_rc, dst_rc;
    struct mp_osd_res osd_rc;
    bool anything_changed;
};

static int init(struct render_backend *ctx, mpv_render_param *params)
{
    ctx->priv = talloc_zero(NULL, struct priv);
    struct priv *p = ctx->priv;

    char *api = get_mpv_render_param(params, MPV_RENDER_PARAM_API_TYPE, NULL);
    if (!api)
        return MPV_ERROR_INVALID_PARAMETER;

    if (strcmp(api, MPV_RENDER_API_TYPE_SW) != 0)
        return MPV_ERROR_NOT_IMPLEMENTED;

    p->sws = mp_sws_alloc(p);
    mp_sws_enable_cmdline_opts(p->sws, ctx->global);

    p->anything_changed = true;

    return 0;
}

static bool check_format(struct render_backend *ctx, int imgfmt)
{
    struct priv *p = ctx->priv;

    // Note: we don't know the output format yet. Using an arbitrary supported
    //       format is fine, because we know that any supported input format can
    //       be converted to any supported output format.
    return mp_sws_supports_formats(p->sws, IMGFMT_RGB0, imgfmt);
}

static int set_parameter(mp_unused struct render_backend *ctx,
                         mp_unused mpv_render_param param)
{
    return MPV_ERROR_NOT_IMPLEMENTED;
}

static void reconfig(struct render_backend *ctx, struct mp_image_params *params)
{
    struct priv *p = ctx->priv;

    p->src_params = *params;
    p->anything_changed = true;
}

static void reset(mp_unused struct render_backend *ctx)
{
    // stateless
}

static void update_external(struct render_backend *ctx, struct vo *vo)
{
    struct priv *p = ctx->priv;

    p->osd = vo ? vo->osd : NULL;
}

static void resize(struct render_backend *ctx, struct mp_rect *src,
                   struct mp_rect *dst, struct mp_osd_res *osd)
{
    struct priv *p = ctx->priv;

    p->src_rc = *src;
    p->dst_rc = *dst;
    p->osd_rc = *osd;
    p->anything_changed = true;
}

static int get_target_size(mp_unused struct render_backend *ctx, mpv_render_param *params,
                           int *out_w, int *out_h)
{
    int *sz = get_mpv_render_param(params, MPV_RENDER_PARAM_SW_SIZE, NULL);
    if (!sz)
        return MPV_ERROR_INVALID_PARAMETER;

    *out_w = sz[0];
    *out_h = sz[1];
    return 0;
}

static int render(struct render_backend *ctx, mpv_render_param *params,
                  struct vo_frame *frame)
{
    struct priv *p = ctx->priv;

    int *sz = get_mpv_render_param(params, MPV_RENDER_PARAM_SW_SIZE, NULL);
    char *fmt = get_mpv_render_param(params, MPV_RENDER_PARAM_SW_FORMAT, NULL);
    size_t *stride = get_mpv_render_param(params, MPV_RENDER_PARAM_SW_STRIDE, NULL);
    void *ptr = get_mpv_render_param(params, MPV_RENDER_PARAM_SW_POINTER, NULL);

    if (!sz || !fmt || !stride || !ptr)
        return MPV_ERROR_INVALID_PARAMETER;

    char *prev_fmt = mp_imgfmt_to_name(p->dst_params.imgfmt);
    if (strcmp(prev_fmt, fmt) != 0)
        p->anything_changed = true;

    if (sz[0] != p->dst_params.w || sz[1] != p->dst_params.h)
        p->anything_changed = true;

    if (p->anything_changed) {
        p->dst_params = (struct mp_image_params){
            .imgfmt = mp_imgfmt_from_name(bstr0(fmt)),
            .w = sz[0],
            .h = sz[1],
        };

        // Exclude "problematic" formats. In particular, reject multi-plane and
        // hw formats. Exclude non-byte-aligned formats for easier stride
        // checking.
        struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(p->dst_params.imgfmt);
        if (!(desc.flags & MP_IMGFLAG_COLOR_RGB) ||
            !(desc.flags & (MP_IMGFLAG_TYPE_UINT | MP_IMGFLAG_TYPE_FLOAT)) ||
            (desc.flags & MP_IMGFLAG_TYPE_PAL8) ||
            !(desc.flags & MP_IMGFLAG_BYTE_ALIGNED) ||
            desc.num_planes != 1)
            return MPV_ERROR_UNSUPPORTED;

        mp_image_params_guess_csp(&p->dst_params);

        // Can be unset if rendering before any video was loaded.
        if (p->src_params.imgfmt) {
            p->sws->src = p->src_params;
            p->sws->src.w = mp_rect_w(p->src_rc);
            p->sws->src.h = mp_rect_h(p->src_rc);

            p->sws->dst = p->dst_params;
            p->sws->dst.w = mp_rect_w(p->dst_rc);
            p->sws->dst.h = mp_rect_h(p->dst_rc);

            if (mp_sws_reinit(p->sws) < 0)
                return MPV_ERROR_UNSUPPORTED; // probably
        }

        p->anything_changed = false;
    }

    struct mp_image wrap_img = {0};
    mp_image_set_params(&wrap_img, &p->dst_params);

    size_t bpp = wrap_img.fmt.bpp[0] / 8;
    if (!bpp || bpp * wrap_img.w > *stride || *stride % bpp)
        return MPV_ERROR_INVALID_PARAMETER;

    wrap_img.planes[0] = ptr;
    wrap_img.stride[0] = *stride;

    struct mp_image *img = frame->current;
    if (img) {
        assert(p->src_params.imgfmt);

        mp_image_clear_rc_inv(&wrap_img, p->dst_rc);

        struct mp_image src = *img;
        struct mp_rect src_rc = p->src_rc;
        src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, src.fmt.align_x);
        src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, src.fmt.align_y);
        mp_image_crop_rc(&src, src_rc);

        struct mp_image dst = wrap_img;
        mp_image_crop_rc(&dst, p->dst_rc);

        if (mp_sws_scale(p->sws, &dst, &src) < 0) {
            mp_image_clear(&wrap_img, 0, 0, wrap_img.w, wrap_img.h);
            return MPV_ERROR_GENERIC;
        }
    } else {
        mp_image_clear(&wrap_img, 0, 0, wrap_img.w, wrap_img.h);
    }

    if (p->osd)
        osd_draw_on_image(p->osd, p->osd_rc, img ? img->pts : 0, 0, &wrap_img);

    return 0;
}

static void destroy(mp_unused struct render_backend *ctx)
{
    // nop
}

const struct render_backend_fns render_backend_sw = {
    .init = init,
    .check_format = check_format,
    .set_parameter = set_parameter,
    .reconfig = reconfig,
    .reset = reset,
    .update_external = update_external,
    .resize = resize,
    .get_target_size = get_target_size,
    .render = render,
    .destroy = destroy,
};
