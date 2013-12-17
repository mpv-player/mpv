/*
 * CoreVideo video output driver
 * Copyright (c) 2005 Nicolas Plourde <nicolasplourde@gmail.com>
 * Copyright (c) 2012-2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <QuartzCore/QuartzCore.h>
#if HAVE_VDA_HWACCEL
#include <IOSurface/IOSurface.h>
#endif

#include <assert.h>

#include "talloc.h"
#include "video/out/vo.h"
#include "sub/osd.h"
#include "options/m_option.h"

#include "video/csputils.h"
#include "video/vfcap.h"
#include "video/mp_image.h"

#include "gl_common.h"
#include "gl_osd.h"
#include "cocoa_common.h"

struct quad {
    GLfloat lowerLeft[2];
    GLfloat lowerRight[2];
    GLfloat upperRight[2];
    GLfloat upperLeft[2];
};

struct cv_priv {
    CVPixelBufferRef pbuf;
    CVOpenGLTextureCacheRef texture_cache;
    CVOpenGLTextureRef texture;
    OSType pixfmt;
};

struct dr_priv {
    CVPixelBufferRef pbuf;
    bool texture_allocated;
    GLuint texture;
    GLuint texture_target;
};

struct cv_functions {
    void (*init)(struct vo *vo);
    void (*uninit)(struct vo *vo);
    void (*prepare_texture)(struct vo *vo, struct mp_image *mpi);
    void (*bind_texture)(struct vo *vo);
    void (*unbind_texture)(struct vo *vo);
    mp_image_t *(*get_screenshot)(struct vo *vo);
    int (*get_yuv_colorspace)(struct vo *vo, struct mp_csp_details *csp);
    int (*set_yuv_colorspace)(struct vo *vo, struct mp_csp_details *csp);
};

struct priv {
    MPGLContext *mpglctx;
    unsigned int image_width;
    unsigned int image_height;
    struct mp_csp_details colorspace;
    struct mp_rect src_rect;
    struct mp_rect dst_rect;
    struct mp_osd_res osd_res;

    // state for normal CoreVideo rendering path: uploads mp_image data as
    // OpenGL textures.
    struct cv_priv cv;

    // state for IOSurface based direct rendering path: accesses the IOSurface
    // wrapped by the CVPixelBuffer returned by VDADecoder and directly
    // renders it to the screen.
    struct dr_priv dr;

    struct quad *quad;
    struct mpgl_osd *osd;

    // functions to to deal with the the OpenGL texture for containing the
    // video frame (behaviour changes depending on the rendering path).
    struct cv_functions fns;
};

static void resize(struct vo *vo)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    gl->Viewport(0, 0, vo->dwidth, vo->dheight);
    gl->MatrixMode(GL_MODELVIEW);
    gl->LoadIdentity();
    gl->Ortho(0, vo->dwidth, vo->dheight, 0, -1, 1);

    vo_get_src_dst_rects(vo, &p->src_rect, &p->dst_rect, &p->osd_res);

    gl->Clear(GL_COLOR_BUFFER_BIT);
    vo->want_redraw = true;
}

static int init_gl(struct vo *vo, uint32_t d_width, uint32_t d_height)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    gl->Disable(GL_BLEND);
    gl->Disable(GL_DEPTH_TEST);
    gl->DepthMask(GL_FALSE);
    gl->Disable(GL_CULL_FACE);
    gl->Enable(GL_TEXTURE_2D);
    gl->DrawBuffer(GL_BACK);
    gl->TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    if (!p->osd)
        p->osd = mpgl_osd_init(gl, vo->log, true);

    resize(vo);

    gl->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (gl->SwapInterval)
        gl->SwapInterval(1);

    return 1;
}

static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct priv *p = vo->priv;
    p->fns.uninit(vo);

    p->image_width  = params->w;
    p->image_height = params->h;

    int mpgl_caps = MPGL_CAP_GL_LEGACY;
    if (!mpgl_config_window(
            p->mpglctx, mpgl_caps, vo->dwidth, vo->dheight, flags))
        return -1;

    init_gl(vo, vo->dwidth, vo->dheight);
    p->fns.init(vo);

    return 0;
}

// map x/y (in range 0..1) to the video texture, and emit OpenGL vertexes
static void video_vertex(struct vo *vo, float x, float y)
{
    struct priv *p = vo->priv;
    struct quad *q = p->quad;
    GL *gl = p->mpglctx->gl;

    double tx0 = q->upperLeft[0];
    double ty0 = q->upperLeft[1];
    double tw = q->lowerRight[0] - tx0;
    double th = q->lowerRight[1] - ty0;

    double sx0 = p->src_rect.x0 / (double)p->image_width;
    double sy0 = p->src_rect.y0 / (double)p->image_height;
    double sw = (p->src_rect.x1 - p->src_rect.x0) / (double)p->image_width;
    double sh = (p->src_rect.y1 - p->src_rect.y0) / (double)p->image_height;

    gl->TexCoord2f(tx0 + (sx0 + x * sw) * tw,
                   ty0 + (sy0 + y * sh) * th);
    gl->Vertex2f(p->dst_rect.x1 * x + p->dst_rect.x0 * (1 - x),
                 p->dst_rect.y1 * y + p->dst_rect.y0 * (1 - y));
}

static void do_render(struct vo *vo)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    p->fns.bind_texture(vo);

    gl->Begin(GL_QUADS);
    video_vertex(vo, 0, 0);
    video_vertex(vo, 0, 1);
    video_vertex(vo, 1, 1);
    video_vertex(vo, 1, 0);
    gl->End();

    p->fns.unbind_texture(vo);
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    p->mpglctx->swapGlBuffers(p->mpglctx);
    p->mpglctx->gl->Clear(GL_COLOR_BUFFER_BIT);
}

static void draw_image(struct vo *vo, struct mp_image *mpi)
{
    struct priv *p = vo->priv;
    p->fns.prepare_texture(vo, mpi);
    do_render(vo);
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->osd)
        mpgl_osd_destroy(p->osd);
    p->fns.uninit(vo);
    mpgl_uninit(p->mpglctx);
}


static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;

    *p = (struct priv) {
        .mpglctx = mpgl_init(vo, "cocoa"),
        .colorspace = MP_CSP_DETAILS_DEFAULTS,
        .quad = talloc_ptrtype(p, p->quad),
    };

    return 0;
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct priv *p = vo->priv;
    assert(p->osd);

    mpgl_osd_draw_legacy(p->osd, osd, p->osd_res);
}

static CFStringRef get_cv_csp_matrix(enum mp_csp format)
{
    switch (format) {
        case MP_CSP_BT_601:
            return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
        case MP_CSP_BT_709:
            return kCVImageBufferYCbCrMatrix_ITU_R_709_2;
        case MP_CSP_SMPTE_240M:
            return kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
        default:
            return NULL;
    }
}

static void apply_csp(struct vo *vo, CVPixelBufferRef pbuf)
{
    struct priv *p = vo->priv;
    CFStringRef matrix = get_cv_csp_matrix(p->colorspace.format);
    assert(matrix);

    CVPixelBufferLockBaseAddress(pbuf, 0);
    CVBufferSetAttachment(pbuf, kCVImageBufferYCbCrMatrixKey, matrix,
        kCVAttachmentMode_ShouldPropagate);
    CVPixelBufferUnlockBaseAddress(pbuf, 0);
}

static int get_yuv_colorspace(struct vo *vo, struct mp_csp_details *csp)
{
    struct priv *p = vo->priv;
    *(struct mp_csp_details *)csp = p->colorspace;
    return VO_TRUE;
}

static int get_image_fmt(struct vo *vo, CVPixelBufferRef pbuf)
{
    OSType pixfmt = CVPixelBufferGetPixelFormatType(pbuf);
    switch (pixfmt) {
        case kYUVSPixelFormat:   return IMGFMT_YUYV;
        case k2vuyPixelFormat:   return IMGFMT_UYVY;
        case k24RGBPixelFormat:  return IMGFMT_RGB24;
        case k32ARGBPixelFormat: return IMGFMT_ARGB;
        case k32BGRAPixelFormat: return IMGFMT_BGRA;
    }
    MP_ERR(vo, "Failed to convert pixel format. Please contact the "
               "developers. PixelFormat: %d\n", pixfmt);
    return -1;
}

static mp_image_t *get_screenshot(struct vo *vo, CVPixelBufferRef pbuf)
{
    int img_fmt = get_image_fmt(vo, pbuf);
    if (img_fmt < 0) return NULL;

    struct priv *p = vo->priv;
    CVPixelBufferLockBaseAddress(pbuf, 0);
    void *base = CVPixelBufferGetBaseAddress(pbuf);
    size_t width  = CVPixelBufferGetWidth(pbuf);
    size_t height = CVPixelBufferGetHeight(pbuf);
    size_t stride = CVPixelBufferGetBytesPerRow(pbuf);

    struct mp_image img = {0};
    mp_image_setfmt(&img, img_fmt);
    mp_image_set_size(&img, width, height);
    img.planes[0] = base;
    img.stride[0] = stride;

    struct mp_image *image = mp_image_new_copy(&img);
    mp_image_set_display_size(image, vo->aspdat.prew, vo->aspdat.preh);
    mp_image_set_colorspace_details(image, &p->colorspace);
    CVPixelBufferUnlockBaseAddress(pbuf, 0);

    return image;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
        case VOCTRL_GET_PANSCAN:
            return VO_TRUE;
        case VOCTRL_SET_PANSCAN:
            resize(vo);
            return VO_TRUE;
        case VOCTRL_REDRAW_FRAME:
            do_render(vo);
            return VO_TRUE;
        case VOCTRL_SET_YUV_COLORSPACE:
            return p->fns.set_yuv_colorspace(vo, data);
        case VOCTRL_GET_YUV_COLORSPACE:
            return p->fns.get_yuv_colorspace(vo, data);
        case VOCTRL_SCREENSHOT: {
            struct voctrl_screenshot_args *args = data;
            if (args->full_window)
                args->out_image = glGetWindowScreenshot(p->mpglctx->gl);
            else
                args->out_image = p->fns.get_screenshot(vo);
            return VO_TRUE;
        }
    }

    int events = 0;
    int r = p->mpglctx->vo_control(vo, &events, request, data);
    if (events & VO_EVENT_RESIZE)
        resize(vo);

    return r;
}

static void dummy_cb(struct vo *vo) { }

static void cv_uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    CVPixelBufferRelease(p->cv.pbuf);
    p->cv.pbuf = NULL;
    CVOpenGLTextureRelease(p->cv.texture);
    p->cv.texture = NULL;
    CVOpenGLTextureCacheRelease(p->cv.texture_cache);
    p->cv.texture_cache = NULL;
}

static void cv_bind_texture(struct vo *vo)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    gl->Enable(CVOpenGLTextureGetTarget(p->cv.texture));
    gl->BindTexture(CVOpenGLTextureGetTarget(p->cv.texture),
                    CVOpenGLTextureGetName(p->cv.texture));

}

static void cv_unbind_texture(struct vo *vo)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    gl->Disable(CVOpenGLTextureGetTarget(p->cv.texture));
}

static void upload_opengl_texture(struct vo *vo, struct mp_image *mpi)
{
    struct priv *p = vo->priv;
    if (!p->cv.texture_cache || !p->cv.pbuf) {
        CVReturn error;
        error = CVOpenGLTextureCacheCreate(NULL, 0, vo_cocoa_cgl_context(vo),
                    vo_cocoa_cgl_pixel_format(vo), 0, &p->cv.texture_cache);
        if(error != kCVReturnSuccess)
            MP_ERR(vo, "Failed to create OpenGL texture Cache(%d)\n", error);

        error = CVPixelBufferCreateWithBytes(NULL, mpi->w, mpi->h,
                    p->cv.pixfmt, mpi->planes[0], mpi->stride[0],
                    NULL, NULL, NULL, &p->cv.pbuf);
        if(error != kCVReturnSuccess)
            MP_ERR(vo, "Failed to create PixelBuffer(%d)\n", error);

        apply_csp(vo, p->cv.pbuf);
    }

    struct quad *q = p->quad;
    CVReturn error;

    CVOpenGLTextureRelease(p->cv.texture);
    error = CVOpenGLTextureCacheCreateTextureFromImage(NULL,
                p->cv.texture_cache, p->cv.pbuf, 0, &p->cv.texture);
    if(error != kCVReturnSuccess)
        MP_ERR(vo, "Failed to create OpenGL texture(%d)\n", error);

    CVOpenGLTextureGetCleanTexCoords(p->cv.texture,
            q->lowerLeft, q->lowerRight, q->upperRight, q->upperLeft);
}

static mp_image_t *cv_get_screenshot(struct vo *vo)
{
    struct priv *p = vo->priv;
    return get_screenshot(vo, p->cv.pbuf);
}

static int cv_set_yuv_colorspace(struct vo *vo, struct mp_csp_details *csp)
{
    struct priv *p = vo->priv;

    if (get_cv_csp_matrix(csp->format)) {
        p->colorspace = *csp;
        return VO_TRUE;
    } else
        return VO_NOTIMPL;
}

static struct cv_functions cv_functions = {
    .init               = dummy_cb,
    .uninit             = cv_uninit,
    .bind_texture       = cv_bind_texture,
    .unbind_texture     = cv_unbind_texture,
    .prepare_texture    = upload_opengl_texture,
    .get_screenshot     = cv_get_screenshot,
    .get_yuv_colorspace = get_yuv_colorspace,
    .set_yuv_colorspace = cv_set_yuv_colorspace,
};

#if HAVE_VDA_HWACCEL
static void iosurface_init(struct vo *vo)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    p->dr.texture_target = GL_TEXTURE_RECTANGLE_ARB;
    p->fns.bind_texture(vo);
    gl->GenTextures(1, &p->dr.texture);
    p->fns.unbind_texture(vo);

    p->dr.texture_allocated = true;
}

static void iosurface_uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;
    if (p->dr.texture_allocated) {
        gl->DeleteTextures(1, &p->dr.texture);
        p->dr.texture_allocated = false;
    }
}

static void iosurface_bind_texture(struct vo *vo)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    gl->Enable(p->dr.texture_target);
    gl->BindTexture(p->dr.texture_target, p->dr.texture);
    gl->MatrixMode(GL_TEXTURE);
    gl->LoadIdentity();
    gl->TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

static void iosurface_unbind_texture(struct vo *vo)
{
    struct priv *p = vo->priv;
    GL *gl = p->mpglctx->gl;

    gl->BindTexture(p->dr.texture_target, 0);
    gl->Disable(p->dr.texture_target);
}

static void extract_texture_from_iosurface(struct vo *vo, struct mp_image *mpi)
{
    struct priv *p = vo->priv;
    CVPixelBufferRelease(p->dr.pbuf);
    p->dr.pbuf = (CVPixelBufferRef)mpi->planes[3];
    CVPixelBufferRetain(p->dr.pbuf);
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(p->dr.pbuf);
    MP_DBG(vo, "iosurface id: %d\n", IOSurfaceGetID(surface));

    p->fns.bind_texture(vo);

    CGLError err = CGLTexImageIOSurface2D(
        vo_cocoa_cgl_context(vo), p->dr.texture_target, GL_RGB8,
        p->image_width, p->image_height,
        GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, surface, 0);

    if (err != kCGLNoError)
        MP_ERR(vo, "error creating IOSurface texture: %s (%x)\n",
               CGLErrorString(err), glGetError());

    p->fns.unbind_texture(vo);

    // video_vertex flips the coordinates.. so feed in a flipped quad
    *p->quad = (struct quad) {
        .lowerRight = { p->image_width, p->image_height },
        .upperLeft  = { 0.0, 0.0 },
    };
}

static mp_image_t *iosurface_get_screenshot(struct vo *vo)
{
    struct priv *p = vo->priv;
    return get_screenshot(vo, p->dr.pbuf);
}

static int iosurface_set_yuv_csp(struct vo *vo, struct mp_csp_details *csp)
{
    if (csp->format == MP_CSP_BT_601) {
        struct priv *p = vo->priv;
        p->colorspace = *csp;
        return VO_TRUE;
    } else
        return VO_NOTIMPL;
}

static struct cv_functions iosurface_functions = {
    .init               = iosurface_init,
    .uninit             = iosurface_uninit,
    .bind_texture       = iosurface_bind_texture,
    .unbind_texture     = iosurface_unbind_texture,
    .prepare_texture    = extract_texture_from_iosurface,
    .get_screenshot     = iosurface_get_screenshot,
    .get_yuv_colorspace = get_yuv_colorspace,
    .set_yuv_colorspace = iosurface_set_yuv_csp,
};
#endif /* HAVE_VDA_HWACCEL */

static int query_format(struct vo *vo, uint32_t format)
{
    struct priv *p = vo->priv;
    const int flags = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;

    switch (format) {
#if HAVE_VDA_HWACCEL
        case IMGFMT_VDA:
            p->fns = iosurface_functions;
            return flags;
#endif

        case IMGFMT_YUYV:
            p->fns       = cv_functions;
            p->cv.pixfmt = kYUVSPixelFormat;
            return flags;

        case IMGFMT_UYVY:
            p->fns       = cv_functions;
            p->cv.pixfmt = k2vuyPixelFormat;
            return flags;

        case IMGFMT_RGB24:
            p->fns       = cv_functions;
            p->cv.pixfmt = k24RGBPixelFormat;
            return flags;

        case IMGFMT_ARGB:
            p->fns       = cv_functions;
            p->cv.pixfmt = k32ARGBPixelFormat;
            return flags;

        case IMGFMT_BGRA:
            p->fns       = cv_functions;
            p->cv.pixfmt = k32BGRAPixelFormat;
            return flags;
    }
    return 0;
}

const struct vo_driver video_out_corevideo = {
    .name = "corevideo",
    .description = "Mac OS X Core Video",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
