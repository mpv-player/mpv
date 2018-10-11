/*
 * MSVC stdio.h compatibility header.
 * Copyright (c) 2015 Matthew Oliver
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

#ifndef SMP_STDIO_H
#define SMP_STDIO_H

#ifndef _MSC_VER
#   include_next <stdio.h>
#else

#include <crtversion.h>
#if _VC_CRT_MAJOR_VERSION >= 14
#   include <../ucrt/stdio.h>
#else
#   include <../include/stdio.h>
#   define snprintf _snprintf
#endif

#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#ifndef SSIZE_MAX
#   define SSIZE_MAX ((ssize_t) (SIZE_MAX / 2))
#endif

static __inline ssize_t getdelim(char **lineptr, size_t *n, int delimiter, FILE *fp)
{
    ssize_t result;
    size_t cur_len = 0;

    if (lineptr == NULL || n == NULL || fp == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (*lineptr == NULL || *n == 0) {
        char *new_lineptr;
        *n = 120;
        new_lineptr = (char *)realloc(*lineptr, *n);
        if (new_lineptr == NULL) {
            result = -1;
            return result;
        }
        *lineptr = new_lineptr;
    }

    for (;;) {
        int i;
        i = getc(fp);
        if (i == EOF) {
            result = -1;
            break;
        }
        if (cur_len + 1 >= *n) {
            size_t needed_max = SSIZE_MAX < SIZE_MAX ? (size_t)SSIZE_MAX + 1 : SIZE_MAX;
            size_t needed = 2 * *n + 1;
            char *new_lineptr;
            if (needed_max < needed) {
                needed = needed_max;
            }
            if (cur_len + 1 >= needed) {
                result = -1;
                errno = EOVERFLOW;
                return result;
            }
            new_lineptr = (char *)realloc(*lineptr, needed);
            if (new_lineptr == NULL) {
                result = -1;
                return result;
            }
            *lineptr = new_lineptr;
            *n = needed;
        }
        (*lineptr)[cur_len] = i;
        cur_len++;
        if (i == delimiter) {
            break;
        }
    }
    (*lineptr)[cur_len] = '\0';
    result = cur_len ? cur_len : result;

    return result;
}

static __inline ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    return getdelim(lineptr, n, '\n', stream);
}

static __inline int vasprintf(char **res, char const *fmt, va_list args)
{
    int	sz, r;
    sz = _vscprintf(fmt, args);
    if (sz < 0) {
        return sz;
    }
    if (sz >= 0) {
        if ((*res = malloc(sz + 1)) == NULL) {
            return -1;
        }
        if ((sz = sprintf(*res, fmt, args)) < 0) {
            free(*res);
            *res = NULL;
        }
        return sz;
    }
#define MAXLN 65535
    * res = NULL;
    for (sz = 128; sz <= MAXLN; sz *= 2) {
        if ((*res = realloc(*res, sz)) == NULL) {
            return -1;
        }
        r = vsnprintf(*res, sz, fmt, args);
        if (r > 0 && r < sz) {
            return r;
        }
    }
    errno = ENOMEM;
    if (*res) {
        free(*res);
        *res = NULL;
    }
    return -1;
}

static __inline int asprintf(char **strp, const char *fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vasprintf(strp, fmt, ap);
    va_end(ap);
    return(r);
}

#define fseeko _fseeki64
#define ftello _ftelli64

#endif /* _MSC_VER */

#endif /* SMP_STDIO_H */