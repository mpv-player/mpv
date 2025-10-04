#ifndef MPV_COMPILER_H
#define MPV_COMPILER_H

#include <assert.h>
#include <stdio.h>

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

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if __has_attribute(nonstring)
#define MP_NONSTRING __attribute__((nonstring))
#else
#define MP_NONSTRING
#endif

#ifndef NDEBUG
#define MP_ASSERT_UNREACHABLE() assert(!"unreachable")
#elif __has_builtin(__builtin_unreachable)
#define MP_ASSERT_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define MP_ASSERT_UNREACHABLE(msg) __assume(0)
#elif __STDC_VERSION__ >= 202311L
#include <stddef.h>
#define MP_ASSERT_UNREACHABLE() unreachable()
#else
#define MP_ASSERT_UNREACHABLE() ((void)0)
#endif

#ifdef __MINGW_PRINTF_FORMAT
#define MP_PRINTF_FORMAT __MINGW_PRINTF_FORMAT
#elif __has_attribute(format)
#define MP_PRINTF_FORMAT __printf__
#endif

#ifdef __MINGW_SCANF_FORMAT
#define MP_SCANF_FORMAT __MINGW_SCANF_FORMAT
#elif __has_attribute(format)
#define MP_SCANF_FORMAT __scanf__
#endif

#ifdef MP_PRINTF_FORMAT
#define MP_PRINTF_ATTRIBUTE(a1, a2) __attribute__((format(MP_PRINTF_FORMAT, a1, a2)))
#else
#define MP_PRINTF_ATTRIBUTE(a1, a2)
#endif

#ifdef MP_SCANF_FORMAT
#define MP_SCANF_ATTRIBUTE(a1, a2) __attribute__((format(MP_SCANF_FORMAT, a1, a2)))
#else
#define MP_SCANF_ATTRIBUTE(a1, a2)
#endif

#endif
