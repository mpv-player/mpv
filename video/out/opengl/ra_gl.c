#include "formats.h"

#include "ra_gl.h"

static struct ra_fns ra_fns_gl;

int ra_init_gl(struct ra *ra, GL *gl)
{
    if (gl->version < 210 && gl->es < 200) {
        MP_ERR(ra, "At least OpenGL 2.1 or OpenGL ES 2.0 required.\n");
        return -1;
    }

    struct ra_gl *p = ra->priv = talloc_zero(NULL, struct ra_gl);
    p->gl = gl;

    ra->fns = &ra_fns_gl;
    ra->caps = 0;
    if (gl->mpgl_caps & MPGL_CAP_1D_TEX)
        ra->caps |= RA_CAP_TEX_1D;
    if (gl->mpgl_caps & MPGL_CAP_3D_TEX)
        ra->caps |= RA_CAP_TEX_3D;

    int gl_fmt_features = gl_format_feature_flags(gl);

    // Test whether we can use 10 bit.
    int depth16 = gl_determine_16bit_tex_depth(gl);
    MP_VERBOSE(ra, "16 bit texture depth: %d.\n", depth16);

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
            .pixel_size     = gl_bytes_per_pixel(gl_fmt->format, gl_fmt->type),
            .luminance_alpha = gl_fmt->format == GL_LUMINANCE_ALPHA,
            .linear_filter  = gl_fmt->flags & F_TF,
            .renderable     = gl_fmt->flags & F_CR,
        };

        int csize = gl_component_size(gl_fmt->type) * 8;
        int depth = csize;
        if (fmt->ctype == RA_CTYPE_UNORM)
            depth = MPMIN(csize, depth16); // naive/approximate
        if (gl_fmt->flags & F_F16) {
            depth = 16;
            csize = 32; // always upload as GL_FLOAT (simpler for us)
        }

        for (int i = 0; i < fmt->num_components; i++) {
            fmt->component_size[i] = csize;
            fmt->component_depth[i] = depth;
        }

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
        if (strcmp(fmt->name, "ashit") == 0) {
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

        MP_TARRAY_APPEND(ra, ra->formats, ra->num_formats, fmt);
    }

    gl->Disable(GL_DITHER);

    return 0;
}

static void gl_destroy(struct ra *ra)
{
    talloc_free(ra->priv);
}

static void gl_tex_destroy(struct ra *ra, struct ra_tex *tex)
{
    struct ra_gl *p = ra->priv;
    struct ra_tex_gl *tex_gl = tex->priv;

    p->gl->DeleteTextures(1, &tex_gl->texture);
    gl_pbo_upload_uninit(&tex_gl->pbo);
    talloc_free(tex_gl);
    talloc_free(tex);
}

static struct ra_tex *gl_tex_create(struct ra *ra,
                                    const struct ra_tex_params *params)
{
    struct ra_gl *p = ra->priv;
    GL *gl = p->gl;

    struct ra_tex *tex = talloc_zero(NULL, struct ra_tex);
    tex->params = *params;
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

    tex->params.initial_data = NULL;

    return tex;
}

static void gl_tex_upload(struct ra *ra, struct ra_tex *tex,
                         const void *src, ptrdiff_t stride,
                         struct ra_mapped_buffer *buf)
{
    struct ra_gl *p = ra->priv;
    GL *gl = p->gl;
    struct ra_tex_gl *tex_gl = tex->priv;
    struct ra_mapped_buffer_gl *buf_gl = NULL;

    if (buf) {
        buf_gl = buf->priv;
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, buf_gl->pbo);
        src = (void *)((uintptr_t)src - (uintptr_t)buf->data);
    }

    gl->BindTexture(tex_gl->target, tex_gl->texture);

    switch (tex->params.dimensions) {
    case 1:
        gl->TexImage1D(tex_gl->target, 0, tex_gl->internal_format,
                       tex->params.w, 0, tex_gl->format, tex_gl->type, src);
        break;
    case 2:
        gl_pbo_upload_tex(&tex_gl->pbo, gl, tex->use_pbo && !buf,
                          tex_gl->target, tex_gl->format, tex_gl->type,
                          tex->params.w, tex->params.h, src, stride,
                          0, 0, tex->params.w, tex->params.h);
        break;
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
        // Make sure the PBO is not reused until GL is done with it. If a
        // previous operation is pending, "update" it by creating a new
        // fence that will cover the previous operation as well.
        gl->DeleteSync(buf_gl->fence);
        buf_gl->fence = gl->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
}

static void gl_destroy_mapped_buffer(struct ra *ra, struct ra_mapped_buffer *buf)
{
    struct ra_gl *p = ra->priv;
    GL *gl = p->gl;
    struct ra_mapped_buffer_gl *buf_gl = buf->priv;

    gl->DeleteSync(buf_gl->fence);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, buf_gl->pbo);
    if (buf->data)
        gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    gl->DeleteBuffers(1, &buf_gl->pbo);

    talloc_free(buf_gl);
    talloc_free(buf);
}

static struct ra_mapped_buffer *gl_create_mapped_buffer(struct ra *ra,
                                                        size_t size)
{
    struct ra_gl *p = ra->priv;
    GL *gl = p->gl;

    if (gl->version < 440)
        return NULL;

    struct ra_mapped_buffer *buf = talloc_zero(NULL, struct ra_mapped_buffer);
    buf->size = size;

    struct ra_mapped_buffer_gl *buf_gl = buf->priv =
        talloc_zero(NULL, struct ra_mapped_buffer_gl);

    unsigned flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT |
                     GL_MAP_COHERENT_BIT;

    gl->GenBuffers(1, &buf_gl->pbo);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, buf_gl->pbo);
    gl->BufferStorage(GL_PIXEL_UNPACK_BUFFER, size, NULL, flags);
    buf->data = gl->MapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, buf->size, flags);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    if (!buf->data) {
        gl_check_error(gl, ra->log, "mapping buffer");
        gl_destroy_mapped_buffer(ra, buf);
        return NULL;
    }

    return buf;
}

static bool gl_poll_mapped_buffer(struct ra *ra, struct ra_mapped_buffer *buf)
{
    struct ra_gl *p = ra->priv;
    GL *gl = p->gl;
    struct ra_mapped_buffer_gl *buf_gl = buf->priv;

    if (buf_gl->fence) {
        GLenum res = gl->ClientWaitSync(buf_gl->fence, 0, 0); // non-blocking
        if (res == GL_ALREADY_SIGNALED) {
            gl->DeleteSync(buf_gl->fence);
            buf_gl->fence = NULL;
        }
    }

    return !buf_gl->fence;
}

static struct ra_fns ra_fns_gl = {
    .destroy                = gl_destroy,
    .tex_create             = gl_tex_create,
    .tex_destroy            = gl_tex_destroy,
    .tex_upload             = gl_tex_upload,
    .create_mapped_buffer   = gl_create_mapped_buffer,
    .destroy_mapped_buffer  = gl_destroy_mapped_buffer,
    .poll_mapped_buffer     = gl_poll_mapped_buffer,
};

