/*
  This file contains most common definitions to make MMX stuff
  easy portable between different kinds of mmx clones
  Written By Nick Kurshev <nickols_k@mail.ru>
*/

#ifndef MMXDEFS_INCLUDED
#define MMXDEFS_INCLUDED

#include "config.h"

#undef HAVE_MMX1
#if defined(HAVE_MMX) && !defined(HAVE_MMX2) && !defined(HAVE_3DNOW) && !defined(HAVE_SSE)
#define HAVE_MMX1
#endif

#undef HAVE_K6_2PLUS
#if !defined( HAVE_MMX2 ) && defined( HAVE_3DNOW )
#define HAVE_K6_2PLUS
#endif

#ifdef HAVE_SSE2
#define MMREG_SIZE 16
#else
#define MMREG_SIZE 8
#endif

#ifdef HAVE_3DNOW
#define PREFETCH  "prefetch"
#define PREFETCHW "prefetchw" 
#define PAVGB	  "pavgusb"
#elif defined ( HAVE_MMX2 )
#define PREFETCH "prefetchnta"
#define PREFETCHW "prefetcht0" 
#define PAVGB	  "pavgb"
#else
#define PREFETCH "/nop"
#define PREFETCHW "/nop" 
#endif

#ifdef HAVE_3DNOW
/* On K6 femms is faster of emms. On K7 femms is directly mapped on emms. */
#define EMMS     "femms"
#else
#define EMMS     "emms"
#endif

#ifdef HAVE_MMX2
#define MOVNTQ "movntq"
#define SFENCE "sfence"
#else
#define MOVNTQ "movq"
#define SFENCE "/nop"
#endif


#endif
