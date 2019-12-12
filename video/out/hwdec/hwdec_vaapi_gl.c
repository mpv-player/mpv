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

#include "config.h"
#include "hwdec_vaapi.h"

#include <EGL/egl.h>
#include "video/out/opengl/ra_gl.h"

typedef void* GLeglImageOES;
typedef void *EGLImageKHR;

// Any EGL_EXT_image_dma_buf_import definitions used in this source file.
#define EGL_LINUX_DMA_BUF_EXT             0x3270
#define EGL_LINUX_DRM_FOURCC_EXT          0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT         0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT     0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT      0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT         0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT     0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT      0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT         0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT     0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT      0x327A

// Any EGL_EXT_image_dma_buf_import definitions used in this source file.
#define EGL_DMA_BUF_PLANE3_FD_EXT         0x3440
#define EGL_DMA_BUF_PLANE3_OFFSET_EXT     0x3441
#define EGL_DMA_BUF_PLANE3_PITCH_EXT      0x3442

struct vaapi_gl_mapper_priv {
    GLuint gl_textures[4];
    EGLImageKHR images[4];

    EGLImageKHR (EGLAPIENTRY *CreateImageKHR)(EGLDisplay, EGLContext,
                                              EGLenum, EGLClientBuffer,
                                              const EGLint *);
    EGLBoolean (EGLAPIENTRY *DestroyImageKHR)(EGLDisplay, EGLImageKHR);
    void (EGLAPIENTRY *EGLImageTargetTexture2DOES)(GLenum, GLeglImageOES);
};

static bool vaapi_gl_mapper_init(struct ra_hwdec_mapper *mapper,
                                 const struct ra_imgfmt_desc *desc)
{
    struct priv *p_mapper = mapper->priv;
    struct vaapi_gl_mapper_priv *p = talloc_ptrtype(NULL, p);
    p_mapper->interop_mapper_priv = p;

    // EGL_KHR_image_base
    p->CreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
    p->DestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
    // GL_OES_EGL_image
    p->EGLImageTargetTexture2DOES =
        (void *)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    if (!p->CreateImageKHR || !p->DestroyImageKHR ||
        !p->EGLImageTargetTexture2DOES)
        return false;

    GL *gl = ra_gl_get(mapper->ra);
    gl->GenTextures(4, p->gl_textures);
    for (int n = 0; n < desc->num_planes; n++) {
        gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->BindTexture(GL_TEXTURE_2D, 0);

        struct ra_tex_params params = {
            .dimensions = 2,
            .w = mp_image_plane_w(&p_mapper->layout, n),
            .h = mp_image_plane_h(&p_mapper->layout, n),
            .d = 1,
            .format = desc->planes[n],
            .render_src = true,
            .src_linear = true,
        };

        if (params.format->ctype != RA_CTYPE_UNORM)
            return false;

        p_mapper->tex[n] = ra_create_wrapped_tex(mapper->ra, &params,
                                                 p->gl_textures[n]);
        if (!p_mapper->tex[n])
            return false;
    }

    return true;
}

static void vaapi_gl_mapper_uninit(const struct ra_hwdec_mapper *mapper)
{
    struct priv *p_mapper = mapper->priv;
    struct vaapi_gl_mapper_priv *p = p_mapper->interop_mapper_priv;

    if (p) {
        GL *gl = ra_gl_get(mapper->ra);
        gl->DeleteTextures(4, p->gl_textures);
        for (int n = 0; n < 4; n++) {
            p->gl_textures[n] = 0;
            ra_tex_free(mapper->ra, &p_mapper->tex[n]);
        }
        talloc_free(p);
        p_mapper->interop_mapper_priv = NULL;
    }
}

#define ADD_ATTRIB(name, value)                         \
    do {                                                \
    assert(num_attribs + 3 < MP_ARRAY_SIZE(attribs));   \
    attribs[num_attribs++] = (name);                    \
    attribs[num_attribs++] = (value);                   \
    attribs[num_attribs] = EGL_NONE;                    \
    } while(0)

#define ADD_PLANE_ATTRIBS(plane) do { \
            ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _FD_EXT, \
                        p_mapper->desc.objects[p_mapper->desc.layers[n].object_index[plane]].fd); \
            ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _OFFSET_EXT, \
                        p_mapper->desc.layers[n].offset[plane]); \
            ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _PITCH_EXT, \
                        p_mapper->desc.layers[n].pitch[plane]); \
        } while (0)

static bool vaapi_gl_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p_mapper = mapper->priv;
    struct vaapi_gl_mapper_priv *p = p_mapper->interop_mapper_priv;

    GL *gl = ra_gl_get(mapper->ra);

    for (int n = 0; n < p_mapper->num_planes; n++) {
        int attribs[20] = {EGL_NONE};
        int num_attribs = 0;

        ADD_ATTRIB(EGL_LINUX_DRM_FOURCC_EXT, p_mapper->desc.layers[n].drm_format);
        ADD_ATTRIB(EGL_WIDTH,  p_mapper->tex[n]->params.w);
        ADD_ATTRIB(EGL_HEIGHT, p_mapper->tex[n]->params.h);

        ADD_PLANE_ATTRIBS(0);
        if (p_mapper->desc.layers[n].num_planes > 1)
            ADD_PLANE_ATTRIBS(1);
        if (p_mapper->desc.layers[n].num_planes > 2)
            ADD_PLANE_ATTRIBS(2);
        if (p_mapper->desc.layers[n].num_planes > 3)
            ADD_PLANE_ATTRIBS(3);

        p->images[n] = p->CreateImageKHR(eglGetCurrentDisplay(),
            EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
        if (!p->images[n])
            return false;

        gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
        p->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, p->images[n]);

        mapper->tex[n] = p_mapper->tex[n];
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);
    return true;
}

static void vaapi_gl_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv *p_mapper = mapper->priv;
    struct vaapi_gl_mapper_priv *p = p_mapper->interop_mapper_priv;

    if (p) {
        for (int n = 0; n < 4; n++) {
            if (p->images[n])
                p->DestroyImageKHR(eglGetCurrentDisplay(), p->images[n]);
            p->images[n] = 0;
        }
    }
}

bool vaapi_gl_init(const struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    if (!ra_is_gl(hw->ra)) {
        // This is not an OpenGL RA.
        return false;
    }

    if (!eglGetCurrentContext())
        return false;

    const char *exts = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    if (!exts)
        return false;

    GL *gl = ra_gl_get(hw->ra);
    if (!gl_check_extension(exts, "EGL_EXT_image_dma_buf_import") ||
        !gl_check_extension(exts, "EGL_KHR_image_base") ||
        !gl_check_extension(gl->extensions, "GL_OES_EGL_image") ||
        !(gl->mpgl_caps & MPGL_CAP_TEX_RG))
        return false;

    MP_VERBOSE(hw, "using VAAPI EGL interop\n");

    p->interop_init = vaapi_gl_mapper_init;
    p->interop_uninit = vaapi_gl_mapper_uninit;
    p->interop_map = vaapi_gl_map;
    p->interop_unmap = vaapi_gl_unmap;

    return true;
}
