#ifndef __MPLAYER_MEMCPY
#define __MPLAYER_MEMCPY 1

#ifdef USE_FASTMEMCPY

#if defined( HAVE_MMX2 ) || defined( HAVE_3DNOW ) || defined( HAVE_MMX )
#include <stddef.h>

extern void * fast_memcpy(void * to, const void * from, size_t len);
#define memcpy(a,b,c) fast_memcpy(a,b,c)

#endif

#endif

#endif
