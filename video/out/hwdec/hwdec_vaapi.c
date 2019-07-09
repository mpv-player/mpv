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
#include <unistd.h>

#include <va/va_drmcommon.h>

#include <libavutil/common.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

#include "config.h"

#include "video/out/gpu/hwdec.h"
#include "video/fmt-conversion.h"
#include "video/mp_image_pool.h"
#include "video/vaapi.h"

#if HAVE_VULKAN
#include "video/out/placebo/ra_pl.h"
#include "video/out/vulkan/common.h"
#endif

#if HAVE_GL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "video/out/opengl/common.h"
#include "video/out/opengl/ra_gl.h"
#include "libmpv/render_gl.h"

#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
#endif
#ifndef EGL_KHR_image
typedef void *EGLImageKHR;
#endif

#ifndef EGL_LINUX_DMA_BUF_EXT
#define EGL_LINUX_DMA_BUF_EXT             0x3270
#define EGL_LINUX_DRM_FOURCC_EXT          0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT         0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT     0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT      0x3274
#endif

#endif // HAVE_GL

#if HAVE_VAAPI_X11
#include <va/va_x11.h>

static VADisplay *create_x11_va_display(struct ra *ra)
{
    Display *x11 = ra_get_native_resource(ra, "x11");
    return x11 ? vaGetDisplay(x11) : NULL;
}
#endif

#if HAVE_VAAPI_WAYLAND
#include <va/va_wayland.h>

static VADisplay *create_wayland_va_display(struct ra *ra)
{
    struct wl_display *wl = ra_get_native_resource(ra, "wl");
    return wl ? vaGetDisplayWl(wl) : NULL;
}
#endif

#if HAVE_VAAPI_DRM
#include <va/va_drm.h>

static VADisplay *create_drm_va_display(struct ra *ra)
{
    mpv_opengl_drm_params *params = ra_get_native_resource(ra, "drm_params");
    if (!params || params->render_fd < 0)
        return NULL;

    return vaGetDisplayDRM(params->render_fd);
}
#endif

struct va_create_native {
    const char *name;
    VADisplay *(*create)(struct ra *ra);
};

static const struct va_create_native create_native_cbs[] = {
#if HAVE_VAAPI_X11
    {"x11",     create_x11_va_display},
#endif
#if HAVE_VAAPI_WAYLAND
    {"wayland", create_wayland_va_display},
#endif
#if HAVE_VAAPI_DRM
    {"drm",     create_drm_va_display},
#endif
};

static VADisplay *create_native_va_display(struct ra *ra, struct mp_log *log)
{
    for (int n = 0; n < MP_ARRAY_SIZE(create_native_cbs); n++) {
        const struct va_create_native *disp = &create_native_cbs[n];
        mp_verbose(log, "Trying to open a %s VA display...\n", disp->name);
        VADisplay *display = disp->create(ra);
        if (display)
            return display;
    }
    return NULL;
}

struct priv_owner {
    struct mp_vaapi_ctx *ctx;
    VADisplay *display;
    int *formats;
    bool probing_formats; // temporary during init
};

struct priv {
    int num_planes;
    struct mp_image layout;
    struct ra_tex *tex[4];
#if HAVE_GL
    GLuint gl_textures[4];
    EGLImageKHR images[4];
#endif
    VAImage current_image;
    bool buffer_acquired;
#if VA_CHECK_VERSION(1, 1, 0)
    bool esh_not_implemented;
    VADRMPRIMESurfaceDescriptor desc;
    bool surface_acquired;
#endif

#if HAVE_GL
    EGLImageKHR (EGLAPIENTRY *CreateImageKHR)(EGLDisplay, EGLContext,
                                              EGLenum, EGLClientBuffer,
                                              const EGLint *);
    EGLBoolean (EGLAPIENTRY *DestroyImageKHR)(EGLDisplay, EGLImageKHR);
    void (EGLAPIENTRY *EGLImageTargetTexture2DOES)(GLenum, GLeglImageOES);
#endif
};

static void determine_working_formats(struct ra_hwdec *hw);

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    if (p->ctx)
        hwdec_devices_remove(hw->devs, &p->ctx->hwctx);
    va_destroy(p->ctx);
}

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

#if HAVE_GL
    if (ra_is_gl(hw->ra)) {
        if (!eglGetCurrentContext())
            return -1;

        const char *exts = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
        if (!exts)
            return -1;

        GL *gl = ra_gl_get(hw->ra);
        if (!strstr(exts, "EXT_image_dma_buf_import") ||
            !strstr(exts, "EGL_KHR_image_base") ||
            !strstr(gl->extensions, "GL_OES_EGL_image") ||
            !(gl->mpgl_caps & MPGL_CAP_TEX_RG))
            return -1;

        MP_VERBOSE(hw, "using VAAPI EGL interop\n");
    }
#endif

#if HAVE_VULKAN
    if (ra_pl_get(hw->ra)) {
        const struct pl_gpu *gpu = ra_pl_get(hw->ra);
        if (!(gpu->import_caps.tex & PL_HANDLE_DMA_BUF)) {
            MP_VERBOSE(hw, "VAAPI Vulkan interop requires support for "
                           "dma_buf import in Vulkan.\n");
            return -1;
        }

        MP_VERBOSE(hw, "using VAAPI Vulkan interop\n");
    }
#endif

    p->display = create_native_va_display(hw->ra, hw->log);
    if (!p->display) {
        MP_VERBOSE(hw, "Could not create a VA display.\n");
        return -1;
    }

    p->ctx = va_initialize(p->display, hw->log, true);
    if (!p->ctx) {
        vaTerminate(p->display);
        return -1;
    }
    if (!p->ctx->av_device_ref) {
        MP_VERBOSE(hw, "libavutil vaapi code rejected the driver?\n");
        return -1;
    }

    if (hw->probing && va_guess_if_emulated(p->ctx)) {
        return -1;
    }

    determine_working_formats(hw);
    if (!p->formats || !p->formats[0]) {
        return -1;
    }

    p->ctx->hwctx.supported_formats = p->formats;
    p->ctx->hwctx.driver_name = hw->driver->name;
    hwdec_devices_add(hw->devs, &p->ctx->hwctx);
    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    VADisplay *display = p_owner->display;
    struct priv *p = mapper->priv;
    VAStatus status;
#if HAVE_GL
    bool is_gl = ra_is_gl(mapper->ra);
#endif
#if HAVE_VULKAN
    const struct pl_gpu *gpu = ra_pl_get(mapper->ra);
#endif

    for (int n = 0; n < 4; n++) {
#if HAVE_GL
        if (is_gl) {
            if (p->images[n])
                p->DestroyImageKHR(eglGetCurrentDisplay(), p->images[n]);
            p->images[n] = 0;
        }
#endif
#if HAVE_VULKAN
        if (gpu) {
            ra_tex_free(mapper->ra, &mapper->tex[n]);
        }
#endif
    }

#if VA_CHECK_VERSION(1, 1, 0)
    if (p->surface_acquired) {
        for (int n = 0; n < p->desc.num_objects; n++)
            close(p->desc.objects[n].fd);
        p->surface_acquired = false;
    }
#endif

    if (p->buffer_acquired) {
        status = vaReleaseBufferHandle(display, p->current_image.buf);
        CHECK_VA_STATUS(mapper, "vaReleaseBufferHandle()");
        p->buffer_acquired = false;
    }
    if (p->current_image.image_id != VA_INVALID_ID) {
        status = vaDestroyImage(display, p->current_image.image_id);
        CHECK_VA_STATUS(mapper, "vaDestroyImage()");
        p->current_image.image_id = VA_INVALID_ID;
    }
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
#if HAVE_GL
    struct priv *p = mapper->priv;

    if (ra_is_gl(mapper->ra)) {
        GL *gl = ra_gl_get(mapper->ra);
        gl->DeleteTextures(4, p->gl_textures);
        for (int n = 0; n < 4; n++) {
            p->gl_textures[n] = 0;
            ra_tex_free(mapper->ra, &p->tex[n]);
        }
    }
#endif
}

static bool check_fmt(struct ra_hwdec_mapper *mapper, int fmt)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    for (int n = 0; p_owner->formats && p_owner->formats[n]; n++) {
        if (p_owner->formats[n] == fmt)
            return true;
    }
    return false;
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
#if HAVE_GL
    bool is_gl = ra_is_gl(mapper->ra);
#endif

    p->current_image.buf = p->current_image.image_id = VA_INVALID_ID;

#if HAVE_GL
    if (is_gl) {
        // EGL_KHR_image_base
        p->CreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
        p->DestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
        // GL_OES_EGL_image
        p->EGLImageTargetTexture2DOES =
            (void *)eglGetProcAddress("glEGLImageTargetTexture2DOES");

        if (!p->CreateImageKHR || !p->DestroyImageKHR ||
            !p->EGLImageTargetTexture2DOES)
            return -1;
    }
#endif

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params.hw_subfmt = 0;

    struct ra_imgfmt_desc desc = {0};

    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &desc))
        return -1;

    p->num_planes = desc.num_planes;
    mp_image_set_params(&p->layout, &mapper->dst_params);

#if HAVE_GL
    if (is_gl) {
        GL *gl = ra_gl_get(mapper->ra);
        gl->GenTextures(4, p->gl_textures);
        for (int n = 0; n < desc.num_planes; n++) {
            gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
            gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            gl->BindTexture(GL_TEXTURE_2D, 0);

            struct ra_tex_params params = {
                .dimensions = 2,
                .w = mp_image_plane_w(&p->layout, n),
                .h = mp_image_plane_h(&p->layout, n),
                .d = 1,
                .format = desc.planes[n],
                .render_src = true,
                .src_linear = true,
            };

            if (params.format->ctype != RA_CTYPE_UNORM)
                return -1;

            p->tex[n] = ra_create_wrapped_tex(mapper->ra, &params,
                                              p->gl_textures[n]);
            if (!p->tex[n])
                return -1;
        }
    }
#endif

    if (!p_owner->probing_formats && !check_fmt(mapper, mapper->dst_params.imgfmt))
    {
        MP_FATAL(mapper, "unsupported VA image format %s\n",
                 mp_imgfmt_to_name(mapper->dst_params.imgfmt));
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
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
#if HAVE_GL
    bool is_gl = ra_is_gl(mapper->ra);
#endif
#if HAVE_VULKAN
    const struct pl_gpu *gpu = ra_pl_get(mapper->ra);
#endif
    VAStatus status;
    VADisplay *display = p_owner->display;

#if VA_CHECK_VERSION(1, 1, 0)
    if (p->esh_not_implemented)
        goto esh_failed;

    status = vaExportSurfaceHandle(display, va_surface_id(mapper->src),
                                   VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                   VA_EXPORT_SURFACE_READ_ONLY |
                                   VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                   &p->desc);
    if (!CHECK_VA_STATUS_LEVEL(mapper, "vaExportSurfaceHandle()",
                               p_owner->probing_formats ? MSGL_V : MSGL_ERR)) {
        if (status == VA_STATUS_ERROR_UNIMPLEMENTED)
            p->esh_not_implemented = true;
        goto esh_failed;
    }
    p->surface_acquired = true;

#if HAVE_GL
    if (is_gl) {
        GL *gl = ra_gl_get(mapper->ra);
        for (int n = 0; n < p->num_planes; n++) {
            int attribs[20] = {EGL_NONE};
            int num_attribs = 0;

            ADD_ATTRIB(EGL_LINUX_DRM_FOURCC_EXT, p->desc.layers[n].drm_format);
            ADD_ATTRIB(EGL_WIDTH,  p->tex[n]->params.w);
            ADD_ATTRIB(EGL_HEIGHT, p->tex[n]->params.h);

    #define ADD_PLANE_ATTRIBS(plane) do { \
                ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _FD_EXT, \
                           p->desc.objects[p->desc.layers[n].object_index[plane]].fd); \
                ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _OFFSET_EXT, \
                           p->desc.layers[n].offset[plane]); \
                ADD_ATTRIB(EGL_DMA_BUF_PLANE ## plane ## _PITCH_EXT, \
                           p->desc.layers[n].pitch[plane]); \
            } while (0)

            ADD_PLANE_ATTRIBS(0);
            if (p->desc.layers[n].num_planes > 1)
                ADD_PLANE_ATTRIBS(1);
            if (p->desc.layers[n].num_planes > 2)
                ADD_PLANE_ATTRIBS(2);
            if (p->desc.layers[n].num_planes > 3)
                ADD_PLANE_ATTRIBS(3);

            p->images[n] = p->CreateImageKHR(eglGetCurrentDisplay(),
                EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
            if (!p->images[n])
                goto esh_failed;

            gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
            p->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, p->images[n]);

            mapper->tex[n] = p->tex[n];
        }
        gl->BindTexture(GL_TEXTURE_2D, 0);
    }
#endif
#if HAVE_VULKAN
    if (gpu) {
        struct ra_imgfmt_desc desc = {0};
        if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &desc))
            goto esh_failed;

        for (int n = 0; n < p->num_planes; n++) {
            if (p->desc.layers[n].num_planes > 1) {
                // Should never happen because we request separate layers
                MP_ERR(mapper, "Multi-plane VA surfaces are not supported\n");
                goto esh_failed;
            }

            const struct ra_format *format = desc.planes[n];
            int id = p->desc.layers[n].object_index[0];
            int fd = p->desc.objects[id].fd;
            uint32_t size = p->desc.objects[id].size;
            uint32_t offset = p->desc.layers[n].offset[0];

            struct pl_tex_params tex_params = {
                .w = mp_image_plane_w(&p->layout, n),
                .h = mp_image_plane_h(&p->layout, n),
                .d = 0,
                .format = format->priv,
                .sampleable = true,
                .sample_mode = format->linear_filter ? PL_TEX_SAMPLE_LINEAR
                                                     : PL_TEX_SAMPLE_NEAREST,
                .import_handle = PL_HANDLE_DMA_BUF,
                .shared_mem = (struct pl_shared_mem) {
                    .handle = {
                        .fd = fd,
                    },
                    .size = size,
                    .offset = offset,
                },
            };

            const struct pl_tex *pltex = pl_tex_create(gpu, &tex_params);
            if (!pltex) {
                goto esh_failed;
            }

            struct ra_tex *ratex = talloc_ptrtype(NULL, ratex);
            int ret = mppl_wrap_tex(mapper->ra, pltex, ratex);
            if (!ret) {
                pl_tex_destroy(gpu, &pltex);
                talloc_free(ratex);
                goto esh_failed;
            }
            mapper->tex[n] = ratex;

            MP_TRACE(mapper, "Object %d with fd %d imported as %p\n",
                    id, fd, ratex);
        }
    }
#endif

    if (p->desc.fourcc == VA_FOURCC_YV12)
        MPSWAP(struct ra_tex*, mapper->tex[1], mapper->tex[2]);

    return 0;

esh_failed:
    if (p->surface_acquired) {
        for (int n = 0; n < p->desc.num_objects; n++)
            close(p->desc.objects[n].fd);
        p->surface_acquired = false;
    }
#endif // VA_CHECK_VERSION

#if HAVE_GL
    if (is_gl) {
        GL *gl = ra_gl_get(mapper->ra);
        VAImage *va_image = &p->current_image;
        status = vaDeriveImage(display, va_surface_id(mapper->src), va_image);
        if (!CHECK_VA_STATUS(mapper, "vaDeriveImage()"))
            goto err;

        VABufferInfo buffer_info = {.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME};
        status = vaAcquireBufferHandle(display, va_image->buf, &buffer_info);
        if (!CHECK_VA_STATUS(mapper, "vaAcquireBufferHandle()"))
            goto err;
        p->buffer_acquired = true;

        int drm_fmts[8] = {
            // 1 bytes per component, 1-4 components
            MKTAG('R', '8', ' ', ' '),       // DRM_FORMAT_R8
            MKTAG('G', 'R', '8', '8'),       // DRM_FORMAT_GR88
            0,                               // untested (DRM_FORMAT_RGB888?)
            0,                               // untested (DRM_FORMAT_RGBA8888?)
            // 2 bytes per component, 1-4 components
            MKTAG('R', '1', '6', ' '),       // proposed DRM_FORMAT_R16
            MKTAG('G', 'R', '3', '2'),       // proposed DRM_FORMAT_GR32
            0,                               // N/A
            0,                               // N/A
        };

        for (int n = 0; n < p->num_planes; n++) {
            int attribs[20] = {EGL_NONE};
            int num_attribs = 0;

            const struct ra_format *fmt = p->tex[n]->params.format;
            int n_comp = fmt->num_components;
            int comp_s = fmt->component_size[n] / 8;
            if (n_comp < 1 || n_comp > 3 || comp_s < 1 || comp_s > 2)
                goto err;
            int drm_fmt = drm_fmts[n_comp - 1 + (comp_s - 1) * 4];
            if (!drm_fmt)
                goto err;

            ADD_ATTRIB(EGL_LINUX_DRM_FOURCC_EXT, drm_fmt);
            ADD_ATTRIB(EGL_WIDTH, p->tex[n]->params.w);
            ADD_ATTRIB(EGL_HEIGHT, p->tex[n]->params.h);
            ADD_ATTRIB(EGL_DMA_BUF_PLANE0_FD_EXT, buffer_info.handle);
            ADD_ATTRIB(EGL_DMA_BUF_PLANE0_OFFSET_EXT, va_image->offsets[n]);
            ADD_ATTRIB(EGL_DMA_BUF_PLANE0_PITCH_EXT, va_image->pitches[n]);

            p->images[n] = p->CreateImageKHR(eglGetCurrentDisplay(),
                EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
            if (!p->images[n])
                goto err;

            gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
            p->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, p->images[n]);

            mapper->tex[n] = p->tex[n];
        }
        gl->BindTexture(GL_TEXTURE_2D, 0);

        if (va_image->format.fourcc == VA_FOURCC_YV12)
            MPSWAP(struct ra_tex*, mapper->tex[1], mapper->tex[2]);

        return 0;
    }
#endif
#if HAVE_VULKAN
    // Seems unnecessary to support Vulkan interop with old libva API.
    if (gpu) {
        mapper_unmap(mapper);
        goto err;
    }
#endif

err:
    if (!p_owner->probing_formats)
        MP_FATAL(mapper, "mapping VAAPI EGL image failed\n");
    return -1;
}

static bool try_format_map(struct ra_hwdec *hw, struct mp_image *surface)
{
    bool ok = false;
    struct ra_hwdec_mapper *mapper = ra_hwdec_mapper_create(hw, &surface->params);
    if (mapper)
        ok = ra_hwdec_mapper_map(mapper, surface) >= 0;
    ra_hwdec_mapper_free(&mapper);
    return ok;
}

static void try_format_pixfmt(struct ra_hwdec *hw, enum AVPixelFormat pixfmt)
{
    struct priv_owner *p = hw->priv;

    int mp_fmt = pixfmt2imgfmt(pixfmt);
    if (!mp_fmt)
        return;

    int num_formats = 0;
    for (int n = 0; p->formats && p->formats[n]; n++) {
        if (p->formats[n] == mp_fmt)
            return; // already added
        num_formats += 1;
    }

    AVBufferRef *fref = NULL;
    struct mp_image *s = NULL;
    AVFrame *frame = NULL;
    fref = av_hwframe_ctx_alloc(p->ctx->av_device_ref);
    if (!fref)
        goto err;
    AVHWFramesContext *fctx = (void *)fref->data;
    fctx->format = AV_PIX_FMT_VAAPI;
    fctx->sw_format = pixfmt;
    fctx->width = 128;
    fctx->height = 128;
    if (av_hwframe_ctx_init(fref) < 0)
        goto err;
    frame = av_frame_alloc();
    if (!frame)
        goto err;
    if (av_hwframe_get_buffer(fref, frame, 0) < 0)
        goto err;
    s = mp_image_from_av_frame(frame);
    if (!s || !mp_image_params_valid(&s->params))
        goto err;
    if (try_format_map(hw, s)) {
        MP_TARRAY_APPEND(p, p->formats, num_formats, mp_fmt);
        MP_TARRAY_APPEND(p, p->formats, num_formats, 0); // terminate it
    }
err:
    talloc_free(s);
    av_frame_free(&frame);
    av_buffer_unref(&fref);
}

static void try_format_config(struct ra_hwdec *hw, AVVAAPIHWConfig *hwconfig)
{
    struct priv_owner *p = hw->priv;
    AVHWFramesConstraints *fc =
            av_hwdevice_get_hwframe_constraints(p->ctx->av_device_ref, hwconfig);
    if (!fc) {
        MP_WARN(hw, "failed to retrieve libavutil frame constraints\n");
        return;
    }
    for (int n = 0; fc->valid_sw_formats &&
                    fc->valid_sw_formats[n] != AV_PIX_FMT_NONE; n++)
        try_format_pixfmt(hw, fc->valid_sw_formats[n]);
    av_hwframe_constraints_free(&fc);
}

static void determine_working_formats(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    VAStatus status;
    VAProfile *profiles = NULL;
    VAEntrypoint *entrypoints = NULL;

    p->probing_formats = true;

    AVVAAPIHWConfig *hwconfig = av_hwdevice_hwconfig_alloc(p->ctx->av_device_ref);
    if (!hwconfig) {
        MP_WARN(hw, "Could not allocate FFmpeg AVVAAPIHWConfig\n");
        goto done;
    }

    profiles = talloc_zero_array(NULL, VAProfile, vaMaxNumProfiles(p->display));
    entrypoints = talloc_zero_array(NULL, VAEntrypoint,
                                    vaMaxNumEntrypoints(p->display));
    int num_profiles = 0;
    status = vaQueryConfigProfiles(p->display, profiles, &num_profiles);
    if (!CHECK_VA_STATUS(hw, "vaQueryConfigProfiles()"))
        num_profiles = 0;

    for (int n = 0; n < num_profiles; n++) {
        VAProfile profile = profiles[n];
        int num_ep = 0;
        status = vaQueryConfigEntrypoints(p->display, profile, entrypoints,
                                          &num_ep);
        if (!CHECK_VA_STATUS(hw, "vaQueryConfigEntrypoints()"))
            continue;
        for (int ep = 0; ep < num_ep; ep++) {
            VAConfigID config = VA_INVALID_ID;
            status = vaCreateConfig(p->display, profile, entrypoints[ep],
                                    NULL, 0, &config);
            if (status != VA_STATUS_SUCCESS) {
                MP_VERBOSE(hw, "vaCreateConfig(): '%s' for profile %d",
                        vaErrorStr(status), (int)profile);
                continue;
            }

            hwconfig->config_id = config;
            try_format_config(hw, hwconfig);

            vaDestroyConfig(p->display, config);
        }
    }

done:
    av_free(hwconfig);
    talloc_free(profiles);
    talloc_free(entrypoints);

    p->probing_formats = false;

    MP_VERBOSE(hw, "Supported formats:\n");
    for (int n = 0; p->formats && p->formats[n]; n++)
        MP_VERBOSE(hw, " %s\n", mp_imgfmt_to_name(p->formats[n]));
}

const struct ra_hwdec_driver ra_hwdec_vaegl = {
    .name = "vaapi-egl",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_VAAPI, 0},
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
