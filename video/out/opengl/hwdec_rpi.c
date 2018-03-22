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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/vc/mmal_vc_api.h>

#include <libavutil/rational.h>

#include "common/common.h"
#include "common/msg.h"
#include "video/mp_image.h"
#include "video/out/gpu/hwdec.h"

#include "common.h"

struct priv {
    struct mp_log *log;

    struct mp_image_params params;

    MMAL_COMPONENT_T *renderer;
    bool renderer_enabled;

    // for RAM input
    MMAL_POOL_T *swpool;

    struct mp_image *current_frame;

    struct mp_rect src, dst;
    int cur_window[4]; // raw user params
};

// Magic alignments (in pixels) expected by the MMAL internals.
#define ALIGN_W 32
#define ALIGN_H 16

// Make mpi point to buffer, assuming MMAL_ENCODING_I420.
// buffer can be NULL.
// Return the required buffer space.
static size_t layout_buffer(struct mp_image *mpi, MMAL_BUFFER_HEADER_T *buffer,
                            struct mp_image_params *params)
{
    assert(params->imgfmt == IMGFMT_420P);
    mp_image_set_params(mpi, params);
    int w = MP_ALIGN_UP(params->w, ALIGN_W);
    int h = MP_ALIGN_UP(params->h, ALIGN_H);
    uint8_t *cur = buffer ? buffer->data : NULL;
    size_t size = 0;
    for (int i = 0; i < 3; i++) {
        int div = i ? 2 : 1;
        mpi->planes[i] = cur;
        mpi->stride[i] = w / div;
        size_t plane_size = h / div * mpi->stride[i];
        if (cur)
            cur += plane_size;
        size += plane_size;
    }
    return size;
}

static MMAL_FOURCC_T map_csp(enum mp_csp csp)
{
    switch (csp) {
    case MP_CSP_BT_601:     return MMAL_COLOR_SPACE_ITUR_BT601;
    case MP_CSP_BT_709:     return MMAL_COLOR_SPACE_ITUR_BT709;
    case MP_CSP_SMPTE_240M: return MMAL_COLOR_SPACE_SMPTE240M;
    default:                return MMAL_COLOR_SPACE_UNKNOWN;
    }
}

static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    mmal_buffer_header_release(buffer);
}

static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    struct mp_image *mpi = buffer->user_data;
    talloc_free(mpi);
}

static void disable_renderer(struct ra_hwdec *hw)
{
    struct priv *p = hw->priv;

    if (p->renderer_enabled) {
        mmal_port_disable(p->renderer->control);
        mmal_port_disable(p->renderer->input[0]);

        mmal_port_flush(p->renderer->control);
        mmal_port_flush(p->renderer->input[0]);

        mmal_component_disable(p->renderer);
    }
    mmal_pool_destroy(p->swpool);
    p->swpool = NULL;
    p->renderer_enabled = false;
}

// check_window_only: assume params and dst/src rc are unchanged
static void update_overlay(struct ra_hwdec *hw, bool check_window_only)
{
    struct priv *p = hw->priv;
    MMAL_PORT_T *input = p->renderer->input[0];
    struct mp_rect src = p->src;
    struct mp_rect dst = p->dst;

    int defs[4] = {0, 0, 0, 0};
    int *z = ra_get_native_resource(hw->ra, "MPV_RPI_WINDOW");
    if (!z)
        z = defs;

    // As documented in the libmpv openglcb headers.
    int display = z[0];
    int layer = z[1];
    int x = z[2];
    int y = z[3];

    if (check_window_only && memcmp(z, p->cur_window, sizeof(p->cur_window)) == 0)
        return;

    memcpy(p->cur_window, z, sizeof(p->cur_window));

    int rotate[] = {MMAL_DISPLAY_ROT0,
                    MMAL_DISPLAY_ROT90,
                    MMAL_DISPLAY_ROT180,
                    MMAL_DISPLAY_ROT270};

    int src_w = src.x1 - src.x0, src_h = src.y1 - src.y0,
        dst_w = dst.x1 - dst.x0, dst_h = dst.y1 - dst.y0;
    int p_x, p_y;
    av_reduce(&p_x, &p_y, dst_w * src_h, src_w * dst_h, 16000);
    MMAL_DISPLAYREGION_T dr = {
        .hdr = { .id = MMAL_PARAMETER_DISPLAYREGION,
                 .size = sizeof(MMAL_DISPLAYREGION_T), },
        .src_rect = { .x = src.x0, .y = src.y0,
                      .width = src_w, .height = src_h },
        .dest_rect = { .x = dst.x0 + x, .y = dst.y0 + y,
                       .width = dst_w, .height = dst_h },
        .layer = layer - 1, // under the GL layer
        .display_num = display,
        .pixel_x = p_x,
        .pixel_y = p_y,
        .transform = rotate[p->params.rotate / 90],
        .fullscreen = 0,
        .set = MMAL_DISPLAY_SET_SRC_RECT | MMAL_DISPLAY_SET_DEST_RECT |
               MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_NUM |
               MMAL_DISPLAY_SET_PIXEL | MMAL_DISPLAY_SET_TRANSFORM |
               MMAL_DISPLAY_SET_FULLSCREEN,
    };

    if (p->params.rotate % 180 == 90) {
        MPSWAP(int, dr.src_rect.x, dr.src_rect.y);
        MPSWAP(int, dr.src_rect.width, dr.src_rect.height);
    }

    if (mmal_port_parameter_set(input, &dr.hdr))
        MP_WARN(p, "could not set video rectangle\n");
}

static int enable_renderer(struct ra_hwdec *hw)
{
    struct priv *p = hw->priv;
    MMAL_PORT_T *input = p->renderer->input[0];
    struct mp_image_params *params = &p->params;

    if (p->renderer_enabled)
        return 0;

    if (!params->imgfmt)
        return -1;

    bool opaque = params->imgfmt == IMGFMT_MMAL;

    input->format->encoding = opaque ? MMAL_ENCODING_OPAQUE : MMAL_ENCODING_I420;
    input->format->es->video.width = MP_ALIGN_UP(params->w, ALIGN_W);
    input->format->es->video.height = MP_ALIGN_UP(params->h, ALIGN_H);
    input->format->es->video.crop = (MMAL_RECT_T){0, 0, params->w, params->h};
    input->format->es->video.par = (MMAL_RATIONAL_T){params->p_w, params->p_h};
    input->format->es->video.color_space = map_csp(params->color.space);

    if (mmal_port_format_commit(input))
        return -1;

    input->buffer_num = MPMAX(input->buffer_num_min,
                              input->buffer_num_recommended) + 3;
    input->buffer_size = MPMAX(input->buffer_size_min,
                               input->buffer_size_recommended);

    if (!opaque) {
        size_t size = layout_buffer(&(struct mp_image){0}, NULL, params);
        if (input->buffer_size != size) {
            MP_FATAL(hw, "We disagree with MMAL about buffer sizes.\n");
            return -1;
        }

        p->swpool = mmal_pool_create(input->buffer_num, input->buffer_size);
        if (!p->swpool) {
            MP_FATAL(hw, "Could not allocate buffer pool.\n");
            return -1;
        }
    }

    update_overlay(hw, false);

    p->renderer_enabled = true;

    if (mmal_port_enable(p->renderer->control, control_port_cb))
        return -1;

    if (mmal_port_enable(input, input_port_cb))
        return -1;

    if (mmal_component_enable(p->renderer)) {
        MP_FATAL(hw, "Failed to enable video renderer.\n");
        return -1;
    }

    return 0;
}

static void free_mmal_buffer(void *arg)
{
    MMAL_BUFFER_HEADER_T *buffer = arg;
    mmal_buffer_header_release(buffer);
}

static struct mp_image *upload(struct ra_hwdec *hw, struct mp_image *hw_image)
{
    struct priv *p = hw->priv;

    MMAL_BUFFER_HEADER_T *buffer = mmal_queue_wait(p->swpool->queue);
    if (!buffer) {
        MP_ERR(hw, "Can't allocate buffer.\n");
        return NULL;
    }
    mmal_buffer_header_reset(buffer);

    struct mp_image *new_ref = mp_image_new_custom_ref(NULL, buffer,
                                                       free_mmal_buffer);
    if (!new_ref) {
        mmal_buffer_header_release(buffer);
        MP_ERR(hw, "Out of memory.\n");
        return NULL;
    }

    mp_image_setfmt(new_ref, IMGFMT_MMAL);
    new_ref->planes[3] = (void *)buffer;

    struct mp_image dmpi = {0};
    buffer->length = layout_buffer(&dmpi, buffer, &p->params);
    mp_image_copy(&dmpi, hw_image);

    return new_ref;
}

static int overlay_frame(struct ra_hwdec *hw, struct mp_image *hw_image,
                         struct mp_rect *src, struct mp_rect *dst, bool newframe)
{
    struct priv *p = hw->priv;

    if (hw_image && !mp_image_params_equal(&p->params, &hw_image->params)) {
        p->params = hw_image->params;

        disable_renderer(hw);
        mp_image_unrefp(&p->current_frame);

        if (enable_renderer(hw) < 0)
            return -1;
    }

    if (hw_image && p->current_frame && !newframe) {
        if (!mp_rect_equals(&p->src, src) ||mp_rect_equals(&p->dst, dst)) {
            p->src = *src;
            p->dst = *dst;
            update_overlay(hw, false);
        }
        return 0; // don't reupload
    }

    mp_image_unrefp(&p->current_frame);

    if (!hw_image) {
        disable_renderer(hw);
        return 0;
    }

    if (enable_renderer(hw) < 0)
        return -1;

    update_overlay(hw, true);

    struct mp_image *mpi = NULL;
    if (hw_image->imgfmt == IMGFMT_MMAL) {
        mpi = mp_image_new_ref(hw_image);
    } else {
        mpi = upload(hw, hw_image);
    }

    if (!mpi) {
        disable_renderer(hw);
        return -1;
    }

    MMAL_BUFFER_HEADER_T *ref = (void *)mpi->planes[3];

    // Assume this field is free for use by us.
    ref->user_data = mpi;

    if (mmal_port_send_buffer(p->renderer->input[0], ref)) {
        MP_ERR(hw, "could not queue picture!\n");
        talloc_free(mpi);
        return -1;
    }

    return 0;
}

static void destroy(struct ra_hwdec *hw)
{
    struct priv *p = hw->priv;

    disable_renderer(hw);

    if (p->renderer)
        mmal_component_release(p->renderer);

    mmal_vc_deinit();
}

static int create(struct ra_hwdec *hw)
{
    struct priv *p = hw->priv;
    p->log = hw->log;

    bcm_host_init();

    if (mmal_vc_init()) {
        MP_FATAL(hw, "Could not initialize MMAL.\n");
        return -1;
    }

    if (mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &p->renderer))
    {
        MP_FATAL(hw, "Could not create MMAL renderer.\n");
        mmal_vc_deinit();
        return -1;
    }

    return 0;
}

const struct ra_hwdec_driver ra_hwdec_rpi_overlay = {
    .name = "rpi-overlay",
    .priv_size = sizeof(struct priv),
    .imgfmts = {IMGFMT_MMAL, IMGFMT_420P, 0},
    .init = create,
    .overlay_frame = overlay_frame,
    .uninit = destroy,
};
