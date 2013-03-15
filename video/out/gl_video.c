/*
 * This file is part of mpv.
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

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <libavutil/common.h>

#include "gl_video.h"

#include "core/bstr.h"
#include "gl_common.h"
#include "gl_osd.h"
#include "filter_kernels.h"
#include "aspect.h"
#include "video/memcpy_pic.h"
#include "bitmap_packer.h"

static const char vo_opengl_shaders[] =
// Generated from gl_video_shaders.glsl
#include "gl_video_shaders.h"
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
    int w, h;
    int tex_w, tex_h;
    GLint gl_internal_format;
    GLenum gl_format;
    GLenum gl_type;
    GLuint gl_texture;
    int gl_buffer;
    int buffer_size;
    void *buffer_ptr;
};

struct video_image {
    struct texplane planes[4];
    bool image_flipped;
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

struct gl_video {
    GL *gl;

    struct gl_video_opts opts;
    bool gl_debug;

    int depth_g;

    GLuint vertex_buffer;
    GLuint vao;

    GLuint osd_programs[SUBBITMAP_COUNT];
    GLuint indirect_program, scale_sep_program, final_program;

    struct mpgl_osd *osd;

    GLuint lut_3d_texture;
    bool use_lut_3d;

    GLuint dither_texture;
    float dither_quantization;
    float dither_multiply;
    int dither_size;

    uint32_t image_w, image_h;
    uint32_t image_dw, image_dh;
    uint32_t image_format;
    int texture_w, texture_h;

    struct mp_imgfmt_desc image_desc;

    bool is_yuv, is_rgb;
    bool is_linear_rgb;

    float input_gamma, conv_gamma;

    // per pixel (full pixel when packed, each component when planar)
    int plane_bits;
    int plane_count;

    struct video_image image;

    struct fbotex indirect_fbo;         // RGB target
    struct fbotex scale_sep_fbo;        // first pass when doing 2 pass scaling

    // state for luma (0) and chroma (1) scalers
    struct scaler scalers[2];

    struct mp_csp_details colorspace;
    struct mp_csp_equalizer video_eq;

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
    {IMGFMT_Y8,      GL_RED,   GL_RED,  GL_UNSIGNED_BYTE},
    {IMGFMT_Y16,     GL_R16,   GL_RED,  GL_UNSIGNED_SHORT},
    {IMGFMT_YA8,     GL_RG,    GL_RG,   GL_UNSIGNED_BYTE},
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

static const int byte_formats[3] =
    {0, IMGFMT_Y8, IMGFMT_Y16};

static const char *osd_shaders[SUBBITMAP_COUNT] = {
    [SUBBITMAP_LIBASS] = "frag_osd_libass",
    [SUBBITMAP_RGBA] =   "frag_osd_rgba",
};

static const struct gl_video_opts gl_video_opts_def = {
    .npot = 1,
    .dither_depth = -1,
    .fbo_format = GL_RGB,
    .scale_sep = 1,
    .scalers = { "bilinear", "bilinear" },
    .scaler_params = {NAN, NAN},
};


static int validate_scaler_opt(const m_option_t *opt, struct bstr name,
                               struct bstr param);

#define OPT_BASE_STRUCT struct gl_video_opts
const struct m_sub_options gl_video_conf = {
    .opts = (m_option_t[]) {
        OPT_FLAG("gamma", gamma, 0),
        OPT_FLAG("srgb", srgb, 0),
        OPT_FLAG("npot", npot, 0),
        OPT_FLAG("pbo", pbo, 0),
        OPT_INT("stereo", stereo_mode, 0),
        OPT_STRING_VALIDATE("lscale", scalers[0], 0, validate_scaler_opt),
        OPT_STRING_VALIDATE("cscale", scalers[1], 0, validate_scaler_opt),
        OPT_FLOAT("lparam1", scaler_params[0], 0),
        OPT_FLOAT("lparam2", scaler_params[1], 0),
        OPT_FLAG("fancy-downscaling", fancy_downscaling, 0),
        OPT_FLAG("indirect", indirect, 0),
        OPT_FLAG("scale-sep", scale_sep, 0),
        OPT_CHOICE("fbo-format", fbo_format, 0,
                   ({"rgb",    GL_RGB},
                    {"rgba",   GL_RGBA},
                    {"rgb8",   GL_RGB8},
                    {"rgb10",  GL_RGB10},
                    {"rgb16",  GL_RGB16},
                    {"rgb16f", GL_RGB16F},
                    {"rgb32f", GL_RGB32F},
                    {"rgba12", GL_RGBA12},
                    {"rgba16", GL_RGBA16},
                    {"rgba16f", GL_RGBA16F},
                    {"rgba32f", GL_RGBA32F})),
        OPT_CHOICE_OR_INT("dither-depth", dither_depth, 0, -1, 16,
                          ({"no", -1}, {"auto", 0})),
        OPT_FLAG("alpha", enable_alpha, 0),
        {0}
    },
    .size = sizeof(struct gl_video_opts),
    .defaults = &gl_video_opts_def,
};

static void uninit_rendering(struct gl_video *p);
static void delete_shaders(struct gl_video *p);
static void check_gl_features(struct gl_video *p);
static bool init_format(int fmt, struct gl_video *init);


static void default_tex_params(struct GL *gl, GLenum target, GLint filter)
{
    gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    gl->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void debug_check_gl(struct gl_video *p, const char *msg)
{
    if (p->gl_debug)
        glCheckError(p->gl, msg);
}

void gl_video_set_debug(struct gl_video *p, bool enable)
{
    p->gl_debug = enable;
}

static void tex_size(struct gl_video *p, int w, int h, int *texw, int *texh)
{
    if (p->opts.npot) {
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

static void draw_triangles(struct gl_video *p, struct vertex *vb, int vert_count)
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

static bool fbotex_init(struct gl_video *p, struct fbotex *fbo, int w, int h)
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
    gl->TexImage2D(GL_TEXTURE_2D, 0, p->opts.fbo_format,
                   fbo->tex_w, fbo->tex_h, 0,
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

static void fbotex_uninit(struct gl_video *p, struct fbotex *fbo)
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

static void update_uniforms(struct gl_video *p, GLuint program)
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
    if (p->image_desc.flags & MP_IMGFLAG_XYZ) {
        cparams.colorspace.format = MP_CSP_XYZ;
        cparams.input_bits = 8;
        cparams.texture_bits = 8;
    }

    loc = gl->GetUniformLocation(program, "transform");
    if (loc >= 0) {
        float matrix[3][3];
        matrix_ortho2d(matrix, 0, p->vp_w, p->vp_h, 0);
        gl->UniformMatrix3fv(loc, 1, GL_FALSE, &matrix[0][0]);
    }

    loc = gl->GetUniformLocation(program, "colormatrix");
    if (loc >= 0) {
        float yuv2rgb[3][4] = {{0}};
        mp_get_yuv2rgb_coeffs(&cparams, yuv2rgb);
        gl->UniformMatrix4x3fv(loc, 1, GL_TRUE, &yuv2rgb[0][0]);
    }

    gl->Uniform1f(gl->GetUniformLocation(program, "input_gamma"),
                  p->input_gamma);

    gl->Uniform1f(gl->GetUniformLocation(program, "conv_gamma"),
                  p->conv_gamma);

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
                      p->image.planes[n].w, p->image.planes[n].h);
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

    float sparam1 = p->opts.scaler_params[0];
    gl->Uniform1f(gl->GetUniformLocation(program, "filter_param1"),
                  isnan(sparam1) ? 0.5f : sparam1);

    gl->UseProgram(0);

    debug_check_gl(p, "update_uniforms()");
}

static void update_all_uniforms(struct gl_video *p)
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
static bool input_is_subsampled(struct gl_video *p)
{
    for (int i = 0; i < p->plane_count; i++)
        if (p->image_desc.xs[i] || p->image_desc.ys[i])
            return true;
    return false;
}

static void compile_shaders(struct gl_video *p)
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

    // Need to pass alpha through the whole chain. (Not needed for OSD shaders.)
    shader_def_opt(&header, "USE_ALPHA", p->opts.enable_alpha);

    char *header_osd = talloc_strdup(tmp, header);
    shader_def_opt(&header_osd, "USE_OSD_LINEAR_CONV", p->opts.srgb &&
                                                      !p->use_lut_3d);
    shader_def_opt(&header_osd, "USE_OSD_3DLUT", p->use_lut_3d);
    shader_def_opt(&header_osd, "USE_OSD_SRGB", p->opts.srgb);

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

    float input_gamma = 1.0;
    float conv_gamma = 1.0;

    if (p->image_desc.flags & MP_IMGFLAG_XYZ) {
        input_gamma *= 2.6;
        conv_gamma *= 1.0 / 2.2;
    }

    if (!p->is_linear_rgb && (p->opts.srgb || p->use_lut_3d))
        conv_gamma *= 1.0 / 0.45;

    p->input_gamma = input_gamma;
    p->conv_gamma = conv_gamma;

    bool convert_input_gamma = p->input_gamma != 1.0;
    bool convert_input_to_linear = p->conv_gamma != 1.0;

    if (p->image_format == IMGFMT_NV12 || p->image_format == IMGFMT_NV21) {
        shader_def(&header_conv, "USE_CONV", "CONV_NV12");
    } else if (p->plane_count > 1) {
        shader_def(&header_conv, "USE_CONV", "CONV_PLANAR");
    }

    shader_def_opt(&header_conv, "USE_GBRP", p->image_format == IMGFMT_GBRP);
    shader_def_opt(&header_conv, "USE_SWAP_UV", p->image_format == IMGFMT_NV21);
    shader_def_opt(&header_conv, "USE_YGRAY", p->is_yuv && p->plane_count == 1);
    shader_def_opt(&header_conv, "USE_INPUT_GAMMA", convert_input_gamma);
    shader_def_opt(&header_conv, "USE_COLORMATRIX", !p->is_rgb);
    shader_def_opt(&header_conv, "USE_CONV_GAMMA", convert_input_to_linear);
    if (p->opts.enable_alpha && p->plane_count == 4)
        shader_def(&header_conv, "USE_ALPHA_PLANE", "3");

    shader_def_opt(&header_final, "USE_LINEAR_CONV_INV", p->use_lut_3d);
    shader_def_opt(&header_final, "USE_GAMMA_POW", p->opts.gamma);
    shader_def_opt(&header_final, "USE_3DLUT", p->use_lut_3d);
    shader_def_opt(&header_final, "USE_SRGB", p->opts.srgb);
    shader_def_opt(&header_final, "USE_DITHER", p->dither_texture != 0);

    if (p->opts.scale_sep && p->scalers[0].kernel) {
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
    bool use_indirect = p->opts.indirect;

    // Don't sample from input video textures before converting the input to
    // linear light. (Unneeded when sRGB textures are used.)
    if (convert_input_gamma || convert_input_to_linear)
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

static void delete_shaders(struct gl_video *p)
{
    GL *gl = p->gl;

    for (int n = 0; n < SUBBITMAP_COUNT; n++)
        delete_program(gl, &p->osd_programs[n]);
    delete_program(gl, &p->indirect_program);
    delete_program(gl, &p->scale_sep_program);
    delete_program(gl, &p->final_program);
}

static double get_scale_factor(struct gl_video *p)
{
    double sx = (p->dst_rect.x1 - p->dst_rect.x0) /
                (double)(p->src_rect.x1 - p->src_rect.x0);
    double sy = (p->dst_rect.y1 - p->dst_rect.y0) /
                (double)(p->src_rect.y1 - p->src_rect.y0);
    // xxx: actually we should use different scalers in X/Y directions if the
    // scale factors are different due to anamorphic content
    return FFMIN(sx, sy);
}

static bool update_scale_factor(struct gl_video *p, struct filter_kernel *kernel)
{
    double scale = get_scale_factor(p);
    if (!p->opts.fancy_downscaling && scale < 1.0)
        scale = 1.0;
    return mp_init_filter(kernel, filter_sizes, FFMAX(1.0, 1.0 / scale));
}

static void init_scaler(struct gl_video *p, struct scaler *scaler)
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
        if (!isnan(p->opts.scaler_params[n]))
            scaler->kernel->params[n] = p->opts.scaler_params[n];
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

static void init_dither(struct gl_video *p)
{
    GL *gl = p->gl;

    // Assume 8 bits per component if unknown.
    int dst_depth = p->depth_g ? p->depth_g : 8;
    if (p->opts.dither_depth > 0)
        dst_depth = p->opts.dither_depth;

    if (p->opts.dither_depth < 0)
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

static void recreate_osd(struct gl_video *p)
{
    if (p->osd)
        mpgl_osd_destroy(p->osd);
    p->osd = mpgl_osd_init(p->gl, false);
    p->osd->use_pbo = p->opts.pbo;
}

static void reinit_rendering(struct gl_video *p)
{
    mp_msg(MSGT_VO, MSGL_V, "[gl] Reinit rendering.\n");

    debug_check_gl(p, "before scaler initialization");

    uninit_rendering(p);

    if (!p->image.planes[0].gl_texture)
        return;

    init_dither(p);

    init_scaler(p, &p->scalers[0]);
    init_scaler(p, &p->scalers[1]);

    compile_shaders(p);
    update_all_uniforms(p);

    if (p->indirect_program && !p->indirect_fbo.fbo)
        fbotex_init(p, &p->indirect_fbo, p->texture_w, p->texture_h);

    recreate_osd(p);
}

static void uninit_rendering(struct gl_video *p)
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
}

void gl_video_set_lut3d(struct gl_video *p, struct lut3d *lut3d)
{
    GL *gl = p->gl;

    assert(!p->lut_3d_texture);

    if (!lut3d)
        return;

    gl->GenTextures(1, &p->lut_3d_texture);
    gl->ActiveTexture(GL_TEXTURE0 + TEXUNIT_3DLUT);
    gl->BindTexture(GL_TEXTURE_3D, p->lut_3d_texture);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->TexImage3D(GL_TEXTURE_3D, 0, GL_RGB16, lut3d->size[0], lut3d->size[1],
                   lut3d->size[2], 0, GL_RGB, GL_UNSIGNED_SHORT, lut3d->data);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    gl->ActiveTexture(GL_TEXTURE0);

    p->use_lut_3d = true;
    check_gl_features(p);

    debug_check_gl(p, "after 3d lut creation");
}

static void set_image_textures(struct gl_video *p, struct video_image *vimg)
{
    GL *gl = p->gl;

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];

        gl->ActiveTexture(GL_TEXTURE0 + n);
        gl->BindTexture(GL_TEXTURE_2D, plane->gl_texture);
    }
    gl->ActiveTexture(GL_TEXTURE0);
}

static void init_video(struct gl_video *p)
{
    GL *gl = p->gl;

    check_gl_features(p);

    if (p->is_rgb && (p->opts.srgb || p->use_lut_3d)) {
        p->is_linear_rgb = true;
        p->image.planes[0].gl_internal_format = GL_SRGB;
    }

    int eq_caps = MP_CSP_EQ_CAPS_GAMMA;
    if (p->is_yuv)
        eq_caps |= MP_CSP_EQ_CAPS_COLORMATRIX;
    p->video_eq.capabilities = eq_caps;

    debug_check_gl(p, "before video texture creation");

    struct video_image *vimg = &p->image;

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];

        plane->w = p->image_w >> p->image_desc.xs[n];
        plane->h = p->image_h >> p->image_desc.ys[n];

        tex_size(p, plane->w, plane->h,
                    &plane->tex_w, &plane->tex_h);

        mp_msg(MSGT_VO, MSGL_V, "[gl] Texture for plane %d: %dx%d\n",
                n, plane->tex_w, plane->tex_h);

        gl->ActiveTexture(GL_TEXTURE0 + n);
        gl->GenTextures(1, &plane->gl_texture);
        gl->BindTexture(GL_TEXTURE_2D, plane->gl_texture);

        gl->TexImage2D(GL_TEXTURE_2D, 0, plane->gl_internal_format,
                       plane->tex_w, plane->tex_h, 0,
                       plane->gl_format, plane->gl_type, NULL);

        default_tex_params(gl, GL_TEXTURE_2D, GL_LINEAR);
    }
    gl->ActiveTexture(GL_TEXTURE0);

    p->texture_w = p->image.planes[0].tex_w;
    p->texture_h = p->image.planes[0].tex_h;

    debug_check_gl(p, "after video texture creation");

    reinit_rendering(p);
}

static void uninit_video(struct gl_video *p)
{
    GL *gl = p->gl;

    uninit_rendering(p);

    struct video_image *vimg = &p->image;

    for (int n = 0; n < 3; n++) {
        struct texplane *plane = &vimg->planes[n];

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

static void render_to_fbo(struct gl_video *p, struct fbotex *fbo, int w, int h,
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

static void handle_pass(struct gl_video *p, struct fbotex **source,
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

void gl_video_render_frame(struct gl_video *p)
{
    GL *gl = p->gl;
    struct vertex vb[VERTICES_PER_QUAD];
    struct video_image *vimg = &p->image;
    bool is_flipped = vimg->image_flipped;

    if (p->dst_rect.x0 > p->vp_x || p->dst_rect.y0 > p->vp_y
        || p->dst_rect.x1 < p->vp_x + p->vp_w
        || p->dst_rect.y1 < p->vp_y + p->vp_h)
    {
        gl->Clear(GL_COLOR_BUFFER_BIT);
    }

    // Order of processing:
    //  [indirect -> [scale_sep ->]] final

    set_image_textures(p, vimg);

    struct fbotex dummy = {
        .vp_w = p->image_w, .vp_h = p->image_h,
        .tex_w = p->texture_w, .tex_h = p->texture_h,
        .texture = vimg->planes[0].gl_texture,
    };
    struct fbotex *source = &dummy;

    handle_pass(p, &source, &p->indirect_fbo, p->indirect_program);
    handle_pass(p, &source, &p->scale_sep_fbo, p->scale_sep_program);

    gl->BindTexture(GL_TEXTURE_2D, source->texture);
    gl->UseProgram(p->final_program);

    float final_texw = p->image_w * source->tex_w / (float)source->vp_w;
    float final_texh = p->image_h * source->tex_h / (float)source->vp_h;

    if (p->opts.stereo_mode) {
        int w = p->src_rect.x1 - p->src_rect.x0;
        int imgw = p->image_w;

        glEnable3DLeft(gl, p->opts.stereo_mode);

        write_quad(vb,
                   p->dst_rect.x0, p->dst_rect.y0,
                   p->dst_rect.x1, p->dst_rect.y1,
                   p->src_rect.x0 / 2, p->src_rect.y0,
                   p->src_rect.x0 / 2 + w / 2, p->src_rect.y1,
                   final_texw, final_texh,
                   NULL, is_flipped);
        draw_triangles(p, vb, VERTICES_PER_QUAD);

        glEnable3DRight(gl, p->opts.stereo_mode);

        write_quad(vb,
                   p->dst_rect.x0, p->dst_rect.y0,
                   p->dst_rect.x1, p->dst_rect.y1,
                   p->src_rect.x0 / 2 + imgw / 2, p->src_rect.y0,
                   p->src_rect.x0 / 2 + imgw / 2 + w / 2, p->src_rect.y1,
                   final_texw, final_texh,
                   NULL, is_flipped);
        draw_triangles(p, vb, VERTICES_PER_QUAD);

        glDisable3D(gl, p->opts.stereo_mode);
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

static void update_window_sized_objects(struct gl_video *p)
{
    if (p->scale_sep_program) {
        int h = p->dst_rect.y1 - p->dst_rect.y0;
        if (h > p->scale_sep_fbo.tex_h) {
            fbotex_uninit(p, &p->scale_sep_fbo);
            // Round up to an arbitrary alignment to make window resizing or
            // panscan controls smoother (less texture reallocations).
            int height = FFALIGN(h, 256);
            fbotex_init(p, &p->scale_sep_fbo, p->image_w, height);
        }
        p->scale_sep_fbo.vp_w = p->image_w;
        p->scale_sep_fbo.vp_h = h;
    }
}

static void check_resize(struct gl_video *p)
{
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
    if (too_small) {
        mp_msg(MSGT_VO, MSGL_WARN, "[gl] Can't downscale that much, window "
                                   "output may look suboptimal.\n");
    }

    update_window_sized_objects(p);
    update_all_uniforms(p);
}

void gl_video_resize(struct gl_video *p, struct mp_rect *window,
                     struct mp_rect *src, struct mp_rect *dst,
                     struct mp_osd_res *osd)
{
    p->src_rect = *src;
    p->dst_rect = *dst;
    p->osd_rect = *osd;

    p->vp_x = window->x0;
    p->vp_y = window->y0;
    p->vp_w = window->x1 - window->x0;
    p->vp_h = window->y1 - window->y0;

    p->gl->Viewport(p->vp_x, p->vp_y, p->vp_w, p->vp_h);

    check_resize(p);
}

static bool get_image(struct gl_video *p, struct mp_image *mpi)
{
    GL *gl = p->gl;

    if (!p->opts.pbo)
        return false;

    // We don't support alpha planes. (Disabling PBOs with normal draw calls is
    // an undesired, but harmless side-effect.)
    if (mpi->num_planes != p->plane_count)
        return false;

    struct video_image *vimg = &p->image;

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];
        mpi->stride[n] = mpi->plane_w[n] * p->image_desc.bytes[n];
        int needed_size = mpi->plane_h[n] * mpi->stride[n];
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

void gl_video_upload_image(struct gl_video *p, struct mp_image *mpi)
{
    GL *gl = p->gl;
    int n;

    assert(mpi->num_planes >= p->plane_count);

    struct video_image *vimg = &p->image;

    mp_image_t mpi2 = *mpi;
    bool pbo = false;
    if (!vimg->planes[0].buffer_ptr && get_image(p, &mpi2)) {
        for (n = 0; n < p->plane_count; n++) {
            int line_bytes = mpi->plane_w[n] * p->image_desc.bytes[n];
            memcpy_pic(mpi2.planes[n], mpi->planes[n], line_bytes, mpi->plane_h[n],
                       mpi2.stride[n], mpi->stride[n]);
        }
        mpi = &mpi2;
        pbo = true;
    }
    vimg->image_flipped = mpi->stride[0] < 0;
    for (n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];
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
        glUploadTex(gl, GL_TEXTURE_2D, plane->gl_format, plane->gl_type,
                    plane_ptr, mpi->stride[n], 0, 0, plane->w, plane->h, 0);
    }
    gl->ActiveTexture(GL_TEXTURE0);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

struct mp_image *gl_video_download_image(struct gl_video *p)
{
    GL *gl = p->gl;

    struct video_image *vimg = &p->image;

    if (!vimg->planes[0].gl_texture)
        return NULL;

    mp_image_t *image = mp_image_alloc(p->image_format, p->texture_w,
                                                        p->texture_h);

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];
        gl->ActiveTexture(GL_TEXTURE0 + n);
        gl->BindTexture(GL_TEXTURE_2D, plane->gl_texture);
        glDownloadTex(gl, GL_TEXTURE_2D, plane->gl_format, plane->gl_type,
                      image->planes[n], image->stride[n]);
    }
    gl->ActiveTexture(GL_TEXTURE0);
    mp_image_set_size(image, p->image_w, p->image_h);
    mp_image_set_display_size(image, p->image_dw, p->image_dh);

    mp_image_set_colorspace_details(image, &p->colorspace);

    return image;
}

static void draw_osd_cb(void *ctx, struct mpgl_osd_part *osd,
                        struct sub_bitmaps *imgs)
{
    struct gl_video *p = ctx;
    GL *gl = p->gl;

    assert(osd->format != SUBBITMAP_EMPTY);

    if (!osd->num_vertices && imgs) {
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

void gl_video_draw_osd(struct gl_video *p, struct osd_state *osd)
{
    GL *gl = p->gl;
    assert(p->osd);

    mpgl_osd_draw_cb(p->osd, osd, p->osd_rect, draw_osd_cb, p);

    // The playloop calls this last before waiting some time until it decides
    // to call flip_page(). Tell OpenGL to start execution of the GPU commands
    // while we sleep (this happens asynchronously).
    gl->Flush();
}

// Disable features that are not supported with the current OpenGL version.
static void check_gl_features(struct gl_video *p)
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
    if (!have_float_tex || (!have_fbo && p->opts.scale_sep)) {
        for (int n = 0; n < 2; n++) {
            struct scaler *scaler = &p->scalers[n];
            if (mp_find_filter_kernel(scaler->name)) {
                scaler->name = "bilinear";
                disabled[n_disabled++]
                    = have_float_tex ? "scaler (FBO)" : "scaler (float tex.)";
            }
        }
    }

    if (!have_srgb && p->opts.srgb) {
        p->opts.srgb = false;
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
        p->opts.scale_sep = false;
        p->opts.indirect = false;
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

static int init_gl(struct gl_video *p)
{
    GL *gl = p->gl;

    debug_check_gl(p, "before init_gl");

    check_gl_features(p);

    gl->Disable(GL_DITHER);
    gl->Disable(GL_BLEND);
    gl->Disable(GL_DEPTH_TEST);
    gl->DepthMask(GL_FALSE);
    gl->Disable(GL_CULL_FACE);

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

    gl->ClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    debug_check_gl(p, "after init_gl");

    return 1;
}

void gl_video_uninit(struct gl_video *p)
{
    GL *gl = p->gl;

    uninit_video(p);

    if (gl->DeleteVertexArrays)
        gl->DeleteVertexArrays(1, &p->vao);
    gl->DeleteBuffers(1, &p->vertex_buffer);
    gl->DeleteTextures(1, &p->lut_3d_texture);

    mpgl_osd_destroy(p->osd);

    talloc_free(p);
}

static bool init_format(int fmt, struct gl_video *init)
{
    bool supported = false;
    struct gl_video dummy;
    if (!init)
        init = &dummy;

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (!desc.id)
        return false;

    if (desc.num_planes > 4)
        return false;

    int plane_format[4] = {0};

    init->image_format = fmt;
    init->plane_bits = desc.bpp[0];

    // YUV/planar formats
    if (!supported && (desc.flags & MP_IMGFLAG_YUV_P)) {
        int bits = desc.plane_bits;
        if ((desc.flags & MP_IMGFLAG_NE) && bits >= 8 && bits <= 16) {
            supported = true;
            init->plane_bits = bits;
            plane_format[0] = byte_formats[(bits + 7) / 8];
        }
    }

    // YUV/half-packed
    if (!supported && (fmt == IMGFMT_NV12 || fmt == IMGFMT_NV21)) {
        supported = true;
        plane_format[0] = IMGFMT_Y8;
        plane_format[1] = IMGFMT_YA8;
    }

    // RGB/planar
    if (!supported && fmt == IMGFMT_GBRP) {
        supported = true;
        plane_format[0] = byte_formats[1];
    }

    // XYZ (same roganization as RGB packed, but requires conversion matrix)
    if (!supported && fmt == IMGFMT_XYZ12) {
        supported = true;
        plane_format[0] = IMGFMT_RGB48;
    }

    // All formats in mp_to_gl_formats[] are supported
    // If it's not in the table, it will be rejected below.
    // Includes packed RGB and YUV formats
    if (!supported && desc.num_planes == 1) {
        supported = true;
        plane_format[0] = fmt;
    }

    if (!supported)
        return false;

    for (int p = 0; p < desc.num_planes; p++) {
        struct texplane *plane = &init->image.planes[p];
        if (p > 0 && !plane_format[p])
            plane_format[p] = plane_format[0];
        for (const struct fmt_entry *e = mp_to_gl_formats; e->mp_format; e++) {
            if (e->mp_format == plane_format[p]) {
                plane->gl_format = e->format;
                plane->gl_internal_format = e->internal_format;
                plane->gl_type = e->type;
                goto found;
            }
        }
        return false; // not found
    found: ;
    }

    // Stuff like IMGFMT_420AP10. Untested, most likely insane.
    if (desc.num_planes == 4 && (init->plane_bits % 8) != 0)
        return false;

    init->is_yuv = desc.flags & MP_IMGFLAG_YUV;
    init->is_rgb = desc.flags & MP_IMGFLAG_RGB;
    init->is_linear_rgb = false;
    init->plane_count = desc.num_planes;
    init->image_desc = desc;

    return true;
}

bool gl_video_check_format(int mp_format)
{
    return init_format(mp_format, NULL);
}

void gl_video_config(struct gl_video *p, int format, int w, int h, int dw, int dh)
{
    if (p->image_format != format || p->image_w != w || p->image_h != h) {
        uninit_video(p);
        p->image_w = w;
        p->image_h = h;
        init_format(format, p);
        init_video(p);
    }
    p->image_dw = dw;
    p->image_dh = dh;
}

void gl_video_set_output_depth(struct gl_video *p, int r, int g, int b)
{
    mp_msg(MSGT_VO, MSGL_V, "[gl] Display depth: R=%d, G=%d, B=%d\n", r, g, b);
    p->depth_g = g;
}

struct gl_video *gl_video_init(GL *gl)
{
    struct gl_video *p = talloc_ptrtype(NULL, p);
    *p = (struct gl_video) {
        .gl = gl,
        .opts = gl_video_opts_def,
        .gl_debug = true,
        .colorspace = MP_CSP_DETAILS_DEFAULTS,
        .scalers = {
            { .index = 0, .name = "bilinear" },
            { .index = 1, .name = "bilinear" },
        },
        .scratch = talloc_zero_array(p, char *, 1),
    };
    init_gl(p);
    recreate_osd(p);
    return p;
}

static bool can_use_filter_kernel(const struct filter_kernel *kernel)
{
    if (!kernel)
        return false;
    struct filter_kernel k = *kernel;
    return mp_init_filter(&k, filter_sizes, 1);
}

// Get static string for scaler shader.
static const char* handle_scaler_opt(const char *name)
{
    if (name) {
        const struct filter_kernel *kernel = mp_find_filter_kernel(name);
        if (can_use_filter_kernel(kernel))
            return kernel->name;

        for (const char **filter = fixed_scale_filters; *filter; filter++) {
            if (strcmp(*filter, name) == 0)
                return *filter;
        }
    }
    return NULL;
}

// Set the options, and possibly update the filter chain too.
// Note: assumes all options are valid and verified by the option parser.
void gl_video_set_options(struct gl_video *p, struct gl_video_opts *opts)
{
    p->opts = *opts;
    for (int n = 0; n < 2; n++) {
        p->opts.scalers[n] = (char *)handle_scaler_opt(p->opts.scalers[n]);
        assert(p->opts.scalers[n]);
        p->scalers[n].name = p->opts.scalers[n];
    }

    check_gl_features(p);
    reinit_rendering(p);
}

bool gl_video_get_csp_override(struct gl_video *p, struct mp_csp_details *csp)
{
    *csp = p->colorspace;
    return true;
}

bool gl_video_set_csp_override(struct gl_video *p, struct mp_csp_details *csp)
{
    if (p->is_yuv) {
        p->colorspace = *csp;
        update_all_uniforms(p);
        return true;
    }
    return false;
}

bool gl_video_set_equalizer(struct gl_video *p, const char *name, int val)
{
    if (mp_csp_equalizer_set(&p->video_eq, name, val) >= 0) {
        if (!p->opts.gamma && p->video_eq.values[MP_CSP_EQ_GAMMA] != 0) {
            mp_msg(MSGT_VO, MSGL_V, "[gl] Auto-enabling gamma.\n");
            p->opts.gamma = true;
            compile_shaders(p);
        }
        update_all_uniforms(p);
        return true;
    }
    return false;
}

bool gl_video_get_equalizer(struct gl_video *p, const char *name, int *val)
{
    return mp_csp_equalizer_get(&p->video_eq, name, val) >= 0;
}

static int validate_scaler_opt(const m_option_t *opt, struct bstr name,
                               struct bstr param)
{
    char s[20];
    snprintf(s, sizeof(s), "%.*s", BSTR_P(param));
    return handle_scaler_opt(s) ? 1 : M_OPT_INVALID;
}

// Resize and redraw the contents of the window without further configuration.
// Intended to be used in situations where the frontend can't really be
// involved with reconfiguring the VO properly.
// gl_video_resize() should be called when user interaction is done.
void gl_video_resize_redraw(struct gl_video *p, int w, int h)
{
    p->gl->Viewport(p->vp_x, p->vp_y, w, h);
    p->vp_w = w;
    p->vp_h = h;
    gl_video_render_frame(p);
    mpgl_osd_redraw_cb(p->osd, draw_osd_cb, p);
}
