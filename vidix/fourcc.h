/*
 * fourcc.h
 * This file is part of VIDIX
 * Copyright 2002 Nick Kurshev
 * Licence: GPL
 * This interface is based on v4l2, fbvid.h, mga_vid.h projects
 * and personally my ideas.
*/
#ifndef FOURCC_H
#define FOURCC_H

/*  Four-character-code (FOURCC) */
#define vid_fourcc(a,b,c,d)\
        (((unsigned)(a)<<0)|((unsigned)(b)<<8)|((unsigned)(c)<<16)|((unsigned)(d)<<24))

/* RGB fourcc */
#define IMGFMT_RGB332  vid_fourcc('R','G','B','1') /*  8  RGB-3-3-2     */
#define IMGFMT_RGB555  vid_fourcc('R','G','B','O') /* 16  RGB-5-5-5     */
#define IMGFMT_RGB565  vid_fourcc('R','G','B','P') /* 16  RGB-5-6-5     */
#define IMGFMT_RGB555X vid_fourcc('R','G','B','Q') /* 16  RGB-5-5-5 BE  */
#define IMGFMT_RGB565X vid_fourcc('R','G','B','R') /* 16  RGB-5-6-5 BE  */
#define IMGFMT_BGR15   vid_fourcc('B','G','R',15)  /* 15  BGR-5-5-5     */
#define IMGFMT_RGB15   vid_fourcc('R','G','B',15)  /* 15  RGB-5-5-5     */
#define IMGFMT_BGR16   vid_fourcc('B','G','R',16)  /* 32  BGR-5-6-5     */
#define IMGFMT_RGB16   vid_fourcc('R','G','B',16)  /* 32  RGB-5-6-5     */
#define IMGFMT_BGR24   vid_fourcc('B','G','R',24)  /* 24  BGR-8-8-8     */
#define IMGFMT_RGB24   vid_fourcc('R','G','B',24)  /* 24  RGB-8-8-8     */
#define IMGFMT_BGR32   vid_fourcc('B','G','R',32)  /* 32  BGR-8-8-8-8   */
#define IMGFMT_RGB32   vid_fourcc('R','G','B',32)  /* 32  RGB-8-8-8-8   */

/* Planar YUV Formats */
#define IMGFMT_YVU9    vid_fourcc('Y','V','U','9') /* 9   YVU 4:1:0 */
#define IMGFMT_IF09    vid_fourcc('I','F','0','9') /* 9.5 YUV 4:1:0 */
#define IMGFMT_YV12    vid_fourcc('Y','V','1','2') /* 12  YVU 4:2:0 */
#define IMGFMT_I420    vid_fourcc('I','4','2','0') /* 12  YUV 4:2:0 */
#define IMGFMT_IYUV    vid_fourcc('I','Y','U','V') /* 12  YUV 4:2:0 */
#define IMGFMT_CLPL    vid_fourcc('C','L','P','L') /* 12            */
#define IMGFMT_Y800    vid_fourcc('Y','8','0','0') /* 8   Y   Grayscale */
#define IMGFMT_Y8      vid_fourcc('Y','8',' ',' ') /* 8   Y   Grayscale */

/* Packed YUV Formats */
#define IMGFMT_IUYV    vid_fourcc('I','U','Y','V') /* 16 line order {0,2,4,...1,3,5} */
#define IMGFMT_IY41    vid_fourcc('I','Y','4','1') /* 12 line order {0,2,4,...1,3,5} */
#define IMGFMT_IYU1    vid_fourcc('I','Y','U','1') /* 12 IEEE 1394 Digital Camera */
#define IMGFMT_IYU2    vid_fourcc('I','Y','U','2') /* 24 IEEE 1394 Digital Camera */
#define IMGFMT_UYVY    vid_fourcc('U','Y','V','Y') /* 16 UYVY 4:2:2 */
#define IMGFMT_UYNV    vid_fourcc('U','Y','N','V') /* 16 UYVY 4:2:2 */
#define IMGFMT_cyuv    vid_fourcc('c','y','u','v') /* 16 */
#define IMGFMT_Y422    vid_fourcc('Y','4','2','2') /* 16 UYVY 4:2:2 */
#define IMGFMT_YUY2    vid_fourcc('Y','U','Y','2') /* 16 YUYV 4:2:2 */
#define IMGFMT_YUNV    vid_fourcc('Y','U','N','V') /* 16 YUYV 4:2:2 */
#define IMGFMT_YVYU    vid_fourcc('Y','V','Y','U') /* 16 YVYU 4:2:2 */
#define IMGFMT_Y41P    vid_fourcc('Y','4','1','P') /* 12 YUV 4:1:1 */
#define IMGFMT_Y211    vid_fourcc('Y','2','1','1') /* 8.5 YUV 2:1:1 */
#define IMGFMT_Y41T    vid_fourcc('Y','4','1','T') /* 12 YUV 4:1:1 */
#define IMGFMT_Y42T    vid_fourcc('Y','4','2','T') /* 16 UYVU 4:2:2 */
#define IMGFMT_V422    vid_fourcc('V','4','2','2') /* 16 YUY2 4:2:2 */
#define IMGFMT_V655    vid_fourcc('V','6','5','5') /* 16 YUV 4:2:2 */
#define IMGFMT_CLJR    vid_fourcc('C','L','J','R') /* 7.9 YUV 4:1:1 */
#define IMGFMT_YUVP    vid_fourcc('Y','U','V','P') /* 24 Y0U0Y1V0 */
#define IMGFMT_UYVP    vid_fourcc('U','Y','V','P') /* 24 U0Y0V0Y1 */

/*  Vendor-specific formats   */
#define IMGFMT_WNVA    vid_fourcc('W','N','V','A') /* Winnov hw compress */

#endif
