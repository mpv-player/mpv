/*
 * This file is part of mpv video player.
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

#include "video/out/wayland_common.h"
#include "video/out/opengl/context.h"
#include "ra_wldmabuf.h"

static void uninit(struct ra_ctx *ctx)
{
    ra_free(&ctx->ra);
    vo_wayland_uninit(ctx->vo);
}

static bool init(struct ra_ctx *ctx)
{
    if (!vo_wayland_init(ctx->vo))
        return false;
    ctx->ra = ra_create_wayland(ctx->log, ctx->vo);

    return true;
}

const struct ra_ctx_fns ra_ctx_wldmabuf = {
    .type               = "none",
    .name               = "wldmabuf",
    .init               = init,
    .uninit             = uninit,
};
