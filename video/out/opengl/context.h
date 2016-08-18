/*
 * common OpenGL routines
 *
 * copyleft (C) 2005-2010 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
 * Special thanks go to the xine team and Matthias Hopf, whose video_out_opengl.c
 * gave me lots of good ideas.
 *
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

#ifndef MP_GL_CONTEXT_H_
#define MP_GL_CONTEXT_H_

#include "common.h"

enum {
    VOFLAG_GLES         = 1 << 0,       // Hint to create a GLES2 context
    VOFLAG_NO_GLES      = 1 << 1,       // Hint to create a desktop GL context
    VOFLAG_GL_DEBUG     = 1 << 2,       // Hint to request debug OpenGL context
    VOFLAG_ALPHA        = 1 << 3,       // Hint to request alpha framebuffer
    VOFLAG_SW           = 1 << 4,       // Hint to accept a software GL renderer
    VOFLAG_ANGLE_DCOMP  = 1 << 5,       // Whether DirectComposition is allowed
};

extern const int mpgl_preferred_gl_versions[];

struct MPGLContext;

// A windowing backend (like X11, win32, ...), which provides OpenGL rendering.
struct mpgl_driver {
    const char *name;

    // Size of the struct allocated for MPGLContext.priv
    int priv_size;

    // Init the GL context and possibly the underlying VO backend.
    // The created context should be compatible to GL 3.2 core profile, but
    // some other GL versions are supported as well (e.g. GL 2.1 or GLES 2).
    // Return 0 on success, negative value (-1) on error.
    int (*init)(struct MPGLContext *ctx, int vo_flags);

    // Resize the window, or create a new window if there isn't one yet.
    // Currently, there is an unfortunate interaction with ctx->vo, and
    // display size etc. are determined by it.
    // Return 0 on success, negative value (-1) on error.
    int (*reconfig)(struct MPGLContext *ctx);

    // Present the frame.
    void (*swap_buffers)(struct MPGLContext *ctx);

    // This behaves exactly like vo_driver.control().
    int (*control)(struct MPGLContext *ctx, int *events, int request, void *arg);

    // These behave exactly like vo_driver.wakeup/wait_events. They are
    // optional.
    void (*wakeup)(struct MPGLContext *ctx);
    void (*wait_events)(struct MPGLContext *ctx, int64_t until_time_us);

    // Destroy the GL context and possibly the underlying VO backend.
    void (*uninit)(struct MPGLContext *ctx);
};

typedef struct MPGLContext {
    GL *gl;
    struct vo *vo;
    const struct mpgl_driver *driver;

    // For hwdec_vaegl.c.
    const char *native_display_type;
    void *native_display;

    // Windows-specific hack. See vo_opengl dwmflush suboption.
    int dwm_flush_opt;

    // Flip the rendered image vertically. This is useful for dxinterop.
    bool flip_v;

    // For free use by the mpgl_driver.
    void *priv;
} MPGLContext;

MPGLContext *mpgl_init(struct vo *vo, const char *backend_name, int vo_flags);
void mpgl_uninit(MPGLContext *ctx);
int mpgl_reconfig_window(struct MPGLContext *ctx);
int mpgl_control(struct MPGLContext *ctx, int *events, int request, void *arg);
void mpgl_swap_buffers(struct MPGLContext *ctx);

int mpgl_find_backend(const char *name);

struct m_option;
int mpgl_validate_backend_opt(struct mp_log *log, const struct m_option *opt,
                              struct bstr name, struct bstr param);

#endif
