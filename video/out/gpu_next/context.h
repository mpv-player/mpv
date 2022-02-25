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

#include <libplacebo/renderer.h>

struct mp_log;
struct ra_ctx;
struct vo;
struct gl_video_opts;

struct gpu_ctx {
    struct mp_log *log;
    struct ra_ctx *ra_ctx;

    pl_log pllog;
    pl_gpu gpu;
    pl_swapchain swapchain;

    void *priv;
};

struct gpu_ctx *gpu_ctx_create(struct vo *vo, struct gl_video_opts *gl_opts);
bool gpu_ctx_resize(struct gpu_ctx *ctx, int w, int h);
void gpu_ctx_destroy(struct gpu_ctx **ctxp);
