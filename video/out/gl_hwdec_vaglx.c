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

#include "x11_common.h"
#include "gl_hwdec.h"
#include "video/vaapi.h"

struct priv {
    struct mp_log *log;
    struct mp_vaapi_ctx *ctx;
    VADisplay *display;
    Display *xdisplay;
    GLuint gl_texture;
    GLXFBConfig fbc;
    Pixmap pixmap;
    GLXPixmap glxpixmap;
    void (*glXBindTexImage)(Display *dpy, GLXDrawable draw, int buffer, int *a);
    void (*glXReleaseTexImage)(Display *dpy, GLXDrawable draw, int buffer);
};

static void destroy_texture(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    if (p->glxpixmap) {
        p->glXReleaseTexImage(p->xdisplay, p->glxpixmap, GLX_FRONT_EXT);
        glXDestroyPixmap(p->xdisplay, p->glxpixmap);
    }
    p->glxpixmap = 0;

    if (p->pixmap)
        XFreePixmap(p->xdisplay, p->pixmap);
    p->pixmap = 0;

    gl->DeleteTextures(1, &p->gl_texture);
    p->gl_texture = 0;
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    destroy_texture(hw);
    va_destroy(p->ctx);
}

static int create(struct gl_hwdec *hw)
{
    if (hw->hwctx)
        return -1;
    Display *x11disp = glXGetCurrentDisplay();
    if (!x11disp)
        return -1;
    int x11scr = DefaultScreen(x11disp);
    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;
    p->log = hw->log;
    p->xdisplay = x11disp;
    const char *glxext = glXQueryExtensionsString(x11disp, x11scr);
    if (!glxext || !strstr(glxext, "GLX_EXT_texture_from_pixmap"))
        return -1;
    p->glXBindTexImage =
        (void*)glXGetProcAddressARB((void*)"glXBindTexImageEXT");
    p->glXReleaseTexImage =
        (void*)glXGetProcAddressARB((void*)"glXReleaseTexImageEXT");
    if (!p->glXBindTexImage || !p->glXReleaseTexImage)
        return -1;
    p->display = vaGetDisplay(x11disp);
    if (!p->display)
        return -1;
    p->ctx = va_initialize(p->display, p->log);
    if (!p->ctx) {
        vaTerminate(p->display);
        return -1;
    }
    if (hw->reject_emulated && va_guess_if_emulated(p->ctx)) {
        destroy(hw);
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
        MP_VERBOSE(p, "No texture-from-pixmap support.\n");
        destroy(hw);
        return -1;
    }

    hw->hwctx = &p->ctx->hwctx;
    hw->converted_imgfmt = IMGFMT_RGB0;
    return 0;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    destroy_texture(hw);

    params->imgfmt = hw->driver->imgfmt;

    gl->GenTextures(1, &p->gl_texture);
    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    p->pixmap = XCreatePixmap(p->xdisplay,
                        RootWindow(p->xdisplay, DefaultScreen(p->xdisplay)),
                        params->w, params->h, 24);
    if (!p->pixmap) {
        MP_FATAL(hw, "could not create pixmap\n");
        return -1;
    }

    int attribs[] = {
        GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
        GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
        GLX_MIPMAP_TEXTURE_EXT, False,
        None,
    };
    p->glxpixmap = glXCreatePixmap(p->xdisplay, p->fbc, p->pixmap, attribs);

    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    p->glXBindTexImage(p->xdisplay, p->glxpixmap, GLX_FRONT_EXT, NULL);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    return 0;
}

static int map_image(struct gl_hwdec *hw, struct mp_image *hw_image,
                     GLuint *out_textures)
{
    struct priv *p = hw->priv;
    VAStatus status;

    if (!p->pixmap)
        return -1;

    va_lock(p->ctx);
    status = vaPutSurface(p->display, va_surface_id(hw_image), p->pixmap,
                          0, 0, hw_image->w, hw_image->h,
                          0, 0, hw_image->w, hw_image->h,
                          NULL, 0,
                          va_get_colorspace_flag(hw_image->params.colorspace));
    CHECK_VA_STATUS(p, "vaPutSurface()");
    va_unlock(p->ctx);

    out_textures[0] = p->gl_texture;
    return 0;
}

const struct gl_hwdec_driver gl_hwdec_vaglx = {
    .api_name = "vaapi",
    .imgfmt = IMGFMT_VAAPI,
    .create = create,
    .reinit = reinit,
    .map_image = map_image,
    .destroy = destroy,
};
