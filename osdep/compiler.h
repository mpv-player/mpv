#ifndef MPV_COMPILER_H
#define MPV_COMPILER_H

#define MP_EXPAND_ARGS(...) __VA_ARGS__

#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format(printf, a1, a2)))
#define MP_NORETURN __attribute__((noreturn))
#define MP_FALLTHROUGH __attribute__((fallthrough))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#define MP_NORETURN
#define MP_FALLTHROUGH do {} while (0)
#endif

// Broken crap with __USE_MINGW_ANSI_STDIO
#if defined(__MINGW32__) && defined(__GNUC__) && !defined(__clang__)
#undef PRINTF_ATTRIBUTE
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (gnu_printf, a1, a2)))
#endif

#ifdef __GNUC__
#define MP_ASSERT_UNREACHABLE() (assert(!"unreachable"), __builtin_unreachable())
#else
#define MP_ASSERT_UNREACHABLE() (assert(!"unreachable"), abort())
#endif

#endif
