#ifndef MPLAYER_FMT_CONVERSION_H
#define MPLAYER_FMT_CONVERSION_H

#include <stdio.h>
#include "libavutil/avutil.h"
#include "libmpcodecs/img_format.h"

enum PixelFormat imgfmt2pixfmt(int fmt)
{
    switch (fmt) {
        case IMGFMT_BGR32:
            return PIX_FMT_RGB32;
        case IMGFMT_BGR24:
            return PIX_FMT_BGR24;
        case IMGFMT_BGR16:
            return PIX_FMT_RGB565;
        case IMGFMT_BGR15:
            return PIX_FMT_RGB555;
        case IMGFMT_BGR8:
            return PIX_FMT_RGB8;
        case IMGFMT_BGR4:
            return PIX_FMT_RGB4;
        case IMGFMT_BGR1:
        case IMGFMT_RGB1:
            return PIX_FMT_MONOBLACK;
        case IMGFMT_RG4B:
            return PIX_FMT_BGR4_BYTE;
        case IMGFMT_BG4B:
            return PIX_FMT_RGB4_BYTE;
        case IMGFMT_RGB32:
            return PIX_FMT_BGR32;
        case IMGFMT_RGB24:
            return PIX_FMT_RGB24;
        case IMGFMT_RGB16:
            return PIX_FMT_BGR565;
        case IMGFMT_RGB15:
            return PIX_FMT_BGR555;
        case IMGFMT_RGB8:
            return PIX_FMT_BGR8;
        case IMGFMT_RGB4:
            return PIX_FMT_BGR4;
        case IMGFMT_YUY2:
            return PIX_FMT_YUYV422;
        case IMGFMT_UYVY:
            return PIX_FMT_UYVY422;
        case IMGFMT_NV12:
            return PIX_FMT_NV12;
        case IMGFMT_NV21:
            return PIX_FMT_NV21;
        case IMGFMT_Y800:
        case IMGFMT_Y8:
            return PIX_FMT_GRAY8;
        case IMGFMT_IF09:
        case IMGFMT_YVU9:
            return PIX_FMT_YUV410P;
        case IMGFMT_I420:
        case IMGFMT_IYUV:
        case IMGFMT_YV12:
            return PIX_FMT_YUV420P;
        case IMGFMT_411P:
            return PIX_FMT_YUV411P;
        case IMGFMT_422P:
            return PIX_FMT_YUV422P;
        case IMGFMT_444P:
            return PIX_FMT_YUV444P;
        default:
            fprintf(stderr, "Unsupported format %s\n", vo_format_name(fmt));
    }

    return PIX_FMT_NONE;
}

#endif /* MPLAYER_FMT_CONVERSION_H */
