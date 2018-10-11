/*
 * MSVC libgen.h compatibility header.
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

#ifndef SMP_LIBGEN_H
#define SMP_LIBGEN_H

#ifndef _MSC_VER
#   include_next <libgen.h>
#else

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static __inline const char *basename(const char *path)
{
    char *p = strrchr(path, '/');

    //Handle DOS paths
    char *q = strrchr(path, '\\');
    char *d = strchr(path, ':');

    p = (p > q) ? p : q;
    p = (p > d) ? p : d;

    if (!p)
        return path;

    return p + 1;
}

static __inline const char *dirname(char *path)
{
    char *p = strrchr(path, '/');

    //Handle DOS paths
    char *q = strrchr(path, '\\');
    char *d = strchr(path, ':');

    d = d ? d + 1 : d;

    p = (p > q) ? p : q;
    p = (p > d) ? p : d;

    if (!p)
        return ".";

    *p = '\0';

    return path;
}

#endif /* _MSC_VER */

#endif /* SMP_LIBGEN_H */