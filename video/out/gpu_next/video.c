#include "video.h"
#include <libplacebo/utils/frame_queue.h>  // for pl_source_frame, pl_queue_...
#include <stddef.h>                        // for NULL
#include <stdint.h>                        // for uint64_t, uint32_t, uintptr_t
#include "assert.h"                        // for assert
#include "common/common.h"                 // for mp_rect, MPMAX, MP_ARRAY_SIZE
#include "common/msg.h"                    // for mp_msg, MSGL_ERR, MSGL_WARN
#include "libplacebo/colorspace.h"         // for pl_color_adjustment, pl_co...
#include "libplacebo/filters.h"            // for pl_filter_nearest
#include "libplacebo/gpu.h"                // for pl_tex_params, pl_tex_t
#include "libplacebo/renderer.h"           // for pl_frame_mix, pl_frame
#include "sub/draw_bmp.h"                  // for mp_draw_sub_formats
#include "sub/osd.h"                       // for sub_bitmap, sub_bitmaps
#include "ta/ta_talloc.h"                  // for talloc_free, talloc_zero
#include "video/csputils.h"                // for mp_csp_params, mp_csp_equa...
#include "video/img_format.h"              // for mp_imgfmt
#include "video/mp_image.h"                // for mp_image, mp_image_params
#include "video/out/gpu_next/ra.h"         // for ra_next_find_fmt, ra_next_...
#include "video/out/vo.h"                  // for vo_frame

// Forward declarations
struct mp_log;
struct mpv_global;
struct osd_state;

/**
 * @brief Holds GPU resources for a single piece of the On-Screen Display (OSD).
 *
 * This struct contains a GPU texture (`pl_tex`) and an array of `pl_overlay_part`
 * which define how different parts of the texture should be drawn on the screen.
 * This is used for rendering subtitles and other OSD elements.
 */
struct pl_video_osd_entry {
    pl_tex tex;                     // The GPU texture containing the bitmap for this OSD part.
    struct pl_overlay_part *parts;  // Array of parts describing how to render the texture.
    int num_parts;                  // The number of parts in the array.
};

/**
 * @brief Manages the state for all OSD elements.
 *
 * This struct holds arrays for all possible OSD parts and the corresponding
 * libplacebo overlay structures that will be passed to the renderer.
 */
struct pl_video_osd_state {
    struct pl_video_osd_entry entries[MAX_OSD_PARTS]; // Storage for individual OSD parts.
    struct pl_overlay overlays[MAX_OSD_PARTS];      // The final overlays to be rendered.
};

/**
 * @brief Main structure managing synchronous libplacebo video rendering.
 *
 * This struct encapsulates all state needed for rendering video frames
 * and OSD elements using libplacebo. It includes the frame queue, current
 * video parameters, OSD resources, and color adjustment state.
 */
struct pl_video {
    struct mp_log *log;
    struct ra_next *ra;    // The libplacebo rendering abstraction
    ra_queue queue;        // The frame queue for handling video frames and interpolation.
    uint64_t last_frame_id;// To avoid pushing duplicate frames into the queue.
    double last_pts;       // Last presentation timestamp we rendered at, for redraws.

    // Render State
    struct mp_image_params current_params; // Current video parameters (resolution, colorspace, etc.).
    struct mp_rect current_dst;            // The current destination rectangle on the target surface.
    struct osd_state *current_osd_state; // Pointer to the core's logical OSD state.

    // OSD rendering resources
    struct mp_osd_res osd_res;                  // OSD resolution and aspect ratio information.
    struct pl_video_osd_state osd_state_storage; // Storage for our GPU resources.
    pl_fmt osd_fmt[SUBBITMAP_COUNT];             // Cached libplacebo formats for different OSD bitmap types.
    pl_tex *sub_tex; // Texture pool for OSD textures.
    int num_sub_tex; // The number of textures in the pool.

    // Color adjustment state
    struct mp_csp_equalizer_state *video_eq; // Manages brightness, contrast, hue, etc.
};

/**
 * @brief Private data attached to each mp_image pushed into the queue.
 * This allows the static callback functions to access the main pl_video state.
 */
struct frame_priv {
    struct pl_video *p; // A pointer back to the main pl_video engine struct.
};

/**
 * @brief Callback to map an mp_image to a pl_frame for rendering.
 *
 * This function is called by the queue when it needs to prepare a frame for rendering.
 * It handles uploading the image data from CPU memory to a GPU texture.
 * @param gpu The libplacebo GPU handle.
 * @param tex (unused) A pointer for an older API, not used here.
 * @param src The source frame from the queue, containing the mp_image.
 * @param frame The destination `pl_frame` to be populated with GPU texture info.
 * @return True on success, false on failure.
 */
static bool map_frame(pl_gpu gpu, pl_tex *tex, const struct pl_source_frame *src,
                      struct pl_frame *frame)
{
    struct mp_image *mpi = src->frame_data;
    struct frame_priv *fp = mpi->priv;
    struct pl_video *p = fp->p;

    // Use the RA helper to upload the mp_image data to a new set of textures
    // and populate the pl_frame struct with the result.
    if (!ra_upload_mp_image(p->ra, frame, mpi)) {
        talloc_free(mpi); // Clean up the mp_image reference on failure
        return false;
    }

    // Store a pointer back to the original mp_image. This is used to get a unique
    // signature for the frame and to access metadata (like colorspace) later.
    frame->user_data = mpi;
    return true;
}

/**
 * @brief Callback to unmap a pl_frame after it has been rendered.
 *
 * This function is called by the queue to free the GPU resources associated
 * with a frame that is no longer needed for rendering.
 * @param gpu The libplacebo GPU handle.
 * @param frame The `pl_frame` containing the GPU textures to be destroyed.
 * @param src The source frame from the queue.
 */
static void unmap_frame(pl_gpu gpu, struct pl_frame *frame,
                        const struct pl_source_frame *src)
{
    struct mp_image *mpi = src->frame_data;
    struct frame_priv *fp = mpi->priv;
    struct pl_video *p = fp->p;

    // Use the RA helper to destroy the GPU textures associated with the frame.
    ra_cleanup_pl_frame(p->ra, frame);
    // Free the mp_image reference itself.
    talloc_free(mpi);
}

/**
 * @brief Callback to discard a frame that was pushed into the queue but never rendered.
 *
 * This is called for frames that are dropped (e.g., due to performance issues).
 * It frees the CPU-side mp_image without touching the GPU.
 * @param src The source frame from the queue to be discarded.
 */
static void discard_frame(const struct pl_source_frame *src)
{
    // We only need to free the mp_image reference, as no GPU resources were created.
    struct mp_image *mpi = src->frame_data;
    talloc_free(mpi);
}

/**
 * @brief Initializes the synchronous libplacebo rendering engine.
 * @param global The mpv global context.
 * @param log The logging context.
 * @param ra The rendering abstraction context.
 * @return A pointer to the newly created pl_video engine, or NULL on failure.
 */
struct pl_video *pl_video_init(struct mpv_global *global, struct mp_log *log, struct ra_next *ra) {
    struct pl_video *p = talloc_zero(NULL, struct pl_video);
    p->log = log;
    p->ra = ra;
    p->queue = ra_next_queue_create(ra);

    // Pre-find the texture formats we'll need for OSD bitmaps for efficiency.
    p->osd_fmt[SUBBITMAP_LIBASS] = ra_next_find_fmt(p->ra, PL_FMT_UNORM, 1, 8, 8, 0);
    p->osd_fmt[SUBBITMAP_BGRA]   = ra_next_find_fmt(p->ra, PL_FMT_UNORM, 4, 8, 8, 0);

    // Create the state object that tracks brightness, contrast, etc.
    p->video_eq = mp_csp_equalizer_create(p, global);

    return p;
}

/**
 * @brief Shuts down and destroys the rendering engine and its resources.
 * @param p_ptr A pointer to the pl_video engine pointer to be freed.
 */
void pl_video_uninit(struct pl_video **p_ptr) {
    struct pl_video *p = *p_ptr;
    if (!p) return;

    ra_next_queue_destroy(&p->queue);

    // Clean up all allocated OSD GPU resources
    for (int i = 0; i < MP_ARRAY_SIZE(p->osd_state_storage.entries); i++) {
        struct pl_video_osd_entry *entry = &p->osd_state_storage.entries[i];
        ra_next_tex_destroy(p->ra, &entry->tex);
        talloc_free(entry->parts);
    }
    for (int i = 0; i < p->num_sub_tex; i++) {
        ra_next_tex_destroy(p->ra, &p->sub_tex[i]);
    }
    talloc_free(p->sub_tex);

    talloc_free(p);
    *p_ptr = NULL;
}

/**
 * @brief Updates the OSD overlays for the current video frame.
 * @param p The pl_video context.
 * @param res The OSD resolution.
 * @param flags The OSD update flags.
 * @param coords The overlay coordinates.
 * @param state The current OSD state.
 * @param frame The video frame to update.
 * @param src The source image for the frame.
 */
static void update_overlays(struct pl_video *p, struct mp_osd_res res,
                            int flags, enum pl_overlay_coords coords,
                            struct pl_video_osd_state *state, struct pl_frame *frame,
                            struct mp_image *src)
{
    frame->num_overlays = 0;
    if (!p->current_osd_state)
        return;

    // Render the logical OSD state into a list of bitmaps.
    double pts = src ? src->pts : 0;
    struct sub_bitmap_list *subs = osd_render(p->current_osd_state, res, pts, flags, mp_draw_sub_formats);
    if (!subs) return;

    frame->overlays = state->overlays;

    // Iterate through each bitmap and convert it into a libplacebo overlay.
    for (int n = 0; n < subs->num_items; n++) {
        const struct sub_bitmaps *item = subs->items[n];
        if (!item->num_parts || !item->packed)
            continue;

        struct pl_video_osd_entry *entry = &state->entries[item->render_index];
        pl_fmt tex_fmt = p->osd_fmt[item->format];

        // Reuse a texture from the pool if available.
        if (!entry->tex)
            MP_TARRAY_POP(p->sub_tex, p->num_sub_tex, &entry->tex);

        // Recreate the texture if its size needs to change.
        bool ok = ra_next_tex_recreate(p->ra, &entry->tex, &(struct pl_tex_params) {
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

        // Upload the new bitmap data to the GPU texture.
        ok = ra_next_tex_upload(p->ra, &(struct pl_tex_transfer_params) {
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

        // Convert each sub-bitmap part into a pl_overlay_part.
        for (int i = 0; i < item->num_parts; i++) {
            const struct sub_bitmap *b = &item->parts[i];
            if (b->dw == 0 || b->dh == 0)
                continue;
            uint32_t c = b->libass.color;
            struct pl_overlay_part part = {
                .src = { b->src_x, b->src_y, b->src_x + b->w, b->src_y + b->h },
                .dst = { b->x, b->y, b->x + b->dw, b->y + b->dh },
                .color = {
                    (c >> 24) / 255.0f,
                    ((c >> 16) & 0xFF) / 255.0f,
                    ((c >> 8) & 0xFF) / 255.0f,
                    (255 - (c & 0xFF)) / 255.0f,
                }
            };
            entry->parts[entry->num_parts++] = part;
        }

        // Create the final pl_overlay structure for rendering.
        struct pl_overlay *ol = &state->overlays[frame->num_overlays++];
        *ol = (struct pl_overlay) {
            .tex = entry->tex,
            .parts = entry->parts,
            .num_parts = entry->num_parts,
            .color = { .primaries = PL_COLOR_PRIM_BT_709, .transfer = PL_COLOR_TRC_SRGB },
            .coords = coords,
        };

        // Set blending modes based on the OSD bitmap format.
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

/**
 * @brief Main rendering function for a single video frame.
 * @param p The pl_video engine context.
 * @param frame The mpv frame to render, containing the current image.
 * @param target_tex The destination GPU texture to render to.
 */
void pl_video_render(struct pl_video *p, struct vo_frame *frame, pl_tex target_tex)
{
    // Describe the target surface for libplacebo.
    struct pl_frame target_frame = {
        .num_planes = 1,
        .planes[0] = { .texture = target_tex, .components = 4, .component_mapping = {0,1,2,3} },
        .crop = { .x0 = p->current_dst.x0, .y0 = p->current_dst.y0, .x1 = p->current_dst.x1, .y1 = p->current_dst.y1 },
        .color = pl_color_space_srgb,
        .repr = pl_color_repr_rgb,
    };

    // The libmpv VO provides one new frame at a time in frame->current.
    // We check the frame_id to avoid pushing duplicates.
    if (frame && frame->current && frame->frame_id > p->last_frame_id) {
        struct mp_image *mpi = mp_image_new_ref(frame->current);
        // Attach our private data to the image for the callbacks.
        struct frame_priv *fp = talloc_zero(mpi, struct frame_priv);
        fp->p = p;
        mpi->priv = fp;

        // Push the frame into the queue with its callbacks.
        ra_next_queue_push(p->queue, &(struct pl_source_frame) {
            .pts = mpi->pts,
            .frame_data = mpi,
            .map = map_frame,
            .unmap = unmap_frame,
            .discard = discard_frame,
        });

        p->last_frame_id = frame->frame_id;
    }

    // If this is a redraw request, frame->current will be NULL. In that case,
    // we reuse the last known PTS to query the queue for the correct frame.
    double target_pts = (frame && frame->current) ? frame->current->pts : p->last_pts;
    p->last_pts = target_pts;

    struct pl_frame_mix queue_mix = {0};
    struct pl_queue_params qparams = *pl_queue_params(.pts = target_pts);

    ra_next_queue_update(p->queue, &queue_mix, &qparams);

    // To render OSD, we need a representative source frame to get color space info.
    // The first frame in the mix is a perfect candidate.
    struct mp_image *representative_img = NULL;
    if (queue_mix.num_frames > 0 && queue_mix.frames) {
        representative_img = queue_mix.frames[0]->user_data;
    }

    // Manually build the final mix for the renderer, including the signatures.
    // We need a local array to hold the signature data. 32 is a safe upper bound.
    uint64_t signatures[32];
    assert(queue_mix.num_frames < MP_ARRAY_SIZE(signatures));
    for (int i = 0; i < queue_mix.num_frames; i++) {
        // Use the mp_image pointer as a unique signature for caching.
        signatures[i] = (uintptr_t)queue_mix.frames[i]->user_data;
    }
    struct pl_frame_mix mix = queue_mix;
    mix.signatures = signatures;

    // Generate and attach OSD overlays to the target frame. If mix.num_frames is 0,
    // representative_img will be NULL, and update_overlays will correctly render
    // OSD against a black background.
    update_overlays(p, p->osd_res, 0, PL_OVERLAY_COORDS_DST_FRAME,
                   &p->osd_state_storage, &target_frame, representative_img);

    // For a simple, non-interpolating backend, we can assume the display
    // frame's duration is equivalent to one source frame (1.0 in normalized time).
    mix.vsync_duration = 1.0f;

    // Prepare the rendering parameters for libplacebo
    struct pl_render_params params = {
        .upscaler = &pl_filter_nearest,
        .downscaler = &pl_filter_nearest,
    };

    // Declare a local struct to hold the color adjustment values.
    struct pl_color_adjustment color_adj;

    // Query the current brightness/contrast/etc values from the equalizer
    struct mp_csp_params cparams = MP_CSP_PARAMS_DEFAULTS;
    mp_csp_equalizer_state_get(p->video_eq, &cparams);

    // Fill our local struct with the values.
    color_adj.brightness = cparams.brightness;
    color_adj.contrast   = cparams.contrast;
    color_adj.hue        = cparams.hue;
    color_adj.saturation = cparams.saturation;
    color_adj.gamma      = cparams.gamma;

    // Point the render params' pointer to our local struct.
    params.color_adjustment = &color_adj;

    // Render the mix. libplacebo handles the empty mix case (no video) correctly.
    if (!ra_next_render_image_mix(p->ra, &mix, &target_frame, &params)) {
        mp_msg(p->log, MSGL_ERR, "Rendering failed.\n");
    }
}

 /**
  * @brief Takes a screenshot of the current video frame.
  * @param ctx The render_backend context.
  * @param frame The video frame to capture.
  * @param args The screenshot arguments.
  */
struct mp_image *pl_video_screenshot(struct pl_video *p, struct vo_frame *frame)
{
    if (!p || !p->ra || !frame || !frame->current) {
        mp_msg(p->log, MSGL_WARN, "pl_video_screenshot: invalid arguments\n");
        return NULL;
    }

    struct mp_image *res = NULL;
    struct pl_frame source_frame = {0};
    pl_tex fbo = NULL;

    // Upload the mp_image to a pl_frame via the RA helper.
    if (!ra_upload_mp_image(p->ra, &source_frame, frame->current)) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: failed to upload source image\n");
        return NULL;
    }

    // Find an 8-bit RGBA renderable + host-readable format.
    pl_fmt fbo_fmt = ra_next_find_fmt(p->ra, PL_FMT_UNORM, 4, 8, 8,
                                 PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_HOST_READABLE);
    if (!fbo_fmt) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: failed to find screenshot format\n");
        goto done;
    }

    // Create a temporary renderable, host-readable texture sized to the source.
    int w = frame->current->w;
    int h = frame->current->h;
    fbo = ra_next_tex_create(p->ra, pl_tex_params(
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

    // Describe the target as an sRGB SDR frame (so libplacebo performs tone-mapping).
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

    // Define the OSD canvas size to match the video frame for the screenshot.
    struct mp_osd_res osd_res = {
        .w = frame->current->w,
        .h = frame->current->h,
        .display_par = 1.0, // Screenshots have square pixels
    };

    // Generate and attach OSD/subtitle overlays to the screenshot's target frame.
    update_overlays(p, osd_res, 0, PL_OVERLAY_COORDS_DST_FRAME,
                    &p->osd_state_storage, &target_frame, frame->current);

    const struct pl_render_params params = {
        .upscaler = &pl_filter_nearest,
        .downscaler = &pl_filter_nearest,
    };

    if (!ra_next_render_image(p->ra, &source_frame, &target_frame, &params)) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: rendering failed\n");
        goto done;
    }

    // Allocate an mp_image for RGBA result and download the texture into it.
    res = mp_image_alloc(IMGFMT_RGBA, w, h);
    if (!res) {
        mp_msg(p->log, MSGL_ERR, "pl_video_screenshot: failed to allocate mp_image\n");
        goto done;
    }

    bool ok = ra_next_tex_download(p->ra, &(struct pl_tex_transfer_params){
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
        ra_next_tex_destroy(p->ra, &fbo);

    ra_cleanup_pl_frame(p->ra, &source_frame);

    return res;
}

/**
 * @brief Informs the engine that the video parameters have changed.
 * @param p The pl_video engine context.
 * @param params The new image parameters.
 */
void pl_video_reconfig(struct pl_video *p, const struct mp_image_params *params) {
    if (params)
        p->current_params = *params;
}

/**
 * @brief Informs the engine that the output viewport has been resized.
 * @param p The pl_video engine context.
 * @param dst The new destination rectangle.
 * @param osd The new OSD resolution information.
 */
void pl_video_resize(struct pl_video *p, const struct mp_rect *dst, const struct mp_osd_res *osd) {
    if (dst)
        p->current_dst = *dst;
    if (osd)
        p->osd_res = *osd;
}

/**
 * @brief Provides the engine with the current On-Screen Display state.
 * @param p The pl_video engine context.
 * @param osd A pointer to the current OSD state.
 */
void pl_video_update_osd(struct pl_video *p, struct osd_state *osd) {
    p->current_osd_state = osd;
}

/**
 * @brief Informs the engine that it should flush internal caches.
 * @param p The pl_video engine context.
 */
void pl_video_reset(struct pl_video *p) {
    if (!p || !p->ra) return;
    ra_pl_reset(p->ra);
    ra_next_queue_reset(p->queue); // Also reset the frame queue.
    p->last_frame_id = 0;
    p->last_pts = 0;
}

/**
 * @brief Asks the engine if a specific image format is supported.
 * @param p The pl_video engine context.
 * @param imgfmt The image format to check.
 * @return True if the format is likely supported, false otherwise.
 */
bool pl_video_check_format(struct pl_video *p, int imgfmt) {
    // For simplicity, we assume libplacebo can handle it.
    // A more robust implementation might query libplacebo's capabilities.
    return true;
}
