/* Copyright (C) 2018 the mpv developers
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MPV_CLIENT_API_RENDER_H_
#define MPV_CLIENT_API_RENDER_H_

#include "client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Overview
 * --------
 *
 * This API can be used to make mpv render using supported graphic APIs (such
 * as OpenGL). It can be used to handle video display.
 *
 * The renderer needs to be created with mpv_render_context_create() before
 * you start playback (or otherwise cause a VO to be created). Then (with most
 * backends) mpv_render_context_render() can be used to explicitly render the
 * current video frame. Use mpv_render_context_set_update_callback() to get
 * notified when there is a new frame to draw.
 *
 * Preferably rendering should be done in a separate thread. If you call
 * normal libmpv API functions on the renderer thread, deadlocks can result
 * (these are made non-fatal with timeouts, but user experience will obviously
 * suffer).
 *
 * You can output and embed video without this API by setting the mpv "wid"
 * option to a native window handle (see "Embedding the video window" section
 * in the client.h header). In general, using the render API is recommended,
 * because window embedding can cause various issues, especially with GUI
 * toolkits and certain platforms.
 *
 * Supported backends
 * ------------------
 *
 * OpenGL: via MPV_RENDER_API_TYPE_OPENGL, see render_gl.h header.
 *
 * Threading
 * ---------
 *
 * The mpv_render_* functions can be called from any thread, under the
 * following conditions:
 *  - only one of the mpv_render_* functions can be called at the same time
 *    (unless they belong to different mpv cores created by mpv_create())
 *  - never can be called from within the callbacks set with
 *    mpv_set_wakeup_callback() or mpv_render_context_set_update_callback()
 *  - if the OpenGL backend is used, for all functions the OpenGL context
 *    must be "current" in the calling thread, and it must be the same OpenGL
 *    context as the mpv_render_context was created with. Otherwise, undefined
 *    behavior will occur.
 *
 * Context and handle lifecycle
 * ----------------------------
 *
 * Video initialization will fail if the render context was not initialized yet
 * (with mpv_render_context_create()), or it will revert to a VO that creates
 * its own window.
 *
 * Calling mpv_render_context_free() while a VO is using the render context is
 * active will disable video.
 *
 * You must free the context with mpv_render_context_free() before the mpv core
 * is destroyed. If this doesn't happen, undefined behavior will result.
 */

/**
 * Opaque context, returned by mpv_render_context_create().
 */
typedef struct mpv_render_context mpv_render_context;

/**
 * Parameters for mpv_render_param (which is used in a few places such as
 * mpv_render_context_create().
 *
 * Also see mpv_render_param for conventions and how to use it.
 */
typedef enum mpv_render_param_type {
    /**
     * Not a valid value, but also used to terminate a params array. Its value
     * is always guaranteed to be 0 (even if the ABI changes in the future).
     */
    MPV_RENDER_PARAM_INVALID = 0,
    /**
     * The render API to use. Valid for mpv_render_context_create().
     *
     * Type: char*
     *
     * Defined APIs:
     *
     *   MPV_RENDER_API_TYPE_OPENGL:
     *      OpenGL desktop 2.1 or later (preferably core profile compatible to
     *      OpenGL 3.2), or OpenGLES 2.0 or later.
     *      Providing MPV_RENDER_PARAM_OPENGL_INIT_PARAMS is required.
     *      It is expected that an OpenGL context is valid and "current" when
     *      calling mpv_render_* functions (unless specified otherwise). It
     *      must be the same context for the same mpv_render_context.
     */
    MPV_RENDER_PARAM_API_TYPE = 1,
    /**
     * Required parameters for initializing the OpenGL renderer. Valid for
     * mpv_render_context_create().
     * Type: mpv_opengl_init_params*
     */
    MPV_RENDER_PARAM_OPENGL_INIT_PARAMS = 2,
    /**
     * Describes a GL render target. Valid for mpv_render_context_render().
     * Type: mpv_opengl_fbo*
     */
    MPV_RENDER_PARAM_OPENGL_FBO = 3,
    /**
     * Control flipped rendering. Valid for mpv_render_context_render().
     * Type: int*
     * If the value is set to 0, render normally. Otherwise, render it flipped,
     * which is needed e.g. when rendering to an OpenGL default framebuffer
     * (which has a flipped coordinate system).
     */
    MPV_RENDER_PARAM_FLIP_Y = 4,
    /**
     * Control surface depth. Valid for mpv_render_context_render().
     * Type: int*
     * This implies the depth of the surface passed to the render function in
     * bits per channel. If omitted or set to 0, the renderer will assume 8.
     * Typically used to control dithering.
     */
    MPV_RENDER_PARAM_DEPTH = 5,
    /**
     * ICC profile blob. Valid for mpv_render_context_set_parameter().
     * Type: mpv_byte_array*
     * Set an ICC profile for use with the "icc-profile-auto" option. (If the
     * option is not enabled, the ICC data will not be used.)
     */
    MPV_RENDER_PARAM_ICC_PROFILE = 6,
    /**
     * Ambient light in lux. Valid for mpv_render_context_set_parameter().
     * Type: int*
     * This can be used for automatic gamma correction.
     */
    MPV_RENDER_PARAM_AMBIENT_LIGHT = 7,
} mpv_render_param_type;

/**
 * Predefined values for MPV_RENDER_PARAM_API_TYPE.
 */
#define MPV_RENDER_API_TYPE_OPENGL "opengl"

/**
 * Used to pass arbitrary parameters to some mpv_render_* functions. The
 * meaning of the data parameter is determined by the type, and each
 * MPV_RENDER_PARAM_* documents what type the value must point to.
 *
 * Each value documents the required data type as the pointer you cast to
 * void* and set on mpv_render_param.data. For example, if MPV_RENDER_PARAM_FOO
 * documents the type as Something* , then the code should look like this:
 *
 *   Something foo = {...};
 *   mpv_render_param param;
 *   param.type = MPV_RENDER_PARAM_FOO;
 *   param.data = & foo;
 *
 * Normally, the data field points to exactly 1 object. If the type is char*,
 * it points to a 0-terminated string.
 *
 * In all cases (unless documented otherwise) the pointers need to remain
 * valid during the call only. Unless otherwise documented, the API functions
 * will not write to the params array or any data pointed to it.
 *
 * As a convention, parameter arrays are always terminated by type==0. There
 * is no specific order of the parameters required. The order of fields is
 * guaranteed (even after ABI changes).
 */
typedef struct mpv_render_param {
    enum mpv_render_param_type type;
    void *data;
} mpv_render_param;

/**
 * Initialize the renderer state. Depending on the backend used, this will
 * access the underlying GPU API and initialize its own objects.
 *
 * You must free the context with mpv_render_context_free(). Not doing so before
 * the mpv core is destroyed may result in memory leaks or crashes.
 *
 * Currently, only at most 1 context can exists per mpv core (it represents the
 * main video output).
 *
 * @param res set to the context (on success) or NULL (on failure). The value
 *            is never read and always overwritten.
 * @param mpv handle used to get the core (the mpv_render_context won't depend
 *            on this specific handle, only the core referenced by it)
 * @param params an array of parameters, terminated by type==0. It's left
 *               unspecified what happens with unknown parameters. At least
 *               MPV_RENDER_PARAM_API_TYPE is required, and most backends will
 *               require another backend-specific parameter.
 * @return error code, including but not limited to:
 *      MPV_ERROR_UNSUPPORTED: the OpenGL version is not supported
 *                             (or required extensions are missing)
 *      MPV_ERROR_NOT_IMPLEMENTED: an unknown API type was provided, or
 *                                 support for the requested API was not
 *                                 built in the used libmpv binary.
 *      MPV_ERROR_INVALID_PARAMETER: at least one of the provided parameters was
 *                                   not valid.
 */
int mpv_render_context_create(mpv_render_context **res, mpv_handle *mpv,
                              mpv_render_param *params);

/**
 * Attempt to change a single parameter. Not all backends and parameter types
 * support all kinds of changes.
 *
 * @param ctx a valid render context
 * @param param the parameter type and data that should be set
 * @return error code. If a parameter could actually be changed, this returns
 *         success, otherwise an error code depending on the parameter type
 *         and situation.
 */
int mpv_render_context_set_parameter(mpv_render_context *ctx,
                                     mpv_render_param param);

typedef void (*mpv_render_update_fn)(void *cb_ctx);

/**
 * Set the callback that notifies you when a new video frame is available, or
 * if the video display configuration somehow changed and requires a redraw.
 * Similar to mpv_set_wakeup_callback(), you must not call any mpv API from
 * the callback, and all the other listed restrictions apply (such as not
 * exiting the callback by throwing exceptions).
 *
 * This can be called from any thread, except from an update callback. In case
 * of the OpenGL backend, no OpenGL state or API is accessed.
 *
 * @param callback callback(callback_ctx) is called if the frame should be
 *                 redrawn
 * @param callback_ctx opaque argument to the callback
 */
void mpv_render_context_set_update_callback(mpv_render_context *ctx,
                                            mpv_render_update_fn callback,
                                            void *callback_ctx);

/**
 * Render video.
 *
 * Typically renders the video to a target surface provided via mpv_render_param
 * (the details depend on the backend in use). Options like "panscan" are
 * applied to determine which part of the video should be visible and how the
 * video should be scaled. You can change these options at runtime by using the
 * mpv property API.
 *
 * The renderer will reconfigure itself every time the target surface
 * configuration (such as size) is changed.
 *
 * This function implicitly pulls a video frame from the internal queue and
 * renders it. If no new frame is available, the previous frame is redrawn.
 * The update callback set with mpv_render_context_set_update_callback()
 * notifies you when a new frame was added. The details potentially depend on
 * the backends and the provided parameters.
 *
 * Generally, libmpv will invoke your update callback some time before the video
 * frame should be shown, and then lets this function block until the supposed
 * display time. This will limit your rendering to video FPS. You can prevent
 * this by setting the "video-timing-offset" global option to 0. (This applies
 * only to "audio" video sync mode.)
 *
 * @param ctx a valid render context
 * @param params an array of parameters, terminated by type==0. Which parameters
 *               are required depends on the backend. It's left unspecified what
 *               happens with unknown parameters.
 * @return error code
 */
int mpv_render_context_render(mpv_render_context *ctx, mpv_render_param *params);

/**
 * Tell the renderer that a frame was flipped at the given time. This is
 * optional, but can help the player to achieve better timing.
 *
 * Note that calling this at least once informs libmpv that you will use this
 * function. If you use it inconsistently, expect bad video playback.
 *
 * If this is called while no video is initialized, it is ignored.
 *
 * @param ctx a valid render context
 */
void mpv_render_context_report_swap(mpv_render_context *ctx);

/**
 * Destroy the mpv renderer state.
 *
 * If video is still active (e.g. a file playing), video will be disabled
 * forcefully.
 *
 * @param ctx a valid render context. After this function returns, this is not
 *            a valid pointer anymore. NULL is also allowed and does nothing.
 */
void mpv_render_context_free(mpv_render_context *ctx);

#ifdef __cplusplus
}
#endif

#endif
