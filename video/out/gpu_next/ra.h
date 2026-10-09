#pragma once

#include <libplacebo/gpu.h>                // for pl_gpu, pl_tex, pl_fmt
#include <libplacebo/log.h>                // for pl_log
#include <libplacebo/renderer.h>           // for pl_renderer
#include <libplacebo/utils/frame_queue.h>  // for pl_queue
#include "stdbool.h"                       // for bool

/* Forward declarations from mpv */
struct mp_image;
struct mp_log;
struct vo;

/* Opaque handle for the frame queue, managed by the RA */
typedef pl_queue ra_queue;

/* Public RA handle exposed to higher layers (minimal surface). */
struct ra_next {
    pl_gpu gpu;
    struct mpv_global *global;
    struct mp_log *log;
};

/* Upload an mp_image into a pl_frame suitable for pl_render_image.
 * Caller must call ra_cleanup_pl_frame() to free any textures created.
 * Returns true on success, false on failure. */
bool ra_upload_mp_image(struct ra_next *ra, struct pl_frame *out_frame,
                        const struct mp_image *img);

/* Cleanup any textures/resources attached to a pl_frame created by upload. */
void ra_cleanup_pl_frame(struct ra_next *ra, struct pl_frame *frame);

/* --- New Texture Management Wrappers (prefixed to avoid conflicts) --- */
pl_tex ra_next_tex_create(struct ra_next *ra, const struct pl_tex_params *params);
void ra_next_tex_destroy(struct ra_next *ra, pl_tex *tex);
bool ra_next_tex_recreate(struct ra_next *ra, pl_tex *tex, const struct pl_tex_params *params);
bool ra_next_tex_upload(struct ra_next *ra, const struct pl_tex_transfer_params *params);
bool ra_next_tex_download(struct ra_next *ra, const struct pl_tex_transfer_params *params);

/* --- New Frame Queue Wrappers --- */
ra_queue ra_next_queue_create(struct ra_next *ra);
void ra_next_queue_destroy(ra_queue *queue);
void ra_next_queue_push(ra_queue queue, const struct pl_source_frame *frame);
void ra_next_queue_update(ra_queue queue, struct pl_frame_mix *mix, const struct pl_queue_params *params);
void ra_next_queue_reset(ra_queue queue);

/* --- New Rendering Wrappers --- */
bool ra_next_render_image_mix(struct ra_next *ra, const struct pl_frame_mix *mix,
                         struct pl_frame *target, const struct pl_render_params *params);
bool ra_next_render_image(struct ra_next *ra, const struct pl_frame *src,
                     struct pl_frame *target, const struct pl_render_params *params);

/* --- New Utility Wrappers --- */
pl_fmt ra_next_find_fmt(struct ra_next *ra, enum pl_fmt_type type, int num_comps,
                   int comp_bits, int alpha_bits, unsigned caps);


/* Get the pl_renderer associated with this RA (may be NULL). */
pl_renderer ra_get_renderer(struct ra_next *ra);

/* Get the raw pl_gpu (for pl_tex_create / pl_tex_download etc). */
pl_gpu ra_get_gpu(struct ra_next *ra);

/* Flush libplacebo internal caches (wrapper for pl_renderer_flush_cache). */
void ra_reset(struct ra_next *ra);

/* Create the pl-specific RA implementation. */
struct ra_next *ra_pl_create(pl_gpu gpu, struct mp_log *log, pl_log log_pl);

/* Destroys the pl-specific RA implementation. */
void ra_pl_destroy(struct ra_next **rap);

/* Optional helper: let VO set a vo pointer on RA implementation. */
void ra_pl_set_vo(struct ra_next *ra, struct vo *vo);

/* Return the pl_log associated with the RA (or NULL). */
pl_log ra_get_pl_log(struct ra_next *ra);

/* Reset the RA (flush caches etc). */
void ra_pl_reset(struct ra_next *ra);

/* Internal helper: upload an mp_image to a pl_frame (used by ra_upload_mp_image).
 * This is exposed for use in pl_video_screenshot. */
bool upload_mp_image_to_pl_frame(struct ra_next *ra, struct pl_frame *out_frame, const struct mp_image *img);

/* Internal helper: cleanup a pl_frame (used by ra_cleanup_pl_frame).
 * This is exposed for use in pl_video_screenshot. */
void ra_pl_cleanup_frame(struct ra_next *ra, struct pl_frame *frame);
