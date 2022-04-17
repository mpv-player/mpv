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

#pragma once

#include "common/common.h"
#include "common/global.h"

struct offscreen_ctx {
    struct mp_log *log;
    struct ra *ra;
    void *priv;

    void (*set_context)(struct offscreen_ctx *ctx, bool enable);
};

struct offscreen_context {
    const char *api;
    struct offscreen_ctx *(*offscreen_ctx_create)(struct mpv_global *,
                                                  struct mp_log *);
};
