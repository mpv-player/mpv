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

#ifndef MPV_VAAPI_H
#define MPV_VAAPI_H

#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>
#include <va/va.h>

#include "mp_image.h"
#include "hwdec.h"

struct mp_vaapi_ctx {
    struct mp_hwdec_ctx hwctx;
    struct mp_log *log;
    VADisplay display;
    struct AVBufferRef *av_device_ref; // AVVAAPIDeviceContext*
    // Internal, for va_create_standalone()
    void *native_ctx;
    void (*destroy_native_ctx)(void *native_ctx);
};

#define CHECK_VA_STATUS_LEVEL(ctx, msg, level) \
    (status == VA_STATUS_SUCCESS ? true \
        : (MP_MSG(ctx, level, "%s failed (%s)\n", msg, vaErrorStr(status)), false))

#define CHECK_VA_STATUS(ctx, msg) \
    CHECK_VA_STATUS_LEVEL(ctx, msg, MSGL_ERR)

int                      va_get_colorspace_flag(enum mp_csp csp);

struct mp_vaapi_ctx *    va_initialize(VADisplay *display, struct mp_log *plog, bool probing);
void                     va_destroy(struct mp_vaapi_ctx *ctx);

VASurfaceID va_surface_id(struct mp_image *mpi);

bool va_guess_if_emulated(struct mp_vaapi_ctx *ctx);

#endif
