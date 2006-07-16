#include "config.h"
#include "img_format.h"

const char *vo_format_name(int format)
{
    switch(format)
    {
	case IMGFMT_RGB1: return("RGB 1-bit");
	case IMGFMT_RGB4: return("RGB 4-bit");
	case IMGFMT_RG4B: return("RGB 4-bit per byte");
	case IMGFMT_RGB8: return("RGB 8-bit");
	case IMGFMT_RGB15: return("RGB 15-bit");
	case IMGFMT_RGB16: return("RGB 16-bit");
	case IMGFMT_RGB24: return("RGB 24-bit");
//	case IMGFMT_RGB32: return("RGB 32-bit");
	case IMGFMT_BGR1: return("BGR 1-bit");
	case IMGFMT_BGR4: return("BGR 4-bit");
	case IMGFMT_BG4B: return("BGR 4-bit per byte");
	case IMGFMT_BGR8: return("BGR 8-bit");
	case IMGFMT_BGR15: return("BGR 15-bit");
	case IMGFMT_BGR16: return("BGR 16-bit");
	case IMGFMT_BGR24: return("BGR 24-bit");
//	case IMGFMT_BGR32: return("BGR 32-bit");
	case IMGFMT_ABGR: return("ABGR");
	case IMGFMT_BGRA: return("BGRA");
	case IMGFMT_ARGB: return("ARGB");
	case IMGFMT_RGBA: return("RGBA");
	case IMGFMT_YVU9: return("Planar YVU9");
	case IMGFMT_IF09: return("Planar IF09");
	case IMGFMT_YV12: return("Planar YV12");
	case IMGFMT_I420: return("Planar I420");
	case IMGFMT_IYUV: return("Planar IYUV");
	case IMGFMT_CLPL: return("Planar CLPL");
	case IMGFMT_Y800: return("Planar Y800");
	case IMGFMT_Y8: return("Planar Y8");
	case IMGFMT_444P: return("Planar 444P");
	case IMGFMT_422P: return("Planar 422P");
	case IMGFMT_411P: return("Planar 411P");
	case IMGFMT_NV12: return("Planar NV12");
	case IMGFMT_NV21: return("Planar NV21");
        case IMGFMT_HM12: return("Planar NV12 Macroblock");
	case IMGFMT_IUYV: return("Packed IUYV");
	case IMGFMT_IY41: return("Packed IY41");
	case IMGFMT_IYU1: return("Packed IYU1");
	case IMGFMT_IYU2: return("Packed IYU2");
	case IMGFMT_UYVY: return("Packed UYVY");
	case IMGFMT_UYNV: return("Packed UYNV");
	case IMGFMT_cyuv: return("Packed CYUV");
	case IMGFMT_Y422: return("Packed Y422");
	case IMGFMT_YUY2: return("Packed YUY2");
	case IMGFMT_YUNV: return("Packed YUNV");
	case IMGFMT_YVYU: return("Packed YVYU");
	case IMGFMT_Y41P: return("Packed Y41P");
	case IMGFMT_Y211: return("Packed Y211");
	case IMGFMT_Y41T: return("Packed Y41T");
	case IMGFMT_Y42T: return("Packed Y42T");
	case IMGFMT_V422: return("Packed V422");
	case IMGFMT_V655: return("Packed V655");
	case IMGFMT_CLJR: return("Packed CLJR");
	case IMGFMT_YUVP: return("Packed YUVP");
	case IMGFMT_UYVP: return("Packed UYVP");
	case IMGFMT_MPEGPES: return("Mpeg PES");
	case IMGFMT_ZRMJPEGNI: return("Zoran MJPEG non-interlaced");
	case IMGFMT_ZRMJPEGIT: return("Zoran MJPEG top field first");
	case IMGFMT_ZRMJPEGIB: return("Zoran MJPEG bottom field first");
	case IMGFMT_XVMC_MOCO_MPEG2: return("MPEG1/2 Motion Compensation");
	case IMGFMT_XVMC_IDCT_MPEG2: return("MPEG1/2 Motion Compensation and IDCT");
    }
    return("Unknown");
}
