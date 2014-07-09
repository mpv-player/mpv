#ifndef MPV_IMG_FOURCC_H
#define MPV_IMG_FOURCC_H

#include "osdep/endian.h"

#define MP_FOURCC(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((unsigned)(d)<<24))

#if BYTE_ORDER == BIG_ENDIAN
#define MP_FOURCC_E(a,b,c,d) MP_FOURCC(a,b,c,d)
#else
#define MP_FOURCC_E(a,b,c,d) MP_FOURCC(d,c,b,a)
#endif

#define MP_FOURCC_RGB8  MP_FOURCC_E(8,   'B', 'G', 'R')
#define MP_FOURCC_RGB12 MP_FOURCC_E(12,  'B', 'G', 'R')
#define MP_FOURCC_RGB15 MP_FOURCC_E(15,  'B', 'G', 'R')
#define MP_FOURCC_RGB16 MP_FOURCC_E(16,  'B', 'G', 'R')
#define MP_FOURCC_RGB24 MP_FOURCC_E(24,  'B', 'G', 'R')
#define MP_FOURCC_RGB32 MP_FOURCC_E('A', 'B', 'G', 'R')

#define MP_FOURCC_BGR8  MP_FOURCC_E(8,   'R', 'G', 'B')
#define MP_FOURCC_BGR12 MP_FOURCC_E(12,  'R', 'G', 'B')
#define MP_FOURCC_BGR15 MP_FOURCC_E(15,  'R', 'G', 'B')
#define MP_FOURCC_BGR16 MP_FOURCC_E(16,  'R', 'G', 'B')
#define MP_FOURCC_BGR24 MP_FOURCC_E(24,  'R', 'G', 'B')
#define MP_FOURCC_BGR32 MP_FOURCC_E('A', 'R', 'G', 'B')

#define MP_FOURCC_YVU9  MP_FOURCC('Y', 'U', 'V', '9')
#define MP_FOURCC_YUV9  MP_FOURCC('Y', 'V', 'U', '9')
#define MP_FOURCC_YV12  MP_FOURCC('Y', 'V', '1', '2')
#define MP_FOURCC_I420  MP_FOURCC('I', '4', '2', '0')
#define MP_FOURCC_IYUV  MP_FOURCC('I', 'Y', 'U', 'V')
#define MP_FOURCC_Y800  MP_FOURCC('Y', '8', '0', '0')
#define MP_FOURCC_Y8    MP_FOURCC('Y', '8', ' ', ' ')
#define MP_FOURCC_NV12  MP_FOURCC('N', 'V', '1', '2')
#define MP_FOURCC_NV21  MP_FOURCC('N', 'V', '2', '1')

#define MP_FOURCC_UYVY  MP_FOURCC('U', 'Y', 'V', 'Y')
#define MP_FOURCC_YUY2  MP_FOURCC('Y', 'U', 'Y', '2')

#define MP_FOURCC_MJPEG MP_FOURCC('M', 'J', 'P', 'G')

// NOTE: no "HM12" decoder exists, as vd_hmblck has been removed
//       likely breaks video with some TV cards
#define MP_FOURCC_HM12 0x32314D48

#endif
