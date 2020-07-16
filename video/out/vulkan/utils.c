#include "video/out/placebo/utils.h"
#include "utils.h"

bool mpvk_init(struct mpvk_ctx *vk, struct ra_ctx *ctx, const char *surface_ext)
{
    vk->ctx = pl_context_create(PL_API_VER, NULL);
    if (!vk->ctx)
        goto error;

    vk->pl_log = mp_log_new(ctx, ctx->log, "libplacebo");
    mppl_ctx_set_log(vk->ctx, vk->pl_log, true);
    mp_verbose(vk->pl_log, "Initialized libplacebo v%d\n", PL_API_VER);

    const char *exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        surface_ext,
    };

    vk->vkinst = pl_vk_inst_create(vk->ctx, &(struct pl_vk_inst_params) {
        .debug = ctx->opts.debug,
        .extensions = exts,
        .num_extensions = MP_ARRAY_SIZE(exts),
    });

    if (!vk->vkinst)
        goto error;

    mppl_ctx_set_log(vk->ctx, vk->pl_log, false); // disable probing
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
        vk->surface = NULL;
    }

    pl_vk_inst_destroy(&vk->vkinst);
    pl_context_destroy(&vk->ctx);
    TA_FREEP(&vk->pl_log);
}
