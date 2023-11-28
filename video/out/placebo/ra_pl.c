#include "common/common.h"
#include "common/msg.h"

#include "ra_pl.h"
#include "utils.h"

struct ra_pl {
    pl_gpu gpu;
    struct ra_timer_pl *active_timer;
};

static inline pl_gpu get_gpu(const struct ra *ra)
{
    struct ra_pl *p = ra->priv;
    return p->gpu;
}

static struct ra_fns ra_fns_pl;

pl_gpu ra_pl_get(const struct ra *ra)
{
    return ra->fns == &ra_fns_pl ? get_gpu(ra) : NULL;
}

static pl_timer get_active_timer(const struct ra *ra);

struct ra *ra_create_pl(pl_gpu gpu, struct mp_log *log)
{
    assert(gpu);

    struct ra *ra = talloc_zero(NULL, struct ra);
    ra->log = log;
    ra->fns = &ra_fns_pl;

    struct ra_pl *p = ra->priv = talloc_zero(ra, struct ra_pl);
    p->gpu = gpu;

    ra->glsl_version = gpu->glsl.version;
    ra->glsl_vulkan = gpu->glsl.vulkan;
    ra->glsl_es = gpu->glsl.gles;

    ra->caps = RA_CAP_DIRECT_UPLOAD | RA_CAP_NESTED_ARRAY | RA_CAP_FRAGCOORD;

    if (gpu->glsl.compute)
        ra->caps |= RA_CAP_COMPUTE | RA_CAP_NUM_GROUPS;
    if (gpu->limits.compute_queues > gpu->limits.fragment_queues)
        ra->caps |= RA_CAP_PARALLEL_COMPUTE;
    if (gpu->limits.max_variable_comps)
        ra->caps |= RA_CAP_GLOBAL_UNIFORM;
    if (!gpu->limits.host_cached)
        ra->caps |= RA_CAP_SLOW_DR;

    if (gpu->limits.max_tex_1d_dim)
        ra->caps |= RA_CAP_TEX_1D;
    if (gpu->limits.max_tex_3d_dim)
        ra->caps |= RA_CAP_TEX_3D;
    if (gpu->limits.max_ubo_size)
        ra->caps |= RA_CAP_BUF_RO;
    if (gpu->limits.max_ssbo_size)
        ra->caps |= RA_CAP_BUF_RW;
    if (gpu->glsl.min_gather_offset && gpu->glsl.max_gather_offset)
        ra->caps |= RA_CAP_GATHER;

    // Semi-hack: assume all textures are blittable if r8 is
    pl_fmt r8 = pl_find_named_fmt(gpu, "r8");
    if (r8->caps & PL_FMT_CAP_BLITTABLE)
        ra->caps |= RA_CAP_BLIT;

    ra->max_texture_wh = gpu->limits.max_tex_2d_dim;
    ra->max_pushc_size = gpu->limits.max_pushc_size;
    ra->max_compute_group_threads = gpu->glsl.max_group_threads;
    ra->max_shmem = gpu->glsl.max_shmem_size;

    // Set up format wrappers
    for (int i = 0; i < gpu->num_formats; i++) {
        pl_fmt plfmt = gpu->formats[i];
        static const enum ra_ctype fmt_type_map[PL_FMT_TYPE_COUNT] = {
            [PL_FMT_UNORM]  = RA_CTYPE_UNORM,
            [PL_FMT_UINT]   = RA_CTYPE_UINT,
            [PL_FMT_FLOAT]  = RA_CTYPE_FLOAT,
        };

        enum ra_ctype type = fmt_type_map[plfmt->type];
        if (!type || !(plfmt->caps & PL_FMT_CAP_SAMPLEABLE))
            continue;

        struct ra_format *rafmt = talloc_zero(ra, struct ra_format);
        *rafmt = (struct ra_format) {
            .name = plfmt->name,
            .priv = (void *) plfmt,
            .ctype = type,
            .ordered = pl_fmt_is_ordered(plfmt),
            .num_components = plfmt->num_components,
            .pixel_size = plfmt->texel_size,
            .linear_filter = plfmt->caps & PL_FMT_CAP_LINEAR,
            .renderable = plfmt->caps & PL_FMT_CAP_RENDERABLE,
            .storable = plfmt->caps & PL_FMT_CAP_STORABLE,
            .glsl_format = plfmt->glsl_format,
        };

        for (int c = 0; c < plfmt->num_components; c++) {
            rafmt->component_size[c] = plfmt->host_bits[c];
            rafmt->component_depth[c] = plfmt->component_depth[c];
        }

        MP_TARRAY_APPEND(ra, ra->formats, ra->num_formats, rafmt);
    }

    return ra;
}

static void destroy_ra_pl(struct ra *ra)
{
    talloc_free(ra);
}

static struct ra_format *map_fmt(struct ra *ra, pl_fmt plfmt)
{
    for (int i = 0; i < ra->num_formats; i++) {
        if (ra->formats[i]->priv == plfmt)
            return ra->formats[i];
    }

    MP_ERR(ra, "Failed mapping pl_fmt '%s' to ra_fmt?\n", plfmt->name);
    return NULL;
}

bool mppl_wrap_tex(struct ra *ra, pl_tex pltex, struct ra_tex *out_tex)
{
    if (!pltex)
        return false;

    *out_tex = (struct ra_tex) {
        .params = {
            .dimensions = pl_tex_params_dimension(pltex->params),
            .w = pltex->params.w,
            .h = pltex->params.h,
            .d = pltex->params.d,
            .format = map_fmt(ra, pltex->params.format),
            .render_src = pltex->params.sampleable,
            .render_dst = pltex->params.renderable,
            .storage_dst = pltex->params.storable,
            .blit_src = pltex->params.blit_src,
            .blit_dst = pltex->params.blit_dst,
            .host_mutable = pltex->params.host_writable,
            .downloadable = pltex->params.host_readable,
            // These don't exist upstream, so just pick something reasonable
            .src_linear = pltex->params.format->caps & PL_FMT_CAP_LINEAR,
            .src_repeat = false,
        },
        .priv = (void *) pltex,
    };

    return !!out_tex->params.format;
}

static struct ra_tex *tex_create_pl(struct ra *ra,
                                    const struct ra_tex_params *params)
{
    pl_gpu gpu = get_gpu(ra);
    pl_tex pltex = pl_tex_create(gpu, &(struct pl_tex_params) {
        .w = params->w,
        .h = params->dimensions >= 2 ? params->h : 0,
        .d = params->dimensions >= 3 ? params->d : 0,
        .format = params->format->priv,
        .sampleable = params->render_src,
        .renderable = params->render_dst,
        .storable = params->storage_dst,
        .blit_src = params->blit_src,
        .blit_dst = params->blit_dst || params->render_dst,
        .host_writable = params->host_mutable,
        .host_readable = params->downloadable,
        .initial_data = params->initial_data,
    });

    struct ra_tex *ratex = talloc_ptrtype(NULL, ratex);
    if (!mppl_wrap_tex(ra, pltex, ratex)) {
        pl_tex_destroy(gpu, &pltex);
        talloc_free(ratex);
        return NULL;
    }

    // Keep track of these, so we can correctly bind them later
    ratex->params.src_repeat = params->src_repeat;
    ratex->params.src_linear = params->src_linear;

    return ratex;
}

static void tex_destroy_pl(struct ra *ra, struct ra_tex *tex)
{
    if (!tex)
        return;

    pl_tex_destroy(get_gpu(ra), (pl_tex *) &tex->priv);
    talloc_free(tex);
}

static bool tex_upload_pl(struct ra *ra, const struct ra_tex_upload_params *params)
{
    pl_gpu gpu = get_gpu(ra);
    pl_tex tex = params->tex->priv;
    struct pl_tex_transfer_params pl_params = {
        .tex = tex,
        .buf = params->buf ? params->buf->priv : NULL,
        .buf_offset = params->buf_offset,
        .ptr = (void *) params->src,
        .timer = get_active_timer(ra),
    };

    pl_buf staging = NULL;
    if (params->tex->params.dimensions == 2) {
        if (params->rc) {
            pl_params.rc = (struct pl_rect3d) {
                .x0 = params->rc->x0, .x1 = params->rc->x1,
                .y0 = params->rc->y0, .y1 = params->rc->y1,
            };
        }

        pl_params.row_pitch = params->stride;
    }

    bool ok = pl_tex_upload(gpu, &pl_params);
    pl_buf_destroy(gpu, &staging);
    return ok;
}

static bool tex_download_pl(struct ra *ra, struct ra_tex_download_params *params)
{
    pl_tex tex = params->tex->priv;
    struct pl_tex_transfer_params pl_params = {
        .tex = tex,
        .ptr = params->dst,
        .timer = get_active_timer(ra),
        .row_pitch = params->stride,
    };

    return pl_tex_download(get_gpu(ra), &pl_params);
}

static struct ra_buf *buf_create_pl(struct ra *ra,
                                    const struct ra_buf_params *params)
{
    pl_buf plbuf = pl_buf_create(get_gpu(ra), &(struct pl_buf_params) {
        .size = params->size,
        .uniform = params->type == RA_BUF_TYPE_UNIFORM,
        .storable = params->type == RA_BUF_TYPE_SHADER_STORAGE,
        .host_mapped = params->host_mapped,
        .host_writable = params->host_mutable,
        .initial_data = params->initial_data,
    });

    if (!plbuf)
        return NULL;

    struct ra_buf *rabuf = talloc_ptrtype(NULL, rabuf);
    *rabuf = (struct ra_buf) {
        .params = *params,
        .data = plbuf->data,
        .priv = (void *) plbuf,
    };

    rabuf->params.initial_data = NULL;
    return rabuf;
}

static void buf_destroy_pl(struct ra *ra, struct ra_buf *buf)
{
    if (!buf)
        return;

    pl_buf_destroy(get_gpu(ra), (pl_buf *) &buf->priv);
    talloc_free(buf);
}

static void buf_update_pl(struct ra *ra, struct ra_buf *buf, ptrdiff_t offset,
                          const void *data, size_t size)
{
    pl_buf_write(get_gpu(ra), buf->priv, offset, data, size);
}

static bool buf_poll_pl(struct ra *ra, struct ra_buf *buf)
{
    return !pl_buf_poll(get_gpu(ra), buf->priv, 0);
}

static void clear_pl(struct ra *ra, struct ra_tex *dst, float color[4],
                     struct mp_rect *scissor)
{
    // TODO: implement scissor clearing by bltting a 1x1 tex instead
    pl_tex_clear(get_gpu(ra), dst->priv, color);
}

static void blit_pl(struct ra *ra, struct ra_tex *dst, struct ra_tex *src,
                    struct mp_rect *dst_rc, struct mp_rect *src_rc)
{
    struct pl_rect3d plsrc = {0}, pldst = {0};
    if (src_rc) {
        plsrc.x0 = MPMIN(MPMAX(src_rc->x0, 0), src->params.w);
        plsrc.y0 = MPMIN(MPMAX(src_rc->y0, 0), src->params.h);
        plsrc.x1 = MPMIN(MPMAX(src_rc->x1, 0), src->params.w);
        plsrc.y1 = MPMIN(MPMAX(src_rc->y1, 0), src->params.h);
    }

    if (dst_rc) {
        pldst.x0 = MPMIN(MPMAX(dst_rc->x0, 0), dst->params.w);
        pldst.y0 = MPMIN(MPMAX(dst_rc->y0, 0), dst->params.h);
        pldst.x1 = MPMIN(MPMAX(dst_rc->x1, 0), dst->params.w);
        pldst.y1 = MPMIN(MPMAX(dst_rc->y1, 0), dst->params.h);
    }

    pl_tex_blit(get_gpu(ra), &(struct pl_tex_blit_params) {
        .src = src->priv,
        .dst = dst->priv,
        .src_rc = plsrc,
        .dst_rc = pldst,
        .sample_mode = src->params.src_linear ? PL_TEX_SAMPLE_LINEAR
                                              : PL_TEX_SAMPLE_NEAREST,
    });
}

static const enum pl_var_type var_type[RA_VARTYPE_COUNT] = {
    [RA_VARTYPE_INT]    = PL_VAR_SINT,
    [RA_VARTYPE_FLOAT]  = PL_VAR_FLOAT,
};

static const enum pl_desc_type desc_type[RA_VARTYPE_COUNT] = {
    [RA_VARTYPE_TEX]    = PL_DESC_SAMPLED_TEX,
    [RA_VARTYPE_IMG_W]  = PL_DESC_STORAGE_IMG,
    [RA_VARTYPE_BUF_RO] = PL_DESC_BUF_UNIFORM,
    [RA_VARTYPE_BUF_RW] = PL_DESC_BUF_STORAGE,
};

static const enum pl_fmt_type fmt_type[RA_VARTYPE_COUNT] = {
    [RA_VARTYPE_INT]        = PL_FMT_SINT,
    [RA_VARTYPE_FLOAT]      = PL_FMT_FLOAT,
    [RA_VARTYPE_BYTE_UNORM] = PL_FMT_UNORM,
};

static const size_t var_size[RA_VARTYPE_COUNT] = {
    [RA_VARTYPE_INT]        = sizeof(int),
    [RA_VARTYPE_FLOAT]      = sizeof(float),
    [RA_VARTYPE_BYTE_UNORM] = sizeof(uint8_t),
};

static struct ra_layout uniform_layout_pl(struct ra_renderpass_input *inp)
{
    // To get the alignment requirements, we try laying this out with
    // an offset of 1 and then see where it ends up. This will always be
    // the minimum alignment requirement.
    struct pl_var_layout layout = pl_buf_uniform_layout(1, &(struct pl_var) {
        .name = inp->name,
        .type = var_type[inp->type],
        .dim_v = inp->dim_v,
        .dim_m = inp->dim_m,
        .dim_a = 1,
    });

    return (struct ra_layout) {
        .align = layout.offset,
        .stride = layout.stride,
        .size = layout.size,
    };
}

static struct ra_layout push_constant_layout_pl(struct ra_renderpass_input *inp)
{
    struct pl_var_layout layout = pl_push_constant_layout(1, &(struct pl_var) {
        .name = inp->name,
        .type = var_type[inp->type],
        .dim_v = inp->dim_v,
        .dim_m = inp->dim_m,
        .dim_a = 1,
    });

    return (struct ra_layout) {
        .align = layout.offset,
        .stride = layout.stride,
        .size = layout.size,
    };
}

static int desc_namespace_pl(struct ra *ra, enum ra_vartype type)
{
    return pl_desc_namespace(get_gpu(ra), desc_type[type]);
}

struct pass_priv {
    pl_pass pass;
    uint16_t *inp_index; // index translation map
    // Space to hold the descriptor bindings and variable updates
    struct pl_desc_binding *binds;
    struct pl_var_update *varups;
    int num_varups;
};

static struct ra_renderpass *renderpass_create_pl(struct ra *ra,
                                    const struct ra_renderpass_params *params)
{
    void *tmp = talloc_new(NULL);
    pl_gpu gpu = get_gpu(ra);
    struct ra_renderpass *pass = NULL;

    static const enum pl_pass_type pass_type[] = {
        [RA_RENDERPASS_TYPE_RASTER]  = PL_PASS_RASTER,
        [RA_RENDERPASS_TYPE_COMPUTE] = PL_PASS_COMPUTE,
    };

    struct pl_var *vars = NULL;
    struct pl_desc *descs = NULL;
    int num_vars = 0, num_descs = 0;

    struct pass_priv *priv = talloc_ptrtype(tmp, priv);
    priv->inp_index = talloc_zero_array(priv, uint16_t, params->num_inputs);

    for (int i = 0; i < params->num_inputs; i++) {
        const struct ra_renderpass_input *inp = &params->inputs[i];
        if (var_type[inp->type]) {
            priv->inp_index[i] = num_vars;
            MP_TARRAY_APPEND(tmp, vars, num_vars, (struct pl_var) {
                .name = inp->name,
                .type = var_type[inp->type],
                .dim_v = inp->dim_v,
                .dim_m = inp->dim_m,
                .dim_a = 1,
            });
        } else if (desc_type[inp->type]) {
            priv->inp_index[i] = num_descs;
            MP_TARRAY_APPEND(tmp, descs, num_descs, (struct pl_desc) {
                .name = inp->name,
                .type = desc_type[inp->type],
                .binding = inp->binding,
                .access = inp->type == RA_VARTYPE_IMG_W ? PL_DESC_ACCESS_WRITEONLY
                        : inp->type == RA_VARTYPE_BUF_RW ? PL_DESC_ACCESS_READWRITE
                        : PL_DESC_ACCESS_READONLY,
            });
        }
    }

    // Allocate space to store the bindings map persistently
    priv->binds = talloc_zero_array(priv, struct pl_desc_binding, num_descs);

    struct pl_pass_params pl_params = {
        .type = pass_type[params->type],
        .variables = vars,
        .num_variables = num_vars,
        .descriptors = descs,
        .num_descriptors = num_descs,
        .push_constants_size = params->push_constants_size,
        .glsl_shader = params->type == RA_RENDERPASS_TYPE_COMPUTE
                            ? params->compute_shader
                            : params->frag_shader,
    };

    struct pl_blend_params blend_params;

    if (params->type == RA_RENDERPASS_TYPE_RASTER) {
        pl_params.vertex_shader = params->vertex_shader;
        pl_params.vertex_type = PL_PRIM_TRIANGLE_LIST;
        pl_params.vertex_stride = params->vertex_stride;
        pl_params.load_target = !params->invalidate_target;
        pl_params.target_format = params->target_format->priv;

        if (params->enable_blend) {
            pl_params.blend_params = &blend_params;
            blend_params = (struct pl_blend_params) {
                // Same enum order as ra_blend
                .src_rgb = (enum pl_blend_mode) params->blend_src_rgb,
                .dst_rgb = (enum pl_blend_mode) params->blend_dst_rgb,
                .src_alpha = (enum pl_blend_mode) params->blend_src_alpha,
                .dst_alpha = (enum pl_blend_mode) params->blend_dst_alpha,
            };
        }

        for (int i = 0; i < params->num_vertex_attribs; i++) {
            const struct ra_renderpass_input *inp = &params->vertex_attribs[i];
            struct pl_vertex_attrib attrib = {
                .name = inp->name,
                .offset = inp->offset,
                .location = i,
                .fmt = pl_find_fmt(gpu, fmt_type[inp->type], inp->dim_v, 0,
                                   var_size[inp->type] * 8, PL_FMT_CAP_VERTEX),
            };

            if (!attrib.fmt) {
                MP_ERR(ra, "Failed mapping vertex attrib '%s' to pl_fmt?\n",
                       inp->name);
                goto error;
            }

            MP_TARRAY_APPEND(tmp, pl_params.vertex_attribs,
                             pl_params.num_vertex_attribs, attrib);
        }
    }

    priv->pass = pl_pass_create(gpu, &pl_params);
    if (!priv->pass)
        goto error;

    pass = talloc_ptrtype(NULL, pass);
    *pass = (struct ra_renderpass) {
        .params = *ra_renderpass_params_copy(pass, params),
        .priv = talloc_steal(pass, priv),
    };

    // fall through
error:
    talloc_free(tmp);
    return pass;
}

static void renderpass_destroy_pl(struct ra *ra, struct ra_renderpass *pass)
{
    if (!pass)
        return;

    struct pass_priv *priv = pass->priv;
    pl_pass_destroy(get_gpu(ra), (pl_pass *) &priv->pass);
    talloc_free(pass);
}

static void renderpass_run_pl(struct ra *ra,
                              const struct ra_renderpass_run_params *params)
{
    struct pass_priv *p = params->pass->priv;
    p->num_varups = 0;

    for (int i = 0; i < params->num_values; i++) {
        const struct ra_renderpass_input_val *val = &params->values[i];
        const struct ra_renderpass_input *inp = &params->pass->params.inputs[i];
        if (var_type[inp->type]) {
            MP_TARRAY_APPEND(p, p->varups, p->num_varups, (struct pl_var_update) {
                .index = p->inp_index[val->index],
                .data = val->data,
            });
        } else {
            struct pl_desc_binding bind = {0};
            switch (inp->type) {
            case RA_VARTYPE_TEX:
            case RA_VARTYPE_IMG_W: {
                struct ra_tex *tex = *((struct ra_tex **) val->data);
                bind.object = tex->priv;
                bind.sample_mode = tex->params.src_linear ? PL_TEX_SAMPLE_LINEAR
                                                          : PL_TEX_SAMPLE_NEAREST;
                bind.address_mode = tex->params.src_repeat ? PL_TEX_ADDRESS_REPEAT
                                                           : PL_TEX_ADDRESS_CLAMP;
                break;
            }
            case RA_VARTYPE_BUF_RO:
            case RA_VARTYPE_BUF_RW:
                bind.object = (* (struct ra_buf **) val->data)->priv;
                break;
            default: MP_ASSERT_UNREACHABLE();
            };

            p->binds[p->inp_index[val->index]] = bind;
        };
    }

    struct pl_pass_run_params pl_params = {
        .pass = p->pass,
        .var_updates = p->varups,
        .num_var_updates = p->num_varups,
        .desc_bindings = p->binds,
        .push_constants = params->push_constants,
        .timer = get_active_timer(ra),
    };

    if (p->pass->params.type == PL_PASS_RASTER) {
        pl_params.target = params->target->priv;
        pl_params.viewport = mp_rect2d_to_pl(params->viewport);
        pl_params.scissors = mp_rect2d_to_pl(params->scissors);
        pl_params.vertex_data = params->vertex_data;
        pl_params.vertex_count = params->vertex_count;
    } else {
        for (int i = 0; i < MP_ARRAY_SIZE(pl_params.compute_groups); i++)
            pl_params.compute_groups[i] = params->compute_groups[i];
    }

    pl_pass_run(get_gpu(ra), &pl_params);
}

struct ra_timer_pl {
    // Because libpplacebo only supports one operation per timer, we need
    // to use multiple pl_timers to sum up multiple passes/transfers
    pl_timer *timers;
    int num_timers;
    int idx_timers;
};

static ra_timer *timer_create_pl(struct ra *ra)
{
    struct ra_timer_pl *t = talloc_zero(ra, struct ra_timer_pl);
    return t;
}

static void timer_destroy_pl(struct ra *ra, ra_timer *timer)
{
    pl_gpu gpu = get_gpu(ra);
    struct ra_timer_pl *t = timer;

    for (int i = 0; i < t->num_timers; i++)
        pl_timer_destroy(gpu, &t->timers[i]);

    talloc_free(t);
}

static void timer_start_pl(struct ra *ra, ra_timer *timer)
{
    struct ra_pl *p = ra->priv;
    struct ra_timer_pl *t = timer;

    // There's nothing easy we can do in this case, since libplacebo only
    // supports one timer object per operation; so just ignore "inner" timers
    // when the user is nesting different timer queries
    if (p->active_timer)
        return;

    p->active_timer = t;
    t->idx_timers = 0;
}

static uint64_t timer_stop_pl(struct ra *ra, ra_timer *timer)
{
    struct ra_pl *p = ra->priv;
    struct ra_timer_pl *t = timer;

    if (p->active_timer != t)
        return 0;

    p->active_timer = NULL;

    // Sum up all of the active results
    uint64_t res = 0;
    for (int i = 0; i < t->idx_timers; i++)
        res += pl_timer_query(p->gpu, t->timers[i]);

    return res;
}

static pl_timer get_active_timer(const struct ra *ra)
{
    struct ra_pl *p = ra->priv;
    if (!p->active_timer)
        return NULL;

    struct ra_timer_pl *t = p->active_timer;
    if (t->idx_timers == t->num_timers)
        MP_TARRAY_APPEND(t, t->timers, t->num_timers, pl_timer_create(p->gpu));

    return t->timers[t->idx_timers++];
}

static struct ra_fns ra_fns_pl = {
    .destroy                = destroy_ra_pl,
    .tex_create             = tex_create_pl,
    .tex_destroy            = tex_destroy_pl,
    .tex_upload             = tex_upload_pl,
    .tex_download           = tex_download_pl,
    .buf_create             = buf_create_pl,
    .buf_destroy            = buf_destroy_pl,
    .buf_update             = buf_update_pl,
    .buf_poll               = buf_poll_pl,
    .clear                  = clear_pl,
    .blit                   = blit_pl,
    .uniform_layout         = uniform_layout_pl,
    .push_constant_layout   = push_constant_layout_pl,
    .desc_namespace         = desc_namespace_pl,
    .renderpass_create      = renderpass_create_pl,
    .renderpass_destroy     = renderpass_destroy_pl,
    .renderpass_run         = renderpass_run_pl,
    .timer_create           = timer_create_pl,
    .timer_destroy          = timer_destroy_pl,
    .timer_start            = timer_start_pl,
    .timer_stop             = timer_stop_pl,
};

