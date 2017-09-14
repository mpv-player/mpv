/*
 * This file is part of mpv.
 *
 * Parts based on the MPlayer VA-API patch (see vo_vaapi.c).
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <GL/glx.h>
#include <va/va_x11.h>

#include "video/out/x11_common.h"
#include "video/out/gpu/hwdec.h"
#include "video/vaapi.h"

#include "ra_gl.h"

struct priv_owner {
    struct mp_vaapi_ctx *ctx;
    VADisplay *display;
    Display *xdisplay;
    GLXFBConfig fbc;
};

struct priv {
    GLuint gl_texture;
    Pixmap pixmap;
    GLXPixmap glxpixmap;
    void (*glXBindTexImage)(Display *dpy, GLXDrawable draw, int buffer, int *a);
    void (*glXReleaseTexImage)(Display *dpy, GLXDrawable draw, int buffer);
};

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    if (p->ctx)
        hwdec_devices_remove(hw->devs, &p->ctx->hwctx);
    va_destroy(p->ctx);
}

static int init(struct ra_hwdec *hw)
{
    Display *x11disp = glXGetCurrentDisplay();
    if (!x11disp || !ra_is_gl(hw->ra))
        return -1;
    int x11scr = DefaultScreen(x11disp);
    struct priv_owner *p = hw->priv;
    p->xdisplay = x11disp;
    const char *glxext = glXQueryExtensionsString(x11disp, x11scr);
    if (!glxext || !strstr(glxext, "GLX_EXT_texture_from_pixmap"))
        return -1;
    p->display = vaGetDisplay(x11disp);
    if (!p->display)
        return -1;
    p->ctx = va_initialize(p->display, hw->log, true);
    if (!p->ctx) {
        vaTerminate(p->display);
        return -1;
    }

    int attribs[] = {
        GLX_BIND_TO_TEXTURE_RGBA_EXT, True,
        GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
        GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
        GLX_Y_INVERTED_EXT, True,
        GLX_DOUBLEBUFFER, False,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 0,
        None
    };

    int fbcount;
    GLXFBConfig *fbc = glXChooseFBConfig(x11disp, x11scr, attribs, &fbcount);
    if (fbcount)
        p->fbc = fbc[0];
    if (fbc)
        XFree(fbc);
    if (!fbcount) {
        MP_VERBOSE(hw, "No texture-from-pixmap support.\n");
        return -1;
    }

    p->ctx->hwctx.driver_name = hw->driver->name;
    hwdec_devices_add(hw->devs, &p->ctx->hwctx);
    return 0;
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);
    Display *xdisplay = p_owner->xdisplay;

    p->glXBindTexImage =
        (void*)glXGetProcAddressARB((void*)"glXBindTexImageEXT");
    p->glXReleaseTexImage =
        (void*)glXGetProcAddressARB((void*)"glXReleaseTexImageEXT");
    if (!p->glXBindTexImage || !p->glXReleaseTexImage)
        return -1;

    gl->GenTextures(1, &p->gl_texture);
    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    p->pixmap = XCreatePixmap(xdisplay,
                        RootWindow(xdisplay, DefaultScreen(xdisplay)),
                        mapper->src_params.w, mapper->src_params.h, 24);
    if (!p->pixmap) {
        MP_FATAL(mapper, "could not create pixmap\n");
        return -1;
    }

    int attribs[] = {
        GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
        GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
        GLX_MIPMAP_TEXTURE_EXT, False,
        None,
    };
    p->glxpixmap = glXCreatePixmap(xdisplay, p_owner->fbc, p->pixmap, attribs);

    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    p->glXBindTexImage(xdisplay, p->glxpixmap, GLX_FRONT_EXT, NULL);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    struct ra_tex_params params = {
        .dimensions = 2,
        .w = mapper->src_params.w,
        .h = mapper->src_params.h,
        .d = 1,
        .format = ra_find_unorm_format(mapper->ra, 1, 4), // unsure
        .render_src = true,
        .src_linear = true,
    };
    if (!params.format)
        return -1;

    mapper->tex[0] = ra_create_wrapped_tex(mapper->ra, &params, p->gl_texture);
    if (!mapper->tex[0])
        return -1;

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = IMGFMT_RGB0;
    mapper->dst_params.hw_subfmt = 0;

    return 0;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);
    Display *xdisplay = p_owner->xdisplay;

    if (p->glxpixmap) {
        p->glXReleaseTexImage(xdisplay, p->glxpixmap, GLX_FRONT_EXT);
        glXDestroyPixmap(xdisplay, p->glxpixmap);
    }
    p->glxpixmap = 0;

    if (p->pixmap)
        XFreePixmap(xdisplay, p->pixmap);
    p->pixmap = 0;

    ra_tex_free(mapper->ra, &mapper->tex[0]);
    gl->DeleteTextures(1, &p->gl_texture);
    p->gl_texture = 0;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    VAStatus status;

    struct mp_image *hw_image = mapper->src;

    if (!p->pixmap)
        return -1;

    status = vaPutSurface(p_owner->display, va_surface_id(hw_image), p->pixmap,
                          0, 0, hw_image->w, hw_image->h,
                          0, 0, hw_image->w, hw_image->h,
                          NULL, 0,
                          va_get_colorspace_flag(hw_image->params.color.space));
    CHECK_VA_STATUS(mapper, "vaPutSurface()");

    return 0;
}

const struct ra_hwdec_driver ra_hwdec_vaglx = {
    .name = "vaapi-glx",
    .priv_size = sizeof(struct priv_owner),
    .api = HWDEC_VAAPI,
    .imgfmts = {IMGFMT_VAAPI, 0},
    .testing_only = true,
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
    },
};
