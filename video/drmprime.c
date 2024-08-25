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

#include <libavutil/hwcontext.h>

#include "hwdec.h"
#include "options/m_config.h"
#include "video/out/drm_common.h"

extern const struct m_sub_options drm_conf;
static struct AVBufferRef *drm_create_standalone(struct mpv_global *global,
        mp_unused struct mp_log *log, mp_unused struct hwcontext_create_dev_params *params)
{
    void *tmp = talloc_new(NULL);
    struct drm_opts *drm_opts = mp_get_config_group(tmp, global, &drm_conf);
    const char *opt_path = drm_opts->device_path;

    const char *device_path = opt_path ? opt_path : "/dev/dri/renderD128";
    AVBufferRef* ref = NULL;
    av_hwdevice_ctx_create(&ref, AV_HWDEVICE_TYPE_DRM, device_path, NULL, 0);

    talloc_free(tmp);
    return ref;
}

const struct hwcontext_fns hwcontext_fns_drmprime = {
    .av_hwdevice_type = AV_HWDEVICE_TYPE_DRM,
    .create_dev = drm_create_standalone,
};
