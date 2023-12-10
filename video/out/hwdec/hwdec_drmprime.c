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

#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/pixdesc.h>
#include <xf86drm.h>

#include "config.h"

#include "libmpv/render_gl.h"
#include "options/m_config.h"
#include "video/fmt-conversion.h"
#include "video/out/drm_common.h"
#include "video/out/gpu/hwdec.h"
#include "video/out/hwdec/dmabuf_interop.h"

extern const struct m_sub_options drm_conf;

struct priv_owner {
    struct mp_hwdec_ctx hwctx;
    int *formats;

    struct dmabuf_interop dmabuf_interop;
};

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    if (p->hwctx.driver_name)
        hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);
}

const static dmabuf_interop_init interop_inits[] = {
#if HAVE_DMABUF_INTEROP_GL
    dmabuf_interop_gl_init,
#endif
#if HAVE_VAAPI
    dmabuf_interop_pl_init,
#endif
#if HAVE_DMABUF_WAYLAND
    dmabuf_interop_wl_init,
#endif
    NULL
};

/**
 * Due to the fact that Raspberry Pi support only exists in forked ffmpegs and
 * also requires custom pixel formats, we need some way to work with those formats
 * without introducing any build time dependencies. We do this by looking up the
 * pixel formats by name. As rpi is an important target platform for this hwdec
 * we don't really have the luxury of ignoring these forks.
 */
const static char *forked_pix_fmt_names[] = {
    "rpi4_8",
    "rpi4_10",
};

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    for (int i = 0; interop_inits[i]; i++) {
        if (interop_inits[i](hw, &p->dmabuf_interop)) {
            break;
        }
    }

    if (!p->dmabuf_interop.interop_map || !p->dmabuf_interop.interop_unmap) {
        MP_VERBOSE(hw, "drmprime hwdec requires at least one dmabuf interop backend.\n");
        return -1;
    }

    /*
     * The drm_params resource is not provided when using X11 or Wayland, but
     * there are extensions that supposedly provide this information from the
     * drivers. Not properly documented. Of course.
     */
    mpv_opengl_drm_params_v2 *params = ra_get_native_resource(hw->ra_ctx->ra,
                                                              "drm_params_v2");

    /*
     * Respect drm_device option, so there is a way to control this when not
     * using a DRM gpu context. If drm_params_v2 are present, they will already
     * respect this option.
     */
    void *tmp = talloc_new(NULL);
    struct drm_opts *drm_opts = mp_get_config_group(tmp, hw->global, &drm_conf);
    const char *opt_path = drm_opts->device_path;

    const char *device_path = params && params->render_fd > -1 ?
                              drmGetRenderDeviceNameFromFd(params->render_fd) :
                              opt_path ? opt_path : "/dev/dri/renderD128";
    MP_VERBOSE(hw, "Using DRM device: %s\n", device_path);

    int ret = av_hwdevice_ctx_create(&p->hwctx.av_device_ref,
                                     AV_HWDEVICE_TYPE_DRM,
                                     device_path, NULL, 0);
    talloc_free(tmp);
    if (ret != 0) {
        MP_VERBOSE(hw, "Failed to create hwdevice_ctx: %s\n", av_err2str(ret));
        return -1;
    }

    /*
     * At the moment, there is no way to discover compatible formats
     * from the hwdevice_ctx, and in fact the ffmpeg hwaccels hard-code
     * formats too, so we're not missing out on anything.
     */
    int num_formats = 0;
    MP_TARRAY_APPEND(p, p->formats, num_formats, IMGFMT_NV12);
    MP_TARRAY_APPEND(p, p->formats, num_formats, IMGFMT_420P);
    MP_TARRAY_APPEND(p, p->formats, num_formats, pixfmt2imgfmt(AV_PIX_FMT_NV16));

    for (int i = 0; i < MP_ARRAY_SIZE(forked_pix_fmt_names); i++) {
        enum AVPixelFormat fmt = av_get_pix_fmt(forked_pix_fmt_names[i]);
        if (fmt != AV_PIX_FMT_NONE) {
            MP_TARRAY_APPEND(p, p->formats, num_formats, pixfmt2imgfmt(fmt));
        }
    }

    MP_TARRAY_APPEND(p, p->formats, num_formats, 0); // terminate it

    p->hwctx.hw_imgfmt = IMGFMT_DRMPRIME;
    p->hwctx.supported_formats = p->formats;
    p->hwctx.driver_name = hw->driver->name;
    hwdec_devices_add(hw->devs, &p->hwctx);

    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct dmabuf_interop_priv *p = mapper->priv;

    p_owner->dmabuf_interop.interop_unmap(mapper);

    if (p->surface_acquired) {
        for (int n = 0; n < p->desc.nb_objects; n++) {
            if (p->desc.objects[n].fd > -1)
                close(p->desc.objects[n].fd);
        }
        p->surface_acquired = false;
    }
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    if (p_owner->dmabuf_interop.interop_uninit) {
        p_owner->dmabuf_interop.interop_uninit(mapper);
    }
}

static bool check_fmt(struct ra_hwdec_mapper *mapper, int fmt)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    for (int n = 0; p_owner->formats && p_owner->formats[n]; n++) {
        if (p_owner->formats[n] == fmt)
            return true;
    }
    return false;
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct dmabuf_interop_priv *p = mapper->priv;

    mapper->dst_params = mapper->src_params;

    /*
     * rpi4_8 and rpi4_10 function identically to NV12. These two pixel
     * formats however are not defined in upstream ffmpeg so a string
     * comparison is used to identify them instead of a mpv IMGFMT.
     */
    const char* fmt_name = mp_imgfmt_to_name(mapper->src_params.hw_subfmt);
    if (strcmp(fmt_name, "rpi4_8") == 0 || strcmp(fmt_name, "rpi4_10") == 0)
        mapper->dst_params.imgfmt = IMGFMT_NV12;
    else
        mapper->dst_params.imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params.hw_subfmt = 0;

    struct ra_imgfmt_desc desc = {0};

    if (mapper->ra->num_formats &&
            !ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &desc))
        return -1;

    p->num_planes = desc.num_planes;
    mp_image_set_params(&p->layout, &mapper->dst_params);

    if (p_owner->dmabuf_interop.interop_init)
        if (!p_owner->dmabuf_interop.interop_init(mapper, &desc))
            return -1;

    if (!check_fmt(mapper, mapper->dst_params.imgfmt))
    {
        MP_FATAL(mapper, "unsupported DRM image format %s\n",
                 mp_imgfmt_to_name(mapper->dst_params.imgfmt));
        return -1;
    }

    return 0;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct dmabuf_interop_priv *p = mapper->priv;

    /*
     * Although we use the same AVDRMFrameDescriptor to hold the dmabuf
     * properties, we additionally need to dup the fds to ensure the
     * frame doesn't disappear out from under us. And then for clarity,
     * we copy all the individual fields.
     */
    const AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *)mapper->src->planes[0];
    p->desc.nb_layers = desc->nb_layers;
    p->desc.nb_objects = desc->nb_objects;
    for (int i = 0; i < desc->nb_layers; i++) {
        p->desc.layers[i].format = desc->layers[i].format;
        p->desc.layers[i].nb_planes = desc->layers[i].nb_planes;
        for (int j = 0; j < desc->layers[i].nb_planes; j++) {
            p->desc.layers[i].planes[j].object_index = desc->layers[i].planes[j].object_index;
            p->desc.layers[i].planes[j].offset = desc->layers[i].planes[j].offset;
            p->desc.layers[i].planes[j].pitch = desc->layers[i].planes[j].pitch;
        }
    }
    for (int i = 0; i < desc->nb_objects; i++) {
        p->desc.objects[i].format_modifier = desc->objects[i].format_modifier;
        p->desc.objects[i].size = desc->objects[i].size;
        // Initialise fds to -1 to make partial failure cleanup easier.
        p->desc.objects[i].fd = -1;
    }
    // Surface is now safe to treat as acquired to allow for unmapping to run.
    p->surface_acquired = true;

    // Now actually dup the fds
    for (int i = 0; i < desc->nb_objects; i++) {
        p->desc.objects[i].fd = fcntl(desc->objects[i].fd, F_DUPFD_CLOEXEC, 0);
        if (p->desc.objects[i].fd == -1) {
            MP_ERR(mapper, "Failed to duplicate dmabuf fd: %s\n",
                   mp_strerror(errno));
            goto err;
        }
    }

    // We can handle composed formats if the total number of planes is still
    // equal the number of planes we expect. Complex formats with auxiliary
    // planes cannot be supported.

    int num_returned_planes = 0;
    for (int i = 0; i < p->desc.nb_layers; i++) {
        num_returned_planes += p->desc.layers[i].nb_planes;
    }

    if (p->num_planes != 0 && p->num_planes != num_returned_planes) {
        MP_ERR(mapper,
               "Mapped surface with format '%s' has unexpected number of planes. "
               "(%d layers and %d planes, but expected %d planes)\n",
               mp_imgfmt_to_name(mapper->src->params.hw_subfmt),
               p->desc.nb_layers, num_returned_planes, p->num_planes);
        goto err;
    }

    if (!p_owner->dmabuf_interop.interop_map(mapper, &p_owner->dmabuf_interop,
                                             false))
        goto err;

    return 0;

err:
    mapper_unmap(mapper);

    MP_FATAL(mapper, "mapping DRM dmabuf failed\n");
    return -1;
}

const struct ra_hwdec_driver ra_hwdec_drmprime = {
    .name = "drmprime",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_DRMPRIME, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct dmabuf_interop_priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
