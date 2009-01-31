// Generic alpha renderers for all YUV modes and RGB depths.
// Optimized by Nick and Michael
// Code from Michael Niedermayer (michaelni@gmx.at) is under GPL

#undef PREFETCH
#undef EMMS
#undef PREFETCHW
#undef PAVGB

#if HAVE_AMD3DNOW
#define PREFETCH  "prefetch"
#define PREFETCHW "prefetchw"
#define PAVGB	  "pavgusb"
#elif HAVE_MMX2
#define PREFETCH "prefetchnta"
#define PREFETCHW "prefetcht0"
#define PAVGB	  "pavgb"
#else
#define PREFETCH " # nop"
#define PREFETCHW " # nop"
#endif

#if HAVE_AMD3DNOW
/* On K6 femms is faster of emms. On K7 femms is directly mapped on emms. */
#define EMMS     "femms"
#else
#define EMMS     "emms"
#endif

static inline void RENAME(vo_draw_alpha_yv12)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
#if defined(FAST_OSD) && !HAVE_MMX
    w=w>>1;
#endif
#if HAVE_MMX
    __asm__ volatile(
        "pcmpeqb %%mm5, %%mm5\n\t" // F..F
        "movq %%mm5, %%mm4\n\t"
        "movq %%mm5, %%mm7\n\t"
        "psllw $8, %%mm5\n\t" //FF00FF00FF00
        "psrlw $8, %%mm4\n\t" //00FF00FF00FF
        ::);        
#endif
    for(y=0;y<h;y++){
        register int x;
#if HAVE_MMX
    __asm__ volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
	::"m"(*dstbase),"m"(*srca),"m"(*src):"memory");
    for(x=0;x<w;x+=8){
	__asm__ volatile(
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
		"paddb	%%mm7, %%mm2\n\t"
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
#if HAVE_MMX
	__asm__ volatile(EMMS:::"memory");
#endif
    return;
}

static inline void RENAME(vo_draw_alpha_yuy2)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
#if defined(FAST_OSD) && !HAVE_MMX
    w=w>>1;
#endif
#if HAVE_MMX
    __asm__ volatile(
        "pxor %%mm7, %%mm7\n\t"
        "pcmpeqb %%mm5, %%mm5\n\t" // F..F
        "movq %%mm5, %%mm6\n\t"
        "movq %%mm5, %%mm4\n\t"
        "psllw $8, %%mm5\n\t" //FF00FF00FF00
        "psrlw $8, %%mm4\n\t" //00FF00FF00FF
        ::);        
#endif
    for(y=0;y<h;y++){
        register int x;
#if HAVE_MMX
    __asm__ volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
	::"m"(*dstbase),"m"(*srca),"m"(*src));
    for(x=0;x<w;x+=4){
	__asm__ volatile(
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
		"paddb	%%mm6, %%mm2\n\t"
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
            if(srca[x]) {
               dstbase[2*x]=((dstbase[2*x]*srca[x])>>8)+src[x];
               dstbase[2*x+1]=((((signed)dstbase[2*x+1]-128)*srca[x])>>8)+128;
           }
#endif
        }
#endif
	src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
#if HAVE_MMX
	__asm__ volatile(EMMS:::"memory");
#endif
    return;
}

static inline void RENAME(vo_draw_alpha_uyvy)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
  int y;
#if defined(FAST_OSD)
  w=w>>1;
#endif
  for(y=0;y<h;y++){
    register int x;
    for(x=0;x<w;x++){
#ifdef FAST_OSD
      if(srca[2*x+0]) dstbase[4*x+2]=src[2*x+0];
      if(srca[2*x+1]) dstbase[4*x+0]=src[2*x+1];
#else
      if(srca[x]) {
	dstbase[2*x+1]=((dstbase[2*x+1]*srca[x])>>8)+src[x];
	dstbase[2*x]=((((signed)dstbase[2*x]-128)*srca[x])>>8)+128;
      }
#endif
    }
    src+=srcstride;
    srca+=srcstride;
    dstbase+=dststride;
  }
}

static inline void RENAME(vo_draw_alpha_rgb24)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
#if HAVE_MMX
    __asm__ volatile(
        "pxor %%mm7, %%mm7\n\t"
        "pcmpeqb %%mm6, %%mm6\n\t" // F..F
        ::);        
#endif
    for(y=0;y<h;y++){
        register unsigned char *dst = dstbase;
        register int x;
#if ARCH_X86 && (!ARCH_X86_64 || HAVE_MMX)
#if HAVE_MMX
    __asm__ volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
	::"m"(*dst),"m"(*srca),"m"(*src):"memory");
    for(x=0;x<w;x+=2){
     if(srca[x] || srca[x+1])
	__asm__ volatile(
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
		"psrlq  $8, %%mm2\n\t" // srca AAABBBB0
		"movq	%%mm2, %%mm3\n\t"
		"punpcklbw %%mm7, %%mm2\n\t" // srca 0A0A0A0B
		"punpckhbw %%mm7, %%mm3\n\t" // srca 0B0B0B00
		"pmullw	%%mm2, %%mm0\n\t"
		"pmullw	%%mm3, %%mm1\n\t"
		"psrlw	$8, %%mm0\n\t"
		"psrlw	$8, %%mm1\n\t"
		"packuswb %%mm1, %%mm0\n\t"
		"movd %2, %%mm2	\n\t" // src ABCD0000
		"punpcklbw %%mm2, %%mm2\n\t" // src AABBCCDD
		"punpcklbw %%mm2, %%mm2\n\t" // src AAAABBBB
		"psrlq  $8, %%mm2\n\t" // src AAABBBB0
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
	    __asm__ volatile(
		"movzbl (%0), %%ecx\n\t"
		"movzbl 1(%0), %%eax\n\t"

		"imull %1, %%ecx\n\t"
		"imull %1, %%eax\n\t"

		"addl %2, %%ecx\n\t"
		"addl %2, %%eax\n\t"

		"movb %%ch, (%0)\n\t"
		"movb %%ah, 1(%0)\n\t"
		
                "movzbl 2(%0), %%eax\n\t"
		"imull %1, %%eax\n\t"
		"addl %2, %%eax\n\t"
		"movb %%ah, 2(%0)\n\t"
		:
		:"D" (dst),
		 "r" ((unsigned)srca[x]),
		 "r" (((unsigned)src[x])<<8)
		:"%eax", "%ecx"
		);
            }
	    dst += 3;
        }
#endif /* !HAVE_MMX */
#else /*non x86 arch or x86_64 with MMX disabled */
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
#if HAVE_MMX
	__asm__ volatile(EMMS:::"memory");
#endif
    return;
}

static inline void RENAME(vo_draw_alpha_rgb32)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
#ifdef WORDS_BIGENDIAN
    dstbase++;
#endif
#if HAVE_MMX
#if HAVE_AMD3DNOW
    __asm__ volatile(
        "pxor %%mm7, %%mm7\n\t"
        "pcmpeqb %%mm6, %%mm6\n\t" // F..F
        ::);
#else /* HAVE_AMD3DNOW */
    __asm__ volatile(
        "pxor %%mm7, %%mm7\n\t"
        "pcmpeqb %%mm5, %%mm5\n\t" // F..F
        "movq %%mm5, %%mm4\n\t"
        "psllw $8, %%mm5\n\t" //FF00FF00FF00
        "psrlw $8, %%mm4\n\t" //00FF00FF00FF
        ::);
#endif /* HAVE_AMD3DNOW */
#endif /* HAVE_MMX */
    for(y=0;y<h;y++){
        register int x;
#if ARCH_X86 && (!ARCH_X86_64 || HAVE_MMX)
#if HAVE_MMX
#if HAVE_AMD3DNOW
    __asm__ volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
	::"m"(*dstbase),"m"(*srca),"m"(*src):"memory");
    for(x=0;x<w;x+=2){
     if(srca[x] || srca[x+1])
	__asm__ volatile(
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
    __asm__ volatile(
	PREFETCHW" %0\n\t"
	PREFETCH" %1\n\t"
	PREFETCH" %2\n\t"
	::"m"(*dstbase),"m"(*srca),"m"(*src):"memory");
    for(x=0;x<w;x+=4){
	__asm__ volatile(
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
		"paddb	%3, %%mm2\n\t"
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
		:: "m" (dstbase[4*x]), "m" (srca[x]), "m" (src[x]), "m" (bFF)
		: "%eax");
	}
#endif
#else /* HAVE_MMX */
    for(x=0;x<w;x++){
        if(srca[x]){
	    __asm__ volatile(
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
#else /*non x86 arch or x86_64 with MMX disabled */
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
#if HAVE_MMX
	__asm__ volatile(EMMS:::"memory");
#endif
    return;
}
