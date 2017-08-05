/*
 * This file is part of mpv.
 * Parts based on MPlayer code by Reimar Döffinger.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <libavutil/sha.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/mem.h>

#include "osdep/io.h"

#include "common/common.h"
#include "options/path.h"
#include "stream/stream.h"
#include "formats.h"
#include "ra_gl.h"
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

void gl_check_error(GL *gl, struct mp_log *log, const char *info)
{
    for (;;) {
        GLenum error = gl->GetError();
        if (error == GL_NO_ERROR)
            break;
        mp_msg(log, MSGL_ERR, "%s: OpenGL error %s.\n", info,
               gl_error_to_string(error));
    }
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
void gl_upload_tex(GL *gl, GLenum target, GLenum format, GLenum type,
                   const void *dataptr, int stride,
                   int x, int y, int w, int h)
{
    int bpp = gl_bytes_per_pixel(format, type);
    const uint8_t *data = dataptr;
    int y_max = y + h;
    if (w <= 0 || h <= 0 || !bpp)
        return;
    if (stride < 0) {
        data += (h - 1) * stride;
        stride = -stride;
    }
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(stride));
    int slice = h;
    if (gl->mpgl_caps & MPGL_CAP_ROW_LENGTH) {
        // this is not always correct, but should work for MPlayer
        gl->PixelStorei(GL_UNPACK_ROW_LENGTH, stride / bpp);
    } else {
        if (stride != bpp * w)
            slice = 1; // very inefficient, but at least it works
    }
    for (; y + slice <= y_max; y += slice) {
        gl->TexSubImage2D(target, 0, x, y, w, slice, format, type, data);
        data += stride * slice;
    }
    if (y < y_max)
        gl->TexSubImage2D(target, 0, x, y, w, y_max - y, format, type, data);
    if (gl->mpgl_caps & MPGL_CAP_ROW_LENGTH)
        gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

mp_image_t *gl_read_fbo_contents(GL *gl, int fbo, int w, int h)
{
    if (gl->es)
        return NULL; // ES can't read from front buffer
    mp_image_t *image = mp_image_alloc(IMGFMT_RGB24, w, h);
    if (!image)
        return NULL;
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLenum obj = fbo ? GL_COLOR_ATTACHMENT0 : GL_FRONT;
    gl->PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl->ReadBuffer(obj);
    //flip image while reading (and also avoid stride-related trouble)
    for (int y = 0; y < h; y++) {
        gl->ReadPixels(0, h - y - 1, w, 1, GL_RGB, GL_UNSIGNED_BYTE,
                       image->planes[0] + y * image->stride[0]);
    }
    gl->PixelStorei(GL_PACK_ALIGNMENT, 4);
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
    return image;
}

static void gl_vao_enable_attribs(struct gl_vao *vao)
{
    GL *gl = vao->gl;

    for (int n = 0; n < vao->num_entries; n++) {
        const struct ra_renderpass_input *e = &vao->entries[n];
        GLenum type = 0;
        bool normalized = false;
        switch (e->type) {
        case RA_VARTYPE_FLOAT:
            type = GL_FLOAT;
            break;
        case RA_VARTYPE_BYTE_UNORM:
            type = GL_UNSIGNED_BYTE;
            normalized = true;
            break;
        default:
            abort();
        }
        assert(e->dim_m == 1);

        gl->EnableVertexAttribArray(n);
        gl->VertexAttribPointer(n, e->dim_v, type, normalized,
                                vao->stride, (void *)(intptr_t)e->binding);
    }
}

void gl_vao_init(struct gl_vao *vao, GL *gl, int stride,
                 const struct ra_renderpass_input *entries,
                 int num_entries)
{
    assert(!vao->vao);
    assert(!vao->buffer);

    *vao = (struct gl_vao){
        .gl = gl,
        .stride = stride,
        .entries = entries,
        .num_entries = num_entries,
    };

    gl->GenBuffers(1, &vao->buffer);

    if (gl->BindVertexArray) {
        gl->BindBuffer(GL_ARRAY_BUFFER, vao->buffer);

        gl->GenVertexArrays(1, &vao->vao);
        gl->BindVertexArray(vao->vao);
        gl_vao_enable_attribs(vao);
        gl->BindVertexArray(0);

        gl->BindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void gl_vao_uninit(struct gl_vao *vao)
{
    GL *gl = vao->gl;
    if (!gl)
        return;

    if (gl->DeleteVertexArrays)
        gl->DeleteVertexArrays(1, &vao->vao);
    gl->DeleteBuffers(1, &vao->buffer);

    *vao = (struct gl_vao){0};
}

static void gl_vao_bind(struct gl_vao *vao)
{
    GL *gl = vao->gl;

    if (gl->BindVertexArray) {
        gl->BindVertexArray(vao->vao);
    } else {
        gl->BindBuffer(GL_ARRAY_BUFFER, vao->buffer);
        gl_vao_enable_attribs(vao);
        gl->BindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

static void gl_vao_unbind(struct gl_vao *vao)
{
    GL *gl = vao->gl;

    if (gl->BindVertexArray) {
        gl->BindVertexArray(0);
    } else {
        for (int n = 0; vao->entries[n].name; n++)
            gl->DisableVertexAttribArray(n);
    }
}

// Draw the vertex data (as described by the gl_vao_entry entries) in ptr
// to the screen. num is the number of vertexes. prim is usually GL_TRIANGLES.
// If ptr is NULL, then skip the upload, and use the data uploaded with the
// previous call.
void gl_vao_draw_data(struct gl_vao *vao, GLenum prim, void *ptr, size_t num)
{
    GL *gl = vao->gl;

    if (ptr) {
        gl->BindBuffer(GL_ARRAY_BUFFER, vao->buffer);
        gl->BufferData(GL_ARRAY_BUFFER, num * vao->stride, ptr, GL_STREAM_DRAW);
        gl->BindBuffer(GL_ARRAY_BUFFER, 0);
    }

    gl_vao_bind(vao);

    gl->DrawArrays(prim, 0, num);

    gl_vao_unbind(vao);
}

static void GLAPIENTRY gl_debug_cb(GLenum source, GLenum type, GLuint id,
                                   GLenum severity, GLsizei length,
                                   const GLchar *message, const void *userParam)
{
    // keep in mind that the debug callback can be asynchronous
    struct mp_log *log = (void *)userParam;
    int level = MSGL_ERR;
    switch (severity) {
    case GL_DEBUG_SEVERITY_NOTIFICATION:level = MSGL_V; break;
    case GL_DEBUG_SEVERITY_LOW:         level = MSGL_INFO; break;
    case GL_DEBUG_SEVERITY_MEDIUM:      level = MSGL_WARN; break;
    case GL_DEBUG_SEVERITY_HIGH:        level = MSGL_ERR; break;
    }
    mp_msg(log, level, "GL: %s\n", message);
}

void gl_set_debug_logger(GL *gl, struct mp_log *log)
{
    if (gl->DebugMessageCallback)
        gl->DebugMessageCallback(log ? gl_debug_cb : NULL, log);
}

// Upload a texture, going through a PBO. PBO supposedly can facilitate
// asynchronous copy from CPU to GPU, so this is an optimization. Note that
// changing format/type/tex_w/tex_h or reusing the PBO in the same frame can
// ruin performance.
// This call is like gl_upload_tex(), plus PBO management/use.
// target, format, type, dataptr, stride, x, y, w, h: texture upload params
//                                                    (see gl_upload_tex())
// tex_w, tex_h: maximum size of the used texture
// use_pbo: for convenience, if false redirects the call to gl_upload_tex
void gl_pbo_upload_tex(struct gl_pbo_upload *pbo, GL *gl, bool use_pbo,
                       GLenum target, GLenum format, GLenum type,
                       int tex_w, int tex_h, const void *dataptr, int stride,
                       int x, int y, int w, int h)
{
    assert(x >= 0 && y >= 0 && w >= 0 && h >= 0);
    assert(x + w <= tex_w && y + h <= tex_h);

    if (!use_pbo) {
        gl_upload_tex(gl, target, format, type, dataptr, stride, x, y, w, h);
        return;
    }

    // We align the buffer size to 4096 to avoid possible subregion
    // dependencies. This is not a strict requirement (the spec requires no
    // alignment), but a good precaution for performance reasons
    size_t needed_size = stride * h;
    size_t buffer_size = MP_ALIGN_UP(needed_size, 4096);

    if (buffer_size != pbo->buffer_size)
        gl_pbo_upload_uninit(pbo);

    if (!pbo->buffer) {
        pbo->gl = gl;
        pbo->buffer_size = buffer_size;
        gl->GenBuffers(1, &pbo->buffer);
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo->buffer);
        // Magic time: Because we memcpy once from RAM to the buffer, and then
        // the GPU needs to read from this anyway, we actually *don't* want
        // this buffer to be allocated in RAM. If we allocate it in VRAM
        // instead, we can reduce this to a single copy: from RAM into VRAM.
        // Unfortunately, drivers e.g. nvidia will think GL_STREAM_DRAW is best
        // allocated on host memory instead of device memory, so we lie about
        // the usage to fool the driver into giving us a buffer in VRAM instead
        // of RAM, which can be significantly faster for our use case.
        // Seriously, fuck OpenGL.
        gl->BufferData(GL_PIXEL_UNPACK_BUFFER, NUM_PBO_BUFFERS * buffer_size,
                       NULL, GL_STREAM_COPY);
    }

    uintptr_t offset = buffer_size * pbo->index;
    pbo->index = (pbo->index + 1) % NUM_PBO_BUFFERS;

    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo->buffer);
    gl->BufferSubData(GL_PIXEL_UNPACK_BUFFER, offset, needed_size, dataptr);
    gl_upload_tex(gl, target, format, type, (void *)offset, stride, x, y, w, h);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void gl_pbo_upload_uninit(struct gl_pbo_upload *pbo)
{
    if (pbo->gl)
        pbo->gl->DeleteBuffers(1, &pbo->buffer);

    *pbo = (struct gl_pbo_upload){0};
}

// The intention is to return the actual depth of any fixed point 16 bit
// textures. (Actually tests only 1 format - hope that is good enough.)
int gl_determine_16bit_tex_depth(GL *gl)
{
    const struct gl_format *fmt = gl_find_unorm_format(gl, 2, 1);
    if (!gl->GetTexLevelParameteriv || !fmt) {
        // ANGLE supports ES 3.0 and the extension, but lacks the function above.
        if (gl->mpgl_caps & MPGL_CAP_EXT16)
            return 16;
        return -1;
    }

    GLuint tex;
    gl->GenTextures(1, &tex);
    gl->BindTexture(GL_TEXTURE_2D, tex);
    gl->TexImage2D(GL_TEXTURE_2D, 0, fmt->internal_format, 64, 64, 0,
                   fmt->format, fmt->type, NULL);
    GLenum pname = 0;
    switch (fmt->format) {
    case GL_RED:        pname = GL_TEXTURE_RED_SIZE; break;
    case GL_LUMINANCE:  pname = GL_TEXTURE_LUMINANCE_SIZE; break;
    }
    GLint param = -1;
    if (pname)
        gl->GetTexLevelParameteriv(GL_TEXTURE_2D, 0, pname, &param);
    gl->DeleteTextures(1, &tex);
    return param;
}

int gl_get_fb_depth(GL *gl, int fbo)
{
    if ((gl->es < 300 && !gl->version) || !(gl->mpgl_caps & MPGL_CAP_FB))
        return -1;

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLenum obj = gl->version ? GL_BACK_LEFT : GL_BACK;
    if (fbo)
        obj = GL_COLOR_ATTACHMENT0;

    GLint depth_g = -1;

    gl->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, obj,
                            GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &depth_g);

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    return depth_g > 0 ? depth_g : -1;
}
