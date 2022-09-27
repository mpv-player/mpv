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
#include "video/out/wldmabuf/ra_wldmabuf.h"
#include "config.h"
#include "dmabuf_interop.h"

static bool mapper_init(struct ra_hwdec_mapper *mapper,
                        const struct ra_imgfmt_desc *desc)
{
    struct dmabuf_interop_priv *p = mapper->priv;

    p->num_planes = 1;
    return true;
}

static void mapper_uninit(const struct ra_hwdec_mapper *mapper)
{
}

static bool map(struct ra_hwdec_mapper *mapper,
                struct dmabuf_interop *dmabuf_interop,
                bool probing)
{
    return true;
}

static void unmap(struct ra_hwdec_mapper *mapper)
{
}

bool dmabuf_interop_wl_init(const struct ra_hwdec *hw,
                            struct dmabuf_interop *dmabuf_interop)
{
    if (!ra_is_wldmabuf(hw->ra))
        return false;

    dmabuf_interop->interop_init = mapper_init;
    dmabuf_interop->interop_uninit = mapper_uninit;
    dmabuf_interop->interop_map = map;
    dmabuf_interop->interop_unmap = unmap;

    return true;
}
