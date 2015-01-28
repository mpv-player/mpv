/*
 * This file is part of mpv.
 * Parts based on MPlayer code by Reimar Döffinger.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
#include "gl_utils.h"

// GLU has this as gluErrorString (we don't use GLU, as it is legacy-OpenGL)
static const char *gl_error_to_string(GLenum error)
{
    switch (error) {
    case GL_INVALID_ENUM: return "INVALID_ENUM";
    case GL_INVALID_VALUE: return "INVALID_VALUE";
    case GL_INVALID_OPERATION: return "INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
    default: return "unknown";
    }
}

void glCheckError(GL *gl, struct mp_log *log, const char *info)
{
    for (;;) {
        GLenum error = gl->GetError();
        if (error == GL_NO_ERROR)
            break;
        mp_msg(log, MSGL_ERR, "%s: OpenGL error %s.\n", info,
               gl_error_to_string(error));
    }
}

// return the number of bytes per pixel for the given format
// does not handle all possible variants, just those used by mpv
int glFmt2bpp(GLenum format, GLenum type)
{
    int component_size = 0;
    switch (type) {
    case GL_UNSIGNED_BYTE_3_3_2:
    case GL_UNSIGNED_BYTE_2_3_3_REV:
        return 1;
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
        return 2;
    case GL_UNSIGNED_BYTE:
        component_size = 1;
        break;
    case GL_UNSIGNED_SHORT:
        component_size = 2;
        break;
    }
    switch (format) {
    case GL_LUMINANCE:
    case GL_ALPHA:
        return component_size;
    case GL_RGB_422_APPLE:
        return 2;
    case GL_RGB:
    case GL_BGR:
    case GL_RGB_INTEGER:
        return 3 * component_size;
    case GL_RGBA:
    case GL_BGRA:
    case GL_RGBA_INTEGER:
        return 4 * component_size;
    case GL_RED:
    case GL_RED_INTEGER:
        return component_size;
    case GL_RG:
    case GL_LUMINANCE_ALPHA:
    case GL_RG_INTEGER:
        return 2 * component_size;
    }
    abort(); // unknown
}

static int get_alignment(int stride)
{
    if (stride % 8 == 0)
        return 8;
    if (stride % 4 == 0)
        return 4;
    if (stride % 2 == 0)
        return 2;
    return 1;
}

// upload a texture, handling things like stride and slices
//  target: texture target, usually GL_TEXTURE_2D
//  format, type: texture parameters
//  dataptr, stride: image data
//  x, y, width, height: part of the image to upload
//  slice: height of an upload slice, 0 for all at once
void glUploadTex(GL *gl, GLenum target, GLenum format, GLenum type,
                 const void *dataptr, int stride,
                 int x, int y, int w, int h, int slice)
{
    const uint8_t *data = dataptr;
    int y_max = y + h;
    if (w <= 0 || h <= 0)
        return;
    if (slice <= 0)
        slice = h;
    if (stride < 0) {
        data += (h - 1) * stride;
        stride = -stride;
    }
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(stride));
    bool use_rowlength = slice > 1 && (gl->mpgl_caps & MPGL_CAP_ROW_LENGTH);
    if (use_rowlength) {
        // this is not always correct, but should work for MPlayer
        gl->PixelStorei(GL_UNPACK_ROW_LENGTH, stride / glFmt2bpp(format, type));
    } else {
        if (stride != glFmt2bpp(format, type) * w)
            slice = 1; // very inefficient, but at least it works
    }
    for (; y + slice <= y_max; y += slice) {
        gl->TexSubImage2D(target, 0, x, y, w, slice, format, type, data);
        data += stride * slice;
    }
    if (y < y_max)
        gl->TexSubImage2D(target, 0, x, y, w, y_max - y, format, type, data);
    if (use_rowlength)
        gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

// Like glUploadTex, but upload a byte array with all elements set to val.
// If scratch is not NULL, points to a resizeable talloc memory block than can
// be freely used by the function (for avoiding temporary memory allocations).
void glClearTex(GL *gl, GLenum target, GLenum format, GLenum type,
                int x, int y, int w, int h, uint8_t val, void **scratch)
{
    int bpp = glFmt2bpp(format, type);
    int stride = w * bpp;
    int size = h * stride;
    if (size < 1)
        return;
    void *data = scratch ? *scratch : NULL;
    if (talloc_get_size(data) < size)
        data = talloc_realloc(NULL, data, char *, size);
    memset(data, val, size);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(stride));
    gl->TexSubImage2D(target, 0, x, y, w, h, format, type, data);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (scratch) {
        *scratch = data;
    } else {
        talloc_free(data);
    }
}

mp_image_t *glGetWindowScreenshot(GL *gl)
{
    if (gl->es)
        return NULL; // ES can't read from front buffer
    GLint vp[4]; //x, y, w, h
    gl->GetIntegerv(GL_VIEWPORT, vp);
    mp_image_t *image = mp_image_alloc(IMGFMT_RGB24, vp[2], vp[3]);
    if (!image)
        return NULL;
    gl->PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl->ReadBuffer(GL_FRONT);
    //flip image while reading (and also avoid stride-related trouble)
    for (int y = 0; y < vp[3]; y++) {
        gl->ReadPixels(vp[0], vp[1] + vp[3] - y - 1, vp[2], 1,
                       GL_RGB, GL_UNSIGNED_BYTE,
                       image->planes[0] + y * image->stride[0]);
    }
    gl->PixelStorei(GL_PACK_ALIGNMENT, 4);
    return image;
}

void mp_log_source(struct mp_log *log, int lev, const char *src)
{
    int line = 1;
    if (!src)
        return;
    while (*src) {
        const char *end = strchr(src, '\n');
        const char *next = end + 1;
        if (!end)
            next = end = src + strlen(src);
        mp_msg(log, lev, "[%3d] %.*s\n", line, (int)(end - src), src);
        line++;
        src = next;
    }
}
