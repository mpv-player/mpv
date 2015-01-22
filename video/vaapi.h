#ifndef MPV_VAAPI_H
#define MPV_VAAPI_H

#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>
#include <va/va.h>
#include <va/va_x11.h>

/* Compatibility glue with VA-API >= 0.31 */
#if defined VA_CHECK_VERSION
#if VA_CHECK_VERSION(0,31,0)
#define vaPutImage2             vaPutImage
#define vaAssociateSubpicture2  vaAssociateSubpicture
#endif
#endif

/* Compatibility glue with VA-API >= 0.34 */
#if VA_CHECK_VERSION(0,34,0)
#include <va/va_compat.h>
#endif

/* Compatibility glue with upstream libva */
#ifndef VA_SDS_VERSION
#define VA_SDS_VERSION          0
#endif

/* Compatibility glue with VA-API >= 0.30 */
#ifndef VA_INVALID_ID
#define VA_INVALID_ID           0xffffffff
#endif
#ifndef VA_FOURCC
#define VA_FOURCC(ch0, ch1, ch2, ch3)           \
    ((uint32_t)(uint8_t)(ch0) |                 \
     ((uint32_t)(uint8_t)(ch1) << 8) |          \
     ((uint32_t)(uint8_t)(ch2) << 16) |         \
     ((uint32_t)(uint8_t)(ch3) << 24 ))
#endif
#if defined VA_SRC_BT601 && defined VA_SRC_BT709
# define USE_VAAPI_COLORSPACE 1
#else
# define USE_VAAPI_COLORSPACE 0
#endif

/* Compatibility glue with VA-API >= 0.31.1 */
#ifndef VA_SRC_SMPTE_240
#define VA_SRC_SMPTE_240        0x00000040
#endif
#if defined VA_FILTER_SCALING_MASK
# define USE_VAAPI_SCALING 1
#else
# define USE_VAAPI_SCALING 0
#endif

#ifndef VA_FOURCC_YV12
#define VA_FOURCC_YV12 0x32315659
#endif
#ifndef VA_FOURCC_IYUV
#define VA_FOURCC_IYUV 0x56555949
#endif
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

struct mp_vaapi_ctx     *va_initialize(VADisplay *display, struct mp_log *log);
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
