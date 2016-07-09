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

#include "common/common.h"
#include "formats.h"
#include "utils.h"

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

mp_image_t *gl_read_window_contents(GL *gl)
{
    if (gl->es)
        return NULL; // ES can't read from front buffer
    GLint vp[4]; //x, y, w, h
    gl->GetIntegerv(GL_VIEWPORT, vp);
    mp_image_t *image = mp_image_alloc(IMGFMT_RGB24, vp[2], vp[3]);
    if (!image)
        return NULL;
    gl->BindFramebuffer(GL_FRAMEBUFFER, gl->main_fb);
    GLenum obj = gl->main_fb ? GL_COLOR_ATTACHMENT0 : GL_FRONT;
    gl->PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl->ReadBuffer(obj);
    //flip image while reading (and also avoid stride-related trouble)
    for (int y = 0; y < vp[3]; y++) {
        gl->ReadPixels(vp[0], vp[1] + vp[3] - y - 1, vp[2], 1,
                       GL_RGB, GL_UNSIGNED_BYTE,
                       image->planes[0] + y * image->stride[0]);
    }
    gl->PixelStorei(GL_PACK_ALIGNMENT, 4);
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
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

    if ((flags & FBOTEX_FUZZY_W) && cw < fbo->rw)
        cw = fbo->rw;
    if ((flags & FBOTEX_FUZZY_H) && ch < fbo->rh)
        ch = fbo->rh;

    if (fbo->rw == cw && fbo->rh == ch && fbo->iformat == iformat) {
        fbo->lw = w;
        fbo->lh = h;
        fbotex_invalidate(fbo);
        return true;
    }

    int lw = w, lh = h;

    if (flags & FBOTEX_FUZZY_W)
        w = MP_ALIGN_UP(w, 256);
    if (flags & FBOTEX_FUZZY_H)
        h = MP_ALIGN_UP(h, 256);

    mp_verbose(log, "Create FBO: %dx%d (%dx%d)\n", lw, lh, w, h);

    const struct gl_format *format = gl_find_internal_format(gl, iformat);
    if (!format || (format->flags & F_CF) != F_CF) {
        mp_verbose(log, "Format 0x%x not supported.\n", (unsigned)iformat);
        return false;
    }
    assert(gl->mpgl_caps & MPGL_CAP_FB);

    GLenum filter = fbo->tex_filter;

    fbotex_uninit(fbo);

    *fbo = (struct fbotex) {
        .gl = gl,
        .rw = w,
        .rh = h,
        .lw = lw,
        .lh = lh,
        .iformat = iformat,
    };

    gl->GenFramebuffers(1, &fbo->fbo);
    gl->GenTextures(1, &fbo->texture);
    gl->BindTexture(GL_TEXTURE_2D, fbo->texture);
    gl->TexImage2D(GL_TEXTURE_2D, 0, format->internal_format, fbo->rw, fbo->rh, 0,
                   format->format, format->type, NULL);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    fbotex_set_filter(fbo, filter ? filter : GL_LINEAR);

    gl_check_error(gl, log, "after creating framebuffer texture");

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

    gl_check_error(gl, log, "after creating framebuffer");

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

// Mark framebuffer contents as unneeded.
void fbotex_invalidate(struct fbotex *fbo)
{
    GL *gl = fbo->gl;

    if (!fbo->fbo || !gl->InvalidateFramebuffer)
        return;

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
    gl->InvalidateFramebuffer(GL_FRAMEBUFFER, 1,
                              (GLenum[]){GL_COLOR_ATTACHMENT0});
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
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

// Apply the effects of one transformation to another, transforming it in the
// process. In other words: post-composes t onto x
void gl_transform_trans(struct gl_transform t, struct gl_transform *x)
{
    struct gl_transform xt = *x;
    x->m[0][0] = t.m[0][0] * xt.m[0][0] + t.m[0][1] * xt.m[1][0];
    x->m[1][0] = t.m[1][0] * xt.m[0][0] + t.m[1][1] * xt.m[1][0];
    x->m[0][1] = t.m[0][0] * xt.m[0][1] + t.m[0][1] * xt.m[1][1];
    x->m[1][1] = t.m[1][0] * xt.m[0][1] + t.m[1][1] * xt.m[1][1];
    gl_transform_vec(t, &x->t[0], &x->t[1]);
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

// Force cache flush if more than this number of shaders is created.
#define SC_MAX_ENTRIES 48

enum uniform_type {
    UT_invalid,
    UT_i,
    UT_f,
    UT_m,
};

union uniform_val {
    GLfloat f[9];
    GLint i[4];
};

struct sc_uniform {
    char *name;
    enum uniform_type type;
    const char *glsl_type;
    int size;
    GLint loc;
    union uniform_val v;
};

struct sc_cached_uniform {
    GLint loc;
    union uniform_val v;
};

struct sc_entry {
    GLuint gl_shader;
    struct sc_cached_uniform *uniforms;
    int num_uniforms;
    bstr frag;
    bstr vert;
    struct gl_vao *vao;
};

struct gl_shader_cache {
    GL *gl;
    struct mp_log *log;

    // permanent
    char **exts;
    int num_exts;

    // this is modified during use (gl_sc_add() etc.) and reset for each shader
    bstr prelude_text;
    bstr header_text;
    bstr text;
    struct gl_vao *vao;

    struct sc_entry *entries;
    int num_entries;

    struct sc_uniform *uniforms;
    int num_uniforms;

    bool error_state; // true if an error occurred

    // temporary buffers (avoids frequent reallocations)
    bstr tmp[5];
};

struct gl_shader_cache *gl_sc_create(GL *gl, struct mp_log *log)
{
    struct gl_shader_cache *sc = talloc_ptrtype(NULL, sc);
    *sc = (struct gl_shader_cache){
        .gl = gl,
        .log = log,
    };
    return sc;
}

void gl_sc_reset(struct gl_shader_cache *sc)
{
    sc->prelude_text.len = 0;
    sc->header_text.len = 0;
    sc->text.len = 0;
    for (int n = 0; n < sc->num_uniforms; n++)
        talloc_free(sc->uniforms[n].name);
    sc->num_uniforms = 0;
}

static void sc_flush_cache(struct gl_shader_cache *sc)
{
    MP_VERBOSE(sc, "flushing shader cache\n");

    for (int n = 0; n < sc->num_entries; n++) {
        struct sc_entry *e = &sc->entries[n];
        sc->gl->DeleteProgram(e->gl_shader);
        talloc_free(e->vert.start);
        talloc_free(e->frag.start);
        talloc_free(e->uniforms);
    }
    sc->num_entries = 0;
}

void gl_sc_destroy(struct gl_shader_cache *sc)
{
    if (!sc)
        return;
    gl_sc_reset(sc);
    sc_flush_cache(sc);
    talloc_free(sc);
}

bool gl_sc_error_state(struct gl_shader_cache *sc)
{
    return sc->error_state;
}

void gl_sc_reset_error(struct gl_shader_cache *sc)
{
    sc->error_state = false;
}

void gl_sc_enable_extension(struct gl_shader_cache *sc, char *name)
{
    for (int n = 0; n < sc->num_exts; n++) {
        if (strcmp(sc->exts[n], name) == 0)
            return;
    }
    MP_TARRAY_APPEND(sc, sc->exts, sc->num_exts, talloc_strdup(sc, name));
}

#define bstr_xappend0(sc, b, s) bstr_xappend(sc, b, bstr0(s))

void gl_sc_add(struct gl_shader_cache *sc, const char *text)
{
    bstr_xappend0(sc, &sc->text, text);
}

void gl_sc_addf(struct gl_shader_cache *sc, const char *textf, ...)
{
    va_list ap;
    va_start(ap, textf);
    bstr_xappend_vasprintf(sc, &sc->text, textf, ap);
    va_end(ap);
}

void gl_sc_hadd(struct gl_shader_cache *sc, const char *text)
{
    bstr_xappend0(sc, &sc->header_text, text);
}

void gl_sc_haddf(struct gl_shader_cache *sc, const char *textf, ...)
{
    va_list ap;
    va_start(ap, textf);
    bstr_xappend_vasprintf(sc, &sc->header_text, textf, ap);
    va_end(ap);
}

void gl_sc_hadd_bstr(struct gl_shader_cache *sc, struct bstr text)
{
    bstr_xappend(sc, &sc->header_text, text);
}

static struct sc_uniform *find_uniform(struct gl_shader_cache *sc,
                                       const char *name)
{
    for (int n = 0; n < sc->num_uniforms; n++) {
        if (strcmp(sc->uniforms[n].name, name) == 0)
            return &sc->uniforms[n];
    }
    // not found -> add it
    struct sc_uniform new = {
        .loc = -1,
        .name = talloc_strdup(NULL, name),
    };
    MP_TARRAY_APPEND(sc, sc->uniforms, sc->num_uniforms, new);
    return &sc->uniforms[sc->num_uniforms - 1];
}

const char* mp_sampler_type(GLenum texture_target)
{
    switch (texture_target) {
    case GL_TEXTURE_1D:         return "sampler1D";
    case GL_TEXTURE_2D:         return "sampler2D";
    case GL_TEXTURE_RECTANGLE:  return "sampler2DRect";
    case GL_TEXTURE_EXTERNAL_OES: return "samplerExternalOES";
    case GL_TEXTURE_3D:         return "sampler3D";
    default: abort();
    }
}

void gl_sc_uniform_sampler(struct gl_shader_cache *sc, char *name, GLenum target,
                           int unit)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_i;
    u->size = 1;
    u->glsl_type = mp_sampler_type(target);
    u->v.i[0] = unit;
}

void gl_sc_uniform_sampler_ui(struct gl_shader_cache *sc, char *name, int unit)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_i;
    u->size = 1;
    u->glsl_type = sc->gl->es ? "highp usampler2D" : "usampler2D";
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

void gl_sc_uniform_i(struct gl_shader_cache *sc, char *name, GLint i)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_i;
    u->size = 1;
    u->glsl_type = "int";
    u->v.i[0] = i;
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
static void update_uniform(GL *gl, struct sc_entry *e, struct sc_uniform *u, int n)
{
    struct sc_cached_uniform *un = &e->uniforms[n];
    GLint loc = un->loc;
    if (loc < 0)
        return;
    switch (u->type) {
    case UT_i:
        assert(u->size == 1);
        if (memcmp(un->v.i, u->v.i, sizeof(u->v.i)) != 0) {
            memcpy(un->v.i, u->v.i, sizeof(u->v.i));
            gl->Uniform1i(loc, u->v.i[0]);
        }
        break;
    case UT_f:
        if (memcmp(un->v.f, u->v.f, sizeof(u->v.f)) != 0) {
            memcpy(un->v.f, u->v.f, sizeof(u->v.f));
            switch (u->size) {
            case 1: gl->Uniform1f(loc, u->v.f[0]); break;
            case 2: gl->Uniform2f(loc, u->v.f[0], u->v.f[1]); break;
            case 3: gl->Uniform3f(loc, u->v.f[0], u->v.f[1], u->v.f[2]); break;
            case 4: gl->Uniform4f(loc, u->v.f[0], u->v.f[1], u->v.f[2],
                                  u->v.f[3]); break;
            default: abort();
            }
        }
        break;
    case UT_m:
        if (memcmp(un->v.f, u->v.f, sizeof(u->v.f)) != 0) {
            memcpy(un->v.f, u->v.f, sizeof(u->v.f));
            switch (u->size) {
            case 2: gl->UniformMatrix2fv(loc, 1, GL_FALSE, &u->v.f[0]); break;
            case 3: gl->UniformMatrix3fv(loc, 1, GL_FALSE, &u->v.f[0]); break;
            default: abort();
            }
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
    if (gl->GetTranslatedShaderSourceANGLE && mp_msg_test(sc->log, MSGL_DEBUG)) {
        GLint len = 0;
        gl->GetShaderiv(shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &len);
        if (len > 0) {
            GLchar *sstr = talloc_zero_size(NULL, len + 1);
            gl->GetTranslatedShaderSourceANGLE(shader, len, NULL, sstr);
            MP_DBG(sc, "Translated shader:\n");
            mp_log_source(sc->log, MSGL_DEBUG, sstr);
        }
    }

    gl->AttachShader(program, shader);
    gl->DeleteShader(shader);

    if (!status)
        sc->error_state = true;
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

    if (!status)
        sc->error_state = true;
}

static GLuint create_program(struct gl_shader_cache *sc, const char *vertex,
                             const char *frag)
{
    GL *gl = sc->gl;
    MP_VERBOSE(sc, "recompiling a shader program:\n");
    if (sc->header_text.len) {
        MP_VERBOSE(sc, "header:\n");
        mp_log_source(sc->log, MSGL_V, sc->header_text.start);
        MP_VERBOSE(sc, "body:\n");
    }
    if (sc->text.len)
        mp_log_source(sc->log, MSGL_V, sc->text.start);
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

#define ADD(x, ...) bstr_xappend_asprintf(sc, (x), __VA_ARGS__)
#define ADD_BSTR(x, s) bstr_xappend(sc, (x), (s))

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

    assert(sc->vao);

    for (int n = 0; n < MP_ARRAY_SIZE(sc->tmp); n++)
        sc->tmp[n].len = 0;

    // set up shader text (header + uniforms + body)
    bstr *header = &sc->tmp[0];
    ADD(header, "#version %d%s\n", gl->glsl_version, gl->es >= 300 ? " es" : "");
    for (int n = 0; n < sc->num_exts; n++)
        ADD(header, "#extension %s : enable\n", sc->exts[n]);
    if (gl->es) {
        ADD(header, "precision mediump float;\n");
        ADD(header, "precision mediump sampler2D;\n");
        if (gl->mpgl_caps & MPGL_CAP_3D_TEX)
            ADD(header, "precision mediump sampler3D;\n");
    }
    ADD_BSTR(header, sc->prelude_text);
    char *vert_in = gl->glsl_version >= 130 ? "in" : "attribute";
    char *vert_out = gl->glsl_version >= 130 ? "out" : "varying";
    char *frag_in = gl->glsl_version >= 130 ? "in" : "varying";

    // vertex shader: we don't use the vertex shader, so just setup a dummy,
    // which passes through the vertex array attributes.
    bstr *vert_head = &sc->tmp[1];
    ADD_BSTR(vert_head, *header);
    bstr *vert_body = &sc->tmp[2];
    ADD(vert_body, "void main() {\n");
    bstr *frag_vaos = &sc->tmp[3];
    for (int n = 0; sc->vao->entries[n].name; n++) {
        const struct gl_vao_entry *e = &sc->vao->entries[n];
        const char *glsl_type = vao_glsl_type(e);
        if (strcmp(e->name, "position") == 0) {
            // setting raster pos. requires setting gl_Position magic variable
            assert(e->num_elems == 2 && e->type == GL_FLOAT);
            ADD(vert_head, "%s vec2 vertex_position;\n", vert_in);
            ADD(vert_body, "gl_Position = vec4(vertex_position, 1.0, 1.0);\n");
        } else {
            ADD(vert_head, "%s %s vertex_%s;\n", vert_in, glsl_type, e->name);
            ADD(vert_head, "%s %s %s;\n", vert_out, glsl_type, e->name);
            ADD(vert_body, "%s = vertex_%s;\n", e->name, e->name);
            ADD(frag_vaos, "%s %s %s;\n", frag_in, glsl_type, e->name);
        }
    }
    ADD(vert_body, "}\n");
    bstr *vert = vert_head;
    ADD_BSTR(vert, *vert_body);

    // fragment shader; still requires adding used uniforms and VAO elements
    bstr *frag = &sc->tmp[4];
    ADD_BSTR(frag, *header);
    if (gl->glsl_version >= 130) {
        ADD(frag, "#define texture1D texture\n");
        ADD(frag, "#define texture3D texture\n");
        ADD(frag, "out vec4 out_color;\n");
    } else {
        ADD(frag, "#define texture texture2D\n");
    }
    ADD_BSTR(frag, *frag_vaos);
    for (int n = 0; n < sc->num_uniforms; n++) {
        struct sc_uniform *u = &sc->uniforms[n];
        ADD(frag, "uniform %s %s;\n", u->glsl_type, u->name);
    }

    // Additional helpers.
    ADD(frag, "#define LUT_POS(x, lut_size)"
              " mix(0.5 / (lut_size), 1.0 - 0.5 / (lut_size), (x))\n");

    // custom shader header
    if (sc->header_text.len) {
        ADD(frag, "// header\n");
        ADD_BSTR(frag, sc->header_text);
        ADD(frag, "// body\n");
    }
    ADD(frag, "void main() {\n");
    // we require _all_ frag shaders to write to a "vec4 color"
    ADD(frag, "vec4 color = vec4(0.0, 0.0, 0.0, 1.0);\n");
    ADD_BSTR(frag, sc->text);
    if (gl->glsl_version >= 130) {
        ADD(frag, "out_color = color;\n");
    } else {
        ADD(frag, "gl_FragColor = color;\n");
    }
    ADD(frag, "}\n");

    struct sc_entry *entry = NULL;
    for (int n = 0; n < sc->num_entries; n++) {
        struct sc_entry *cur = &sc->entries[n];
        if (bstr_equals(cur->frag, *frag) && bstr_equals(cur->vert, *vert)) {
            entry = cur;
            break;
        }
    }
    if (!entry) {
        if (sc->num_entries == SC_MAX_ENTRIES)
            sc_flush_cache(sc);
        MP_TARRAY_GROW(sc, sc->entries, sc->num_entries);
        entry = &sc->entries[sc->num_entries++];
        *entry = (struct sc_entry){
            .vert = bstrdup(NULL, *vert),
            .frag = bstrdup(NULL, *frag),
        };
    }
    // build vertex shader from vao and cache the locations of the uniform variables
    if (!entry->gl_shader) {
        entry->gl_shader = create_program(sc, vert->start, frag->start);
        for (int n = 0; n < sc->num_uniforms; n++) {
            struct sc_cached_uniform un = {
                .loc = gl->GetUniformLocation(entry->gl_shader,
                                              sc->uniforms[n].name),
            };
            MP_TARRAY_APPEND(sc, entry->uniforms, entry->num_uniforms, un);
        }
    }

    gl->UseProgram(entry->gl_shader);

    assert(sc->num_uniforms == entry->num_uniforms);

    for (int n = 0; n < sc->num_uniforms; n++)
        update_uniform(gl, entry, &sc->uniforms[n], n);

    gl_sc_reset(sc);
}

// Maximum number of simultaneous query objects to keep around. Reducing this
// number might cause rendering to block until the result of a previous query is
// available
#define QUERY_OBJECT_NUM 8

// How many samples to keep around, for the sake of average and peak
// calculations. This corresponds to a few seconds (exact time variable)
#define QUERY_SAMPLE_SIZE 256

struct gl_timer {
    GL *gl;
    GLuint query[QUERY_OBJECT_NUM];
    int query_idx;

    GLuint64 samples[QUERY_SAMPLE_SIZE];
    int sample_idx;
    int sample_count;

    uint64_t avg_sum;
    uint64_t peak;
};

int gl_timer_sample_count(struct gl_timer *timer)
{
    return timer->sample_count;
}

uint64_t gl_timer_last_us(struct gl_timer *timer)
{
    return timer->samples[(timer->sample_idx - 1) % QUERY_SAMPLE_SIZE] / 1000;
}

uint64_t gl_timer_avg_us(struct gl_timer *timer)
{
    if (timer->sample_count <= 0)
        return 0;

    return timer->avg_sum / timer->sample_count / 1000;
}

uint64_t gl_timer_peak_us(struct gl_timer *timer)
{
    return timer->peak / 1000;
}

struct gl_timer *gl_timer_create(GL *gl)
{
    struct gl_timer *timer = talloc_ptrtype(NULL, timer);
    *timer = (struct gl_timer){ .gl = gl };

    if (gl->GenQueries)
        gl->GenQueries(QUERY_OBJECT_NUM, timer->query);

    return timer;
}

void gl_timer_free(struct gl_timer *timer)
{
    if (!timer)
        return;

    GL *gl = timer->gl;
    if (gl && gl->DeleteQueries) {
        // this is a no-op on already uninitialized queries
        gl->DeleteQueries(QUERY_OBJECT_NUM, timer->query);
    }

    talloc_free(timer);
}

static void gl_timer_record(struct gl_timer *timer, GLuint64 new)
{
    // Input res into the buffer and grab the previous value
    GLuint64 old = timer->samples[timer->sample_idx];
    timer->samples[timer->sample_idx++] = new;
    timer->sample_idx %= QUERY_SAMPLE_SIZE;

    // Update average and sum
    timer->avg_sum = timer->avg_sum + new - old;
    timer->sample_count = MPMIN(timer->sample_count + 1, QUERY_SAMPLE_SIZE);

    // Update peak if necessary
    if (new >= timer->peak) {
        timer->peak = new;
    } else if (timer->peak == old) {
        // It's possible that the last peak was the value we just removed,
        // if so we need to scan for the new peak
        uint64_t peak = new;
        for (int i = 0; i < QUERY_SAMPLE_SIZE; i++)
            peak = MPMAX(peak, timer->samples[i]);
        timer->peak = peak;
    }
}

// If no free query is available, this can block. Shouldn't ever happen in
// practice, though. (If it does, consider increasing QUERY_OBJECT_NUM)
// IMPORTANT: only one gl_timer object may ever be active at a single time.
// The caling code *MUST* ensure this
void gl_timer_start(struct gl_timer *timer)
{
    GL *gl = timer->gl;
    if (!gl->BeginQuery)
        return;

    // Get the next query object
    GLuint id = timer->query[timer->query_idx++];
    timer->query_idx %= QUERY_OBJECT_NUM;

    // If this query object already holds a result, we need to get and
    // record it first
    if (gl->IsQuery(id)) {
        GLuint64 elapsed;
        gl->GetQueryObjectui64v(id, GL_QUERY_RESULT, &elapsed);
        gl_timer_record(timer, elapsed);
    }

    gl->BeginQuery(GL_TIME_ELAPSED, id);
}

void gl_timer_stop(struct gl_timer *timer)
{
    GL *gl = timer->gl;
    if (gl->EndQuery)
        gl->EndQuery(GL_TIME_ELAPSED);
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

    if (!use_pbo || !gl->MapBufferRange)
        goto no_pbo;

    size_t pix_stride = gl_bytes_per_pixel(format, type);
    size_t buffer_size = pix_stride * tex_w * tex_h;
    size_t needed_size = pix_stride * w * h;

    if (buffer_size != pbo->buffer_size)
        gl_pbo_upload_uninit(pbo);

    if (!pbo->buffers[0]) {
        pbo->gl = gl;
        pbo->buffer_size = buffer_size;
        gl->GenBuffers(2, &pbo->buffers[0]);
        for (int n = 0; n < 2; n++) {
            gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo->buffers[n]);
            gl->BufferData(GL_PIXEL_UNPACK_BUFFER, buffer_size, NULL,
                           GL_DYNAMIC_COPY);
        }
    }

    pbo->index = (pbo->index + 1) % 2;

    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo->buffers[pbo->index]);
    void *data = gl->MapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, needed_size,
                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (!data)
        goto no_pbo;

    memcpy_pic(data, dataptr, pix_stride * w,  h, pix_stride * w, stride);

    if (!gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER)) {
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        goto no_pbo;
    }

    gl_upload_tex(gl, target, format, type, NULL, pix_stride * w, x, y, w, h);

    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return;

no_pbo:
    gl_upload_tex(gl, target, format, type, dataptr, stride, x, y, w, h);
}

void gl_pbo_upload_uninit(struct gl_pbo_upload *pbo)
{
    if (pbo->gl)
        pbo->gl->DeleteBuffers(2, &pbo->buffers[0]);
    *pbo = (struct gl_pbo_upload){0};
}
