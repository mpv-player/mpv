/*
 * video output driver for Exynos display hardware.
 *
 * by Tobias Jakobi <tjakobi@math.uni-bielefeld.de>
 *
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


#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "drm_common.h"

#include <libdrm/drm_fourcc.h>
#include <libdrm/exynos_drmif.h>
#include <exynos/exynos_drm.h>
#include <exynos/exynos_fimg2d.h>
#include <exynos/exynos_ipp.h>

#include "common/msg.h"
#include "osdep/timer.h"
#include "sub/osd.h"
#include "video/mp_image.h"
#include "vo.h"
#include "drm_atomic.h"

enum {
    exynos_num_pages = 3,
    exynos_size_align = 4,
    exynos_osd_width = 640,
    exynos_osd_height = 480,
};

struct priv;
struct exynos_page;

typedef int (*setup_cb)(struct priv*,
    struct exynos_page*, struct mp_image_params*);

struct exynos_page {
    struct atomic_page page;

    // Buffer objects for scanout (not mapped to userspace).
    // We allocate only one buffer object, even if the
    // format is multi-planar.
    struct exynos_bo *bo[atomic_plane_num];

    struct ipp_task *task;

    struct g2d_image img;

    // Source buffer object for the IPP task.
    struct exynos_bo *ipp_src_bo;

    // Memory addresses for each video plane.
    uint8_t *ipp_src_addr[4];
};

struct priv {
    struct kms_atomic *kms;

    struct exynos_device *device;
    struct exynos_page *pages;
    unsigned num_pages;

    // G2D and IPP context:
    // These are used for scaling and colorspace conversion.
    struct g2d_context *g2d;
    struct ipp_context *ipp;

    // Index of the IPP module to use.
    unsigned ipp_index;

    // Atomic planes used for video and OSD rendering.
    const struct atomic_plane *video_plane;
    const struct atomic_plane *osd_plane;

    // Currently selected IPP module.
    const struct ipp_module *cur_ipp;

    struct vt_switcher vt_switcher;

    setup_cb buffer_setup;
    setup_cb ipp_setup;

    unsigned vt_switcher_active:1;
    unsigned ready:1;
    unsigned active:1;
    unsigned vp_available:1;

    unsigned screen_w;
    unsigned screen_h;
    unsigned front_buf;

    struct mp_image *last_input;
    struct mp_image *cur_frame;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_osd_res osd;
};


static bool is_direct_format(struct priv* p, uint32_t drm_format)
{
    unsigned i;

    assert(p->video_plane);

    for (i = 0; i < p->video_plane->num_formats; ++i) {
        if (drm_format == p->video_plane->formats[i])
            return true;
    }

    return false;
}

static bool is_csc_format(struct priv* p, uint32_t drm_format)
{
    unsigned i;

    assert(p->video_plane);

    for (i = 0; i < p->cur_ipp->num_formats; ++i) {
        const struct ipp_format *fmt;

        fmt = &p->cur_ipp->formats[i];

        // TODO: We want to check IPP limits here.

        if (fmt->type_src && drm_format == fmt->fourcc)
            return true;
    }

    return false;
}

static bool is_vp_plane(const struct atomic_plane *p)
{
    assert(p);

    if (p->num_formats != 2)
        return false;

    return (p->formats[0] == DRM_FORMAT_NV12 &&
            p->formats[1] == DRM_FORMAT_NV21);
}

static bool is_fmt_available(const struct atomic_plane *p, uint32_t fmt)
{
    unsigned i;

    assert(p);

    for (i = 0; i < p->num_formats; ++i) {
        if (p->formats[i] == fmt)
            return true;
    }

    return false;
}

// Translate an enum mp_imgfmt to a DRM fourcc
static uint32_t translate_imgfmt(int format)
{
    switch (format) {
    case IMGFMT_420P:
        return DRM_FORMAT_YUV420;
    case IMGFMT_NV12:
        return DRM_FORMAT_NV12;
    default:
        return 0;
    }
}

static void set_buf_params(struct ipp_buffer *b, struct mp_image_params *p)
{
    unsigned i;

    int total;
    int pitch[MP_MAX_PLANES], offset[MP_MAX_PLANES];

    total = mp_image_params_info(p, exynos_size_align, pitch, offset);

    for (i = 0; i < 4; ++i) {
        fprintf(stderr, "DEBUG: set_buf_params(): buffer %u: pitch = %d, offset = %d\n",
            i, pitch[i], offset[i]);
        b->pitch[i] = (pitch[i] >= 0 ? pitch[i] : 0);
        b->offset[i] = (offset[i] >= 0 ? offset[i] : 0);
    }
}

// For a VP configuration, we let the IPP module only perform CSC. The 
// scaling step is then done by the VP itself. In particular this saves
// precious memory bandwidth, since the VP has to scan out less data.
static int ipp_setup_vp(struct priv *p, struct exynos_page *pg,
                        struct mp_image_params *params)
{
    struct ipp_buffer buffer;
    struct ipp_rect rect;
    uint32_t handle, format;

    unsigned i, size;
    int ret;

    format = translate_imgfmt(params->imgfmt);
    buffer = (struct ipp_buffer) {
        .fourcc = format,
        .width = params->w,
        .height = params->h
    };

    size = mp_image_get_alloc_size(params->imgfmt, params->w, params->h, exynos_size_align); // TODO
    fprintf(stderr, "DEBUG: ipp_setup_vp(): ipp size = %u\n", size);
    pg->ipp_src_bo = exynos_bo_create(p->device, size, 0);

    if (!pg->ipp_src_bo)
        return -1;

    exynos_bo_map(pg->ipp_src_bo);
    handle = exynos_bo_handle(pg->ipp_src_bo);

    for (i = 0; i < 4; ++i)
        buffer.gem_id[i] = handle;

    set_buf_params(&buffer, params);

    for (i = 0; i < 4; ++i)
        pg->ipp_src_addr[i] = (uint8_t*)pg->ipp_src_bo->vaddr + buffer.offset[i];

    rect.x = 0;
    rect.y = 0;
    rect.w = params->w;
    rect.h = params->h;

    pg->task = ipp_task_create(p->ipp_index);

    if (!pg->task) {
        ret = -2;
        goto fail;
    }

    if (ipp_task_config_src(pg->task, &buffer, &rect) < 0) {
        ret = -3;
        goto fail_cfg;
    }

    handle = exynos_bo_handle(pg->bo[atomic_plane_video]);
    buffer = (struct ipp_buffer) {
        .fourcc = DRM_FORMAT_NV12,
        .width = params->w,
        .height = params->h,
        .gem_id = {handle, handle, 0, 0},
        .pitch = {params->w, params->w, 0, 0},
        .offset = {0, params->w * params->h, 0, 0}
    };

    if (ipp_task_config_dst(pg->task, &buffer, &rect) < 0) {
        ret = -4;
        goto fail_cfg;
    }

    return 0;

fail_cfg:
    ipp_task_destroy(pg->task);
    pg->task = NULL;

fail:
    exynos_bo_destroy(pg->ipp_src_bo);
    pg->ipp_src_bo = NULL;

    return ret;
}

// If no VP is available, we let the IPP module do both CSC and scaling.
// This is the expected situation on Exynos5 (and newer) SoC types, where
// memory bandwidth is less an issue compared to the Exynos4.
static int ipp_setup_scale(struct priv *p, struct exynos_page *pg,
                           struct mp_image_params *params)
{
    struct ipp_buffer buffer;
    struct ipp_rect rect;
    uint32_t handle, format;

    unsigned i, size;
    int ret;

    format = translate_imgfmt(params->imgfmt);
    buffer = (struct ipp_buffer) {
        .fourcc = format,
        .width = params->w,
        .height = params->h,
        .modifier = 0
    };

    size = mp_image_get_alloc_size(params->imgfmt, params->w, params->h, 4); // TODO
    fprintf(stderr, "DEBUG: ipp_setup_scale(): ipp size = %u\n", size);
    pg->ipp_src_bo = exynos_bo_create(p->device, size, 0);

    if (!pg->ipp_src_bo)
        return -1;

    exynos_bo_map(pg->ipp_src_bo);
    handle = exynos_bo_handle(pg->ipp_src_bo);

    for (i = 0; i < 4; ++i)
        buffer.gem_id[i] = handle;

    set_buf_params(&buffer, params);

    for (i = 0; i < 4; ++i)
        pg->ipp_src_addr[i] = (uint8_t*)pg->ipp_src_bo->vaddr + buffer.offset[i];

    rect.x = 0;
    rect.y = 0;
    rect.w = params->w;
    rect.h = params->h;

    pg->task = ipp_task_create(p->ipp_index);

    if (!pg->task) {
        ret = -2;
        goto fail;
    }

    if (ipp_task_config_src(pg->task, &buffer, &rect) < 0) {
        ret = -3;
        goto fail_cfg;
    }
 
    handle = exynos_bo_handle(pg->bo[atomic_plane_video]);
    buffer = (struct ipp_buffer) {
        .fourcc = DRM_FORMAT_XRGB8888,
        .width = p->screen_w,
        .height = p->screen_h,
        .gem_id = {handle, 0, 0, 0},
        .pitch = {params->w * sizeof(uint32_t), 0, 0, 0}
    };

    rect.w = p->screen_w;
    rect.h = p->screen_h;

    if (ipp_task_config_dst(pg->task, &buffer, &rect) < 0) {
        ret = -4;
        goto fail_cfg;
    }
 
    return 0;

fail_cfg:
    ipp_task_destroy(pg->task);
    pg->task = NULL;

fail:
    exynos_bo_destroy(pg->ipp_src_bo);
    pg->ipp_src_bo = NULL;

    return ret;
}

static int g2d_setup(struct priv *p, struct exynos_page *pg)
{
    /*
    unsigned int i;
    int ret;

    p->img.color_mode = G2D_COLOR_FMT_XRGB8888 | G2D_ORDER_AXRGB;
    p->img.width = p->device_w;
    p->img.height = p->device_h;
    p->img.stride = p->device_w * sizeof(uint32_t);
    p->img.color = 0xff000000;
    p->img.buf_type = G2D_IMGBUF_GEM;
    p->img.bo[0] = p->fbs[i].bo->handle;

    // Clear the framebuffer
    ret = g2d_solid_fill(p->g2d, &p->g2d_img[i], 0, 0, p->device_w, p->device_h);
    if (ret) {
        MP_ERR(vo, "Failed to clear framebuffer with index %u.\n", i);
        break;
    }
    */

    return 0;
}

static int buffer_setup_vp(struct priv *p, struct exynos_page *pg,
                           struct mp_image_params *params)
{
    uint32_t handle;
    unsigned size;

    assert(p);
    assert(pg);

    // TODO: respect aspect ratio
    size = params->w * params->h * 3 / 2;
    pg->bo[atomic_plane_video] = exynos_bo_create(p->device, size, 0);

    if (!pg->bo[atomic_plane_video])
        goto fail;

    size = exynos_osd_width * exynos_osd_height * sizeof(uint32_t);
    pg->bo[atomic_plane_osd] = exynos_bo_create(p->device, size, EXYNOS_BO_CACHABLE);

    if (!pg->bo[atomic_plane_osd])
        goto fail;

    handle = exynos_bo_handle(pg->bo[atomic_plane_video]);
    pg->page.desc[atomic_plane_video] = (struct buffer_desc) {
        .width = params->w,
        .height = params->h,
        .format = DRM_FORMAT_NV12,
        .handles = {handle, handle, 0, 0},
        .pitches = {params->w, params->w, 0, 0},
        .offsets = {0, params->w * params->h, 0, 0},
        .flags = 0
    };

    handle = exynos_bo_handle(pg->bo[atomic_plane_osd]);
    pg->page.desc[atomic_plane_osd] = (struct buffer_desc) {
        .width = exynos_osd_width,
        .height = exynos_osd_height,
        .format = DRM_FORMAT_ARGB8888,
        .handles = {handle, 0, 0, 0},
        .pitches = {exynos_osd_width * sizeof(uint32_t), 0, 0, 0},
        .offsets = {0, 0, 0, 0},
        .flags = 0
    };

    pg->page.cfg[atomic_plane_video] = (struct plane_config) {
        .plane = p->video_plane,
        .x = 0,
        .y = 0,
        .w = p->screen_w,
        .h = p->screen_h
    };

    pg->page.cfg[atomic_plane_osd] = (struct plane_config) {
        .plane = p->osd_plane,
        .x = (exynos_osd_width >= p->screen_w ? 0 :
              (p->screen_w - exynos_osd_width) / 2),
        .y = (exynos_osd_height >= p->screen_h ? 0 :
              (p->screen_h - exynos_osd_height) / 2),
        .w = exynos_osd_width,
        .h = exynos_osd_height
    };

    return 0;

fail:
    exynos_bo_destroy(pg->bo[atomic_plane_video]);
    exynos_bo_destroy(pg->bo[atomic_plane_osd]);

    pg->bo[atomic_plane_video] = NULL;
    pg->bo[atomic_plane_osd] = NULL;

    return -ENOMEM;
}

static int buffer_setup_scale(struct priv *p, struct exynos_page *pg,
                              struct mp_image_params *params)
{
    uint32_t handle;
    unsigned size, pitch;

    assert(p);
    assert(pg);

    // TODO: respect aspect ratio
    size = p->screen_w * p->screen_h * sizeof(uint32_t);
    pg->bo[atomic_plane_video] = exynos_bo_create(p->device, size, 0);

    if (!pg->bo[atomic_plane_video])
        goto fail;

    size = exynos_osd_width * exynos_osd_height * sizeof(uint32_t);
    pg->bo[atomic_plane_osd] = exynos_bo_create(p->device, size, EXYNOS_BO_CACHABLE);

    if (!pg->bo[atomic_plane_osd])
        goto fail;

    handle = exynos_bo_handle(pg->bo[atomic_plane_video]);
    pitch = p->screen_w * sizeof(uint32_t);
    pg->page.desc[atomic_plane_video] = (struct buffer_desc) {
        .width = p->screen_w,
        .height = p->screen_h,
        .format = DRM_FORMAT_XRGB8888,
        .handles = {handle, 0, 0, 0},
        .pitches = {pitch, 0, 0, 0},
        .offsets = {0, 0, 0, 0},
        .flags = 0
    };

    handle = exynos_bo_handle(pg->bo[atomic_plane_osd]);
    pg->page.desc[atomic_plane_osd] = (struct buffer_desc) {
        .width = exynos_osd_width,
        .height = exynos_osd_height,
        .format = DRM_FORMAT_XRGB8888,
        .handles = {handle, 0, 0, 0},
        .pitches = {exynos_osd_width * sizeof(uint32_t), 0, 0, 0},
        .offsets = {0, 0, 0, 0},
        .flags = 0
    };

    pg->page.cfg[atomic_plane_video] = (struct plane_config) {
        .plane = p->video_plane,
        .x = 0,
        .y = 0,
        .w = p->screen_w,
        .h = p->screen_h
    };

    pg->page.cfg[atomic_plane_osd] = (struct plane_config) {
        .plane = p->osd_plane,
        .x = (p->screen_w - exynos_osd_width) / 2,
        .y = (p->screen_h - exynos_osd_height) / 2,
        .w = exynos_osd_width,
        .h = exynos_osd_height
    };

    return 0;

fail:
    exynos_bo_destroy(pg->bo[atomic_plane_video]);
    exynos_bo_destroy(pg->bo[atomic_plane_osd]);

    pg->bo[atomic_plane_video] = NULL;
    pg->bo[atomic_plane_osd] = NULL;

    return -ENOMEM;
}

static int pages_setup(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p;
    unsigned i;
    int ret;

    p = vo->priv;

    if (p->ready)
        return 0;

    assert(!p->active);

    p->num_pages = exynos_num_pages;
    p->pages = talloc_zero_array(NULL, struct exynos_page, p->num_pages);

    for (i = 0; i < p->num_pages; ++i) {
        ret = p->buffer_setup(p, &p->pages[i], params);
        if (ret < 0) {
            MP_ERR(vo, "Failed to setup buffer for page %u (%d).\n", i, ret);
            ret = -1;
            goto fail;
        }

        ret = p->ipp_setup(p, &p->pages[i], params);
        if (ret < 0) {
            MP_ERR(vo, "Failed to setup IPP for page %u (%d).\n", i, ret);
            ret = -2;
            goto fail;
        }
        
        ret = g2d_setup(p, &p->pages[i]);
        if (ret < 0) {
            MP_ERR(vo, "Failed to setup G2D for page %u (%d).\n", i, ret);
            ret = -3;
            goto fail;
        }

        ret = kms_atomic_register_page(p->kms, &p->pages[i].page);
        if (ret < 0) {
            MP_ERR(vo, "Failed to register page with index %u (%d).\n", i, ret);
            ret = -4;
            goto fail;
        }
    }

    p->ready = 1;

    return 0;

fail:
    talloc_free(p->pages);
    p->pages = NULL;
    p->num_pages = 0;

    return ret;
}

// Counterpart to pages_setup().
static void pages_release(struct vo *vo)
{
    struct priv *p;
    unsigned i, j;

    p = vo->priv;

    if (!p->ready)
        return;

    assert(!p->active);

    kms_atomic_unregister_pages(p->kms);

    for (i = 0; i < p->num_pages; ++i) {
        struct exynos_page *pg = &p->pages[i];

        ipp_task_destroy(pg->task);
        pg->task = NULL;

        for (j = 0; j < 4; ++j)
            pg->ipp_src_addr[j] = NULL;

        exynos_bo_destroy(pg->ipp_src_bo);
        pg->ipp_src_bo = NULL;

        for (j = 0; j < atomic_plane_num; ++j) {
            exynos_bo_destroy(pg->bo[j]);
            pg->bo[j] = NULL;
        }
    }

    talloc_free(p->pages);
    p->pages = NULL;
    p->num_pages = 0;

    p->ready = 0;
}

static int crtc_setup(struct vo *vo)
{
    struct priv *p;
    unsigned page_index;
    int ret;

    p = vo->priv;

    if (p->active)
        return 0;

    assert(p->ready);

    page_index = (p->front_buf + p->num_pages - 1) % p->num_pages;

    ret = kms_atomic_enable(p->kms, &p->pages[page_index].page);
    if (ret < 0)
        return ret;

    p->active = 1;

    return 0;
}

// Counterpart to crtc_setup().
static void crtc_release(struct vo *vo)
{
    struct priv *p = vo->priv;

    if (!p->active)
        return;

    assert(p->ready);

    while (kms_atomic_pageflip_pending(p->kms))
        kms_atomic_wait_for_flip(p->kms);

    kms_atomic_disable(p->kms);

    p->active = 0;
}

static void wait_events(struct vo *vo, int64_t until_time_us)
{
    // Do nothing.
}

static void wakeup(struct vo *vo)
{
    // Do nothing.
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p;
    int ret;

    p = vo->priv;

    crtc_release(vo);
    pages_release(vo);

    kms_atomic_mode_dim(p->kms, &p->screen_w, &p->screen_h);

    if (p->vp_available) {
        p->buffer_setup = buffer_setup_vp;
        p->ipp_setup = ipp_setup_vp;
    } else {
        p->buffer_setup = buffer_setup_scale;
        p->ipp_setup = ipp_setup_scale;
    }

    ret = pages_setup(vo, params);
    if (ret < 0) {
        MP_ERR(vo, "Failed to setup pages (%d).\n", ret);
        return -1;
    }

    vo->dwidth = p->screen_w;
    vo->dheight = p->screen_h;
    vo_get_src_dst_rects(vo, &p->src, &p->dst, &p->osd);

    int w = p->dst.x1 - p->dst.x0;
    int h = p->dst.y1 - p->dst.y0;

    // p->osd contains the parameters assuming OSD rendering in window
    // coordinates, but OSD can only be rendered in the intersection
    // between window and video rectangle (i.e. not into panscan borders).
    p->osd.w = w;
    p->osd.h = h;
    p->osd.mt = MPMIN(0, p->osd.mt);
    p->osd.mb = MPMIN(0, p->osd.mb);
    p->osd.mr = MPMIN(0, p->osd.mr);
    p->osd.ml = MPMIN(0, p->osd.ml);

    /*talloc_free(p->cur_frame);
    p->cur_frame = mp_image_alloc(IMGFMT, p->device_w, p->device_h);
    mp_image_params_guess_csp(&p->sws->dst);
    mp_image_set_params(p->cur_frame, &p->sws->dst);*/

    /*for (i = 0; i < BUF_COUNT; ++i) {
        if (g2d_solid_fill(p->g2d, &p->g2d_img[i], 0, 0, p->device_w, p->device_h))
            return 1;
    }*/

    ret = crtc_setup(vo);
    if (ret < 0) {
        MP_ERR(vo, "Failed to setup CRTC (%d).\n", ret);
        return -2;
    }

    vo->want_redraw = true;

    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;
    struct ipp_buffer *src;
    struct mp_image_params *params;
    
    int ret;

    if (p->active && mpi) {
        //struct mp_image src = *mpi;
        struct mp_rect src_rc = p->src;
        struct exynos_page *pg;
        unsigned i;

        pg = &p->pages[p->front_buf];
        params = &mpi->params;
        
        // TODO: remove this, precompute
        const unsigned size[] = {
            mpi->stride[0] * params->h,
            mpi->stride[1] * params->h / 2,
            mpi->stride[2] * params->h / 2
        };

        for (i = 0; i < 3; ++i){
            //fprintf(stderr, "DEBUG: draw_image(): size[%u] = %u\n", i, size[i]);
            memcpy(pg->ipp_src_addr[i], mpi->planes[i], size[i]);
        }

        ret = ipp_commit(p->ipp, pg->task, NULL);
        if (ret < 0)
            MP_WARN(vo, "Failed to commit IPP task for page %p (%d).\n", pg, ret);

        src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, mpi->fmt.align_x);
        src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, mpi->fmt.align_y);
        /*mp_image_crop_rc(&src, src_rc);
        mp_sws_scale(p->sws, p->cur_frame, &src);
        osd_draw_on_image(vo->osd, p->osd, src.pts, 0, p->cur_frame);

        struct modeset_buf *front_buf = &p->dev->bufs[p->dev->front_buf];
        int w = p->dst.x1 - p->dst.x0;
        int h = p->dst.y1 - p->dst.y0;
        int x = (p->device_w - w) >> 1;
        int y = (p->device_h - h) >> 1;
        int shift = y * front_buf->stride + x * BYTES_PER_PIXEL;
        memcpy_pic(front_buf->map + shift,
                   p->cur_frame->planes[0],
                   w * BYTES_PER_PIXEL,
                   h,
                   front_buf->stride,
                   p->cur_frame->stride[0]);*/
    }

    if (mpi != p->last_input) {
        talloc_free(p->last_input);
        p->last_input = mpi;
    }
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    int ret;

    if (!p->active)
        return;

    assert(p->ready);

    ret = kms_atomic_issue_flip(p->kms, &p->pages[p->front_buf].page);

    if (ret < 0) {
        MP_ERR(vo, "Failed to flip page (%d).\n", ret);
        return;
    }

    p->front_buf++;
    p->front_buf %= p->num_pages;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    crtc_release(vo);
    pages_release(vo);

    ipp_fini(p->ipp);
    g2d_fini(p->g2d);
    exynos_device_destroy(p->device);
    kms_atomic_destroy(p->kms);

    p->ipp = NULL;
    p->g2d = NULL;
    p->device = NULL;
    p->kms = NULL;

    talloc_free(p->last_input);
    talloc_free(p->cur_frame);
}

static int preinit(struct vo *vo)
{
    struct priv *p;
    int fd;
    unsigned i, num_planes;

    p = vo->priv;

    p->kms = kms_atomic_create(vo->log, "exynos",
                               vo->opts->drm_mode_id);
    if (!p->kms) {
        MP_ERR(vo, "Failed to create atomic KMS.\n");
        return -EFAULT;
    }

    fd = kms_atomic_get_fd(p->kms);

    p->device = exynos_device_create(fd);
    if (!p->device) {
        MP_ERR(vo, "Failed to create Exynos device.\n");
        goto fail_exynos;
    }

    p->g2d = g2d_init(fd);
    if (!p->g2d) {
        MP_ERR(vo, "Failed to create G2D context.\n");
        goto fail_g2d;
    }

    p->ipp = ipp_init(fd);
    if (!p->ipp) {
        MP_ERR(vo, "Failed to create IPP context.\n");
        goto fail_ipp;
    }

    if (ipp_num_modules(p->ipp) == 0) {
        MP_ERR(vo, "No IPP modules found.\n");
        goto fail_ipp_modules;
    }

    if (vo->opts->exynos_ipp < 0)
        p->ipp_index = 0;
    else
        p->ipp_index = vo->opts->exynos_ipp;

    if (p->ipp_index >= ipp_num_modules(p->ipp)) {
        MP_ERR(vo, "Invalid IPP module index %u.\n", p->ipp_index);
        goto fail_ipp_index;
    }

    p->cur_ipp = ipp_get_module(p->ipp, p->ipp_index);
    if (!p->cur_ipp) {
        MP_ERR(vo, "Failed to get IPP module.\n");
        goto fail_ipp_get;
    }

    num_planes = kms_atomic_num_planes(p->kms);

    // Check if the display engine of the Exynos SoC has
    // a video processor (VP).
    for (i = 0; i < num_planes; ++i) {
        const struct atomic_plane *plane =
            kms_atomic_get_plane(p->kms, i);

        if (is_vp_plane(plane)) {
            p->vp_available = 1;
            p->video_plane = plane;
            break;
        }
    }

    // In case no VP is available, we just select one of the XR24 planes.
    i = 0;
    if (!p->video_plane || vo->opts->exynos_disable_vp) {
        for (; i < num_planes; ++i) {
            const struct atomic_plane *plane =
                kms_atomic_get_plane(p->kms, i);

            if (is_fmt_available(plane, DRM_FORMAT_XRGB8888)) {
                p->video_plane = plane;
                break;
            }
        }
    }

    // An AR24 plane is used for to display the OSD.
    // We want some alpha-capable format here for blending.
    ++i;
    for (; i < num_planes; ++i) {
        const struct atomic_plane *plane =
            kms_atomic_get_plane(p->kms, i);

        if (is_fmt_available(plane, DRM_FORMAT_ARGB8888)) {
            p->osd_plane = plane;
            break;
        }
    }

    if (!p->video_plane) {
        MP_ERR(vo, "Failed to select video plane.\n");
        goto fail_video;
    }
    if (!p->osd_plane) {
        MP_ERR(vo, "Failed to select OSD plane.\n");
        goto fail_osd;
    }

    if (p->vp_available) {
        MP_INFO(vo, "Video Processor is available.\n");

        if (vo->opts->exynos_disable_vp) {
            MP_INFO(vo, "Not using VP for scaling.\n");
            p->vp_available = 0;
        }
    }

    return 0;

fail_osd:
fail_video:
    p->video_plane = NULL;
    p->osd_plane = NULL;
    
fail_ipp_get:
fail_ipp_index:
fail_ipp_modules:
    ipp_fini(p->ipp);

fail_ipp:
    g2d_fini(p->g2d);

fail_g2d:
    exynos_device_destroy(p->device);

fail_exynos:
    kms_atomic_destroy(p->kms);

    return -1;
}

static int query_format(struct vo *vo, int format)
{
    struct priv *p;
    uint32_t drm_format;

    p = vo->priv;
    drm_format = translate_imgfmt(format);

    if (is_direct_format(p, drm_format))
        return 1;

    if (is_csc_format(p, drm_format))
        return 1;

    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;

    switch (request) {
    case VOCTRL_SCREENSHOT_WIN:
        *(struct mp_image**)data = mp_image_new_copy(p->cur_frame);
        return VO_TRUE;

    case VOCTRL_REDRAW_FRAME:
        draw_image(vo, p->last_input);
        return VO_TRUE;

    case VOCTRL_SET_PANSCAN:
        if (vo->config_ok)
            reconfig(vo, vo->params);
        return VO_TRUE;
    }

    return VO_NOTIMPL;
}

#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_exynos = {
    .name = "exynos",
    .description = "Exynos IPP/G2D",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .wait_events = wait_events,
    .wakeup = wakeup,
    .priv_size = sizeof(struct priv),
};
