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
#include "options/options.h"

#include <libavutil/hwcontext.h>

static struct AVBufferRef *cuda_create_standalone(struct mpv_global *global,
        struct mp_log *log, struct hwcontext_create_dev_params *params)
{
    void *tmp = talloc_new(NULL);
    struct cuda_opts *opts =
        mp_get_config_group(tmp, global, &cuda_conf);
    int decode_dev_idx = opts->cuda_device;
    talloc_free(tmp);

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
