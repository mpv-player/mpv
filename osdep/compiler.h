#ifndef MPV_COMPILER_H
#define MPV_COMPILER_H

#include <assert.h>

#define MP_EXPAND_ARGS(...) __VA_ARGS__

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MP_NORETURN [[noreturn]]
#define MP_FALLTHROUGH [[fallthrough]]
#define MP_WARN_UNUSED_RESULT [[nodiscard]]
#define MP_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__clang__)
#define MP_NORETURN __attribute__((noreturn))
#define MP_FALLTHROUGH __attribute__((fallthrough))
#define MP_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define MP_UNUSED __attribute__((unused))
#else
#define MP_NORETURN
#define MP_FALLTHROUGH do {} while (0)
#define MP_WARN_UNUSED_RESULT
#define MP_UNUSED
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ < 202311L) && !defined(thread_local)
#define thread_local _Thread_local
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if __has_attribute(nonstring)
#define MP_NONSTRING __attribute__((nonstring))
#else
#define MP_NONSTRING
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__((format(printf, a1, a2)))
#define SCANF_ATTRIBUTE(a1, a2) __attribute__((format(scanf, a1, a2)))
#define MP_ASSERT_UNREACHABLE() (assert(!"unreachable"), __builtin_unreachable())
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#define SCANF_ATTRIBUTE(a1, a2)
#define MP_ASSERT_UNREACHABLE() (assert(!"unreachable"), abort())
#endif

// Broken crap with __USE_MINGW_ANSI_STDIO
#if defined(__MINGW32__) && defined(__GNUC__) && !defined(__clang__)
#undef PRINTF_ATTRIBUTE
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (gnu_printf, a1, a2)))
#endif

#endif
