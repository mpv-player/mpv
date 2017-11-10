#pragma once

#include "common/msg.h"
#include "common/common.h"
#include "context.h"

enum glsl_shader {
    GLSL_SHADER_VERTEX,
    GLSL_SHADER_FRAGMENT,
    GLSL_SHADER_COMPUTE,
};

#define SPIRV_NAME_MAX_LEN 32

struct spirv_compiler {
    char name[SPIRV_NAME_MAX_LEN];
    const struct spirv_compiler_fns *fns;
    struct mp_log *log;
    void *priv;

    const char *required_ext; // or NULL
    int glsl_version;         // GLSL version supported
    int compiler_version;     // for cache invalidation, may be left as 0
    int ra_caps;              // RA_CAP_* provided by this implementation, if any
};

struct spirv_compiler_fns {
    // Compile GLSL to SPIR-V, under GL_KHR_vulkan_glsl semantics.
    bool (*compile_glsl)(struct spirv_compiler *spirv, void *tactx,
                         enum glsl_shader type, const char *glsl,
                         struct bstr *out_spirv);

    // Called by spirv_compiler_init / ra_ctx_destroy. These don't need to
    // allocate/free ctx->spirv, that is done by the caller
    bool (*init)(struct ra_ctx *ctx);
    void (*uninit)(struct ra_ctx *ctx); // optional
};

// Initializes ctx->spirv to a valid SPIR-V compiler, or returns false on
// failure. Cleanup will be handled by ra_ctx_destroy.
bool spirv_compiler_init(struct ra_ctx *ctx);
