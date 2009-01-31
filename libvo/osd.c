// Generic alpha renderers for all YUV modes and RGB depths.
// These are "reference implementations", should be optimized later (MMX, etc)
// Templating Code from Michael Niedermayer (michaelni@gmx.at) is under GPL

//#define FAST_OSD
//#define FAST_OSD_TABLE

#include "config.h"
#include "osd.h"
#include "mp_msg.h"
#include <inttypes.h>
#include "cpudetect.h"

#if ARCH_X86
#define CAN_COMPILE_X86_ASM
#endif

#ifdef CAN_COMPILE_X86_ASM
static const uint64_t bFF __attribute__((aligned(8))) = 0xFFFFFFFFFFFFFFFFULL;
static const unsigned long long mask24lh  __attribute__((aligned(8))) = 0xFFFF000000000000ULL;
static const unsigned long long mask24hl  __attribute__((aligned(8))) = 0x0000FFFFFFFFFFFFULL;
#endif

//Note: we have C, X86-nommx, MMX, MMX2, 3DNOW version therse no 3DNOW+MMX2 one
//Plain C versions
#if !HAVE_MMX || defined (RUNTIME_CPUDETECT)
#define COMPILE_C
#endif

#ifdef CAN_COMPILE_X86_ASM

#if (HAVE_MMX && !HAVE_AMD3DNOW && !HAVE_MMX2) || defined (RUNTIME_CPUDETECT)
#define COMPILE_MMX
#endif

#if HAVE_MMX2 || defined (RUNTIME_CPUDETECT)
#define COMPILE_MMX2
#endif

#if (HAVE_AMD3DNOW && !HAVE_MMX2) || defined (RUNTIME_CPUDETECT)
#define COMPILE_3DNOW
#endif
#endif //CAN_COMPILE_X86_ASM

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#define HAVE_MMX 0
#define HAVE_MMX2 0
#define HAVE_AMD3DNOW 0

#ifndef CAN_COMPILE_X86_ASM

#ifdef COMPILE_C
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#define HAVE_MMX 0
#define HAVE_MMX2 0
#define HAVE_AMD3DNOW 0
#define RENAME(a) a ## _C
#include "osd_template.c"
#endif

#else

//X86 noMMX versions
#ifdef COMPILE_C
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#define HAVE_MMX 0
#define HAVE_MMX2 0
#define HAVE_AMD3DNOW 0
#define RENAME(a) a ## _X86
#include "osd_template.c"
#endif

//MMX versions
#ifdef COMPILE_MMX
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#define HAVE_MMX 1
#define HAVE_MMX2 0
#define HAVE_AMD3DNOW 0
#define RENAME(a) a ## _MMX
#include "osd_template.c"
#endif

//MMX2 versions
#ifdef COMPILE_MMX2
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#define HAVE_MMX 1
#define HAVE_MMX2 1
#define HAVE_AMD3DNOW 0
#define RENAME(a) a ## _MMX2
#include "osd_template.c"
#endif

//3DNOW versions
#ifdef COMPILE_3DNOW
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#define HAVE_MMX 1
#define HAVE_MMX2 0
#define HAVE_AMD3DNOW 1
#define RENAME(a) a ## _3DNow
#include "osd_template.c"
#endif

#endif //CAN_COMPILE_X86_ASM

void vo_draw_alpha_yv12(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
#ifdef RUNTIME_CPUDETECT
#ifdef CAN_COMPILE_X86_ASM
	// ordered by speed / fastest first
	if(gCpuCaps.hasMMX2)
		vo_draw_alpha_yv12_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.has3DNow)
		vo_draw_alpha_yv12_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.hasMMX)
		vo_draw_alpha_yv12_MMX(w, h, src, srca, srcstride, dstbase, dststride);
	else
		vo_draw_alpha_yv12_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_yv12_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#else //RUNTIME_CPUDETECT
#if HAVE_MMX2
		vo_draw_alpha_yv12_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_AMD3DNOW
		vo_draw_alpha_yv12_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_MMX
		vo_draw_alpha_yv12_MMX(w, h, src, srca, srcstride, dstbase, dststride);
#elif ARCH_X86
		vo_draw_alpha_yv12_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_yv12_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#endif //!RUNTIME_CPUDETECT
}

void vo_draw_alpha_yuy2(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
#ifdef RUNTIME_CPUDETECT
#ifdef CAN_COMPILE_X86_ASM
	// ordered by speed / fastest first
	if(gCpuCaps.hasMMX2)
		vo_draw_alpha_yuy2_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.has3DNow)
		vo_draw_alpha_yuy2_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.hasMMX)
		vo_draw_alpha_yuy2_MMX(w, h, src, srca, srcstride, dstbase, dststride);
	else
		vo_draw_alpha_yuy2_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_yuy2_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#else //RUNTIME_CPUDETECT
#if HAVE_MMX2
		vo_draw_alpha_yuy2_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_AMD3DNOW
		vo_draw_alpha_yuy2_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_MMX
		vo_draw_alpha_yuy2_MMX(w, h, src, srca, srcstride, dstbase, dststride);
#elif ARCH_X86
		vo_draw_alpha_yuy2_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_yuy2_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#endif //!RUNTIME_CPUDETECT
}

void vo_draw_alpha_uyvy(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
#ifdef RUNTIME_CPUDETECT
#ifdef CAN_COMPILE_X86_ASM
	// ordered by speed / fastest first
	if(gCpuCaps.hasMMX2)
		vo_draw_alpha_uyvy_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.has3DNow)
		vo_draw_alpha_uyvy_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.hasMMX)
		vo_draw_alpha_uyvy_MMX(w, h, src, srca, srcstride, dstbase, dststride);
	else
		vo_draw_alpha_uyvy_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_uyvy_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#else //RUNTIME_CPUDETECT
#if HAVE_MMX2
		vo_draw_alpha_uyvy_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_AMD3DNOW
		vo_draw_alpha_uyvy_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_MMX
		vo_draw_alpha_uyvy_MMX(w, h, src, srca, srcstride, dstbase, dststride);
#elif ARCH_X86
		vo_draw_alpha_uyvy_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_uyvy_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#endif //!RUNTIME_CPUDETECT
}

void vo_draw_alpha_rgb24(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
#ifdef RUNTIME_CPUDETECT
#ifdef CAN_COMPILE_X86_ASM
	// ordered by speed / fastest first
	if(gCpuCaps.hasMMX2)
		vo_draw_alpha_rgb24_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.has3DNow)
		vo_draw_alpha_rgb24_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.hasMMX)
		vo_draw_alpha_rgb24_MMX(w, h, src, srca, srcstride, dstbase, dststride);
	else
		vo_draw_alpha_rgb24_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_rgb24_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#else //RUNTIME_CPUDETECT
#if HAVE_MMX2
		vo_draw_alpha_rgb24_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_AMD3DNOW
		vo_draw_alpha_rgb24_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_MMX
		vo_draw_alpha_rgb24_MMX(w, h, src, srca, srcstride, dstbase, dststride);
#elif ARCH_X86
		vo_draw_alpha_rgb24_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_rgb24_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#endif //!RUNTIME_CPUDETECT
}

void vo_draw_alpha_rgb32(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
#ifdef RUNTIME_CPUDETECT
#ifdef CAN_COMPILE_X86_ASM
	// ordered by speed / fastest first
	if(gCpuCaps.hasMMX2)
		vo_draw_alpha_rgb32_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.has3DNow)
		vo_draw_alpha_rgb32_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
	else if(gCpuCaps.hasMMX)
		vo_draw_alpha_rgb32_MMX(w, h, src, srca, srcstride, dstbase, dststride);
	else
		vo_draw_alpha_rgb32_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_rgb32_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#else //RUNTIME_CPUDETECT
#if HAVE_MMX2
		vo_draw_alpha_rgb32_MMX2(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_AMD3DNOW
		vo_draw_alpha_rgb32_3DNow(w, h, src, srca, srcstride, dstbase, dststride);
#elif HAVE_MMX
		vo_draw_alpha_rgb32_MMX(w, h, src, srca, srcstride, dstbase, dststride);
#elif ARCH_X86
		vo_draw_alpha_rgb32_X86(w, h, src, srca, srcstride, dstbase, dststride);
#else
		vo_draw_alpha_rgb32_C(w, h, src, srca, srcstride, dstbase, dststride);
#endif
#endif //!RUNTIME_CPUDETECT
}

#ifdef FAST_OSD_TABLE
static unsigned short fast_osd_15bpp_table[256];
static unsigned short fast_osd_16bpp_table[256];
#endif

void vo_draw_alpha_init(void){
#ifdef FAST_OSD_TABLE
    int i;
    for(i=0;i<256;i++){
        fast_osd_15bpp_table[i]=((i>>3)<<10)|((i>>3)<<5)|(i>>3);
        fast_osd_16bpp_table[i]=((i>>3)<<11)|((i>>2)<<5)|(i>>3);
    }
#endif
//FIXME the optimized stuff is a lie for 15/16bpp as they aren't optimized yet
	if( mp_msg_test(MSGT_OSD,MSGL_V) )
	{
#ifdef RUNTIME_CPUDETECT
#ifdef CAN_COMPILE_X86_ASM
		// ordered per speed fasterst first
		if(gCpuCaps.hasMMX2)
			mp_msg(MSGT_OSD,MSGL_INFO,"Using MMX (with tiny bit MMX2) Optimized OnScreenDisplay\n");
		else if(gCpuCaps.has3DNow)
			mp_msg(MSGT_OSD,MSGL_INFO,"Using MMX (with tiny bit 3DNow) Optimized OnScreenDisplay\n");
		else if(gCpuCaps.hasMMX)
			mp_msg(MSGT_OSD,MSGL_INFO,"Using MMX Optimized OnScreenDisplay\n");
		else
			mp_msg(MSGT_OSD,MSGL_INFO,"Using X86 Optimized OnScreenDisplay\n");
#else
			mp_msg(MSGT_OSD,MSGL_INFO,"Using Unoptimized OnScreenDisplay\n");
#endif
#else //RUNTIME_CPUDETECT
#if HAVE_MMX2
			mp_msg(MSGT_OSD,MSGL_INFO,"Using MMX (with tiny bit MMX2) Optimized OnScreenDisplay\n");
#elif HAVE_AMD3DNOW
			mp_msg(MSGT_OSD,MSGL_INFO,"Using MMX (with tiny bit 3DNow) Optimized OnScreenDisplay\n");
#elif HAVE_MMX
			mp_msg(MSGT_OSD,MSGL_INFO,"Using MMX Optimized OnScreenDisplay\n");
#elif ARCH_X86
			mp_msg(MSGT_OSD,MSGL_INFO,"Using X86 Optimized OnScreenDisplay\n");
#else
			mp_msg(MSGT_OSD,MSGL_INFO,"Using Unoptimized OnScreenDisplay\n");
#endif
#endif //!RUNTIME_CPUDETECT
	}
}

void vo_draw_alpha_rgb15(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register unsigned short *dst = (unsigned short*) dstbase;
        register int x;
        for(x=0;x<w;x++){
            if(srca[x]){
#ifdef FAST_OSD
#ifdef FAST_OSD_TABLE
                dst[x]=fast_osd_15bpp_table[src[x]];
#else
		register unsigned int a=src[x]>>3;
                dst[x]=(a<<10)|(a<<5)|a;
#endif
#else
                unsigned char r=dst[x]&0x1F;
                unsigned char g=(dst[x]>>5)&0x1F;
                unsigned char b=(dst[x]>>10)&0x1F;
                r=(((r*srca[x])>>5)+src[x])>>3;
                g=(((g*srca[x])>>5)+src[x])>>3;
                b=(((b*srca[x])>>5)+src[x])>>3;
                dst[x]=(b<<10)|(g<<5)|r;
#endif
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
#ifdef FAST_OSD
#ifdef FAST_OSD_TABLE
                dst[x]=fast_osd_16bpp_table[src[x]];
#else
                dst[x]=((src[x]>>3)<<11)|((src[x]>>2)<<5)|(src[x]>>3);
#endif
#else
                unsigned char r=dst[x]&0x1F;
                unsigned char g=(dst[x]>>5)&0x3F;
                unsigned char b=(dst[x]>>11)&0x1F;
                r=(((r*srca[x])>>5)+src[x])>>3;
                g=(((g*srca[x])>>6)+src[x])>>2;
                b=(((b*srca[x])>>5)+src[x])>>3;
                dst[x]=(b<<11)|(g<<5)|r;
#endif
            }
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
    return;
}

