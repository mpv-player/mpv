/*
 * Copyright (C) 2021 Niklas Haas
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

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <libplacebo/colorspace.h>
#include <libplacebo/options.h>
#include <libplacebo/renderer.h>
#include <libplacebo/shaders/lut.h>
#include <libplacebo/shaders/icc.h>
#include <libplacebo/utils/libav.h>
#include <libplacebo/utils/frame_queue.h>

#include "config.h"
#include "common/common.h"
#include "misc/io_utils.h"
#include "options/m_config.h"
#include "options/options.h"
#include "options/path.h"
#include "osdep/io.h"
#include "osdep/threads.h"
#include "stream/stream.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "video/out/placebo/ra_pl.h"
#include "placebo/utils.h"
#include "gpu/context.h"
#include "gpu/hwdec.h"
#include "gpu/video.h"
#include "gpu/video_shaders.h"
#include "sub/osd.h"
#include "gpu_next/context.h"

#if HAVE_GL && defined(PL_HAVE_OPENGL)
#include <libplacebo/opengl.h>
#include "video/out/opengl/ra_gl.h"
#endif

#if HAVE_D3D11 && defined(PL_HAVE_D3D11)
#include <libplacebo/d3d11.h>
#include "video/out/d3d11/ra_d3d11.h"
#include "osdep/windows_utils.h"
#endif


struct osd_entry {
    pl_tex tex;
    struct pl_overlay_part *parts;
    int num_parts;
};

struct osd_state {
    struct osd_entry entries[MAX_OSD_PARTS];
    struct pl_overlay overlays[MAX_OSD_PARTS];
};

struct scaler_params {
    struct pl_filter_config config;
};

struct user_hook {
    char *path;
    const struct pl_hook *hook;
};

struct user_lut {
    char *opt;
    char *path;
    int type;
    struct pl_custom_lut *lut;
};

struct frame_info {
    int count;
    struct pl_dispatch_info info[VO_PASS_PERF_MAX];
};

struct cache {
    struct mp_log *log;
    struct mpv_global *global;
    char *dir;
    const char *name;
    size_t size_limit;
    pl_cache cache;
};

struct priv {
    struct mp_log *log;
    struct mpv_global *global;
    struct ra_ctx *ra_ctx;
    struct gpu_ctx *context;
    struct ra_hwdec_ctx hwdec_ctx;
    struct ra_hwdec_mapper *hwdec_mapper;

    // Allocated DR buffers
    mp_mutex dr_lock;
    pl_buf *dr_buffers;
    int num_dr_buffers;

    pl_log pllog;
    pl_gpu gpu;
    pl_renderer rr;
    pl_queue queue;
    pl_swapchain sw;
    pl_fmt osd_fmt[SUBBITMAP_COUNT];
    pl_tex *sub_tex;
    int num_sub_tex;

    struct mp_rect src, dst;
    struct mp_osd_res osd_res;
    struct osd_state osd_state;

    uint64_t last_id;
    uint64_t osd_sync;
    double last_pts;
    bool is_interpolated;
    bool want_reset;
    bool frame_pending;
    bool redraw;

    pl_options pars;
    struct m_config_cache *opts_cache;
    struct m_config_cache *next_opts_cache;
    struct gl_next_opts *next_opts;
    struct cache shader_cache, icc_cache;
    struct mp_csp_equalizer_state *video_eq;
    struct scaler_params scalers[SCALER_COUNT];
    const struct pl_hook **hooks; // storage for `params.hooks`
    enum pl_color_levels output_levels;

    struct pl_icc_params icc_params;
    char *icc_path;
    pl_icc_object icc_profile;

    // Cached shaders, preserved across options updates
    struct user_hook *user_hooks;
    int num_user_hooks;

    // Performance data of last frame
    struct frame_info perf_fresh;
    struct frame_info perf_redraw;
};

static void update_render_options(struct vo *vo);
static void update_lut(struct priv *p, struct user_lut *lut);

struct gl_next_opts {
    bool delayed_peak;
    int border_background;
    float corner_rounding;
    bool inter_preserve;
    struct user_lut lut;
    struct user_lut image_lut;
    struct user_lut target_lut;
    bool target_hint;
    char **raw_opts;
};

const struct m_opt_choice_alternatives lut_types[] = {
    {"auto",        PL_LUT_UNKNOWN},
    {"native",      PL_LUT_NATIVE},
    {"normalized",  PL_LUT_NORMALIZED},
    {"conversion",  PL_LUT_CONVERSION},
    {0}
};

#define OPT_BASE_STRUCT struct gl_next_opts
const struct m_sub_options gl_next_conf = {
    .opts = (const struct m_option[]) {
        {"allow-delayed-peak-detect", OPT_BOOL(delayed_peak)},
        {"border-background", OPT_CHOICE(border_background,
            {"none",  BACKGROUND_NONE},
            {"color", BACKGROUND_COLOR},
            {"tiles", BACKGROUND_TILES})},
        {"corner-rounding", OPT_FLOAT(corner_rounding), M_RANGE(0, 1)},
        {"interpolation-preserve", OPT_BOOL(inter_preserve)},
        {"lut", OPT_STRING(lut.opt), .flags = M_OPT_FILE},
        {"lut-type", OPT_CHOICE_C(lut.type, lut_types)},
        {"image-lut", OPT_STRING(image_lut.opt), .flags = M_OPT_FILE},
        {"image-lut-type", OPT_CHOICE_C(image_lut.type, lut_types)},
        {"target-lut", OPT_STRING(target_lut.opt), .flags = M_OPT_FILE},
        {"target-colorspace-hint", OPT_BOOL(target_hint)},
        // No `target-lut-type` because we don't support non-RGB targets
        {"libplacebo-opts", OPT_KEYVALUELIST(raw_opts)},
        {0},
    },
    .defaults = &(struct gl_next_opts) {
        .border_background = BACKGROUND_COLOR,
        .inter_preserve = true,
    },
    .size = sizeof(struct gl_next_opts),
    .change_flags = UPDATE_VIDEO,
};

static pl_buf get_dr_buf(struct priv *p, const uint8_t *ptr)
{
    mp_mutex_lock(&p->dr_lock);

    for (int i = 0; i < p->num_dr_buffers; i++) {
        pl_buf buf = p->dr_buffers[i];
        if (ptr >= buf->data && ptr < buf->data + buf->params.size) {
            mp_mutex_unlock(&p->dr_lock);
            return buf;
        }
    }

    mp_mutex_unlock(&p->dr_lock);
    return NULL;
}

static void free_dr_buf(void *opaque, uint8_t *data)
{
    struct priv *p = opaque;
    mp_mutex_lock(&p->dr_lock);

    for (int i = 0; i < p->num_dr_buffers; i++) {
        if (p->dr_buffers[i]->data == data) {
            pl_buf_destroy(p->gpu, &p->dr_buffers[i]);
            MP_TARRAY_REMOVE_AT(p->dr_buffers, p->num_dr_buffers, i);
            mp_mutex_unlock(&p->dr_lock);
            return;
        }
    }

    MP_ASSERT_UNREACHABLE();
}

static struct mp_image *get_image(struct vo *vo, int imgfmt, int w, int h,
                                  int stride_align, int flags)
{
    struct priv *p = vo->priv;
    pl_gpu gpu = p->gpu;
    if (!gpu->limits.thread_safe || !gpu->limits.max_mapped_size)
        return NULL;

    if ((flags & VO_DR_FLAG_HOST_CACHED) && !gpu->limits.host_cached)
        return NULL;

    stride_align = mp_lcm(stride_align, gpu->limits.align_tex_xfer_pitch);
    stride_align = mp_lcm(stride_align, gpu->limits.align_tex_xfer_offset);
    int size = mp_image_get_alloc_size(imgfmt, w, h, stride_align);
    if (size < 0)
        return NULL;

    pl_buf buf = pl_buf_create(gpu, &(struct pl_buf_params) {
        .memory_type = PL_BUF_MEM_HOST,
        .host_mapped = true,
        .size = size + stride_align,
    });

    if (!buf)
        return NULL;

    struct mp_image *mpi = mp_image_from_buffer(imgfmt, w, h, stride_align,
                                                buf->data, buf->params.size,
                                                p, free_dr_buf);
    if (!mpi) {
        pl_buf_destroy(gpu, &buf);
        return NULL;
    }

    mp_mutex_lock(&p->dr_lock);
    MP_TARRAY_APPEND(p, p->dr_buffers, p->num_dr_buffers, buf);
    mp_mutex_unlock(&p->dr_lock);

    return mpi;
}

static void update_overlays(struct vo *vo, struct mp_osd_res res,
                            int flags, enum pl_overlay_coords coords,
                            struct osd_state *state, struct pl_frame *frame,
                            struct mp_image *src)
{
    struct priv *p = vo->priv;
    static const bool subfmt_all[SUBBITMAP_COUNT] = {
        [SUBBITMAP_LIBASS] = true,
        [SUBBITMAP_BGRA]   = true,
    };

    double pts = src ? src->pts : 0;
    struct sub_bitmap_list *subs = osd_render(vo->osd, res, pts, flags, subfmt_all);

    frame->overlays = state->overlays;
    frame->num_overlays = 0;

    for (int n = 0; n < subs->num_items; n++) {
        const struct sub_bitmaps *item = subs->items[n];
        if (!item->num_parts || !item->packed)
            continue;
        struct osd_entry *entry = &state->entries[item->render_index];
        pl_fmt tex_fmt = p->osd_fmt[item->format];
        if (!entry->tex)
            MP_TARRAY_POP(p->sub_tex, p->num_sub_tex, &entry->tex);
        bool ok = pl_tex_recreate(p->gpu, &entry->tex, &(struct pl_tex_params) {
            .format = tex_fmt,
            .w = MPMAX(item->packed_w, entry->tex ? entry->tex->params.w : 0),
            .h = MPMAX(item->packed_h, entry->tex ? entry->tex->params.h : 0),
            .host_writable = true,
            .sampleable = true,
        });
        if (!ok) {
            MP_ERR(vo, "Failed recreating OSD texture!\n");
            break;
        }
        ok = pl_tex_upload(p->gpu, &(struct pl_tex_transfer_params) {
            .tex        = entry->tex,
            .rc         = { .x1 = item->packed_w, .y1 = item->packed_h, },
            .row_pitch  = item->packed->stride[0],
            .ptr        = item->packed->planes[0],
        });
        if (!ok) {
            MP_ERR(vo, "Failed uploading OSD texture!\n");
            break;
        }

        entry->num_parts = 0;
        for (int i = 0; i < item->num_parts; i++) {
            const struct sub_bitmap *b = &item->parts[i];
            if (b->dw == 0 || b->dh == 0)
                continue;
            uint32_t c = b->libass.color;
            struct pl_overlay_part part = {
                .src = { b->src_x, b->src_y, b->src_x + b->w, b->src_y + b->h },
                .dst = { b->x, b->y, b->x + b->dw, b->y + b->dh },
                .color = {
                    (c >> 24) / 255.0,
                    ((c >> 16) & 0xFF) / 255.0,
                    ((c >> 8) & 0xFF) / 255.0,
                    1.0 - (c & 0xFF) / 255.0,
                }
            };
            MP_TARRAY_APPEND(p, entry->parts, entry->num_parts, part);
        }

        struct pl_overlay *ol = &state->overlays[frame->num_overlays++];
        *ol = (struct pl_overlay) {
            .tex = entry->tex,
            .parts = entry->parts,
            .num_parts = entry->num_parts,
            .color = {
                .primaries = PL_COLOR_PRIM_BT_709,
                .transfer = PL_COLOR_TRC_SRGB,
            },
            .coords = coords,
        };

        switch (item->format) {
        case SUBBITMAP_BGRA:
            ol->mode = PL_OVERLAY_NORMAL;
            ol->repr.alpha = PL_ALPHA_PREMULTIPLIED;
            // Infer bitmap colorspace from source
            if (src) {
                ol->color = src->params.color;
                // Seems like HDR subtitles are targeting SDR white
                if (pl_color_transfer_is_hdr(ol->color.transfer)) {
                    ol->color.hdr = (struct pl_hdr_metadata) {
                        .max_luma = PL_COLOR_SDR_WHITE,
                    };
                }
            }
            break;
        case SUBBITMAP_LIBASS:
            if (src && item->video_color_space)
                ol->color = src->params.color;
            ol->mode = PL_OVERLAY_MONOCHROME;
            ol->repr.alpha = PL_ALPHA_INDEPENDENT;
            break;
        }
    }

    talloc_free(subs);
}

struct frame_priv {
    struct vo *vo;
    struct osd_state subs;
    uint64_t osd_sync;
    struct ra_hwdec *hwdec;
};

static int plane_data_from_imgfmt(struct pl_plane_data out_data[4],
                                  struct pl_bit_encoding *out_bits,
                                  enum mp_imgfmt imgfmt)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(imgfmt);
    if (!desc.num_planes || !(desc.flags & MP_IMGFLAG_HAS_COMPS))
        return 0;

    if (desc.flags & MP_IMGFLAG_HWACCEL)
        return 0; // HW-accelerated frames need to be mapped differently

    if (!(desc.flags & MP_IMGFLAG_NE))
        return 0; // GPU endianness follows the host's

    if (desc.flags & MP_IMGFLAG_PAL)
        return 0; // Palette formats (currently) not supported in libplacebo

    if ((desc.flags & MP_IMGFLAG_TYPE_FLOAT) && (desc.flags & MP_IMGFLAG_YUV))
        return 0; // Floating-point YUV (currently) unsupported

    bool has_bits = false;
    bool any_padded = false;

    for (int p = 0; p < desc.num_planes; p++) {
        struct pl_plane_data *data = &out_data[p];
        struct mp_imgfmt_comp_desc sorted[MP_NUM_COMPONENTS];
        int num_comps = 0;
        if (desc.bpp[p] % 8)
            return 0; // Pixel size is not byte-aligned

        for (int c = 0; c < mp_imgfmt_desc_get_num_comps(&desc); c++) {
            if (desc.comps[c].plane != p)
                continue;

            data->component_map[num_comps] = c;
            sorted[num_comps] = desc.comps[c];
            num_comps++;

            // Sort components by offset order, while keeping track of the
            // semantic mapping in `data->component_map`
            for (int i = num_comps - 1; i > 0; i--) {
                if (sorted[i].offset >= sorted[i - 1].offset)
                    break;
                MPSWAP(struct mp_imgfmt_comp_desc, sorted[i], sorted[i - 1]);
                MPSWAP(int, data->component_map[i], data->component_map[i - 1]);
            }
        }

        uint64_t total_bits = 0;

        // Fill in the pl_plane_data fields for each component
        memset(data->component_size, 0, sizeof(data->component_size));
        for (int c = 0; c < num_comps; c++) {
            data->component_size[c] = sorted[c].size;
            data->component_pad[c] = sorted[c].offset - total_bits;
            total_bits += data->component_pad[c] + data->component_size[c];
            any_padded |= sorted[c].pad;

            // Ignore bit encoding of alpha channel
            if (!out_bits || data->component_map[c] == PL_CHANNEL_A)
                continue;

            struct pl_bit_encoding bits = {
                .sample_depth = data->component_size[c],
                .color_depth = sorted[c].size - abs(sorted[c].pad),
                .bit_shift = MPMAX(sorted[c].pad, 0),
            };

            if (!has_bits) {
                *out_bits = bits;
                has_bits = true;
            } else {
                if (!pl_bit_encoding_equal(out_bits, &bits)) {
                    // Bit encoding differs between components/planes,
                    // cannot handle this
                    *out_bits = (struct pl_bit_encoding) {0};
                    out_bits = NULL;
                }
            }
        }

        data->pixel_stride = desc.bpp[p] / 8;
        data->type = (desc.flags & MP_IMGFLAG_TYPE_FLOAT)
                            ? PL_FMT_FLOAT
                            : PL_FMT_UNORM;
    }

    if (any_padded && !out_bits)
        return 0; // can't handle padded components without `pl_bit_encoding`

    return desc.num_planes;
}

static bool hwdec_reconfig(struct priv *p, struct ra_hwdec *hwdec,
                           const struct mp_image_params *par)
{
    if (p->hwdec_mapper) {
        if (mp_image_params_equal(par, &p->hwdec_mapper->src_params)) {
            return p->hwdec_mapper;
        } else {
            ra_hwdec_mapper_free(&p->hwdec_mapper);
        }
    }

    p->hwdec_mapper = ra_hwdec_mapper_create(hwdec, par);
    if (!p->hwdec_mapper) {
        MP_ERR(p, "Initializing texture for hardware decoding failed.\n");
        return NULL;
    }

    return p->hwdec_mapper;
}

// For RAs not based on ra_pl, this creates a new pl_tex wrapper
static pl_tex hwdec_get_tex(struct priv *p, int n)
{
    struct ra_tex *ratex = p->hwdec_mapper->tex[n];
    struct ra *ra = p->hwdec_mapper->ra;
    if (ra_pl_get(ra))
        return (pl_tex) ratex->priv;

#if HAVE_GL && defined(PL_HAVE_OPENGL)
    if (ra_is_gl(ra) && pl_opengl_get(p->gpu)) {
        struct pl_opengl_wrap_params par = {
            .width = ratex->params.w,
            .height = ratex->params.h,
        };

        ra_gl_get_format(ratex->params.format, &par.iformat,
                         &(GLenum){0}, &(GLenum){0});
        ra_gl_get_raw_tex(ra, ratex, &par.texture, &par.target);
        return pl_opengl_wrap(p->gpu, &par);
    }
#endif

#if HAVE_D3D11 && defined(PL_HAVE_D3D11)
    if (ra_is_d3d11(ra)) {
        int array_slice = 0;
        ID3D11Resource *res = ra_d3d11_get_raw_tex(ra, ratex, &array_slice);
        pl_tex tex = pl_d3d11_wrap(p->gpu, pl_d3d11_wrap_params(
            .tex = res,
            .array_slice = array_slice,
            .fmt = ra_d3d11_get_format(ratex->params.format),
            .w = ratex->params.w,
            .h = ratex->params.h,
        ));
        SAFE_RELEASE(res);
        return tex;
    }
#endif

    MP_ERR(p, "Failed mapping hwdec frame? Open a bug!\n");
    return false;
}

static bool hwdec_acquire(pl_gpu gpu, struct pl_frame *frame)
{
    struct mp_image *mpi = frame->user_data;
    struct frame_priv *fp = mpi->priv;
    struct priv *p = fp->vo->priv;
    if (!hwdec_reconfig(p, fp->hwdec, &mpi->params))
        return false;

    if (ra_hwdec_mapper_map(p->hwdec_mapper, mpi) < 0) {
        MP_ERR(p, "Mapping hardware decoded surface failed.\n");
        return false;
    }

    for (int n = 0; n < frame->num_planes; n++) {
        if (!(frame->planes[n].texture = hwdec_get_tex(p, n)))
            return false;
    }

    return true;
}

static void hwdec_release(pl_gpu gpu, struct pl_frame *frame)
{
    struct mp_image *mpi = frame->user_data;
    struct frame_priv *fp = mpi->priv;
    struct priv *p = fp->vo->priv;
    if (!ra_pl_get(p->hwdec_mapper->ra)) {
        for (int n = 0; n < frame->num_planes; n++)
            pl_tex_destroy(p->gpu, &frame->planes[n].texture);
    }

    ra_hwdec_mapper_unmap(p->hwdec_mapper);
}

static bool map_frame(pl_gpu gpu, pl_tex *tex, const struct pl_source_frame *src,
                      struct pl_frame *frame)
{
    struct mp_image *mpi = src->frame_data;
    const struct mp_image_params *par = &mpi->params;
    struct frame_priv *fp = mpi->priv;
    struct vo *vo = fp->vo;
    struct priv *p = vo->priv;

    fp->hwdec = ra_hwdec_get(&p->hwdec_ctx, mpi->imgfmt);
    if (fp->hwdec) {
        // Note: We don't actually need the mapper to map the frame yet, we
        // only reconfig the mapper here (potentially creating it) to access
        // `dst_params`. In practice, though, this should not matter unless the
        // image format changes mid-stream.
        if (!hwdec_reconfig(p, fp->hwdec, &mpi->params)) {
            talloc_free(mpi);
            return false;
        }

        par = &p->hwdec_mapper->dst_params;
    }

    *frame = (struct pl_frame) {
        .color = par->color,
        .repr = par->repr,
        .profile = {
            .data = mpi->icc_profile ? mpi->icc_profile->data : NULL,
            .len = mpi->icc_profile ? mpi->icc_profile->size : 0,
        },
        .rotation = par->rotate / 90,
        .user_data = mpi,
    };

    // mp_image, like AVFrame, likes communicating RGB/XYZ/YCbCr status
    // implicitly via the image format, rather than the actual tagging.
    switch (mp_imgfmt_get_forced_csp(par->imgfmt)) {
    case PL_COLOR_SYSTEM_RGB:
        frame->repr.sys = PL_COLOR_SYSTEM_RGB;
        frame->repr.levels = PL_COLOR_LEVELS_FULL;
        break;
    case PL_COLOR_SYSTEM_XYZ:
        frame->repr.sys = PL_COLOR_SYSTEM_XYZ;
        break;
    case PL_COLOR_SYSTEM_UNKNOWN:
        if (!frame->repr.sys)
            frame->repr.sys = pl_color_system_guess_ycbcr(par->w, par->h);
        break;
    default: break;
    }

    if (fp->hwdec) {

        struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(par->imgfmt);
        frame->acquire = hwdec_acquire;
        frame->release = hwdec_release;
        frame->num_planes = desc.num_planes;
        for (int n = 0; n < frame->num_planes; n++) {
            struct pl_plane *plane = &frame->planes[n];
            int *map = plane->component_mapping;
            for (int c = 0; c < mp_imgfmt_desc_get_num_comps(&desc); c++) {
                if (desc.comps[c].plane != n)
                    continue;

                // Sort by component offset
                uint8_t offset = desc.comps[c].offset;
                int index = plane->components++;
                while (index > 0 && desc.comps[map[index - 1]].offset > offset) {
                    map[index] = map[index - 1];
                    index--;
                }
                map[index] = c;
            }
        }

    } else { // swdec

        struct pl_plane_data data[4] = {0};
        frame->num_planes = plane_data_from_imgfmt(data, &frame->repr.bits, mpi->imgfmt);
        for (int n = 0; n < frame->num_planes; n++) {
            struct pl_plane *plane = &frame->planes[n];
            data[n].width = mp_image_plane_w(mpi, n);
            data[n].height = mp_image_plane_h(mpi, n);
            if (mpi->stride[n] < 0) {
                data[n].pixels = mpi->planes[n] + (data[n].height - 1) * mpi->stride[n];
                data[n].row_stride = -mpi->stride[n];
                plane->flipped = true;
            } else {
                data[n].pixels = mpi->planes[n];
                data[n].row_stride = mpi->stride[n];
            }

            pl_buf buf = get_dr_buf(p, data[n].pixels);
            if (buf) {
                data[n].buf = buf;
                data[n].buf_offset = (uint8_t *) data[n].pixels - buf->data;
                data[n].pixels = NULL;
            } else if (gpu->limits.callbacks) {
                data[n].callback = talloc_free;
                data[n].priv = mp_image_new_ref(mpi);
            }

            if (!pl_upload_plane(gpu, plane, &tex[n], &data[n])) {
                MP_ERR(vo, "Failed uploading frame!\n");
                talloc_free(data[n].priv);
                talloc_free(mpi);
                return false;
            }
        }

    }

    // Update chroma location, must be done after initializing planes
    pl_frame_set_chroma_location(frame, par->chroma_location);

    // Set the frame DOVI metadata
    mp_map_dovi_metadata_to_pl(mpi, frame);

    if (mpi->film_grain)
        pl_film_grain_from_av(&frame->film_grain, (AVFilmGrainParams *) mpi->film_grain->data);

    // Compute a unique signature for any attached ICC profile. Wasteful in
    // theory if the ICC profile is the same for multiple frames, but in
    // practice ICC profiles are overwhelmingly going to be attached to
    // still images so it shouldn't matter.
    pl_icc_profile_compute_signature(&frame->profile);

    // Update LUT attached to this frame
    update_lut(p, &p->next_opts->image_lut);
    frame->lut = p->next_opts->image_lut.lut;
    frame->lut_type = p->next_opts->image_lut.type;
    return true;
}

static void unmap_frame(pl_gpu gpu, struct pl_frame *frame,
                        const struct pl_source_frame *src)
{
    struct mp_image *mpi = src->frame_data;
    struct frame_priv *fp = mpi->priv;
    struct priv *p = fp->vo->priv;
    for (int i = 0; i < MP_ARRAY_SIZE(fp->subs.entries); i++) {
        pl_tex tex = fp->subs.entries[i].tex;
        if (tex)
            MP_TARRAY_APPEND(p, p->sub_tex, p->num_sub_tex, tex);
    }
    talloc_free(mpi);
}

static void discard_frame(const struct pl_source_frame *src)
{
    struct mp_image *mpi = src->frame_data;
    talloc_free(mpi);
}

static void info_callback(void *priv, const struct pl_render_info *info)
{
    struct vo *vo = priv;
    struct priv *p = vo->priv;
    if (info->index >= VO_PASS_PERF_MAX)
        return; // silently ignore clipped passes, whatever

    struct frame_info *frame;
    switch (info->stage) {
    case PL_RENDER_STAGE_FRAME: frame = &p->perf_fresh; break;
    case PL_RENDER_STAGE_BLEND: frame = &p->perf_redraw; break;
    default: abort();
    }

    frame->count = info->index + 1;
    pl_dispatch_info_move(&frame->info[info->index], info->pass);
}

static void update_options(struct vo *vo)
{
    struct priv *p = vo->priv;
    pl_options pars = p->pars;
    bool changed = m_config_cache_update(p->opts_cache);
    changed = m_config_cache_update(p->next_opts_cache) || changed;
    if (changed)
        update_render_options(vo);

    update_lut(p, &p->next_opts->lut);
    pars->params.lut = p->next_opts->lut.lut;
    pars->params.lut_type = p->next_opts->lut.type;

    // Update equalizer state
    struct mp_csp_params cparams = MP_CSP_PARAMS_DEFAULTS;
    mp_csp_equalizer_state_get(p->video_eq, &cparams);
    pars->color_adjustment.brightness = cparams.brightness;
    pars->color_adjustment.contrast = cparams.contrast;
    pars->color_adjustment.hue = cparams.hue;
    pars->color_adjustment.saturation = cparams.saturation;
    pars->color_adjustment.gamma = cparams.gamma;
    p->output_levels = cparams.levels_out;

    for (char **kv = p->next_opts->raw_opts; kv && kv[0]; kv += 2)
        pl_options_set_str(pars, kv[0], kv[1]);
}

static void apply_target_contrast(struct priv *p, struct pl_color_space *color)
{
    const struct gl_video_opts *opts = p->opts_cache->opts;

    // Auto mode, leave as is
    if (!opts->target_contrast)
        return;

    // Infinite contrast
    if (opts->target_contrast == -1) {
        color->hdr.min_luma = 1e-7;
        return;
    }

    // Infer max_luma for current pl_color_space
    pl_color_space_nominal_luma_ex(pl_nominal_luma_params(
        .color = color,
        // with HDR10 meta to respect value if already set
        .metadata = PL_HDR_METADATA_HDR10,
        .scaling = PL_HDR_NITS,
        .out_max = &color->hdr.max_luma
    ));

    color->hdr.min_luma = color->hdr.max_luma / opts->target_contrast;
}

static void apply_target_options(struct priv *p, struct pl_frame *target)
{
    update_lut(p, &p->next_opts->target_lut);
    target->lut = p->next_opts->target_lut.lut;
    target->lut_type = p->next_opts->target_lut.type;

    // Colorspace overrides
    const struct gl_video_opts *opts = p->opts_cache->opts;
    if (p->output_levels)
        target->repr.levels = p->output_levels;
    if (opts->target_prim)
        target->color.primaries = opts->target_prim;
    if (opts->target_trc)
        target->color.transfer = opts->target_trc;
    // If swapchain returned a value use this, override is used in hint
    if (opts->target_peak && !target->color.hdr.max_luma)
        target->color.hdr.max_luma = opts->target_peak;
    if (!target->color.hdr.min_luma)
        apply_target_contrast(p, &target->color);
    if (opts->target_gamut) {
        // Ensure resulting gamut still fits inside container
        const struct pl_raw_primaries *gamut, *container;
        gamut = pl_raw_primaries_get(opts->target_gamut);
        container = pl_raw_primaries_get(target->color.primaries);
        target->color.hdr.prim = pl_primaries_clip(gamut, container);
    }
    if (opts->dither_depth > 0) {
        struct pl_bit_encoding *tbits = &target->repr.bits;
        tbits->color_depth += opts->dither_depth - tbits->sample_depth;
        tbits->sample_depth = opts->dither_depth;
    }

    if (opts->icc_opts->icc_use_luma) {
        p->icc_params.max_luma = 0.0f;
    } else {
        pl_color_space_nominal_luma_ex(pl_nominal_luma_params(
            .color    = &target->color,
            .metadata = PL_HDR_METADATA_HDR10, // use only static HDR nits
            .scaling  = PL_HDR_NITS,
            .out_max  = &p->icc_params.max_luma,
        ));
    }

    pl_icc_update(p->pllog, &p->icc_profile, NULL, &p->icc_params);
    target->icc = p->icc_profile;
}

static void apply_crop(struct pl_frame *frame, struct mp_rect crop,
                       int width, int height)
{
    frame->crop = (struct pl_rect2df) {
        .x0 = crop.x0,
        .y0 = crop.y0,
        .x1 = crop.x1,
        .y1 = crop.y1,
    };

    // mpv gives us rotated/flipped rects, libplacebo expects unrotated
    pl_rect2df_rotate(&frame->crop, -frame->rotation);
    if (frame->crop.x1 < frame->crop.x0) {
        frame->crop.x0 = width - frame->crop.x0;
        frame->crop.x1 = width - frame->crop.x1;
    }

    if (frame->crop.y1 < frame->crop.y0) {
        frame->crop.y0 = height - frame->crop.y0;
        frame->crop.y1 = height - frame->crop.y1;
    }
}

static void update_tm_viz(struct pl_color_map_params *params,
                          const struct pl_frame *target)
{
    if (!params->visualize_lut)
        return;

    // Use right half of sceen for TM visualization, constrain to 1:1 AR
    const float out_w = fabsf(pl_rect_w(target->crop));
    const float out_h = fabsf(pl_rect_h(target->crop));
    const float size = MPMIN(out_w / 2.0f, out_h);
    params->visualize_rect = (pl_rect2df) {
        .x0 = 1.0f - size / out_w,
        .x1 = 1.0f,
        .y0 = 0.0f,
        .y1 = size / out_h,
    };

    // Visualize red-blue plane
    params->visualize_hue = M_PI / 4.0;
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;
    pl_options pars = p->pars;
    pl_gpu gpu = p->gpu;
    update_options(vo);

    struct pl_render_params params = pars->params;
    const struct gl_video_opts *opts = p->opts_cache->opts;
    bool will_redraw = frame->display_synced && frame->num_vsyncs > 1;
    bool cache_frame = will_redraw || frame->still;
    bool can_interpolate = opts->interpolation && frame->display_synced &&
                           !frame->still && frame->num_frames > 1;
    double pts_offset = can_interpolate ? frame->ideal_frame_vsync : 0;
    params.info_callback = info_callback;
    params.info_priv = vo;
    params.skip_caching_single_frame = !cache_frame;
    params.preserve_mixing_cache = p->next_opts->inter_preserve && !frame->still;
    if (frame->still)
        params.frame_mixer = NULL;

    // pl_queue advances its internal virtual PTS and culls available frames
    // based on this value and the VPS/FPS ratio. Requesting a non-monotonic PTS
    // is an invalid use of pl_queue. Reset it if this happens in an attempt to
    // recover as much as possible. Ideally, this should never occur, and if it
    // does, it should be corrected. The ideal_frame_vsync may be negative if
    // the last draw did not align perfectly with the vsync. In this case, we
    // should have the previous frame available in pl_queue, or a reset is
    // already requested. Clamp the check to 0, as we don't have the previous
    // frame in vo_frame anyway.
    struct pl_source_frame vpts;
    if (frame->current && !p->want_reset) {
        if (pl_queue_peek(p->queue, 0, &vpts) &&
            frame->current->pts + MPMAX(0, pts_offset) < vpts.pts)
        {
            MP_VERBOSE(vo, "Forcing queue refill, PTS(%f + %f | %f) < VPTS(%f)\n",
                       frame->current->pts, pts_offset,
                       frame->ideal_frame_vsync_duration, vpts.pts);
            p->want_reset = true;
        }
    }

    // Push all incoming frames into the frame queue
    for (int n = 0; n < frame->num_frames; n++) {
        int id = frame->frame_id + n;

        if (p->want_reset) {
            pl_renderer_flush_cache(p->rr);
            pl_queue_reset(p->queue);
            p->last_pts = 0.0;
            p->last_id = 0;
            p->want_reset = false;
        }

        if (id <= p->last_id)
            continue; // ignore already seen frames

        struct mp_image *mpi = mp_image_new_ref(frame->frames[n]);
        struct frame_priv *fp = talloc_zero(mpi, struct frame_priv);
        mpi->priv = fp;
        fp->vo = vo;

        pl_queue_push(p->queue, &(struct pl_source_frame) {
            .pts = mpi->pts,
            .duration = can_interpolate ? frame->approx_duration : 0,
            .frame_data = mpi,
            .map = map_frame,
            .unmap = unmap_frame,
            .discard = discard_frame,
        });

        p->last_id = id;
    }

    if (p->next_opts->target_hint && frame->current) {
        struct pl_color_space hint = frame->current->params.color;
        if (opts->target_prim)
            hint.primaries = opts->target_prim;
        if (opts->target_trc)
            hint.transfer = opts->target_trc;
        if (opts->target_peak)
            hint.hdr.max_luma = opts->target_peak;
        apply_target_contrast(p, &hint);
        pl_swapchain_colorspace_hint(p->sw, &hint);
    } else if (!p->next_opts->target_hint) {
        pl_swapchain_colorspace_hint(p->sw, NULL);
    }

    struct pl_swapchain_frame swframe;
    struct ra_swapchain *sw = p->ra_ctx->swapchain;
    bool should_draw = sw->fns->start_frame(sw, NULL); // for wayland logic
    if (!should_draw || !pl_swapchain_start_frame(p->sw, &swframe)) {
        if (frame->current) {
            // Advance the queue state to the current PTS to discard unused frames
            pl_queue_update(p->queue, NULL, pl_queue_params(
                .pts = frame->current->pts + pts_offset,
                .radius = pl_frame_mix_radius(&params),
                .vsync_duration = can_interpolate ? frame->ideal_frame_vsync_duration : 0,
#if PL_API_VER >= 340
                .drift_compensation = 0,
#endif
            ));
        }
        return;
    }

    bool valid = false;
    p->is_interpolated = false;

    // Calculate target
    struct pl_frame target;
    pl_frame_from_swapchain(&target, &swframe);
    apply_target_options(p, &target);
    update_overlays(vo, p->osd_res,
                    (frame->current && opts->blend_subs) ? OSD_DRAW_OSD_ONLY : 0,
                    PL_OVERLAY_COORDS_DST_FRAME, &p->osd_state, &target, frame->current);
    apply_crop(&target, p->dst, swframe.fbo->params.w, swframe.fbo->params.h);
    update_tm_viz(&pars->color_map_params, &target);

    struct pl_frame_mix mix = {0};
    if (frame->current) {
        // Update queue state
        struct pl_queue_params qparams = *pl_queue_params(
            .pts = frame->current->pts + pts_offset,
            .radius = pl_frame_mix_radius(&params),
            .vsync_duration = can_interpolate ? frame->ideal_frame_vsync_duration : 0,
            .interpolation_threshold = opts->interpolation_threshold,
#if PL_API_VER >= 340
            .drift_compensation = 0,
#endif
        );

        // Depending on the vsync ratio, we may be up to half of the vsync
        // duration before the current frame time. This works fine because
        // pl_queue will have this frame, unless it's after a reset event. In
        // this case, start from the first available frame.
        struct pl_source_frame first;
        if (pl_queue_peek(p->queue, 0, &first) && qparams.pts < first.pts) {
            if (first.pts != frame->current->pts)
                MP_VERBOSE(vo, "Current PTS(%f) != VPTS(%f)\n", frame->current->pts, first.pts);
            MP_VERBOSE(vo, "Clamping first frame PTS from %f to %f\n", qparams.pts, first.pts);
            qparams.pts = first.pts;
        }
        p->last_pts = qparams.pts;

        switch (pl_queue_update(p->queue, &mix, &qparams)) {
        case PL_QUEUE_ERR:
            MP_ERR(vo, "Failed updating frames!\n");
            goto done;
        case PL_QUEUE_EOF:
            abort(); // we never signal EOF
        case PL_QUEUE_MORE:
            // This is expected to happen semi-frequently near the start and
            // end of a file, so only log it at high verbosity and move on.
            MP_DBG(vo, "Render queue underrun.\n");
            break;
        case PL_QUEUE_OK:
            break;
        }

        // Update source crop and overlays on all existing frames. We
        // technically own the `pl_frame` struct so this is kosher. This could
        // be partially avoided by instead flushing the queue on resizes, but
        // doing it this way avoids unnecessarily re-uploading frames.
        for (int i = 0; i < mix.num_frames; i++) {
            struct pl_frame *image = (struct pl_frame *) mix.frames[i];
            struct mp_image *mpi = image->user_data;
            struct frame_priv *fp = mpi->priv;
            apply_crop(image, p->src, vo->params->w, vo->params->h);
            if (opts->blend_subs) {
                if (frame->redraw)
                    p->osd_sync++;
                if (fp->osd_sync < p->osd_sync) {
                    float rx = pl_rect_w(p->dst) / pl_rect_w(image->crop);
                    float ry = pl_rect_h(p->dst) / pl_rect_h(image->crop);
                    struct mp_osd_res res = {
                        .w = pl_rect_w(p->dst),
                        .h = pl_rect_h(p->dst),
                        .ml = -image->crop.x0 * rx,
                        .mr = (image->crop.x1 - vo->params->w) * rx,
                        .mt = -image->crop.y0 * ry,
                        .mb = (image->crop.y1 - vo->params->h) * ry,
                        .display_par = 1.0,
                    };
                    update_overlays(vo, res, OSD_DRAW_SUB_ONLY,
                                    PL_OVERLAY_COORDS_DST_CROP,
                                    &fp->subs, image, mpi);
                    fp->osd_sync = p->osd_sync;
                }
            } else {
                // Disable overlays when blend_subs is disabled
                image->num_overlays = 0;
                fp->osd_sync = 0;
            }

            // Update the frame signature to include the current OSD sync
            // value, in order to disambiguate between identical frames with
            // modified OSD. Shift the OSD sync value by a lot to avoid
            // collisions with low signature values.
            //
            // This is safe to do because `pl_frame_mix.signature` lives in
            // temporary memory that is only valid for this `pl_queue_update`.
            ((uint64_t *) mix.signatures)[i] ^= fp->osd_sync << 48;
        }
    }

    // Render frame
    if (!pl_render_image_mix(p->rr, &mix, &target, &params)) {
        MP_ERR(vo, "Failed rendering frame!\n");
        goto done;
    }

    mp_mutex_lock(&vo->params_mutex);
    if (!vo->target_params)
        vo->target_params = talloc(vo, struct mp_image_params);
    *vo->target_params = (struct mp_image_params){
        .imgfmt_name = swframe.fbo->params.format
                        ? swframe.fbo->params.format->name : NULL,
        .w = swframe.fbo->params.w,
        .h = swframe.fbo->params.h,
        .color = target.color,
        .repr = target.repr,
        .rotate = target.rotation,
    };

    const struct pl_frame *cur_frame = pl_frame_mix_nearest(&mix);
    if (cur_frame && vo->params) {
        vo->params->color.hdr = cur_frame->color.hdr;
        // Augment metadata with peak detection max_pq_y / avg_pq_y
        pl_renderer_get_hdr_metadata(p->rr, &vo->params->color.hdr);
    }
    mp_mutex_unlock(&vo->params_mutex);

    p->is_interpolated = pts_offset != 0 && mix.num_frames > 1;
    valid = true;
    // fall through

done:
    if (!valid) // clear with purple to indicate error
        pl_tex_clear(gpu, swframe.fbo, (float[4]){ 0.5, 0.0, 1.0, 1.0 });

    pl_gpu_flush(gpu);
    p->frame_pending = true;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct ra_swapchain *sw = p->ra_ctx->swapchain;

    if (p->frame_pending) {
        if (!pl_swapchain_submit_frame(p->sw))
            MP_ERR(vo, "Failed presenting frame!\n");
        p->frame_pending = false;
    }

    sw->fns->swap_buffers(sw);
}

static void get_vsync(struct vo *vo, struct vo_vsync_info *info)
{
    struct priv *p = vo->priv;
    struct ra_swapchain *sw = p->ra_ctx->swapchain;
    if (sw->fns->get_vsync)
        sw->fns->get_vsync(sw, info);
}

static int query_format(struct vo *vo, int format)
{
    struct priv *p = vo->priv;
    if (ra_hwdec_get(&p->hwdec_ctx, format))
        return true;

    struct pl_bit_encoding bits;
    struct pl_plane_data data[4] = {0};
    int planes = plane_data_from_imgfmt(data, &bits, format);
    if (!planes)
        return false;

    for (int i = 0; i < planes; i++) {
        if (!pl_plane_find_fmt(p->gpu, NULL, &data[i]))
            return false;
    }

    return true;
}

static void resize(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct mp_rect src, dst;
    struct mp_osd_res osd;
    vo_get_src_dst_rects(vo, &src, &dst, &osd);
    if (vo->dwidth && vo->dheight) {
        gpu_ctx_resize(p->context, vo->dwidth, vo->dheight);
        vo->want_redraw = true;
    }

    if (mp_rect_equals(&p->src, &src) &&
        mp_rect_equals(&p->dst, &dst) &&
        osd_res_equals(p->osd_res, osd))
        return;

    p->osd_sync++;
    p->osd_res = osd;
    p->src = src;
    p->dst = dst;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;
    if (!p->ra_ctx->fns->reconfig(p->ra_ctx))
        return -1;

    resize(vo);
    return 0;
}

// Takes over ownership of `icc`. Can be used to unload profile (icc.len == 0)
static bool update_icc(struct priv *p, struct bstr icc)
{
    struct pl_icc_profile profile = {
        .data = icc.start,
        .len  = icc.len,
    };

    pl_icc_profile_compute_signature(&profile);

    bool ok = pl_icc_update(p->pllog, &p->icc_profile, &profile, &p->icc_params);
    talloc_free(icc.start);
    return ok;
}

// Returns whether the ICC profile was updated (even on failure)
static bool update_auto_profile(struct priv *p, int *events)
{
    const struct gl_video_opts *opts = p->opts_cache->opts;
    if (!opts->icc_opts || !opts->icc_opts->profile_auto || p->icc_path)
        return false;

    MP_VERBOSE(p, "Querying ICC profile...\n");
    bstr icc = {0};
    int r = p->ra_ctx->fns->control(p->ra_ctx, events, VOCTRL_GET_ICC_PROFILE, &icc);

    if (r != VO_NOTAVAIL) {
        if (r == VO_FALSE) {
            MP_WARN(p, "Could not retrieve an ICC profile.\n");
        } else if (r == VO_NOTIMPL) {
            MP_ERR(p, "icc-profile-auto not implemented on this platform.\n");
        }

        update_icc(p, icc);
        return true;
    }

    return false;
}

static void video_screenshot(struct vo *vo, struct voctrl_screenshot *args)
{
    struct priv *p = vo->priv;
    pl_options pars = p->pars;
    pl_gpu gpu = p->gpu;
    pl_tex fbo = NULL;
    args->res = NULL;

    update_options(vo);
    struct pl_render_params params = pars->params;
    params.info_callback = NULL;
    params.skip_caching_single_frame = true;
    params.preserve_mixing_cache = false;
    params.frame_mixer = NULL;

    struct pl_peak_detect_params peak_params;
    if (params.peak_detect_params) {
        peak_params = *params.peak_detect_params;
        params.peak_detect_params = &peak_params;
        peak_params.allow_delayed = false;
    }

    // Retrieve the current frame from the frame queue
    struct pl_frame_mix mix;
    enum pl_queue_status status;
    status = pl_queue_update(p->queue, &mix, pl_queue_params(
        .pts = p->last_pts,
#if PL_API_VER >= 340
        .drift_compensation = 0,
#endif
    ));
    assert(status != PL_QUEUE_EOF);
    if (status == PL_QUEUE_ERR) {
        MP_ERR(vo, "Unknown error occurred while trying to take screenshot!\n");
        return;
    }
    if (!mix.num_frames) {
        MP_ERR(vo, "No frames available to take screenshot of, is a file loaded?\n");
        return;
    }

    // Passing an interpolation radius of 0 guarantees that the first frame in
    // the resulting mix is the correct frame for this PTS
    struct pl_frame image = *(struct pl_frame *) mix.frames[0];
    struct mp_image *mpi = image.user_data;
    struct mp_rect src = p->src, dst = p->dst;
    struct mp_osd_res osd = p->osd_res;
    if (!args->scaled) {
        int w, h;
        mp_image_params_get_dsize(&mpi->params, &w, &h);
        if (w < 1 || h < 1)
            return;

        int src_w = mpi->params.w;
        int src_h = mpi->params.h;
        src = (struct mp_rect) {0, 0, src_w, src_h};
        dst = (struct mp_rect) {0, 0, w, h};

        if (mp_image_crop_valid(&mpi->params))
            src = mpi->params.crop;

        if (mpi->params.rotate % 180 == 90) {
            MPSWAP(int, w, h);
            MPSWAP(int, src_w, src_h);
        }
        mp_rect_rotate(&src, src_w, src_h, mpi->params.rotate);
        mp_rect_rotate(&dst, w, h, mpi->params.rotate);

        osd = (struct mp_osd_res) {
            .display_par = 1.0,
            .w = mp_rect_w(dst),
            .h = mp_rect_h(dst),
        };
    }

    // Create target FBO, try high bit depth first
    int mpfmt;
    for (int depth = args->high_bit_depth ? 16 : 8; depth; depth -= 8) {
        if (depth == 16) {
            mpfmt = IMGFMT_RGBA64;
        } else {
            mpfmt = p->ra_ctx->opts.want_alpha ? IMGFMT_RGBA : IMGFMT_RGB0;
        }
        pl_fmt fmt = pl_find_fmt(gpu, PL_FMT_UNORM, 4, depth, depth,
                                 PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_HOST_READABLE);
        if (!fmt)
            continue;

        fbo = pl_tex_create(gpu, pl_tex_params(
            .w = osd.w,
            .h = osd.h,
            .format = fmt,
            .blit_dst = true,
            .renderable = true,
            .host_readable = true,
            .storable = fmt->caps & PL_FMT_CAP_STORABLE,
        ));
        if (fbo)
            break;
    }

    if (!fbo) {
        MP_ERR(vo, "Failed creating target FBO for screenshot!\n");
        return;
    }

    struct pl_frame target = {
        .repr = pl_color_repr_rgb,
        .num_planes = 1,
        .planes[0] = {
            .texture = fbo,
            .components = 4,
            .component_mapping = {0, 1, 2, 3},
        },
    };

    if (args->scaled) {
        // Apply target LUT, ICC profile and CSP override only in window mode
        apply_target_options(p, &target);
    } else if (args->native_csp) {
        target.color = image.color;
    } else {
        target.color = pl_color_space_srgb;
    }

    apply_crop(&image, src, mpi->params.w, mpi->params.h);
    apply_crop(&target, dst, fbo->params.w, fbo->params.h);
    update_tm_viz(&pars->color_map_params, &target);

    int osd_flags = 0;
    if (!args->subs)
        osd_flags |= OSD_DRAW_OSD_ONLY;
    if (!args->osd)
        osd_flags |= OSD_DRAW_SUB_ONLY;

    const struct gl_video_opts *opts = p->opts_cache->opts;
    struct frame_priv *fp = mpi->priv;
    if (opts->blend_subs) {
        float rx = pl_rect_w(dst) / pl_rect_w(image.crop);
        float ry = pl_rect_h(dst) / pl_rect_h(image.crop);
        struct mp_osd_res res = {
            .w = pl_rect_w(dst),
            .h = pl_rect_h(dst),
            .ml = -image.crop.x0 * rx,
            .mr = (image.crop.x1 - vo->params->w) * rx,
            .mt = -image.crop.y0 * ry,
            .mb = (image.crop.y1 - vo->params->h) * ry,
            .display_par = 1.0,
        };
        update_overlays(vo, res, osd_flags,
                        PL_OVERLAY_COORDS_DST_CROP,
                        &fp->subs, &image, mpi);
    } else {
        // Disable overlays when blend_subs is disabled
        update_overlays(vo, osd, osd_flags, PL_OVERLAY_COORDS_DST_FRAME,
                        &p->osd_state, &target, mpi);
        image.num_overlays = 0;
    }

    if (!pl_render_image(p->rr, &image, &target, &params)) {
        MP_ERR(vo, "Failed rendering frame!\n");
        goto done;
    }

    args->res = mp_image_alloc(mpfmt, fbo->params.w, fbo->params.h);
    if (!args->res)
        goto done;

    args->res->params.color.primaries = target.color.primaries;
    args->res->params.color.transfer = target.color.transfer;
    args->res->params.repr.levels = target.repr.levels;
    args->res->params.color.hdr = target.color.hdr;
    if (args->scaled)
        args->res->params.p_w = args->res->params.p_h = 1;

    bool ok = pl_tex_download(gpu, pl_tex_transfer_params(
        .tex = fbo,
        .ptr = args->res->planes[0],
        .row_pitch = args->res->stride[0],
    ));

    if (!ok)
        TA_FREEP(&args->res);

    // fall through
done:
    pl_tex_destroy(gpu, &fbo);
}

static inline void copy_frame_info_to_mp(struct frame_info *pl,
                                         struct mp_frame_perf *mp) {
    static_assert(MP_ARRAY_SIZE(pl->info) == MP_ARRAY_SIZE(mp->perf), "");
    assert(pl->count <= VO_PASS_PERF_MAX);
    mp->count = MPMIN(pl->count, VO_PASS_PERF_MAX);

    for (int i = 0; i < mp->count; ++i) {
        const struct pl_dispatch_info *pass = &pl->info[i];

        static_assert(VO_PERF_SAMPLE_COUNT >= MP_ARRAY_SIZE(pass->samples), "");
        assert(pass->num_samples <= MP_ARRAY_SIZE(pass->samples));

        struct mp_pass_perf *perf = &mp->perf[i];
        perf->count = MPMIN(pass->num_samples, VO_PERF_SAMPLE_COUNT);
        memcpy(perf->samples, pass->samples, perf->count * sizeof(pass->samples[0]));
        perf->last = pass->last;
        perf->peak = pass->peak;
        perf->avg = pass->average;

        strncpy(mp->desc[i], pass->shader->description, sizeof(mp->desc[i]) - 1);
        mp->desc[i][sizeof(mp->desc[i]) - 1] = '\0';
    }
}

static void update_ra_ctx_options(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct gl_video_opts *gl_opts = p->opts_cache->opts;
    bool border_alpha = (p->next_opts->border_background == BACKGROUND_COLOR &&
                         gl_opts->background_color.a != 255) ||
                         p->next_opts->border_background == BACKGROUND_NONE;
    p->ra_ctx->opts.want_alpha = (gl_opts->background == BACKGROUND_COLOR &&
                                  gl_opts->background_color.a != 255) ||
                                  gl_opts->background == BACKGROUND_NONE ||
                                  border_alpha;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;

    switch (request) {
    case VOCTRL_SET_PANSCAN:
        resize(vo);
        return VO_TRUE;
    case VOCTRL_PAUSE:
        if (p->is_interpolated)
            vo->want_redraw = true;
        return VO_TRUE;

    case VOCTRL_UPDATE_RENDER_OPTS: {
        m_config_cache_update(p->opts_cache);
        update_ra_ctx_options(vo);
        if (p->ra_ctx->fns->update_render_opts)
            p->ra_ctx->fns->update_render_opts(p->ra_ctx);
        update_render_options(vo);
        vo->want_redraw = true;

        // Also re-query the auto profile, in case `update_render_options`
        // unloaded a manually specified icc profile in favor of
        // icc-profile-auto
        int events = 0;
        update_auto_profile(p, &events);
        vo_event(vo, events);
        return VO_TRUE;
    }

    case VOCTRL_RESET:
        // Defer until the first new frame (unique ID) actually arrives
        p->want_reset = true;
        return VO_TRUE;

    case VOCTRL_PERFORMANCE_DATA: {
        struct voctrl_performance_data *perf = data;
        copy_frame_info_to_mp(&p->perf_fresh, &perf->fresh);
        copy_frame_info_to_mp(&p->perf_redraw, &perf->redraw);
        return true;
    }

    case VOCTRL_SCREENSHOT:
        video_screenshot(vo, data);
        return true;

    case VOCTRL_EXTERNAL_RESIZE:
        reconfig(vo, NULL);
        return true;

    case VOCTRL_LOAD_HWDEC_API:
        ra_hwdec_ctx_load_fmt(&p->hwdec_ctx, vo->hwdec_devs, data);
        return true;
    }

    int events = 0;
    int r = p->ra_ctx->fns->control(p->ra_ctx, &events, request, data);
    if (events & VO_EVENT_ICC_PROFILE_CHANGED) {
        if (update_auto_profile(p, &events))
            vo->want_redraw = true;
    }
    if (events & VO_EVENT_RESIZE)
        resize(vo);
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    vo_event(vo, events);

    return r;
}

static void wakeup(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->ra_ctx && p->ra_ctx->fns->wakeup)
        p->ra_ctx->fns->wakeup(p->ra_ctx);
}

static void wait_events(struct vo *vo, int64_t until_time_ns)
{
    struct priv *p = vo->priv;
    if (p->ra_ctx && p->ra_ctx->fns->wait_events) {
        p->ra_ctx->fns->wait_events(p->ra_ctx, until_time_ns);
    } else {
        vo_wait_default(vo, until_time_ns);
    }
}

static char *cache_filepath(void *ta_ctx, char *dir, const char *prefix, uint64_t key)
{
    bstr filename = {0};
    bstr_xappend_asprintf(ta_ctx, &filename, "%s_%016" PRIx64, prefix, key);
    return mp_path_join_bstr(ta_ctx, bstr0(dir), filename);
}

static pl_cache_obj cache_load_obj(void *p, uint64_t key)
{
    struct cache *c = p;
    void *ta_ctx = talloc_new(NULL);
    pl_cache_obj obj = {0};

    if (!c->dir)
        goto done;

    char *filepath = cache_filepath(ta_ctx, c->dir, c->name, key);
    if (!filepath)
        goto done;

    if (stat(filepath, &(struct stat){0}))
        goto done;

    int64_t load_start = mp_time_ns();
    struct bstr data = stream_read_file(filepath, ta_ctx, c->global, STREAM_MAX_READ_SIZE);
    int64_t load_end = mp_time_ns();
    MP_DBG(c, "%s: key(%" PRIx64 "), size(%zu), load time(%.3f ms)\n",
           __func__, key, data.len,
           MP_TIME_NS_TO_MS(load_end - load_start));

    obj = (pl_cache_obj){
        .key = key,
        .data = talloc_steal(NULL, data.start),
        .size = data.len,
        .free = talloc_free,
    };

done:
    talloc_free(ta_ctx);
    return obj;
}

static void cache_save_obj(void *p, pl_cache_obj obj)
{
    if (!obj.data || !obj.size)
        return;

    const struct cache *c = p;
    void *ta_ctx = talloc_new(NULL);

    if (!c->dir)
        goto done;

    char *filepath = cache_filepath(ta_ctx, c->dir, c->name, obj.key);
    if (!filepath)
        goto done;

    // Don't save if already exists
    if (!stat(filepath, &(struct stat){0})) {
        MP_DBG(c, "%s: key(%"PRIx64"), size(%zu)\n", __func__, obj.key, obj.size);
        goto done;
    }

    int64_t save_start = mp_time_ns();
    mp_save_to_file(filepath, obj.data, obj.size);
    int64_t save_end = mp_time_ns();
    MP_DBG(c, "%s: key(%" PRIx64 "), size(%zu), save time(%.3f ms)\n",
           __func__, obj.key, obj.size,
           MP_TIME_NS_TO_MS(save_end - save_start));

done:
    talloc_free(ta_ctx);
}

static void cache_init(struct vo *vo, struct cache *cache, size_t max_size,
                       const char *dir_opt)
{
    struct priv *p = vo->priv;
    const char *name = cache == &p->shader_cache ? "shader" : "icc";
    const size_t limit = cache == &p->shader_cache ? 128 << 20 : 1536 << 20;

    char *dir;
    if (dir_opt && dir_opt[0]) {
        dir = mp_get_user_path(vo, p->global, dir_opt);
    } else {
        dir = mp_find_user_file(vo, p->global, "cache", "");
    }
    if (!dir || !dir[0])
        return;

    mp_mkdirp(dir);
    *cache = (struct cache){
        .log        = p->log,
        .global     = p->global,
        .dir        = dir,
        .name       = name,
        .size_limit = limit,
        .cache = pl_cache_create(pl_cache_params(
            .log = p->pllog,
            .get = cache_load_obj,
            .set = cache_save_obj,
            .priv = cache
        )),
    };
}

struct file_entry {
    char *filepath;
    size_t size;
    time_t atime;
};

static int compare_atime(const void *a, const void *b)
{
    return (((struct file_entry *)b)->atime - ((struct file_entry *)a)->atime);
}

static void cache_uninit(struct priv *p, struct cache *cache)
{
    if (!cache->cache)
        return;

    void *ta_ctx = talloc_new(NULL);
    struct file_entry *files = NULL;
    size_t num_files = 0;
    assert(cache->dir);
    assert(cache->name);

    DIR *d = opendir(cache->dir);
    if (!d)
        goto done;

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        char *filepath = mp_path_join(ta_ctx, cache->dir, dir->d_name);
        if (!filepath)
            continue;
        struct stat filestat;
        if (stat(filepath, &filestat))
            continue;
        if (!S_ISREG(filestat.st_mode))
            continue;
        bstr fname = bstr0(dir->d_name);
        if (!bstr_eatstart0(&fname, cache->name))
            continue;
        if (!bstr_eatstart0(&fname, "_"))
            continue;
        if (fname.len != 16) // %016x
            continue;
        MP_TARRAY_APPEND(ta_ctx, files, num_files,
                         (struct file_entry){
                             .filepath = filepath,
                             .size     = filestat.st_size,
                             .atime    = filestat.st_atime,
                         });
    }
    closedir(d);

    if (!num_files)
        goto done;

    qsort(files, num_files, sizeof(struct file_entry), compare_atime);

    time_t t = time(NULL);
    size_t cache_size = 0;
    size_t cache_limit = cache->size_limit ? cache->size_limit : SIZE_MAX;
    for (int i = 0; i < num_files; i++) {
        // Remove files that exceed the size limit but are older than one day.
        // This allows for temporary maintaining a larger cache size while
        // adjusting the configuration. The cache will be cleared the next day
        // for unused entries. We don't need to be overly aggressive with cache
        // cleaning; in most cases, it will not grow much, and in others, it may
        // actually be useful to cache more.
        cache_size += files[i].size;
        double rel_use = difftime(t, files[i].atime);
        if (cache_size > cache_limit && rel_use > 60 * 60 * 24) {
            MP_VERBOSE(p, "Removing %s | size: %9zu bytes | last used: %9d seconds ago\n",
                       files[i].filepath, files[i].size, (int)rel_use);
            unlink(files[i].filepath);
        }
    }

done:
    talloc_free(ta_ctx);
    pl_cache_destroy(&cache->cache);
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    pl_queue_destroy(&p->queue); // destroy this first
    for (int i = 0; i < MP_ARRAY_SIZE(p->osd_state.entries); i++)
        pl_tex_destroy(p->gpu, &p->osd_state.entries[i].tex);
    for (int i = 0; i < p->num_sub_tex; i++)
        pl_tex_destroy(p->gpu, &p->sub_tex[i]);
    for (int i = 0; i < p->num_user_hooks; i++)
        pl_mpv_user_shader_destroy(&p->user_hooks[i].hook);

    if (vo->hwdec_devs) {
        ra_hwdec_mapper_free(&p->hwdec_mapper);
        ra_hwdec_ctx_uninit(&p->hwdec_ctx);
        hwdec_devices_set_loader(vo->hwdec_devs, NULL, NULL);
        hwdec_devices_destroy(vo->hwdec_devs);
    }

    assert(p->num_dr_buffers == 0);
    mp_mutex_destroy(&p->dr_lock);

    cache_uninit(p, &p->shader_cache);
    cache_uninit(p, &p->icc_cache);

    pl_lut_free(&p->next_opts->image_lut.lut);
    pl_lut_free(&p->next_opts->lut.lut);
    pl_lut_free(&p->next_opts->target_lut.lut);

    pl_icc_close(&p->icc_profile);
    pl_renderer_destroy(&p->rr);

    for (int i = 0; i < VO_PASS_PERF_MAX; ++i) {
        pl_shader_info_deref(&p->perf_fresh.info[i].shader);
        pl_shader_info_deref(&p->perf_redraw.info[i].shader);
    }

    pl_options_free(&p->pars);

    p->ra_ctx = NULL;
    p->pllog = NULL;
    p->gpu = NULL;
    p->sw = NULL;
    gpu_ctx_destroy(&p->context);
}

static void load_hwdec_api(void *ctx, struct hwdec_imgfmt_request *params)
{
    vo_control(ctx, VOCTRL_LOAD_HWDEC_API, params);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    p->opts_cache = m_config_cache_alloc(p, vo->global, &gl_video_conf);
    p->next_opts_cache = m_config_cache_alloc(p, vo->global, &gl_next_conf);
    p->next_opts = p->next_opts_cache->opts;
    p->video_eq = mp_csp_equalizer_create(p, vo->global);
    p->global = vo->global;
    p->log = vo->log;

    struct gl_video_opts *gl_opts = p->opts_cache->opts;
    p->context = gpu_ctx_create(vo, gl_opts);
    if (!p->context)
        goto err_out;
    // For the time being
    p->ra_ctx = p->context->ra_ctx;
    p->pllog = p->context->pllog;
    p->gpu = p->context->gpu;
    p->sw = p->context->swapchain;
    p->hwdec_ctx = (struct ra_hwdec_ctx) {
        .log = p->log,
        .global = p->global,
        .ra_ctx = p->ra_ctx,
    };
    update_ra_ctx_options(vo);

    vo->hwdec_devs = hwdec_devices_create();
    hwdec_devices_set_loader(vo->hwdec_devs, load_hwdec_api, vo);
    ra_hwdec_ctx_init(&p->hwdec_ctx, vo->hwdec_devs, gl_opts->hwdec_interop, false);
    mp_mutex_init(&p->dr_lock);

    if (gl_opts->shader_cache)
        cache_init(vo, &p->shader_cache, 10 << 20, gl_opts->shader_cache_dir);
    if (gl_opts->icc_opts->cache)
        cache_init(vo, &p->icc_cache, 20 << 20, gl_opts->icc_opts->cache_dir);

    pl_gpu_set_cache(p->gpu, p->shader_cache.cache);
    p->rr = pl_renderer_create(p->pllog, p->gpu);
    p->queue = pl_queue_create(p->gpu);
    p->osd_fmt[SUBBITMAP_LIBASS] = pl_find_named_fmt(p->gpu, "r8");
    p->osd_fmt[SUBBITMAP_BGRA] = pl_find_named_fmt(p->gpu, "bgra8");
    p->osd_sync = 1;

    p->pars = pl_options_alloc(p->pllog);
    update_render_options(vo);
    return 0;

err_out:
    uninit(vo);
    return -1;
}

static const struct pl_filter_config *map_scaler(struct priv *p,
                                                 enum scaler_unit unit)
{
    const struct pl_filter_preset fixed_scalers[] = {
        { "bilinear",       &pl_filter_bilinear },
        { "bicubic_fast",   &pl_filter_bicubic },
        { "nearest",        &pl_filter_nearest },
        { "oversample",     &pl_filter_oversample },
        {0},
    };

    const struct pl_filter_preset fixed_frame_mixers[] = {
        { "linear",         &pl_filter_bilinear },
        { "oversample",     &pl_filter_oversample },
        {0},
    };

    const struct pl_filter_preset *fixed_presets =
        unit == SCALER_TSCALE ? fixed_frame_mixers : fixed_scalers;

    const struct gl_video_opts *opts = p->opts_cache->opts;
    const struct scaler_config *cfg = &opts->scaler[unit];
    if (unit == SCALER_DSCALE && (!cfg->kernel.name || !cfg->kernel.name[0]))
        cfg = &opts->scaler[SCALER_SCALE];
    if (unit == SCALER_CSCALE && (!cfg->kernel.name || !cfg->kernel.name[0]))
        cfg = &opts->scaler[SCALER_SCALE];

    for (int i = 0; fixed_presets[i].name; i++) {
        if (strcmp(cfg->kernel.name, fixed_presets[i].name) == 0)
            return fixed_presets[i].filter;
    }

    // Attempt loading filter preset first, fall back to raw filter function
    struct scaler_params *par = &p->scalers[unit];
    const struct pl_filter_preset *preset;
    const struct pl_filter_function_preset *fpreset;
    if ((preset = pl_find_filter_preset(cfg->kernel.name))) {
        par->config = *preset->filter;
    } else if ((fpreset = pl_find_filter_function_preset(cfg->kernel.name))) {
        par->config = (struct pl_filter_config) {
            .kernel = fpreset->function,
            .params[0] = fpreset->function->params[0],
            .params[1] = fpreset->function->params[1],
        };
    } else {
        MP_ERR(p, "Failed mapping filter function '%s', no libplacebo analog?\n",
               cfg->kernel.name);
        return &pl_filter_bilinear;
    }

    const struct pl_filter_function_preset *wpreset;
    if ((wpreset = pl_find_filter_function_preset(cfg->window.name))) {
        par->config.window = wpreset->function;
        par->config.wparams[0] = wpreset->function->params[0];
        par->config.wparams[1] = wpreset->function->params[1];
    }

    for (int i = 0; i < 2; i++) {
        if (!isnan(cfg->kernel.params[i]))
            par->config.params[i] = cfg->kernel.params[i];
        if (!isnan(cfg->window.params[i]))
            par->config.wparams[i] = cfg->window.params[i];
    }

    par->config.clamp = cfg->clamp;
    if (cfg->kernel.blur > 0.0)
        par->config.blur = cfg->kernel.blur;
    if (cfg->kernel.taper > 0.0)
        par->config.taper = cfg->kernel.taper;
    if (cfg->radius > 0.0) {
        if (par->config.kernel->resizable) {
            par->config.radius = cfg->radius;
        } else {
            MP_WARN(p, "Filter radius specified but filter '%s' is not "
                    "resizable, ignoring\n", cfg->kernel.name);
        }
    }

    return &par->config;
}

static const struct pl_hook *load_hook(struct priv *p, const char *path)
{
    if (!path || !path[0])
        return NULL;

    for (int i = 0; i < p->num_user_hooks; i++) {
        if (strcmp(p->user_hooks[i].path, path) == 0)
            return p->user_hooks[i].hook;
    }

    char *fname = mp_get_user_path(NULL, p->global, path);
    bstr shader = stream_read_file(fname, p, p->global, 1000000000); // 1GB
    talloc_free(fname);

    const struct pl_hook *hook = NULL;
    if (shader.len)
        hook = pl_mpv_user_shader_parse(p->gpu, shader.start, shader.len);

    MP_TARRAY_APPEND(p, p->user_hooks, p->num_user_hooks, (struct user_hook) {
        .path = talloc_strdup(p, path),
        .hook = hook,
    });

    return hook;
}

static void update_icc_opts(struct priv *p, const struct mp_icc_opts *opts)
{
    if (!opts)
        return;

    if (!opts->profile_auto && !p->icc_path) {
        // Un-set any auto-loaded profiles if icc-profile-auto was disabled
        update_icc(p, (bstr) {0});
    }

    int s_r = 0, s_g = 0, s_b = 0;
    gl_parse_3dlut_size(opts->size_str, &s_r, &s_g, &s_b);
    p->icc_params = pl_icc_default_params;
    p->icc_params.intent = opts->intent;
    p->icc_params.size_r = s_r;
    p->icc_params.size_g = s_g;
    p->icc_params.size_b = s_b;
    p->icc_params.cache = p->icc_cache.cache;

    if (!opts->profile || !opts->profile[0]) {
        // No profile enabled, un-load any existing profiles
        update_icc(p, (bstr) {0});
        TA_FREEP(&p->icc_path);
        return;
    }

    if (p->icc_path && strcmp(opts->profile, p->icc_path) == 0)
        return; // ICC profile hasn't changed

    char *fname = mp_get_user_path(NULL, p->global, opts->profile);
    MP_VERBOSE(p, "Opening ICC profile '%s'\n", fname);
    struct bstr icc = stream_read_file(fname, p, p->global, 100000000); // 100 MB
    talloc_free(fname);
    update_icc(p, icc);

    // Update cached path
    talloc_free(p->icc_path);
    p->icc_path = talloc_strdup(p, opts->profile);
}

static void update_lut(struct priv *p, struct user_lut *lut)
{
    if (!lut->opt) {
        pl_lut_free(&lut->lut);
        TA_FREEP(&lut->path);
        return;
    }

    if (lut->path && strcmp(lut->path, lut->opt) == 0)
        return; // no change

    // Update cached path
    pl_lut_free(&lut->lut);
    talloc_free(lut->path);
    lut->path = talloc_strdup(p, lut->opt);

    // Load LUT file
    char *fname = mp_get_user_path(NULL, p->global, lut->path);
    MP_VERBOSE(p, "Loading custom LUT '%s'\n", fname);
    struct bstr lutdata = stream_read_file(fname, p, p->global, 100000000); // 100 MB
    lut->lut = pl_lut_parse_cube(p->pllog, lutdata.start, lutdata.len);
    talloc_free(fname);
    talloc_free(lutdata.start);
}

static void update_hook_opts(struct priv *p, char **opts, const char *shaderpath,
                             const struct pl_hook *hook)
{
    if (!opts)
        return;

    const char *basename = mp_basename(shaderpath);
    struct bstr shadername;
    if (!mp_splitext(basename, &shadername))
        shadername = bstr0(basename);

    for (int n = 0; opts[n * 2]; n++) {
        struct bstr k = bstr0(opts[n * 2 + 0]);
        struct bstr v = bstr0(opts[n * 2 + 1]);
        int pos;
        if ((pos = bstrchr(k, '/')) >= 0) {
            if (!bstr_equals(bstr_splice(k, 0, pos), shadername))
                continue;
            k = bstr_cut(k, pos + 1);
        }

        for (int i = 0; i < hook->num_parameters; i++) {
            const struct pl_hook_par *hp = &hook->parameters[i];
            if (!bstr_equals0(k, hp->name) != 0)
                continue;

            m_option_t opt = {
                .name = hp->name,
            };

            if (hp->names) {
                for (int j = hp->minimum.i; j <= hp->maximum.i; j++) {
                    if (bstr_equals0(v, hp->names[j])) {
                        hp->data->i = j;
                        goto next_hook;
                    }
                }
            }

            switch (hp->type) {
            case PL_VAR_FLOAT:
                opt.type = &m_option_type_float;
                opt.min = hp->minimum.f;
                opt.max = hp->maximum.f;
                break;
            case PL_VAR_SINT:
                opt.type = &m_option_type_int;
                opt.min = hp->minimum.i;
                opt.max = hp->maximum.i;
                break;
            case PL_VAR_UINT:
                opt.type = &m_option_type_int;
                opt.min = MPMIN(hp->minimum.u, INT_MAX);
                opt.max = MPMIN(hp->maximum.u, INT_MAX);
                break;
            }

            if (!opt.type)
                goto next_hook;

            opt.type->parse(p->log, &opt, k, v, hp->data);
            goto next_hook;
        }

    next_hook:;
    }
}

static void update_render_options(struct vo *vo)
{
    struct priv *p = vo->priv;
    pl_options pars = p->pars;
    const struct gl_video_opts *opts = p->opts_cache->opts;
    pars->params.antiringing_strength = opts->scaler[0].antiring;
    pars->params.background_color[0] = opts->background_color.r / 255.0;
    pars->params.background_color[1] = opts->background_color.g / 255.0;
    pars->params.background_color[2] = opts->background_color.b / 255.0;
    pars->params.background_transparency = 1 - opts->background_color.a / 255.0;
    pars->params.skip_anti_aliasing = !opts->correct_downscaling;
    pars->params.disable_linear_scaling = !opts->linear_downscaling && !opts->linear_upscaling;
    pars->params.disable_fbos = opts->dumb_mode == 1;

#if PL_API_VER >= 346
    int map_background_types[3] = {
        PL_CLEAR_SKIP,  // BACKGROUND_NONE
        PL_CLEAR_COLOR, // BACKGROUND_COLOR
        PL_CLEAR_TILES, // BACKGROUND_TILES
    };
    pars->params.background = map_background_types[opts->background];
    pars->params.border = map_background_types[p->next_opts->border_background];
#else
    pars->params.blend_against_tiles = opts->background == BACKGROUND_TILES;
#endif

    pars->params.corner_rounding = p->next_opts->corner_rounding;
    pars->params.correct_subpixel_offsets = !opts->scaler_resizes_only;

    // Map scaler options as best we can
    pars->params.upscaler = map_scaler(p, SCALER_SCALE);
    pars->params.downscaler = map_scaler(p, SCALER_DSCALE);
    pars->params.plane_upscaler = map_scaler(p, SCALER_CSCALE);
    pars->params.frame_mixer = opts->interpolation ? map_scaler(p, SCALER_TSCALE) : NULL;

    // Request as many frames as required from the decoder, depending on the
    // speed VPS/FPS ratio libplacebo may need more frames. Request frames up to
    // ratio of 1/2, but only if anti aliasing is enabled.
    int req_frames = 2;
    if (pars->params.frame_mixer) {
        req_frames += ceilf(pars->params.frame_mixer->kernel->radius) *
                      (pars->params.skip_anti_aliasing ? 1 : 2);
    }
    vo_set_queue_params(vo, 0, MPMIN(VO_MAX_REQ_FRAMES, req_frames));

    pars->params.deband_params = opts->deband ? &pars->deband_params : NULL;
    pars->deband_params.iterations = opts->deband_opts->iterations;
    pars->deband_params.radius = opts->deband_opts->range;
    pars->deband_params.threshold = opts->deband_opts->threshold / 16.384;
    pars->deband_params.grain = opts->deband_opts->grain / 8.192;

    pars->params.sigmoid_params = opts->sigmoid_upscaling ? &pars->sigmoid_params : NULL;
    pars->sigmoid_params.center = opts->sigmoid_center;
    pars->sigmoid_params.slope = opts->sigmoid_slope;

    pars->params.peak_detect_params = opts->tone_map.compute_peak >= 0 ? &pars->peak_detect_params : NULL;
    pars->peak_detect_params.smoothing_period = opts->tone_map.decay_rate;
    pars->peak_detect_params.scene_threshold_low = opts->tone_map.scene_threshold_low;
    pars->peak_detect_params.scene_threshold_high = opts->tone_map.scene_threshold_high;
    pars->peak_detect_params.percentile = opts->tone_map.peak_percentile;
    pars->peak_detect_params.allow_delayed = p->next_opts->delayed_peak;

    const struct pl_tone_map_function * const tone_map_funs[] = {
        [TONE_MAPPING_AUTO]     = &pl_tone_map_auto,
        [TONE_MAPPING_CLIP]     = &pl_tone_map_clip,
        [TONE_MAPPING_MOBIUS]   = &pl_tone_map_mobius,
        [TONE_MAPPING_REINHARD] = &pl_tone_map_reinhard,
        [TONE_MAPPING_HABLE]    = &pl_tone_map_hable,
        [TONE_MAPPING_GAMMA]    = &pl_tone_map_gamma,
        [TONE_MAPPING_LINEAR]   = &pl_tone_map_linear,
        [TONE_MAPPING_SPLINE]   = &pl_tone_map_spline,
        [TONE_MAPPING_BT_2390]  = &pl_tone_map_bt2390,
        [TONE_MAPPING_BT_2446A] = &pl_tone_map_bt2446a,
        [TONE_MAPPING_ST2094_40] = &pl_tone_map_st2094_40,
        [TONE_MAPPING_ST2094_10] = &pl_tone_map_st2094_10,
    };

    const struct pl_gamut_map_function * const gamut_modes[] = {
        [GAMUT_AUTO]            = pl_color_map_default_params.gamut_mapping,
        [GAMUT_CLIP]            = &pl_gamut_map_clip,
        [GAMUT_PERCEPTUAL]      = &pl_gamut_map_perceptual,
        [GAMUT_RELATIVE]        = &pl_gamut_map_relative,
        [GAMUT_SATURATION]      = &pl_gamut_map_saturation,
        [GAMUT_ABSOLUTE]        = &pl_gamut_map_absolute,
        [GAMUT_DESATURATE]      = &pl_gamut_map_desaturate,
        [GAMUT_DARKEN]          = &pl_gamut_map_darken,
        [GAMUT_WARN]            = &pl_gamut_map_highlight,
        [GAMUT_LINEAR]          = &pl_gamut_map_linear,
    };

    pars->color_map_params.tone_mapping_function = tone_map_funs[opts->tone_map.curve];
    pars->color_map_params.tone_mapping_param = opts->tone_map.curve_param;
    if (isnan(pars->color_map_params.tone_mapping_param)) // vo_gpu compatibility
        pars->color_map_params.tone_mapping_param = 0.0;
    pars->color_map_params.inverse_tone_mapping = opts->tone_map.inverse;
    pars->color_map_params.contrast_recovery = opts->tone_map.contrast_recovery;
    pars->color_map_params.visualize_lut = opts->tone_map.visualize;
    pars->color_map_params.contrast_smoothness = opts->tone_map.contrast_smoothness;
    pars->color_map_params.gamut_mapping = gamut_modes[opts->tone_map.gamut_mode];

    switch (opts->dither_algo) {
    case DITHER_NONE:
        pars->params.dither_params = NULL;
        break;
    case DITHER_ERROR_DIFFUSION:
        pars->params.error_diffusion = pl_find_error_diffusion_kernel(opts->error_diffusion);
        if (!pars->params.error_diffusion) {
            MP_WARN(p, "Could not find error diffusion kernel '%s', falling "
                    "back to fruit.\n", opts->error_diffusion);
        }
        MP_FALLTHROUGH;
    case DITHER_ORDERED:
    case DITHER_FRUIT:
        pars->params.dither_params = &pars->dither_params;
        pars->dither_params.method = opts->dither_algo == DITHER_ORDERED
                                ? PL_DITHER_ORDERED_FIXED
                                : PL_DITHER_BLUE_NOISE;
        pars->dither_params.lut_size = opts->dither_size;
        pars->dither_params.temporal = opts->temporal_dither;
        break;
    }

    if (opts->dither_depth < 0)
        pars->params.dither_params = NULL;

    update_icc_opts(p, opts->icc_opts);

    pars->params.num_hooks = 0;
    const struct pl_hook *hook;
    for (int i = 0; opts->user_shaders && opts->user_shaders[i]; i++) {
        if ((hook = load_hook(p, opts->user_shaders[i]))) {
            MP_TARRAY_APPEND(p, p->hooks, pars->params.num_hooks, hook);
            update_hook_opts(p, opts->user_shader_opts, opts->user_shaders[i], hook);
        }
    }

    pars->params.hooks = p->hooks;
}

const struct vo_driver video_out_gpu_next = {
    .description = "Video output based on libplacebo",
    .name = "gpu-next",
    .caps = VO_CAP_ROTATE90 |
            VO_CAP_FILM_GRAIN |
            0x0,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .get_image_ts = get_image,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .get_vsync = get_vsync,
    .wait_events = wait_events,
    .wakeup = wakeup,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
