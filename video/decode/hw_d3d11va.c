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

#include <libavcodec/d3d11va.h>
#include <libavutil/mem.h>

#include "config.h"

#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "osdep/windows_utils.h"
#include "video/fmt-conversion.h"
#include "video/mp_image_pool.h"
#include "video/hwdec.h"

#include "d3d.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>


const struct vd_lavc_hwdec mp_vd_lavc_d3d11va = {
    .type = HWDEC_D3D11VA,
    .image_format = IMGFMT_D3D11VA,
    .generic_hwaccel = true,
    .set_hwframes = true,
};

const struct vd_lavc_hwdec mp_vd_lavc_d3d11va_copy = {
    .type = HWDEC_D3D11VA_COPY,
    .copying = true,
    .image_format = IMGFMT_D3D11VA,
    .generic_hwaccel = true,
    .create_standalone_dev = true,
    .create_standalone_dev_type = AV_HWDEVICE_TYPE_D3D11VA,
    .set_hwframes = true,
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};
