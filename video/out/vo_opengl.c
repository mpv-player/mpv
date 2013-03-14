/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include "config.h"

#include <libavutil/common.h>

#ifdef CONFIG_LCMS2
#include <lcms2.h>
#include "stream/stream.h"
#endif

#include "talloc.h"
#include "core/mp_common.h"
#include "core/bstr.h"
#include "core/mp_msg.h"
#include "core/subopt-helper.h"
#include "vo.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "sub/sub.h"
#include "bitmap_packer.h"

#include "gl_common.h"
#include "gl_osd.h"
#include "filter_kernels.h"
#include "video/memcpy_pic.h"

static const char vo_opengl_shaders[] =
// Generated from libvo/vo_opengl_shaders.glsl
#include "video/out/vo_opengl_shaders.h"
;

// Pixel width of 1D lookup textures.
#define LOOKUP_TEXTURE_SIZE 256

// Texture units 0-2 are used by the video, with unit 0 for free use.
// Units 3-4 are used for scaler LUTs.
#define TEXUNIT_SCALERS 3
#define TEXUNIT_3DLUT 5
#define TEXUNIT_DITHER 6

// lscale/cscale arguments that map directly to shader filter routines.
// Note that the convolution filters are not included in this list.
static const char *fixed_scale_filters[] = {
    "bilinear",
    "bicubic_fast",
    "sharpen3",
    "sharpen5",
    NULL
};

struct lut_tex_format {
    int pixels;
    GLint internal_format;
    GLenum format;
};

// Indexed with filter_kernel->size.
// This must match the weightsN functions in the shader.
// Each entry uses (size+3)/4 pixels per LUT entry, and size/pixels components
// per pixel.
struct lut_tex_format lut_tex_formats[] = {
    [2] =  {1, GL_RG16F,   GL_RG},
    [4] =  {1, GL_RGBA16F, GL_RGBA},
    [6] =  {2, GL_RGB16F,  GL_RGB},
    [8] =  {2, GL_RGBA16F, GL_RGBA},
    [12] = {3, GL_RGBA16F, GL_RGBA},
    [16] = {4, GL_RGBA16F, GL_RGBA},
};

// must be sorted, and terminated with 0
static const int filter_sizes[] = {2, 4, 6, 8, 12, 16, 0};

struct vertex {
    float position[2];
    uint8_t color[4];
    float texcoord[2];
};

#define VERTEX_ATTRIB_POSITION 0
#define VERTEX_ATTRIB_COLOR 1
#define VERTEX_ATTRIB_TEXCOORD 2

// 2 triangles primitives per quad = 6 vertices per quad
// (GL_QUAD is deprecated, strips can't be used with OSD image lists)
#define VERTICES_PER_QUAD 6

struct texplane {
    int shift_x, shift_y;
    GLuint gl_texture;
    int gl_buffer;
    int buffer_size;
    void *buffer_ptr;
};

struct scaler {
    int index;
    const char *name;
    float params[2];
    struct filter_kernel *kernel;
    GLuint gl_lut;
    const char *lut_name;

    // kernel points here
    struct filter_kernel kernel_storage;
};

struct fbotex {
    GLuint fbo;
    GLuint texture;
    int tex_w, tex_h;           // size of .texture
    int vp_w, vp_h;             // viewport of fbo / used part of the texture
};

struct gl_priv {
    struct vo *vo;
    MPGLContext *glctx;
    GL *gl;

    int use_indirect;
    int use_gamma;
    int use_srgb;
    int use_scale_sep;
    int use_fancy_downscaling;
    int use_lut_3d;
    int use_npot;
    int use_pbo;
    int use_glFinish;
    int use_gl_debug;
    int allow_sw;

    int dither_depth;
    int swap_interval;
    GLint fbo_format;
    int stereo_mode;

    struct gl_priv *defaults;
    struct gl_priv *orig_cmdline;

    GLuint vertex_buffer;
    GLuint vao;

    GLuint osd_programs[SUBBITMAP_COUNT];
    GLuint indirect_program, scale_sep_program, final_program;

    struct mpgl_osd *osd;

    GLuint lut_3d_texture;
    int lut_3d_w, lut_3d_h, lut_3d_d;
    void *lut_3d_data;

    GLuint dither_texture;
    float dither_quantization;
    float dither_multiply;
    int dither_size;

    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_format;
    int texture_width;
    int texture_height;

    bool is_yuv;
    bool is_linear_rgb;

    // per pixel (full pixel when packed, each component when planar)
    int plane_bytes;
    int plane_bits;

    GLint gl_internal_format;
    GLenum gl_format;
    GLenum gl_type;

    int plane_count;
    struct texplane planes[3];

    struct fbotex indirect_fbo;         // RGB target
    struct fbotex scale_sep_fbo;        // first pass when doing 2 pass scaling

    // state for luma (0) and chroma (1) scalers
    struct scaler scalers[2];
    // luma scaler parameters (the same are used for chroma)
    float scaler_params[2];

    struct mp_csp_details colorspace;
    struct mp_csp_equalizer video_eq;

    int mpi_flipped;
    int vo_flipped;

    struct mp_rect src_rect;    // displayed part of the source video
    struct mp_rect dst_rect;    // video rectangle on output window
    struct mp_osd_res osd_rect; // OSD size/margins
    int vp_x, vp_y, vp_w, vp_h; // GL viewport

    int frames_rendered;

    void *scratch;
};

struct fmt_entry {
    int mp_format;
    GLint internal_format;
    GLenum format;
    GLenum type;
};

static const struct fmt_entry mp_to_gl_formats[] = {
    {IMGFMT_RGB48,   GL_RGB16, GL_RGB,  GL_UNSIGNED_SHORT},
    {IMGFMT_RGB24,   GL_RGB,   GL_RGB,  GL_UNSIGNED_BYTE},
    {IMGFMT_RGBA,    GL_RGBA,  GL_RGBA, GL_UNSIGNED_BYTE},
    {IMGFMT_RGB15,   GL_RGBA,  GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    {IMGFMT_RGB16,   GL_RGB,   GL_RGB,  GL_UNSIGNED_SHORT_5_6_5_REV},
    {IMGFMT_BGR15,   GL_RGBA,  GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    {IMGFMT_BGR16,   GL_RGB,   GL_RGB,  GL_UNSIGNED_SHORT_5_6_5},
    {IMGFMT_BGR24,   GL_RGB,   GL_BGR,  GL_UNSIGNED_BYTE},
    {IMGFMT_BGRA,    GL_RGBA,  GL_BGRA, GL_UNSIGNED_BYTE},
    {0},
};

static const char *osd_shaders[SUBBITMAP_COUNT] = {
    [SUBBITMAP_LIBASS] = "frag_osd_libass",
    [SUBBITMAP_RGBA] =   "frag_osd_rgba",
};


static const char help_text[];

static void uninit_rendering(struct gl_priv *p);
static void delete_shaders(struct gl_priv *p);
static bool reparse_cmdline(struct gl_priv *p, char *arg);


static void default_tex_params(struct GL *gl, GLenum target, GLint filter)
{
    gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    gl->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void debug_check_gl(struct gl_priv *p, const char *msg)
{
    if (p->use_gl_debug || p->frames_rendered < 5)
        glCheckError(p->gl, msg);
}

static void tex_size(struct gl_priv *p, int w, int h, int *texw, int *texh)
{
    if (p->use_npot) {
        *texw = w;
        *texh = h;
    } else {
        *texw = 32;
        while (*texw < w)
            *texw *= 2;
        *texh = 32;
        while (*texh < h)
            *texh *= 2;
    }
}

static void draw_triangles(struct gl_priv *p, struct vertex *vb, int vert_count)
{
    GL *gl = p->gl;

    assert(vert_count % 3 == 0);

    gl->BindBuffer(GL_ARRAY_BUFFER, p->vertex_buffer);
    gl->BufferData(GL_ARRAY_BUFFER, vert_count * sizeof(struct vertex), vb,
                   GL_DYNAMIC_DRAW);
    gl->BindBuffer(GL_ARRAY_BUFFER, 0);

    if (gl->BindVertexArray)
        gl->BindVertexArray(p->vao);

    gl->DrawArrays(GL_TRIANGLES, 0, vert_count);

    if (gl->BindVertexArray)
        gl->BindVertexArray(0);

    debug_check_gl(p, "after rendering");
}

// Write a textured quad to a vertex array.
// va = destination vertex array, VERTICES_PER_QUAD entries will be overwritten
// x0, y0, x1, y1 = destination coordinates of the quad
// tx0, ty0, tx1, ty1 = source texture coordinates (usually in pixels)
// texture_w, texture_h = size of the texture, or an inverse factor
// color = optional color for all vertices, NULL for opaque white
// flip = flip vertically
static void write_quad(struct vertex *va,
                       float x0, float y0, float x1, float y1,
                       float tx0, float ty0, float tx1, float ty1,
                       float texture_w, float texture_h,
                       const uint8_t color[4], bool flip)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };

    if (!color)
        color = white;

    tx0 /= texture_w;
    ty0 /= texture_h;
    tx1 /= texture_w;
    ty1 /= texture_h;

    if (flip) {
        float tmp = ty0;
        ty0 = ty1;
        ty1 = tmp;
    }

#define COLOR_INIT {color[0], color[1], color[2], color[3]}
    va[0] = (struct vertex) { {x0, y0}, COLOR_INIT, {tx0, ty0} };
    va[1] = (struct vertex) { {x0, y1}, COLOR_INIT, {tx0, ty1} };
    va[2] = (struct vertex) { {x1, y0}, COLOR_INIT, {tx1, ty0} };
    va[3] = (struct vertex) { {x1, y1}, COLOR_INIT, {tx1, ty1} };
    va[4] = va[2];
    va[5] = va[1];
#undef COLOR_INIT
}

static bool fbotex_init(struct gl_priv *p, struct fbotex *fbo, int w, int h)
{
    GL *gl = p->gl;
    bool res = true;

    assert(gl->mpgl_caps & MPGL_CAP_FB);
    assert(!fbo->fbo);
    assert(!fbo->texture);

    tex_size(p, w, h, &fbo->tex_w, &fbo->tex_h);

    fbo->vp_w = w;
    fbo->vp_h = h;

    mp_msg(MSGT_VO, MSGL_V, "[gl] Create FBO: %dx%d\n", fbo->tex_w, fbo->tex_h);

    gl->GenFramebuffers(1, &fbo->fbo);
    gl->GenTextures(1, &fbo->texture);
    gl->BindTexture(GL_TEXTURE_2D, fbo->texture);
    gl->TexImage2D(GL_TEXTURE_2D, 0, p->fbo_format, fbo->tex_w, fbo->tex_h, 0,
                   GL_RGB, GL_UNSIGNED_BYTE, NULL);
    default_tex_params(gl, GL_TEXTURE_2D, GL_LINEAR);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, fbo->texture, 0);

    if (gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] Error: framebuffer completeness "
                                  "check failed!\n");
        res = false;
    }

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    debug_check_gl(p, "after creating framebuffer & associated texture");

    return res;
}

static void fbotex_uninit(struct gl_priv *p, struct fbotex *fbo)
{
    GL *gl = p->gl;

    if (gl->mpgl_caps & MPGL_CAP_FB) {
        gl->DeleteFramebuffers(1, &fbo->fbo);
        gl->DeleteTextures(1, &fbo->texture);
        *fbo = (struct fbotex) {0};
    }
}

static void matrix_ortho2d(float m[3][3], float x0, float x1,
                           float y0, float y1)
{
    memset(m, 0, 9 * sizeof(float));
    m[0][0] = 2.0f / (x1 - x0);
    m[1][1] = 2.0f / (y1 - y0);
    m[2][0] = -(x1 + x0) / (x1 - x0);
    m[2][1] = -(y1 + y0) / (y1 - y0);
    m[2][2] = 1.0f;
}

static void update_uniforms(struct gl_priv *p, GLuint program)
{
    GL *gl = p->gl;
    GLint loc;

    if (program == 0)
        return;

    gl->UseProgram(program);

    struct mp_csp_params cparams = {
        .colorspace = p->colorspace,
        .input_bits = p->plane_bits,
        .texture_bits = (p->plane_bits + 7) & ~7,
    };
    mp_csp_copy_equalizer_values(&cparams, &p->video_eq);

    loc = gl->GetUniformLocation(program, "transform");
    if (loc >= 0) {
        float matrix[3][3];
        matrix_ortho2d(matrix, 0, p->vp_w, p->vp_h, 0);
        gl->UniformMatrix3fv(loc, 1, GL_FALSE, &matrix[0][0]);
    }

    loc = gl->GetUniformLocation(program, "colormatrix");
    if (loc >= 0) {
        float yuv2rgb[3][4] = {{0}};
        if (p->is_yuv)
            mp_get_yuv2rgb_coeffs(&cparams, yuv2rgb);
        gl->UniformMatrix4x3fv(loc, 1, GL_TRUE, &yuv2rgb[0][0]);
    }

    gl->Uniform3f(gl->GetUniformLocation(program, "inv_gamma"),
                  1.0 / cparams.rgamma,
                  1.0 / cparams.ggamma,
                  1.0 / cparams.bgamma);

    for (int n = 0; n < p->plane_count; n++) {
        char textures_n[32];
        char textures_size_n[32];
        snprintf(textures_n, sizeof(textures_n), "textures[%d]", n);
        snprintf(textures_size_n, sizeof(textures_size_n), "textures_size[%d]", n);

        gl->Uniform1i(gl->GetUniformLocation(program, textures_n), n);
        gl->Uniform2f(gl->GetUniformLocation(program, textures_size_n),
                      p->texture_width >> p->planes[n].shift_x,
                      p->texture_height >> p->planes[n].shift_y);
    }

    gl->Uniform2f(gl->GetUniformLocation(program, "dither_size"),
                  p->dither_size, p->dither_size);

    gl->Uniform1i(gl->GetUniformLocation(program, "lut_3d"), TEXUNIT_3DLUT);

    for (int n = 0; n < 2; n++) {
        const char *lut = p->scalers[n].lut_name;
        if (lut)
            gl->Uniform1i(gl->GetUniformLocation(program, lut),
                          TEXUNIT_SCALERS + n);
    }

    gl->Uniform1i(gl->GetUniformLocation(program, "dither"), TEXUNIT_DITHER);
    gl->Uniform1f(gl->GetUniformLocation(program, "dither_quantization"),
                  p->dither_quantization);
    gl->Uniform1f(gl->GetUniformLocation(program, "dither_multiply"),
                  p->dither_multiply);

    float sparam1 = p->scaler_params[0];
    gl->Uniform1f(gl->GetUniformLocation(program, "filter_param1"),
                  isnan(sparam1) ? 0.5f : sparam1);

    gl->UseProgram(0);

    debug_check_gl(p, "update_uniforms()");
}

static void update_all_uniforms(struct gl_priv *p)
{
    for (int n = 0; n < SUBBITMAP_COUNT; n++)
        update_uniforms(p, p->osd_programs[n]);
    update_uniforms(p, p->indirect_program);
    update_uniforms(p, p->scale_sep_program);
    update_uniforms(p, p->final_program);
}

#define SECTION_HEADER "#!section "

static char *get_section(void *talloc_ctx, struct bstr source,
                         const char *section)
{
    char *res = talloc_strdup(talloc_ctx, "");
    bool copy = false;
    while (source.len) {
        struct bstr line = bstr_strip_linebreaks(bstr_getline(source, &source));
        if (bstr_eatstart(&line, bstr0(SECTION_HEADER))) {
            copy = bstrcmp0(line, section) == 0;
        } else if (copy) {
            res = talloc_asprintf_append_buffer(res, "%.*s\n", BSTR_P(line));
        }
    }
    return res;
}

static char *t_concat(void *talloc_ctx, const char *s1, const char *s2)
{
    return talloc_asprintf(talloc_ctx, "%s%s", s1, s2);
}

static GLuint create_shader(GL *gl, GLenum type, const char *header,
                            const char *source)
{
    void *tmp = talloc_new(NULL);
    const char *full_source = t_concat(tmp, header, source);

    GLuint shader = gl->CreateShader(type);
    gl->ShaderSource(shader, 1, &full_source, NULL);
    gl->CompileShader(shader);
    GLint status;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint log_length;
    gl->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

    int pri = status ? (log_length > 1 ? MSGL_V : MSGL_DBG2) : MSGL_ERR;
    const char *typestr = type == GL_VERTEX_SHADER ? "vertex" : "fragment";
    if (mp_msg_test(MSGT_VO, pri)) {
        mp_msg(MSGT_VO, pri, "[gl] %s shader source:\n", typestr);
        mp_log_source(MSGT_VO, pri, full_source);
    }
    if (log_length > 1) {
        GLchar *log = talloc_zero_size(tmp, log_length + 1);
        gl->GetShaderInfoLog(shader, log_length, NULL, log);
        mp_msg(MSGT_VO, pri, "[gl] %s shader compile log (status=%d):\n%s\n",
               typestr, status, log);
    }

    talloc_free(tmp);

    return shader;
}

static void prog_create_shader(GL *gl, GLuint program, GLenum type,
                               const char *header, const char *source)
{
    GLuint shader = create_shader(gl, type, header, source);
    gl->AttachShader(program, shader);
    gl->DeleteShader(shader);
}

static void link_shader(GL *gl, GLuint program)
{
    gl->LinkProgram(program);
    GLint status;
    gl->GetProgramiv(program, GL_LINK_STATUS, &status);
    GLint log_length;
    gl->GetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

    int pri = status ? (log_length > 1 ? MSGL_V : MSGL_DBG2) : MSGL_ERR;
    if (mp_msg_test(MSGT_VO, pri)) {
        GLchar *log = talloc_zero_size(NULL, log_length + 1);
        gl->GetProgramInfoLog(program, log_length, NULL, log);
        mp_msg(MSGT_VO, pri, "[gl] shader link log (status=%d): %s\n",
               status, log);
        talloc_free(log);
    }
}

static void bind_attrib_locs(GL *gl, GLuint program)
{
    gl->BindAttribLocation(program, VERTEX_ATTRIB_POSITION, "vertex_position");
    gl->BindAttribLocation(program, VERTEX_ATTRIB_COLOR, "vertex_color");
    gl->BindAttribLocation(program, VERTEX_ATTRIB_TEXCOORD, "vertex_texcoord");
}

static GLuint create_program(GL *gl, const char *name, const char *header,
                             const char *vertex, const char *frag)
{
    mp_msg(MSGT_VO, MSGL_V, "[gl] compiling shader program '%s'\n", name);
    mp_msg(MSGT_VO, MSGL_V, "[gl] header:\n");
    mp_log_source(MSGT_VO, MSGL_V, header);
    GLuint prog = gl->CreateProgram();
    prog_create_shader(gl, prog, GL_VERTEX_SHADER, header, vertex);
    prog_create_shader(gl, prog, GL_FRAGMENT_SHADER, header, frag);
    bind_attrib_locs(gl, prog);
    link_shader(gl, prog);
    return prog;
}

static void shader_def(char **shader, const char *name,
                       const char *value)
{
    *shader = talloc_asprintf_append(*shader, "#define %s %s\n", name, value);
}

static void shader_def_opt(char **shader, const char *name, bool b)
{
    if (b)
        shader_def(shader, name, "1");
}

static void shader_setup_scaler(char **shader, struct scaler *scaler, int pass)
{
    const char *target = scaler->index == 0 ? "SAMPLE_L" : "SAMPLE_C";
    if (!scaler->kernel) {
        *shader = talloc_asprintf_append(*shader, "#define %s sample_%s\n",
                                         target, scaler->name);
    } else {
        int size = scaler->kernel->size;
        if (pass != -1) {
            // The direction/pass assignment is rather arbitrary, but fixed in
            // other parts of the code (like FBO setup).
            const char *direction = pass == 0 ? "0, 1" : "1, 0";
            *shader = talloc_asprintf_append(*shader, "#define %s(p0, p1, p2) "
                "sample_convolution_sep%d(vec2(%s), %s, p0, p1, p2)\n",
                target, size, direction, scaler->lut_name);
        } else {
            *shader = talloc_asprintf_append(*shader, "#define %s(p0, p1, p2) "
                "sample_convolution%d(%s, p0, p1, p2)\n",
                target, size, scaler->lut_name);
        }
    }
}

// return false if RGB or 4:4:4 YUV
static bool input_is_subsampled(struct gl_priv *p)
{
    for (int i = 0; i < p->plane_count; i++)
        if (p->planes[i].shift_x || p->planes[i].shift_y)
            return true;
    return false;
}

static void compile_shaders(struct gl_priv *p)
{
    GL *gl = p->gl;

    delete_shaders(p);

    void *tmp = talloc_new(NULL);

    struct bstr src = bstr0(vo_opengl_shaders);
    char *vertex_shader = get_section(tmp, src, "vertex_all");
    char *shader_prelude = get_section(tmp, src, "prelude");
    char *s_video = get_section(tmp, src, "frag_video");

    char *header = talloc_asprintf(tmp, "#version %d\n%s", gl->glsl_version,
                                   shader_prelude);

    char *header_osd = talloc_strdup(tmp, header);
    shader_def_opt(&header_osd, "USE_OSD_LINEAR_CONV", p->use_srgb &&
                                                      !p->use_lut_3d);
    shader_def_opt(&header_osd, "USE_OSD_3DLUT", p->use_lut_3d);
    shader_def_opt(&header_osd, "USE_OSD_SRGB", p->use_srgb);

    for (int n = 0; n < SUBBITMAP_COUNT; n++) {
        const char *name = osd_shaders[n];
        if (name) {
            char *s_osd = get_section(tmp, src, name);
            p->osd_programs[n] =
                create_program(gl, name, header_osd, vertex_shader, s_osd);
        }
    }

    char *header_conv = talloc_strdup(tmp, "");
    char *header_final = talloc_strdup(tmp, "");
    char *header_sep = NULL;

    bool convert_input_to_linear = !p->is_linear_rgb
                                   && (p->use_srgb || p->use_lut_3d);

    shader_def_opt(&header_conv, "USE_PLANAR", p->plane_count > 1);
    shader_def_opt(&header_conv, "USE_GBRP", p->image_format == IMGFMT_GBRP);
    shader_def_opt(&header_conv, "USE_YGRAY", p->is_yuv && p->plane_count == 1);
    shader_def_opt(&header_conv, "USE_COLORMATRIX", p->is_yuv);
    shader_def_opt(&header_conv, "USE_LINEAR_CONV", convert_input_to_linear);

    shader_def_opt(&header_final, "USE_LINEAR_CONV_INV", p->use_lut_3d);
    shader_def_opt(&header_final, "USE_GAMMA_POW", p->use_gamma);
    shader_def_opt(&header_final, "USE_3DLUT", p->use_lut_3d);
    shader_def_opt(&header_final, "USE_SRGB", p->use_srgb);
    shader_def_opt(&header_final, "USE_DITHER", p->dither_texture != 0);

    if (p->use_scale_sep && p->scalers[0].kernel) {
        header_sep = talloc_strdup(tmp, "");
        shader_def_opt(&header_sep, "FIXED_SCALE", true);
        shader_setup_scaler(&header_sep, &p->scalers[0], 0);
        shader_setup_scaler(&header_final, &p->scalers[0], 1);
    } else {
        shader_setup_scaler(&header_final, &p->scalers[0], -1);
    }

    // We want to do scaling in linear light. Scaling is closely connected to
    // texture sampling due to how the shader is structured (or if GL bilinear
    // scaling is used). The purpose of the "indirect" pass is to convert the
    // input video to linear RGB.
    // Another purpose is reducing input to a single texture for scaling.
    bool use_indirect = p->use_indirect;

    // Don't sample from input video textures before converting the input to
    // linear light. (Unneeded when sRGB textures are used.)
    if (convert_input_to_linear)
        use_indirect = true;

    // It doesn't make sense to scale the chroma with cscale in the 1. scale
    // step and with lscale in the 2. step. If the chroma is subsampled, a
    // convolution filter wouldn't even work entirely correctly, because the
    // luma scaler would sample two texels instead of one per tap for chroma.
    // Also, even with 4:4:4 YUV or planar RGB, the indirection might be faster,
    // because the shader can't use one scaler for sampling from 3 textures. It
    // has to fetch the coefficients for each texture separately, even though
    // they're the same (this is not an inherent restriction, but would require
    // to restructure the shader).
    if (header_sep && p->plane_count > 1)
        use_indirect = true;

    if (input_is_subsampled(p)) {
        shader_setup_scaler(&header_conv, &p->scalers[1], -1);
    } else {
        // Force using the luma scaler on chroma. If the "indirect" stage is
        // used, the actual scaling will happen in the next stage.
        shader_def(&header_conv, "SAMPLE_C",
                   use_indirect ? "sample_bilinear" : "SAMPLE_L");
    }

    if (use_indirect) {
        // We don't use filtering for the Y-plane (luma), because it's never
        // scaled in this scenario.
        shader_def(&header_conv, "SAMPLE_L", "sample_bilinear");
        shader_def_opt(&header_conv, "FIXED_SCALE", true);
        header_conv = t_concat(tmp, header, header_conv);
        p->indirect_program =
            create_program(gl, "indirect", header_conv, vertex_shader, s_video);
    } else if (header_sep) {
        header_sep = t_concat(tmp, header_sep, header_conv);
    } else {
        header_final = t_concat(tmp, header_final, header_conv);
    }

    if (header_sep) {
        header_sep = t_concat(tmp, header, header_sep);
        p->scale_sep_program =
            create_program(gl, "scale_sep", header_sep, vertex_shader, s_video);
    }

    header_final = t_concat(tmp, header, header_final);
    p->final_program =
        create_program(gl, "final", header_final, vertex_shader, s_video);

    debug_check_gl(p, "shader compilation");

    talloc_free(tmp);
}

static void delete_program(GL *gl, GLuint *prog)
{
    gl->DeleteProgram(*prog);
    *prog = 0;
}

static void delete_shaders(struct gl_priv *p)
{
    GL *gl = p->gl;

    for (int n = 0; n < SUBBITMAP_COUNT; n++)
        delete_program(gl, &p->osd_programs[n]);
    delete_program(gl, &p->indirect_program);
    delete_program(gl, &p->scale_sep_program);
    delete_program(gl, &p->final_program);
}

static double get_scale_factor(struct gl_priv *p)
{
    double sx = (p->dst_rect.x1 - p->dst_rect.x0) /
                (double)(p->src_rect.x1 - p->src_rect.x0);
    double sy = (p->dst_rect.y1 - p->dst_rect.y0) /
                (double)(p->src_rect.y1 - p->src_rect.y0);
    // xxx: actually we should use different scalers in X/Y directions if the
    // scale factors are different due to anamorphic content
    return FFMIN(sx, sy);
}

static bool update_scale_factor(struct gl_priv *p, struct filter_kernel *kernel)
{
    double scale = get_scale_factor(p);
    if (!p->use_fancy_downscaling && scale < 1.0)
        scale = 1.0;
    return mp_init_filter(kernel, filter_sizes, FFMAX(1.0, 1.0 / scale));
}

static void init_scaler(struct gl_priv *p, struct scaler *scaler)
{
    GL *gl = p->gl;

    assert(scaler->name);

    scaler->kernel = NULL;

    const struct filter_kernel *t_kernel = mp_find_filter_kernel(scaler->name);
    if (!t_kernel)
        return;

    scaler->kernel_storage = *t_kernel;
    scaler->kernel = &scaler->kernel_storage;

    for (int n = 0; n < 2; n++) {
        if (!isnan(p->scaler_params[n]))
            scaler->kernel->params[n] = p->scaler_params[n];
    }

    update_scale_factor(p, scaler->kernel);

    int size = scaler->kernel->size;
    assert(size < FF_ARRAY_ELEMS(lut_tex_formats));
    struct lut_tex_format *fmt = &lut_tex_formats[size];
    bool use_2d = fmt->pixels > 1;
    bool is_luma = scaler->index == 0;
    scaler->lut_name = use_2d
                       ? (is_luma ? "lut_l_2d" : "lut_c_2d")
                       : (is_luma ? "lut_l_1d" : "lut_c_1d");

    gl->ActiveTexture(GL_TEXTURE0 + TEXUNIT_SCALERS + scaler->index);
    GLenum target = use_2d ? GL_TEXTURE_2D : GL_TEXTURE_1D;

    if (!scaler->gl_lut)
        gl->GenTextures(1, &scaler->gl_lut);

    gl->BindTexture(target, scaler->gl_lut);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    float *weights = talloc_array(NULL, float, LOOKUP_TEXTURE_SIZE * size);
    mp_compute_lut(scaler->kernel, LOOKUP_TEXTURE_SIZE, weights);
    if (use_2d) {
        gl->TexImage2D(GL_TEXTURE_2D, 0, fmt->internal_format, fmt->pixels,
                       LOOKUP_TEXTURE_SIZE, 0, fmt->format, GL_FLOAT,
                       weights);
    } else {
        gl->TexImage1D(GL_TEXTURE_1D, 0, fmt->internal_format,
                       LOOKUP_TEXTURE_SIZE, 0, fmt->format, GL_FLOAT,
                       weights);
    }
    talloc_free(weights);

    gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl->ActiveTexture(GL_TEXTURE0);

    debug_check_gl(p, "after initializing scaler");
}

static void make_dither_matrix(unsigned char *m, int size)
{
    m[0] = 0;
    for (int sz = 1; sz < size; sz *= 2) {
        int offset[] = {sz*size, sz, sz * (size+1), 0};
        for (int i = 0; i < 4; i++)
            for (int y = 0; y < sz * size; y += size)
                for (int x = 0; x < sz; x++)
                    m[x+y+offset[i]] = m[x+y] * 4 + (3-i) * 256/size/size;
    }
}

static void init_dither(struct gl_priv *p)
{
    GL *gl = p->gl;

    // Assume 8 bits per component if unknown.
    int dst_depth = p->glctx->depth_g ? p->glctx->depth_g : 8;
    if (p->dither_depth > 0)
        dst_depth = p->dither_depth;

    if (p->dither_depth < 0)
        return;

    mp_msg(MSGT_VO, MSGL_V, "[gl] Dither to %d.\n", dst_depth);

    // This defines how many bits are considered significant for output on
    // screen. The superfluous bits will be used for rounded according to the
    // dither matrix. The precision of the source implicitly decides how many
    // dither patterns can be visible.
    p->dither_quantization = (1 << dst_depth) - 1;
    int size = 8;
    p->dither_multiply = p->dither_quantization + 1.0 / (size*size);
    unsigned char dither[256];
    make_dither_matrix(dither, size);

    p->dither_size = size;

    gl->ActiveTexture(GL_TEXTURE0 + TEXUNIT_DITHER);
    gl->GenTextures(1, &p->dither_texture);
    gl->BindTexture(GL_TEXTURE_2D, p->dither_texture);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RED, size, size, 0, GL_RED,
                   GL_UNSIGNED_BYTE, dither);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl->ActiveTexture(GL_TEXTURE0);
}

static void reinit_rendering(struct gl_priv *p)
{
    mp_msg(MSGT_VO, MSGL_V, "[gl] Reinit rendering.\n");

    if (p->gl->SwapInterval && p->swap_interval >= 0)
        p->gl->SwapInterval(p->swap_interval);

    debug_check_gl(p, "before scaler initialization");

    uninit_rendering(p);

    init_dither(p);

    init_scaler(p, &p->scalers[0]);
    init_scaler(p, &p->scalers[1]);

    compile_shaders(p);

    if (p->indirect_program && !p->indirect_fbo.fbo)
        fbotex_init(p, &p->indirect_fbo, p->texture_width, p->texture_height);

    if (!p->osd) {
        p->osd = mpgl_osd_init(p->gl, false);
        p->osd->use_pbo = p->use_pbo;
    }
}

static void uninit_rendering(struct gl_priv *p)
{
    GL *gl = p->gl;

    delete_shaders(p);

    for (int n = 0; n < 2; n++) {
        gl->DeleteTextures(1, &p->scalers[n].gl_lut);
        p->scalers[n].gl_lut = 0;
        p->scalers[n].lut_name = NULL;
        p->scalers[n].kernel = NULL;
    }

    gl->DeleteTextures(1, &p->dither_texture);
    p->dither_texture = 0;

    if (p->osd)
        mpgl_osd_destroy(p->osd);
    p->osd = NULL;
}

static void init_lut_3d(struct gl_priv *p)
{
    GL *gl = p->gl;

    gl->GenTextures(1, &p->lut_3d_texture);
    gl->ActiveTexture(GL_TEXTURE0 + TEXUNIT_3DLUT);
    gl->BindTexture(GL_TEXTURE_3D, p->lut_3d_texture);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->TexImage3D(GL_TEXTURE_3D, 0, GL_RGB16, p->lut_3d_w, p->lut_3d_h,
                   p->lut_3d_d, 0, GL_RGB, GL_UNSIGNED_SHORT, p->lut_3d_data);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    gl->ActiveTexture(GL_TEXTURE0);

    debug_check_gl(p, "after 3d lut creation");
}

static void init_video(struct gl_priv *p)
{
    GL *gl = p->gl;

    if (p->use_lut_3d && !p->lut_3d_texture)
        init_lut_3d(p);

    if (!p->is_yuv && (p->use_srgb || p->use_lut_3d)) {
        p->is_linear_rgb = true;
        p->gl_internal_format = GL_SRGB;
    }

    int eq_caps = MP_CSP_EQ_CAPS_GAMMA;
    if (p->is_yuv)
        eq_caps |= MP_CSP_EQ_CAPS_COLORMATRIX;
    p->video_eq.capabilities = eq_caps;

    debug_check_gl(p, "before video texture creation");

    tex_size(p, p->image_width, p->image_height,
             &p->texture_width, &p->texture_height);

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &p->planes[n];

        int w = p->texture_width >> plane->shift_x;
        int h = p->texture_height >> plane->shift_y;

        mp_msg(MSGT_VO, MSGL_V, "[gl] Texture for plane %d: %dx%d\n", n, w, h);

        gl->ActiveTexture(GL_TEXTURE0 + n);
        gl->GenTextures(1, &plane->gl_texture);
        gl->BindTexture(GL_TEXTURE_2D, plane->gl_texture);

        gl->TexImage2D(GL_TEXTURE_2D, 0, p->gl_internal_format, w, h, 0,
                       p->gl_format, p->gl_type, NULL);
        default_tex_params(gl, GL_TEXTURE_2D, GL_LINEAR);
    }
    gl->ActiveTexture(GL_TEXTURE0);

    debug_check_gl(p, "after video texture creation");

    reinit_rendering(p);
}

static void uninit_video(struct gl_priv *p)
{
    GL *gl = p->gl;

    uninit_rendering(p);

    for (int n = 0; n < 3; n++) {
        struct texplane *plane = &p->planes[n];

        gl->DeleteTextures(1, &plane->gl_texture);
        plane->gl_texture = 0;
        gl->DeleteBuffers(1, &plane->gl_buffer);
        plane->gl_buffer = 0;
        plane->buffer_ptr = NULL;
        plane->buffer_size = 0;
    }

    fbotex_uninit(p, &p->indirect_fbo);
    fbotex_uninit(p, &p->scale_sep_fbo);
}

static void render_to_fbo(struct gl_priv *p, struct fbotex *fbo, int w, int h,
                          int tex_w, int tex_h)
{
    GL *gl = p->gl;

    gl->Viewport(0, 0, fbo->vp_w, fbo->vp_h);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);

    struct vertex vb[VERTICES_PER_QUAD];
    write_quad(vb, -1, -1, 1, 1,
               0, 0, w, h,
               tex_w, tex_h,
               NULL, false);
    draw_triangles(p, vb, VERTICES_PER_QUAD);

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl->Viewport(p->vp_x, p->vp_y, p->vp_w, p->vp_h);

}

static void handle_pass(struct gl_priv *p, struct fbotex **source,
                        struct fbotex *fbo, GLuint program)
{
    GL *gl = p->gl;

    if (!program)
        return;

    gl->BindTexture(GL_TEXTURE_2D, (*source)->texture);
    gl->UseProgram(program);
    render_to_fbo(p, fbo, (*source)->vp_w, (*source)->vp_h,
                  (*source)->tex_w, (*source)->tex_h);
    *source = fbo;
}

static void do_render(struct gl_priv *p)
{
    GL *gl = p->gl;
    struct vertex vb[VERTICES_PER_QUAD];
    bool is_flipped = p->mpi_flipped ^ p->vo_flipped;

    // Order of processing:
    //  [indirect -> [scale_sep ->]] final

    struct fbotex dummy = {
        .vp_w = p->image_width, .vp_h = p->image_height,
        .tex_w = p->texture_width, .tex_h = p->texture_height,
        .texture = p->planes[0].gl_texture,
    };
    struct fbotex *source = &dummy;

    handle_pass(p, &source, &p->indirect_fbo, p->indirect_program);
    handle_pass(p, &source, &p->scale_sep_fbo, p->scale_sep_program);

    gl->BindTexture(GL_TEXTURE_2D, source->texture);
    gl->UseProgram(p->final_program);

    float final_texw = p->image_width * source->tex_w / (float)source->vp_w;
    float final_texh = p->image_height * source->tex_h / (float)source->vp_h;

    if (p->stereo_mode) {
        int w = p->src_rect.x1 - p->src_rect.x0;
        int imgw = p->image_width;

        glEnable3DLeft(gl, p->stereo_mode);

        write_quad(vb,
                   p->dst_rect.x0, p->dst_rect.y0,
                   p->dst_rect.x1, p->dst_rect.y1,
                   p->src_rect.x0 / 2, p->src_rect.y0,
                   p->src_rect.x0 / 2 + w / 2, p->src_rect.y1,
                   final_texw, final_texh,
                   NULL, is_flipped);
        draw_triangles(p, vb, VERTICES_PER_QUAD);

        glEnable3DRight(gl, p->stereo_mode);

        write_quad(vb,
                   p->dst_rect.x0, p->dst_rect.y0,
                   p->dst_rect.x1, p->dst_rect.y1,
                   p->src_rect.x0 / 2 + imgw / 2, p->src_rect.y0,
                   p->src_rect.x0 / 2 + imgw / 2 + w / 2, p->src_rect.y1,
                   final_texw, final_texh,
                   NULL, is_flipped);
        draw_triangles(p, vb, VERTICES_PER_QUAD);

        glDisable3D(gl, p->stereo_mode);
    } else {
        write_quad(vb,
                   p->dst_rect.x0, p->dst_rect.y0,
                   p->dst_rect.x1, p->dst_rect.y1,
                   p->src_rect.x0, p->src_rect.y0,
                   p->src_rect.x1, p->src_rect.y1,
                   final_texw, final_texh,
                   NULL, is_flipped);
        draw_triangles(p, vb, VERTICES_PER_QUAD);
    }

    gl->UseProgram(0);

    debug_check_gl(p, "after video rendering");
}

static void update_window_sized_objects(struct gl_priv *p)
{
    if (p->scale_sep_program) {
        int h = p->dst_rect.y1 - p->dst_rect.y0;
        if (h > p->scale_sep_fbo.tex_h) {
            fbotex_uninit(p, &p->scale_sep_fbo);
            // Round up to an arbitrary alignment to make window resizing or
            // panscan controls smoother (less texture reallocations).
            int height = FFALIGN(h, 256);
            fbotex_init(p, &p->scale_sep_fbo, p->image_width, height);
        }
        p->scale_sep_fbo.vp_w = p->image_width;
        p->scale_sep_fbo.vp_h = h;
    }
}

static void resize(struct gl_priv *p)
{
    GL *gl = p->gl;
    struct vo *vo = p->vo;

    mp_msg(MSGT_VO, MSGL_V, "[gl] Resize: %dx%d\n", vo->dwidth, vo->dheight);
    p->vp_x = 0, p->vp_y = 0;
    p->vp_w = vo->dwidth, p->vp_h = vo->dheight;
    gl->Viewport(p->vp_x, p->vp_y, p->vp_w, p->vp_h);

    vo_get_src_dst_rects(vo, &p->src_rect, &p->dst_rect, &p->osd_rect);

    bool need_scaler_reinit = false;    // filter size change needed
    bool need_scaler_update = false;    // filter LUT change needed
    bool too_small = false;
    for (int n = 0; n < 2; n++) {
        if (p->scalers[n].kernel) {
            struct filter_kernel tkernel = *p->scalers[n].kernel;
            struct filter_kernel old = tkernel;
            bool ok = update_scale_factor(p, &tkernel);
            too_small |= !ok;
            need_scaler_reinit |= (tkernel.size != old.size);
            need_scaler_update |= (tkernel.inv_scale != old.inv_scale);
        }
    }
    if (need_scaler_reinit) {
        reinit_rendering(p);
    } else if (need_scaler_update) {
        init_scaler(p, &p->scalers[0]);
        init_scaler(p, &p->scalers[1]);
    }
    if (too_small)
        mp_msg(MSGT_VO, MSGL_WARN, "[gl] Can't downscale that much, window "
                                   "output may look suboptimal.\n");

    update_window_sized_objects(p);
    update_all_uniforms(p);

    gl->Clear(GL_COLOR_BUFFER_BIT);
    vo->want_redraw = true;
}

static void flip_page(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    if (p->use_glFinish)
        gl->Finish();

    p->glctx->swapGlBuffers(p->glctx);

    if (p->dst_rect.x0 > p->vp_x || p->dst_rect.y0 > p->vp_y
        || p->dst_rect.x1 < p->vp_x + p->vp_w
        || p->dst_rect.y1 < p->vp_y + p->vp_h)
    {
        gl->Clear(GL_COLOR_BUFFER_BIT);
    }

    p->frames_rendered++;
}

static bool get_image(struct vo *vo, mp_image_t *mpi)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    if (!p->use_pbo)
        return false;

    // We don't support alpha planes. (Disabling PBOs with normal draw calls is
    // an undesired, but harmless side-effect.)
    if (mpi->num_planes != p->plane_count)
        return false;

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &p->planes[n];
        mpi->stride[n] = (mpi->w >> plane->shift_x) * p->plane_bytes;
        int needed_size = (mpi->h >> plane->shift_y) * mpi->stride[n];
        if (!plane->gl_buffer)
            gl->GenBuffers(1, &plane->gl_buffer);
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, plane->gl_buffer);
        if (needed_size > plane->buffer_size) {
            plane->buffer_size = needed_size;
            gl->BufferData(GL_PIXEL_UNPACK_BUFFER, plane->buffer_size,
                           NULL, GL_DYNAMIC_DRAW);
        }
        if (!plane->buffer_ptr)
            plane->buffer_ptr = gl->MapBuffer(GL_PIXEL_UNPACK_BUFFER,
                                              GL_WRITE_ONLY);
        mpi->planes[n] = plane->buffer_ptr;
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    return true;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;
    int n;

    assert(mpi->num_planes >= p->plane_count);

    mp_image_t mpi2 = *mpi;
    int w = mpi->w, h = mpi->h;
    bool pbo = false;
    if (!p->planes[0].buffer_ptr && get_image(p->vo, &mpi2)) {
        for (n = 0; n < p->plane_count; n++) {
            struct texplane *plane = &p->planes[n];
            int xs = plane->shift_x, ys = plane->shift_y;
            int line_bytes = (mpi->w >> xs) * p->plane_bytes;
            memcpy_pic(mpi2.planes[n], mpi->planes[n], line_bytes, mpi->h >> ys,
                       mpi2.stride[n], mpi->stride[n]);
        }
        mpi = &mpi2;
        pbo = true;
    }
    p->mpi_flipped = mpi->stride[0] < 0;
    for (n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &p->planes[n];
        int xs = plane->shift_x, ys = plane->shift_y;
        void *plane_ptr = mpi->planes[n];
        if (pbo) {
            gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, plane->gl_buffer);
            if (!gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER))
                mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Video PBO upload failed. "
                       "Remove the 'pbo' suboption.\n");
            plane->buffer_ptr = NULL;
            plane_ptr = NULL; // PBO offset 0
        }
        gl->ActiveTexture(GL_TEXTURE0 + n);
        gl->BindTexture(GL_TEXTURE_2D, plane->gl_texture);
        glUploadTex(gl, GL_TEXTURE_2D, p->gl_format, p->gl_type, plane_ptr,
                    mpi->stride[n], 0, 0, w >> xs, h >> ys, 0);
    }
    gl->ActiveTexture(GL_TEXTURE0);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    do_render(p);
}

static mp_image_t *get_screenshot(struct gl_priv *p)
{
    GL *gl = p->gl;

    mp_image_t *image = mp_image_alloc(p->image_format, p->texture_width,
                                                        p->texture_height);

    // NOTE about image formats with alpha plane: we don't even have the alpha
    // anymore. We never upload it to any texture, as it would be a waste of
    // time. On the other hand, we can't find a "similar", non-alpha image
    // format easily. So we just leave the alpha plane of the newly allocated
    // image as-is, and hope that the alpha is ignored by the receiver of the
    // screenshot. (If not, code should be added to make it fully opaque.)

    for (int n = 0; n < p->plane_count; n++) {
        gl->ActiveTexture(GL_TEXTURE0 + n);
        gl->BindTexture(GL_TEXTURE_2D, p->planes[n].gl_texture);
        glDownloadTex(gl, GL_TEXTURE_2D, p->gl_format, p->gl_type,
                      image->planes[n], image->stride[n]);
    }
    gl->ActiveTexture(GL_TEXTURE0);
    mp_image_set_size(image, p->image_width, p->image_height);
    mp_image_set_display_size(image, p->vo->aspdat.prew, p->vo->aspdat.preh);

    mp_image_set_colorspace_details(image, &p->colorspace);

    return image;
}

static void draw_osd_cb(void *ctx, struct sub_bitmaps *imgs)
{
    struct gl_priv *p = ctx;
    GL *gl = p->gl;

    struct mpgl_osd_part *osd = mpgl_osd_generate(p->osd, imgs);
    if (!osd)
        return;

    assert(osd->format != SUBBITMAP_EMPTY);

    if (!osd->num_vertices) {
        osd->vertices = talloc_realloc(osd, osd->vertices, struct vertex,
                                       osd->packer->count * VERTICES_PER_QUAD);

        struct vertex *va = osd->vertices;

        for (int n = 0; n < osd->packer->count; n++) {
            struct sub_bitmap *b = &imgs->parts[n];
            struct pos p = osd->packer->result[n];

            // NOTE: the blend color is used with SUBBITMAP_LIBASS only, so it
            //       doesn't matter that we upload garbage for the other formats
            uint32_t c = b->libass.color;
            uint8_t color[4] = { c >> 24, (c >> 16) & 0xff,
                                (c >> 8) & 0xff, 255 - (c & 0xff) };

            write_quad(&va[osd->num_vertices],
                    b->x, b->y, b->x + b->dw, b->y + b->dh,
                    p.x, p.y, p.x + b->w, p.y + b->h,
                    osd->w, osd->h, color, false);
            osd->num_vertices += VERTICES_PER_QUAD;
        }
    }

    debug_check_gl(p, "before drawing osd");

    gl->UseProgram(p->osd_programs[osd->format]);
    mpgl_osd_set_gl_state(p->osd, osd);
    draw_triangles(p, osd->vertices, osd->num_vertices);
    mpgl_osd_unset_gl_state(p->osd, osd);
    gl->UseProgram(0);

    debug_check_gl(p, "after drawing osd");
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;
    assert(p->osd);

    osd_draw(osd, p->osd_rect, osd->vo_pts, 0, p->osd->formats, draw_osd_cb, p);

    // The playloop calls this last before waiting some time until it decides
    // to call flip_page(). Tell OpenGL to start execution of the GPU commands
    // while we sleep (this happens asynchronously).
    gl->Flush();
}

// Disable features that are not supported with the current OpenGL version.
static void check_gl_features(struct gl_priv *p)
{
    GL *gl = p->gl;
    bool have_float_tex = gl->mpgl_caps & MPGL_CAP_FLOAT_TEX;
    bool have_fbo = gl->mpgl_caps & MPGL_CAP_FB;
    bool have_srgb = gl->mpgl_caps & MPGL_CAP_SRGB_TEX;

    // srgb_compand() not available
    if (gl->glsl_version < 130)
        have_srgb = false;

    char *disabled[10];
    int n_disabled = 0;

    if (have_fbo) {
        struct fbotex fbo = {0};
        have_fbo = fbotex_init(p, &fbo, 16, 16);
        fbotex_uninit(p, &fbo);
    }

    // Disable these only if the user didn't disable scale-sep on the command
    // line, so convolution filter can still be forced to be run.
    // Normally, we want to disable them by default if FBOs are unavailable,
    // because they will be slow (not critically slow, but still slower).
    // Without FP textures, we must always disable them.
    if (!have_float_tex || (!have_fbo && p->use_scale_sep)) {
        for (int n = 0; n < 2; n++) {
            struct scaler *scaler = &p->scalers[n];
            if (mp_find_filter_kernel(scaler->name)) {
                scaler->name = "bilinear";
                disabled[n_disabled++]
                    = have_float_tex ? "scaler (FBO)" : "scaler (float tex.)";
            }
        }
    }

    if (!have_srgb && p->use_srgb) {
        p->use_srgb = false;
        disabled[n_disabled++] = "sRGB";
    }
    if (!have_fbo && p->use_lut_3d) {
        p->use_lut_3d = false;
        disabled[n_disabled++] = "color management (FBO)";
    }
    if (!have_srgb && p->use_lut_3d) {
        p->use_lut_3d = false;
        disabled[n_disabled++] = "color management (sRGB)";
    }

    if (!have_fbo) {
        p->use_scale_sep = false;
        p->use_indirect = false;
    }

    if (n_disabled) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] Some OpenGL extensions not detected, "
               "disabling: ");
        for (int n = 0; n < n_disabled; n++) {
            if (n)
                mp_msg(MSGT_VO, MSGL_ERR, ", ");
            mp_msg(MSGT_VO, MSGL_ERR, "%s", disabled[n]);
        }
        mp_msg(MSGT_VO, MSGL_ERR, ".\n");
    }
}

static void setup_vertex_array(GL *gl)
{
    size_t stride = sizeof(struct vertex);

    gl->EnableVertexAttribArray(VERTEX_ATTRIB_POSITION);
    gl->VertexAttribPointer(VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE,
                            stride, (void*)offsetof(struct vertex, position));

    gl->EnableVertexAttribArray(VERTEX_ATTRIB_COLOR);
    gl->VertexAttribPointer(VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                            stride, (void*)offsetof(struct vertex, color));

    gl->EnableVertexAttribArray(VERTEX_ATTRIB_TEXCOORD);
    gl->VertexAttribPointer(VERTEX_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE,
                            stride, (void*)offsetof(struct vertex, texcoord));
}

static int init_gl(struct gl_priv *p)
{
    GL *gl = p->gl;

    debug_check_gl(p, "before init_gl");

    const char *vendor     = gl->GetString(GL_VENDOR);
    const char *version    = gl->GetString(GL_VERSION);
    const char *renderer   = gl->GetString(GL_RENDERER);
    const char *glsl       = gl->GetString(GL_SHADING_LANGUAGE_VERSION);
    mp_msg(MSGT_VO, MSGL_V, "[gl] GL_RENDERER='%s', GL_VENDOR='%s', "
                            "GL_VERSION='%s', GL_SHADING_LANGUAGE_VERSION='%s'"
                            "\n", renderer, vendor, version, glsl);
    mp_msg(MSGT_VO, MSGL_V, "[gl] Display depth: R=%d, G=%d, B=%d\n",
           p->glctx->depth_r, p->glctx->depth_g, p->glctx->depth_b);

    check_gl_features(p);

    gl->Disable(GL_DITHER);
    gl->Disable(GL_BLEND);
    gl->Disable(GL_DEPTH_TEST);
    gl->DepthMask(GL_FALSE);
    gl->Disable(GL_CULL_FACE);
    gl->DrawBuffer(GL_BACK);

    gl->GenBuffers(1, &p->vertex_buffer);
    gl->BindBuffer(GL_ARRAY_BUFFER, p->vertex_buffer);

    if (gl->BindVertexArray) {
        gl->GenVertexArrays(1, &p->vao);
        gl->BindVertexArray(p->vao);
        setup_vertex_array(gl);
        gl->BindVertexArray(0);
    } else {
        setup_vertex_array(gl);
    }

    gl->BindBuffer(GL_ARRAY_BUFFER, 0);

    gl->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->Clear(GL_COLOR_BUFFER_BIT);

    debug_check_gl(p, "after init_gl");

    return 1;
}

static void uninit_gl(struct gl_priv *p)
{
    GL *gl = p->gl;

    // NOTE: GL functions might not be loaded yet
    if (!(p->glctx && p->gl->DeleteTextures))
        return;

    uninit_video(p);

    if (gl->DeleteVertexArrays)
        gl->DeleteVertexArrays(1, &p->vao);
    p->vao = 0;
    gl->DeleteBuffers(1, &p->vertex_buffer);
    p->vertex_buffer = 0;

    gl->DeleteTextures(1, &p->lut_3d_texture);
    p->lut_3d_texture = 0;
}

static bool init_format(int fmt, struct gl_priv *init)
{
    bool supported = false;
    struct gl_priv dummy;
    if (!init)
        init = &dummy;

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (!desc.id)
        return false;

    init->image_format = fmt;
    init->plane_bits = desc.plane_bits;

    // RGB/packed formats
    for (const struct fmt_entry *e = mp_to_gl_formats; e->mp_format; e++) {
        if (e->mp_format == fmt) {
            supported = true;
            init->plane_bits = desc.bpp[0];
            init->gl_format = e->format;
            init->gl_internal_format = e->internal_format;
            init->gl_type = e->type;
            break;
        }
    }

    // YUV/planar formats
    if (!supported && (desc.flags & MP_IMGFLAG_YUV_P)) {
        init->gl_format = GL_RED;
        if (init->plane_bits == 8) {
            supported = true;
            init->gl_internal_format = GL_RED;
            init->gl_type = GL_UNSIGNED_BYTE;
        } else if (init->plane_bits <= 16 && (desc.flags & MP_IMGFLAG_NE)) {
            supported = true;
            init->gl_internal_format = GL_R16;
            init->gl_type = GL_UNSIGNED_SHORT;
        }
    }

    // RGB/planar
    if (!supported && fmt == IMGFMT_GBRP) {
        supported = true;
        init->plane_bits = 8;
        init->gl_format = GL_RED;
        init->gl_internal_format = GL_RED;
        init->gl_type = GL_UNSIGNED_BYTE;
    }

    if (!supported)
        return false;

    init->plane_bytes = (init->plane_bits + 7) / 8;
    init->is_yuv = desc.flags & MP_IMGFLAG_YUV;
    init->is_linear_rgb = false;

    // NOTE: we throw away the additional alpha plane, if one exists.
    init->plane_count = desc.num_planes > 2 ? 3 : 1;
    assert(desc.num_planes >= init->plane_count);
    assert(desc.num_planes <= init->plane_count + 1);

    for (int n = 0; n < init->plane_count; n++) {
        struct texplane *plane = &init->planes[n];

        plane->shift_x = desc.xs[n];
        plane->shift_y = desc.ys[n];
    }

    return true;
}

static int query_format(struct vo *vo, uint32_t format)
{
    int caps = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_FLIP;
    if (!init_format(format, NULL))
        return 0;
    return caps;
}

static bool config_window(struct gl_priv *p, uint32_t d_width,
                          uint32_t d_height, uint32_t flags)
{
    if (p->stereo_mode == GL_3D_QUADBUFFER)
        flags |= VOFLAG_STEREO;

    if (p->use_gl_debug)
        flags |= VOFLAG_GL_DEBUG;

    int mpgl_caps = MPGL_CAP_GL21 | MPGL_CAP_TEX_RG;
    if (!p->allow_sw)
        mpgl_caps |= MPGL_CAP_NO_SW;
    return mpgl_config_window(p->glctx, mpgl_caps, d_width, d_height, flags);
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct gl_priv *p = vo->priv;

    if (!config_window(p, d_width, d_height, flags))
        return -1;

    if (!p->vertex_buffer)
        init_gl(p);

    p->vo_flipped = !!(flags & VOFLAG_FLIPPING);

    if (p->image_format != format || p->image_width != width
        || p->image_height != height)
    {
        uninit_video(p);
        p->image_height = height;
        p->image_width = width;
        init_format(format, p);
        init_video(p);
    }

    resize(p);

    return 0;
}

static void check_events(struct vo *vo)
{
    struct gl_priv *p = vo->priv;

    int e = p->glctx->check_events(vo);
    if (e & VO_EVENT_RESIZE)
        resize(p);
    if (e & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct gl_priv *p = vo->priv;

    switch (request) {
    case VOCTRL_ONTOP:
        if (!p->glctx->ontop)
            break;
        p->glctx->ontop(vo);
        return VO_TRUE;
    case VOCTRL_PAUSE:
        if (!p->glctx->pause)
            break;
        p->glctx->pause(vo);
        return VO_TRUE;
    case VOCTRL_RESUME:
        if (!p->glctx->resume)
            break;
        p->glctx->resume(vo);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        p->glctx->fullscreen(vo);
        resize(p);
        return VO_TRUE;
    case VOCTRL_BORDER:
        if (!p->glctx->border)
            break;
        p->glctx->border(vo);
        resize(p);
        return VO_TRUE;
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        resize(p);
        return VO_TRUE;
    case VOCTRL_GET_EQUALIZER: {
        struct voctrl_get_equalizer_args *args = data;
        return mp_csp_equalizer_get(&p->video_eq, args->name, args->valueptr)
               >= 0 ? VO_TRUE : VO_NOTIMPL;
    }
    case VOCTRL_SET_EQUALIZER: {
        struct voctrl_set_equalizer_args *args = data;
        if (mp_csp_equalizer_set(&p->video_eq, args->name, args->value) < 0)
            return VO_NOTIMPL;
        if (!p->use_gamma && p->video_eq.values[MP_CSP_EQ_GAMMA] != 0) {
            mp_msg(MSGT_VO, MSGL_V, "[gl] Auto-enabling gamma.\n");
            p->use_gamma = true;
            compile_shaders(p);
        }
        update_all_uniforms(p);
        vo->want_redraw = true;
        return VO_TRUE;
    }
    case VOCTRL_SET_YUV_COLORSPACE: {
        if (p->is_yuv) {
            p->colorspace = *(struct mp_csp_details *)data;
            update_all_uniforms(p);
            vo->want_redraw = true;
        }
        return VO_TRUE;
    }
    case VOCTRL_GET_YUV_COLORSPACE:
        *(struct mp_csp_details *)data = p->colorspace;
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        if (!p->glctx->update_xinerama_info)
            break;
        p->glctx->update_xinerama_info(vo);
        return VO_TRUE;
    case VOCTRL_SCREENSHOT: {
        struct voctrl_screenshot_args *args = data;
        if (args->full_window)
            args->out_image = glGetWindowScreenshot(p->gl);
        else
            args->out_image = get_screenshot(p);
        return true;
    }
    case VOCTRL_REDRAW_FRAME:
        do_render(p);
        return true;
    case VOCTRL_SET_COMMAND_LINE: {
        char *arg = data;
        if (!reparse_cmdline(p, arg))
            return false;
        check_gl_features(p);
        reinit_rendering(p);
        resize(p);
        vo->want_redraw = true;
        return true;
    }
    }
    return VO_NOTIMPL;
}

static void uninit(struct vo *vo)
{
    struct gl_priv *p = vo->priv;

    uninit_gl(p);
    mpgl_uninit(p->glctx);
    p->glctx = NULL;
    p->gl = NULL;
}

#ifdef CONFIG_LCMS2

static void lcms2_error_handler(cmsContext ctx, cmsUInt32Number code,
                                const char *msg)
{
    mp_msg(MSGT_VO, MSGL_ERR, "[gl] lcms2: %s\n", msg);
}

static struct bstr load_file(struct gl_priv *p, void *talloc_ctx,
                             const char *filename)
{
    struct bstr res = {0};
    stream_t *s = open_stream(filename, NULL, NULL);
    if (s) {
        res = stream_read_complete(s, talloc_ctx, 1000000000, 0);
        free_stream(s);
    }
    return res;
}

#define LUT3D_CACHE_HEADER "mpv 3dlut cache 1.0\n"

static bool load_icc(struct gl_priv *p, const char *icc_file,
                     const char *icc_cache, int icc_intent,
                     int s_r, int s_g, int s_b)
{
    void *tmp = talloc_new(p);
    uint16_t *output = talloc_array(tmp, uint16_t, s_r * s_g * s_b * 3);

    if (icc_intent == -1)
        icc_intent = INTENT_ABSOLUTE_COLORIMETRIC;

    mp_msg(MSGT_VO, MSGL_INFO, "[gl] Opening ICC profile '%s'\n", icc_file);
    struct bstr iccdata = load_file(p, tmp, icc_file);
    if (!iccdata.len)
        goto error_exit;

    char *cache_info = talloc_asprintf(tmp, "intent=%d, size=%dx%dx%d\n",
                                       icc_intent, s_r, s_g, s_b);

    // check cache
    if (icc_cache) {
        mp_msg(MSGT_VO, MSGL_INFO, "[gl] Opening 3D LUT cache in file '%s'.\n",
               icc_cache);
        struct bstr cachedata = load_file(p, tmp, icc_cache);
        if (bstr_eatstart(&cachedata, bstr0(LUT3D_CACHE_HEADER))
            && bstr_eatstart(&cachedata, bstr0(cache_info))
            && bstr_eatstart(&cachedata, iccdata)
            && cachedata.len == talloc_get_size(output))
        {
            memcpy(output, cachedata.start, cachedata.len);
            goto done;
        } else {
            mp_msg(MSGT_VO, MSGL_WARN, "[gl] 3D LUT cache invalid!\n");
        }
    }

    cmsSetLogErrorHandler(lcms2_error_handler);

    cmsHPROFILE profile = cmsOpenProfileFromMem(iccdata.start, iccdata.len);
    if (!profile)
        goto error_exit;

    cmsCIExyY d65;
    cmsWhitePointFromTemp(&d65, 6504);
    static const cmsCIExyYTRIPLE bt709prim = {
        .Red   = {0.64, 0.33, 1.0},
        .Green = {0.30, 0.60, 1.0},
        .Blue  = {0.15, 0.06, 1.0},
    };
    cmsToneCurve *tonecurve = cmsBuildGamma(NULL, 1.0/0.45);
    cmsHPROFILE vid_profile = cmsCreateRGBProfile(&d65, &bt709prim,
                        (cmsToneCurve*[3]){tonecurve, tonecurve, tonecurve});
    cmsFreeToneCurve(tonecurve);
    cmsHTRANSFORM trafo = cmsCreateTransform(vid_profile, TYPE_RGB_16,
                                             profile, TYPE_RGB_16,
                                             icc_intent,
                                             cmsFLAGS_HIGHRESPRECALC);
    cmsCloseProfile(profile);
    cmsCloseProfile(vid_profile);

    if (!trafo)
        goto error_exit;

    // transform a (s_r)x(s_g)x(s_b) cube, with 3 components per channel
    uint16_t *input = talloc_array(tmp, uint16_t, s_r * 3);
    for (int b = 0; b < s_b; b++) {
        for (int g = 0; g < s_g; g++) {
            for (int r = 0; r < s_r; r++) {
                input[r * 3 + 0] = r * 65535 / (s_r - 1);
                input[r * 3 + 1] = g * 65535 / (s_g - 1);
                input[r * 3 + 2] = b * 65535 / (s_b - 1);
            }
            size_t base = (b * s_r * s_g + g * s_r) * 3;
            cmsDoTransform(trafo, input, output + base, s_r);
        }
    }

    cmsDeleteTransform(trafo);

    if (icc_cache) {
        FILE *out = fopen(icc_cache, "wb");
        if (out) {
            fprintf(out, "%s%s", LUT3D_CACHE_HEADER, cache_info);
            fwrite(iccdata.start, iccdata.len, 1, out);
            fwrite(output, talloc_get_size(output), 1, out);
            fclose(out);
        }
    }

done:

    p->lut_3d_data = talloc_steal(p, output);
    p->lut_3d_w = s_r, p->lut_3d_h = s_g, p->lut_3d_d = s_b;
    p->use_lut_3d = true;

    talloc_free(tmp);
    return true;

error_exit:
    mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Error loading ICC profile.\n");
    talloc_free(tmp);
    return false;
}

#else /* CONFIG_LCMS2 */

static bool load_icc(struct gl_priv *p, ...)
{
    mp_msg(MSGT_VO, MSGL_FATAL, "[gl] LCMS2 support not compiled.\n");
    return false;
}

#endif /* CONFIG_LCMS2 */

static bool parse_3dlut_size(const char *s, int *p1, int *p2, int *p3)
{
    if (sscanf(s, "%dx%dx%d", p1, p2, p3) != 3)
        return false;
    for (int n = 0; n < 3; n++) {
        int s = ((int[]) { *p1, *p2, *p3 })[n];
        if (s < 2 || s > 256 || ((s - 1) & s))
            return false;
    }
    return true;
}

static int lut3d_size_valid(void *arg)
{
    char *s = *(char **)arg;
    int p1, p2, p3;
    return parse_3dlut_size(s, &p1, &p2, &p3);
}

static int backend_valid(void *arg)
{
    return mpgl_find_backend(*(const char **)arg) >= 0;
}

struct fbo_format {
    const char *name;
    GLint format;
};

const struct fbo_format fbo_formats[] = {
    {"rgb",    GL_RGB},
    {"rgba",   GL_RGBA},
    {"rgb8",   GL_RGB8},
    {"rgb10",  GL_RGB10},
    {"rgb16",  GL_RGB16},
    {"rgb16f", GL_RGB16F},
    {"rgb32f", GL_RGB32F},
    {0}
};

static GLint find_fbo_format(const char *name)
{
    for (const struct fbo_format *fmt = fbo_formats; fmt->name; fmt++) {
        if (strcmp(fmt->name, name) == 0)
            return fmt->format;
    }
    return -1;
}

static int fbo_format_valid(void *arg)
{
    return find_fbo_format(*(const char **)arg) >= 0;
}

static bool can_use_filter_kernel(const struct filter_kernel *kernel)
{
    if (!kernel)
        return false;
    struct filter_kernel k = *kernel;
    return mp_init_filter(&k, filter_sizes, 1);
}

static const char* handle_scaler_opt(const char *name)
{
    const struct filter_kernel *kernel = mp_find_filter_kernel(name);
    if (can_use_filter_kernel(kernel))
        return kernel->name;

    for (const char **filter = fixed_scale_filters; *filter; filter++) {
        if (strcmp(*filter, name) == 0)
            return *filter;
    }

    return NULL;
}

static int scaler_valid(void *arg)
{
    return handle_scaler_opt(*(const char **)arg) != NULL;
}

#if 0
static void print_scalers(void)
{
    mp_msg(MSGT_VO, MSGL_INFO, "Available scalers:\n");
    for (const char **e = fixed_scale_filters; *e; e++) {
        mp_msg(MSGT_VO, MSGL_INFO, "    %s\n", *e);
    }
    for (const struct filter_kernel *e = mp_filter_kernels; e->name; e++) {
        if (can_use_filter_kernel(e))
            mp_msg(MSGT_VO, MSGL_INFO, "    %s\n", e->name);
    }
}
#endif

static bool reparse_cmdline(struct gl_priv *p, char *arg)
{
    struct gl_priv tmp = *p->defaults;
    struct gl_priv *opt = &tmp;

    if (strcmp(arg, "-") == 0) {
        tmp = *p->orig_cmdline;
        arg = "";
    }

    char *scalers[2] = {0};
    char *fbo_format = NULL;

    const opt_t subopts[] = {
        {"srgb",                OPT_ARG_BOOL,   &opt->use_srgb},
        {"pbo",                 OPT_ARG_BOOL,   &opt->use_pbo},
        {"glfinish",            OPT_ARG_BOOL,   &opt->use_glFinish},
        {"swapinterval",        OPT_ARG_INT,    &opt->swap_interval},
        {"lscale",              OPT_ARG_MSTRZ,  &scalers[0], scaler_valid},
        {"cscale",              OPT_ARG_MSTRZ,  &scalers[1], scaler_valid},
        {"lparam1",             OPT_ARG_FLOAT,  &opt->scaler_params[0]},
        {"lparam2",             OPT_ARG_FLOAT,  &opt->scaler_params[1]},
        {"fancy-downscaling",   OPT_ARG_BOOL,   &opt->use_fancy_downscaling},
        {"indirect",            OPT_ARG_BOOL,   &opt->use_indirect},
        {"scale-sep",           OPT_ARG_BOOL,   &opt->use_scale_sep},
        {"fbo-format",          OPT_ARG_MSTRZ,  &fbo_format, fbo_format_valid},
        {"dither-depth",        OPT_ARG_INT,    &opt->dither_depth},
        {NULL}
    };

    if (subopt_parse(arg, subopts) != 0)
        return false;

    p->fbo_format = opt->fbo_format;
    if (fbo_format)
        p->fbo_format = find_fbo_format(fbo_format);
    free(fbo_format);

    for (int n = 0; n < 2; n++) {
        p->scalers[n].name = opt->scalers[n].name;
        if (scalers[n])
            p->scalers[n].name = handle_scaler_opt(scalers[n]);
        free(scalers[n]);
    }

    // xxx ideally we'd put all options into an option struct, and just copy
    p->use_srgb = opt->use_srgb; //xxx changing srgb will be wrong on RGB input!
    p->use_pbo = opt->use_pbo;
    p->use_glFinish = opt->use_glFinish;
    p->swap_interval = opt->swap_interval;
    memcpy(p->scaler_params, opt->scaler_params, sizeof(p->scaler_params));
    p->use_fancy_downscaling = opt->use_fancy_downscaling;
    p->use_indirect = opt->use_indirect;
    p->use_scale_sep = opt->use_scale_sep;
    p->dither_depth = opt->dither_depth;

    check_gl_features(p);

    return true;
}

static int preinit(struct vo *vo, const char *arg)
{
    struct gl_priv *p = talloc_zero(vo, struct gl_priv);
    vo->priv = p;

    bool hq = strcmp(vo->driver->info->short_name, "opengl-hq") == 0;

    *p = (struct gl_priv) {
        .vo = vo,
        .colorspace = MP_CSP_DETAILS_DEFAULTS,
        .use_npot = 1,
        .use_pbo = hq,
        .swap_interval = 1,
        .dither_depth = hq ? 0 : -1,
        .fbo_format = hq ? GL_RGB16 : GL_RGB,
        .use_scale_sep = 1,
        .scalers = {
            { .index = 0, .name = hq ? "lanczos2" : "bilinear" },
            { .index = 1, .name = "bilinear" },
        },
        .scaler_params = {NAN, NAN},
        .scratch = talloc_zero_array(p, char *, 1),
    };

    p->defaults = talloc(p, struct gl_priv);
    *p->defaults = *p;

    char *scalers[2] = {0};
    char *backend_arg = NULL;
    char *fbo_format = NULL;
    char *icc_profile = NULL;
    char *icc_cache = NULL;
    int icc_intent = -1;
    char *icc_size_str = NULL;

    const opt_t subopts[] = {
        {"gamma",               OPT_ARG_BOOL,   &p->use_gamma},
        {"srgb",                OPT_ARG_BOOL,   &p->use_srgb},
        {"npot",                OPT_ARG_BOOL,   &p->use_npot},
        {"pbo",                 OPT_ARG_BOOL,   &p->use_pbo},
        {"glfinish",            OPT_ARG_BOOL,   &p->use_glFinish},
        {"swapinterval",        OPT_ARG_INT,    &p->swap_interval},
        {"stereo",              OPT_ARG_INT,    &p->stereo_mode},
        {"lscale",              OPT_ARG_MSTRZ,  &scalers[0], scaler_valid},
        {"cscale",              OPT_ARG_MSTRZ,  &scalers[1], scaler_valid},
        {"lparam1",             OPT_ARG_FLOAT,  &p->scaler_params[0]},
        {"lparam2",             OPT_ARG_FLOAT,  &p->scaler_params[1]},
        {"fancy-downscaling",   OPT_ARG_BOOL,   &p->use_fancy_downscaling},
        {"debug",               OPT_ARG_BOOL,   &p->use_gl_debug},
        {"indirect",            OPT_ARG_BOOL,   &p->use_indirect},
        {"scale-sep",           OPT_ARG_BOOL,   &p->use_scale_sep},
        {"fbo-format",          OPT_ARG_MSTRZ,  &fbo_format, fbo_format_valid},
        {"backend",             OPT_ARG_MSTRZ,  &backend_arg, backend_valid},
        {"sw",                  OPT_ARG_BOOL,   &p->allow_sw},
        {"icc-profile",         OPT_ARG_MSTRZ,  &icc_profile},
        {"icc-cache",           OPT_ARG_MSTRZ,  &icc_cache},
        {"icc-intent",          OPT_ARG_INT,    &icc_intent},
        {"3dlut-size",          OPT_ARG_MSTRZ,  &icc_size_str,
         lut3d_size_valid},
        {"dither-depth",        OPT_ARG_INT,    &p->dither_depth},
        {NULL}
    };

    if (subopt_parse(arg, subopts) != 0) {
        mp_msg(MSGT_VO, MSGL_FATAL, "%s", help_text);
        goto err_out;
    }

    int backend = backend_arg ? mpgl_find_backend(backend_arg) : GLTYPE_AUTO;
    free(backend_arg);

    if (fbo_format)
        p->fbo_format = find_fbo_format(fbo_format);
    free(fbo_format);

    for (int n = 0; n < 2; n++) {
        if (scalers[n])
            p->scalers[n].name = handle_scaler_opt(scalers[n]);
        free(scalers[n]);
    }

    int s_r = 128, s_g = 256, s_b = 64;
    if (icc_size_str)
        parse_3dlut_size(icc_size_str, &s_r, &s_g, &s_b);
    free(icc_size_str);

    bool success = true;
    if (icc_profile) {
        success = load_icc(p, icc_profile, icc_cache, icc_intent,
                           s_r, s_g, s_b);
    }
    free(icc_profile);
    free(icc_cache);

    if (!success)
        goto err_out;

    p->orig_cmdline = talloc(p, struct gl_priv);
    *p->orig_cmdline = *p;

    p->glctx = mpgl_init(backend, vo);
    if (!p->glctx)
        goto err_out;
    p->gl = p->glctx->gl;

    if (!config_window(p, 320, 200, VOFLAG_HIDDEN))
        goto err_out;
    check_gl_features(p);

    return 0;

err_out:
    uninit(vo);
    return -1;
}

const struct vo_driver video_out_opengl = {
    .info = &(const vo_info_t) {
        "Extended OpenGL Renderer",
        "opengl",
        "Based on vo_gl.c by Reimar Doeffinger",
        ""
    },
    .preinit = preinit,
    .query_format = query_format,
    .config = config,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
};

const struct vo_driver video_out_opengl_hq = {
    .info = &(const vo_info_t) {
        "Extended OpenGL Renderer (high quality rendering preset)",
        "opengl-hq",
        "Based on vo_gl.c by Reimar Doeffinger",
        ""
    },
    .preinit = preinit,
    .query_format = query_format,
    .config = config,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
};

static const char help_text[] =
"\n--vo=opengl command line help:\n"
"Example: mpv --vo=opengl:scale-sep:lscale=lanczos2\n"
"\nOptions:\n"
"  lscale=<filter>\n"
"    Set the scaling filter. Possible choices:\n"
"    bilinear: bilinear texture filtering (fastest).\n"
"    bicubic_fast: bicubic filter (without lookup texture).\n"
"    sharpen3: unsharp masking (sharpening) with radius=3.\n"
"    sharpen5: unsharp masking (sharpening) with radius=5.\n"
"    lanczos2: Lanczos with radius=2 (recommended).\n"
"    lanczos3: Lanczos with radius=3 (not recommended).\n"
"    mitchell: Mitchell-Netravali.\n"
"    Default: bilinear\n"
"  lparam1=<value> / lparam2=<value>\n"
"    Set parameters for configurable filters. Affects chroma scaler\n"
"    as well.\n"
"    Filters which use this:\n"
"     mitchell: b and c params (defaults: b=1/3 c=1/3)\n"
"     kaiser: (defaults: 6.33 6.33)\n"
"     sharpen3: lparam1 sets sharpening strength (default: 0.5)\n"
"     sharpen5: as with sharpen3\n"
"  stereo=<n>\n"
"    0: normal display\n"
"    1: side-by-side to red-cyan stereo\n"
"    2: side-by-side to green-magenta stereo\n"
"    3: side-by-side to quadbuffer stereo\n"
"  srgb\n"
"    Enable gamma-correct scaling by working in linear light. This\n"
"    makes use of sRGB textures and framebuffers.\n"
"    This option forces the options 'indirect' and 'gamma'.\n"
"    NOTE: For YUV colorspaces, gamma 1/0.45 is assumed. RGB input is always\n"
"    assumed to be in sRGB.\n"
"  pbo\n"
"    Enable use of PBOs. This is faster, but can sometimes lead to\n"
"    sporadic and temporary image corruption.\n"
"  dither-depth=<n>\n"
"    Positive non-zero values select the target bit depth.\n"
"    -1: Disable any dithering done by mpv.\n"
"     0: Automatic selection. If output bit depth can't be detected,\n"
"        8 bits per component are assumed.\n"
"     8: Dither to 8 bit output.\n"
"    Default: -1.\n"
"  debug\n"
"    Check for OpenGL errors, i.e. call glGetError(). Also request a\n"
"    debug OpenGL context.\n"
"Less useful options:\n"
"  swapinterval=<n>\n"
"    Interval in displayed frames between to buffer swaps.\n"
"    1 is equivalent to enable VSYNC, 0 to disable VSYNC.\n"
"  no-scale-sep\n"
"    When using a separable scale filter for luma, usually two filter\n"
"    passes are done. This is often faster. However, it forces\n"
"    conversion to RGB in an extra pass, so it can actually be slower\n"
"    if used with fast filters on small screen resolutions. Using\n"
"    this options will make rendering a single operation.\n"
"    Note that chroma scalers are always done as 1-pass filters.\n"
"  cscale=<n>\n"
"    As lscale but for chroma (2x slower with little visible effect).\n"
"    Note that with some scaling filters, upscaling is always done in\n"
"    RGB. If chroma is not subsampled, this option is ignored, and the\n"
"    luma scaler is used instead. Setting this option is often useless.\n"
"  fancy-downscaling\n"
"    When using convolution based filters, extend the filter size\n"
"    when downscaling. Trades quality for reduced downscaling performance.\n"
"  no-npot\n"
"    Force use of power-of-2 texture sizes. For debugging only.\n"
"    Borders will look discolored due to filtering.\n"
"  glfinish\n"
"    Call glFinish() before swapping buffers\n"
"  backend=<sys>\n"
"    auto: auto-select (default)\n"
"    cocoa: Cocoa/OSX\n"
"    win: Win32/WGL\n"
"    x11: X11/GLX\n"
"    wayland: Wayland/EGL\n"
"  indirect\n"
"    Do YUV conversion and scaling as separate passes. This will\n"
"    first render the video into a video-sized RGB texture, and\n"
"    draw the result on screen. The luma scaler is used to scale\n"
"    the RGB image when rendering to screen. The chroma scaler\n"
"    is used only on YUV conversion, and only if the video uses\n"
"    chroma-subsampling.\n"
"    This mechanism is disabled on RGB input.\n"
"  fbo-format=<fmt>\n"
"    Selects the internal format of any FBO textures used.\n"
"    fmt can be one of: rgb, rgba, rgb8, rgb10, rgb16, rgb16f, rgb32f\n"
"    Default: rgb.\n"
"  gamma\n"
"    Always enable gamma control. (Disables delayed enabling.)\n"
"Color management:\n"
"  icc-profile=<file>\n"
"    Load an ICC profile and use it to transform linear RGB to\n"
"    screen output. Needs LittleCMS2 support compiled in.\n"
"  icc-cache=<file>\n"
"    Store and load the 3D LUT created from the ICC profile in\n"
"    this file. This can be used to speed up loading, since\n"
"    LittleCMS2 can take a while to create the 3D LUT.\n"
"    Note that this file will be up to ~100 MB big.\n"
"  icc-intent=<value>\n"
"    0: perceptual\n"
"    1: relative colorimetric\n"
"    2: saturation\n"
"    3: absolute colorimetric (default)\n"
"  3dlut-size=<r>x<g>x<b>\n"
"    Size of the 3D LUT generated from the ICC profile in each\n"
"    dimension. Default is 128x256x64.\n"
"    Sizes must be a power of two, and 256 at most.\n"
"Note: all defaults mentioned are for 'opengl', not 'opengl-hq'.\n"
"\n";
