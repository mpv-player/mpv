// Generic alpha renderers for all YUV modes and RGB depths.
// These are "reference implementations", should be optimized later (MMX, etc)
// Optimized by Nick and Michael

//#define FAST_OSD
//#define FAST_OSD_TABLE

#include "config.h"
#include "osd.h"
#include "../mmx_defs.h"
//#define ENABLE_PROFILE
#include "../my_profile.h"
#include <inttypes.h>

#ifdef HAVE_MMX
static const uint64_t bFF  __attribute__((aligned(8))) = 0xFFFFFFFFFFFFFFFFULL;
#endif

void vo_draw_alpha_yv12(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
#if defined(FAST_OSD) && !defined(HAVE_MMX)
    w=w>>1;
#endif
PROFILE_START();
    for(y=0;y<h;y++){
        register int x;
#ifdef HAVE_MMX
    asm volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
//	"pxor %%mm7, %%mm7\n\t"
	"pcmpeqb %%mm5, %%mm5\n\t" // F..F
	"movq %%mm5, %%mm4\n\t"
	"psllw $8, %%mm5\n\t" //FF00FF00FF00
	"psrlw $8, %%mm4\n\t" //00FF00FF00FF
	::"m"(*dstbase),"m"(*srca),"m"(*src):"memory");
    for(x=0;x<w;x+=8){
	asm volatile(
		"movl %1, %%eax\n\t"
		"orl 4%1, %%eax\n\t"
		" jz 1f\n\t"
		PREFETCHW" 32%0\n\t"
		PREFETCH" 32%1\n\t"
		PREFETCH" 32%2\n\t"
		"movq	%0, %%mm0\n\t" // dstbase
		"movq	%%mm0, %%mm1\n\t"
		"pand %%mm4, %%mm0\n\t" 	//0Y0Y0Y0Y
		"psrlw $8, %%mm1\n\t"		//0Y0Y0Y0Y
		"movq	%1, %%mm2\n\t" 		//srca HGFEDCBA
		"paddb	bFF, %%mm2\n\t"
		"movq %%mm2, %%mm3\n\t"
		"pand %%mm4, %%mm2\n\t" 	//0G0E0C0A
		"psrlw $8, %%mm3\n\t"		//0H0F0D0B
		"pmullw	%%mm2, %%mm0\n\t"
		"pmullw	%%mm3, %%mm1\n\t"
		"psrlw	$8, %%mm0\n\t"
		"pand %%mm5, %%mm1\n\t"
		"por %%mm1, %%mm0\n\t"
		"paddb	%2, %%mm0\n\t"
		"movq	%%mm0, %0\n\t"
		"1:\n\t"
		:: "m" (dstbase[x]), "m" (srca[x]), "m" (src[x])
		: "%eax");
	}
#else
        for(x=0;x<w;x++){
#ifdef FAST_OSD
            if(srca[2*x+0]) dstbase[2*x+0]=src[2*x+0];
            if(srca[2*x+1]) dstbase[2*x+1]=src[2*x+1];
#else
            if(srca[x]) dstbase[x]=((dstbase[x]*srca[x])>>8)+src[x];
#endif
        }
#endif
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
#ifdef HAVE_MMX
	asm volatile(EMMS:::"memory");
#endif
PROFILE_END("vo_draw_alpha_yv12");
    return;
}

void vo_draw_alpha_yuy2(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
#if defined(FAST_OSD) && !defined(HAVE_MMX)
    w=w>>1;
#endif
PROFILE_START();
    for(y=0;y<h;y++){
        register int x;
#ifdef HAVE_MMX
    asm volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
	"pxor %%mm7, %%mm7\n\t"
	"pcmpeqb %%mm5, %%mm5\n\t" // F..F
	"movq %%mm5, %%mm4\n\t"
	"psllw $8, %%mm5\n\t" //FF00FF00FF00
	"psrlw $8, %%mm4\n\t" //00FF00FF00FF
	::"m"(*dstbase),"m"(*srca),"m"(*src));
    for(x=0;x<w;x+=4){
	asm volatile(
		"movl %1, %%eax\n\t"
		"orl %%eax, %%eax\n\t"
		" jz 1f\n\t"
		PREFETCHW" 32%0\n\t"
		PREFETCH" 32%1\n\t"
		PREFETCH" 32%2\n\t"
		"movq	%0, %%mm0\n\t" // dstbase
		"movq	%%mm0, %%mm1\n\t"
		"pand %%mm4, %%mm0\n\t" 	//0Y0Y0Y0Y
		"movd	%%eax, %%mm2\n\t"	//srca 0000DCBA
		"paddb	bFF, %%mm2\n\t"
		"punpcklbw %%mm7, %%mm2\n\t"	//srca 0D0C0B0A
		"pmullw	%%mm2, %%mm0\n\t"
		"psrlw	$8, %%mm0\n\t"
		"pand %%mm5, %%mm1\n\t" 	//U0V0U0V0
		"movd %2, %%mm2\n\t"		//src 0000DCBA
		"punpcklbw %%mm7, %%mm2\n\t"	//srca 0D0C0B0A
		"por %%mm1, %%mm0\n\t"
		"paddb	%%mm2, %%mm0\n\t"
		"movq	%%mm0, %0\n\t"
		"1:\n\t"
		:: "m" (dstbase[x*2]), "m" (srca[x]), "m" (src[x])
		: "%eax");
	}
#else
        for(x=0;x<w;x++){
#ifdef FAST_OSD
            if(srca[2*x+0]) dstbase[4*x+0]=src[2*x+0];
            if(srca[2*x+1]) dstbase[4*x+2]=src[2*x+1];
#else
            if(srca[x]) dstbase[2*x]=((dstbase[2*x]*srca[x])>>8)+src[x];
#endif
        }
#endif
	src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
#ifdef HAVE_MMX
	asm volatile(EMMS:::"memory");
#endif
PROFILE_END("vo_draw_alpha_yuy2");
    return;
}

#ifdef HAVE_MMX
static const unsigned long long mask24lh  __attribute__((aligned(8))) = 0xFFFF000000000000ULL;
static const unsigned long long mask24hl  __attribute__((aligned(8))) = 0x0000FFFFFFFFFFFFULL;
#endif
void vo_draw_alpha_rgb24(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register unsigned char *dst = dstbase;
        register int x;
#ifdef ARCH_X86
#ifdef HAVE_MMX
    asm volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
	"pxor %%mm7, %%mm7\n\t"
	"pcmpeqb %%mm6, %%mm6\n\t" // F..F
	::"m"(*dst),"m"(*srca),"m"(*src):"memory");
    for(x=0;x<w;x+=2){
     if(srca[x] || srca[x+1])
	asm volatile(
		PREFETCHW" 32%0\n\t"
		PREFETCH" 32%1\n\t"
		PREFETCH" 32%2\n\t"
		"movq	%0, %%mm0\n\t" // dstbase
		"movq	%%mm0, %%mm1\n\t"
		"movq	%%mm0, %%mm5\n\t"
		"punpcklbw %%mm7, %%mm0\n\t"
		"punpckhbw %%mm7, %%mm1\n\t"
		"movd	%1, %%mm2\n\t" // srca ABCD0000
		"paddb	%%mm6, %%mm2\n\t"
		"punpcklbw %%mm2, %%mm2\n\t" // srca AABBCCDD
		"punpcklbw %%mm2, %%mm2\n\t" // srca AAAABBBB
		"movq	%%mm2, %%mm3\n\t"
		"punpcklbw %%mm7, %%mm2\n\t" // srca 0A0A0A0A
		"punpckhbw %%mm7, %%mm3\n\t" // srca 0B0B0B0B
		"pmullw	%%mm2, %%mm0\n\t"
		"pmullw	%%mm3, %%mm1\n\t"
		"psrlw	$8, %%mm0\n\t"
		"psrlw	$8, %%mm1\n\t"
		"packuswb %%mm1, %%mm0\n\t"
		"movd %2, %%mm2	\n\t" // src ABCD0000
		"punpcklbw %%mm2, %%mm2\n\t" // src AABBCCDD
		"punpcklbw %%mm2, %%mm2\n\t" // src AAAABBBB
		"paddb	%%mm2, %%mm0\n\t"
		"pand	%4, %%mm5\n\t"
		"pand	%3, %%mm0\n\t"
		"por	%%mm0, %%mm5\n\t"
		"movq	%%mm5, %0\n\t"
		:: "m" (dst[0]), "m" (srca[x]), "m" (src[x]), "m"(mask24hl), "m"(mask24lh));
		dst += 6;
	}
#else /* HAVE_MMX */
    for(x=0;x<w;x++){
        if(srca[x]){
	    asm volatile(
		"movzbl (%0), %%ecx\n\t"
		"movzbl 1(%0), %%eax\n\t"
		"movzbl 2(%0), %%edx\n\t"

		"imull %1, %%ecx\n\t"
		"imull %1, %%eax\n\t"
		"imull %1, %%edx\n\t"

 		"addl %2, %%ecx\n\t"
		"addl %2, %%eax\n\t"
		"addl %2, %%edx\n\t"

		"movb %%ch, (%0)\n\t"
		"movb %%ah, 1(%0)\n\t"
		"movb %%dh, 2(%0)\n\t"

		:
		:"r" (dst),
		 "r" ((unsigned)srca[x]),
		 "r" (((unsigned)src[x])<<8)
		:"%eax", "%ecx", "%edx"
		);
            }
	    dst += 3;
        }
#endif /* HAVE_MMX */
#else /*non x86 arch*/
        for(x=0;x<w;x++){
            if(srca[x]){
#ifdef FAST_OSD
		dst[0]=dst[1]=dst[2]=src[x];
#else
		dst[0]=((dst[0]*srca[x])>>8)+src[x];
		dst[1]=((dst[1]*srca[x])>>8)+src[x];
		dst[2]=((dst[2]*srca[x])>>8)+src[x];
#endif
            }
            dst+=3; // 24bpp
        }
#endif /* arch_x86 */
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
#ifdef HAVE_MMX
	asm volatile(EMMS:::"memory");
#endif
    return;
}

void vo_draw_alpha_rgb32(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
PROFILE_START();
    for(y=0;y<h;y++){
        register int x;
#ifdef ARCH_X86
#ifdef HAVE_MMX
#ifdef HAVE_3DNOW
    asm volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
	"pxor %%mm7, %%mm7\n\t"
	"pcmpeqb %%mm6, %%mm6\n\t" // F..F
	::"m"(*dstbase),"m"(*srca),"m"(*src):"memory");
    for(x=0;x<w;x+=2){
     if(srca[x] || srca[x+1])
	asm volatile(
		PREFETCHW" 32%0\n\t"
		PREFETCH" 32%1\n\t"
		PREFETCH" 32%2\n\t"
		"movq	%0, %%mm0\n\t" // dstbase
		"movq	%%mm0, %%mm1\n\t"
		"punpcklbw %%mm7, %%mm0\n\t"
		"punpckhbw %%mm7, %%mm1\n\t"
		"movd	%1, %%mm2\n\t" // srca ABCD0000
		"paddb	%%mm6, %%mm2\n\t"
		"punpcklbw %%mm2, %%mm2\n\t" // srca AABBCCDD
		"punpcklbw %%mm2, %%mm2\n\t" // srca AAAABBBB
		"movq	%%mm2, %%mm3\n\t"
		"punpcklbw %%mm7, %%mm2\n\t" // srca 0A0A0A0A
		"punpckhbw %%mm7, %%mm3\n\t" // srca 0B0B0B0B
		"pmullw	%%mm2, %%mm0\n\t"
		"pmullw	%%mm3, %%mm1\n\t"
		"psrlw	$8, %%mm0\n\t"
		"psrlw	$8, %%mm1\n\t"
		"packuswb %%mm1, %%mm0\n\t"
		"movd %2, %%mm2	\n\t" // src ABCD0000
		"punpcklbw %%mm2, %%mm2\n\t" // src AABBCCDD
		"punpcklbw %%mm2, %%mm2\n\t" // src AAAABBBB
		"paddb	%%mm2, %%mm0\n\t"
		"movq	%%mm0, %0\n\t"
		:: "m" (dstbase[4*x]), "m" (srca[x]), "m" (src[x]));
	}
#else //this is faster for intels crap
    asm volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
	"pxor %%mm7, %%mm7\n\t"
	"pcmpeqb %%mm5, %%mm5\n\t" // F..F
	"movq %%mm5, %%mm4\n\t"
	"psllw $8, %%mm5\n\t" //FF00FF00FF00
	"psrlw $8, %%mm4\n\t" //00FF00FF00FF
	::"m"(*dstbase),"m"(*srca),"m"(*src):"memory");
    for(x=0;x<w;x+=4){
	asm volatile(
		"movl %1, %%eax\n\t"
		"orl %%eax, %%eax\n\t"
		" jz 1f\n\t"
		PREFETCHW" 32%0\n\t"
		PREFETCH" 32%1\n\t"
		PREFETCH" 32%2\n\t"
		"movq	%0, %%mm0\n\t" // dstbase
		"movq	%%mm0, %%mm1\n\t"
		"pand %%mm4, %%mm0\n\t" 	//0R0B0R0B
		"psrlw $8, %%mm1\n\t"		//0?0G0?0G
		"movd	%%eax, %%mm2\n\t" 	//srca 0000DCBA
		"paddb	bFF, %%mm2\n\t"
		"punpcklbw %%mm2, %%mm2\n\t"	//srca DDCCBBAA
		"movq %%mm2, %%mm3\n\t"
		"punpcklbw %%mm7, %%mm2\n\t"	//srca 0B0B0A0A
		"pmullw	%%mm2, %%mm0\n\t"
		"pmullw	%%mm2, %%mm1\n\t"
		"psrlw	$8, %%mm0\n\t"
		"pand %%mm5, %%mm1\n\t"
		"por %%mm1, %%mm0\n\t"
		"movd %2, %%mm2	\n\t"		//src 0000DCBA
		"punpcklbw %%mm2, %%mm2\n\t" 	//src DDCCBBAA
		"movq %%mm2, %%mm6\n\t"
		"punpcklbw %%mm2, %%mm2\n\t"	//src BBBBAAAA
		"paddb	%%mm2, %%mm0\n\t"
		"movq	%%mm0, %0\n\t"

		"movq	8%0, %%mm0\n\t" // dstbase
		"movq	%%mm0, %%mm1\n\t"
		"pand %%mm4, %%mm0\n\t" 	//0R0B0R0B
		"psrlw $8, %%mm1\n\t"		//0?0G0?0G
		"punpckhbw %%mm7, %%mm3\n\t"	//srca 0D0D0C0C
		"pmullw	%%mm3, %%mm0\n\t"
		"pmullw	%%mm3, %%mm1\n\t"
		"psrlw	$8, %%mm0\n\t"
		"pand %%mm5, %%mm1\n\t"
		"por %%mm1, %%mm0\n\t"
		"punpckhbw %%mm6, %%mm6\n\t"	//src DDDDCCCC
		"paddb	%%mm6, %%mm0\n\t"
		"movq	%%mm0, 8%0\n\t"
		"1:\n\t"
		:: "m" (dstbase[4*x]), "m" (srca[x]), "m" (src[x])
		: "%eax");
	}
#endif
#else /* HAVE_MMX */
    for(x=0;x<w;x++){
        if(srca[x]){
	    asm volatile(
		"movzbl (%0), %%ecx\n\t"
		"movzbl 1(%0), %%eax\n\t"
		"movzbl 2(%0), %%edx\n\t"

		"imull %1, %%ecx\n\t"
		"imull %1, %%eax\n\t"
		"imull %1, %%edx\n\t"

 		"addl %2, %%ecx\n\t"
		"addl %2, %%eax\n\t"
		"addl %2, %%edx\n\t"

		"movb %%ch, (%0)\n\t"
		"movb %%ah, 1(%0)\n\t"
		"movb %%dh, 2(%0)\n\t"

		:
		:"r" (&dstbase[4*x]),
		 "r" ((unsigned)srca[x]),
		 "r" (((unsigned)src[x])<<8)
		:"%eax", "%ecx", "%edx"
		);
            }
        }
#endif /* HAVE_MMX */
#else /*non x86 arch*/
        for(x=0;x<w;x++){
            if(srca[x]){
#ifdef FAST_OSD
		dstbase[4*x+0]=dstbase[4*x+1]=dstbase[4*x+2]=src[x];
#else
		dstbase[4*x+0]=((dstbase[4*x+0]*srca[x])>>8)+src[x];
		dstbase[4*x+1]=((dstbase[4*x+1]*srca[x])>>8)+src[x];
		dstbase[4*x+2]=((dstbase[4*x+2]*srca[x])>>8)+src[x];
#endif
            }
        }
#endif /* arch_x86 */
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
#ifdef HAVE_MMX
	asm volatile(EMMS:::"memory");
#endif
PROFILE_END("vo_draw_alpha_rgb32");
    return;
}

#ifdef FAST_OSD_TABLE
static unsigned short fast_osd_15bpp_table[256];
static unsigned short fast_osd_16bpp_table[256];
#endif

void vo_draw_alpha_init(){
#ifdef FAST_OSD_TABLE
    int i;
    for(i=0;i<256;i++){
        fast_osd_15bpp_table[i]=((i>>3)<<10)|((i>>3)<<5)|(i>>3);
        fast_osd_16bpp_table[i]=((i>>3)<<11)|((i>>2)<<5)|(i>>3);
    }
#endif
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

