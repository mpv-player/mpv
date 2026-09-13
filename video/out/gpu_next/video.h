#pragma once

#include "stdbool.h"         // for bool
#include <libplacebo/gpu.h>  // for pl_gpu, pl_tex

// Forward declarations
struct mp_image_params;
struct mp_log;
struct mp_osd_res;
struct mp_rect;
struct mpv_global;
struct osd_state;
struct ra_next;
struct vo_frame;

/**
 * Initializes the rendering engine.
 */
struct pl_video *pl_video_init(struct mpv_global *global, struct mp_log *log, struct ra_next *ra);

/**
 * Shuts down and destroys the rendering engine.
 */
void pl_video_uninit(struct pl_video **p_ptr);

/**
 * Synchronously renders a video frame to a display target using libplacebo.
 */
void pl_video_render(struct pl_video *p, struct vo_frame *frame, pl_tex target_tex);

/**
 * Synchronously renders `frame` into an sRGB temporary and returns a newly
 * allocated mp_image (RGBA) with the result. Caller must free the returned
 * mp_image with talloc_free() or mp_image_free if available.
 *
 * Returns NULL on failure.
 */
struct mp_image *pl_video_screenshot(struct pl_video *p, struct vo_frame *frame);

/**
 * Informs the engine that the video parameters have changed.
 */
void pl_video_reconfig(struct pl_video *p, const struct mp_image_params *params);

/**
 * Informs the engine that the output viewport has been resized.
 */
// void pl_video_resize(struct pl_video *p, const struct mp_rect *dst);
void pl_video_resize(struct pl_video *p, const struct mp_rect *dst, const struct mp_osd_res *osd);
/**
 * Provides the engine with the current On-Screen Display state.
 */
void pl_video_update_osd(struct pl_video *p, struct osd_state *osd);

/**
 * Informs the engine that it should flush libplacebo's internal caches.
 */
void pl_video_reset(struct pl_video *p);

/**
 * Asks the engine if a specific image format is supported.
 */
bool pl_video_check_format(struct pl_video *p, int imgfmt);
