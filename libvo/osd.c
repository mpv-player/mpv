// Generic alpha renderers for all YUV modes and RGB depths.
// These are "reference implementations", should be optimized later (MMX, etc)

//#define FAST_OSD
//#define FAST_OSD_TABLE

#include "config.h"
#include "osd.h"
#include "../mmx_defs.h"

void vo_draw_alpha_yv12(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
#ifdef FAST_OSD
    w=w>>1;
#endif
    for(y=0;y<h;y++){
        register int x;
        for(x=0;x<w;x++){
#ifdef FAST_OSD
            if(srca[2*x+0]) dstbase[2*x+0]=src[2*x+0];
            if(srca[2*x+1]) dstbase[2*x+1]=src[2*x+1];
#else
            if(srca[x]) dstbase[x]=((dstbase[x]*srca[x])>>8)+src[x];
#endif
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
    return;
}

void vo_draw_alpha_yuy2(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
#ifdef FAST_OSD
    w=w>>1;
#endif
    for(y=0;y<h;y++){
        register int x;
        for(x=0;x<w;x++){
#ifdef FAST_OSD
            if(srca[2*x+0]) dstbase[4*x+0]=src[2*x+0];
            if(srca[2*x+1]) dstbase[4*x+2]=src[2*x+1];
#else
            if(srca[x]) dstbase[2*x]=((dstbase[2*x]*srca[x])>>8)+src[x];
#endif
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
    return;
}

void vo_draw_alpha_rgb24(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register unsigned char *dst = dstbase;
        register int x;
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
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
    return;
}

void vo_draw_alpha_rgb32(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register int x;
#ifdef ARCH_X86
#if 0 /*def HAVE_MMX2*/
	asm volatile(
		"pxor %%mm7, %%mm7		\n\t"
		"xorl %%eax, %%eax		\n\t"
		"pcmpeqb %%mm6, %%mm6		\n\t" // F..F
		".align 16\n\t"
		"1:				\n\t"
		"movq (%0, %%eax, 4), %%mm0	\n\t" // dstbase
		"movq %%mm0, %%mm1		\n\t"
		"punpcklbw %%mm7, %%mm0		\n\t"
		"punpckhbw %%mm7, %%mm1		\n\t"
		"movd (%1, %%eax), %%mm2	\n\t" // srca ABCD0000
		"paddb %%mm6, %%mm2		\n\t"
		"punpcklbw %%mm2, %%mm2		\n\t" // srca AABBCCDD
		"punpcklbw %%mm2, %%mm2		\n\t" // srca AAAABBBB
		"movq %%mm2, %%mm3		\n\t"
		"punpcklbw %%mm7, %%mm2		\n\t" // srca 0A0A0A0A
		"punpckhbw %%mm7, %%mm3		\n\t" // srca 0B0B0B0B
		"pmullw %%mm2, %%mm0		\n\t"
		"pmullw %%mm3, %%mm1		\n\t"
		"psrlw $8, %%mm0		\n\t"
		"psrlw $8, %%mm1		\n\t"
		"packuswb %%mm1, %%mm0		\n\t"
		"movd (%2, %%eax), %%mm2	\n\t" // src ABCD0000
		"punpcklbw %%mm2, %%mm2		\n\t" // src AABBCCDD
		"punpcklbw %%mm2, %%mm2		\n\t" // src AAAABBBB
		"paddb %%mm2, %%mm0		\n\t"
		MOVNTQ" %%mm0, (%0, %%eax, 4)	\n\t"
		"addl $2, %%eax			\n\t"
		"cmpl %3, %%eax			\n\t"
		" jb 1b				\n\t"

		:: "r" (dstbase), "r" (srca), "r" (src), "r" (w)
		: "%eax"
		);
#else /* 0 HAVE_MMX2*/
	asm volatile(
		"xorl %%eax, %%eax		\n\t"
		"xorl %%ebx, %%ebx		\n\t"
		"xorl %%edx, %%edx		\n\t"
		".align 16\n\t"
		"1:				\n\t"
		"movb (%1, %%eax), %%bl		\n\t"
		"cmpb $0, %%bl			\n\t"
		" jz 2f				\n\t"
		"movzxb (%2, %%eax), %%edx	\n\t"
		"shll $8, %%edx			\n\t"
		"decb %%bl			\n\t"
		"movzxb (%0, %%eax, 4), %%ecx	\n\t"
		"imull %%ebx, %%ecx		\n\t"
		"addl %%edx, %%ecx		\n\t"
		"movb %%ch, (%0, %%eax, 4)	\n\t"

		"movzxb 1(%0, %%eax, 4), %%ecx	\n\t"
		"imull %%ebx, %%ecx		\n\t"
		"addl %%edx, %%ecx		\n\t"
		"movb %%ch, 1(%0, %%eax, 4)	\n\t"

		"movzxb 2(%0, %%eax, 4), %%ecx	\n\t"
		"imull %%ebx, %%ecx		\n\t"
		"addl %%edx, %%ecx		\n\t"
		"movb %%ch, 2(%0, %%eax, 4)	\n\t"

		"2:				\n\t"
		"addl $1, %%eax			\n\t"
		"cmpl %3, %%eax			\n\t"
		" jb 1b				\n\t"

		:: "r" (dstbase), "r" (srca), "r" (src), "m" (w)
		: "%eax", "%ebx", "%ecx", "%edx"
		);
#endif /* 0 HAVE_MMX*/
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
#if 0 /*def HAVE_MMX2*/
	asm volatile(SFENCE:::"memory");
	asm volatile(EMMS:::"memory");
#endif
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

