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

#ifndef MPV_VAAPI_H
#define MPV_VAAPI_H

#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>
#include <va/va.h>
#include <va/va_x11.h>

#ifndef VA_FOURCC_I420
#define VA_FOURCC_I420 VA_FOURCC('I', '4', '2', '0')
#endif
#ifndef VA_FOURCC_RGBX
#define VA_FOURCC_RGBX 0x58424752
#endif
#ifndef VA_FOURCC_BGRX
#define VA_FOURCC_BGRX 0x58524742
#endif

#define VA_STR_FOURCC(fcc) \
    (const char[]){(fcc), (fcc) >> 8u, (fcc) >> 16u, (fcc) >> 24u, 0}

#include "mp_image.h"
#include "hwdec.h"

struct mp_image_pool;
struct mp_log;

struct mp_vaapi_ctx {
    struct mp_hwdec_ctx hwctx;
    struct mp_log *log;
    VADisplay display;
    struct va_image_formats *image_formats;
    pthread_mutex_t lock;
};

bool check_va_status(struct mp_log *log, VAStatus status, const char *msg);

#define CHECK_VA_STATUS(ctx, msg) check_va_status((ctx)->log, status, msg)

#define va_lock(ctx)     pthread_mutex_lock(&(ctx)->lock)
#define va_unlock(ctx)   pthread_mutex_unlock(&(ctx)->lock)

int                      va_get_colorspace_flag(enum mp_csp csp);

struct mp_vaapi_ctx *    va_initialize(VADisplay *display, struct mp_log *plog, bool probing);
void                     va_destroy(struct mp_vaapi_ctx *ctx);

enum mp_imgfmt           va_fourcc_to_imgfmt(uint32_t fourcc);
uint32_t                 va_fourcc_from_imgfmt(int imgfmt);
VAImageFormat *          va_image_format_from_imgfmt(struct mp_vaapi_ctx *ctx, int imgfmt);
bool                     va_image_map(struct mp_vaapi_ctx *ctx, VAImage *image, struct mp_image *mpi);
bool                     va_image_unmap(struct mp_vaapi_ctx *ctx, VAImage *image);

void va_pool_set_allocator(struct mp_image_pool *pool, struct mp_vaapi_ctx *ctx,
                           int rt_format);

VASurfaceID va_surface_id(struct mp_image *mpi);
int va_surface_rt_format(struct mp_image *mpi);
struct mp_image *va_surface_download(struct mp_image *src,
                                     struct mp_image_pool *pool);

int va_surface_alloc_imgfmt(struct mp_image *img, int imgfmt);
int va_surface_upload(struct mp_image *va_dst, struct mp_image *sw_src);

bool va_guess_if_emulated(struct mp_vaapi_ctx *ctx);

#endif
