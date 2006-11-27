#ifdef __WINE_PSHPACK_H
#undef __WINE_PSHPACK_H

#if (defined(__GNUC__) || defined(__SUNPRO_C)) && !defined(__APPLE__)
#pragma pack()
#elif defined(__SUNPRO_CC) || defined(__APPLE__)
#warning "Assumes default alignment is 4"
#pragma pack(4)
#elif !defined(RC_INVOKED)
#error "Restoration of the previous alignment isn't supported by the compiler"
#endif /* defined(__GNUC__) || defined(__SUNPRO_C) ; !defined(RC_INVOKED) */

#else /* defined(__WINE_PSHPACK_H) */
#error "Popping alignment isn't possible since no alignment has been pushed"
#endif /* defined(__WINE_PSHPACK_H) */
