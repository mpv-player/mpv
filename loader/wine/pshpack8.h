#ifndef __WINE_PSHPACK_H
#define __WINE_PSHPACK_H 8

#if 0
//#pragma pack(8)
#elif !defined(RC_INVOKED)
#error "8 as alignment is not supported"
#endif /* 0 ; !defined(RC_INVOKED) */

#else /* !defined(__WINE_PSHPACK_H) */
#error "Nested pushing of alignment isn't supported by the compiler"
#endif /* !defined(__WINE_PSHPACK_H) */
