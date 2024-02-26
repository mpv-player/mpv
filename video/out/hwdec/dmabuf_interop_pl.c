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

#include <errno.h>
#include <unistd.h>

#include "dmabuf_interop.h"
#include "video/out/placebo/ra_pl.h"
#include "video/out/placebo/utils.h"

static bool vaapi_pl_map(struct ra_hwdec_mapper *mapper,
                         struct dmabuf_interop *dmabuf_interop,
                         bool probing)
{
    struct dmabuf_interop_priv *p = mapper->priv;
    pl_gpu gpu = ra_pl_get(mapper->ra);

    struct ra_imgfmt_desc desc = {0};
    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &desc))
        return false;

    // The calling code validates that the total number of exported planes
    // equals the number we expected in p->num_planes.
    int layer = 0;
    int layer_plane = 0;
    for (int n = 0; n < p->num_planes; n++) {

        const struct ra_format *format = desc.planes[n];
        int id = p->desc.layers[layer].planes[layer_plane].object_index;
        int fd = p->desc.objects[id].fd;
        uint32_t size = p->desc.objects[id].size;
        uint32_t offset = p->desc.layers[layer].planes[layer_plane].offset;
        uint32_t pitch = p->desc.layers[layer].planes[layer_plane].pitch;

        // AMD drivers do not return the size in the surface description, so we
        // need to query it manually.
        if (size == 0) {
            size = lseek(fd, 0, SEEK_END);
            if (size == -1) {
                MP_ERR(mapper, "Cannot obtain size of object with fd %d: %s\n",
                       fd, mp_strerror(errno));
                return false;
            }
            off_t err = lseek(fd, 0, SEEK_SET);
            if (err == -1) {
                MP_ERR(mapper, "Failed to reset offset for fd %d: %s\n",
                       fd, mp_strerror(errno));
                return false;
            }
        }

        struct pl_tex_params tex_params = {
            .w = mp_image_plane_w(&p->layout, n),
            .h = mp_image_plane_h(&p->layout, n),
            .d = 0,
            .format = format->priv,
            .sampleable = true,
            .import_handle = PL_HANDLE_DMA_BUF,
            .shared_mem = (struct pl_shared_mem) {
                .handle = {
                    .fd = fd,
                },
                .size = size,
                .offset = offset,
                .drm_format_mod = p->desc.objects[id].format_modifier,
                .stride_w = pitch,
            },
        };

        mppl_log_set_probing(gpu->log, probing);
        pl_tex pltex = pl_tex_create(gpu, &tex_params);
        mppl_log_set_probing(gpu->log, false);
        if (!pltex)
            return false;

        struct ra_tex *ratex = talloc_ptrtype(NULL, ratex);
        int ret = mppl_wrap_tex(mapper->ra, pltex, ratex);
        if (!ret) {
            pl_tex_destroy(gpu, &pltex);
            talloc_free(ratex);
            return false;
        }
        mapper->tex[n] = ratex;

        MP_TRACE(mapper, "Object %d with fd %d imported as %p\n",
                id, fd, ratex);

        layer_plane++;
        if (layer_plane == p->desc.layers[layer].nb_planes) {
            layer_plane = 0;
            layer++;
        }
    }
    return true;
}

static void vaapi_pl_unmap(struct ra_hwdec_mapper *mapper)
{
    for (int n = 0; n < MP_ARRAY_SIZE(mapper->tex); n++)
        ra_tex_free(mapper->ra, &mapper->tex[n]);
}

bool dmabuf_interop_pl_init(const struct ra_hwdec *hw,
                            struct dmabuf_interop *dmabuf_interop)
{
    pl_gpu gpu = ra_pl_get(hw->ra_ctx->ra);
    if (!gpu) {
        // This is not a libplacebo RA;
        return false;
    }

    if (!(gpu->import_caps.tex & PL_HANDLE_DMA_BUF)) {
        MP_VERBOSE(hw, "libplacebo dmabuf interop requires support for "
                        "PL_HANDLE_DMA_BUF import.\n");
        return false;
    }

    MP_VERBOSE(hw, "using libplacebo dmabuf interop\n");

    dmabuf_interop->interop_map = vaapi_pl_map;
    dmabuf_interop->interop_unmap = vaapi_pl_unmap;

    return true;
}
