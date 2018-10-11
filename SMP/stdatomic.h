/*
 * MSVC stdatomic.h compatibility header.
 * Copyright (c) 2017 Matthew Oliver
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef SMP_STDATOMIC_H
#define SMP_STDATOMIC_H

#if !defined(_MSC_VER) || defined(__INTEL_COMPILER)
#   include_next <stdatomic.h>
#else

#include <stddef.h>
#include <stdint.h>
#include <intrin.h>
#include <windows.h>

#define _Atomic

#ifndef __ATOMIC_RELAXED
#define __ATOMIC_RELAXED 0
#endif
#ifndef __ATOMIC_CONSUME
#define __ATOMIC_CONSUME 1
#endif
#ifndef __ATOMIC_ACQUIRE
#define __ATOMIC_ACQUIRE 2
#endif
#ifndef __ATOMIC_RELEASE
#define __ATOMIC_RELEASE 3
#endif
#ifndef __ATOMIC_ACQ_REL
#define __ATOMIC_ACQ_REL 4
#endif
#ifndef __ATOMIC_SEQ_CST
#define __ATOMIC_SEQ_CST 5
#endif

enum memory_order
{
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
};

typedef enum memory_order memory_order;

typedef _Atomic _Bool atomic_bool;
typedef _Atomic char atomic_char;
typedef _Atomic signed char atomic_schar;
typedef _Atomic unsigned char atomic_uchar;
typedef _Atomic short atomic_short;
typedef _Atomic unsigned short atomic_ushort;
typedef _Atomic int atomic_int;
typedef _Atomic unsigned int atomic_uint;
typedef _Atomic long atomic_long;
typedef _Atomic unsigned long atomic_ulong;
typedef _Atomic long long atomic_llong;
typedef _Atomic unsigned long long atomic_ullong;
typedef _Atomic unsigned short atomic_char16_t;
typedef _Atomic unsigned int atomic_char32_t;
typedef _Atomic wchar_t atomic_wchar_t;
typedef _Atomic int_least8_t atomic_int_least8_t;
typedef _Atomic uint_least8_t atomic_uint_least8_t;
typedef _Atomic int_least16_t atomic_int_least16_t;
typedef _Atomic uint_least16_t atomic_uint_least16_t;
typedef _Atomic int_least32_t atomic_int_least32_t;
typedef _Atomic uint_least32_t atomic_uint_least32_t;
typedef _Atomic int_least64_t atomic_int_least64_t;
typedef _Atomic uint_least64_t atomic_uint_least64_t;
typedef _Atomic int_fast8_t atomic_int_fast8_t;
typedef _Atomic uint_fast8_t atomic_uint_fast8_t;
typedef _Atomic int_fast16_t atomic_int_fast16_t;
typedef _Atomic uint_fast16_t atomic_uint_fast16_t;
typedef _Atomic int_fast32_t atomic_int_fast32_t;
typedef _Atomic uint_fast32_t atomic_uint_fast32_t;
typedef _Atomic int_fast64_t atomic_int_fast64_t;
typedef _Atomic uint_fast64_t atomic_uint_fast64_t;
typedef _Atomic intptr_t atomic_intptr_t;
typedef _Atomic uintptr_t atomic_uintptr_t;
typedef _Atomic size_t atomic_size_t;
typedef _Atomic ptrdiff_t atomic_ptrdiff_t;
typedef _Atomic intmax_t atomic_intmax_t;
typedef _Atomic uintmax_t atomic_uintmax_t;

#define ATOMIC_VAR_INIT(value) (value)

#define atomic_init(obj, value) \
do {                            \
    *(obj) = (value);           \
} while (0)

#define kill_dependency(y) ((void)0)

#define atomic_thread_fence(order)  MemoryBarrier()
#define atomic_signal_fence(order) _ReadWriteBarrier()

#define atomic_is_lock_free(obj) (sizeof(obj) <= sizeof(void *))

static __inline _Bool _atomic_compare_exchange(volatile void *object, void *expected, uint64_t desired, uint8_t size)
{
    switch (size) {
        case sizeof(uint64_t) : {
            LONGLONG old = *(LONGLONG *)object;
            *(LONGLONG *)expected = _InterlockedCompareExchange64((LONGLONG *)object, *(LONGLONG *)&desired, *(LONGLONG *)expected);
            return *(LONGLONG *)expected == old;
            break;
        }
        case sizeof(uint32_t) : {
            LONG old = *(LONG *)object;
            *(LONG *)expected = _InterlockedCompareExchange((LONG *)object, *(LONG *)&desired, *(LONG *)expected);
            return *(LONG *)expected == old;
            break;
        }
        case sizeof(uint16_t) : {
            SHORT old = *(SHORT *)object;
            *(SHORT *)expected = _InterlockedCompareExchange16((SHORT *)object, *(SHORT *)&desired, *(SHORT *)expected);
            return *(SHORT *)expected == old;
            break;
        }
        case sizeof(uint8_t) : {
            char old = *(char *)object;
            *(char *)expected = _InterlockedCompareExchange8((char *)object, *(char *)&desired, *(char *)expected);
            return *(char *)expected == old;
            break;
        }
        default:
            exit(-1);
            break;
    }
}
#define atomic_compare_exchange_strong(object, expected, desired) \
    _atomic_compare_exchange(object, expected, desired, sizeof(*(object)))

#define atomic_compare_exchange_weak(object, expected, desired) \
    atomic_compare_exchange_strong(object, expected, desired)

#define atomic_compare_exchange_strong_explicit(object, expected, desired, success, failure) \
    atomic_compare_exchange_strong(object, expected, desired)

#define atomic_compare_exchange_weak_explicit(object, expected, desired, success, failure) \
    atomic_compare_exchange_weak(object, expected, desired)

#define atomic_exchange(object, operand)                                                               \
    ((sizeof(*(object)) == sizeof(uint64_t)) ? _InterlockedExchange64((LONGLONG *)(object), operand) :       \
    ((sizeof(*(object)) == sizeof(uint32_t)) ? _InterlockedExchange((LONG *)(object), operand) :             \
    ((sizeof(*(object)) == sizeof(uint16_t)) ? _InterlockedExchange16((SHORT *)(object), operand) :          \
    ((sizeof(*(object)) == sizeof(uint8_t)) ? _InterlockedExchange8((char *)(object), operand) : NULL))))

#define atomic_exchange_explicit(object, desired, order) \
    atomic_exchange(object, desired)

#define atomic_fetch_add(object, operand)                                                               \
    ((sizeof(*(object)) == sizeof(uint64_t)) ? _InterlockedExchangeAdd64((LONGLONG *)(object), operand) :       \
    ((sizeof(*(object)) == sizeof(uint32_t)) ? _InterlockedExchangeAdd((LONG *)(object), operand) :             \
    ((sizeof(*(object)) == sizeof(uint16_t)) ? _InterlockedExchangeAdd16((SHORT *)(object), operand) :          \
    ((sizeof(*(object)) == sizeof(uint8_t)) ? _InterlockedExchangeAdd8((char *)(object), operand) : NULL))))

#define atomic_fetch_add_explicit(object, operand, order) \
    atomic_fetch_add(object, operand)

#define atomic_fetch_sub(object, operand) \
    atomic_fetch_add(object, -(operand))

#define atomic_fetch_sub_explicit(object, operand, order) \
    atomic_fetch_sub(object, operand)

#define atomic_fetch_and(object, operand)                                                               \
    ((sizeof(*(object)) == sizeof(uint64_t)) ? _InterlockedAnd64((LONGLONG *)(object), operand) :       \
    ((sizeof(*(object)) == sizeof(uint32_t)) ? _InterlockedAnd((LONG *)(object), operand) :             \
    ((sizeof(*(object)) == sizeof(uint16_t)) ? _InterlockedAnd16((SHORT *)(object), operand) :          \
    ((sizeof(*(object)) == sizeof(uint8_t)) ? _InterlockedAnd8((char *)(object), operand) : NULL))))

#define atomic_fetch_and_explicit(object, operand, order) \
    atomic_fetch_and(object, operand)

#define atomic_fetch_or(object, operand)                                                               \
    ((sizeof(*(object)) == sizeof(uint64_t)) ? _InterlockedOr64((LONGLONG *)(object), operand) :       \
    ((sizeof(*(object)) == sizeof(uint32_t)) ? _InterlockedOr((LONG *)(object), operand) :             \
    ((sizeof(*(object)) == sizeof(uint16_t)) ? _InterlockedOr16((SHORT *)(object), operand) :          \
    ((sizeof(*(object)) == sizeof(uint8_t)) ? _InterlockedOr8((char *)(object), operand) : NULL))))

#define atomic_fetch_or_explicit(object, operand, order) \
    atomic_fetch_or(object, operand)

#define atomic_fetch_xor(object, operand)                                                               \
    ((sizeof(*(object)) == sizeof(uint64_t)) ? _InterlockedXor64((LONGLONG *)(object), operand) :       \
    ((sizeof(*(object)) == sizeof(uint32_t)) ? _InterlockedXor((LONG *)(object), operand) :             \
    ((sizeof(*(object)) == sizeof(uint16_t)) ? _InterlockedXor16((SHORT *)(object), operand) :          \
    ((sizeof(*(object)) == sizeof(uint8_t)) ? _InterlockedXor8((char *)(object), operand) : NULL))))

#define atomic_fetch_xor_explicit(object, operand, order) \
    atomic_fetch_xor(object, operand)

#define atomic_load(object) \
    (MemoryBarrier(), *(object))

#define atomic_load_explicit(object, order) \
    atomic_load(object)

#define atomic_store(object, desired) \
do {                                  \
    MemoryBarrier();                  \
    *(object) = (desired);            \
    MemoryBarrier();                  \
} while (0)

#define atomic_store_explicit(object, desired, order) \
    atomic_store(object, desired)

typedef atomic_bool atomic_flag;

#define ATOMIC_FLAG_INIT ATOMIC_VAR_INIT(0)

#define atomic_flag_test_and_set(object) \
    atomic_exchange(object, 1)

#define atomic_flag_test_and_set_explicit(object, order) \
    atomic_flag_test_and_set(object)

#define atomic_flag_clear(object) \
    atomic_store(object, 0)

#define atomic_flag_clear_explicit(object, order) \
    atomic_flag_clear(object)

#endif /* _MSC_VER */

#endif /* SMP_STDATOMIC_H */
