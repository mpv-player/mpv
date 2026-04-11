#include "video.h"
#include <libplacebo/utils/frame_queue.h>
#include <libplacebo/utils/upload.h>
#include <libplacebo/shaders/custom.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "assert.h"
#include "common/common.h"
#include "common/msg.h"
#include "libplacebo/colorspace.h"
#include "libplacebo/filters.h"
#include "libplacebo/gpu.h"
#include "libplacebo/options.h"
#include "libplacebo/renderer.h"
#include "sub/draw_bmp.h"
#include "sub/osd.h"
#include "ta/ta_talloc.h"
#include "video/csputils.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/out/vo.h"
#include "video/out/placebo/ra_pl.h"
#include "video/out/placebo/utils.h"
#include "options/m_config.h"
#include "options/options.h"
#include "options/path.h"
#include "stream/stream.h"
#include "video/out/gpu/video.h"  // For struct gl_video_opts

struct pl_video_osd_entry {
    pl_tex tex;
    struct pl_overlay_part *parts;
    int num_parts;
};

struct pl_video_osd_state {
    struct pl_video_osd_entry entries[MAX_OSD_PARTS];
    struct pl_overlay overlays[MAX_OSD_PARTS];
};

struct scaler_params {
    struct pl_filter_config config;
};

struct pl_video {
    struct mp_log *log;
    struct mpv_global *global;
    struct ra *ra;
    pl_gpu gpu;
    pl_renderer renderer;
    pl_log pl_log;

    pl_queue queue;
    uint64_t last_frame_id;
    double last_pts;

    struct mp_image_params current_params;
    struct mp_rect current_dst;
    struct osd_state *current_osd_state;

    struct mp_osd_res osd_res;
    struct pl_video_osd_state osd_state_storage;
    pl_fmt osd_fmt[SUBBITMAP_COUNT];
    pl_tex *sub_tex;
    int num_sub_tex;

    struct mp_csp_equalizer_state *video_eq;
    struct m_config_cache *opts_cache;
    struct scaler_params scalers[SCALER_COUNT];

    // User shader hooks (glsl-shaders)
    struct user_hook {
        char *path;
        const struct pl_hook *hook;
    } *user_hooks;
    int num_user_hooks;
    const struct pl_hook **hooks;  // array for pl_render_params
    int num_hooks;
};

struct frame_priv {
    struct pl_video *p;
};

// Helper from ra.c
static int plane_data_from_imgfmt(struct pl_plane_data out_data[4],
                                  struct pl_bit_encoding *out_bits,
                                  int imgfmt, bool use_uint)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(imgfmt);
    if (!desc.num_planes || !(desc.flags & MP_IMGFLAG_HAS_COMPS))
        return 0;

    if (desc.flags & MP_IMGFLAG_HWACCEL || !(desc.flags & MP_IMGFLAG_NE) ||
        desc.flags & MP_IMGFLAG_PAL ||
        ((desc.flags & MP_IMGFLAG_TYPE_FLOAT) && (desc.flags & MP_IMGFLAG_YUV)))
        return 0;

    bool has_bits = false;
    bool any_padded = false;

    for (int p = 0; p < desc.num_planes; p++) {
        struct pl_plane_data *data = &out_data[p];
        struct mp_imgfmt_comp_desc sorted[MP_NUM_COMPONENTS];
        int num_comps = 0;
        if (desc.bpp[p] % 8)
            return 0;

        for (int c = 0; c < mp_imgfmt_desc_get_num_comps(&desc); c++) {
            if (desc.comps[c].plane != p)
                continue;

            data->component_map[num_comps] = c;
            sorted[num_comps] = desc.comps[c];
            num_comps++;

            for (int i = num_comps - 1; i > 0; i--) {
                if (sorted[i].offset >= sorted[i - 1].offset)
                    break;
                MPSWAP(struct mp_imgfmt_comp_desc, sorted[i], sorted[i - 1]);
                MPSWAP(int, data->component_map[i], data->component_map[i - 1]);
            }
        }

        uint64_t total_bits = 0;
        memset(data->component_size, 0, sizeof(data->component_size));
        for (int c = 0; c < num_comps; c++) {
            data->component_size[c] = sorted[c].size;
            data->component_pad[c] = sorted[c].offset - total_bits;
            total_bits += data->component_pad[c] + data->component_size[c];
            any_padded |= sorted[c].pad;

            if (!out_bits || data->component_map[c] == 3) // PL_CHANNEL_A
                continue;

            struct pl_bit_encoding bits = {
                .sample_depth = data->component_size[c],
                .color_depth = sorted[c].size - abs(sorted[c].pad),
                .bit_shift = MPMAX(sorted[c].pad, 0),
            };

            if (!has_bits) {
                *out_bits = bits;
                has_bits = true;
            } else if (!pl_bit_encoding_equal(out_bits, &bits)) {
                *out_bits = (struct pl_bit_encoding){0};
                out_bits = NULL;
            }
        }

        data->pixel_stride = desc.bpp[p] / 8;
        data->type = (desc.flags & MP_IMGFLAG_TYPE_FLOAT)
                            ? PL_FMT_FLOAT
                            : (use_uint ? PL_FMT_UINT : PL_FMT_UNORM);
    }

    if (any_padded && !out_bits)
        return 0;

    return desc.num_planes;
}

static void cleanup_pl_frame(struct pl_video *p, struct pl_frame *frame)
{
    if (!frame) return;
    for (int i = 0; i < frame->num_planes; i++)
        pl_tex_destroy(p->gpu, &frame->planes[i].texture);
}

static bool upload_mp_image(struct pl_video *p, struct pl_frame *out_frame,
                            const struct mp_image *img)
{
    *out_frame = (struct pl_frame){
        .color = img->params.color,
        .repr = img->params.repr,
        .crop = {
            .x0 = 0, .y0 = 0,
            .x1 = img->w, .y1 = img->h,
        },
    };

    struct pl_plane_data data[4] = {0};
    int planes = plane_data_from_imgfmt(data, &out_frame->repr.bits, img->imgfmt, false);
    if (!planes) {
        mp_msg(p->log, MSGL_ERR, "Failed to describe image format '%s'\n",
               mp_imgfmt_to_name(img->imgfmt));
        return false;
    }

    out_frame->num_planes = planes;

    for (int n = 0; n < planes; n++) {
        data[n].width = mp_image_plane_w((struct mp_image *)img, n);
        data[n].height = mp_image_plane_h((struct mp_image *)img, n);
        data[n].row_stride = img->stride[n];
        data[n].pixels = img->planes[n];

        if (!pl_upload_plane(p->gpu, &out_frame->planes[n],
                             &out_frame->planes[n].texture, &data[n]))
        {
            mp_msg(p->log, MSGL_ERR, "Failed to upload mp_image plane %d\n", n);
            cleanup_pl_frame(p, out_frame);
            return false;
        }
    }

    return true;
}

static bool map_frame(pl_gpu gpu, pl_tex *tex, const struct pl_source_frame *src,
                      struct pl_frame *frame)
{
    struct mp_image *mpi = src->frame_data;
    struct frame_priv *fp = mpi->priv;
    struct pl_video *p = fp->p;

    if (!upload_mp_image(p, frame, mpi)) {
        talloc_free(mpi);
        return false;
    }

    // Set rotation from mp_image params (same as vo_gpu_next.c line 670)
    // This is critical for iPhone videos and other rotated content
    int rotation_quarter_turns = mpi->params.rotate / 90;
    frame->rotation = rotation_quarter_turns;
    
    // Debug: log rotation to help diagnose subtitle rotation issues
    // Use MSGL_WARN to ensure it's visible even when log level is set to warn
    if (rotation_quarter_turns != 0) {
        mp_msg(p->log, MSGL_WARN, "[gpu_next] Source frame rotation: %d (rotate=%d degrees)\n",
               rotation_quarter_turns, mpi->params.rotate);
    }
    
    // Store vflip in user_data context for later use in render params
    // vflip will be applied via distort_params.transform.mat in pl_video_render()
    frame->user_data = mpi;
    return true;
}

static void unmap_frame(pl_gpu gpu, struct pl_frame *frame,
                        const struct pl_source_frame *src)
{
    struct mp_image *mpi = src->frame_data;
    struct frame_priv *fp = mpi->priv;
    struct pl_video *p = fp->p;

    cleanup_pl_frame(p, frame);
    talloc_free(mpi);
}

static void discard_frame(const struct pl_source_frame *src)
{
    struct mp_image *mpi = src->frame_data;
    talloc_free(mpi);
}

struct pl_video *pl_video_init(struct mpv_global *global, struct mp_log *log, struct ra *ra)
{
    struct pl_video *p = talloc_zero(NULL, struct pl_video);
    p->log = log;
    p->global = global;
    p->ra = ra;
    p->gpu = ra_pl_get(ra);
    
    if (!p->gpu) {
        talloc_free(p);
        return NULL;
    }

    p->pl_log = mppl_log_create(p, p->log);
    p->renderer = pl_renderer_create(p->pl_log, p->gpu);
    if (!p->renderer) {
        pl_log_destroy(&p->pl_log);
        talloc_free(p);
        return NULL;
    }

    p->queue = pl_queue_create(p->gpu);

    p->osd_fmt[SUBBITMAP_LIBASS] = pl_find_fmt(p->gpu, PL_FMT_UNORM, 1, 8, 8, 0);
    p->osd_fmt[SUBBITMAP_BGRA]   = pl_find_fmt(p->gpu, PL_FMT_UNORM, 4, 8, 8, 0);

    p->video_eq = mp_csp_equalizer_create(p, global);
    
    // Cache gl_video options to access target-prim, target-trc, target-peak, etc.
    // Similar to vo_gpu_next.c: use gl_video_conf to access gl_video_opts
    extern const struct m_sub_options gl_video_conf;
    p->opts_cache = m_config_cache_alloc(p, global, &gl_video_conf);

    return p;
}

void pl_video_uninit(struct pl_video **p_ptr)
{
    struct pl_video *p = *p_ptr;
    if (!p) return;

    pl_queue_destroy(&p->queue);

    for (int i = 0; i < MP_ARRAY_SIZE(p->osd_state_storage.entries); i++) {
        struct pl_video_osd_entry *entry = &p->osd_state_storage.entries[i];
        pl_tex_destroy(p->gpu, &entry->tex);
        talloc_free(entry->parts);
    }
    for (int i = 0; i < p->num_sub_tex; i++) {
        pl_tex_destroy(p->gpu, &p->sub_tex[i]);
    }
    talloc_free(p->sub_tex);

    // Free user shader hooks
    for (int i = 0; i < p->num_user_hooks; i++)
        pl_mpv_user_shader_destroy(&p->user_hooks[i].hook);

    pl_renderer_destroy(&p->renderer);
    pl_log_destroy(&p->pl_log);

    talloc_free(p);
    *p_ptr = NULL;
}

static void update_overlays(struct pl_video *p, struct mp_osd_res res,
                            int flags, enum pl_overlay_coords coords,
                            struct pl_video_osd_state *state, struct pl_frame *frame,
                            struct mp_image *src)
{
    frame->num_overlays = 0;
    if (!p->current_osd_state)
        return;

    double pts = src ? src->pts : 0;
    struct sub_bitmap_list *subs = osd_render(p->current_osd_state, res, pts, flags, mp_draw_sub_formats);
    if (!subs) return;

    frame->overlays = state->overlays;

    for (int n = 0; n < subs->num_items; n++) {
        const struct sub_bitmaps *item = subs->items[n];
        if (!item->num_parts || !item->packed)
            continue;

        struct pl_video_osd_entry *entry = &state->entries[item->render_index];
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
            mp_msg(p->log, MSGL_ERR, "Failed recreating OSD texture!\n");
            break;
        }

        ok = pl_tex_upload(p->gpu, &(struct pl_tex_transfer_params) {
            .tex        = entry->tex,
            .rc         = { .x1 = item->packed_w, .y1 = item->packed_h, },
            .row_pitch  = item->packed->stride[0],
            .ptr        = item->packed->planes[0],
        });
        if (!ok) {
            mp_msg(p->log, MSGL_ERR, "Failed uploading OSD texture!\n");
            break;
        }

        entry->num_parts = 0;
        talloc_free(entry->parts);
        entry->parts = talloc_array(p, struct pl_overlay_part, item->num_parts);

        // Debug: log subtitle coordinates and source rotation
        int src_rotation = 0;
        if (src) {
            src_rotation = src->params.rotate / 90;
        }
        // Log subtitle info when subtitles are present (limited to avoid spam)
        static int subtitle_log_count = 0;
        if (item->num_parts > 0 && subtitle_log_count++ < 5) {
            const struct sub_bitmap *first = &item->parts[0];
            mp_msg(p->log, MSGL_INFO, "[gpu_next] Subtitle info: src_rotation=%d, "
                   "first_part: x=%d y=%d dw=%d dh=%d, res: w=%d h=%d, crop: x0=%.1f y0=%.1f x1=%.1f y1=%.1f, coords=%d\n",
                   src_rotation, first->x, first->y, first->dw, first->dh, res.w, res.h,
                   frame->crop.x0, frame->crop.y0, frame->crop.x1, frame->crop.y1, (int)coords);
        }
        
        for (int i = 0; i < item->num_parts; i++) {
            const struct sub_bitmap *b = &item->parts[i];
            if (b->dw == 0 || b->dh == 0)
                continue;
            uint32_t c = b->libass.color;

            // When target crop Y is reversed (y0 > y1) due to FLIP_Y=1, mpv's OSD
            // coordinates are in normal Y-up space, but libplacebo expects Y-down.
            // We need to flip subtitle Y coordinates to match the rendering space.
            float dst_x0 = b->x;
            float dst_y0 = b->y;
            float dst_x1 = b->x + b->dw;
            float dst_y1 = b->y + b->dh;
            
            // Check if crop is Y-reversed (this happens with FLIP_Y=1)
            bool crop_y_reversed = frame->crop.y1 < frame->crop.y0;
            if (coords == PL_OVERLAY_COORDS_DST_FRAME && crop_y_reversed) {
                // Try alternative flip: just flip each coordinate without swapping
                // This might be what libplacebo expects when crop is reversed
                float temp_y0 = dst_y0;
                float temp_y1 = dst_y1;
                dst_y0 = res.h - temp_y0;  // Flip y0 only
                dst_y1 = res.h - temp_y1;  // Flip y1 only
                
                // Debug: log the flip operation (first 3 times only)
                static int flip_log_count = 0;
                if (flip_log_count++ < 3) {
                    mp_msg(p->log, MSGL_INFO, "[gpu_next] Flipping subtitle Y (alt): "
                           "original y0=%.0f y1=%.0f -> flipped y0=%.0f y1=%.0f (res.h=%d, crop y0=%.1f y1=%.1f)\n",
                           temp_y0, temp_y1, dst_y0, dst_y1, res.h, frame->crop.y0, frame->crop.y1);
                }
            }

            struct pl_overlay_part part = {
                .src = { b->src_x, b->src_y, b->src_x + b->w, b->src_y + b->h },
                .dst = { dst_x0, dst_y0, dst_x1, dst_y1 },
                .color = {
                    (c >> 24) / 255.0f,
                    ((c >> 16) & 0xFF) / 255.0f,
                    ((c >> 8) & 0xFF) / 255.0f,
                    (255 - (c & 0xFF)) / 255.0f,
                }
            };
            entry->parts[entry->num_parts++] = part;
        }

        struct pl_overlay *ol = &state->overlays[frame->num_overlays++];
        *ol = (struct pl_overlay) {
            .tex = entry->tex,
            .parts = entry->parts,
            .num_parts = entry->num_parts,
            .color = { .primaries = PL_COLOR_PRIM_BT_709, .transfer = PL_COLOR_TRC_SRGB },
            .coords = coords,
        };

        if (item->format == SUBBITMAP_BGRA) {
            ol->mode = PL_OVERLAY_NORMAL;
            ol->repr.alpha = PL_ALPHA_PREMULTIPLIED;
            if (src) ol->color = src->params.color;
        } else if (item->format == SUBBITMAP_LIBASS) {
            ol->mode = PL_OVERLAY_MONOCHROME;
            ol->repr.alpha = PL_ALPHA_INDEPENDENT;
            if (src && item->video_color_space) ol->color = src->params.color;
        }
    }

    talloc_free(subs);
}

// Map mpv scaler options to libplacebo filter configs
// Adapted from vo_gpu_next.c map_scaler()
static const struct pl_filter_config *map_scaler(struct pl_video *p,
                                                  const struct gl_video_opts *opts,
                                                  enum scaler_unit unit)
{
    const struct pl_filter_preset fixed_scalers[] = {
        { "bilinear",       &pl_filter_bilinear },
        { "bicubic_fast",   &pl_filter_bicubic },
        { "nearest",        &pl_filter_nearest },
        { "oversample",     &pl_filter_oversample },
        {0},
    };

    const struct scaler_config *cfg = &opts->scaler[unit];
    if (cfg->kernel.function == SCALER_INHERIT)
        cfg = &opts->scaler[SCALER_SCALE];
    const char *kernel_name = m_opt_choice_str(cfg->kernel.functions,
                                               cfg->kernel.function);

    for (int i = 0; fixed_scalers[i].name; i++) {
        if (strcmp(kernel_name, fixed_scalers[i].name) == 0)
            return fixed_scalers[i].filter;
    }

    // Attempt loading filter preset first, fall back to raw filter function
    struct scaler_params *par = &p->scalers[unit];
    const struct pl_filter_preset *preset;
    const struct pl_filter_function_preset *fpreset;
    if ((preset = pl_find_filter_preset(kernel_name))) {
        par->config = *preset->filter;
    } else if ((fpreset = pl_find_filter_function_preset(kernel_name))) {
        par->config = (struct pl_filter_config) {
            .kernel = fpreset->function,
            .params[0] = fpreset->function->params[0],
            .params[1] = fpreset->function->params[1],
        };
    } else {
        mp_msg(p->log, MSGL_ERR, "Failed mapping filter '%s', no libplacebo analog\n",
               kernel_name);
        return &pl_filter_bilinear;
    }

    const struct pl_filter_function_preset *wpreset;
    if ((wpreset = pl_find_filter_function_preset(
             m_opt_choice_str(cfg->window.functions, cfg->window.function)))) {
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
    if (cfg->antiring > 0.0)
        par->config.antiring = cfg->antiring;
    if (cfg->kernel.blur > 0.0)
        par->config.blur = cfg->kernel.blur;
    if (cfg->kernel.taper > 0.0)
        par->config.taper = cfg->kernel.taper;
    if (cfg->radius > 0.0) {
        if (par->config.kernel->resizable) {
            par->config.radius = cfg->radius;
        } else {
            mp_msg(p->log, MSGL_WARN, "Filter radius specified but filter '%s' "
                    "is not resizable, ignoring\n", kernel_name);
        }
    }

    return &par->config;
}

// ── User shader hook loading (mirrors vo_gpu_next.c load_hook / update_render_options) ──

static const struct pl_hook *load_user_hook(struct pl_video *p, const char *path)
{
    // Check cache first
    for (int i = 0; i < p->num_user_hooks; i++) {
        if (strcmp(p->user_hooks[i].path, path) == 0)
            return p->user_hooks[i].hook;
    }

    char *fname = mp_get_user_path(NULL, p->global, path);
    bstr shader = stream_read_file(fname, p, p->global, 1000000000);
    talloc_free(fname);

    const struct pl_hook *hook = NULL;
    if (shader.len)
        hook = pl_mpv_user_shader_parse(p->gpu, shader.start, shader.len);

    if (!hook)
        mp_msg(p->log, MSGL_WARN, "Failed to parse user shader: %s\n", path);

    MP_TARRAY_APPEND(p, p->user_hooks, p->num_user_hooks, (struct user_hook) {
        .path = talloc_strdup(p, path),
        .hook = hook,
    });

    return hook;
}

static void update_user_shaders(struct pl_video *p, const struct gl_video_opts *opts)
{
    // Destroy old hooks before reloading
    for (int i = 0; i < p->num_user_hooks; i++)
        pl_mpv_user_shader_destroy(&p->user_hooks[i].hook);
    p->num_user_hooks = 0;

    p->num_hooks = 0;
    const struct pl_hook *hook;
    for (int i = 0; opts->user_shaders && opts->user_shaders[i]; i++) {
        if ((hook = load_user_hook(p, opts->user_shaders[i])))
            MP_TARRAY_APPEND(p, p->hooks, p->num_hooks, hook);
    }
}

void pl_video_render(struct pl_video *p, struct vo_frame *frame, pl_tex target_tex)
{
    // Build target color space from mpv options instead of hardcoding sRGB
    // This fixes HDR overexposure - libplacebo needs to know the correct target colorspace
    // Reference: vo_gpu_next.c apply_target_options() and draw_frame()
    struct pl_color_space target_color = {0};  // Start with empty, let libplacebo infer
    bool opts_changed = p->opts_cache && m_config_cache_update(p->opts_cache);

    if (p->opts_cache) {
        // opts_cache now points directly to gl_video_opts (via gl_video_conf)
        const struct gl_video_opts *gopts = p->opts_cache->opts;
        
        // Access target color space settings from gl_video_opts
        if (gopts) {
            
            // Similar to vo_gpu_next.c line 1120-1121: if target_trc is not set, default to HDR10
            // But we also need to check the source to decide
            if (frame && frame->current) {
                const struct pl_color_space *source = &frame->current->params.color;
                
                // If target_trc is not explicitly set, infer from source or default to HDR10 for HDR content
                if (gopts->target_trc) {
                    target_color.transfer = gopts->target_trc;
                } else if (pl_color_transfer_is_hdr(source->transfer)) {
                    // If source is HDR and target_trc is auto, use HDR10 transfer
                    target_color.transfer = pl_color_space_hdr10.transfer;
                } else {
                    // Default to sRGB for SDR
                    target_color = pl_color_space_srgb;
                }
            } else {
                // No frame yet, default to sRGB
                target_color = pl_color_space_srgb;
            }
            
            // Set target primaries (e.g., bt.2020, display-p3)
            // Only set if explicitly configured (not 0/auto)
            if (gopts->target_prim)
                target_color.primaries = gopts->target_prim;
            
            // Set target peak brightness in nits (max_luma)
            // This is critical for correct HDR tone mapping
            // Only set if explicitly configured (not 0/auto)
            if (gopts->target_peak > 0) {
                target_color.hdr.max_luma = gopts->target_peak;
            } else if (pl_color_transfer_is_hdr(target_color.transfer)) {
                // If HDR but no explicit peak, use nominal HDR10 peak
                pl_color_space_nominal_luma_ex(pl_nominal_luma_params(
                    .color = &target_color,
                    .metadata = PL_HDR_METADATA_HDR10,
                    .scaling = PL_HDR_NITS,
                    .out_max = &target_color.hdr.max_luma
                ));
            }
            
            // Set target contrast (affects min_luma)
            if (gopts->target_contrast > 0) {
                // min_luma will be computed from contrast in apply_target_contrast
                // For now, set to 0 to let libplacebo compute
                target_color.hdr.min_luma = 0.0;
            }
        } else {
            // No opts available, default to sRGB
            target_color = pl_color_space_srgb;
        }
    } else {
        // No opts_cache, default to sRGB
        target_color = pl_color_space_srgb;
    }
    
    // Debug: check source rotation if available
    int source_rotation = 0;
    if (frame && frame->current) {
        source_rotation = frame->current->params.rotate / 90;
        if (source_rotation != 0) {
            mp_msg(p->log, MSGL_WARN, "[gpu_next] Current frame rotation: %d (rotate=%d degrees)\n",
                   source_rotation, frame->current->params.rotate);
        }
    }
    
    // libmpv render API note:
    // The embedder (native/mpv_render_gl.mm) passes MPV_RENDER_PARAM_FLIP_Y=1 when
    // rendering into an OpenGL FBO. This makes the effective target coordinate
    // system Y-up. mpv's mp_get_src_dst_rects() produces dst in that coordinate
    // space, so we must apply dst with reversed Y for libplacebo.
    struct pl_frame target_frame = {
        .num_planes = 1,
        .planes[0] = { .texture = target_tex, .components = 4, .component_mapping = {0,1,2,3} },
        // Apply dst rect with reversed Y to match FLIP_Y=1
        .crop = { .x0 = p->current_dst.x0, .y0 = p->current_dst.y1,
                  .x1 = p->current_dst.x1, .y1 = p->current_dst.y0 },
        .color = target_color,
        .repr = pl_color_repr_rgb,
        .rotation = 0,  // Target is always unrotated - source rotation is handled in map_frame
    };

    if (frame && frame->current && frame->frame_id > p->last_frame_id) {
        struct mp_image *mpi = mp_image_new_ref(frame->current);
        struct frame_priv *fp = talloc_zero(mpi, struct frame_priv);
        fp->p = p;
        mpi->priv = fp;

        pl_queue_push(p->queue, &(struct pl_source_frame) {
            .pts = mpi->pts,
            .frame_data = mpi,
            .map = map_frame,
            .unmap = unmap_frame,
            .discard = discard_frame,
        });

        p->last_frame_id = frame->frame_id;
    }

    double target_pts = (frame && frame->current) ? frame->current->pts : p->last_pts;
    p->last_pts = target_pts;

    struct pl_frame_mix queue_mix = {0};
    struct pl_queue_params qparams = *pl_queue_params(.pts = target_pts);

    pl_queue_update(p->queue, &queue_mix, &qparams);

    struct mp_image *representative_img = NULL;
    if (queue_mix.num_frames > 0 && queue_mix.frames) {
        representative_img = queue_mix.frames[0]->user_data;
    }

    uint64_t signatures[32];
    assert(queue_mix.num_frames < MP_ARRAY_SIZE(signatures));
    for (int i = 0; i < queue_mix.num_frames; i++) {
        signatures[i] = (uintptr_t)queue_mix.frames[i]->user_data;
    }
    struct pl_frame_mix mix = queue_mix;
    mix.signatures = signatures;

    update_overlays(p, p->osd_res, 0, PL_OVERLAY_COORDS_DST_FRAME,
                   &p->osd_state_storage, &target_frame, representative_img);

    mix.vsync_duration = 1.0f;

    // Build render params dynamically from mpv options
    // Reference: vo_gpu_next.c update_render_options() lines 2517-2540
    struct pl_render_params params = pl_render_default_params;
    struct pl_sigmoid_params sigmoid_params = pl_sigmoid_default_params;

    if (p->opts_cache) {
        const struct gl_video_opts *gopts = p->opts_cache->opts;
        if (gopts) {
            // Map scaler options from mpv config to libplacebo filters
            params.upscaler = map_scaler(p, gopts, SCALER_SCALE);
            params.downscaler = map_scaler(p, gopts, SCALER_DSCALE);
            params.plane_upscaler = map_scaler(p, gopts, SCALER_CSCALE);

            // Sigmoid upscaling
            if (gopts->sigmoid_upscaling) {
                sigmoid_params.center = gopts->sigmoid_center;
                sigmoid_params.slope = gopts->sigmoid_slope;
                params.sigmoid_params = &sigmoid_params;
            } else {
                params.sigmoid_params = NULL;
            }

            // Correct subpixel offsets (inverse of scaler_resizes_only)
            params.correct_subpixel_offsets = !gopts->scaler_resizes_only;

            // Skip anti-aliasing only if linear downscaling is disabled
            params.skip_anti_aliasing = !gopts->linear_downscaling;

            // Load user shaders (glsl-shaders option) — only re-parse on options change
            if (opts_changed)
                update_user_shaders(p, gopts);
            params.hooks = p->hooks;
            params.num_hooks = p->num_hooks;
        }
    }

    struct pl_color_adjustment color_adj;
    struct mp_csp_params cparams = MP_CSP_PARAMS_DEFAULTS;
    mp_csp_equalizer_state_get(p->video_eq, &cparams);

    color_adj.brightness = cparams.brightness;
    color_adj.contrast   = cparams.contrast;
    color_adj.hue        = cparams.hue;
    color_adj.saturation = cparams.saturation;
    color_adj.gamma      = cparams.gamma;

    params.color_adjustment = &color_adj;

    // Handle vertical flip (vflip) - same as vo_gpu_next.c line 1026-1031
    // iPhone videos and other content may have vflip metadata that needs to be applied
    static struct pl_distort_params distort_params;
    if (frame && frame->current && frame->current->params.vflip) {
        pl_matrix2x2 m = { .m = {{1, 0}, {0, -1}}, };
        distort_params.transform.mat = m;
        params.distort_params = &distort_params;
    } else {
        params.distort_params = NULL;
    }

    if (!pl_render_image_mix(p->renderer, &mix, &target_frame, &params)) {
        mp_msg(p->log, MSGL_ERR, "Rendering failed.\n");
    }
}

struct mp_image *pl_video_screenshot(struct pl_video *p, struct vo_frame *frame)
{
    if (!p || !p->gpu || !frame || !frame->current) {
        mp_msg(p->log, MSGL_WARN, "pl_video_screenshot: invalid arguments\n");
        return NULL;
    }

    struct mp_image *res = NULL;
    struct pl_frame source_frame = {0};
    pl_tex fbo = NULL;

    if (!upload_mp_image(p, &source_frame, frame->current)) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: failed to upload source image\n");
        return NULL;
    }

    pl_fmt fbo_fmt = pl_find_fmt(p->gpu, PL_FMT_UNORM, 4, 8, 8,
                                 PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_HOST_READABLE);
    if (!fbo_fmt) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: failed to find screenshot format\n");
        goto done;
    }

    int w = frame->current->w;
    int h = frame->current->h;
    fbo = pl_tex_create(p->gpu, pl_tex_params(
        .w = w,
        .h = h,
        .format = fbo_fmt,
        .renderable = true,
        .host_readable = true
    ));
    if (!fbo) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: failed to create temporary texture\n");
        goto done;
    }

    struct pl_frame target_frame = {
        .num_planes = 1,
        .planes[0] = {
            .texture = fbo,
            .components = 4,
            .component_mapping = {0, 1, 2, 3},
        },
        .color = pl_color_space_srgb,
        .repr = pl_color_repr_rgb,
    };

    struct mp_osd_res osd_res = {
        .w = frame->current->w,
        .h = frame->current->h,
        .display_par = 1.0, 
    };

    update_overlays(p, osd_res, 0, PL_OVERLAY_COORDS_DST_FRAME,
                    &p->osd_state_storage, &target_frame, frame->current);

    const struct pl_render_params params = pl_render_default_params;

    if (!pl_render_image(p->renderer, &source_frame, &target_frame, &params)) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: rendering failed\n");
        goto done;
    }

    res = mp_image_alloc(IMGFMT_RGBA, w, h);
    if (!res) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: failed to allocate mp_image\n");
        goto done;
    }

    bool ok = pl_tex_download(p->gpu, &(struct pl_tex_transfer_params){
        .tex = fbo,
        .ptr = res->planes[0],
        .row_pitch = res->stride[0],
    });

    if (!ok) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: texture download failed\n");
        talloc_free(res);
        res = NULL;
        goto done;
    }

done:
    if (fbo)
        pl_tex_destroy(p->gpu, &fbo);

    cleanup_pl_frame(p, &source_frame);

    return res;
}

void pl_video_reconfig(struct pl_video *p, const struct mp_image_params *params) {
    if (params)
        p->current_params = *params;
}

void pl_video_resize(struct pl_video *p, const struct mp_rect *dst, const struct mp_osd_res *osd) {
    if (dst)
        p->current_dst = *dst;
    if (osd)
        p->osd_res = *osd;
}

void pl_video_update_osd(struct pl_video *p, struct osd_state *osd) {
    p->current_osd_state = osd;
}

void pl_video_reset(struct pl_video *p) {
    if (!p || !p->renderer) return;
    pl_renderer_flush_cache(p->renderer);
    pl_queue_reset(p->queue);
    p->last_frame_id = 0;
    p->last_pts = 0;
}

bool pl_video_check_format(struct pl_video *p, int imgfmt) {
    return true;
}
