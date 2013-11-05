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
#include <assert.h>

#include <GL/glx.h>
#include <va/va_glx.h>

#include "x11_common.h"
#include "gl_common.h"
#include "video/vaapi.h"
#include "video/decode/dec_video.h"

struct priv {
    struct mp_vaapi_ctx *ctx;
    VADisplay *display;
    GLuint gl_texture;
    void *vaglx_surface;
};

static bool query_format(int imgfmt)
{
    return imgfmt == IMGFMT_VAAPI;
}

static void destroy_texture(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    VAStatus status;

    if (p->vaglx_surface) {
        status = vaDestroySurfaceGLX(p->display, p->vaglx_surface);
        check_va_status(status, "vaDestroySurfaceGLX()");
        p->vaglx_surface = NULL;
    }

    glDeleteTextures(1, &p->gl_texture);
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
    if (hw->info->vaapi_ctx)
        return -1;
    if (!hw->mpgl->vo->x11 || !glXGetCurrentContext())
        return -1;
    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;
    p->display = vaGetDisplayGLX(hw->mpgl->vo->x11->display);
    if (!p->display)
        return -1;
    p->ctx = va_initialize(p->display);
    if (!p->ctx) {
        vaTerminate(p->display);
        return -1;
    }
    hw->info->vaapi_ctx = p->ctx;
    hw->converted_imgfmt = IMGFMT_RGB0;
    return 0;
}

static int reinit(struct gl_hwdec *hw, int w, int h)
{
    struct priv *p = hw->priv;
    GL *gl = hw->mpgl->gl;
    VAStatus status;

    destroy_texture(hw);

    gl->GenTextures(1, &p->gl_texture);
    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    status = vaCreateSurfaceGLX(p->display, GL_TEXTURE_2D,
                                p->gl_texture, &p->vaglx_surface);
    return check_va_status(status, "vaCreateSurfaceGLX()") ? 0 : -1;
}

static int load_image(struct gl_hwdec *hw, struct mp_image *hw_image,
                      GLuint *out_textures)
{
    struct priv *p = hw->priv;
    VAStatus status;

    if (!p->vaglx_surface)
        return -1;

    status = vaCopySurfaceGLX(p->display, p->vaglx_surface,
                              va_surface_id_in_mp_image(hw_image),
                              va_get_colorspace_flag(hw_image->colorspace));
    if (!check_va_status(status, "vaCopySurfaceGLX()"))
        return -1;

    out_textures[0] = p->gl_texture;
    return 0;
}

static void unload_image(struct gl_hwdec *hw)
{
}

const struct gl_hwdec_driver gl_hwdec_vaglx = {
    .api_name = "vaapi",
    .query_format = query_format,
    .create = create,
    .reinit = reinit,
    .load_image = load_image,
    .unload_image = unload_image,
    .destroy = destroy,
};
