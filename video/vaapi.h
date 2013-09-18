#ifndef MPV_VAAPI_H
#define MPV_VAAPI_H

#include <stdbool.h>
#include <inttypes.h>

#include <va/va.h>

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

#include "mpvcore/mp_msg.h"
#include "csputils.h"

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
    int rt_format;
    struct mp_image *(*get_surface)(struct mp_vaapi_ctx *ctx, bool for_dec,
                                    int mp_format, int w, int h);
    void (*flush)(struct mp_vaapi_ctx *ctx);
    void *priv; // for VO
};

#endif
