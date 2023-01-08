#include "video/out/placebo/utils.h"
#include "utils.h"

bool mpvk_init(struct mpvk_ctx *vk, struct ra_ctx *ctx, const char *surface_ext)
{
    vk->pllog = mppl_log_create(ctx, ctx->vo->log);
    if (!vk->pllog)
        goto error;

    const char *exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        surface_ext,
    };

    mppl_log_set_probing(vk->pllog, true);
    vk->vkinst = pl_vk_inst_create(vk->pllog, &(struct pl_vk_inst_params) {
        .debug = ctx->opts.debug,
        .extensions = exts,
        .num_extensions = MP_ARRAY_SIZE(exts),
    });
    mppl_log_set_probing(vk->pllog, false);
    if (!vk->vkinst)
        goto error;

    return true;

error:
    mpvk_uninit(vk);
    return false;
}

void mpvk_uninit(struct mpvk_ctx *vk)
{
    if (vk->surface) {
        assert(vk->vkinst);
        vkDestroySurfaceKHR(vk->vkinst->instance, vk->surface, NULL);
        vk->surface = VK_NULL_HANDLE;
    }

    pl_vk_inst_destroy(&vk->vkinst);
    pl_log_destroy(&vk->pllog);
}
