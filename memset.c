#include "config.h"

#ifdef USE_FASTMEMSET
#if defined(HAVE_SSE) || defined(HAVE_SSE2)
/* (C) 2001 Csabai Csaba <csibi@diablo.ovinet.hu> */
inline void *fast_memset(void *ptr, long val, long num) 
{
    __asm__ __volatile__(
	"cmpxchg8 (%2)"
	: "=a" (val), "=d" (num)
	: "r" (ptr), "0" (val), "1" (num)
	:"memory");

    return(ptr);
}
#endif
#endif
