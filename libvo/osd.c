/*
 * generic alpha renderers for all YUV modes and RGB depths
 * These are "reference implementations", should be optimized later (MMX, etc).
 * templating code by Michael Niedermayer (michaelni@gmx.at)
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "osd.h"
#include "mp_msg.h"
#include <inttypes.h>
#include <sys/types.h>
#include "cpudetect.h"

#if ARCH_X86
static const uint64_t bFF __attribute__((aligned(8))) = 0xFFFFFFFFFFFFFFFFULL;
static const unsigned long long mask24lh  __attribute__((aligned(8))) = 0xFFFF000000000000ULL;
static const unsigned long long mask24hl  __attribute__((aligned(8))) = 0x0000FFFFFFFFFFFFULL;
#endif

//Note: we have C, X86-nommx, MMX, MMX2
//Plain C versions
#define COMPILE_C

#if ARCH_X86

#define COMPILE_MMX
#define COMPILE_MMX2

#endif /* ARCH_X86 */

#undef HAVE_MMX
#undef HAVE_MMX2
#define HAVE_MMX 0
#define HAVE_MMX2 0

#if ! ARCH_X86

#ifdef COMPILE_C
#undef HAVE_MMX
#undef HAVE_MMX2
#define HAVE_MMX 0
#define HAVE_MMX2 0
#define RENAME(a) a ## _C
#include "osd_template.c"
#endif

#else

//X86 noMMX versions
#ifdef COMPILE_C
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#define HAVE_MMX 0
#define HAVE_MMX2 0
#define RENAME(a) a ## _X86
#include "osd_template.c"
#endif

//MMX versions
#ifdef COMPILE_MMX
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#define HAVE_MMX 1
#define HAVE_MMX2 0
#define RENAME(a) a ## _MMX
#include "osd_template.c"
#endif

//MMX2 versions
#ifdef COMPILE_MMX2
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#define HAVE_MMX 1
#define HAVE_MMX2 1
#define RENAME(a) a ## _MMX2
#include "osd_template.c"
#endif

#endif /* ARCH_X86 */

void vo_draw_alpha_yv12(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
#if ARCH_X86
	// ordered by speed / fastest first
	if(gCpuCaps.hasMMX2)
		vo_draw_alpha_yv12_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.hasMMX)
		vo_draw_alpha_yv12_MMX(w, h, src, srca, srcstride, dstbase, dststride);
	else
		vo_draw_alpha_yv12_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_yv12_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
}

void vo_draw_alpha_yuy2(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
#if ARCH_X86
	// ordered by speed / fastest first
	if(gCpuCaps.hasMMX2)
		vo_draw_alpha_yuy2_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.hasMMX)
		vo_draw_alpha_yuy2_MMX(w, h, src, srca, srcstride, dstbase, dststride);
	else
		vo_draw_alpha_yuy2_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_yuy2_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
}

void vo_draw_alpha_rgb24(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
#if ARCH_X86
	// ordered by speed / fastest first
	if(gCpuCaps.hasMMX2)
		vo_draw_alpha_rgb24_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.hasMMX)
		vo_draw_alpha_rgb24_MMX(w, h, src, srca, srcstride, dstbase, dststride);
	else
		vo_draw_alpha_rgb24_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_rgb24_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
}

void vo_draw_alpha_rgb32(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
#if ARCH_X86
	// ordered by speed / fastest first
	if(gCpuCaps.hasMMX2)
		vo_draw_alpha_rgb32_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.hasMMX)
		vo_draw_alpha_rgb32_MMX(w, h, src, srca, srcstride, dstbase, dststride);
	else
		vo_draw_alpha_rgb32_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_rgb32_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
}

void vo_draw_alpha_init(void){
//FIXME the optimized stuff is a lie for 15/16bpp as they aren't optimized yet
	if( mp_msg_test(MSGT_OSD,MSGL_V) )
	{
#if ARCH_X86
		// ordered per speed fasterst first
		if(gCpuCaps.hasMMX2)
			mp_msg(MSGT_OSD,MSGL_INFO,"Using MMX (with tiny bit MMX2) Optimized OnScreenDisplay\n");
		else if(gCpuCaps.hasMMX)
			mp_msg(MSGT_OSD,MSGL_INFO,"Using MMX Optimized OnScreenDisplay\n");
		else
			mp_msg(MSGT_OSD,MSGL_INFO,"Using X86 Optimized OnScreenDisplay\n");
#else
			mp_msg(MSGT_OSD,MSGL_INFO,"Using Unoptimized OnScreenDisplay\n");
#endif
	}
}

void vo_draw_alpha_rgb12(int w, int h, unsigned char* src, unsigned char *srca,
                         int srcstride, unsigned char* dstbase, int dststride) {
    int y;
    for (y = 0; y < h; y++) {
        register unsigned short *dst = (unsigned short*) dstbase;
        register int x;
        for (x = 0; x < w; x++) {
            if(srca[x]){
                unsigned char r = dst[x] & 0x0F;
                unsigned char g = (dst[x] >> 4) & 0x0F;
                unsigned char b = (dst[x] >> 8) & 0x0F;
                r = (((r*srca[x]) >> 4) + src[x]) >> 4;
                g = (((g*srca[x]) >> 4) + src[x]) >> 4;
                b = (((b*srca[x]) >> 4) + src[x]) >> 4;
                dst[x] = (b << 8) | (g << 4) | r;
            }
        }
        src += srcstride;
        srca += srcstride;
        dstbase += dststride;
    }
    return;
}

void vo_draw_alpha_rgb15(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register unsigned short *dst = (unsigned short*) dstbase;
        register int x;
        for(x=0;x<w;x++){
            if(srca[x]){
                unsigned char r=dst[x]&0x1F;
                unsigned char g=(dst[x]>>5)&0x1F;
                unsigned char b=(dst[x]>>10)&0x1F;
                r=(((r*srca[x])>>5)+src[x])>>3;
                g=(((g*srca[x])>>5)+src[x])>>3;
                b=(((b*srca[x])>>5)+src[x])>>3;
                dst[x]=(b<<10)|(g<<5)|r;
            }
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
    return;
}

void vo_draw_alpha_rgb16(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register unsigned short *dst = (unsigned short*) dstbase;
        register int x;
        for(x=0;x<w;x++){
            if(srca[x]){
                unsigned char r=dst[x]&0x1F;
                unsigned char g=(dst[x]>>5)&0x3F;
                unsigned char b=(dst[x]>>11)&0x1F;
                r=(((r*srca[x])>>5)+src[x])>>3;
                g=(((g*srca[x])>>6)+src[x])>>2;
                b=(((b*srca[x])>>5)+src[x])>>3;
                dst[x]=(b<<11)|(g<<5)|r;
            }
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
    return;
}
