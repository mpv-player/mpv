#include <libavutil/intreadwrite.h>

#include "formats.h"
#include "utils.h"
#include "ra_gl.h"

static struct ra_fns ra_fns_gl;

// For ra.priv
struct ra_gl {
    GL *gl;
    bool debug_enable;
    bool timer_active; // hack for GL_TIME_ELAPSED limitations
};

// For ra_tex.priv
struct ra_tex_gl {
    struct ra_buf_pool pbo; // for ra.use_pbo
    bool own_objects;
    GLenum target;
    GLuint texture; // 0 if no texture data associated
    GLuint fbo; // 0 if no rendering requested, or it default framebuffer
    // These 3 fields can be 0 if unknown.
    GLint internal_format;
    GLenum format;
    GLenum type;
};

// For ra_buf.priv
struct ra_buf_gl {
    GLenum target;
    GLuint buffer;
    GLsync fence;
};

// For ra_renderpass.priv
struct ra_renderpass_gl {
    GLuint program;
    // 1 entry for each ra_renderpass_params.inputs[] entry
    GLint *uniform_loc;
    int num_uniform_loc; // == ra_renderpass_params.num_inputs
    struct gl_vao vao;
};

// (Init time only.)
static void probe_real_size(GL *gl, struct ra_format *fmt)
{
    const struct gl_format *gl_fmt = fmt->priv;

    if (!gl->GetTexLevelParameteriv)
        return; // GLES

    bool is_la = gl_fmt->format == GL_LUMINANCE ||
                 gl_fmt->format == GL_LUMINANCE_ALPHA;
    if (is_la && gl->es)
        return; // GLES doesn't provide GL_TEXTURE_LUMINANCE_SIZE.

    GLuint tex;
    gl->GenTextures(1, &tex);
    gl->BindTexture(GL_TEXTURE_2D, tex);
    gl->TexImage2D(GL_TEXTURE_2D, 0, gl_fmt->internal_format, 64, 64, 0,
                   gl_fmt->format, gl_fmt->type, NULL);
    for (int i = 0; i < fmt->num_components; i++) {
        const GLenum pnames[] = {
            GL_TEXTURE_RED_SIZE,
            GL_TEXTURE_GREEN_SIZE,
            GL_TEXTURE_BLUE_SIZE,
            GL_TEXTURE_ALPHA_SIZE,
            GL_TEXTURE_LUMINANCE_SIZE,
            GL_TEXTURE_ALPHA_SIZE,
        };
        int comp = is_la ? i + 4 : i;
        assert(comp < MP_ARRAY_SIZE(pnames));
        GLint param = -1;
        gl->GetTexLevelParameteriv(GL_TEXTURE_2D, 0, pnames[comp], &param);
        fmt->component_depth[i] = param > 0 ? param : 0;
    }
    gl->DeleteTextures(1, &tex);
}

static int ra_init_gl(struct ra *ra, GL *gl)
{
    if (gl->version < 210 && gl->es < 200) {
        MP_ERR(ra, "At least OpenGL 2.1 or OpenGL ES 2.0 required.\n");
        return -1;
    }

    struct ra_gl *p = ra->priv = talloc_zero(NULL, struct ra_gl);
    p->gl = gl;

    ra_gl_set_debug(ra, true);

    ra->fns = &ra_fns_gl;
    ra->glsl_version = gl->glsl_version;
    ra->glsl_es = gl->es > 0;

    static const int caps_map[][2] = {
        {RA_CAP_DIRECT_UPLOAD,      0},
        {RA_CAP_GLOBAL_UNIFORM,     0},
        {RA_CAP_FRAGCOORD,          0},
        {RA_CAP_TEX_1D,             MPGL_CAP_1D_TEX},
        {RA_CAP_TEX_3D,             MPGL_CAP_3D_TEX},
        {RA_CAP_COMPUTE,            MPGL_CAP_COMPUTE_SHADER},
        {RA_CAP_NUM_GROUPS,         MPGL_CAP_COMPUTE_SHADER},
        {RA_CAP_NESTED_ARRAY,       MPGL_CAP_NESTED_ARRAY},
    };

    for (int i = 0; i < MP_ARRAY_SIZE(caps_map); i++) {
        if ((gl->mpgl_caps & caps_map[i][1]) == caps_map[i][1])
            ra->caps |= caps_map[i][0];
    }

    if (gl->BindBufferBase) {
        if (gl->mpgl_caps & MPGL_CAP_UBO)
            ra->caps |= RA_CAP_BUF_RO;
        if (gl->mpgl_caps & MPGL_CAP_SSBO)
            ra->caps |= RA_CAP_BUF_RW;
    }

    // textureGather is only supported in GLSL 400+
    if (ra->glsl_version >= 400)
        ra->caps |= RA_CAP_GATHER;

    if (gl->BlitFramebuffer)
        ra->caps |= RA_CAP_BLIT;

    // Disable compute shaders for GLSL < 420. This work-around is needed since
    // some buggy OpenGL drivers expose compute shaders for lower GLSL versions,
    // despite the spec requiring 420+.
    if (ra->glsl_version < 420)
        ra->caps &= ~RA_CAP_COMPUTE;

    int gl_fmt_features = gl_format_feature_flags(gl);

    for (int n = 0; gl_formats[n].internal_format; n++) {
        const struct gl_format *gl_fmt = &gl_formats[n];

        if (!(gl_fmt->flags & gl_fmt_features))
            continue;

        struct ra_format *fmt = talloc_zero(ra, struct ra_format);
        *fmt = (struct ra_format){
            .name           = gl_fmt->name,
            .priv           = (void *)gl_fmt,
            .ctype          = gl_format_type(gl_fmt),
            .num_components = gl_format_components(gl_fmt->format),
            .ordered        = gl_fmt->format != GL_RGB_422_APPLE,
            .pixel_size     = gl_bytes_per_pixel(gl_fmt->format, gl_fmt->type),
            .luminance_alpha = gl_fmt->format == GL_LUMINANCE_ALPHA,
            .linear_filter  = gl_fmt->flags & F_TF,
            .renderable     = (gl_fmt->flags & F_CR) &&
                              (gl->mpgl_caps & MPGL_CAP_FB),
            // TODO: Check whether it's a storable format
            // https://www.khronos.org/opengl/wiki/Image_Load_Store
            .storable       = true,
        };

        int csize = gl_component_size(gl_fmt->type) * 8;
        int depth = csize;

        if (gl_fmt->flags & F_F16) {
            depth = 16;
            csize = 32; // always upload as GL_FLOAT (simpler for us)
        }

        for (int i = 0; i < fmt->num_components; i++) {
            fmt->component_size[i] = csize;
            fmt->component_depth[i] = depth;
        }

        if (fmt->ctype == RA_CTYPE_UNORM && depth != 8)
            probe_real_size(gl, fmt);

        // Special formats for which OpenGL happens to have direct support.
        if (strcmp(fmt->name, "rgb565") == 0) {
            fmt->special_imgfmt = IMGFMT_RGB565;
            struct ra_imgfmt_desc *desc = talloc_zero(fmt, struct ra_imgfmt_desc);
            fmt->special_imgfmt_desc = desc;
            desc->num_planes = 1;
            desc->planes[0] = fmt;
            for (int i = 0; i < 3; i++)
                desc->components[0][i] = i + 1;
            desc->chroma_w = desc->chroma_h = 1;
        }
        if (strcmp(fmt->name, "rgb10_a2") == 0) {
            fmt->special_imgfmt = IMGFMT_RGB30;
            struct ra_imgfmt_desc *desc = talloc_zero(fmt, struct ra_imgfmt_desc);
            fmt->special_imgfmt_desc = desc;
            desc->component_bits = 10;
            desc->num_planes = 1;
            desc->planes[0] = fmt;
            for (int i = 0; i < 3; i++)
                desc->components[0][i] = 3 - i;
            desc->chroma_w = desc->chroma_h = 1;
        }
        if (strcmp(fmt->name, "appleyp") == 0) {
            fmt->special_imgfmt = IMGFMT_UYVY;
            struct ra_imgfmt_desc *desc = talloc_zero(fmt, struct ra_imgfmt_desc);
            fmt->special_imgfmt_desc = desc;
            desc->num_planes = 1;
            desc->planes[0] = fmt;
            desc->components[0][0] = 3;
            desc->components[0][1] = 1;
            desc->components[0][2] = 2;
            desc->chroma_w = desc->chroma_h = 1;
        }

        fmt->glsl_format = ra_fmt_glsl_format(fmt);

        MP_TARRAY_APPEND(ra, ra->formats, ra->num_formats, fmt);
    }

    GLint ival;
    gl->GetIntegerv(GL_MAX_TEXTURE_SIZE, &ival);
    ra->max_texture_wh = ival;

    if (ra->caps & RA_CAP_COMPUTE) {
        gl->GetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &ival);
        ra->max_shmem = ival;
    }

    gl->Disable(GL_DITHER);

    if (!ra_find_unorm_format(ra, 2, 1))
        MP_VERBOSE(ra, "16 bit UNORM textures not available.\n");

    return 0;
}

struct ra *ra_create_gl(GL *gl, struct mp_log *log)
{
    struct ra *ra = talloc_zero(NULL, struct ra);
    ra->log = log;
    if (ra_init_gl(ra, gl) < 0) {
        talloc_free(ra);
        return NULL;
    }
    return ra;
}

static void gl_destroy(struct ra *ra)
{
    talloc_free(ra->priv);
}

void ra_gl_set_debug(struct ra *ra, bool enable)
{
    struct ra_gl *p = ra->priv;
    GL *gl = ra_gl_get(ra);

    p->debug_enable = enable;
    if (gl->debug_context)
        gl_set_debug_logger(gl, enable ? ra->log : NULL);
}

static void gl_tex_destroy(struct ra *ra, struct ra_tex *tex)
{
    GL *gl = ra_gl_get(ra);
    struct ra_tex_gl *tex_gl = tex->priv;

    ra_buf_pool_uninit(ra, &tex_gl->pbo);

    if (tex_gl->own_objects) {
        if (tex_gl->fbo)
            gl->DeleteFramebuffers(1, &tex_gl->fbo);

        gl->DeleteTextures(1, &tex_gl->texture);
    }
    talloc_free(tex_gl);
    talloc_free(tex);
}

static struct ra_tex *gl_tex_create_blank(struct ra *ra,
                                          const struct ra_tex_params *params)
{
    struct ra_tex *tex = talloc_zero(NULL, struct ra_tex);
    tex->params = *params;
    tex->params.initial_data = NULL;
    struct ra_tex_gl *tex_gl = tex->priv = talloc_zero(NULL, struct ra_tex_gl);

    const struct gl_format *fmt = params->format->priv;
    tex_gl->internal_format = fmt->internal_format;
    tex_gl->format = fmt->format;
    tex_gl->type = fmt->type;
    switch (params->dimensions) {
    case 1: tex_gl->target = GL_TEXTURE_1D; break;
    case 2: tex_gl->target = GL_TEXTURE_2D; break;
    case 3: tex_gl->target = GL_TEXTURE_3D; break;
    default: abort();
    }
    if (params->non_normalized) {
        assert(params->dimensions == 2);
        tex_gl->target = GL_TEXTURE_RECTANGLE;
    }
    if (params->external_oes) {
        assert(params->dimensions == 2 && !params->non_normalized);
        tex_gl->target = GL_TEXTURE_EXTERNAL_OES;
    }

    if (params->downloadable && !(params->dimensions == 2 &&
                                  params->format->renderable))
    {
        gl_tex_destroy(ra, tex);
        return NULL;
    }

    return tex;
}

static struct ra_tex *gl_tex_create(struct ra *ra,
                                    const struct ra_tex_params *params)
{
    GL *gl = ra_gl_get(ra);
    assert(!params->format->dummy_format);

    struct ra_tex *tex = gl_tex_create_blank(ra, params);
    if (!tex)
        return NULL;
    struct ra_tex_gl *tex_gl = tex->priv;

    tex_gl->own_objects = true;

    gl->GenTextures(1, &tex_gl->texture);
    gl->BindTexture(tex_gl->target, tex_gl->texture);

    GLint filter = params->src_linear ? GL_LINEAR : GL_NEAREST;
    GLint wrap = params->src_repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    gl->TexParameteri(tex_gl->target, GL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(tex_gl->target, GL_TEXTURE_MAG_FILTER, filter);
    gl->TexParameteri(tex_gl->target, GL_TEXTURE_WRAP_S, wrap);
    if (params->dimensions > 1)
        gl->TexParameteri(tex_gl->target, GL_TEXTURE_WRAP_T, wrap);
    if (params->dimensions > 2)
        gl->TexParameteri(tex_gl->target, GL_TEXTURE_WRAP_R, wrap);

    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    switch (params->dimensions) {
    case 1:
        gl->TexImage1D(tex_gl->target, 0, tex_gl->internal_format, params->w,
                       0, tex_gl->format, tex_gl->type, params->initial_data);
        break;
    case 2:
        gl->TexImage2D(tex_gl->target, 0, tex_gl->internal_format, params->w,
                       params->h, 0, tex_gl->format, tex_gl->type,
                       params->initial_data);
        break;
    case 3:
        gl->TexImage3D(tex_gl->target, 0, tex_gl->internal_format, params->w,
                       params->h, params->d, 0, tex_gl->format, tex_gl->type,
                       params->initial_data);
        break;
    }
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);

    gl->BindTexture(tex_gl->target, 0);

    gl_check_error(gl, ra->log, "after creating texture");

    // Even blitting needs an FBO in OpenGL for strange reasons.
    // Download is handled by reading from an FBO.
    if (tex->params.render_dst || tex->params.blit_src ||
        tex->params.blit_dst || tex->params.downloadable)
    {
        if (!tex->params.format->renderable) {
            MP_ERR(ra, "Trying to create renderable texture with unsupported "
                   "format.\n");
            ra_tex_free(ra, &tex);
            return NULL;
        }

        assert(gl->mpgl_caps & MPGL_CAP_FB);

        gl->GenFramebuffers(1, &tex_gl->fbo);
        gl->BindFramebuffer(GL_FRAMEBUFFER, tex_gl->fbo);
        gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D, tex_gl->texture, 0);
        GLenum err = gl->CheckFramebufferStatus(GL_FRAMEBUFFER);
        gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

        if (err != GL_FRAMEBUFFER_COMPLETE) {
            MP_ERR(ra, "Error: framebuffer completeness check failed (error=%d).\n",
                   (int)err);
            ra_tex_free(ra, &tex);
            return NULL;
        }


        gl_check_error(gl, ra->log, "after creating framebuffer");
    }

    return tex;
}

// Create a ra_tex that merely wraps an existing texture. The returned object
// is freed with ra_tex_free(), but this will not delete the texture passed to
// this function.
// Some features are unsupported, e.g. setting params->initial_data or render_dst.
struct ra_tex *ra_create_wrapped_tex(struct ra *ra,
                                     const struct ra_tex_params *params,
                                     GLuint gl_texture)
{
    struct ra_tex *tex = gl_tex_create_blank(ra, params);
    if (!tex)
        return NULL;
    struct ra_tex_gl *tex_gl = tex->priv;
    tex_gl->texture = gl_texture;
    return tex;
}

static const struct ra_format fbo_dummy_format = {
    .name = "unknown_fbo",
    .priv = (void *)&(const struct gl_format){
        .name = "unknown",
        .format = GL_RGBA,
        .flags = F_CR,
    },
    .renderable = true,
    .dummy_format = true,
};

// Create a ra_tex that merely wraps an existing framebuffer. gl_fbo can be 0
// to wrap the default framebuffer.
// The returned object is freed with ra_tex_free(), but this will not delete
// the framebuffer object passed to this function.
struct ra_tex *ra_create_wrapped_fb(struct ra *ra, GLuint gl_fbo, int w, int h)
{
    struct ra_tex *tex = talloc_zero(ra, struct ra_tex);
    *tex = (struct ra_tex){
        .params = {
            .dimensions = 2,
            .w = w, .h = h, .d = 1,
            .format = &fbo_dummy_format,
            .render_dst = true,
            .blit_src = true,
            .blit_dst = true,
        },
    };

    struct ra_tex_gl *tex_gl = tex->priv = talloc_zero(NULL, struct ra_tex_gl);
    *tex_gl = (struct ra_tex_gl){
        .fbo = gl_fbo,
        .internal_format = 0,
        .format = GL_RGBA,
        .type = 0,
    };

    return tex;
}

GL *ra_gl_get(struct ra *ra)
{
    struct ra_gl *p = ra->priv;
    return p->gl;
}

// Return the associate glTexImage arguments for the given format. Sets all
// fields to 0 on failure.
void ra_gl_get_format(const struct ra_format *fmt, GLint *out_internal_format,
                      GLenum *out_format, GLenum *out_type)
{
    const struct gl_format *gl_format = fmt->priv;
    *out_internal_format = gl_format->internal_format;
    *out_format = gl_format->format;
    *out_type = gl_format->type;
}

void ra_gl_get_raw_tex(struct ra *ra, struct ra_tex *tex,
                       GLuint *out_texture, GLenum *out_target)
{
    struct ra_tex_gl *tex_gl = tex->priv;
    *out_texture = tex_gl->texture;
    *out_target = tex_gl->target;
}

// Return whether the ra instance was created with ra_create_gl(). This is the
// _only_ function that can be called on a ra instance of any type.
bool ra_is_gl(struct ra *ra)
{
    return ra->fns == &ra_fns_gl;
}

static bool gl_tex_upload(struct ra *ra,
                          const struct ra_tex_upload_params *params)
{
    GL *gl = ra_gl_get(ra);
    struct ra_tex *tex = params->tex;
    struct ra_buf *buf = params->buf;
    struct ra_tex_gl *tex_gl = tex->priv;
    struct ra_buf_gl *buf_gl = buf ? buf->priv : NULL;
    assert(tex->params.host_mutable);
    assert(!params->buf || !params->src);

    if (ra->use_pbo && !params->buf)
        return ra_tex_upload_pbo(ra, &tex_gl->pbo, params);

    const void *src = params->src;
    if (buf) {
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, buf_gl->buffer);
        src = (void *)params->buf_offset;
    }

    gl->BindTexture(tex_gl->target, tex_gl->texture);
    if (params->invalidate && gl->InvalidateTexImage)
        gl->InvalidateTexImage(tex_gl->texture, 0);

    switch (tex->params.dimensions) {
    case 1:
        gl->TexImage1D(tex_gl->target, 0, tex_gl->internal_format,
                       tex->params.w, 0, tex_gl->format, tex_gl->type, src);
        break;
    case 2: {
        struct mp_rect rc = {0, 0, tex->params.w, tex->params.h};
        if (params->rc)
            rc = *params->rc;
        gl_upload_tex(gl, tex_gl->target, tex_gl->format, tex_gl->type,
                      src, params->stride, rc.x0, rc.y0, rc.x1 - rc.x0,
                      rc.y1 - rc.y0);
        break;
    }
    case 3:
        gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
        gl->TexImage3D(GL_TEXTURE_3D, 0, tex_gl->internal_format, tex->params.w,
                       tex->params.h, tex->params.d, 0, tex_gl->format,
                       tex_gl->type, src);
        gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
        break;
    }

    gl->BindTexture(tex_gl->target, 0);

    if (buf) {
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        if (buf->params.host_mapped) {
            // Make sure the PBO is not reused until GL is done with it. If a
            // previous operation is pending, "update" it by creating a new
            // fence that will cover the previous operation as well.
            gl->DeleteSync(buf_gl->fence);
            buf_gl->fence = gl->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        }
    }

    return true;
}

static bool gl_tex_download(struct ra *ra, struct ra_tex_download_params *params)
{
    GL *gl = ra_gl_get(ra);
    struct ra_tex *tex = params->tex;
    struct ra_tex_gl *tex_gl = tex->priv;
    if (!tex_gl->fbo)
        return false;
    return gl_read_fbo_contents(gl, tex_gl->fbo, 1, tex_gl->format, tex_gl->type,
                                tex->params.w, tex->params.h, params->dst,
                                params->stride);
}

static void gl_buf_destroy(struct ra *ra, struct ra_buf *buf)
{
    if (!buf)
        return;

    GL *gl = ra_gl_get(ra);
    struct ra_buf_gl *buf_gl = buf->priv;

    if (buf_gl->fence)
        gl->DeleteSync(buf_gl->fence);

    if (buf->data) {
        gl->BindBuffer(buf_gl->target, buf_gl->buffer);
        gl->UnmapBuffer(buf_gl->target);
        gl->BindBuffer(buf_gl->target, 0);
    }
    gl->DeleteBuffers(1, &buf_gl->buffer);

    talloc_free(buf_gl);
    talloc_free(buf);
}

static struct ra_buf *gl_buf_create(struct ra *ra,
                                    const struct ra_buf_params *params)
{
    GL *gl = ra_gl_get(ra);

    if (params->host_mapped && gl->version < 440)
        return NULL;

    struct ra_buf *buf = talloc_zero(NULL, struct ra_buf);
    buf->params = *params;
    buf->params.initial_data = NULL;

    struct ra_buf_gl *buf_gl = buf->priv = talloc_zero(NULL, struct ra_buf_gl);
    gl->GenBuffers(1, &buf_gl->buffer);

    switch (params->type) {
    case RA_BUF_TYPE_TEX_UPLOAD:     buf_gl->target = GL_PIXEL_UNPACK_BUFFER;   break;
    case RA_BUF_TYPE_SHADER_STORAGE: buf_gl->target = GL_SHADER_STORAGE_BUFFER; break;
    case RA_BUF_TYPE_UNIFORM:        buf_gl->target = GL_UNIFORM_BUFFER;        break;
    default: abort();
    };

    gl->BindBuffer(buf_gl->target, buf_gl->buffer);

    if (params->host_mapped) {
        unsigned flags = GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT |
                         GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;

        unsigned storflags = flags;
        if (params->type == RA_BUF_TYPE_TEX_UPLOAD)
            storflags |= GL_CLIENT_STORAGE_BIT;

        gl->BufferStorage(buf_gl->target, params->size, params->initial_data,
                          storflags);
        buf->data = gl->MapBufferRange(buf_gl->target, 0, params->size, flags);
        if (!buf->data) {
            gl_check_error(gl, ra->log, "mapping buffer");
            gl_buf_destroy(ra, buf);
            buf = NULL;
        }
    } else {
        GLenum hint;
        switch (params->type) {
        case RA_BUF_TYPE_TEX_UPLOAD:     hint = GL_STREAM_DRAW; break;
        case RA_BUF_TYPE_SHADER_STORAGE: hint = GL_STREAM_COPY; break;
        case RA_BUF_TYPE_UNIFORM:        hint = GL_STATIC_DRAW; break;
        default: abort();
        }

        gl->BufferData(buf_gl->target, params->size, params->initial_data, hint);
    }

    gl->BindBuffer(buf_gl->target, 0);
    return buf;
}

static void gl_buf_update(struct ra *ra, struct ra_buf *buf, ptrdiff_t offset,
                          const void *data, size_t size)
{
    GL *gl = ra_gl_get(ra);
    struct ra_buf_gl *buf_gl = buf->priv;
    assert(buf->params.host_mutable);

    gl->BindBuffer(buf_gl->target, buf_gl->buffer);
    gl->BufferSubData(buf_gl->target, offset, size, data);
    gl->BindBuffer(buf_gl->target, 0);
}

static bool gl_buf_poll(struct ra *ra, struct ra_buf *buf)
{
    // Non-persistently mapped buffers are always implicitly reusable in OpenGL,
    // the implementation will create more buffers under the hood if needed.
    if (!buf->data)
        return true;

    GL *gl = ra_gl_get(ra);
    struct ra_buf_gl *buf_gl = buf->priv;

    if (buf_gl->fence) {
        GLenum res = gl->ClientWaitSync(buf_gl->fence, 0, 0); // non-blocking
        if (res == GL_ALREADY_SIGNALED) {
            gl->DeleteSync(buf_gl->fence);
            buf_gl->fence = NULL;
        }
    }

    return !buf_gl->fence;
}

static void gl_clear(struct ra *ra, struct ra_tex *dst, float color[4],
                     struct mp_rect *scissor)
{
    GL *gl = ra_gl_get(ra);

    assert(dst->params.render_dst);
    struct ra_tex_gl *dst_gl = dst->priv;

    gl->BindFramebuffer(GL_FRAMEBUFFER, dst_gl->fbo);

    gl->Scissor(scissor->x0, scissor->y0,
                scissor->x1 - scissor->x0,
                scissor->y1 - scissor->y0);

    gl->Enable(GL_SCISSOR_TEST);
    gl->ClearColor(color[0], color[1], color[2], color[3]);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    gl->Disable(GL_SCISSOR_TEST);

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void gl_blit(struct ra *ra, struct ra_tex *dst, struct ra_tex *src,
                    struct mp_rect *dst_rc, struct mp_rect *src_rc)
{
    GL *gl = ra_gl_get(ra);

    assert(src->params.blit_src);
    assert(dst->params.blit_dst);

    struct ra_tex_gl *src_gl = src->priv;
    struct ra_tex_gl *dst_gl = dst->priv;

    gl->BindFramebuffer(GL_READ_FRAMEBUFFER, src_gl->fbo);
    gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_gl->fbo);
    gl->BlitFramebuffer(src_rc->x0, src_rc->y0, src_rc->x1, src_rc->y1,
                        dst_rc->x0, dst_rc->y0, dst_rc->x1, dst_rc->y1,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
    gl->BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

static int gl_desc_namespace(struct ra *ra, enum ra_vartype type)
{
    return type;
}

static void gl_renderpass_destroy(struct ra *ra, struct ra_renderpass *pass)
{
    GL *gl = ra_gl_get(ra);
    struct ra_renderpass_gl *pass_gl = pass->priv;
    gl->DeleteProgram(pass_gl->program);
    gl_vao_uninit(&pass_gl->vao);

    talloc_free(pass_gl);
    talloc_free(pass);
}

static const char *shader_typestr(GLenum type)
{
    switch (type) {
    case GL_VERTEX_SHADER:   return "vertex";
    case GL_FRAGMENT_SHADER: return "fragment";
    case GL_COMPUTE_SHADER:  return "compute";
    default: abort();
    }
}

static void compile_attach_shader(struct ra *ra, GLuint program,
                                  GLenum type, const char *source, bool *ok)
{
    GL *gl = ra_gl_get(ra);

    GLuint shader = gl->CreateShader(type);
    gl->ShaderSource(shader, 1, &source, NULL);
    gl->CompileShader(shader);
    GLint status = 0;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint log_length = 0;
    gl->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

    int pri = status ? (log_length > 1 ? MSGL_V : MSGL_DEBUG) : MSGL_ERR;
    const char *typestr = shader_typestr(type);
    if (mp_msg_test(ra->log, pri)) {
        MP_MSG(ra, pri, "%s shader source:\n", typestr);
        mp_log_source(ra->log, pri, source);
    }
    if (log_length > 1) {
        GLchar *logstr = talloc_zero_size(NULL, log_length + 1);
        gl->GetShaderInfoLog(shader, log_length, NULL, logstr);
        MP_MSG(ra, pri, "%s shader compile log (status=%d):\n%s\n",
               typestr, status, logstr);
        talloc_free(logstr);
    }
    if (gl->GetTranslatedShaderSourceANGLE && mp_msg_test(ra->log, MSGL_DEBUG)) {
        GLint len = 0;
        gl->GetShaderiv(shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &len);
        if (len > 0) {
            GLchar *sstr = talloc_zero_size(NULL, len + 1);
            gl->GetTranslatedShaderSourceANGLE(shader, len, NULL, sstr);
            MP_DBG(ra, "Translated shader:\n");
            mp_log_source(ra->log, MSGL_DEBUG, sstr);
        }
    }

    gl->AttachShader(program, shader);
    gl->DeleteShader(shader);

    *ok &= status;
}

static void link_shader(struct ra *ra, GLuint program, bool *ok)
{
    GL *gl = ra_gl_get(ra);

    gl->LinkProgram(program);
    GLint status = 0;
    gl->GetProgramiv(program, GL_LINK_STATUS, &status);
    GLint log_length = 0;
    gl->GetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

    int pri = status ? (log_length > 1 ? MSGL_V : MSGL_DEBUG) : MSGL_ERR;
    if (mp_msg_test(ra->log, pri)) {
        GLchar *logstr = talloc_zero_size(NULL, log_length + 1);
        gl->GetProgramInfoLog(program, log_length, NULL, logstr);
        MP_MSG(ra, pri, "shader link log (status=%d): %s\n", status, logstr);
        talloc_free(logstr);
    }

    *ok &= status;
}

// either 'compute' or both 'vertex' and 'frag' are needed
static GLuint compile_program(struct ra *ra, const struct ra_renderpass_params *p)
{
    GL *gl = ra_gl_get(ra);

    GLuint prog = gl->CreateProgram();
    bool ok = true;
    if (p->type == RA_RENDERPASS_TYPE_COMPUTE)
        compile_attach_shader(ra, prog, GL_COMPUTE_SHADER, p->compute_shader, &ok);
    if (p->type == RA_RENDERPASS_TYPE_RASTER) {
        compile_attach_shader(ra, prog, GL_VERTEX_SHADER, p->vertex_shader, &ok);
        compile_attach_shader(ra, prog, GL_FRAGMENT_SHADER, p->frag_shader, &ok);
        for (int n = 0; n < p->num_vertex_attribs; n++)
            gl->BindAttribLocation(prog, n, p->vertex_attribs[n].name);
    }
    link_shader(ra, prog, &ok);
    if (!ok) {
        gl->DeleteProgram(prog);
        prog = 0;
    }
    return prog;
}

static GLuint load_program(struct ra *ra, const struct ra_renderpass_params *p,
                           bstr *out_cached_data)
{
    GL *gl = ra_gl_get(ra);

    GLuint prog = 0;

    if (gl->ProgramBinary && p->cached_program.len > 4) {
        GLenum format = AV_RL32(p->cached_program.start);
        prog = gl->CreateProgram();
        gl_check_error(gl, ra->log, "before loading program");
        gl->ProgramBinary(prog, format, p->cached_program.start + 4,
                                        p->cached_program.len - 4);
        gl->GetError(); // discard potential useless error
        GLint status = 0;
        gl->GetProgramiv(prog, GL_LINK_STATUS, &status);
        if (status) {
            MP_DBG(ra, "Loading binary program succeeded.\n");
        } else {
            gl->DeleteProgram(prog);
            prog = 0;
        }
    }

    if (!prog) {
        prog = compile_program(ra, p);

        if (gl->GetProgramBinary && prog) {
            GLint size = 0;
            gl->GetProgramiv(prog, GL_PROGRAM_BINARY_LENGTH, &size);
            uint8_t *buffer = talloc_size(NULL, size + 4);
            GLsizei actual_size = 0;
            GLenum binary_format = 0;
            if (size > 0) {
                gl->GetProgramBinary(prog, size, &actual_size, &binary_format,
                                     buffer + 4);
            }
            AV_WL32(buffer, binary_format);
            if (actual_size) {
                *out_cached_data = (bstr){buffer, actual_size + 4};
            } else {
                talloc_free(buffer);
            }
        }
    }

    return prog;
}

static struct ra_renderpass *gl_renderpass_create(struct ra *ra,
                                    const struct ra_renderpass_params *params)
{
    GL *gl = ra_gl_get(ra);

    struct ra_renderpass *pass = talloc_zero(NULL, struct ra_renderpass);
    pass->params = *ra_renderpass_params_copy(pass, params);
    pass->params.cached_program = (bstr){0};
    struct ra_renderpass_gl *pass_gl = pass->priv =
        talloc_zero(NULL, struct ra_renderpass_gl);

    bstr cached = {0};
    pass_gl->program = load_program(ra, params, &cached);
    if (!pass_gl->program) {
        gl_renderpass_destroy(ra, pass);
        return NULL;
    }

    talloc_steal(pass, cached.start);
    pass->params.cached_program = cached;

    gl->UseProgram(pass_gl->program);
    for (int n = 0; n < params->num_inputs; n++) {
        GLint loc =
            gl->GetUniformLocation(pass_gl->program, params->inputs[n].name);
        MP_TARRAY_APPEND(pass_gl, pass_gl->uniform_loc, pass_gl->num_uniform_loc,
                         loc);

        // For compatibility with older OpenGL, we need to explicitly update
        // the texture/image unit bindings after creating the shader program,
        // since specifying it directly requires GLSL 4.20+
        switch (params->inputs[n].type) {
        case RA_VARTYPE_TEX:
        case RA_VARTYPE_IMG_W:
            gl->Uniform1i(loc, params->inputs[n].binding);
            break;
        }
    }
    gl->UseProgram(0);

    gl_vao_init(&pass_gl->vao, gl, pass->params.vertex_stride,
                pass->params.vertex_attribs, pass->params.num_vertex_attribs);

    return pass;
}

static GLenum map_blend(enum ra_blend blend)
{
    switch (blend) {
    case RA_BLEND_ZERO:                 return GL_ZERO;
    case RA_BLEND_ONE:                  return GL_ONE;
    case RA_BLEND_SRC_ALPHA:            return GL_SRC_ALPHA;
    case RA_BLEND_ONE_MINUS_SRC_ALPHA:  return GL_ONE_MINUS_SRC_ALPHA;
    default: return 0;
    }
}

// Assumes program is current (gl->UseProgram(program)).
static void update_uniform(struct ra *ra, struct ra_renderpass *pass,
                           struct ra_renderpass_input_val *val)
{
    GL *gl = ra_gl_get(ra);
    struct ra_renderpass_gl *pass_gl = pass->priv;

    struct ra_renderpass_input *input = &pass->params.inputs[val->index];
    assert(val->index >= 0 && val->index < pass_gl->num_uniform_loc);
    GLint loc = pass_gl->uniform_loc[val->index];

    switch (input->type) {
    case RA_VARTYPE_INT: {
        assert(input->dim_v * input->dim_m == 1);
        if (loc < 0)
            break;
        gl->Uniform1i(loc, *(int *)val->data);
        break;
    }
    case RA_VARTYPE_FLOAT: {
        float *f = val->data;
        if (loc < 0)
            break;
        if (input->dim_m == 1) {
            switch (input->dim_v) {
            case 1: gl->Uniform1f(loc, f[0]); break;
            case 2: gl->Uniform2f(loc, f[0], f[1]); break;
            case 3: gl->Uniform3f(loc, f[0], f[1], f[2]); break;
            case 4: gl->Uniform4f(loc, f[0], f[1], f[2], f[3]); break;
            default: abort();
            }
        } else if (input->dim_v == 2 && input->dim_m == 2) {
            gl->UniformMatrix2fv(loc, 1, GL_FALSE, f);
        } else if (input->dim_v == 3 && input->dim_m == 3) {
            gl->UniformMatrix3fv(loc, 1, GL_FALSE, f);
        } else {
            abort();
        }
        break;
    }
    case RA_VARTYPE_IMG_W: {
        struct ra_tex *tex = *(struct ra_tex **)val->data;
        struct ra_tex_gl *tex_gl = tex->priv;
        assert(tex->params.storage_dst);
        gl->BindImageTexture(input->binding, tex_gl->texture, 0, GL_FALSE, 0,
                             GL_WRITE_ONLY, tex_gl->internal_format);
        break;
    }
    case RA_VARTYPE_TEX: {
        struct ra_tex *tex = *(struct ra_tex **)val->data;
        struct ra_tex_gl *tex_gl = tex->priv;
        assert(tex->params.render_src);
        gl->ActiveTexture(GL_TEXTURE0 + input->binding);
        gl->BindTexture(tex_gl->target, tex_gl->texture);
        break;
    }
    case RA_VARTYPE_BUF_RO: // fall through
    case RA_VARTYPE_BUF_RW: {
        struct ra_buf *buf = *(struct ra_buf **)val->data;
        struct ra_buf_gl *buf_gl = buf->priv;
        gl->BindBufferBase(buf_gl->target, input->binding, buf_gl->buffer);
        // SSBOs are not implicitly coherent in OpengL
        if (input->type == RA_VARTYPE_BUF_RW)
            gl->MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        break;
    }
    default:
        abort();
    }
}

static void disable_binding(struct ra *ra, struct ra_renderpass *pass,
                           struct ra_renderpass_input_val *val)
{
    GL *gl = ra_gl_get(ra);

    struct ra_renderpass_input *input = &pass->params.inputs[val->index];

    switch (input->type) {
    case RA_VARTYPE_IMG_W: /* fall  through */
    case RA_VARTYPE_TEX: {
        struct ra_tex *tex = *(struct ra_tex **)val->data;
        struct ra_tex_gl *tex_gl = tex->priv;
        assert(tex->params.render_src);
        if (input->type == RA_VARTYPE_TEX) {
            gl->ActiveTexture(GL_TEXTURE0 + input->binding);
            gl->BindTexture(tex_gl->target, 0);
        } else {
            gl->BindImageTexture(input->binding, 0, 0, GL_FALSE, 0,
                                 GL_WRITE_ONLY, tex_gl->internal_format);
        }
        break;
    }
    case RA_VARTYPE_BUF_RW:
        gl->BindBufferBase(GL_SHADER_STORAGE_BUFFER, input->binding, 0);
        break;
    }
}

static void gl_renderpass_run(struct ra *ra,
                              const struct ra_renderpass_run_params *params)
{
    GL *gl = ra_gl_get(ra);
    struct ra_renderpass *pass = params->pass;
    struct ra_renderpass_gl *pass_gl = pass->priv;

    gl->UseProgram(pass_gl->program);

    for (int n = 0; n < params->num_values; n++)
        update_uniform(ra, pass, &params->values[n]);
    gl->ActiveTexture(GL_TEXTURE0);

    switch (pass->params.type) {
    case RA_RENDERPASS_TYPE_RASTER: {
        struct ra_tex_gl *target_gl = params->target->priv;
        assert(params->target->params.render_dst);
        assert(params->target->params.format == pass->params.target_format);
        gl->BindFramebuffer(GL_FRAMEBUFFER, target_gl->fbo);
        if (pass->params.invalidate_target && gl->InvalidateFramebuffer) {
            GLenum fb = target_gl->fbo ? GL_COLOR_ATTACHMENT0 : GL_COLOR;
            gl->InvalidateFramebuffer(GL_FRAMEBUFFER, 1, &fb);
        }
        gl->Viewport(params->viewport.x0, params->viewport.y0,
                     mp_rect_w(params->viewport),
                     mp_rect_h(params->viewport));
        gl->Scissor(params->scissors.x0, params->scissors.y0,
                    mp_rect_w(params->scissors),
                    mp_rect_h(params->scissors));
        gl->Enable(GL_SCISSOR_TEST);
        if (pass->params.enable_blend) {
            gl->BlendFuncSeparate(map_blend(pass->params.blend_src_rgb),
                                  map_blend(pass->params.blend_dst_rgb),
                                  map_blend(pass->params.blend_src_alpha),
                                  map_blend(pass->params.blend_dst_alpha));
            gl->Enable(GL_BLEND);
        }
        gl_vao_draw_data(&pass_gl->vao, GL_TRIANGLES, params->vertex_data,
                         params->vertex_count);
        gl->Disable(GL_SCISSOR_TEST);
        gl->Disable(GL_BLEND);
        gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
        break;
    }
    case RA_RENDERPASS_TYPE_COMPUTE: {
        gl->DispatchCompute(params->compute_groups[0],
                            params->compute_groups[1],
                            params->compute_groups[2]);

        gl->MemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
        break;
    }
    default: abort();
    }

    for (int n = 0; n < params->num_values; n++)
        disable_binding(ra, pass, &params->values[n]);
    gl->ActiveTexture(GL_TEXTURE0);

    gl->UseProgram(0);
}

// Timers in GL use query objects, and are asynchronous. So pool a few of
// these together. GL_QUERY_OBJECT_NUM should be large enough to avoid this
// ever blocking. We can afford to throw query objects around, there's no
// practical limit on them and their overhead is small.

#define GL_QUERY_OBJECT_NUM 8

struct gl_timer {
    GLuint query[GL_QUERY_OBJECT_NUM];
    int idx;
    uint64_t result;
    bool active;
};

static ra_timer *gl_timer_create(struct ra *ra)
{
    GL *gl = ra_gl_get(ra);

    if (!gl->GenQueries)
        return NULL;

    struct gl_timer *timer = talloc_zero(NULL, struct gl_timer);
    gl->GenQueries(GL_QUERY_OBJECT_NUM, timer->query);

    return (ra_timer *)timer;
}

static void gl_timer_destroy(struct ra *ra, ra_timer *ratimer)
{
    if (!ratimer)
        return;

    GL *gl = ra_gl_get(ra);
    struct gl_timer *timer = ratimer;

    gl->DeleteQueries(GL_QUERY_OBJECT_NUM, timer->query);
    talloc_free(timer);
}

static void gl_timer_start(struct ra *ra, ra_timer *ratimer)
{
    struct ra_gl *p = ra->priv;
    GL *gl = p->gl;
    struct gl_timer *timer = ratimer;

    // GL_TIME_ELAPSED queries are not re-entrant, so just do nothing instead
    // of crashing. Work-around for shitty GL limitations
    if (p->timer_active)
        return;

    // If this query object already contains a result, we need to retrieve it
    timer->result = 0;
    if (gl->IsQuery(timer->query[timer->idx])) {
        gl->GetQueryObjectui64v(timer->query[timer->idx], GL_QUERY_RESULT,
                                &timer->result);
    }

    gl->BeginQuery(GL_TIME_ELAPSED, timer->query[timer->idx++]);
    timer->idx %= GL_QUERY_OBJECT_NUM;

    p->timer_active = timer->active = true;
}

static uint64_t gl_timer_stop(struct ra *ra, ra_timer *ratimer)
{
    struct ra_gl *p = ra->priv;
    GL *gl = p->gl;
    struct gl_timer *timer = ratimer;

    if (!timer->active)
        return 0;

    gl->EndQuery(GL_TIME_ELAPSED);
    p->timer_active = timer->active = false;

    return timer->result;
}

static void gl_debug_marker(struct ra *ra, const char *msg)
{
    struct ra_gl *p = ra->priv;

    if (p->debug_enable)
        gl_check_error(p->gl, ra->log, msg);
}

static struct ra_fns ra_fns_gl = {
    .destroy                = gl_destroy,
    .tex_create             = gl_tex_create,
    .tex_destroy            = gl_tex_destroy,
    .tex_upload             = gl_tex_upload,
    .tex_download           = gl_tex_download,
    .buf_create             = gl_buf_create,
    .buf_destroy            = gl_buf_destroy,
    .buf_update             = gl_buf_update,
    .buf_poll               = gl_buf_poll,
    .clear                  = gl_clear,
    .blit                   = gl_blit,
    .uniform_layout         = std140_layout,
    .desc_namespace         = gl_desc_namespace,
    .renderpass_create      = gl_renderpass_create,
    .renderpass_destroy     = gl_renderpass_destroy,
    .renderpass_run         = gl_renderpass_run,
    .timer_create           = gl_timer_create,
    .timer_destroy          = gl_timer_destroy,
    .timer_start            = gl_timer_start,
    .timer_stop             = gl_timer_stop,
    .debug_marker           = gl_debug_marker,
};
