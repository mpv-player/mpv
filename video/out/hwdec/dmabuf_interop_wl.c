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
#include "dmabuf_interop.h"

static bool mapper_init(mp_unused struct ra_hwdec_mapper *mapper,
                        mp_unused const struct ra_imgfmt_desc *desc)
{
    return true;
}

static void mapper_uninit(mp_unused const struct ra_hwdec_mapper *mapper)
{
}

static bool map(struct ra_hwdec_mapper *mapper,
                struct dmabuf_interop *dmabuf_interop,
                mp_unused bool probing)
{
    // 1. only validate format when composed layers is enabled (i.e. vaapi)
    // 2. for drmprime, just return true for now, as this use case
    // has not been tested.
    if (!dmabuf_interop->composed_layers)
        return true;

    int layer_no = 0;
    struct dmabuf_interop_priv *mapper_p = mapper->priv;
    uint32_t drm_format = mapper_p->desc.layers[layer_no].format;

    if (mapper_p->desc.nb_layers != 1) {
        MP_VERBOSE(mapper, "Mapped surface has separate layers - expected composed layers.\n");
        return false;
    } else if (!ra_compatible_format(mapper->ra, drm_format,
        mapper_p->desc.objects[0].format_modifier)) {
        MP_VERBOSE(mapper, "Mapped surface with format %s; drm format '%s(%016" PRIx64 ")' "
                   "is not supported by compositor.\n",
                   mp_imgfmt_to_name(mapper->src->params.hw_subfmt),
                   mp_tag_str(drm_format),
                   mapper_p->desc.objects[0].format_modifier);
        return false;
    }

    MP_VERBOSE(mapper, "Supported Wayland display format %s: '%s(%016" PRIx64 ")'\n",
               mp_imgfmt_to_name(mapper->src->params.hw_subfmt),
               mp_tag_str(drm_format), mapper_p->desc.objects[0].format_modifier);

    return true;
}

static void unmap(mp_unused struct ra_hwdec_mapper *mapper)
{
}

bool dmabuf_interop_wl_init(const struct ra_hwdec *hw,
                            struct dmabuf_interop *dmabuf_interop)
{
    if (!ra_is_wldmabuf(hw->ra_ctx->ra))
        return false;

    if (strstr(hw->driver->name, "vaapi") != NULL)
        dmabuf_interop->composed_layers = true;

    dmabuf_interop->interop_init = mapper_init;
    dmabuf_interop->interop_uninit = mapper_uninit;
    dmabuf_interop->interop_map = map;
    dmabuf_interop->interop_unmap = unmap;

    return true;
}
