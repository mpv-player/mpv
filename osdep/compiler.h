#ifndef MPV_COMPILER_H
#define MPV_COMPILER_H

#define MP_EXPAND_ARGS(...) __VA_ARGS__

#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format(printf, a1, a2)))
#define MP_NORETURN __attribute__((noreturn))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#define MP_NORETURN
#endif

// Broken crap with __USE_MINGW_ANSI_STDIO
#if defined(__MINGW32__) && defined(__GNUC__) && !defined(__clang__)
#undef PRINTF_ATTRIBUTE
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (gnu_printf, a1, a2)))
#endif

#if __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#else
#define alignof(x) (offsetof(struct {char unalign_; x u;}, u))
#endif

#endif
