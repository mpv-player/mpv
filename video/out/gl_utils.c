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
#include <stdarg.h>
#include <assert.h>

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

static void gl_vao_enable_attribs(struct gl_vao *vao)
{
    GL *gl = vao->gl;

    for (int n = 0; vao->entries[n].name; n++) {
        const struct gl_vao_entry *e = &vao->entries[n];

        gl->EnableVertexAttribArray(n);
        gl->VertexAttribPointer(n, e->num_elems, e->type, e->normalized,
                                vao->stride, (void *)(intptr_t)e->offset);
    }
}

void gl_vao_init(struct gl_vao *vao, GL *gl, int stride,
                 const struct gl_vao_entry *entries)
{
    assert(!vao->vao);
    assert(!vao->buffer);

    *vao = (struct gl_vao){
        .gl = gl,
        .stride = stride,
        .entries = entries,
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

void gl_vao_bind(struct gl_vao *vao)
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

void gl_vao_unbind(struct gl_vao *vao)
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
        gl->BufferData(GL_ARRAY_BUFFER, num * vao->stride, ptr, GL_DYNAMIC_DRAW);
        gl->BindBuffer(GL_ARRAY_BUFFER, 0);
    }

    gl_vao_bind(vao);

    gl->DrawArrays(prim, 0, num);

    gl_vao_unbind(vao);
}

// Create a texture and a FBO using the texture as color attachments.
//  iformat: texture internal format
// Returns success.
bool fbotex_init(struct fbotex *fbo, GL *gl, struct mp_log *log, int w, int h,
                 GLenum iformat)
{
    assert(!fbo->fbo);
    assert(!fbo->texture);
    return fbotex_change(fbo, gl, log, w, h, iformat, 0);
}

// Like fbotex_init(), except it can be called on an already initialized FBO;
// and if the parameters are the same as the previous call, do not touch it.
// flags can be 0, or a combination of FBOTEX_FUZZY_W and FBOTEX_FUZZY_H.
// Enabling FUZZY for W or H means the w or h does not need to be exact.
bool fbotex_change(struct fbotex *fbo, GL *gl, struct mp_log *log, int w, int h,
                   GLenum iformat, int flags)
{
    bool res = true;

    int cw = w, ch = h;

    if ((flags & FBOTEX_FUZZY_W) && cw < fbo->tex_w)
        cw = fbo->tex_w;
    if ((flags & FBOTEX_FUZZY_H) && ch < fbo->tex_h)
        ch = fbo->tex_h;

    if (fbo->tex_w == cw && fbo->tex_h == ch && fbo->iformat == iformat)
        return true;

    if (flags & FBOTEX_FUZZY_W)
        w = MP_ALIGN_UP(w, 256);
    if (flags & FBOTEX_FUZZY_H)
        h = MP_ALIGN_UP(h, 256);

    GLenum filter = fbo->tex_filter;

    *fbo = (struct fbotex) {
        .gl = gl,
        .tex_w = w,
        .tex_h = h,
        .iformat = iformat,
    };

    mp_verbose(log, "Create FBO: %dx%d\n", fbo->tex_w, fbo->tex_h);

    if (!(gl->mpgl_caps & MPGL_CAP_FB))
        return false;

    gl->GenFramebuffers(1, &fbo->fbo);
    gl->GenTextures(1, &fbo->texture);
    gl->BindTexture(GL_TEXTURE_2D, fbo->texture);
    gl->TexImage2D(GL_TEXTURE_2D, 0, iformat, fbo->tex_w, fbo->tex_h, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    fbotex_set_filter(fbo, filter ? filter : GL_LINEAR);

    glCheckError(gl, log, "after creating framebuffer texture");

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, fbo->texture, 0);

    GLenum err = gl->CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (err != GL_FRAMEBUFFER_COMPLETE) {
        mp_err(log, "Error: framebuffer completeness check failed (error=%d).\n",
               (int)err);
        res = false;
    }

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    glCheckError(gl, log, "after creating framebuffer");

    return res;
}

void fbotex_set_filter(struct fbotex *fbo, GLenum tex_filter)
{
    GL *gl = fbo->gl;

    if (fbo->tex_filter != tex_filter && fbo->texture) {
        gl->BindTexture(GL_TEXTURE_2D, fbo->texture);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_filter);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_filter);
        gl->BindTexture(GL_TEXTURE_2D, 0);
    }
    fbo->tex_filter = tex_filter;
}

void fbotex_uninit(struct fbotex *fbo)
{
    GL *gl = fbo->gl;

    if (gl && (gl->mpgl_caps & MPGL_CAP_FB)) {
        gl->DeleteFramebuffers(1, &fbo->fbo);
        gl->DeleteTextures(1, &fbo->texture);
        *fbo = (struct fbotex) {0};
    }
}

// Standard parallel 2D projection, except y1 < y0 means that the coordinate
// system is flipped, not the projection.
void gl_transform_ortho(struct gl_transform *t, float x0, float x1,
                        float y0, float y1)
{
    if (y1 < y0) {
        float tmp = y0;
        y0 = tmp - y1;
        y1 = tmp;
    }

    t->m[0][0] = 2.0f / (x1 - x0);
    t->m[0][1] = 0.0f;
    t->m[1][0] = 0.0f;
    t->m[1][1] = 2.0f / (y1 - y0);
    t->t[0] = -(x1 + x0) / (x1 - x0);
    t->t[1] = -(y1 + y0) / (y1 - y0);
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
    if (gl->DebugMessageCallback) {
        if (log) {
            gl->DebugMessageCallback(gl_debug_cb, log);
        } else {
            gl->DebugMessageCallback(NULL, NULL);
        }
    }
}

#define SC_ENTRIES 16
#define SC_UNIFORM_ENTRIES 20

enum uniform_type {
    UT_invalid,
    UT_i,
    UT_f,
    UT_m,
};

struct sc_uniform {
    char *name;
    enum uniform_type type;
    const char *glsl_type;
    int size;
    GLint loc;
    union {
        GLfloat f[9];
        GLint i[4];
    } v;
};

struct sc_entry {
    GLuint gl_shader;
    // the following fields define the shader's contents
    char *key; // vertex+frag shader (mangled)
    struct gl_vao *vao;
};

struct gl_shader_cache {
    GL *gl;
    struct mp_log *log;

    // this is modified during use (gl_sc_add() etc.)
    char *text;
    struct gl_vao *vao;

    struct sc_entry entries[SC_ENTRIES];
    int num_entries;

    struct sc_uniform uniforms[SC_UNIFORM_ENTRIES];
    int num_uniforms;
};

struct gl_shader_cache *gl_sc_create(GL *gl, struct mp_log *log)
{
    struct gl_shader_cache *sc = talloc_ptrtype(NULL, sc);
    *sc = (struct gl_shader_cache){
        .gl = gl,
        .log = log,
        .text = talloc_strdup(sc, ""),
    };
    return sc;
}

void gl_sc_reset(struct gl_shader_cache *sc)
{
    sc->text[0] = '\0';
    for (int n = 0; n < sc->num_uniforms; n++)
        talloc_free(sc->uniforms[n].name);
    sc->num_uniforms = 0;
}

static void sc_flush_cache(struct gl_shader_cache *sc)
{
    for (int n = 0; n < sc->num_entries; n++) {
        struct sc_entry *e = &sc->entries[n];
        sc->gl->DeleteProgram(e->gl_shader);
        talloc_free(e->key);
    }
    sc->num_entries = 0;
}

void gl_sc_destroy(struct gl_shader_cache *sc)
{
    gl_sc_reset(sc);
    sc_flush_cache(sc);
    talloc_free(sc);
}

void gl_sc_add(struct gl_shader_cache *sc, const char *text)
{
    sc->text = talloc_strdup_append(sc->text, text);
}

void gl_sc_addf(struct gl_shader_cache *sc, const char *textf, ...)
{
    va_list ap;
    va_start(ap, textf);
    ta_xvasprintf_append(&sc->text, textf, ap);
    va_end(ap);
}

static struct sc_uniform *find_uniform(struct gl_shader_cache *sc,
                                       const char *name)
{
    for (int n = 0; n < sc->num_uniforms; n++) {
        if (strcmp(sc->uniforms[n].name, name) == 0)
            return &sc->uniforms[n];
    }
    // not found -> add it
    assert(sc->num_uniforms < SC_UNIFORM_ENTRIES); // just don't have too many
    struct sc_uniform *new = &sc->uniforms[sc->num_uniforms++];
    *new = (struct sc_uniform) { .loc = -1, .name = talloc_strdup(NULL, name) };
    return new;
}

void gl_sc_uniform_sampler(struct gl_shader_cache *sc, char *name, GLenum target,
                           int unit)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_i;
    u->size = 1;
    switch (target) {
    case GL_TEXTURE_1D: u->glsl_type = "sampler1D"; break;
    case GL_TEXTURE_2D: u->glsl_type = "sampler2D"; break;
    case GL_TEXTURE_RECTANGLE: u->glsl_type = "sampler2DRect"; break;
    case GL_TEXTURE_3D: u->glsl_type = "sampler3D"; break;
    default: abort();
    }
    u->v.i[0] = unit;
}

void gl_sc_uniform_f(struct gl_shader_cache *sc, char *name, GLfloat f)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_f;
    u->size = 1;
    u->glsl_type = "float";
    u->v.f[0] = f;
}

void gl_sc_uniform_vec2(struct gl_shader_cache *sc, char *name, GLfloat f[2])
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_f;
    u->size = 2;
    u->glsl_type = "vec2";
    u->v.f[0] = f[0];
    u->v.f[1] = f[1];
}

void gl_sc_uniform_vec3(struct gl_shader_cache *sc, char *name, GLfloat f[3])
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_f;
    u->size = 3;
    u->glsl_type = "vec3";
    u->v.f[0] = f[0];
    u->v.f[1] = f[1];
    u->v.f[2] = f[2];
}

static void transpose2x2(float r[2 * 2])
{
    MPSWAP(float, r[0+2*1], r[1+2*0]);
}

void gl_sc_uniform_mat2(struct gl_shader_cache *sc, char *name,
                        bool transpose, GLfloat *v)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_m;
    u->size = 2;
    u->glsl_type = "mat2";
    for (int n = 0; n < 4; n++)
        u->v.f[n] = v[n];
    if (transpose)
        transpose2x2(&u->v.f[0]);
}

static void transpose3x3(float r[3 * 3])
{
    MPSWAP(float, r[0+3*1], r[1+3*0]);
    MPSWAP(float, r[0+3*2], r[2+3*0]);
    MPSWAP(float, r[1+3*2], r[2+3*1]);
}

void gl_sc_uniform_mat3(struct gl_shader_cache *sc, char *name,
                        bool transpose, GLfloat *v)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_m;
    u->size = 3;
    u->glsl_type = "mat3";
    for (int n = 0; n < 9; n++)
        u->v.f[n] = v[n];
    if (transpose)
        transpose3x3(&u->v.f[0]);
}

// This will call glBindAttribLocation() on the shader before it's linked
// (OpenGL requires this to happen before linking). Basically, it associates
// the input variable names with the fields in the vao.
// The vertex shader is setup such that the elements are available as fragment
// shader variables using the names in the vao entries, which "position" being
// set to gl_Position.
void gl_sc_set_vao(struct gl_shader_cache *sc, struct gl_vao *vao)
{
    sc->vao = vao;
}

static const char *vao_glsl_type(const struct gl_vao_entry *e)
{
    // pretty dumb... too dumb, but works for us
    switch (e->num_elems) {
    case 1: return "float";
    case 2: return "vec2";
    case 3: return "vec3";
    case 4: return "vec4";
    default: abort();
    }
}

// Assumes program is current (gl->UseProgram(program)).
static void update_uniform(GL *gl, GLuint program, struct sc_uniform *u)
{
    GLint loc = gl->GetUniformLocation(program, u->name);
    if (loc < 0)
        return;
    switch (u->type) {
    case UT_i:
        assert(u->size == 1);
        gl->Uniform1i(loc, u->v.i[0]);
        break;
    case UT_f:
        switch (u->size) {
        case 1: gl->Uniform1f(loc, u->v.f[0]); break;
        case 2: gl->Uniform2f(loc, u->v.f[0], u->v.f[1]); break;
        case 3: gl->Uniform3f(loc, u->v.f[0], u->v.f[1], u->v.f[2]); break;
        case 4: gl->Uniform4f(loc, u->v.f[0], u->v.f[1], u->v.f[2], u->v.f[3]); break;
        default: abort();
        }
        break;
    case UT_m:
        switch (u->size) {
        case 2: gl->UniformMatrix2fv(loc, 1, GL_FALSE, &u->v.f[0]); break;
        case 3: gl->UniformMatrix3fv(loc, 1, GL_FALSE, &u->v.f[0]); break;
        default: abort();
        }
        break;
    default:
        abort();
    }
}

static void compile_attach_shader(struct gl_shader_cache *sc, GLuint program,
                                  GLenum type, const char *source)
{
    GL *gl = sc->gl;

    GLuint shader = gl->CreateShader(type);
    gl->ShaderSource(shader, 1, &source, NULL);
    gl->CompileShader(shader);
    GLint status;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint log_length;
    gl->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

    int pri = status ? (log_length > 1 ? MSGL_V : MSGL_DEBUG) : MSGL_ERR;
    const char *typestr = type == GL_VERTEX_SHADER ? "vertex" : "fragment";
    if (mp_msg_test(sc->log, pri)) {
        MP_MSG(sc, pri, "%s shader source:\n", typestr);
        mp_log_source(sc->log, pri, source);
    }
    if (log_length > 1) {
        GLchar *logstr = talloc_zero_size(NULL, log_length + 1);
        gl->GetShaderInfoLog(shader, log_length, NULL, logstr);
        MP_MSG(sc, pri, "%s shader compile log (status=%d):\n%s\n",
               typestr, status, logstr);
        talloc_free(logstr);
    }

    gl->AttachShader(program, shader);
    gl->DeleteShader(shader);
}

static void link_shader(struct gl_shader_cache *sc, GLuint program)
{
    GL *gl = sc->gl;
    gl->LinkProgram(program);
    GLint status;
    gl->GetProgramiv(program, GL_LINK_STATUS, &status);
    GLint log_length;
    gl->GetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

    int pri = status ? (log_length > 1 ? MSGL_V : MSGL_DEBUG) : MSGL_ERR;
    if (mp_msg_test(sc->log, pri)) {
        GLchar *logstr = talloc_zero_size(NULL, log_length + 1);
        gl->GetProgramInfoLog(program, log_length, NULL, logstr);
        MP_MSG(sc, pri, "shader link log (status=%d): %s\n", status, logstr);
        talloc_free(logstr);
    }
}

static GLuint create_program(struct gl_shader_cache *sc, const char *vertex,
                             const char *frag)
{
    GL *gl = sc->gl;
    MP_VERBOSE(sc, "recompiling a shader program:\n");
    mp_log_source(sc->log, MSGL_V, sc->text);
    GLuint prog = gl->CreateProgram();
    compile_attach_shader(sc, prog, GL_VERTEX_SHADER, vertex);
    compile_attach_shader(sc, prog, GL_FRAGMENT_SHADER, frag);
    for (int n = 0; sc->vao->entries[n].name; n++) {
        char vname[80];
        snprintf(vname, sizeof(vname), "vertex_%s", sc->vao->entries[n].name);
        gl->BindAttribLocation(prog, n, vname);
    }
    link_shader(sc, prog);
    return prog;
}

#define ADD(x, ...) (x) = talloc_asprintf_append(x, __VA_ARGS__)

// 1. Generate vertex and fragment shaders from the fragment shader text added
//    with gl_sc_add(). The generated shader program is cached (based on the
//    text), so actual compilation happens only the first time.
// 2. Update the uniforms set with gl_sc_uniform_*.
// 3. Make the new shader program current (glUseProgram()).
// 4. Reset the sc state and prepare for a new shader program. (All uniforms
//    and fragment operations needed for the next program have to be re-added.)
void gl_sc_gen_shader_and_reset(struct gl_shader_cache *sc)
{
    GL *gl = sc->gl;
    void *tmp = talloc_new(NULL);

    assert(sc->vao);

    // set up shader text (header + uniforms + body)
    char *header = talloc_asprintf(tmp, "#version %d%s\n", gl->glsl_version,
                                   gl->es >= 300 ? " es" : "");
    if (gl->es)
        ADD(header, "precision mediump float;\n");
    char *vert_in = gl->glsl_version >= 130 ? "in" : "attribute";
    char *vert_out = gl->glsl_version >= 130 ? "out" : "varying";
    char *frag_in = gl->glsl_version >= 130 ? "in" : "varying";

    // vertex shader: we don't use the vertex shader, so just setup a dummy,
    // which passes through the vertex array attributes.
    char *vert_head = talloc_strdup(tmp, header);
    char *vert_body = talloc_strdup(tmp, "void main() {\n");
    char *frag_vaos = talloc_strdup(tmp, "");
    for (int n = 0; sc->vao->entries[n].name; n++) {
        const struct gl_vao_entry *e = &sc->vao->entries[n];
        const char *glsl_type = vao_glsl_type(e);
        if (strcmp(e->name, "position") == 0) {
            // setting raster pos. requires setting gl_Position magic variable
            assert(e->num_elems == 2 && e->type == GL_FLOAT);
            ADD(vert_head, "%s vec2 position;\n", vert_in);
            ADD(vert_body, "gl_Position = vec4(position, 1.0, 1.0);\n");
        } else {
            ADD(vert_head, "%s %s vertex_%s;\n", vert_in, glsl_type, e->name);
            ADD(vert_head, "%s %s %s;\n", vert_out, glsl_type, e->name);
            ADD(vert_body, "%s = vertex_%s;\n", e->name, e->name);
            ADD(frag_vaos, "%s %s %s;\n", frag_in, glsl_type, e->name);
        }
    }
    ADD(vert_body, "}\n");
    char *vert = talloc_asprintf(tmp, "%s%s", vert_head, vert_body);

    // fragment shader; still requires adding used uniforms and VAO elements
    char *frag = talloc_strdup(tmp, header);
    ADD(frag, "#define RG %s\n", gl->mpgl_caps & MPGL_CAP_TEX_RG ? "rg" : "ra");
    if (gl->glsl_version >= 130) {
        ADD(frag, "#define texture1D texture\n");
        ADD(frag, "#define texture3D texture\n");
        ADD(frag, "out vec4 out_color;\n");
    } else {
        ADD(frag, "#define texture texture2D\n");
    }
    ADD(frag, "%s", frag_vaos);
    for (int n = 0; n < sc->num_uniforms; n++) {
        struct sc_uniform *u = &sc->uniforms[n];
        ADD(frag, "uniform %s %s;\n", u->glsl_type, u->name);
    }
    ADD(frag, "void main() {\n");
    ADD(frag, "%s", sc->text);
    // we require _all_ frag shaders to write to a "vec4 color"
    if (gl->glsl_version >= 130) {
        ADD(frag, "out_color = color;\n");
    } else {
        ADD(frag, "gl_FragColor = color;\n");
    }
    ADD(frag, "}\n");

    char *key = talloc_asprintf(tmp, "%s%s", vert, frag);
    struct sc_entry *entry = NULL;
    for (int n = 0; n < sc->num_entries; n++) {
        if (strcmp(key, sc->entries[n].key) == 0) {
            entry = &sc->entries[n];
            break;
        }
    }
    if (!entry) {
        if (sc->num_entries == SC_ENTRIES)
            sc_flush_cache(sc);
        entry = &sc->entries[sc->num_entries++];
        *entry = (struct sc_entry){.key = talloc_strdup(NULL, key)};
    }
    // build vertex shader from vao
    if (!entry->gl_shader)
        entry->gl_shader = create_program(sc, vert, frag);

    gl->UseProgram(entry->gl_shader);

    // For now we set the uniforms every time. This is probably bad, and we
    // should switch to caching them.
    for (int n = 0; n < sc->num_uniforms; n++)
        update_uniform(gl, entry->gl_shader, &sc->uniforms[n]);

    talloc_free(tmp);

    gl_sc_reset(sc);
}
