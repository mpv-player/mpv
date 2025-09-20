#include "libmpv_gpu_next.h"
#include <stddef.h>             // for NULL
#include "common/msg.h"         // for mp_log_new, MP_ERR
#include "config.h"             // for HAVE_GL
#include "libplacebo/config.h"  // for PL_HAVE_OPENGL
#include "libplacebo/gpu.h"     // for pl_tex, pl_tex_params, pl_tex_t
#include "mpv/client.h"         // for mpv_error
#include "mpv/render.h"         // for mpv_render_param, mpv_render_param_type
#include "ra.h"                 // for ra_next_tex_destroy
#include "stdbool.h"            // for bool, false
#include "string.h"             // for strcmp
#include "ta/ta_talloc.h"       // for talloc_free, talloc_zero
#include "video.h"              // for pl_video_check_format, pl_video_init
#include "video/hwdec.h"        // for hwdec_devices_create, hwdec_devices_d...
#include "video/out/libmpv.h"   // for render_backend, get_mpv_render_param
#include "video/out/vo.h"       // for vo_frame (ptr only), voctrl_screenshot

/*
 * Structure for the image parameters.
 */
struct mp_image_params;
struct mp_osd_res;
struct mp_rect;

/*
 * Private data for the GPU next render backend.
 */
struct priv {
    struct libmpv_gpu_next_context *context; // Manages the API (e.g., OpenGL)
    struct pl_video *video_engine;           // Manages synchronous libplacebo rendering
};

/*
* List of available API context implementations (e.g., GL, Vulkan - currently only OpenGL)
*/
static const struct libmpv_gpu_next_context_fns *context_backends[] = {
#if HAVE_GL && defined(PL_HAVE_OPENGL)
    &libmpv_gpu_next_context_gl,
#endif
    NULL
};

/*
 * @brief Initializes the render_backend layer.
 * @param ctx The render_backend context.
 * @param params The render parameters.
 * @return 0 on success, negative error code on failure.
 */
static int init(struct render_backend *ctx, mpv_render_param *params)
{
    ctx->priv = talloc_zero(NULL, struct priv);
    struct priv *p = ctx->priv;

    // Get the API type from the render parameters.
    char *api = get_mpv_render_param(params, MPV_RENDER_PARAM_API_TYPE, NULL);
    if (!api) {
        MP_ERR(ctx, "API type not specified.\n");
        return MPV_ERROR_INVALID_PARAMETER;
    }

    // Find and initialize the requested API context (e.g., _gl.c).
    // This will create the pl_gpu and the `ra` (libplacebo render abstraction).
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

    // Initialize our synchronous libplacebo rendering engine.
    p->video_engine = pl_video_init(ctx->global, ctx->log, p->context->ra);
    if (!p->video_engine) {
        p->context->fns->destroy(p->context);
        talloc_free(p->context);
        return MPV_ERROR_VO_INIT_FAILED;
    }

    // Create hardware decoder devices.
    ctx->hwdec_devs = hwdec_devices_create();
    ctx->driver_caps = VO_CAP_ROTATE90 | VO_CAP_VFLIP;
    return 0;
}

/*
 * @brief Destroys the render_backend layer.
 * @param ctx The render_backend context.
 */
static void destroy(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;
    if (!p) return;

    hwdec_devices_destroy(ctx->hwdec_devs);
    pl_video_uninit(&p->video_engine);
    if (p->context) {
        p->context->fns->destroy(p->context); // This destroys the RA
        talloc_free(p->context);
    }
    talloc_free(p);
    ctx->priv = NULL;
}

/*
 * @brief Renders a video frame.
 * @param ctx The render_backend context.
 * @param params The render parameters.
 * @param frame The video frame to render.
 * @return 0 on success, negative error code on failure.
 */
static int render(struct render_backend *ctx, mpv_render_param *params,
                  struct vo_frame *frame)
{
    struct priv *p = ctx->priv;
    if (!p->video_engine) return MPV_ERROR_UNINITIALIZED;

    // Wrap the framebuffer object (FBO) for rendering.
    pl_tex target_tex = NULL;
    int err = p->context->fns->wrap_fbo(p->context, params, &target_tex);
    if (err < 0) return err;
    if (!target_tex) return MPV_ERROR_GENERIC;

    // Render the video frame.
    pl_video_render(p->video_engine, frame, target_tex);

    // Destroy the temporary wrapper texture via the RA.
    ra_next_tex_destroy(p->context->ra, &target_tex);

    if (p->context->fns->done_frame)
        p->context->fns->done_frame(p->context);

    return 0;
}

/*
 * @brief Reconfigures the video engine with new image parameters.
 * @param ctx The render_backend context.
 * @param params The new image parameters.
 */
static void reconfig(struct render_backend *ctx, struct mp_image_params *params)
{
    struct priv *p = ctx->priv;
    if (p->video_engine)
        pl_video_reconfig(p->video_engine, params);
}

/*
 * @brief Resizes the video output.
 * @param ctx The render_backend context.
 * @param src The source rectangle.
 * @param dst The destination rectangle.
 * @param osd The OSD rectangle.
 */
static void resize(struct render_backend *ctx, struct mp_rect *src,
                   struct mp_rect *dst, struct mp_osd_res *osd)
{
    struct priv *p = ctx->priv;
    if (p->video_engine)
        pl_video_resize(p->video_engine, dst, osd);
}

/*
 * @brief Updates the external state of the render_backend.
 * @param ctx The render_backend context.
 * @param vo The video output context.
 */
static void update_external(struct render_backend *ctx, struct vo *vo)
{
    struct priv *p = ctx->priv;
    if (p->video_engine)
        pl_video_update_osd(p->video_engine, vo ? vo->osd : NULL);
}

/*
 * @brief Resets the video engine.
 * @param ctx The render_backend context.
 */
static void reset(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;
    if (p->video_engine)
        pl_video_reset(p->video_engine);
}

/*
 * @brief Checks if the given image format is supported.
 * @param ctx The render_backend context.
 * @param imgfmt The image format to check.
 * @return True if the format is supported, false otherwise.
 */
static bool check_format(struct render_backend *ctx, int imgfmt)
{
    struct priv *p = ctx->priv;
    return p->video_engine ? pl_video_check_format(p->video_engine, imgfmt) : false;
}

/*
 * @brief Gets the target size for rendering.
 * @param ctx The render_backend context.
 * @param params The render parameters.
 * @param out_w Pointer to the output width.
 * @param out_h Pointer to the output height.
 * @return 0 on success, negative error code on failure.
 */
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
    // Destroy the temporary wrapper texture via the RA.
    ra_next_tex_destroy(p->context->ra, &tex);
    return 0;
}

/*
 * @brief Takes a screenshot of the current video frame.
 * @param ctx The render_backend context.
 * @param frame The video frame to capture.
 * @param args The screenshot arguments.
 */
static void screenshot(struct render_backend *ctx, struct vo_frame *frame,
                       struct voctrl_screenshot *args)
{
    struct priv *p = ctx->priv;
    args->res = NULL;
    if (!p || !p->video_engine)
        return;

    /* Let the pl_video engine perform the screenshot (uploads, tone-mapping,
     * render to an sRGB temporary, download). Returns an mp_image* or NULL. */
    struct mp_image *img = pl_video_screenshot(p->video_engine, frame);
    if (img)
        args->res = img;
}

/*
 * @brief Sets a render parameter.
 * @param ctx The render_backend context.
 * @param param The render parameter to set.
 * @return 0 on success, negative error code on failure.
 */
static int set_parameter(struct render_backend *ctx, mpv_render_param param)
{
    return MPV_ERROR_NOT_IMPLEMENTED;
}

/*
 * @brief Gets an image for rendering.
 * @param ctx The render_backend context.
 * @param imgfmt The image format.
 * @param w The width of the image.
 * @param h The height of the image.
 * @param stride_align The stride alignment.
 * @param flags The image flags.
 * @return A pointer to the image, or NULL on failure.
 */
static struct mp_image *get_image(struct render_backend *ctx, int imgfmt,
                                  int w, int h, int stride_align, int flags)
{
    return NULL;
}

/*
 * @brief Collects performance data from the render_backend.
 * @param ctx The render_backend context.
 * @param out The output structure to fill with performance data.
 */
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
