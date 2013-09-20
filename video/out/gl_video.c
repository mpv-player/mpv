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

#include "mpvcore/bstr.h"
#include "gl_common.h"
#include "gl_osd.h"
#include "filter_kernels.h"
#include "aspect.h"
#include "video/memcpy_pic.h"
#include "bitmap_packer.h"
#include "dither.h"

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
    int vp_x, vp_y, vp_w, vp_h; // viewport of fbo / used part of the texture
};

struct gl_video {
    GL *gl;

    struct mp_log *log;
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
    float dither_center;
    int dither_size;

    uint32_t image_w, image_h;
    uint32_t image_dw, image_dh;
    uint32_t image_format;
    int texture_w, texture_h;

    struct mp_imgfmt_desc image_desc;

    bool is_yuv, is_rgb;
    bool is_linear_rgb;
    bool has_alpha;
    char color_swizzle[5];

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
    struct mp_image_params image_params;

    struct mp_rect src_rect;    // displayed part of the source video
    struct mp_rect dst_rect;    // video rectangle on output window
    struct mp_osd_res osd_rect; // OSD size/margins
    int vp_x, vp_y, vp_w, vp_h; // GL viewport

    int frames_rendered;

    // Cached because computing it can take relatively long
    int last_dither_matrix_size;
    float *last_dither_matrix;

    void *scratch;
};

struct fmt_entry {
    int mp_format;
    GLint internal_format;
    GLenum format;
    GLenum type;
};

// Very special formats, for which OpenGL happens to have direct support
static const struct fmt_entry mp_to_gl_formats[] = {
    {IMGFMT_RGB15,   GL_RGBA,  GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    {IMGFMT_RGB16,   GL_RGB,   GL_RGB,  GL_UNSIGNED_SHORT_5_6_5_REV},
    {IMGFMT_BGR15,   GL_RGBA,  GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    {IMGFMT_BGR16,   GL_RGB,   GL_RGB,  GL_UNSIGNED_SHORT_5_6_5},
    {0},
};

static const struct fmt_entry gl_byte_formats[] = {
    {0, GL_RED,     GL_RED,     GL_UNSIGNED_BYTE},      // 1 x 8
    {0, GL_RG,      GL_RG,      GL_UNSIGNED_BYTE},      // 2 x 8
    {0, GL_RGB,     GL_RGB,     GL_UNSIGNED_BYTE},      // 3 x 8
    {0, GL_RGBA,    GL_RGBA,    GL_UNSIGNED_BYTE},      // 4 x 8
    {0, GL_R16,     GL_RED,     GL_UNSIGNED_SHORT},     // 1 x 16
    {0, GL_RG16,    GL_RG,      GL_UNSIGNED_SHORT},     // 2 x 16
    {0, GL_RGB16,   GL_RGB,     GL_UNSIGNED_SHORT},     // 3 x 16
    {0, GL_RGBA16,  GL_RGBA,    GL_UNSIGNED_SHORT},     // 4 x 16
};

struct packed_fmt_entry {
    int fmt;
    int8_t component_size;
    int8_t components[4]; // source component - 0 means unmapped
};

static const struct packed_fmt_entry mp_packed_formats[] = {
    //                      R  G  B  A
    {IMGFMT_Y8,         1, {1, 0, 0, 0}},
    {IMGFMT_Y16,        2, {1, 0, 0, 0}},
    {IMGFMT_YA8,        1, {1, 0, 0, 2}},
    {IMGFMT_ARGB,       1, {2, 3, 4, 1}},
    {IMGFMT_0RGB,       1, {2, 3, 4, 0}},
    {IMGFMT_BGRA,       1, {3, 2, 1, 4}},
    {IMGFMT_BGR0,       1, {3, 2, 1, 0}},
    {IMGFMT_ABGR,       1, {4, 3, 2, 1}},
    {IMGFMT_0BGR,       1, {4, 3, 2, 0}},
    {IMGFMT_RGBA,       1, {1, 2, 3, 4}},
    {IMGFMT_RGB0,       1, {1, 2, 3, 0}},
    {IMGFMT_BGR24,      1, {3, 2, 1, 0}},
    {IMGFMT_RGB24,      1, {1, 2, 3, 0}},
    {IMGFMT_RGB48,      2, {1, 2, 3, 0}},
    {IMGFMT_RGBA64,     2, {1, 2, 3, 4}},
    {IMGFMT_BGRA64,     2, {3, 2, 1, 4}},
    {0},
};

static const char *osd_shaders[SUBBITMAP_COUNT] = {
    [SUBBITMAP_LIBASS] = "frag_osd_libass",
    [SUBBITMAP_RGBA] =   "frag_osd_rgba",
};

static const struct gl_video_opts gl_video_opts_def = {
    .npot = 1,
    .dither_depth = -1,
    .dither_size = 6,
    .fbo_format = GL_RGB,
    .scale_sep = 1,
    .scalers = { "bilinear", "bilinear" },
    .scaler_params = {NAN, NAN},
    .alpha_mode = 2,
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
        OPT_CHOICE("stereo", stereo_mode, 0,
                   ({"no", 0},
                    {"red-cyan",        GL_3D_RED_CYAN},
                    {"green-magenta",   GL_3D_GREEN_MAGENTA},
                    {"quadbuffer",      GL_3D_QUADBUFFER})),
        OPT_STRING_VALIDATE("lscale", scalers[0], 0, validate_scaler_opt),
        OPT_STRING_VALIDATE("cscale", scalers[1], 0, validate_scaler_opt),
        OPT_FLOAT("lparam1", scaler_params[0], 0),
        OPT_FLOAT("lparam2", scaler_params[1], 0),
        OPT_FLAG("scaler-resizes-only", scaler_resizes_only, 0),
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
        OPT_CHOICE("dither", dither_algo, 0,
                   ({"fruit", 0}, {"ordered", 1}, {"no", -1})),
        OPT_INTRANGE("dither-size-fruit", dither_size, 0, 2, 8),
        OPT_FLAG("temporal-dither", temporal_dither, 0),
        OPT_CHOICE("chroma-location", chroma_location, 0,
                   ({"auto",   MP_CHROMA_AUTO},
                    {"center", MP_CHROMA_CENTER},
                    {"left",   MP_CHROMA_LEFT})),
        OPT_CHOICE("alpha", alpha_mode, M_OPT_OPTIONAL_PARAM,
                   ({"no", 0},
                    {"yes", 1}, {"", 1},
                    {"blend", 2})),
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
        glCheckError(p->gl, p->log, msg);
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

static bool fbotex_init(struct gl_video *p, struct fbotex *fbo, int w, int h,
                        GLenum iformat)
{
    GL *gl = p->gl;
    bool res = true;

    assert(!fbo->fbo);
    assert(!fbo->texture);

    *fbo = (struct fbotex) {
        .vp_w = w,
        .vp_h = h,
    };

    tex_size(p, w, h, &fbo->tex_w, &fbo->tex_h);

    MP_VERBOSE(p, "Create FBO: %dx%d\n", fbo->tex_w, fbo->tex_h);

    if (!(gl->mpgl_caps & MPGL_CAP_FB))
        return false;

    gl->GenFramebuffers(1, &fbo->fbo);
    gl->GenTextures(1, &fbo->texture);
    gl->BindTexture(GL_TEXTURE_2D, fbo->texture);
    gl->TexImage2D(GL_TEXTURE_2D, 0, iformat,
                   fbo->tex_w, fbo->tex_h, 0,
                   GL_RGB, GL_UNSIGNED_BYTE, NULL);
    default_tex_params(gl, GL_TEXTURE_2D, GL_LINEAR);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, fbo->texture, 0);

    if (gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        MP_ERR(p, "Error: framebuffer completeness check failed!\n");
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
        snprintf(textures_n, sizeof(textures_n), "texture%d", n);
        snprintf(textures_size_n, sizeof(textures_size_n), "textures_size[%d]", n);

        gl->Uniform1i(gl->GetUniformLocation(program, textures_n), n);
        gl->Uniform2f(gl->GetUniformLocation(program, textures_size_n),
                      p->image.planes[n].tex_w, p->image.planes[n].tex_h);
    }

    loc = gl->GetUniformLocation(program, "chroma_center_offset");
    if (loc >= 0) {
        int chr = p->opts.chroma_location;
        if (!chr)
            chr = p->image_params.chroma_location;
        int cx, cy;
        mp_get_chroma_location(chr, &cx, &cy);
        // By default texture coordinates are such that chroma is centered with
        // any chroma subsampling. If a specific direction is given, make it
        // so that the luma and chroma sample line up exactly.
        // For 4:4:4, setting chroma location should have no effect at all.
        // luma sample size (in chroma coord. space)
        float ls_w = 1.0 / (1 << p->image_desc.chroma_xs);
        float ls_h = 1.0 / (1 << p->image_desc.chroma_ys);
        // move chroma center to luma center (in chroma coord. space)
        float o_x = ls_w < 1 ? ls_w * -cx / 2 : 0;
        float o_y = ls_h < 1 ? ls_h * -cy / 2 : 0;
        gl->Uniform2f(loc, o_x / FFMAX(p->image.planes[1].w, 1),
                           o_y / FFMAX(p->image.planes[1].h, 1));
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
    gl->Uniform1f(gl->GetUniformLocation(program, "dither_center"),
                  p->dither_center);

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

static GLuint create_shader(struct gl_video *p, GLenum type, const char *header,
                            const char *source)
{
    GL *gl = p->gl;

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
    if (mp_msg_test_log(p->log, pri)) {
        MP_MSG(p, pri, "%s shader source:\n", typestr);
        mp_log_source(p->log, pri, full_source);
    }
    if (log_length > 1) {
        GLchar *logstr = talloc_zero_size(tmp, log_length + 1);
        gl->GetShaderInfoLog(shader, log_length, NULL, logstr);
        MP_MSG(p, pri, "%s shader compile log (status=%d):\n%s\n",
               typestr, status, logstr);
    }

    talloc_free(tmp);

    return shader;
}

static void prog_create_shader(struct gl_video *p, GLuint program, GLenum type,
                               const char *header,  const char *source)
{
    GL *gl = p->gl;
    GLuint shader = create_shader(p, type, header, source);
    gl->AttachShader(program, shader);
    gl->DeleteShader(shader);
}

static void link_shader(struct gl_video *p, GLuint program)
{
    GL *gl = p->gl;
    gl->LinkProgram(program);
    GLint status;
    gl->GetProgramiv(program, GL_LINK_STATUS, &status);
    GLint log_length;
    gl->GetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

    int pri = status ? (log_length > 1 ? MSGL_V : MSGL_DBG2) : MSGL_ERR;
    if (mp_msg_test_log(p->log, pri)) {
        GLchar *logstr = talloc_zero_size(NULL, log_length + 1);
        gl->GetProgramInfoLog(program, log_length, NULL, logstr);
        MP_MSG(p, pri, "shader link log (status=%d): %s\n", status, logstr);
        talloc_free(logstr);
    }
}

static void bind_attrib_locs(GL *gl, GLuint program)
{
    gl->BindAttribLocation(program, VERTEX_ATTRIB_POSITION, "vertex_position");
    gl->BindAttribLocation(program, VERTEX_ATTRIB_COLOR, "vertex_color");
    gl->BindAttribLocation(program, VERTEX_ATTRIB_TEXCOORD, "vertex_texcoord");
}

#define PRELUDE_END "// -- prelude end\n"

static GLuint create_program(struct gl_video *p, const char *name,
                             const char *header, const char *vertex,
                             const char *frag)
{
    GL *gl = p->gl;
    MP_VERBOSE(p, "compiling shader program '%s', header:\n", name);
    const char *real_header = strstr(header, PRELUDE_END);
    real_header = real_header ? real_header + strlen(PRELUDE_END) : header;
    mp_log_source(p->log, MSGL_V, real_header);
    GLuint prog = gl->CreateProgram();
    prog_create_shader(p, prog, GL_VERTEX_SHADER, header, vertex);
    prog_create_shader(p, prog, GL_FRAGMENT_SHADER, header, frag);
    bind_attrib_locs(gl, prog);
    link_shader(p, prog);
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

    char *header = talloc_asprintf(tmp, "#version %d\n%s%s", gl->glsl_version,
                                   shader_prelude, PRELUDE_END);

    // Need to pass alpha through the whole chain. (Not needed for OSD shaders.)
    if (p->opts.alpha_mode == 1)
        shader_def_opt(&header, "USE_ALPHA", p->has_alpha);

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
                create_program(p, name, header_osd, vertex_shader, s_osd);
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

    if (p->color_swizzle[0])
        shader_def(&header_conv, "USE_COLOR_SWIZZLE", p->color_swizzle);
    shader_def_opt(&header_conv, "USE_SWAP_UV", p->image_format == IMGFMT_NV21);
    shader_def_opt(&header_conv, "USE_YGRAY", p->is_yuv && p->plane_count == 1);
    shader_def_opt(&header_conv, "USE_INPUT_GAMMA", convert_input_gamma);
    shader_def_opt(&header_conv, "USE_COLORMATRIX", !p->is_rgb);
    shader_def_opt(&header_conv, "USE_CONV_GAMMA", convert_input_to_linear);
    if (p->opts.alpha_mode > 0 && p->has_alpha && p->plane_count > 3)
        shader_def(&header_conv, "USE_ALPHA_PLANE", "3");
    if (p->opts.alpha_mode == 2 && p->has_alpha)
        shader_def(&header_conv, "USE_ALPHA_BLEND", "1");

    shader_def_opt(&header_final, "USE_LINEAR_CONV_INV", p->use_lut_3d);
    shader_def_opt(&header_final, "USE_GAMMA_POW", p->opts.gamma);
    shader_def_opt(&header_final, "USE_3DLUT", p->use_lut_3d);
    shader_def_opt(&header_final, "USE_SRGB", p->opts.srgb);
    shader_def_opt(&header_final, "USE_DITHER", p->dither_texture != 0);
    shader_def_opt(&header_final, "USE_TEMPORAL_DITHER", p->opts.temporal_dither);

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
            create_program(p, "indirect", header_conv, vertex_shader, s_video);
    } else if (header_sep) {
        header_sep = t_concat(tmp, header_sep, header_conv);
    } else {
        header_final = t_concat(tmp, header_final, header_conv);
    }

    if (header_sep) {
        header_sep = t_concat(tmp, header, header_sep);
        p->scale_sep_program =
            create_program(p, "scale_sep", header_sep, vertex_shader, s_video);
    }

    header_final = t_concat(tmp, header, header_final);
    p->final_program =
        create_program(p, "final", header_final, vertex_shader, s_video);

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

static void init_dither(struct gl_video *p)
{
    GL *gl = p->gl;

    // Assume 8 bits per component if unknown.
    int dst_depth = p->depth_g ? p->depth_g : 8;
    if (p->opts.dither_depth > 0)
        dst_depth = p->opts.dither_depth;

    if (p->opts.dither_depth < 0 || p->opts.dither_algo < 0)
        return;

    MP_VERBOSE(p, "Dither to %d.\n", dst_depth);

    int tex_size;
    void *tex_data;
    GLint tex_iformat;
    GLenum tex_type;
    unsigned char temp[256];

    if (p->opts.dither_algo == 0) {
        int sizeb = p->opts.dither_size;
        int size = 1 << sizeb;

        if (p->last_dither_matrix_size != size) {
            p->last_dither_matrix = talloc_realloc(p, p->last_dither_matrix,
                                                   float, size * size);
            mp_make_fruit_dither_matrix(p->last_dither_matrix, sizeb);
            p->last_dither_matrix_size = size;
        }

        tex_size = size;
        tex_iformat = GL_R16;
        tex_type = GL_FLOAT;
        tex_data = p->last_dither_matrix;
    } else {
        assert(sizeof(temp) >= 8 * 8);
        mp_make_ordered_dither_matrix(temp, 8);

        tex_size = 8;
        tex_iformat = GL_RED;
        tex_type = GL_UNSIGNED_BYTE;
        tex_data = temp;
    }

    // This defines how many bits are considered significant for output on
    // screen. The superfluous bits will be used for rounding according to the
    // dither matrix. The precision of the source implicitly decides how many
    // dither patterns can be visible.
    p->dither_quantization = (1 << dst_depth) - 1;
    p->dither_center = 0.5 / (tex_size * tex_size);
    p->dither_size = tex_size;

    gl->ActiveTexture(GL_TEXTURE0 + TEXUNIT_DITHER);
    gl->GenTextures(1, &p->dither_texture);
    gl->BindTexture(GL_TEXTURE_2D, p->dither_texture);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->TexImage2D(GL_TEXTURE_2D, 0, tex_iformat, tex_size, tex_size, 0, GL_RED,
                   tex_type, tex_data);
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
    p->osd = mpgl_osd_init(p->gl, p->log, false);
    p->osd->use_pbo = p->opts.pbo;
}

static bool does_resize(struct mp_rect src, struct mp_rect dst)
{
    return src.x1 - src.x0 != dst.x1 - dst.x0 ||
           src.y1 - src.y0 != dst.y1 - dst.y0;
}

static const char *expected_scaler(struct gl_video *p, int unit)
{
    if (p->opts.scaler_resizes_only && unit == 0 &&
        !does_resize(p->src_rect, p->dst_rect))
    {
        return "bilinear";
    }
    return p->opts.scalers[unit];
}

static void reinit_rendering(struct gl_video *p)
{
    MP_VERBOSE(p, "Reinit rendering.\n");

    debug_check_gl(p, "before scaler initialization");

    uninit_rendering(p);

    if (!p->image.planes[0].gl_texture)
        return;

    for (int n = 0; n < 2; n++)
        p->scalers[n].name = expected_scaler(p, n);

    init_dither(p);

    init_scaler(p, &p->scalers[0]);
    init_scaler(p, &p->scalers[1]);

    compile_shaders(p);
    update_all_uniforms(p);

    if (p->indirect_program && !p->indirect_fbo.fbo)
        fbotex_init(p, &p->indirect_fbo, p->image_w, p->image_h,
                    p->opts.fbo_format);

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

    // For video with odd sizes: enlarge the luma texture so that it covers all
    // chroma pixels - then we can render these correctly by cropping the final
    // image (conceptually).
    // Image allocations are always such that the "additional" luma border
    // exists and can be accessed.
    int full_w = MP_ALIGN_UP(p->image_w, 1 << p->image_desc.chroma_xs);
    int full_h = MP_ALIGN_UP(p->image_h, 1 << p->image_desc.chroma_ys);

    struct video_image *vimg = &p->image;

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];

        plane->w = full_w >> p->image_desc.xs[n];
        plane->h = full_h >> p->image_desc.ys[n];

        tex_size(p, plane->w, plane->h,
                    &plane->tex_w, &plane->tex_h);

        MP_VERBOSE(p, "Texture for plane %d: %dx%d\n",
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

static void change_dither_trafo(struct gl_video *p)
{
    GL *gl = p->gl;
    int program = p->final_program;

    int phase = p->frames_rendered % 8u;
    float r = phase * (M_PI / 2); // rotate
    float m = phase < 4 ? 1 : -1; // mirror

    gl->UseProgram(program);

    float matrix[2][2] = {{cos(r),     -sin(r)    },
                          {sin(r) * m,  cos(r) * m}};
    gl->UniformMatrix2fv(gl->GetUniformLocation(program, "dither_trafo"),
                         1, GL_TRUE, &matrix[0][0]);

    gl->UseProgram(0);
}

static void render_to_fbo(struct gl_video *p, struct fbotex *fbo,
                          int x, int y, int w, int h, int tex_w, int tex_h)
{
    GL *gl = p->gl;

    gl->Viewport(fbo->vp_x, fbo->vp_y, fbo->vp_w, fbo->vp_h);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);

    struct vertex vb[VERTICES_PER_QUAD];
    write_quad(vb, -1, -1, 1, 1,
               x, y, x + w, y + h,
               tex_w, tex_h,
               NULL, false);
    draw_triangles(p, vb, VERTICES_PER_QUAD);

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl->Viewport(p->vp_x, p->vp_y, p->vp_w, p->vp_h);

}

// *chain contains the source, and is overwritten with a copy of the result
static void handle_pass(struct gl_video *p, struct fbotex *chain,
                        struct fbotex *fbo, GLuint program)
{
    GL *gl = p->gl;

    if (!program)
        return;

    gl->BindTexture(GL_TEXTURE_2D, chain->texture);
    gl->UseProgram(program);
    render_to_fbo(p, fbo, chain->vp_x, chain->vp_y,
                  chain->vp_w, chain->vp_h,
                  chain->tex_w, chain->tex_h);
    *chain = *fbo;
}

void gl_video_render_frame(struct gl_video *p)
{
    GL *gl = p->gl;
    struct vertex vb[VERTICES_PER_QUAD];
    struct video_image *vimg = &p->image;
    bool is_flipped = vimg->image_flipped;

    if (p->opts.temporal_dither)
        change_dither_trafo(p);

    if (p->dst_rect.x0 > p->vp_x || p->dst_rect.y0 > p->vp_y
        || p->dst_rect.x1 < p->vp_x + p->vp_w
        || p->dst_rect.y1 < p->vp_y + p->vp_h)
    {
        gl->Clear(GL_COLOR_BUFFER_BIT);
    }

    // Order of processing:
    //  [indirect -> [scale_sep ->]] final

    set_image_textures(p, vimg);

    struct fbotex chain = {
        .vp_w = p->image_w,
        .vp_h = p->image_h,
        .tex_w = p->texture_w,
        .tex_h = p->texture_h,
        .texture = vimg->planes[0].gl_texture,
    };

    handle_pass(p, &chain, &p->indirect_fbo, p->indirect_program);

    // Clip to visible height so that separate scaling scales the visible part
    // only (and the target FBO texture can have a bounded size).
    // Don't clamp width; too hard to get correct final scaling on l/r borders.
    chain.vp_y = p->src_rect.y0,
    chain.vp_h = p->src_rect.y1 - p->src_rect.y0,

    handle_pass(p, &chain, &p->scale_sep_fbo, p->scale_sep_program);

    gl->BindTexture(GL_TEXTURE_2D, chain.texture);
    gl->UseProgram(p->final_program);

    struct mp_rect src = {p->src_rect.x0, chain.vp_y,
                          p->src_rect.x1, chain.vp_y + chain.vp_h};
    int src_texw = chain.tex_w;
    int src_texh = chain.tex_h;

    if (p->opts.stereo_mode) {
        int w = src.x1 - src.x0;
        int imgw = p->image_w;

        glEnable3DLeft(gl, p->opts.stereo_mode);

        write_quad(vb,
                   p->dst_rect.x0, p->dst_rect.y0,
                   p->dst_rect.x1, p->dst_rect.y1,
                   src.x0 / 2, src.y0,
                   src.x0 / 2 + w / 2, src.y1,
                   src_texw, src_texh,
                   NULL, is_flipped);
        draw_triangles(p, vb, VERTICES_PER_QUAD);

        glEnable3DRight(gl, p->opts.stereo_mode);

        write_quad(vb,
                   p->dst_rect.x0, p->dst_rect.y0,
                   p->dst_rect.x1, p->dst_rect.y1,
                   src.x0 / 2 + imgw / 2, src.y0,
                   src.x0 / 2 + imgw / 2 + w / 2, src.y1,
                   src_texw, src_texh,
                   NULL, is_flipped);
        draw_triangles(p, vb, VERTICES_PER_QUAD);

        glDisable3D(gl, p->opts.stereo_mode);
    } else {
        write_quad(vb,
                   p->dst_rect.x0, p->dst_rect.y0,
                   p->dst_rect.x1, p->dst_rect.y1,
                   src.x0, src.y0,
                   src.x1, src.y1,
                   src_texw, src_texh,
                   NULL, is_flipped);
        draw_triangles(p, vb, VERTICES_PER_QUAD);
    }

    gl->UseProgram(0);

    p->frames_rendered++;

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
            fbotex_init(p, &p->scale_sep_fbo, p->image_w, height,
                        p->opts.fbo_format);
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
    for (int n = 0; n < 2; n++) {
        if (strcmp(p->scalers[n].name, expected_scaler(p, n)) != 0)
            need_scaler_reinit = true;
    }
    if (need_scaler_reinit) {
        reinit_rendering(p);
    } else if (need_scaler_update) {
        init_scaler(p, &p->scalers[0]);
        init_scaler(p, &p->scalers[1]);
    }
    if (too_small) {
        MP_WARN(p, "Can't downscale that much, window "
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

    struct video_image *vimg = &p->image;

    // See comments in init_video() about odd video sizes.
    // The normal upload path does this too, but less explicit.
    mp_image_set_size(mpi, vimg->planes[0].w, vimg->planes[0].h);

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

    assert(mpi->num_planes == p->plane_count);

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
                MP_FATAL(p, "Video PBO upload failed. "
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

    assert(p->image_format == p->image_params.imgfmt);
    assert(p->texture_w >= p->image_params.w);
    assert(p->texture_h >= p->image_params.h);

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
    mp_image_set_params(image, &p->image_params);

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
            struct pos pos = osd->packer->result[n];

            // NOTE: the blend color is used with SUBBITMAP_LIBASS only, so it
            //       doesn't matter that we upload garbage for the other formats
            uint32_t c = b->libass.color;
            uint8_t color[4] = { c >> 24, (c >> 16) & 0xff,
                                (c >> 8) & 0xff, 255 - (c & 0xff) };

            write_quad(&va[osd->num_vertices],
                    b->x, b->y, b->x + b->dw, b->y + b->dh,
                    pos.x, pos.y, pos.x + b->w, pos.y + b->h,
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

static bool test_fbo(struct gl_video *p, GLenum format)
{
    static const float vals[] = {
        127 / 255.0f,                   // full 8 bit integer
        32767 / 65535.0f,               // full 16 bit integer
        0xFFFFFF / (float)(1 << 25),    // float mantissa
        2,                              // out of range value
    };
    static const char *val_names[] = {
        "8-bit precision",
        "16-bit precision",
        "full float",
        "out of range value (2)",
    };

    GL *gl = p->gl;
    bool success = false;
    struct fbotex fbo = {0};
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    gl->PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl->PixelStorei(GL_PACK_ROW_LENGTH, 0);
    if (fbotex_init(p, &fbo, 16, 16, format)) {
        gl->BindFramebuffer(GL_FRAMEBUFFER, fbo.fbo);
        gl->ReadBuffer(GL_COLOR_ATTACHMENT0);
        for (int i = 0; i < 4; i++) {
            float pixel = -1;
            float val = vals[i];
            gl->ClearColor(val, 0.0f, 0.0f, 1.0f);
            gl->Clear(GL_COLOR_BUFFER_BIT);
            gl->ReadPixels(0, 0, 1, 1, GL_RED, GL_FLOAT, &pixel);
            MP_VERBOSE(p, "   %s: %a\n", val_names[i], val - pixel);
        }
        gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
        glCheckError(gl, p->log, "after FBO read");
        success = true;
    }
    fbotex_uninit(p, &fbo);
    glCheckError(gl, p->log, "FBO test");
    gl->ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    return success;
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
        MP_VERBOSE(p, "Testing user-set FBO format\n");
        have_fbo = test_fbo(p, p->opts.fbo_format);
    }

    // fruit dithering mode and the 3D lut use this texture format
    if (have_fbo && ((p->opts.dither_depth >= 0 && p->opts.dither_algo == 0) ||
                     p->use_lut_3d))
    {
        // doesn't disable anything; it's just for the log
        MP_VERBOSE(p, "Testing GL_R16 FBO (dithering/LUT)\n");
        test_fbo(p, GL_R16);
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
        MP_ERR(p, "Some OpenGL extensions not detected, "
               "disabling: ");
        for (int n = 0; n < n_disabled; n++) {
            if (n)
                MP_ERR(p, ", ");
            MP_ERR(p, "%s", disabled[n]);
        }
        MP_ERR(p, ".\n");
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

// dest = src.<w> (always using 4 components)
static void packed_fmt_swizzle(char w[5], const struct packed_fmt_entry *fmt)
{
    for (int c = 0; c < 4; c++)
        w[c] = "rgba"[MPMAX(fmt->components[c] - 1, 0)];
    w[4] = '\0';
}

static const struct fmt_entry *find_tex_format(int bytes_per_comp, int n_channels)
{
    assert(bytes_per_comp == 1 || bytes_per_comp == 2);
    assert(n_channels >= 1 && n_channels <= 4);
    return &gl_byte_formats[n_channels - 1 + (bytes_per_comp - 1) * 4];
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

    const struct fmt_entry *plane_format[4] = {0};

    init->image_format = fmt;
    init->plane_bits = desc.bpp[0];
    init->color_swizzle[0] = '\0';
    init->has_alpha = false;

    // YUV/planar formats
    if (!supported && (desc.flags & MP_IMGFLAG_YUV_P)) {
        int bits = desc.plane_bits;
        if ((desc.flags & MP_IMGFLAG_NE) && bits >= 8 && bits <= 16) {
            supported = true;
            init->plane_bits = bits;
            init->has_alpha = desc.num_planes > 3;
            plane_format[0] = find_tex_format((bits + 7) / 8, 1);
            for (int p = 1; p < desc.num_planes; p++)
                plane_format[p] = plane_format[0];
        }
    }

    // YUV/half-packed
    if (!supported && (fmt == IMGFMT_NV12 || fmt == IMGFMT_NV21)) {
        supported = true;
        plane_format[0] = find_tex_format(1, 1);
        plane_format[1] = find_tex_format(1, 2);
        if (fmt == IMGFMT_NV21)
            snprintf(init->color_swizzle, sizeof(init->color_swizzle), "rbga");
    }

    // RGB/planar
    if (!supported && fmt == IMGFMT_GBRP) {
        supported = true;
        snprintf(init->color_swizzle, sizeof(init->color_swizzle), "brga");
        plane_format[0] = find_tex_format(1, 1);
        for (int p = 1; p < desc.num_planes; p++)
            plane_format[p] = plane_format[0];
    }

    // XYZ (same organization as RGB packed, but requires conversion matrix)
    if (!supported && fmt == IMGFMT_XYZ12) {
        supported = true;
        plane_format[0] = find_tex_format(2, 3);
    }

    // Packed RGB special formats
    if (!supported) {
        for (const struct fmt_entry *e = mp_to_gl_formats; e->mp_format; e++) {
            if (e->mp_format == fmt) {
                supported = true;
                plane_format[0] = e;
                break;
            }
        }
    }

    // Packed RGB(A) formats
    if (!supported) {
        for (const struct packed_fmt_entry *e = mp_packed_formats; e->fmt; e++) {
            if (e->fmt == fmt) {
                supported = true;
                int n_comp = desc.bytes[0] / e->component_size;
                plane_format[0] = find_tex_format(e->component_size, n_comp);
                packed_fmt_swizzle(init->color_swizzle, e);
                init->has_alpha = e->components[3] != 0;
                break;
            }
        }
    }

    if (!supported)
        return false;

    // Stuff like IMGFMT_420AP10. Untested, most likely insane.
    if (desc.num_planes == 4 && (init->plane_bits % 8) != 0)
        return false;

    for (int p = 0; p < desc.num_planes; p++) {
        struct texplane *plane = &init->image.planes[p];
        const struct fmt_entry *format = plane_format[p];
        assert(format);
        plane->gl_format = format->format;
        plane->gl_internal_format = format->internal_format;
        plane->gl_type = format->type;
    }

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

void gl_video_config(struct gl_video *p, struct mp_image_params *params)
{
    if (p->image_format != params->imgfmt || p->image_w != params->w ||
        p->image_h != params->h)
    {
        uninit_video(p);
        p->image_w = params->w;
        p->image_h = params->h;
        init_format(params->imgfmt, p);
        init_video(p);
    }
    p->image_dw = params->d_w;
    p->image_dh = params->d_h;
    p->image_params = *params;

    struct mp_csp_details csp = MP_CSP_DETAILS_DEFAULTS;
    csp.levels_in = params->colorlevels;
    csp.levels_out = params->outputlevels;
    csp.format = params->colorspace;
    p->colorspace = csp;
}

void gl_video_set_output_depth(struct gl_video *p, int r, int g, int b)
{
    MP_VERBOSE(p, "Display depth: R=%d, G=%d, B=%d\n", r, g, b);
    p->depth_g = g;
}

struct gl_video *gl_video_init(GL *gl, struct mp_log *log)
{
    struct gl_video *p = talloc_ptrtype(NULL, p);
    *p = (struct gl_video) {
        .gl = gl,
        .log = log,
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

bool gl_video_set_equalizer(struct gl_video *p, const char *name, int val)
{
    if (mp_csp_equalizer_set(&p->video_eq, name, val) >= 0) {
        if (!p->opts.gamma && p->video_eq.values[MP_CSP_EQ_GAMMA] != 0) {
            MP_VERBOSE(p, "Auto-enabling gamma.\n");
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
    if (bstr_equals0(param, "help")) {
        mp_msg(MSGT_VO, MSGL_INFO, "Available scalers:\n");
        for (const char **filter = fixed_scale_filters; *filter; filter++)
            mp_msg(MSGT_VO, MSGL_INFO, "    %s\n", *filter);
        for (int n = 0; mp_filter_kernels[n].name; n++)
            mp_msg(MSGT_VO, MSGL_INFO, "    %s\n", mp_filter_kernels[n].name);
        return M_OPT_EXIT - 1;
    }
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
