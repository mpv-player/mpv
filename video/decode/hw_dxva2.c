/*
 * Ported from FFmpeg ffmpeg_dxva2.c (2dbee1a3935a91842c22eb65fd13f77e8d590e07).
 * Original copyright header follows:
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define DXVA2API_USE_BITFIELDS
#include <libavcodec/dxva2.h>
#include <libavutil/common.h>

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
#include <libavutil/hwcontext_dxva2.h>

const struct vd_lavc_hwdec mp_vd_lavc_dxva2 = {
    .type = HWDEC_DXVA2,
    .image_format = IMGFMT_DXVA2,
    .generic_hwaccel = true,
    .set_hwframes = true,
};

const struct vd_lavc_hwdec mp_vd_lavc_dxva2_copy = {
    .type = HWDEC_DXVA2_COPY,
    .copying = true,
    .image_format = IMGFMT_DXVA2,
    .generic_hwaccel = true,
    .create_standalone_dev = true,
    .create_standalone_dev_type = AV_HWDEVICE_TYPE_DXVA2,
    .set_hwframes = true,
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};
