/*
 * CoreVideo video output driver
 * Copyright (c) 2005 Nicolas Plourde <nicolasplourde@gmail.com>
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

#include <assert.h>

#import "vo_corevideo.h"

// mplayer includes
#import "talloc.h"
#import "vo.h"
#import "aspect.h"
#import "sub/sub.h"
#import "core/subopt-helper.h"

#import "video/csputils.h"
#import "video/vfcap.h"
#import "video/mp_image.h"

#import "gl_common.h"
#import "gl_osd.h"
#import "cocoa_common.h"

struct quad {
    GLfloat lowerLeft[2];
    GLfloat lowerRight[2];
    GLfloat upperRight[2];
    GLfloat upperLeft[2];
};

struct priv {
    MPGLContext *mpglctx;
    OSType pixelFormat;
    unsigned int image_width;
    unsigned int image_height;
    struct mp_csp_details colorspace;
    struct mp_rect src_rect;
    struct mp_rect dst_rect;
    struct mp_osd_res osd_res;

    CVPixelBufferRef pixelBuffer;
    CVOpenGLTextureCacheRef textureCache;
    CVOpenGLTextureRef texture;
    struct quad *quad;

    struct mpgl_osd *osd;
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

    const char *vendor     = gl->GetString(GL_VENDOR);
    const char *version    = gl->GetString(GL_VERSION);
    const char *renderer   = gl->GetString(GL_RENDERER);

    mp_msg(MSGT_VO, MSGL_V, "[vo_corevideo] Running on OpenGL '%s' by '%s',"
           " version '%s'\n", renderer, vendor, version);

    gl->Disable(GL_BLEND);
    gl->Disable(GL_DEPTH_TEST);
    gl->DepthMask(GL_FALSE);
    gl->Disable(GL_CULL_FACE);
    gl->Enable(GL_TEXTURE_2D);
    gl->DrawBuffer(GL_BACK);
    gl->TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    if (!p->osd)
        p->osd = mpgl_osd_init(gl, true);

    resize(vo);

    gl->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (gl->SwapInterval)
        gl->SwapInterval(1);
    return 1;
}

static void release_cv_entities(struct vo *vo) {
    struct priv *p = vo->priv;
    CVPixelBufferRelease(p->pixelBuffer);
    p->pixelBuffer = NULL;
    CVOpenGLTextureRelease(p->texture);
    p->texture = NULL;
    CVOpenGLTextureCacheRelease(p->textureCache);
    p->textureCache = NULL;
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct priv *p = vo->priv;
    release_cv_entities(vo);
    p->image_width = width;
    p->image_height = height;

    int mpgl_caps = MPGL_CAP_GL_LEGACY;
    if (!mpgl_config_window(p->mpglctx, mpgl_caps, d_width, d_height, flags))
        return -1;

    init_gl(vo, vo->dwidth, vo->dheight);

    return 0;
}

static void check_events(struct vo *vo)
{
    struct priv *p = vo->priv;
    int e = p->mpglctx->check_events(vo);
    if (e & VO_EVENT_RESIZE)
        resize(vo);
}

static void prepare_texture(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct quad *q = p->quad;
    CVReturn error;

    CVOpenGLTextureRelease(p->texture);
    error = CVOpenGLTextureCacheCreateTextureFromImage(NULL,
                p->textureCache, p->pixelBuffer, 0, &p->texture);
    if(error != kCVReturnSuccess)
        mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL"
                                 " texture(%d)\n", error);

    CVOpenGLTextureGetCleanTexCoords(p->texture, q->lowerLeft, q->lowerRight,
                                                 q->upperRight, q->upperLeft);
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
    prepare_texture(vo);

    gl->Enable(CVOpenGLTextureGetTarget(p->texture));
    gl->BindTexture(
            CVOpenGLTextureGetTarget(p->texture),
            CVOpenGLTextureGetName(p->texture));

    gl->Begin(GL_QUADS);
    video_vertex(vo, 0, 0);
    video_vertex(vo, 0, 1);
    video_vertex(vo, 1, 1);
    video_vertex(vo, 1, 0);
    gl->End();

    gl->Disable(CVOpenGLTextureGetTarget(p->texture));
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    p->mpglctx->swapGlBuffers(p->mpglctx);
    p->mpglctx->gl->Clear(GL_COLOR_BUFFER_BIT);
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;
    CVReturn error;

    if (!p->textureCache || !p->pixelBuffer) {
        error = CVOpenGLTextureCacheCreate(NULL, 0, vo_cocoa_cgl_context(vo),
                    vo_cocoa_cgl_pixel_format(vo), 0, &p->textureCache);
        if(error != kCVReturnSuccess)
            mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL"
                                     " texture Cache(%d)\n", error);

        error = CVPixelBufferCreateWithBytes(NULL, mpi->w, mpi->h,
                    p->pixelFormat, mpi->planes[0], mpi->stride[0],
                    NULL, NULL, NULL, &p->pixelBuffer);
        if(error != kCVReturnSuccess)
            mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create Pixel"
                                     "Buffer(%d)\n", error);
    }

    do_render(vo);
}

static int query_format(struct vo *vo, uint32_t format)
{
    struct priv *p = vo->priv;
    const int flags = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
    switch (format) {
        case IMGFMT_YUYV:
            p->pixelFormat = kYUVSPixelFormat;
            return flags;

        case IMGFMT_RGB24:
            p->pixelFormat = k24RGBPixelFormat;
            return flags;

        case IMGFMT_ARGB:
            p->pixelFormat = k32ARGBPixelFormat;
            return flags;

        case IMGFMT_BGRA:
            p->pixelFormat = k32BGRAPixelFormat;
            return flags;
    }
    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->osd)
        mpgl_osd_destroy(p->osd);
    release_cv_entities(vo);
    mpgl_uninit(p->mpglctx);
}


static int preinit(struct vo *vo, const char *arg)
{
    struct priv *p = vo->priv;

    *p = (struct priv) {
        .mpglctx = mpgl_init(GLTYPE_COCOA, vo),
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

static CFStringRef get_cv_csp_matrix(struct vo *vo)
{
    struct priv *p = vo->priv;
    switch (p->colorspace.format) {
        case MP_CSP_BT_601:
            return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
        case MP_CSP_BT_709:
            return kCVImageBufferYCbCrMatrix_ITU_R_709_2;
        case MP_CSP_SMPTE_240M:
            return kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
        default:
            return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
    }
}

static void set_yuv_colorspace(struct vo *vo)
{
    struct priv *p = vo->priv;
    CVBufferSetAttachment(p->pixelBuffer,
                          kCVImageBufferYCbCrMatrixKey, get_cv_csp_matrix(vo),
                          kCVAttachmentMode_ShouldPropagate);
    vo->want_redraw = true;
}

static int get_image_fmt(struct vo *vo)
{
    struct priv *p = vo->priv;
    switch (p->pixelFormat) {
        case kYUVSPixelFormat:   return IMGFMT_YUYV;
        case k24RGBPixelFormat:  return IMGFMT_RGB24;
        case k32ARGBPixelFormat: return IMGFMT_ARGB;
        case k32BGRAPixelFormat: return IMGFMT_BGRA;
    }
    mp_msg(MSGT_VO, MSGL_ERR, "[vo_corevideo] Failed to convert pixel format. "
        "Please contact the developers. PixelFormat: %d\n", p->pixelFormat);
    return -1;
}

static mp_image_t *get_screenshot(struct vo *vo)
{
    int img_fmt = get_image_fmt(vo);
    if (img_fmt < 0) return NULL;

    struct priv *p = vo->priv;
    void *base = CVPixelBufferGetBaseAddress(p->pixelBuffer);

    size_t width      = CVPixelBufferGetWidth(p->pixelBuffer);
    size_t height     = CVPixelBufferGetHeight(p->pixelBuffer);
    size_t stride     = CVPixelBufferGetBytesPerRow(p->pixelBuffer);

    struct mp_image img = {0};
    mp_image_setfmt(&img, img_fmt);
    mp_image_set_size(&img, width, height);
    img.planes[0] = base;
    img.stride[0] = stride;

    struct mp_image *image = mp_image_new_copy(&img);
    mp_image_set_display_size(image, vo->aspdat.prew, vo->aspdat.preh);
    mp_image_set_colorspace_details(image, &p->colorspace);

    return image;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
        case VOCTRL_ONTOP:
            p->mpglctx->ontop(vo);
            return VO_TRUE;
        case VOCTRL_PAUSE:
            if (!p->mpglctx->pause)
                break;
            p->mpglctx->pause(vo);
            return VO_TRUE;
        case VOCTRL_RESUME:
            if (!p->mpglctx->resume)
                break;
            p->mpglctx->resume(vo);
            return VO_TRUE;
        case VOCTRL_FULLSCREEN:
            p->mpglctx->fullscreen(vo);
            resize(vo);
            return VO_TRUE;
        case VOCTRL_GET_PANSCAN:
            return VO_TRUE;
        case VOCTRL_SET_PANSCAN:
            resize(vo);
            return VO_TRUE;
        case VOCTRL_UPDATE_SCREENINFO:
            p->mpglctx->update_xinerama_info(vo);
            return VO_TRUE;
        case VOCTRL_REDRAW_FRAME:
            do_render(vo);
            return VO_TRUE;
        case VOCTRL_SET_YUV_COLORSPACE:
            p->colorspace.format = ((struct mp_csp_details *)data)->format;
            set_yuv_colorspace(vo);
            return VO_TRUE;
        case VOCTRL_GET_YUV_COLORSPACE:
            *(struct mp_csp_details *)data = p->colorspace;
            return VO_TRUE;
        case VOCTRL_SCREENSHOT: {
            struct voctrl_screenshot_args *args = data;
            if (args->full_window)
                args->out_image = glGetWindowScreenshot(p->mpglctx->gl);
            else
                args->out_image = get_screenshot(vo);
            return VO_TRUE;
        }
    }
    return VO_NOTIMPL;
}

const struct vo_driver video_out_corevideo = {
    .info = &(const vo_info_t) {
        "Mac OS X Core Video",
        "corevideo",
        "Nicolas Plourde <nicolas.plourde@gmail.com> and others",
        ""
    },
    .preinit = preinit,
    .query_format = query_format,
    .config = config,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
