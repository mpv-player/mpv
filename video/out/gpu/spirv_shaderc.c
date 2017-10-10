#include "common/msg.h"

#include "context.h"
#include "spirv.h"

#include <shaderc/shaderc.h>

struct priv {
    shaderc_compiler_t compiler;
    shaderc_compile_options_t opts;
};

static void shaderc_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->spirv->priv;
    if (!p)
        return;

    shaderc_compile_options_release(p->opts);
    shaderc_compiler_release(p->compiler);
}

static bool shaderc_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->spirv->priv = talloc_zero(ctx->spirv, struct priv);

    p->compiler = shaderc_compiler_initialize();
    if (!p->compiler)
        goto error;
    p->opts = shaderc_compile_options_initialize();
    if (!p->opts)
        goto error;

    shaderc_compile_options_set_optimization_level(p->opts,
                                            shaderc_optimization_level_size);
    if (ctx->opts.debug)
        shaderc_compile_options_set_generate_debug_info(p->opts);

    int ver, rev;
    shaderc_get_spv_version(&ver, &rev);
    ctx->spirv->compiler_version = ver * 100 + rev; // forwards compatibility
    ctx->spirv->glsl_version = 450; // impossible to query?
    return true;

error:
    shaderc_uninit(ctx);
    return false;
}

static shaderc_compilation_result_t compile(struct priv *p,
                                            enum glsl_shader type,
                                            const char *glsl, bool debug)
{
    static const shaderc_shader_kind kinds[] = {
        [GLSL_SHADER_VERTEX]   = shaderc_glsl_vertex_shader,
        [GLSL_SHADER_FRAGMENT] = shaderc_glsl_fragment_shader,
        [GLSL_SHADER_COMPUTE]  = shaderc_glsl_compute_shader,
    };

    if (debug) {
        return shaderc_compile_into_spv_assembly(p->compiler, glsl, strlen(glsl),
                                        kinds[type], "input", "main", p->opts);
    } else {
        return shaderc_compile_into_spv(p->compiler, glsl, strlen(glsl),
                                        kinds[type], "input", "main", p->opts);
    }
}

static bool shaderc_compile(struct spirv_compiler *spirv, void *tactx,
                            enum glsl_shader type, const char *glsl,
                            struct bstr *out_spirv)
{
    struct priv *p = spirv->priv;

    shaderc_compilation_result_t res = compile(p, type, glsl, false);
    int errs = shaderc_result_get_num_errors(res),
        warn = shaderc_result_get_num_warnings(res),
        msgl = errs ? MSGL_ERR : warn ? MSGL_WARN : MSGL_V;

    const char *msg = shaderc_result_get_error_message(res);
    if (msg[0])
        MP_MSG(spirv, msgl, "shaderc output:\n%s", msg);

    int s = shaderc_result_get_compilation_status(res);
    bool success = s == shaderc_compilation_status_success;

    static const char *results[] = {
        [shaderc_compilation_status_success]            = "success",
        [shaderc_compilation_status_invalid_stage]      = "invalid stage",
        [shaderc_compilation_status_compilation_error]  = "error",
        [shaderc_compilation_status_internal_error]     = "internal error",
        [shaderc_compilation_status_null_result_object] = "no result",
        [shaderc_compilation_status_invalid_assembly]   = "invalid assembly",
    };

    const char *status = s < MP_ARRAY_SIZE(results) ? results[s] : "unknown";
    MP_MSG(spirv, msgl, "shaderc compile status '%s' (%d errors, %d warnings)\n",
           status, errs, warn);

    if (success) {
        void *bytes = (void *) shaderc_result_get_bytes(res);
        out_spirv->len = shaderc_result_get_length(res);
        out_spirv->start = talloc_memdup(tactx, bytes, out_spirv->len);
    }

    // Also print SPIR-V disassembly for debugging purposes. Unfortunately
    // there doesn't seem to be a way to get this except compiling the shader
    // a second time..
    if (mp_msg_test(spirv->log, MSGL_TRACE)) {
        shaderc_compilation_result_t dis = compile(p, type, glsl, true);
        MP_TRACE(spirv, "Generated SPIR-V:\n%.*s",
                 (int)shaderc_result_get_length(dis),
                 shaderc_result_get_bytes(dis));
        shaderc_result_release(dis);
    }

    shaderc_result_release(res);
    return success;
}

const struct spirv_compiler_fns spirv_shaderc = {
    .compile_glsl = shaderc_compile,
    .init = shaderc_init,
    .uninit = shaderc_uninit,
};
