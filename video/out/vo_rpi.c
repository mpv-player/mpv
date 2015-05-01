/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
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

#include "osdep/atomics.h"

#include "common/common.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "vo.h"
#include "video/mp_image.h"
#include "sub/osd.h"
#include "sub/img_convert.h"

// In theory, the number of RGBA subbitmaps the OSD code could give us is
// unlimited; but in practice there will be rarely many elements.
#define MAX_OSD_ELEMS MP_SUB_BB_LIST_MAX

struct osd_elem {
    DISPMANX_RESOURCE_HANDLE_T resource;
    DISPMANX_ELEMENT_HANDLE_T element;
};

struct osd_part {
    struct osd_elem elems[MAX_OSD_ELEMS];
    int num_elems;
    int change_id;
    bool needed;
};

struct priv {
    DISPMANX_DISPLAY_HANDLE_T display;
    DISPMANX_ELEMENT_HANDLE_T window;
    DISPMANX_RESOURCE_HANDLE_T window_back;
    DISPMANX_UPDATE_HANDLE_T update;
    uint32_t w, h;

    struct osd_part osd_parts[MAX_OSD_PARTS];
    double osd_pts;
    struct mp_osd_res osd_res;

    MMAL_COMPONENT_T *renderer;
    bool renderer_enabled;

    struct mp_image *next_image;

    // for RAM input
    MMAL_POOL_T *swpool;

    atomic_bool update_display;

    int background_layer;
    int video_layer;
    int osd_layer;

    int display_nr;
    int layer;
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

static void wipe_osd_part(struct vo *vo, struct osd_part *part)
{
    struct priv *p = vo->priv;

    for (int n = 0; n < part->num_elems; n++) {
        vc_dispmanx_element_remove(p->update, part->elems[n].element);
        vc_dispmanx_resource_delete(part->elems[n].resource);
    }
    part->num_elems = 0;
    part->change_id = -1;
}

static void wipe_osd(struct vo *vo)
{
    struct priv *p = vo->priv;

    for (int x = 0; x < MAX_OSD_PARTS; x++)
        wipe_osd_part(vo, &p->osd_parts[x]);
}

static int add_element(struct vo *vo, struct osd_part *part, int index,
                       struct sub_bitmap *sub)
{
    struct priv *p = vo->priv;
    VC_IMAGE_TYPE_T format = VC_IMAGE_ARGB8888; // assuming RPI is always LE

    struct osd_elem *elem = &part->elems[index];
    *elem = (struct osd_elem){0};

    // I have no idea why stride must be passed in such a hacky way. It's not
    // documented. Other software does it too. Other software claims aligning
    // the width and "probably" the height is required too, but for me it works
    // just fine without on rpi2. (See Weston's rpi renderer.)
    elem->resource = vc_dispmanx_resource_create(format,
                                                 sub->w | (sub->stride << 16),
                                                 sub->h,
                                                 &(int32_t){0});
    if (!elem->resource) {
        MP_ERR(vo, "Could not create %dx%d sub-bitmap\n", sub->w, sub->h);
        return -1;
    }

    VC_RECT_T rc = {.width = sub->w, .height = sub->h};
    vc_dispmanx_resource_write_data(elem->resource, format,
                                    sub->stride, sub->bitmap, &rc);
    VC_RECT_T src = {.width = sub->w << 16, .height = sub->h << 16};
    VC_RECT_T dst = {.x = sub->x, .y = sub->y, .width = sub->dw, .height = sub->dh};
    VC_DISPMANX_ALPHA_T alpha = {
        .flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_PREMULT,
        .opacity = 0xFF,
    };
    elem->element = vc_dispmanx_element_add(p->update, p->display, p->osd_layer,
                                            &dst, elem->resource, &src,
                                            DISPMANX_PROTECTION_NONE,
                                            &alpha, 0, 0);
    if (!elem->element) {
        MP_ERR(vo, "Could not create sub-bitmap element\n");
        return -1;
    }

    return 0;
}

static void osd_draw_cb(void *ctx, struct sub_bitmaps *imgs)
{
    struct vo *vo = ctx;
    struct priv *p = vo->priv;
    struct osd_part *part = &p->osd_parts[imgs->render_index];

    part->needed = true;

    if (imgs->change_id == part->change_id)
        return;

    wipe_osd_part(vo, part);
    part->change_id = imgs->change_id;

    for (int n = 0; n < imgs->num_parts; n++) {
        if (part->num_elems == MAX_OSD_ELEMS) {
            MP_ERR(vo, "Too many OSD elements.\n");
            break;
        }
        int index = part->num_elems++;
        if (add_element(vo, part, index, &imgs->parts[n]) < 0)
            break;
    }
}

static void update_osd(struct vo *vo)
{
    struct priv *p = vo->priv;

    for (int x = 0; x < MAX_OSD_PARTS; x++)
        p->osd_parts[x].needed = false;

    static const bool formats[SUBBITMAP_COUNT] = {[SUBBITMAP_RGBA] = true};
    osd_draw(vo->osd, p->osd_res, p->osd_pts, 0, formats, osd_draw_cb, vo);

    for (int x = 0; x < MAX_OSD_PARTS; x++) {
        struct osd_part *part = &p->osd_parts[x];
        if (!part->needed)
            wipe_osd_part(vo, part);
    }
}

static void resize(struct vo *vo)
{
    struct priv *p = vo->priv;
    MMAL_PORT_T *input = p->renderer->input[0];

    struct mp_rect src, dst;

    vo_get_src_dst_rects(vo, &src, &dst, &p->osd_res);

    MMAL_DISPLAYREGION_T dr = {
        .hdr = { .id = MMAL_PARAMETER_DISPLAYREGION,
                 .size = sizeof(MMAL_DISPLAYREGION_T), },
        .src_rect = { .x = src.x0, .y = src.y0,
                      .width = src.x1 - src.x0, .height = src.y1 - src.y0, },
        .dest_rect = { .x = dst.x0, .y = dst.y0,
                       .width = dst.x1 - dst.x0, .height = dst.y1 - dst.y0, },
        .layer = p->video_layer,
        .display_num = p->display_nr,
        .set = MMAL_DISPLAY_SET_SRC_RECT | MMAL_DISPLAY_SET_DEST_RECT |
               MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_NUM,
    };

    if (mmal_port_parameter_set(input, &dr.hdr))
        MP_WARN(vo, "could not set video rectangle\n");
}

static int update_display_size(struct vo *vo)
{
    struct priv *p = vo->priv;

    uint32_t n_w = 0, n_h = 0;
    if (graphics_get_display_size(0, &n_w, &n_h) < 0) {
        MP_FATAL(vo, "Could not get display size.\n");
        return -1;
    }

    if (p->w == n_w && p->h == n_h)
        return 0;

    p->w = n_w;
    p->h = n_h;

    MP_VERBOSE(vo, "Display size: %dx%d\n", p->w, p->h);

    if (p->window)
        vc_dispmanx_element_remove(p->update, p->window);
    if (p->window_back)
        vc_dispmanx_resource_delete(p->window_back);
    p->window = 0;
    p->window_back = 0;

    VC_IMAGE_TYPE_T format = VC_IMAGE_ARGB8888; // assuming RPI is always LE
    p->window_back = vc_dispmanx_resource_create(format, 1 | (4 << 16), 1,
                                                 &(int32_t){0});
    if (!p->window_back) {
        MP_ERR(vo, "Could not create background bitmap.\n");
        return -1;
    }

    uint32_t px = 0;
    VC_RECT_T rc = {.width = 1, .height = 1};
    vc_dispmanx_resource_write_data(p->window_back, format, 4, &px, &rc);

    // Use the whole screen.
    VC_RECT_T dst = {.width = p->w, .height = p->h};
    VC_RECT_T src = {.width = 1 << 16, .height = 1 << 16};
    VC_DISPMANX_ALPHA_T alpha = {
        .flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,
        .opacity = 0xFF,
    };
    p->window = vc_dispmanx_element_add(p->update, p->display, p->background_layer,
                                        &dst, p->window_back, &src,
                                        DISPMANX_PROTECTION_NONE, &alpha, 0, 0);
    if (!p->window) {
        MP_FATAL(vo, "Could not add DISPMANX element.\n");
        return -1;
    }

    vc_dispmanx_update_submit_sync(p->update);
    p->update = vc_dispmanx_update_start(10);

    return 0;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct mp_image *mpi = p->next_image;
    p->next_image = NULL;

    // For OSD
    vc_dispmanx_update_submit_sync(p->update);
    p->update = vc_dispmanx_update_start(10);

    if (mpi) {
        MMAL_PORT_T *input = p->renderer->input[0];
        MMAL_BUFFER_HEADER_T *ref = (void *)mpi->planes[3];

        // Assume this field is free for use by us.
        ref->user_data = mpi;

        if (mmal_port_send_buffer(input, ref)) {
            MP_ERR(vo, "could not queue picture!\n");
            talloc_free(mpi);
        }
    }
}

static void free_mmal_buffer(void *arg)
{
    MMAL_BUFFER_HEADER_T *buffer = arg;
    mmal_buffer_header_release(buffer);
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;

    talloc_free(p->next_image);
    p->next_image = NULL;

    p->osd_pts = mpi->pts;
    update_osd(vo);

    if (vo->params->imgfmt != IMGFMT_MMAL) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_wait(p->swpool->queue);
        if (!buffer) {
            talloc_free(mpi);
            MP_ERR(vo, "Can't allocate buffer.\n");
            return;
        }
        mmal_buffer_header_reset(buffer);

        struct mp_image *new_ref = mp_image_new_custom_ref(&(struct mp_image){0},
                                                           buffer,
                                                           free_mmal_buffer);
        if (!new_ref) {
            mmal_buffer_header_release(buffer);
            talloc_free(mpi);
            MP_ERR(vo, "Out of memory.\n");
            return;
        }

        mp_image_setfmt(new_ref, IMGFMT_MMAL);
        new_ref->planes[3] = (void *)buffer;

        struct mp_image dmpi = {0};
        buffer->length = layout_buffer(&dmpi, buffer, vo->params);
        mp_image_copy(&dmpi, mpi);

        talloc_free(mpi);
        mpi = new_ref;
    }

    p->next_image = mpi;
}

static int query_format(struct vo *vo, int format)
{
    return format == IMGFMT_MMAL || format == IMGFMT_420P;
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

static void disable_renderer(struct vo *vo)
{
    struct priv *p = vo->priv;

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

static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct priv *p = vo->priv;
    MMAL_PORT_T *input = p->renderer->input[0];
    bool opaque = params->imgfmt == IMGFMT_MMAL;

    vo->dwidth = p->w;
    vo->dheight = p->h;

    disable_renderer(vo);

    AVRational dr = {params->d_w, params->d_h};
    AVRational ir = {params->w, params->h};
    AVRational par = av_div_q(dr, ir);

    input->format->encoding = opaque ? MMAL_ENCODING_OPAQUE : MMAL_ENCODING_I420;
    input->format->es->video.width = MP_ALIGN_UP(params->w, ALIGN_W);
    input->format->es->video.height = MP_ALIGN_UP(params->h, ALIGN_H);
    input->format->es->video.crop = (MMAL_RECT_T){0, 0, params->w, params->h};
    input->format->es->video.par = (MMAL_RATIONAL_T){par.num, par.den};
    input->format->es->video.color_space = map_csp(params->colorspace);

    if (mmal_port_format_commit(input))
        return -1;

    input->buffer_num = MPMAX(input->buffer_num_min,
                              input->buffer_num_recommended) + 3;
    input->buffer_size = MPMAX(input->buffer_size_min,
                               input->buffer_size_recommended);

    if (!opaque) {
        size_t size = layout_buffer(&(struct mp_image){0}, NULL, params);
        if (input->buffer_size != size) {
            MP_FATAL(vo, "We disagree with MMAL about buffer sizes.\n");
            return -1;
        }

        p->swpool = mmal_pool_create(input->buffer_num, input->buffer_size);
        if (!p->swpool) {
            MP_FATAL(vo, "Could not allocate buffer pool.\n");
            return -1;
        }
    }

    resize(vo);

    p->renderer_enabled = true;

    if (mmal_port_enable(p->renderer->control, control_port_cb))
        return -1;

    if (mmal_port_enable(input, input_port_cb))
        return -1;

    if (mmal_component_enable(p->renderer)) {
        MP_FATAL(vo, "Failed to enable video renderer.\n");
        return -1;
    }

    return 0;
}

static struct mp_image *take_screenshot(struct vo *vo)
{
    struct priv *p = vo->priv;

    struct mp_image *img = mp_image_alloc(IMGFMT_BGR0, p->w, p->h);
    if (!img)
        return NULL;

    DISPMANX_RESOURCE_HANDLE_T resource =
        vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
                                    img->w | ((img->w * 4) << 16), img->h,
                                    &(int32_t){0});
    if (!resource)
        goto fail;

    if (vc_dispmanx_snapshot(p->display, resource, 0))
        goto fail;

    VC_RECT_T rc = {.width = img->w, .height = img->h};
    if (vc_dispmanx_resource_read_data(resource, &rc, img->planes[0], img->stride[0]))
        goto fail;

    vc_dispmanx_resource_delete(resource);
    return img;

fail:
    vc_dispmanx_resource_delete(resource);
    talloc_free(img);
    return NULL;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;

    switch (request) {
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        if (p->renderer_enabled)
            resize(vo);
        return VO_TRUE;
    case VOCTRL_REDRAW_FRAME:
        update_osd(vo);
        return VO_TRUE;
    case VOCTRL_SCREENSHOT_WIN:
        *(struct mp_image **)data = take_screenshot(vo);
        return VO_TRUE;
    case VOCTRL_CHECK_EVENTS:
        if (atomic_load(&p->update_display)) {
            atomic_store(&p->update_display, false);
            update_display_size(vo);
            if (p->renderer_enabled)
                resize(vo);
        }
        return VO_TRUE;
    }

    return VO_NOTIMPL;
}

static void tv_callback(void *callback_data, uint32_t reason, uint32_t param1,
                        uint32_t param2)
{
    struct vo *vo = callback_data;
    struct priv *p = vo->priv;
    atomic_store(&p->update_display, true);
    vo_wakeup(vo);
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    vc_tv_unregister_callback_full(tv_callback, vo);

    talloc_free(p->next_image);

    wipe_osd(vo);

    if (p->window)
        vc_dispmanx_element_remove(p->update, p->window);
    if (p->window_back)
        vc_dispmanx_resource_delete(p->window_back);

    if (p->update)
        vc_dispmanx_update_submit_sync(p->update);

    if (p->renderer) {
        disable_renderer(vo);
        mmal_component_release(p->renderer);
    }

    if (p->display)
        vc_dispmanx_display_close(p->display);

    mmal_vc_deinit();
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;

    p->background_layer = p->layer;
    p->video_layer = p->layer + 1;
    p->osd_layer = p->layer + 2;

    bcm_host_init();

    if (mmal_vc_init()) {
        MP_FATAL(vo, "Could not initialize MMAL.\n");
        return -1;
    }

    p->display = vc_dispmanx_display_open(p->display_nr);
    p->update = vc_dispmanx_update_start(0);
    if (!p->display || !p->update) {
        MP_FATAL(vo, "Could not get DISPMANX objects.\n");
        goto fail;
    }

    if (mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &p->renderer))
    {
        MP_FATAL(vo, "Could not create MMAL renderer.\n");
        goto fail;
    }

    if (update_display_size(vo) < 0)
        goto fail;

    vc_tv_register_callback(tv_callback, vo);

    return 0;

fail:
    uninit(vo);
    return -1;
}

#define OPT_BASE_STRUCT struct priv
static const struct m_option options[] = {
    OPT_INT("display", display_nr, 0),
    OPT_INT("layer", layer, 0, OPTDEF_INT(-10)),
    {0},
};

const struct vo_driver video_out_rpi = {
    .description = "Raspberry Pi (MMAL)",
    .name = "rpi",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .options = options,
};
