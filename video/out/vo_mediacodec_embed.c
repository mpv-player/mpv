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

#include <libavcodec/mediacodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_mediacodec.h>

#include "common/common.h"
#include "vo.h"
#include "video/mp_image.h"
#include "video/hwdec.h"

struct priv {
    struct mp_image *next_image;
    struct mp_hwdec_ctx hwctx;
};

static AVBufferRef *create_mediacodec_device_ref(struct vo *vo)
{
    AVBufferRef *device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
    if (!device_ref)
        return NULL;

    AVHWDeviceContext *ctx = (void *)device_ref->data;
    AVMediaCodecDeviceContext *hwctx = ctx->hwctx;
    hwctx->surface = (void *)(intptr_t)(vo->opts->WinID);

    if (av_hwdevice_ctx_init(device_ref) < 0)
        av_buffer_unref(&device_ref);

    return device_ref;
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    vo->hwdec_devs = hwdec_devices_create();
    p->hwctx = (struct mp_hwdec_ctx){
        .driver_name = "mediacodec_embed",
        .av_device_ref = create_mediacodec_device_ref(vo),
    };
    hwdec_devices_add(vo->hwdec_devs, &p->hwctx);
    return 0;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (!p->next_image)
        return;

    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)p->next_image->planes[3];
    av_mediacodec_release_buffer(buffer, 1);
    mp_image_unrefp(&p->next_image);
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;

    mp_image_t *mpi = NULL;
    if (!frame->redraw && !frame->repeat)
        mpi = mp_image_new_ref(frame->current);

    talloc_free(p->next_image);
    p->next_image = mpi;
}

static int query_format(struct vo *vo, int format)
{
    return format == IMGFMT_MEDIACODEC;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    mp_image_unrefp(&p->next_image);

    hwdec_devices_remove(vo->hwdec_devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);
}

const struct vo_driver video_out_mediacodec_embed = {
    .description = "Android (Embedded MediaCodec Surface)",
    .name = "mediacodec_embed",
    .caps = VO_CAP_NORETAIN,
    .preinit = preinit,
    .query_format = query_format,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .reconfig = reconfig,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
