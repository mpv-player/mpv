#include "libmpv_gpu_next.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "common/msg.h"
#include "config.h"
#include "libplacebo/config.h"
#include "libplacebo/gpu.h"
#include "libplacebo/opengl.h"
#include "mpv/client.h"
#include "mpv/render.h"
#include "mpv/render_gl.h"
#include "ta/ta_talloc.h"
#include "video/out/gpu/ra.h"
#include "video/out/placebo/ra_pl.h"
#include "video/out/libmpv.h"
#include "video/out/vo.h"
#include "video/hwdec.h"

#include "video.h" // For pl_video engine

// ============================================================================
// GL Context Implementation (Ported from fork's context.c)
// ============================================================================

#if HAVE_GL && defined(PL_HAVE_OPENGL)

struct gl_priv {
    pl_log pl_log;
    pl_opengl gl;
    pl_gpu gpu;
    struct ra *ra;

    // Store a persistent copy of the init params to avoid a dangling pointer.
    mpv_opengl_init_params gl_params;
};

static bool pl_callback_makecurrent_gl(void *priv)
{
    mpv_opengl_init_params *gl_params = priv;
    if (gl_params && gl_params->get_proc_address) {
        gl_params->get_proc_address(gl_params->get_proc_address_ctx, "glGetString");
        return true;
    }
    return false;
}

static void pl_callback_releasecurrent_gl(void *priv)
{
}

static void pl_log_cb(void *log_priv, enum pl_log_level level, const char *msg)
{
    struct mp_log *log = log_priv;
    mp_msg(log, MSGL_WARN, "[gpu-next:pl] %s\n", msg);
}

static int libmpv_gpu_next_init_gl(struct libmpv_gpu_next_context *ctx, mpv_render_param *params)
{
    ctx->priv = talloc_zero(NULL, struct gl_priv);
    struct gl_priv *p = ctx->priv;

    mpv_opengl_init_params *gl_params =
        get_mpv_render_param(params, MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, NULL);
    if (!gl_params || !gl_params->get_proc_address)
        return MPV_ERROR_INVALID_PARAMETER;

    // Make a persistent copy of the params struct's contents.
    p->gl_params = *gl_params;

    // Setup libplacebo logging
    struct pl_log_params log_params = {
        .log_level = PL_LOG_DEBUG
    };

    // Enable verbose logging if trace is enabled
    if (mp_msg_test(ctx->log, MSGL_TRACE)) {
        log_params.log_cb = pl_log_cb;
        log_params.log_priv = ctx->log;
    }

    p->pl_log = pl_log_create(PL_API_VER, &log_params);
    p->gl = pl_opengl_create(p->pl_log, pl_opengl_params(
        .get_proc_addr_ex = (pl_voidfunc_t (*)(void*, const char*))gl_params->get_proc_address,
        .proc_ctx = gl_params->get_proc_address_ctx,
        .make_current = pl_callback_makecurrent_gl,
        .release_current = pl_callback_releasecurrent_gl,
        .priv = &p->gl_params // Pass the ADDRESS of our persistent copy
    ));

    if (!p->gl) {
        MP_ERR(ctx, "Failed to create libplacebo OpenGL context.\n");
        pl_log_destroy(&p->pl_log);
        return MPV_ERROR_UNSUPPORTED;
    }
    p->gpu = p->gl->gpu;

    // Pass the libplacebo log to the RA as well.
    // NOTE: ra_create_pl in current mpv does NOT take pl_log, unlike fork.
    p->ra = ra_create_pl(p->gpu, ctx->log);
    if (!p->ra) {
        pl_opengl_destroy(&p->gl);
        pl_log_destroy(&p->pl_log);
        return MPV_ERROR_VO_INIT_FAILED;
    }

    ctx->ra = p->ra;
    ctx->gpu = p->gpu;
    return 0;
}

static int libmpv_gpu_next_wrap_fbo_gl(struct libmpv_gpu_next_context *ctx,
                    mpv_render_param *params, pl_tex *out_tex)
{
    struct gl_priv *p = ctx->priv;
    *out_tex = NULL;

    // Get the FBO from the render parameters
    mpv_opengl_fbo *fbo =
        get_mpv_render_param(params, MPV_RENDER_PARAM_OPENGL_FBO, NULL);
    if (!fbo)
        return MPV_ERROR_INVALID_PARAMETER;

    // Wrap the FBO as a libplacebo texture
    pl_tex tex = pl_opengl_wrap(p->gpu, pl_opengl_wrap_params(
        .framebuffer = fbo->fbo,
        .width = fbo->w,
        .height = fbo->h,
        .iformat = fbo->internal_format
    ));

    if (!tex) {
        MP_ERR(ctx, "Failed to wrap provided FBO as a libplacebo texture.\n");
        return MPV_ERROR_GENERIC;
    }

    *out_tex = tex;
    return 0;
}

static void libmpv_gpu_next_done_frame_gl(struct libmpv_gpu_next_context *ctx)
{
    // Nothing to do (yet), leaving the function empty.
}

static void libmpv_gpu_next_destroy_gl(struct libmpv_gpu_next_context *ctx)
{
    struct gl_priv *p = ctx->priv;
    if (!p)
        return;

    // ra_pl_destroy(&p->ra); // Standard ra does not have ra_pl_destroy, talloc_free(ra) should suffice if it's talloc'ed
    // Wait, ra is talloc'ed in ra_create_pl.
    talloc_free(p->ra);

    pl_opengl_destroy(&p->gl);
    pl_log_destroy(&p->pl_log);
}

const struct libmpv_gpu_next_context_fns libmpv_gpu_next_context_gl = {
    .api_name = MPV_RENDER_API_TYPE_OPENGL,
    .init = libmpv_gpu_next_init_gl,
    .wrap_fbo = libmpv_gpu_next_wrap_fbo_gl,
    .done_frame = libmpv_gpu_next_done_frame_gl,
    .destroy = libmpv_gpu_next_destroy_gl,
};

#endif // HAVE_GL

// ============================================================================
// Main Backend Implementation
// ============================================================================

struct priv {
    struct libmpv_gpu_next_context *context;
    struct pl_video *video_engine;
};

static const struct libmpv_gpu_next_context_fns *context_backends[] = {
#if HAVE_GL && defined(PL_HAVE_OPENGL)
    &libmpv_gpu_next_context_gl,
#endif
    NULL
};

static int init(struct render_backend *ctx, mpv_render_param *params)
{
    ctx->priv = talloc_zero(NULL, struct priv);
    struct priv *p = ctx->priv;

    char *api = get_mpv_render_param(params, MPV_RENDER_PARAM_API_TYPE, NULL);
    if (!api) {
        MP_ERR(ctx, "API type not specified.\n");
        return MPV_ERROR_INVALID_PARAMETER;
    }

    for (int n = 0; context_backends[n]; n++) {
        const struct libmpv_gpu_next_context_fns *backend = context_backends[n];
        if (strcmp(backend->api_name, api) == 0) {
            p->context = talloc_zero(p, struct libmpv_gpu_next_context);
            *p->context = (struct libmpv_gpu_next_context){
                .global = ctx->global,
                .log = mp_log_new(p, ctx->log, "gpu-next-ctx"),
                .fns = backend,
            };
            break;
        }
    }
    if (!p->context) {
        MP_ERR(ctx, "Requested API type '%s' is not supported.\n", api);
        return MPV_ERROR_NOT_IMPLEMENTED;
    }
    int err = p->context->fns->init(p->context, params);
    if (err < 0) {
        talloc_free(p->context);
        p->context = NULL;
        return err;
    }

    p->video_engine = pl_video_init(ctx->global, ctx->log, p->context->ra);
    if (!p->video_engine) {
        p->context->fns->destroy(p->context);
        talloc_free(p->context);
        return MPV_ERROR_VO_INIT_FAILED;
    }

    ctx->hwdec_devs = hwdec_devices_create();
    ctx->driver_caps = VO_CAP_ROTATE90 | VO_CAP_VFLIP;
    return 0;
}

static void destroy(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;
    if (!p) return;

    hwdec_devices_destroy(ctx->hwdec_devs);
    pl_video_uninit(&p->video_engine);
    if (p->context) {
        p->context->fns->destroy(p->context);
        talloc_free(p->context);
    }
    talloc_free(p);
    ctx->priv = NULL;
}

static int render(struct render_backend *ctx, mpv_render_param *params,
                  struct vo_frame *frame)
{
    struct priv *p = ctx->priv;
    if (!p->video_engine) return MPV_ERROR_UNINITIALIZED;

    pl_tex target_tex = NULL;
    int err = p->context->fns->wrap_fbo(p->context, params, &target_tex);
    if (err < 0) return err;
    if (!target_tex) return MPV_ERROR_GENERIC;

    pl_video_render(p->video_engine, frame, target_tex);

    // Destroy the temporary wrapper texture
    // Fork used ra_next_tex_destroy, we use pl_tex_destroy directly since we created it via pl_opengl_wrap
    pl_tex_destroy(p->context->gpu, &target_tex);

    if (p->context->fns->done_frame)
        p->context->fns->done_frame(p->context);

    return 0;
}

static void reconfig(struct render_backend *ctx, struct mp_image_params *params)
{
    struct priv *p = ctx->priv;
    if (p->video_engine)
        pl_video_reconfig(p->video_engine, params);
}

static void resize(struct render_backend *ctx, struct mp_rect *src,
                   struct mp_rect *dst, struct mp_osd_res *osd)
{
    struct priv *p = ctx->priv;
    if (p->video_engine)
        pl_video_resize(p->video_engine, dst, osd);
}

static void update_external(struct render_backend *ctx, struct vo *vo)
{
    struct priv *p = ctx->priv;
    if (p->video_engine)
        pl_video_update_osd(p->video_engine, vo ? vo->osd : NULL);
}

static void reset(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;
    if (p->video_engine)
        pl_video_reset(p->video_engine);
}

static bool check_format(struct render_backend *ctx, int imgfmt)
{
    struct priv *p = ctx->priv;
    return p->video_engine ? pl_video_check_format(p->video_engine, imgfmt) : false;
}

static int get_target_size(struct render_backend *ctx, mpv_render_param *params, int *out_w, int *out_h)
{
    struct priv *p = ctx->priv;
    if (!p->context || !p->context->fns || !p->context->ra) return MPV_ERROR_UNINITIALIZED;
    pl_tex tex = NULL;
    int err = p->context->fns->wrap_fbo(p->context, params, &tex);
    if (err < 0) return err;
    if (!tex) return MPV_ERROR_GENERIC;
    *out_w = tex->params.w;
    *out_h = tex->params.h;
    
    pl_tex_destroy(p->context->gpu, &tex);
    return 0;
}

static void screenshot(struct render_backend *ctx, struct vo_frame *frame,
                       struct voctrl_screenshot *args)
{
    struct priv *p = ctx->priv;
    args->res = NULL;
    if (!p || !p->video_engine)
        return;

    struct mp_image *img = pl_video_screenshot(p->video_engine, frame);
    if (img)
        args->res = img;
}

static int set_parameter(struct render_backend *ctx, mpv_render_param param)
{
    return MPV_ERROR_NOT_IMPLEMENTED;
}

static struct mp_image *get_image(struct render_backend *ctx, int imgfmt,
                                  int w, int h, int stride_align, int flags)
{
    return NULL;
}

static void perfdata(struct render_backend *ctx,
                     struct voctrl_performance_data *out)
{
}

const struct render_backend_fns render_backend_gpu_next = {
    .init = init,
    .destroy = destroy,
    .render = render,
    .check_format = check_format,
    .set_parameter = set_parameter,
    .reconfig = reconfig,
    .reset = reset,
    .update_external = update_external,
    .resize = resize,
    .get_target_size = get_target_size,
    .get_image = get_image,
    .screenshot = screenshot,
    .perfdata = perfdata,
};
