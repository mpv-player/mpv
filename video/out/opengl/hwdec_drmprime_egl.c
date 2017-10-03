/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <drm_fourcc.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "video/hwdec.h"
#include "video/out/gpu/hwdec.h"
#include "libavutil/hwcontext_drm.h"
#include "ra_gl.h"

#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
#endif

#define MAX_NUM_PLANES  4

static const EGLint egl_dmabuf_plane_fd_attr[MAX_NUM_PLANES] = {
        EGL_DMA_BUF_PLANE0_FD_EXT,
        EGL_DMA_BUF_PLANE1_FD_EXT,
        EGL_DMA_BUF_PLANE2_FD_EXT,
        EGL_DMA_BUF_PLANE3_FD_EXT,
};
static const EGLint egl_dmabuf_plane_offset_attr[MAX_NUM_PLANES] = {
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
        EGL_DMA_BUF_PLANE1_OFFSET_EXT,
        EGL_DMA_BUF_PLANE2_OFFSET_EXT,
        EGL_DMA_BUF_PLANE3_OFFSET_EXT,
};
static const EGLint egl_dmabuf_plane_pitch_attr[MAX_NUM_PLANES] = {
        EGL_DMA_BUF_PLANE0_PITCH_EXT,
        EGL_DMA_BUF_PLANE1_PITCH_EXT,
        EGL_DMA_BUF_PLANE2_PITCH_EXT,
        EGL_DMA_BUF_PLANE3_PITCH_EXT,
};

struct priv {
    int num_planes;
    struct ra_tex *tex[MAX_NUM_PLANES];
    GLuint gl_textures[MAX_NUM_PLANES];
    EGLImageKHR images[MAX_NUM_PLANES];

    EGLImageKHR (EGLAPIENTRY *CreateImageKHR)(EGLDisplay, EGLContext,
                                              EGLenum, EGLClientBuffer,
                                              const EGLint *);
    EGLBoolean (EGLAPIENTRY *DestroyImageKHR)(EGLDisplay, EGLImageKHR);
    void (EGLAPIENTRY *EGLImageTargetTexture2DOES)(GLenum, GLeglImageOES);
};

static void uninit(struct ra_hwdec *hw)
{
}

static int get_egl_mp_image_colorspace_attr(struct mp_image *mpi)
{
    switch(mpi->params.color.space) {
        case MP_CSP_BT_601 :
            return EGL_ITU_REC601_EXT;

        case MP_CSP_BT_709 :
            return EGL_ITU_REC709_EXT;

        case MP_CSP_BT_2020_C :
        case MP_CSP_BT_2020_NC :
            return EGL_ITU_REC2020_EXT;
        default:
            return EGL_ITU_REC601_EXT;
    }
}

static int init(struct ra_hwdec *hw)
{
    if (!ra_is_gl(hw->ra) || !eglGetCurrentContext())
        return -1;

    const char *exts = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    if (!exts)
        return -1;

    GL *gl = ra_gl_get(hw->ra);
    if (!strstr(exts, "EXT_image_dma_buf_import") ||
        !strstr(exts, "EGL_KHR_image_base") ||
        !strstr(gl->extensions, "GL_OES_EGL_image"))
    {
        MP_ERR(hw, "EGL doesn't support the following extensions : "
                   "EXT_image_dma_buf_import, EGL_KHR_image_base, "
                   "GL_OES_EGL_image\n");
        return -1;
    }

    static const char *gles_exts[] = {"GL_OES_EGL_image_external", 0};
    hw->glsl_extensions = gles_exts;

    MP_VERBOSE(hw, "using RKMPP EGL interop\n");

    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    for (int i = 0; i < MAX_NUM_PLANES; i++) {
        if (p->images[i])
            p->DestroyImageKHR(eglGetCurrentDisplay(), p->images[i]);
        p->images[i] = 0;
    }
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    gl->DeleteTextures(MAX_NUM_PLANES, p->gl_textures);
    for (int i = 0; i < MAX_NUM_PLANES; i++) {
        p->gl_textures[i] = 0;
        ra_tex_free(mapper->ra, &p->tex[i]);
    }
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    // EGL_KHR_image_base
    p->CreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
    p->DestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
    // GL_OES_EGL_image
    p->EGLImageTargetTexture2DOES =
        (void *)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    if (!p->CreateImageKHR || !p->DestroyImageKHR ||
        !p->EGLImageTargetTexture2DOES)
        return -1;

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = IMGFMT_RGB0;
    mapper->dst_params.hw_subfmt = 0;

    struct ra_imgfmt_desc desc = {0};
    struct mp_image layout = {0};

    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &desc))
        return -1;

    p->num_planes = desc.num_planes;
    mp_image_set_params(&layout, &mapper->dst_params);

    gl->GenTextures(MAX_NUM_PLANES, p->gl_textures);
    for (int n = 0; n < desc.num_planes; n++) {
        gl->BindTexture(GL_TEXTURE_EXTERNAL_OES, p->gl_textures[n]);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->BindTexture(GL_TEXTURE_2D, 0);

        struct ra_tex_params params = {
            .dimensions = 2,
            .w = mp_image_plane_w(&layout, n),
            .h = mp_image_plane_h(&layout, n),
            .d = 1,
            .format = desc.planes[n],
            .render_src = true,
            .src_linear = true,
            .external_oes = true,
        };

        if (params.format->ctype != RA_CTYPE_UNORM)
            return -1;

        p->tex[n] = ra_create_wrapped_tex(mapper->ra, &params,
                                          p->gl_textures[n]);
        if (!p->tex[n])
            return -1;
    }

    return 0;
}

#define ADD_ATTRIB(name, value)                         \
    do {                                                \
    assert(num_attribs + 3 < MP_ARRAY_SIZE(attribs));   \
    attribs[num_attribs++] = (name);                    \
    attribs[num_attribs++] = (value);                   \
    attribs[num_attribs] = EGL_NONE;                    \
    } while(0)

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);
    AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *)mapper->src->planes[0];
    AVDRMLayerDescriptor *layer = NULL;
    if (!desc)
        goto err;

    mapper_unmap(mapper);

    for (int l = 0; l < desc->nb_layers; l++) {
        int attribs[40] = {EGL_NONE};
        int num_attribs = 0;
        layer = &desc->layers[l];

        ADD_ATTRIB(EGL_LINUX_DRM_FOURCC_EXT, layer->format);
        ADD_ATTRIB(EGL_WIDTH,  p->tex[l]->params.w);
        ADD_ATTRIB(EGL_HEIGHT, p->tex[l]->params.h);

        for (int plane=0; plane < layer->nb_planes; plane++) {
            ADD_ATTRIB(egl_dmabuf_plane_fd_attr[plane],
                       desc->objects[layer->planes[plane].object_index].fd);
            ADD_ATTRIB(egl_dmabuf_plane_offset_attr[plane],
                       layer->planes[plane].offset);
            ADD_ATTRIB(egl_dmabuf_plane_pitch_attr[plane],
                       layer->planes[plane].pitch);
        }

        ADD_ATTRIB(EGL_YUV_COLOR_SPACE_HINT_EXT,
                   get_egl_mp_image_colorspace_attr(mapper->src));
        ADD_ATTRIB(EGL_SAMPLE_RANGE_HINT_EXT, EGL_YUV_FULL_RANGE_EXT);

        p->images[l] = p->CreateImageKHR(eglGetCurrentDisplay(),
                EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
        if (p->images[l] == EGL_NO_IMAGE_KHR) {
            MP_FATAL(mapper, "Failed to CreateImageKHR (%x)\n", eglGetError());
            goto err;
        }

        gl->ActiveTexture(GL_TEXTURE0);
        gl->BindTexture(GL_TEXTURE_EXTERNAL_OES, p->gl_textures[l]);
        p->EGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, p->images[l]);

        mapper->tex[l] = p->tex[l];
    }

    gl->BindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    return 0;

err:
    mapper_unmap(mapper);
    return -1;
}

const struct ra_hwdec_driver ra_hwdec_drmprime_egl = {
    .name = "drmprime-egl",
    .api = HWDEC_RKMPP,
    .imgfmts = {IMGFMT_DRMPRIME, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
