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

#include "config.h"

#include "hwdec.h"
#include "options/m_config.h"

#include <libavutil/hwcontext.h>

static struct AVBufferRef *cuda_create_standalone(struct mpv_global *global,
        struct mp_log *log, struct hwcontext_create_dev_params *params)
{
    int decode_dev_idx;
    mp_read_option_raw(global, "cuda-decode-device", &m_option_type_choice,
                       &decode_dev_idx);

    char *decode_dev = NULL;
    if (decode_dev_idx != -1) {
        decode_dev = talloc_asprintf(NULL, "%d", decode_dev_idx);
    }

    AVBufferRef* ref = NULL;
    av_hwdevice_ctx_create(&ref, AV_HWDEVICE_TYPE_CUDA, decode_dev, NULL, 0);

    ta_free(decode_dev);
    return ref;
}

const struct hwcontext_fns hwcontext_fns_cuda = {
    .av_hwdevice_type = AV_HWDEVICE_TYPE_CUDA,
    .create_dev = cuda_create_standalone,
};
