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
 * License along with mpv.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "libplacebo/gpu.h"        // for pl_gpu
#include "libplacebo/log.h"        // for pl_log
#include "libplacebo/swapchain.h"  // for pl_swapchain
#include "stdbool.h"               // for bool

/**
 * The rendering abstraction context.
 */
struct ra_ctx_opts;
struct vo;

/**
 * The main GPU context structure.
 */
struct gpu_ctx {
    struct mp_log *log;
    struct ra_ctx *ra_ctx;

    pl_log pllog;
    pl_gpu gpu;
    pl_swapchain swapchain;

    void *priv;
};

struct gpu_ctx *gpu_ctx_create(struct vo *vo, struct ra_ctx_opts *ctx_opts);
bool gpu_ctx_resize(struct gpu_ctx *ctx, int w, int h);
void gpu_ctx_destroy(struct gpu_ctx **ctxp);
