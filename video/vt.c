#include <CoreVideo/CoreVideo.h>

#include "video/decode/lavc.h"

#include "mp_image.h"
#include "mp_image_pool.h"
#include "vt.h"

int mp_imgfmt_from_cvpixelformat(uint32_t cvpixfmt)
{
    switch (cvpixfmt) {
    case kCVPixelFormatType_420YpCbCr8Planar:               return IMGFMT_420P;
    case kCVPixelFormatType_422YpCbCr8:                     return IMGFMT_UYVY;
    case kCVPixelFormatType_32BGRA:                         return IMGFMT_RGB0;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:   return IMGFMT_NV12;
    }
    return 0;
}

// (ctx is unused - it's for compatibility with mp_hwdec_ctx.download_image())
struct mp_image *mp_vt_download_image(struct mp_hwdec_ctx *ctx,
                                      struct mp_image *hw_image,
                                      struct mp_image_pool *swpool)
{
    if (hw_image->imgfmt != IMGFMT_VIDEOTOOLBOX)
        return NULL;

    struct mp_image *image = NULL;
    CVPixelBufferRef pbuf = (CVPixelBufferRef)hw_image->planes[3];
    CVPixelBufferLockBaseAddress(pbuf, kCVPixelBufferLock_ReadOnly);
    size_t width  = CVPixelBufferGetWidth(pbuf);
    size_t height = CVPixelBufferGetHeight(pbuf);
    uint32_t cvpixfmt = CVPixelBufferGetPixelFormatType(pbuf);
    int imgfmt = mp_imgfmt_from_cvpixelformat(cvpixfmt);
    if (!imgfmt)
        goto unlock;

    struct mp_image img = {0};
    mp_image_setfmt(&img, imgfmt);
    mp_image_set_size(&img, width, height);

    if (CVPixelBufferIsPlanar(pbuf)) {
        int planes = CVPixelBufferGetPlaneCount(pbuf);
        for (int i = 0; i < planes; i++) {
            img.planes[i] = CVPixelBufferGetBaseAddressOfPlane(pbuf, i);
            img.stride[i] = CVPixelBufferGetBytesPerRowOfPlane(pbuf, i);
        }
    } else {
        img.planes[0] = CVPixelBufferGetBaseAddress(pbuf);
        img.stride[0] = CVPixelBufferGetBytesPerRow(pbuf);
    }

    mp_image_copy_attributes(&img, hw_image);

    image = mp_image_pool_new_copy(swpool, &img);

unlock:
    CVPixelBufferUnlockBaseAddress(pbuf, kCVPixelBufferLock_ReadOnly);
    return image;
}
