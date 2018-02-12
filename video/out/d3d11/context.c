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

#include "common/msg.h"
#include "options/m_config.h"
#include "osdep/windows_utils.h"

#include "video/out/gpu/context.h"
#include "video/out/gpu/d3d11_helpers.h"
#include "video/out/gpu/spirv.h"
#include "video/out/w32_common.h"
#include "ra_d3d11.h"

struct d3d11_opts {
    int feature_level;
    int warp;
    int flip;
    int sync_interval;
};

#define OPT_BASE_STRUCT struct d3d11_opts
const struct m_sub_options d3d11_conf = {
    .opts = (const struct m_option[]) {
        OPT_CHOICE("d3d11-warp", warp, 0,
                   ({"auto", -1},
                    {"no", 0},
                    {"yes", 1})),
        OPT_CHOICE("d3d11-feature-level", feature_level, 0,
                   ({"12_1", D3D_FEATURE_LEVEL_12_1},
                    {"12_0", D3D_FEATURE_LEVEL_12_0},
                    {"11_1", D3D_FEATURE_LEVEL_11_1},
                    {"11_0", D3D_FEATURE_LEVEL_11_0},
                    {"10_1", D3D_FEATURE_LEVEL_10_1},
                    {"10_0", D3D_FEATURE_LEVEL_10_0},
                    {"9_3", D3D_FEATURE_LEVEL_9_3},
                    {"9_2", D3D_FEATURE_LEVEL_9_2},
                    {"9_1", D3D_FEATURE_LEVEL_9_1})),
        OPT_FLAG("d3d11-flip", flip, 0),
        OPT_INTRANGE("d3d11-sync-interval", sync_interval, 0, 0, 4),
        {0}
    },
    .defaults = &(const struct d3d11_opts) {
        .feature_level = D3D_FEATURE_LEVEL_12_1,
        .warp = -1,
        .flip = 1,
        .sync_interval = 1,
    },
    .size = sizeof(struct d3d11_opts)
};

struct priv {
    struct d3d11_opts *opts;

    struct ra_tex *backbuffer;
    ID3D11Device *device;
    IDXGISwapChain *swapchain;
};

static struct ra_tex *get_backbuffer(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ID3D11Texture2D *backbuffer = NULL;
    struct ra_tex *tex = NULL;
    HRESULT hr;

    hr = IDXGISwapChain_GetBuffer(p->swapchain, 0, &IID_ID3D11Texture2D,
                                  (void**)&backbuffer);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Couldn't get swapchain image\n");
        goto done;
    }

    tex = ra_d3d11_wrap_tex(ctx->ra, (ID3D11Resource *)backbuffer);
done:
    SAFE_RELEASE(backbuffer);
    return tex;
}

static bool resize(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    HRESULT hr;

    ra_tex_free(ctx->ra, &p->backbuffer);

    hr = IDXGISwapChain_ResizeBuffers(p->swapchain, 0, ctx->vo->dwidth,
        ctx->vo->dheight, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        MP_FATAL(ctx, "Couldn't resize swapchain: %s\n", mp_HRESULT_to_str(hr));
        return false;
    }

    p->backbuffer = get_backbuffer(ctx);

    return true;
}

static bool d3d11_reconfig(struct ra_ctx *ctx)
{
    vo_w32_config(ctx->vo);
    return resize(ctx);
}

static int d3d11_color_depth(struct ra_swapchain *sw)
{
    return 8;
}

static bool d3d11_start_frame(struct ra_swapchain *sw, struct ra_fbo *out_fbo)
{
    struct priv *p = sw->priv;

    if (!p->backbuffer)
        return false;

    *out_fbo = (struct ra_fbo) {
        .tex = p->backbuffer,
        .flip = false,
    };
    return true;
}

static bool d3d11_submit_frame(struct ra_swapchain *sw,
                               const struct vo_frame *frame)
{
    ra_d3d11_flush(sw->ctx->ra);
    return true;
}

static void d3d11_swap_buffers(struct ra_swapchain *sw)
{
    struct priv *p = sw->priv;
    IDXGISwapChain_Present(p->swapchain, p->opts->sync_interval, 0);
}

static int d3d11_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_w32_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE) {
        if (!resize(ctx))
            return VO_ERROR;
    }
    return ret;
}

static void d3d11_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_tex_free(ctx->ra, &p->backbuffer);
    SAFE_RELEASE(p->swapchain);
    vo_w32_uninit(ctx->vo);
    SAFE_RELEASE(p->device);

    // Destory the RA last to prevent objects we hold from showing up in D3D's
    // leak checker
    ctx->ra->fns->destroy(ctx->ra);
}

static const struct ra_swapchain_fns d3d11_swapchain = {
    .color_depth  = d3d11_color_depth,
    .start_frame  = d3d11_start_frame,
    .submit_frame = d3d11_submit_frame,
    .swap_buffers = d3d11_swap_buffers,
};

static bool d3d11_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    p->opts = mp_get_config_group(ctx, ctx->global, &d3d11_conf);

    struct ra_swapchain *sw = ctx->swapchain = talloc_zero(ctx, struct ra_swapchain);
    sw->priv = p;
    sw->ctx = ctx;
    sw->fns = &d3d11_swapchain;

    struct d3d11_device_opts dopts = {
        .debug = ctx->opts.debug,
        .allow_warp = p->opts->warp != 0,
        .force_warp = p->opts->warp == 1,
        .max_feature_level = p->opts->feature_level,
        .max_frame_latency = ctx->opts.swapchain_depth,
    };
    if (!mp_d3d11_create_present_device(ctx->log, &dopts, &p->device))
        goto error;

    if (!spirv_compiler_init(ctx))
        goto error;
    ctx->ra = ra_d3d11_create(p->device, ctx->log, ctx->spirv);
    if (!ctx->ra)
        goto error;

    if (!vo_w32_init(ctx->vo))
        goto error;

    struct d3d11_swapchain_opts scopts = {
        .window = vo_w32_hwnd(ctx->vo),
        .width = ctx->vo->dwidth,
        .height = ctx->vo->dheight,
        .flip = p->opts->flip,
        // Add one frame for the backbuffer and one frame of "slack" to reduce
        // contention with the window manager when acquiring the backbuffer
        .length = ctx->opts.swapchain_depth + 2,
        .usage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    };
    if (!mp_d3d11_create_swapchain(p->device, ctx->log, &scopts, &p->swapchain))
        goto error;

    p->backbuffer = get_backbuffer(ctx);
    if (!p->backbuffer)
        goto error;

    return true;

error:
    d3d11_uninit(ctx);
    return false;
}

const struct ra_ctx_fns ra_ctx_d3d11 = {
    .type     = "d3d11",
    .name     = "d3d11",
    .reconfig = d3d11_reconfig,
    .control  = d3d11_control,
    .init     = d3d11_init,
    .uninit   = d3d11_uninit,
};
