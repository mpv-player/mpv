/*
 * Copyright (c) 2012 Rudolf Polzer <divVerent@xonotic.org>
 *
 * This file is part of mpv's vf_dlopen examples.
 *
 * mpv's vf_dlopen examples are free software; you can redistribute them and/or
 * modify them under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * mpv's vf_dlopen examples are distributed in the hope that they will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv's vf_dlopen examples; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <assert.h>
#include <string.h>

#include "filterutils.h"

void copy_plane(
    unsigned char *dest, unsigned dest_stride,
    const unsigned char *src, unsigned src_stride,
    unsigned length,
    unsigned rows
    )
{
    unsigned i;
    assert(dest_stride >= length);
    assert(src_stride >= length);
    for (i = 0; i < rows; ++i)
        memcpy(&dest[dest_stride * i], &src[src_stride * i], length);
}

