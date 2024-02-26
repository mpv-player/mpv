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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/options.h"
#include "options/m_option.h"
#include "video/out/vo.h"

#include "context.h"
#include "spirv.h"

/* OpenGL */
extern const struct ra_ctx_fns ra_ctx_glx;
extern const struct ra_ctx_fns ra_ctx_x11_egl;
extern const struct ra_ctx_fns ra_ctx_drm_egl;
extern const struct ra_ctx_fns ra_ctx_wayland_egl;
extern const struct ra_ctx_fns ra_ctx_wgl;
extern const struct ra_ctx_fns ra_ctx_angle;
extern const struct ra_ctx_fns ra_ctx_dxgl;
extern const struct ra_ctx_fns ra_ctx_android;

/* Vulkan */
extern const struct ra_ctx_fns ra_ctx_vulkan_wayland;
extern const struct ra_ctx_fns ra_ctx_vulkan_win;
extern const struct ra_ctx_fns ra_ctx_vulkan_xlib;
extern const struct ra_ctx_fns ra_ctx_vulkan_android;
extern const struct ra_ctx_fns ra_ctx_vulkan_display;
extern const struct ra_ctx_fns ra_ctx_vulkan_mac;

/* Direct3D 11 */
extern const struct ra_ctx_fns ra_ctx_d3d11;

/* No API */
extern const struct ra_ctx_fns ra_ctx_wldmabuf;

static const struct ra_ctx_fns *contexts[] = {
#if HAVE_D3D11
    &ra_ctx_d3d11,
#endif

// OpenGL contexts:
#if HAVE_EGL_ANDROID
    &ra_ctx_android,
#endif
#if HAVE_EGL_ANGLE_WIN32
    &ra_ctx_angle,
#endif
#if HAVE_GL_WIN32
    &ra_ctx_wgl,
#endif
#if HAVE_GL_DXINTEROP
    &ra_ctx_dxgl,
#endif
#if HAVE_EGL_WAYLAND
    &ra_ctx_wayland_egl,
#endif
#if HAVE_EGL_X11
    &ra_ctx_x11_egl,
#endif
#if HAVE_GL_X11
    &ra_ctx_glx,
#endif
#if HAVE_EGL_DRM
    &ra_ctx_drm_egl,
#endif

// Vulkan contexts:
#if HAVE_VULKAN

#if HAVE_ANDROID
    &ra_ctx_vulkan_android,
#endif
#if HAVE_WIN32_DESKTOP
    &ra_ctx_vulkan_win,
#endif
#if HAVE_WAYLAND
    &ra_ctx_vulkan_wayland,
#endif
#if HAVE_X11
    &ra_ctx_vulkan_xlib,
#endif
#if HAVE_VK_KHR_DISPLAY
    &ra_ctx_vulkan_display,
#endif
#if HAVE_COCOA && HAVE_SWIFT
    &ra_ctx_vulkan_mac,
#endif
#endif

/* No API contexts: */
#if HAVE_DMABUF_WAYLAND
    &ra_ctx_wldmabuf,
#endif
};

static int ra_ctx_api_help(struct mp_log *log, const struct m_option *opt,
                           struct bstr name)
{
    mp_info(log, "GPU APIs (contexts):\n");
    mp_info(log, "    auto (autodetect)\n");
    for (int n = 0; n < MP_ARRAY_SIZE(contexts); n++) {
        if (!contexts[n]->hidden)
            mp_info(log, "    %s (%s)\n", contexts[n]->type, contexts[n]->name);
    }
    return M_OPT_EXIT;
}

static inline OPT_STRING_VALIDATE_FUNC(ra_ctx_validate_api)
{
    struct bstr param = bstr0(*value);
    if (bstr_equals0(param, "auto"))
        return 1;
    for (int i = 0; i < MP_ARRAY_SIZE(contexts); i++) {
        if (bstr_equals0(param, contexts[i]->type) && !contexts[i]->hidden)
            return 1;
    }
    return M_OPT_INVALID;
}

static int ra_ctx_context_help(struct mp_log *log, const struct m_option *opt,
                               struct bstr name)
{
    mp_info(log, "GPU contexts (APIs):\n");
    mp_info(log, "    auto (autodetect)\n");
    for (int n = 0; n < MP_ARRAY_SIZE(contexts); n++) {
        if (!contexts[n]->hidden)
            mp_info(log, "    %s (%s)\n", contexts[n]->name, contexts[n]->type);
    }
    return M_OPT_EXIT;
}

static inline OPT_STRING_VALIDATE_FUNC(ra_ctx_validate_context)
{
    struct bstr param = bstr0(*value);
    if (bstr_equals0(param, "auto"))
        return 1;
    for (int i = 0; i < MP_ARRAY_SIZE(contexts); i++) {
        if (bstr_equals0(param, contexts[i]->name) && !contexts[i]->hidden)
            return 1;
    }
    return M_OPT_INVALID;
}

// Create a VO window and create a RA context on it.
//  vo_flags: passed to the backend's create window function
struct ra_ctx *ra_ctx_create(struct vo *vo, struct ra_ctx_opts opts)
{
    bool api_auto = !opts.context_type || strcmp(opts.context_type, "auto") == 0;
    bool ctx_auto = !opts.context_name || strcmp(opts.context_name, "auto") == 0;

    if (ctx_auto) {
        MP_VERBOSE(vo, "Probing for best GPU context.\n");
        opts.probing = true;
    }

    // Hack to silence backend (X11/Wayland/etc.) errors. Kill it once backends
    // are separate from `struct vo`
    bool old_probing = vo->probing;
    vo->probing = opts.probing;

    for (int i = 0; i < MP_ARRAY_SIZE(contexts); i++) {
        if (contexts[i]->hidden)
            continue;
        if (!opts.probing && strcmp(contexts[i]->name, opts.context_name) != 0)
            continue;
        if (!api_auto && strcmp(contexts[i]->type, opts.context_type) != 0)
            continue;

        struct ra_ctx *ctx = talloc_ptrtype(NULL, ctx);
        *ctx = (struct ra_ctx) {
            .vo = vo,
            .global = vo->global,
            .log = mp_log_new(ctx, vo->log, contexts[i]->type),
            .opts = opts,
            .fns = contexts[i],
        };

        MP_VERBOSE(ctx, "Initializing GPU context '%s'\n", ctx->fns->name);
        if (contexts[i]->init(ctx)) {
            vo->probing = old_probing;
            vo->context_name = ctx->fns->name;
            return ctx;
        }

        talloc_free(ctx);
    }

    vo->probing = old_probing;

    // If we've reached this point, then none of the contexts matched the name
    // requested, or the backend creation failed for all of them.
    if (!vo->probing)
        MP_ERR(vo, "Failed initializing any suitable GPU context!\n");
    return NULL;
}

struct ra_ctx *ra_ctx_create_by_name(struct vo *vo, const char *name)
{
    for (int i = 0; i < MP_ARRAY_SIZE(contexts); i++) {
        if (strcmp(name, contexts[i]->name) != 0)
            continue;

        struct ra_ctx *ctx = talloc_ptrtype(NULL, ctx);
        *ctx = (struct ra_ctx) {
            .vo = vo,
            .global = vo->global,
            .log = mp_log_new(ctx, vo->log, contexts[i]->type),
            .fns = contexts[i],
        };

        MP_VERBOSE(ctx, "Initializing GPU context '%s'\n", ctx->fns->name);
        if (contexts[i]->init(ctx))
            return ctx;
        talloc_free(ctx);
    }
    return NULL;
}

void ra_ctx_destroy(struct ra_ctx **ctx_ptr)
{
    struct ra_ctx *ctx = *ctx_ptr;
    if (!ctx)
        return;

    if (ctx->spirv && ctx->spirv->fns->uninit)
        ctx->spirv->fns->uninit(ctx);

    ctx->fns->uninit(ctx);
    talloc_free(ctx);

    *ctx_ptr = NULL;
}

#define OPT_BASE_STRUCT struct ra_ctx_opts
const struct m_sub_options ra_ctx_conf = {
    .opts = (const m_option_t[]) {
        {"gpu-context",
            OPT_STRING_VALIDATE(context_name, ra_ctx_validate_context),
            .help = ra_ctx_context_help},
        {"gpu-api",
            OPT_STRING_VALIDATE(context_type, ra_ctx_validate_api),
            .help = ra_ctx_api_help},
        {"gpu-debug", OPT_BOOL(debug)},
        {"gpu-sw", OPT_BOOL(allow_sw)},
        {0}
    },
    .size = sizeof(struct ra_ctx_opts),
};
