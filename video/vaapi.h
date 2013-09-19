#ifndef MPV_VAAPI_H
#define MPV_VAAPI_H

#include <stdbool.h>
#include <inttypes.h>
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
#ifndef VA_FOURCC_NV12
#define VA_FOURCC_NV12 VA_FOURCC('N', 'V', '1', '2')
#endif

#define VA_STR_FOURCC(fcc) \
    (const char[]){(fcc), (fcc) >> 8u, (fcc) >> 16u, (fcc) >> 24u, 0}

#include "mpvcore/mp_msg.h"
#include "mp_image.h"

static inline bool check_va_status(VAStatus status, const char *msg)
{
    if (status != VA_STATUS_SUCCESS) {
        mp_msg(MSGT_VO, MSGL_ERR, "[vaapi] %s: %s\n", msg, vaErrorStr(status));
        return false;
    }
    return true;
}

static inline int get_va_colorspace_flag(enum mp_csp csp)
{
#if USE_VAAPI_COLORSPACE
    switch (csp) {
    case MP_CSP_BT_601:         return VA_SRC_BT601;
    case MP_CSP_BT_709:         return VA_SRC_BT709;
    case MP_CSP_SMPTE_240M:     return VA_SRC_SMPTE_240;
    }
#endif
    return 0;
}

struct mp_vaapi_ctx {
    VADisplay display;
    void *priv; // for VO
};

typedef struct va_surface_pool va_surface_pool_t;

typedef struct va_surface {
    VASurfaceID id;      // VA_INVALID_ID if unallocated
    int w, h, rt_format; // parameters of allocated image (0/0/-1 unallocated)

    struct va_surface_priv *p;
} va_surface_t;

VADisplay          va_display_ref(Display *x11);
void               va_display_unref(void);

enum mp_imgfmt     va_fourcc_to_imgfmt(uint32_t fourcc);
VAImageFormat *    va_image_format_from_imgfmt(int imgfmt);
VAImageFormat *    va_image_formats_available(void);
int                va_image_formats_available_num(void);
bool               va_image_map(VADisplay display, VAImage *image, mp_image_t *mpi);
bool               va_image_unmap(VADisplay display, VAImage *image);

// ctx can be NULL. pool can be shared by specifying the same ctx value.
va_surface_pool_t *va_surface_pool_ref(VADisplay display, int rt_format, void *ctx);
void               va_surface_pool_unref(va_surface_pool_t **pool);
void               va_surface_pool_clear(va_surface_pool_t *pool);
bool               va_surface_pool_reserve(va_surface_pool_t *pool, int count, int w, int h);
int                va_surface_pool_rt_format(const va_surface_pool_t *pool);
va_surface_t *     va_surface_pool_get_by_imgfmt(va_surface_pool_t *pool, int imgfmt, int w, int h);
va_surface_t *     va_surface_pool_get(va_surface_pool_t *pool, int w, int h);
mp_image_t *       va_surface_pool_get_wrapped(va_surface_pool_t *pool, int imgfmt, int w, int h);

void               va_surface_unref(va_surface_t **surface);
va_surface_t *     va_surface_in_mp_image(mp_image_t *mpi);
mp_image_t *       va_surface_wrap(va_surface_t *surface); // takes ownership
VASurfaceID        va_surface_id(const va_surface_t *surface);
VASurfaceID        va_surface_id_in_mp_image(const mp_image_t *mpi);
bool               va_surface_upload(va_surface_t *surface, const mp_image_t *mpi);
mp_image_t *       va_surface_download(const va_surface_t *surface);

#endif
