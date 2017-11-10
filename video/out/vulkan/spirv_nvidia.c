#include "video/out/gpu/spirv.h"

#include "common.h"
#include "context.h"
#include "utils.h"

static bool nv_glsl_compile(struct spirv_compiler *spirv, void *tactx,
                            enum glsl_shader type, const char *glsl,
                            struct bstr *out_spirv)
{
    // The nvidia extension literally assumes your SPIRV is in fact valid GLSL
    *out_spirv = bstr0(glsl);
    return true;
}

static bool nv_glsl_init(struct ra_ctx *ctx)
{
    struct mpvk_ctx *vk = ra_vk_ctx_get(ctx);
    if (!vk)
        return false;

    struct spirv_compiler *spv = ctx->spirv;
    spv->required_ext = VK_NV_GLSL_SHADER_EXTENSION_NAME;
    spv->glsl_version = 450; // impossible to query, so hard-code it..
    spv->ra_caps = RA_CAP_NESTED_ARRAY;

    // Make sure the extension is actually available, and fail gracefully
    // if it isn't
    VkExtensionProperties *props = NULL;
    uint32_t extnum = 0;
    VK(vkEnumerateDeviceExtensionProperties(vk->physd, NULL, &extnum, NULL));
    props = talloc_array(NULL, VkExtensionProperties, extnum);
    VK(vkEnumerateDeviceExtensionProperties(vk->physd, NULL, &extnum, props));

    bool ret = true;
    for (int e = 0; e < extnum; e++) {
        if (strncmp(props[e].extensionName, spv->required_ext,
                    VK_MAX_EXTENSION_NAME_SIZE) == 0)
            goto done;
    }

error:
    MP_VERBOSE(ctx, "Device doesn't support VK_NV_glsl_shader, skipping..\n");
    ret = false;

done:
    talloc_free(props);
    return ret;
}

const struct spirv_compiler_fns spirv_nvidia_builtin = {
    .compile_glsl = nv_glsl_compile,
    .init = nv_glsl_init,
};
