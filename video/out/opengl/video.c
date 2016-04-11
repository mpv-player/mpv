/*
 * This file is part of mpv.
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

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <libavutil/common.h>
#include <libavutil/lfg.h>

#include "video.h"

#include "misc/bstr.h"
#include "options/m_config.h"
#include "common/global.h"
#include "options/options.h"
#include "common.h"
#include "utils.h"
#include "hwdec.h"
#include "osd.h"
#include "stream/stream.h"
#include "superxbr.h"
#include "nnedi3.h"
#include "video_shaders.h"
#include "video/out/filter_kernels.h"
#include "video/out/aspect.h"
#include "video/out/bitmap_packer.h"
#include "video/out/dither.h"
#include "video/out/vo.h"

// Maximal number of passes that prescaler can be applied.
#define MAX_PRESCALE_PASSES 5

// Maximal number of steps each pass of prescaling contains
#define MAX_PRESCALE_STEPS 2

// scale/cscale arguments that map directly to shader filter routines.
// Note that the convolution filters are not included in this list.
static const char *const fixed_scale_filters[] = {
    "bilinear",
    "bicubic_fast",
    "oversample",
    "custom",
    NULL
};
static const char *const fixed_tscale_filters[] = {
    "oversample",
    NULL
};

// must be sorted, and terminated with 0
int filter_sizes[] =
    {2, 4, 6, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 0};
int tscale_sizes[] = {2, 4, 6, 0}; // limited by TEXUNIT_VIDEO_NUM

struct vertex_pt {
    float x, y;
};

struct vertex {
    struct vertex_pt position;
    struct vertex_pt texcoord[TEXUNIT_VIDEO_NUM];
};

static const struct gl_vao_entry vertex_vao[] = {
    {"position", 2, GL_FLOAT, false, offsetof(struct vertex, position)},
    {"texcoord0", 2, GL_FLOAT, false, offsetof(struct vertex, texcoord[0])},
    {"texcoord1", 2, GL_FLOAT, false, offsetof(struct vertex, texcoord[1])},
    {"texcoord2", 2, GL_FLOAT, false, offsetof(struct vertex, texcoord[2])},
    {"texcoord3", 2, GL_FLOAT, false, offsetof(struct vertex, texcoord[3])},
    {"texcoord4", 2, GL_FLOAT, false, offsetof(struct vertex, texcoord[4])},
    {"texcoord5", 2, GL_FLOAT, false, offsetof(struct vertex, texcoord[5])},
    {0}
};

struct texplane {
    int w, h;
    GLint gl_internal_format;
    GLenum gl_target;
    bool use_integer;
    GLenum gl_format;
    GLenum gl_type;
    GLuint gl_texture;
    int gl_buffer;
};

struct video_image {
    struct texplane planes[4];
    bool image_flipped;
    struct mp_image *mpi;       // original input image
};

enum plane_type {
    PLANE_NONE = 0,
    PLANE_RGB,
    PLANE_LUMA,
    PLANE_CHROMA,
    PLANE_ALPHA,
    PLANE_XYZ,
};

// A self-contained description of a source image which can be bound to a
// texture unit and sampled from. Contains metadata about how it's to be used
struct img_tex {
    enum plane_type type; // must be set to something non-zero
    int components; // number of relevant coordinates
    float multiplier; // multiplier to be used when sampling
    GLuint gl_tex;
    GLenum gl_target;
    bool use_integer;
    int tex_w, tex_h; // source texture size
    int w, h; // logical size (with pre_transform applied)
    struct gl_transform pre_transform; // source texture space
    struct gl_transform transform; // rendering transformation
    bool texture_la; // it's a GL_LUMINANCE_ALPHA texture (access with .ra not .rg)
};

struct fbosurface {
    struct fbotex fbotex;
    double pts;
};

#define FBOSURFACES_MAX 10

struct cached_file {
    char *path;
    char *body;
};

struct gl_video {
    GL *gl;

    struct mpv_global *global;
    struct mp_log *log;
    struct gl_video_opts opts;
    struct gl_lcms *cms;
    bool gl_debug;

    int texture_16bit_depth;    // actual bits available in 16 bit textures

    struct gl_shader_cache *sc;

    GLenum gl_target; // texture target (GL_TEXTURE_2D, ...) for video and FBOs

    struct gl_vao vao;

    struct osd_state *osd_state;
    struct mpgl_osd *osd;
    double osd_pts;

    GLuint lut_3d_texture;
    bool use_lut_3d;

    GLuint dither_texture;
    int dither_size;

    GLuint nnedi3_weights_buffer;

    struct mp_image_params real_image_params;   // configured format
    struct mp_image_params image_params;        // texture format (mind hwdec case)
    struct mp_imgfmt_desc image_desc;
    int plane_count;

    bool is_yuv, is_packed_yuv;
    bool has_alpha;
    char color_swizzle[5];
    bool use_integer_conversion;

    struct video_image image;

    bool dumb_mode;
    bool forced_dumb_mode;

    struct fbotex merge_fbo[4];
    struct fbotex deband_fbo[4];
    struct fbotex scale_fbo[4];
    struct fbotex integer_fbo[4];
    struct fbotex indirect_fbo;
    struct fbotex blend_subs_fbo;
    struct fbotex unsharp_fbo;
    struct fbotex output_fbo;
    struct fbosurface surfaces[FBOSURFACES_MAX];

    // these are duplicated so we can keep rendering back and forth between
    // them to support an unlimited number of shader passes per step
    struct fbotex pre_fbo[2];
    struct fbotex post_fbo[2];

    struct fbotex prescale_fbo[MAX_PRESCALE_PASSES][MAX_PRESCALE_STEPS];

    int surface_idx;
    int surface_now;
    int frames_drawn;
    bool is_interpolated;
    bool output_fbo_valid;

    // state for configured scalers
    struct scaler scaler[SCALER_COUNT];

    struct mp_csp_equalizer video_eq;

    struct mp_rect src_rect;    // displayed part of the source video
    struct mp_rect dst_rect;    // video rectangle on output window
    struct mp_osd_res osd_rect; // OSD size/margins
    int vp_w, vp_h;

    // temporary during rendering
    struct img_tex pass_tex[TEXUNIT_VIDEO_NUM];
    int pass_tex_num;
    int texture_w, texture_h;
    struct gl_transform texture_offset; // texture transform without rotation
    int components;
    bool use_linear;
    float user_gamma;

    int frames_uploaded;
    int frames_rendered;
    AVLFG lfg;

    // Cached because computing it can take relatively long
    int last_dither_matrix_size;
    float *last_dither_matrix;

    struct cached_file files[10];
    int num_files;

    struct gl_hwdec *hwdec;
    bool hwdec_active;

    bool dsi_warned;
    bool custom_shader_fn_warned;
};

struct fmt_entry {
    int mp_format;
    GLint internal_format;
    GLenum format;
    GLenum type;
};

// Very special formats, for which OpenGL happens to have direct support
static const struct fmt_entry mp_to_gl_formats[] = {
    {IMGFMT_RGB565,  GL_RGB,   GL_RGB,  GL_UNSIGNED_SHORT_5_6_5},
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

static const struct fmt_entry gl_byte_formats_gles3[] = {
    {0, GL_R8,       GL_RED,    GL_UNSIGNED_BYTE},      // 1 x 8
    {0, GL_RG8,      GL_RG,     GL_UNSIGNED_BYTE},      // 2 x 8
    {0, GL_RGB8,     GL_RGB,    GL_UNSIGNED_BYTE},      // 3 x 8
    {0, GL_RGBA8,    GL_RGBA,   GL_UNSIGNED_BYTE},      // 4 x 8
    // There are no filterable texture formats that can be uploaded as
    // GL_UNSIGNED_SHORT, so apparently we're out of luck.
    {0, 0,           0,         0},                     // 1 x 16
    {0, 0,           0,         0},                     // 2 x 16
    {0, 0,           0,         0},                     // 3 x 16
    {0, 0,           0,         0},                     // 4 x 16
};

static const struct fmt_entry gl_ui_byte_formats_gles3[] = {
    {0, GL_R8UI,      GL_RED_INTEGER,   GL_UNSIGNED_BYTE},  // 1 x 8
    {0, GL_RG8UI,     GL_RG_INTEGER,    GL_UNSIGNED_BYTE},  // 2 x 8
    {0, GL_RGB8UI,    GL_RGB_INTEGER,   GL_UNSIGNED_BYTE},  // 3 x 8
    {0, GL_RGBA8UI,   GL_RGBA_INTEGER,  GL_UNSIGNED_BYTE},  // 4 x 8
    {0, GL_R16UI,     GL_RED_INTEGER,   GL_UNSIGNED_SHORT}, // 1 x 16
    {0, GL_RG16UI,    GL_RG_INTEGER,    GL_UNSIGNED_SHORT}, // 2 x 16
    {0, GL_RGB16UI,   GL_RGB_INTEGER,   GL_UNSIGNED_SHORT}, // 3 x 16
    {0, GL_RGBA16UI,  GL_RGBA_INTEGER,  GL_UNSIGNED_SHORT}, // 4 x 16
};

static const struct fmt_entry gl_byte_formats_gles2[] = {
    {0, GL_LUMINANCE,           GL_LUMINANCE,       GL_UNSIGNED_BYTE}, // 1 x 8
    {0, GL_LUMINANCE_ALPHA,     GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE}, // 2 x 8
    {0, GL_RGB,                 GL_RGB,             GL_UNSIGNED_BYTE}, // 3 x 8
    {0, GL_RGBA,                GL_RGBA,            GL_UNSIGNED_BYTE}, // 4 x 8
    {0, 0,                      0,                  0},                // 1 x 16
    {0, 0,                      0,                  0},                // 2 x 16
    {0, 0,                      0,                  0},                // 3 x 16
    {0, 0,                      0,                  0},                // 4 x 16
};

static const struct fmt_entry gl_byte_formats_legacy[] = {
    {0, GL_LUMINANCE,           GL_LUMINANCE,       GL_UNSIGNED_BYTE}, // 1 x 8
    {0, GL_LUMINANCE_ALPHA,     GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE}, // 2 x 8
    {0, GL_RGB,                 GL_RGB,             GL_UNSIGNED_BYTE}, // 3 x 8
    {0, GL_RGBA,                GL_RGBA,            GL_UNSIGNED_BYTE}, // 4 x 8
    {0, GL_LUMINANCE16,         GL_LUMINANCE,       GL_UNSIGNED_SHORT},// 1 x 16
    {0, GL_LUMINANCE16_ALPHA16, GL_LUMINANCE_ALPHA, GL_UNSIGNED_SHORT},// 2 x 16
    {0, GL_RGB16,               GL_RGB,             GL_UNSIGNED_SHORT},// 3 x 16
    {0, GL_RGBA16,              GL_RGBA,            GL_UNSIGNED_SHORT},// 4 x 16
};

static const struct fmt_entry gl_float16_formats[] = {
    {0, GL_R16F,    GL_RED,     GL_FLOAT},              // 1 x f
    {0, GL_RG16F,   GL_RG,      GL_FLOAT},              // 2 x f
    {0, GL_RGB16F,  GL_RGB,     GL_FLOAT},              // 3 x f
    {0, GL_RGBA16F, GL_RGBA,    GL_FLOAT},              // 4 x f
};

static const struct fmt_entry gl_apple_formats[] = {
    {IMGFMT_UYVY, GL_RGB, GL_RGB_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE},
    {IMGFMT_YUYV, GL_RGB, GL_RGB_422_APPLE, GL_UNSIGNED_SHORT_8_8_REV_APPLE},
    {0}
};

struct packed_fmt_entry {
    int fmt;
    int8_t component_size;
    int8_t components[4]; // source component - 0 means unmapped
};

static const struct packed_fmt_entry mp_packed_formats[] = {
    //                  w   R  G  B  A
    {IMGFMT_Y8,         1, {1, 0, 0, 0}},
    {IMGFMT_Y16,        2, {1, 0, 0, 0}},
    {IMGFMT_YA8,        1, {1, 0, 0, 2}},
    {IMGFMT_YA16,       2, {1, 0, 0, 2}},
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

const struct gl_video_opts gl_video_opts_def = {
    .dither_depth = -1,
    .dither_size = 6,
    .temporal_dither_period = 1,
    .fbo_format = 0,
    .sigmoid_center = 0.75,
    .sigmoid_slope = 6.5,
    .scaler = {
        {{"bilinear",   .params={NAN, NAN}}, {.params = {NAN, NAN}}}, // scale
        {{NULL,         .params={NAN, NAN}}, {.params = {NAN, NAN}}}, // dscale
        {{"bilinear",   .params={NAN, NAN}}, {.params = {NAN, NAN}}}, // cscale
        {{"mitchell",   .params={NAN, NAN}}, {.params = {NAN, NAN}},
         .clamp = 1, }, // tscale
    },
    .scaler_resizes_only = 1,
    .scaler_lut_size = 6,
    .interpolation_threshold = 0.0001,
    .alpha_mode = 3,
    .background = {0, 0, 0, 255},
    .gamma = 1.0f,
    .prescale_passes = 1,
    .prescale_downscaling_threshold = 2.0f,
};

const struct gl_video_opts gl_video_opts_hq_def = {
    .dither_depth = 0,
    .dither_size = 6,
    .temporal_dither_period = 1,
    .fbo_format = 0,
    .correct_downscaling = 1,
    .sigmoid_center = 0.75,
    .sigmoid_slope = 6.5,
    .sigmoid_upscaling = 1,
    .scaler = {
        {{"spline36",   .params={NAN, NAN}}, {.params = {NAN, NAN}}}, // scale
        {{"mitchell",   .params={NAN, NAN}}, {.params = {NAN, NAN}}}, // dscale
        {{"spline36",   .params={NAN, NAN}}, {.params = {NAN, NAN}}}, // cscale
        {{"mitchell",   .params={NAN, NAN}}, {.params = {NAN, NAN}},
         .clamp = 1, }, // tscale
    },
    .scaler_resizes_only = 1,
    .scaler_lut_size = 6,
    .interpolation_threshold = 0.0001,
    .alpha_mode = 3,
    .background = {0, 0, 0, 255},
    .gamma = 1.0f,
    .blend_subs = 0,
    .deband = 1,
    .prescale_passes = 1,
    .prescale_downscaling_threshold = 2.0f,
};

static int validate_scaler_opt(struct mp_log *log, const m_option_t *opt,
                               struct bstr name, struct bstr param);

static int validate_window_opt(struct mp_log *log, const m_option_t *opt,
                               struct bstr name, struct bstr param);

#define OPT_BASE_STRUCT struct gl_video_opts

#define SCALER_OPTS(n, i) \
    OPT_STRING_VALIDATE(n, scaler[i].kernel.name, 0, validate_scaler_opt), \
    OPT_FLOAT(n"-param1", scaler[i].kernel.params[0], 0),                  \
    OPT_FLOAT(n"-param2", scaler[i].kernel.params[1], 0),                  \
    OPT_FLOAT(n"-blur",   scaler[i].kernel.blur, 0),                       \
    OPT_FLOAT(n"-wparam", scaler[i].window.params[0], 0),                  \
    OPT_FLAG(n"-clamp",   scaler[i].clamp, 0),                             \
    OPT_FLOATRANGE(n"-radius",    scaler[i].radius, 0, 0.5, 16.0),         \
    OPT_FLOATRANGE(n"-antiring",  scaler[i].antiring, 0, 0.0, 1.0),        \
    OPT_STRING_VALIDATE(n"-window", scaler[i].window.name, 0, validate_window_opt)

const struct m_sub_options gl_video_conf = {
    .opts = (const m_option_t[]) {
        OPT_FLAG("dumb-mode", dumb_mode, 0),
        OPT_FLOATRANGE("gamma", gamma, 0, 0.1, 2.0),
        OPT_FLAG("gamma-auto", gamma_auto, 0),
        OPT_CHOICE_C("target-prim", target_prim, 0, mp_csp_prim_names),
        OPT_CHOICE_C("target-trc", target_trc, 0, mp_csp_trc_names),
        OPT_FLAG("pbo", pbo, 0),
        SCALER_OPTS("scale",  SCALER_SCALE),
        SCALER_OPTS("dscale", SCALER_DSCALE),
        SCALER_OPTS("cscale", SCALER_CSCALE),
        SCALER_OPTS("tscale", SCALER_TSCALE),
        OPT_INTRANGE("scaler-lut-size", scaler_lut_size, 0, 4, 10),
        OPT_FLAG("scaler-resizes-only", scaler_resizes_only, 0),
        OPT_FLAG("linear-scaling", linear_scaling, 0),
        OPT_FLAG("correct-downscaling", correct_downscaling, 0),
        OPT_FLAG("sigmoid-upscaling", sigmoid_upscaling, 0),
        OPT_FLOATRANGE("sigmoid-center", sigmoid_center, 0, 0.0, 1.0),
        OPT_FLOATRANGE("sigmoid-slope", sigmoid_slope, 0, 1.0, 20.0),
        OPT_CHOICE("fbo-format", fbo_format, 0,
                   ({"rgb",    GL_RGB},
                    {"rgba",   GL_RGBA},
                    {"rgb8",   GL_RGB8},
                    {"rgba8",  GL_RGBA8},
                    {"rgb10",  GL_RGB10},
                    {"rgb10_a2", GL_RGB10_A2},
                    {"rgb16",  GL_RGB16},
                    {"rgb16f", GL_RGB16F},
                    {"rgb32f", GL_RGB32F},
                    {"rgba12", GL_RGBA12},
                    {"rgba16", GL_RGBA16},
                    {"rgba16f", GL_RGBA16F},
                    {"rgba32f", GL_RGBA32F},
                    {"auto",   0})),
        OPT_CHOICE_OR_INT("dither-depth", dither_depth, 0, -1, 16,
                          ({"no", -1}, {"auto", 0})),
        OPT_CHOICE("dither", dither_algo, 0,
                   ({"fruit", 0}, {"ordered", 1}, {"no", -1})),
        OPT_INTRANGE("dither-size-fruit", dither_size, 0, 2, 8),
        OPT_FLAG("temporal-dither", temporal_dither, 0),
        OPT_INTRANGE("temporal-dither-period", temporal_dither_period, 0, 1, 128),
        OPT_CHOICE("alpha", alpha_mode, 0,
                   ({"no", 0},
                    {"yes", 1},
                    {"blend", 2},
                    {"blend-tiles", 3})),
        OPT_FLAG("rectangle-textures", use_rectangle, 0),
        OPT_COLOR("background", background, 0),
        OPT_FLAG("interpolation", interpolation, 0),
        OPT_FLOAT("interpolation-threshold", interpolation_threshold, 0),
        OPT_CHOICE("blend-subtitles", blend_subs, 0,
                   ({"no", 0},
                    {"yes", 1},
                    {"video", 2})),
        OPT_STRING("scale-shader", scale_shader, 0),
        OPT_STRINGLIST("pre-shaders", pre_shaders, 0),
        OPT_STRINGLIST("post-shaders", post_shaders, 0),
        OPT_FLAG("deband", deband, 0),
        OPT_SUBSTRUCT("deband", deband_opts, deband_conf, 0),
        OPT_FLOAT("sharpen", unsharp, 0),
        OPT_CHOICE("prescale-luma", prescale_luma, 0,
                   ({"none", 0},
                    {"superxbr", 1}
#if HAVE_NNEDI
                    , {"nnedi3", 2}
#endif
                    )),
        OPT_INTRANGE("prescale-passes",
                     prescale_passes, 0, 1, MAX_PRESCALE_PASSES),
        OPT_FLOATRANGE("prescale-downscaling-threshold",
                       prescale_downscaling_threshold, 0, 0.0, 32.0),
        OPT_SUBSTRUCT("superxbr", superxbr_opts, superxbr_conf, 0),
        OPT_SUBSTRUCT("nnedi3", nnedi3_opts, nnedi3_conf, 0),

        OPT_REMOVED("approx-gamma", "this is always enabled now"),
        OPT_REMOVED("cscale-down", "chroma is never downscaled"),
        OPT_REMOVED("scale-sep", "this is set automatically whenever sane"),
        OPT_REMOVED("indirect", "this is set automatically whenever sane"),
        OPT_REMOVED("srgb", "use target-prim=bt709:target-trc=srgb instead"),
        OPT_REMOVED("source-shader", "use :deband to enable debanding"),

        OPT_REPLACED("lscale", "scale"),
        OPT_REPLACED("lscale-down", "scale-down"),
        OPT_REPLACED("lparam1", "scale-param1"),
        OPT_REPLACED("lparam2", "scale-param2"),
        OPT_REPLACED("lradius", "scale-radius"),
        OPT_REPLACED("lantiring", "scale-antiring"),
        OPT_REPLACED("cparam1", "cscale-param1"),
        OPT_REPLACED("cparam2", "cscale-param2"),
        OPT_REPLACED("cradius", "cscale-radius"),
        OPT_REPLACED("cantiring", "cscale-antiring"),
        OPT_REPLACED("smoothmotion", "interpolation"),
        OPT_REPLACED("smoothmotion-threshold", "tscale-param1"),
        OPT_REPLACED("scale-down", "dscale"),
        OPT_REPLACED("fancy-downscaling", "correct-downscaling"),
        OPT_REPLACED("prescale", "prescale-luma"),

        {0}
    },
    .size = sizeof(struct gl_video_opts),
    .defaults = &gl_video_opts_def,
};

static void uninit_rendering(struct gl_video *p);
static void uninit_scaler(struct gl_video *p, struct scaler *scaler);
static void check_gl_features(struct gl_video *p);
static bool init_format(int fmt, struct gl_video *init);
static void gl_video_upload_image(struct gl_video *p, struct mp_image *mpi);
static void assign_options(struct gl_video_opts *dst, struct gl_video_opts *src);
static void get_scale_factors(struct gl_video *p, bool transpose_rot, double xy[2]);

#define GLSL(x) gl_sc_add(p->sc, #x "\n");
#define GLSLF(...) gl_sc_addf(p->sc, __VA_ARGS__)
#define GLSLHF(...) gl_sc_haddf(p->sc, __VA_ARGS__)

// Return a fixed point texture format with given characteristics.
static const struct fmt_entry *find_tex_format(GL *gl, int bytes_per_comp,
                                               int n_channels)
{
    assert(bytes_per_comp == 1 || bytes_per_comp == 2);
    assert(n_channels >= 1 && n_channels <= 4);
    const struct fmt_entry *fmts = gl_byte_formats;
    if (gl->es >= 300) {
        fmts = gl_byte_formats_gles3;
    } else if (gl->es) {
        fmts = gl_byte_formats_gles2;
    } else if (!(gl->mpgl_caps & MPGL_CAP_TEX_RG)) {
        fmts = gl_byte_formats_legacy;
    }
    return &fmts[n_channels - 1 + (bytes_per_comp - 1) * 4];
}

static bool is_integer_format(const struct fmt_entry *fmt)
{
    // Tests only the formats which we actually declare somewhere.
    switch (fmt->format) {
    case GL_RED_INTEGER:
    case GL_RG_INTEGER:
    case GL_RGB_INTEGER:
    case GL_RGBA_INTEGER:
        return true;
    }
    return false;
}

static const char *load_cached_file(struct gl_video *p, const char *path)
{
    if (!path || !path[0])
        return NULL;
    for (int n = 0; n < p->num_files; n++) {
        if (strcmp(p->files[n].path, path) == 0)
            return p->files[n].body;
    }
    // not found -> load it
    if (p->num_files == MP_ARRAY_SIZE(p->files)) {
        // empty cache when it overflows
        for (int n = 0; n < p->num_files; n++) {
            talloc_free(p->files[n].path);
            talloc_free(p->files[n].body);
        }
        p->num_files = 0;
    }
    struct bstr s = stream_read_file(path, p, p->global, 100000); // 100 kB
    if (s.len) {
        struct cached_file *new = &p->files[p->num_files++];
        *new = (struct cached_file) {
            .path = talloc_strdup(p, path),
            .body = s.start
        };
        return new->body;
    }
    return NULL;
}

static void debug_check_gl(struct gl_video *p, const char *msg)
{
    if (p->gl_debug)
        glCheckError(p->gl, p->log, msg);
}

void gl_video_set_debug(struct gl_video *p, bool enable)
{
    GL *gl = p->gl;

    p->gl_debug = enable;
    if (p->gl->debug_context)
        gl_set_debug_logger(gl, enable ? p->log : NULL);
}

static void gl_video_reset_surfaces(struct gl_video *p)
{
    for (int i = 0; i < FBOSURFACES_MAX; i++)
        p->surfaces[i].pts = MP_NOPTS_VALUE;
    p->surface_idx = 0;
    p->surface_now = 0;
    p->frames_drawn = 0;
    p->output_fbo_valid = false;
}

static inline int fbosurface_wrap(int id)
{
    id = id % FBOSURFACES_MAX;
    return id < 0 ? id + FBOSURFACES_MAX : id;
}

static void recreate_osd(struct gl_video *p)
{
    mpgl_osd_destroy(p->osd);
    p->osd = NULL;
    if (p->osd_state) {
        p->osd = mpgl_osd_init(p->gl, p->log, p->osd_state);
        mpgl_osd_set_options(p->osd, p->opts.pbo);
    }
}

static void reinit_rendering(struct gl_video *p)
{
    MP_VERBOSE(p, "Reinit rendering.\n");

    debug_check_gl(p, "before scaler initialization");

    uninit_rendering(p);

    recreate_osd(p);
}

static void uninit_rendering(struct gl_video *p)
{
    GL *gl = p->gl;

    for (int n = 0; n < SCALER_COUNT; n++)
        uninit_scaler(p, &p->scaler[n]);

    gl->DeleteTextures(1, &p->dither_texture);
    p->dither_texture = 0;

    gl->DeleteBuffers(1, &p->nnedi3_weights_buffer);
    p->nnedi3_weights_buffer = 0;

    for (int n = 0; n < 4; n++) {
        fbotex_uninit(&p->merge_fbo[n]);
        fbotex_uninit(&p->deband_fbo[n]);
        fbotex_uninit(&p->scale_fbo[n]);
        fbotex_uninit(&p->integer_fbo[n]);
    }

    fbotex_uninit(&p->indirect_fbo);
    fbotex_uninit(&p->blend_subs_fbo);
    fbotex_uninit(&p->unsharp_fbo);

    for (int n = 0; n < 2; n++) {
        fbotex_uninit(&p->pre_fbo[n]);
        fbotex_uninit(&p->post_fbo[n]);
    }

    for (int pass = 0; pass < MAX_PRESCALE_PASSES; pass++) {
        for (int step = 0; step < MAX_PRESCALE_STEPS; step++)
            fbotex_uninit(&p->prescale_fbo[pass][step]);
    }

    for (int n = 0; n < FBOSURFACES_MAX; n++)
        fbotex_uninit(&p->surfaces[n].fbotex);

    gl_video_reset_surfaces(p);
}

void gl_video_update_profile(struct gl_video *p)
{
    if (p->use_lut_3d)
        return;

    p->use_lut_3d = true;
    check_gl_features(p);

    reinit_rendering(p);
}

static bool gl_video_get_lut3d(struct gl_video *p, enum mp_csp_prim prim,
                               enum mp_csp_trc trc)
{
    GL *gl = p->gl;

    if (!p->cms || !p->use_lut_3d)
        return false;

    if (!gl_lcms_has_changed(p->cms, prim, trc))
        return true;

    struct lut3d *lut3d = NULL;
    if (!gl_lcms_get_lut3d(p->cms, &lut3d, prim, trc) || !lut3d) {
        return false;
    }

    if (!p->lut_3d_texture)
        gl->GenTextures(1, &p->lut_3d_texture);

    gl->ActiveTexture(GL_TEXTURE0 + TEXUNIT_3DLUT);
    gl->BindTexture(GL_TEXTURE_3D, p->lut_3d_texture);
    gl->TexImage3D(GL_TEXTURE_3D, 0, GL_RGB16, lut3d->size[0], lut3d->size[1],
                   lut3d->size[2], 0, GL_RGB, GL_UNSIGNED_SHORT, lut3d->data);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    gl->ActiveTexture(GL_TEXTURE0);

    debug_check_gl(p, "after 3d lut creation");

    return true;
}

// Fill an img_tex struct from an FBO + some metadata
static struct img_tex img_tex_fbo(struct fbotex *fbo, struct gl_transform t,
                                  enum plane_type type, int components)
{
    assert(type != PLANE_NONE);
    return (struct img_tex){
        .type = type,
        .gl_tex = fbo->texture,
        .gl_target = GL_TEXTURE_2D,
        .multiplier = 1.0,
        .use_integer = false,
        .tex_w = fbo->rw,
        .tex_h = fbo->rh,
        .w = fbo->lw,
        .h = fbo->lh,
        .pre_transform = identity_trans,
        .transform = t,
        .components = components,
    };
}

// Bind an img_tex to a free texture unit and return its ID. At most
// TEXUNIT_VIDEO_NUM texture units can be bound at once
static int pass_bind(struct gl_video *p, struct img_tex tex)
{
    assert(p->pass_tex_num < TEXUNIT_VIDEO_NUM);
    p->pass_tex[p->pass_tex_num] = tex;
    return p->pass_tex_num++;
}

// Rotation by 90° and flipping.
static void get_plane_source_transform(struct gl_video *p, int w, int h,
                                       struct gl_transform *out_tr)
{
    struct gl_transform tr = identity_trans;
    int a = p->image_params.rotate % 90 ? 0 : p->image_params.rotate / 90;
    int sin90[4] = {0, 1, 0, -1}; // just to avoid rounding issues etc.
    int cos90[4] = {1, 0, -1, 0};
    struct gl_transform rot = {{{cos90[a], sin90[a]}, {-sin90[a], cos90[a]}}};
    gl_transform_trans(rot, &tr);

    // basically, recenter to keep the whole image in view
    float b[2] = {1, 1};
    gl_transform_vec(rot, &b[0], &b[1]);
    tr.t[0] += b[0] < 0 ? w : 0;
    tr.t[1] += b[1] < 0 ? h : 0;

    if (p->image.image_flipped) {
        struct gl_transform flip = {{{1, 0}, {0, -1}}, {0, h}};
        gl_transform_trans(flip, &tr);
    }

    *out_tr = tr;
}

// Places a video_image's image textures + associated metadata into tex[]. The
// number of textures is equal to p->plane_count.
static void pass_get_img_tex(struct gl_video *p, struct video_image *vimg,
                             struct img_tex tex[4])
{
    assert(vimg->mpi);

    // Determine the chroma offset
    struct gl_transform chroma = (struct gl_transform){{{0}}};

    float ls_w = 1.0 / (1 << p->image_desc.chroma_xs);
    float ls_h = 1.0 / (1 << p->image_desc.chroma_ys);

    if (p->image_params.chroma_location != MP_CHROMA_CENTER) {
        int cx, cy;
        mp_get_chroma_location(p->image_params.chroma_location, &cx, &cy);
        // By default texture coordinates are such that chroma is centered with
        // any chroma subsampling. If a specific direction is given, make it
        // so that the luma and chroma sample line up exactly.
        // For 4:4:4, setting chroma location should have no effect at all.
        // luma sample size (in chroma coord. space)
        chroma.t[0] = ls_w < 1 ? ls_w * -cx / 2 : 0;
        chroma.t[1] = ls_h < 1 ? ls_h * -cy / 2 : 0;
    }

    // Make sure luma/chroma sizes are aligned.
    // Example: For 4:2:0 with size 3x3, the subsampled chroma plane is 2x2
    // so luma (3,3) has to align with chroma (2,2).
    chroma.m[0][0] = ls_w * (float)vimg->planes[0].w / vimg->planes[1].w;
    chroma.m[1][1] = ls_h * (float)vimg->planes[0].h / vimg->planes[1].h;

    // The existing code assumes we just have a single tex multiplier for
    // all of the planes. This may change in the future
    float tex_mul = 1.0 / mp_get_csp_mul(p->image_params.colorspace,
                                         p->image_desc.component_bits,
                                         p->image_desc.component_full_bits);

    memset(tex, 0, 4 * sizeof(tex[0]));
    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *t = &vimg->planes[n];

        enum plane_type type;
        if (n >= 3) {
            type = PLANE_ALPHA;
        } else if (p->image_desc.flags & MP_IMGFLAG_RGB) {
            type = PLANE_RGB;
        } else if (p->image_desc.flags & MP_IMGFLAG_YUV) {
            type = n == 0 ? PLANE_LUMA : PLANE_CHROMA;
        } else if (p->image_desc.flags & MP_IMGFLAG_XYZ) {
            type = PLANE_XYZ;
        } else {
            abort();
        }

        tex[n] = (struct img_tex){
            .type = type,
            .gl_tex = t->gl_texture,
            .gl_target = t->gl_target,
            .multiplier = tex_mul,
            .use_integer = t->use_integer,
            .tex_w = t->w,
            .tex_h = t->h,
            .w = t->w,
            .h = t->h,
            .transform = type == PLANE_CHROMA ? chroma : identity_trans,
            .components = p->image_desc.components[n],
            .texture_la = t->gl_format == GL_LUMINANCE_ALPHA,
        };
        get_plane_source_transform(p, t->w, t->h, &tex[n].pre_transform);
        if (p->image_params.rotate % 180 == 90)
            MPSWAP(int, tex[n].w, tex[n].h);
    }
}

static void init_video(struct gl_video *p)
{
    GL *gl = p->gl;

    init_format(p->image_params.imgfmt, p);
    p->gl_target = p->opts.use_rectangle ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    check_gl_features(p);

    if (p->hwdec_active) {
        if (p->hwdec->driver->reinit(p->hwdec, &p->image_params) < 0)
            MP_ERR(p, "Initializing texture for hardware decoding failed.\n");
        init_format(p->image_params.imgfmt, p);
        p->image_params.imgfmt = p->image_desc.id;
        p->gl_target = p->hwdec->gl_texture_target;
    }

    mp_image_params_guess_csp(&p->image_params);

    int eq_caps = MP_CSP_EQ_CAPS_GAMMA;
    if (p->image_params.colorspace != MP_CSP_BT_2020_C)
        eq_caps |= MP_CSP_EQ_CAPS_COLORMATRIX;
    if (p->image_desc.flags & MP_IMGFLAG_XYZ)
        eq_caps |= MP_CSP_EQ_CAPS_BRIGHTNESS;
    p->video_eq.capabilities = eq_caps;

    av_lfg_init(&p->lfg, 1);

    debug_check_gl(p, "before video texture creation");

    struct video_image *vimg = &p->image;

    struct mp_image layout = {0};
    mp_image_set_params(&layout, &p->image_params);

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];

        plane->gl_target = p->gl_target;

        plane->w = mp_image_plane_w(&layout, n);
        plane->h = mp_image_plane_h(&layout, n);

        if (!p->hwdec_active) {
            gl->ActiveTexture(GL_TEXTURE0 + n);
            gl->GenTextures(1, &plane->gl_texture);
            gl->BindTexture(p->gl_target, plane->gl_texture);

            gl->TexImage2D(p->gl_target, 0, plane->gl_internal_format,
                           plane->w, plane->h, 0,
                           plane->gl_format, plane->gl_type, NULL);

            int filter = plane->use_integer ? GL_NEAREST : GL_LINEAR;
            gl->TexParameteri(p->gl_target, GL_TEXTURE_MIN_FILTER, filter);
            gl->TexParameteri(p->gl_target, GL_TEXTURE_MAG_FILTER, filter);
            gl->TexParameteri(p->gl_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl->TexParameteri(p->gl_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        MP_VERBOSE(p, "Texture for plane %d: %dx%d\n", n, plane->w, plane->h);
    }
    gl->ActiveTexture(GL_TEXTURE0);

    debug_check_gl(p, "after video texture creation");

    reinit_rendering(p);
}

static void uninit_video(struct gl_video *p)
{
    GL *gl = p->gl;

    uninit_rendering(p);

    struct video_image *vimg = &p->image;

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];

        if (!p->hwdec_active)
            gl->DeleteTextures(1, &plane->gl_texture);
        plane->gl_texture = 0;
        gl->DeleteBuffers(1, &plane->gl_buffer);
        plane->gl_buffer = 0;
    }
    mp_image_unrefp(&vimg->mpi);

    // Invalidate image_params to ensure that gl_video_config() will call
    // init_video() on uninitialized gl_video.
    p->real_image_params = (struct mp_image_params){0};
    p->image_params = p->real_image_params;
}

static void pass_prepare_src_tex(struct gl_video *p)
{
    GL *gl = p->gl;
    struct gl_shader_cache *sc = p->sc;

    for (int n = 0; n < p->pass_tex_num; n++) {
        struct img_tex *s = &p->pass_tex[n];
        if (!s->gl_tex)
            continue;

        char texture_name[32];
        char texture_size[32];
        char pixel_size[32];
        snprintf(texture_name, sizeof(texture_name), "texture%d", n);
        snprintf(texture_size, sizeof(texture_size), "texture_size%d", n);
        snprintf(pixel_size, sizeof(pixel_size), "pixel_size%d", n);

        if (s->use_integer) {
            gl_sc_uniform_sampler_ui(sc, texture_name, n);
        } else {
            gl_sc_uniform_sampler(sc, texture_name, s->gl_target, n);
        }
        float f[2] = {1, 1};
        if (s->gl_target != GL_TEXTURE_RECTANGLE) {
            f[0] = s->tex_w;
            f[1] = s->tex_h;
        }
        gl_sc_uniform_vec2(sc, texture_size, f);
        gl_sc_uniform_vec2(sc, pixel_size, (GLfloat[]){1.0f / f[0],
                                                       1.0f / f[1]});

        gl->ActiveTexture(GL_TEXTURE0 + n);
        gl->BindTexture(s->gl_target, s->gl_tex);
    }
    gl->ActiveTexture(GL_TEXTURE0);
}

static void render_pass_quad(struct gl_video *p, int vp_w, int vp_h,
                             const struct mp_rect *dst)
{
    struct vertex va[4] = {0};

    struct gl_transform t;
    gl_transform_ortho(&t, 0, vp_w, 0, vp_h);

    float x[2] = {dst->x0, dst->x1};
    float y[2] = {dst->y0, dst->y1};
    gl_transform_vec(t, &x[0], &y[0]);
    gl_transform_vec(t, &x[1], &y[1]);

    for (int n = 0; n < 4; n++) {
        struct vertex *v = &va[n];
        v->position.x = x[n / 2];
        v->position.y = y[n % 2];
        for (int i = 0; i < p->pass_tex_num; i++) {
            struct img_tex *s = &p->pass_tex[i];
            if (!s->gl_tex)
                continue;
            struct gl_transform tr = s->transform;
            gl_transform_trans(s->pre_transform, &tr);
            float tx = (n / 2) * s->w;
            float ty = (n % 2) * s->h;
            gl_transform_vec(tr, &tx, &ty);
            bool rect = s->gl_target == GL_TEXTURE_RECTANGLE;
            v->texcoord[i].x = tx / (rect ? 1 : s->tex_w);
            v->texcoord[i].y = ty / (rect ? 1 : s->tex_h);
        }
    }

    p->gl->Viewport(0, 0, vp_w, abs(vp_h));
    gl_vao_draw_data(&p->vao, GL_TRIANGLE_STRIP, va, 4);

    debug_check_gl(p, "after rendering");
}

// flags: see render_pass_quad
static void finish_pass_direct(struct gl_video *p, GLint fbo, int vp_w, int vp_h,
                               const struct mp_rect *dst)
{
    GL *gl = p->gl;
    pass_prepare_src_tex(p);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl_sc_gen_shader_and_reset(p->sc);
    render_pass_quad(p, vp_w, vp_h, dst);
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
    memset(&p->pass_tex, 0, sizeof(p->pass_tex));
    p->pass_tex_num = 0;
}

// dst_fbo: this will be used for rendering; possibly reallocating the whole
//          FBO, if the required parameters have changed
// w, h: required FBO target dimension, and also defines the target rectangle
//       used for rasterization
// flags: 0 or combination of FBOTEX_FUZZY_W/FBOTEX_FUZZY_H (setting the fuzzy
//        flags allows the FBO to be larger than the w/h parameters)
static void finish_pass_fbo(struct gl_video *p, struct fbotex *dst_fbo,
                            int w, int h, int flags)
{
    fbotex_change(dst_fbo, p->gl, p->log, w, h, p->opts.fbo_format, flags);

    finish_pass_direct(p, dst_fbo->fbo, dst_fbo->rw, dst_fbo->rh,
                       &(struct mp_rect){0, 0, w, h});
}

static void skip_unused(struct gl_video *p, int num_components)
{
    for (int i = num_components; i < 4; i++)
        GLSLF("color.%c = %f;\n", "rgba"[i], i < 3 ? 0.0 : 1.0);
}

static void uninit_scaler(struct gl_video *p, struct scaler *scaler)
{
    GL *gl = p->gl;
    fbotex_uninit(&scaler->sep_fbo);
    gl->DeleteTextures(1, &scaler->gl_lut);
    scaler->gl_lut = 0;
    scaler->kernel = NULL;
    scaler->initialized = false;
}

static void load_shader(struct gl_video *p, const char *body)
{
    gl_sc_hadd(p->sc, body);
    gl_sc_uniform_f(p->sc, "random", (double)av_lfg_get(&p->lfg) / UINT32_MAX);
    gl_sc_uniform_f(p->sc, "frame", p->frames_uploaded);
    gl_sc_uniform_vec2(p->sc, "image_size", (GLfloat[]){p->image_params.w,
                                                        p->image_params.h});
}

static const char *get_custom_shader_fn(struct gl_video *p, const char *body)
{
    if (!p->gl->es && strstr(body, "sample") && !strstr(body, "sample_pixel")) {
        if (!p->custom_shader_fn_warned) {
            MP_WARN(p, "sample() is deprecated in custom shaders. "
                       "Use sample_pixel()\n");
            p->custom_shader_fn_warned = true;
        }
        return "sample";
    }
    return "sample_pixel";
}

// Applies an arbitrary number of shaders in sequence, using the given pair
// of FBOs as intermediate buffers. Returns whether any shaders were applied.
static bool apply_shaders(struct gl_video *p, char **shaders, int w, int h,
                          struct fbotex textures[2])
{
    if (!shaders)
        return false;
    bool success = false;
    int tex = 0;
    for (int n = 0; shaders[n]; n++) {
        const char *body = load_cached_file(p, shaders[n]);
        if (!body)
            continue;
        finish_pass_fbo(p, &textures[tex], w, h, 0);
        int id = pass_bind(p, img_tex_fbo(&textures[tex], identity_trans,
                                          PLANE_RGB, p->components));
        GLSLHF("#define pixel_size pixel_size%d\n", id);
        load_shader(p, body);
        const char *fn_name = get_custom_shader_fn(p, body);
        GLSLF("// custom shader\n");
        GLSLF("color = %s(texture%d, texcoord%d, texture_size%d);\n",
              fn_name, id, id, id);
        tex = (tex+1) % 2;
        success = true;
    }
    return success;
}

// Semantic equality
static bool double_seq(double a, double b)
{
    return (isnan(a) && isnan(b)) || a == b;
}

static bool scaler_fun_eq(struct scaler_fun a, struct scaler_fun b)
{
    if ((a.name && !b.name) || (b.name && !a.name))
        return false;

    return ((!a.name && !b.name) || strcmp(a.name, b.name) == 0) &&
           double_seq(a.params[0], b.params[0]) &&
           double_seq(a.params[1], b.params[1]) &&
           a.blur == b.blur;
}

static bool scaler_conf_eq(struct scaler_config a, struct scaler_config b)
{
    // Note: antiring isn't compared because it doesn't affect LUT
    // generation
    return scaler_fun_eq(a.kernel, b.kernel) &&
           scaler_fun_eq(a.window, b.window) &&
           a.radius == b.radius &&
           a.clamp == b.clamp;
}

static void reinit_scaler(struct gl_video *p, struct scaler *scaler,
                          const struct scaler_config *conf,
                          double scale_factor,
                          int sizes[])
{
    GL *gl = p->gl;

    if (scaler_conf_eq(scaler->conf, *conf) &&
        scaler->scale_factor == scale_factor &&
        scaler->initialized)
        return;

    uninit_scaler(p, scaler);

    scaler->conf = *conf;
    scaler->scale_factor = scale_factor;
    scaler->insufficient = false;
    scaler->initialized = true;

    const struct filter_kernel *t_kernel = mp_find_filter_kernel(conf->kernel.name);
    if (!t_kernel)
        return;

    scaler->kernel_storage = *t_kernel;
    scaler->kernel = &scaler->kernel_storage;

    const char *win = conf->window.name;
    if (!win || !win[0])
        win = t_kernel->window; // fall back to the scaler's default window
    const struct filter_window *t_window = mp_find_filter_window(win);
    if (t_window)
        scaler->kernel->w = *t_window;

    for (int n = 0; n < 2; n++) {
        if (!isnan(conf->kernel.params[n]))
            scaler->kernel->f.params[n] = conf->kernel.params[n];
        if (!isnan(conf->window.params[n]))
            scaler->kernel->w.params[n] = conf->window.params[n];
    }

    if (conf->kernel.blur > 0.0)
        scaler->kernel->f.blur = conf->kernel.blur;
    if (conf->window.blur > 0.0)
        scaler->kernel->w.blur = conf->window.blur;

    if (scaler->kernel->f.resizable && conf->radius > 0.0)
        scaler->kernel->f.radius = conf->radius;

    scaler->kernel->clamp = conf->clamp;

    scaler->insufficient = !mp_init_filter(scaler->kernel, sizes, scale_factor);

    if (scaler->kernel->polar && (gl->mpgl_caps & MPGL_CAP_1D_TEX)) {
        scaler->gl_target = GL_TEXTURE_1D;
    } else {
        scaler->gl_target = GL_TEXTURE_2D;
    }

    int size = scaler->kernel->size;
    int elems_per_pixel = 4;
    if (size == 1) {
        elems_per_pixel = 1;
    } else if (size == 2) {
        elems_per_pixel = 2;
    } else if (size == 6) {
        elems_per_pixel = 3;
    }
    int width = size / elems_per_pixel;
    assert(size == width * elems_per_pixel);
    const struct fmt_entry *fmt = &gl_float16_formats[elems_per_pixel - 1];
    GLenum target = scaler->gl_target;

    gl->ActiveTexture(GL_TEXTURE0 + TEXUNIT_SCALERS + scaler->index);

    if (!scaler->gl_lut)
        gl->GenTextures(1, &scaler->gl_lut);

    gl->BindTexture(target, scaler->gl_lut);

    scaler->lut_size = 1 << p->opts.scaler_lut_size;

    float *weights = talloc_array(NULL, float, scaler->lut_size * size);
    mp_compute_lut(scaler->kernel, scaler->lut_size, weights);

    if (target == GL_TEXTURE_1D) {
        gl->TexImage1D(target, 0, fmt->internal_format, scaler->lut_size,
                       0, fmt->format, GL_FLOAT, weights);
    } else {
        gl->TexImage2D(target, 0, fmt->internal_format, width, scaler->lut_size,
                       0, fmt->format, GL_FLOAT, weights);
    }

    talloc_free(weights);

    gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    if (target != GL_TEXTURE_1D)
        gl->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl->ActiveTexture(GL_TEXTURE0);

    debug_check_gl(p, "after initializing scaler");
}

// Special helper for sampling from two separated stages
static void pass_sample_separated(struct gl_video *p, struct img_tex src,
                                  struct scaler *scaler, int w, int h)
{
    // Separate the transformation into x and y components, per pass
    struct gl_transform t_x = {
        .m = {{src.transform.m[0][0], 0.0}, {src.transform.m[1][0], 1.0}},
        .t = {src.transform.t[0], 0.0},
    };
    struct gl_transform t_y = {
        .m = {{1.0, src.transform.m[0][1]}, {0.0, src.transform.m[1][1]}},
        .t = {0.0, src.transform.t[1]},
    };

    // First pass (scale only in the y dir)
    src.transform = t_y;
    sampler_prelude(p->sc, pass_bind(p, src));
    GLSLF("// pass 1\n");
    pass_sample_separated_gen(p->sc, scaler, 0, 1);
    GLSLF("color *= %f;\n", src.multiplier);
    finish_pass_fbo(p, &scaler->sep_fbo, src.w, h, FBOTEX_FUZZY_H);

    // Second pass (scale only in the x dir)
    src = img_tex_fbo(&scaler->sep_fbo, t_x, src.type, src.components);
    sampler_prelude(p->sc, pass_bind(p, src));
    GLSLF("// pass 2\n");
    pass_sample_separated_gen(p->sc, scaler, 1, 0);
}

// Sample from img_tex, with the src rectangle given by it.
// The dst rectangle is implicit by what the caller will do next, but w and h
// must still be what is going to be used (to dimension FBOs correctly).
// This will write the scaled contents to the vec4 "color".
// The scaler unit is initialized by this function; in order to avoid cache
// thrashing, the scaler unit should usually use the same parameters.
static void pass_sample(struct gl_video *p, struct img_tex tex,
                        struct scaler *scaler, const struct scaler_config *conf,
                        double scale_factor, int w, int h)
{
    reinit_scaler(p, scaler, conf, scale_factor, filter_sizes);

    bool is_separated = scaler->kernel && !scaler->kernel->polar;

    // Set up the transformation+prelude and bind the texture, for everything
    // other than separated scaling (which does this in the subfunction)
    if (!is_separated)
        sampler_prelude(p->sc, pass_bind(p, tex));

    // Dispatch the scaler. They're all wildly different.
    const char *name = scaler->conf.kernel.name;
    if (strcmp(name, "bilinear") == 0) {
        GLSL(color = texture(tex, pos);)
    } else if (strcmp(name, "bicubic_fast") == 0) {
        pass_sample_bicubic_fast(p->sc);
    } else if (strcmp(name, "oversample") == 0) {
        pass_sample_oversample(p->sc, scaler, w, h);
    } else if (strcmp(name, "custom") == 0) {
        const char *body = load_cached_file(p, p->opts.scale_shader);
        if (body) {
            load_shader(p, body);
            const char *fn_name = get_custom_shader_fn(p, body);
            GLSLF("// custom scale-shader\n");
            GLSLF("color = %s(tex, pos, size);\n", fn_name);
        } else {
            p->opts.scale_shader = NULL;
        }
    } else if (scaler->kernel && scaler->kernel->polar) {
        pass_sample_polar(p->sc, scaler);
    } else if (scaler->kernel) {
        pass_sample_separated(p, tex, scaler, w, h);
    } else {
        // Should never happen
        abort();
    }

    // Apply any required multipliers. Separated scaling already does this in
    // its first stage
    if (!is_separated)
        GLSLF("color *= %f;\n", tex.multiplier);

    // Micro-optimization: Avoid scaling unneeded channels
    skip_unused(p, tex.components);
}

// Get the number of passes for prescaler, with given display size.
static int get_prescale_passes(struct gl_video *p, struct img_tex tex[4])
{
    if (!p->opts.prescale_luma)
        return 0;

    // Return 0 if no luma planes exist
    for (int n = 0; ; n++) {
        if (n > 4)
            return 0;

        if (tex[n].type == PLANE_LUMA)
            break;
    }

    // The downscaling threshold check is turned off.
    if (p->opts.prescale_downscaling_threshold < 1.0f)
        return p->opts.prescale_passes;

    double scale_factors[2];
    get_scale_factors(p, true, scale_factors);

    int passes = 0;
    for (; passes < p->opts.prescale_passes; passes ++) {
        // The scale factor happens to be the same for superxbr and nnedi3.
        scale_factors[0] /= 2;
        scale_factors[1] /= 2;

        if (1.0f / scale_factors[0] > p->opts.prescale_downscaling_threshold)
            break;
        if (1.0f / scale_factors[1] > p->opts.prescale_downscaling_threshold)
            break;
    }

    return passes;
}

// Upload the NNEDI3 UBO weights only if needed
static void upload_nnedi3_weights(struct gl_video *p)
{
    GL *gl = p->gl;

    if (p->opts.nnedi3_opts->upload == NNEDI3_UPLOAD_UBO &&
        !p->nnedi3_weights_buffer)
    {
        gl->GenBuffers(1, &p->nnedi3_weights_buffer);
        gl->BindBufferBase(GL_UNIFORM_BUFFER, 0, p->nnedi3_weights_buffer);

        int size;
        const float *weights = get_nnedi3_weights(p->opts.nnedi3_opts, &size);

        MP_VERBOSE(p, "Uploading NNEDI3 weights via UBO (size=%d)\n", size);

        // We don't know the endianness of GPU, just assume it's LE
        gl->BufferData(GL_UNIFORM_BUFFER, size, weights, GL_STATIC_DRAW);
    }
}

// Applies a single pass of the prescaler, and accumulates the offset in
// pass_transform.
static void pass_prescale_luma(struct gl_video *p, struct img_tex *tex,
                               struct gl_transform *pass_transform,
                               struct fbotex fbo[MAX_PRESCALE_STEPS])
{
    // Happens to be the same for superxbr and nnedi3.
    const int num_steps = 2;

    for (int step = 0; step < num_steps; step++) {
        struct gl_transform step_transform = {{{0}}};
        int id = pass_bind(p, *tex);
        int planes = tex->components;

        switch(p->opts.prescale_luma) {
        case 1:
            assert(planes == 1);
            pass_superxbr(p->sc, id, step, tex->multiplier,
                          p->opts.superxbr_opts, &step_transform);
            break;
        case 2:
            upload_nnedi3_weights(p);
            pass_nnedi3(p->gl, p->sc, planes, id, step, tex->multiplier,
                        p->opts.nnedi3_opts, &step_transform, tex->gl_target);
            break;
        default:
            abort();
        }

        int new_w = tex->w * (int)step_transform.m[0][0],
            new_h = tex->h * (int)step_transform.m[1][1];

        skip_unused(p, planes);
        finish_pass_fbo(p, &fbo[step], new_w, new_h, 0);
        *tex = img_tex_fbo(&fbo[step], identity_trans, tex->type, tex->components);

        // Accumulate the local transform
        gl_transform_trans(step_transform, pass_transform);
    }
}

// Copy a texture to the vec4 color, while increasing offset. Also applies
// the texture multiplier to the sampled color
static void copy_img_tex(struct gl_video *p, int *offset, struct img_tex img)
{
    int count = img.components;
    assert(*offset + count <= 4);

    int id = pass_bind(p, img);
    char src[5] = {0};
    char dst[5] = {0};
    const char *tex_fmt = img.texture_la ? "ragg" : "rgba";
    const char *dst_fmt = "rgba";
    for (int i = 0; i < count; i++) {
        src[i] = tex_fmt[i];
        dst[i] = dst_fmt[*offset + i];
    }

    if (img.use_integer) {
        uint64_t tex_max = 1ull << p->image_desc.component_full_bits;
        img.multiplier *= 1.0 / (tex_max - 1);
    }

    GLSLF("color.%s = %f * vec4(texture(texture%d, texcoord%d)).%s;\n",
          dst, img.multiplier, id, id, src);

    *offset += count;
}

// sample from video textures, set "color" variable to yuv value
static void pass_read_video(struct gl_video *p)
{
    struct img_tex tex[4];
    pass_get_img_tex(p, &p->image, tex);

    // Most of the steps here don't actually apply image transformations yet,
    // save for the actual upscaling - so as a code convenience we store them
    // separately
    struct gl_transform transforms[4];
    struct gl_transform tex_trans = identity_trans;
    for (int i = 0; i < 4; i++) {
        transforms[i] = tex[i].transform;
        tex[i].transform = identity_trans;
    }

    int prescale_passes = get_prescale_passes(p, tex);

    int dst_w = p->texture_w << prescale_passes,
        dst_h = p->texture_h << prescale_passes;

    bool needs_deband[4];
    int scaler_id[4]; // ID if needed, -1 otherwise
    int needs_prescale[4]; // number of prescaling passes left

    // Determine what needs to be done for which plane
    for (int i=0; i < 4; i++) {
        enum plane_type type = tex[i].type;
        if (type == PLANE_NONE) {
            needs_deband[i] = false;
            needs_prescale[i] = 0;
            scaler_id[i] = -1;
            continue;
        }

        needs_deband[i] = type != PLANE_ALPHA ? p->opts.deband : false;
        needs_prescale[i] = type == PLANE_LUMA ? prescale_passes : 0;

        scaler_id[i] = -1;
        switch (type) {
        case PLANE_RGB:
        case PLANE_LUMA:
        case PLANE_XYZ:
            scaler_id[i] = SCALER_SCALE;
            break;

        case PLANE_CHROMA:
            scaler_id[i] = SCALER_CSCALE;
            break;

        case PLANE_ALPHA: // always use bilinear for alpha
        default:
            continue;
        }

        // We can skip scaling if the texture is already at the required size
        if (tex[i].w == dst_w && tex[i].h == dst_h)
            scaler_id[i] = -1;
    }

    // Process all the planes that need some action performed
    while (true) {
        // Find next plane to operate on
        int n = -1;
        for (int i = 0; i < 4; i++) {
            if (tex[i].type != PLANE_NONE &&
                (scaler_id[i] >= 0 || needs_deband[i] || needs_prescale[i]))
            {
                n = i;
                break;
            }
        }

        if (n == -1) // no textures left
            break;

        // Figure out if it needs to be merged with anything else first
        int o = -1;
        for (int i = n+1; i < 4; i++) {
            if (tex[i].type == tex[n].type
                && tex[i].w == tex[n].w
                && tex[i].h == tex[n].h
                && gl_transform_eq(transforms[i], transforms[n]))
            {
                o = i;
                break;
            }
        }

        // Multiple planes share the same dimensions and type, merge them for
        // upscaling/debanding efficiency
        if (o != -1) {
            GLSLF("// merging plane %d into %d\n", o, n);

            int num = 0;
            copy_img_tex(p, &num, tex[n]);
            copy_img_tex(p, &num, tex[o]);
            finish_pass_fbo(p, &p->merge_fbo[n], tex[n].w, tex[n].h, 0);
            tex[n] = img_tex_fbo(&p->merge_fbo[n], identity_trans,
                                 tex[n].type, num);

            memset(&tex[o], 0, sizeof(tex[o]));
            continue;
        }

        // The steps after this point (debanding, upscaling) can't handle
        // integer textures, so the plane is still in that format by this point
        // we need to ensure it gets converted
        if (tex[n].use_integer) {
            GLSLF("// use_integer fix for plane %d\n", n);

            copy_img_tex(p, &(int){0}, tex[n]);
            finish_pass_fbo(p, &p->integer_fbo[n], tex[n].w, tex[n].h, 0);
            tex[n] = img_tex_fbo(&p->integer_fbo[n], identity_trans,
                                 tex[n].type, tex[n].components);
            continue;
        }

        // Plane is not yet debanded
        if (needs_deband[n]) {
            GLSLF("// debanding plane %d\n", n);

            int id = pass_bind(p, tex[n]);
            pass_sample_deband(p->sc, p->opts.deband_opts, id, tex[n].multiplier,
                               tex[n].gl_target, &p->lfg);
            skip_unused(p, tex[n].components);
            finish_pass_fbo(p, &p->deband_fbo[n], tex[n].w, tex[n].h, 0);
            tex[n] = img_tex_fbo(&p->deband_fbo[n], identity_trans,
                                 tex[n].type, tex[n].components);

            needs_deband[n] = false;
            continue;
        }

        // Plane still needs prescaling passes
        if (needs_prescale[n]) {
            GLSLF("// prescaling plane %d (%d left)\n", n, needs_prescale[n]);
            pass_prescale_luma(p, &tex[n], &tex_trans,
                               p->prescale_fbo[needs_prescale[n]-1]);
            needs_prescale[n]--;

            // We can skip scaling if we arrived at our target res
            if (tex[n].w == dst_w && tex[n].h == dst_h)
                scaler_id[n] = -1;

            // If we're done prescaling, we need to adjust all of the
            // other transforms to make sure the planes still align
            if (needs_prescale[n] == 0) {
                for (int i = 0; i < 4; i++) {
                    if (n == i)
                        continue;

                    transforms[i].t[0] -= tex_trans.t[0] / tex_trans.m[0][0];
                    transforms[i].t[1] -= tex_trans.t[1] / tex_trans.m[1][1];
                }
            }
            continue;
        }

        // Plane is not yet upscaled
        if (scaler_id[n] >= 0) {
            const struct scaler_config *conf = &p->opts.scaler[scaler_id[n]];
            struct scaler *scaler = &p->scaler[scaler_id[n]];

            // This is the only step that actually uses the transform
            tex[n].transform = transforms[n];

            // Bilinear scaling is a no-op due to GPU sampling
            if (strcmp(conf->kernel.name, "bilinear") != 0) {
                GLSLF("// upscaling plane %d\n", n);
                pass_sample(p, tex[n], scaler, conf, 1.0, dst_w, dst_h);
                finish_pass_fbo(p, &p->scale_fbo[n], dst_w, dst_h, FBOTEX_FUZZY);
                tex[n] = img_tex_fbo(&p->scale_fbo[n], identity_trans,
                                     tex[n].type, tex[n].components);
                transforms[n] = identity_trans;
            }

            scaler_id[n] = -1;
            continue;
        }

        // Execution should never reach this point
        abort();
    }

    // All planes are of the same size and properly aligned at this point
    GLSLF("// combining planes\n");
    int coord = 0;
    for (int i = 0; i < 4; i++) {
        if (tex[i].type != PLANE_NONE)
            copy_img_tex(p, &coord, tex[i]);
    }

    p->texture_w = dst_w;
    p->texture_h = dst_h;
    p->texture_offset = tex_trans;
    p->components = coord;
}

// Utility function that simply binds an FBO and reads from it, without any
// transformations. Returns the ID of the texture unit it was bound to
static int pass_read_fbo(struct gl_video *p, struct fbotex *fbo)
{
    struct img_tex tex = img_tex_fbo(fbo, identity_trans, PLANE_RGB, p->components);
    copy_img_tex(p, &(int){0}, tex);

    return pass_bind(p, tex);
}

// yuv conversion, and any other conversions before main up/down-scaling
static void pass_convert_yuv(struct gl_video *p)
{
    struct gl_shader_cache *sc = p->sc;

    struct mp_csp_params cparams = MP_CSP_PARAMS_DEFAULTS;
    cparams.gray = p->is_yuv && !p->is_packed_yuv && p->plane_count == 1;
    cparams.input_bits = p->image_desc.component_bits;
    cparams.texture_bits = p->image_desc.component_full_bits;
    mp_csp_set_image_params(&cparams, &p->image_params);
    mp_csp_copy_equalizer_values(&cparams, &p->video_eq);
    p->user_gamma = 1.0 / (cparams.gamma * p->opts.gamma);

    GLSLF("// color conversion\n");

    if (p->color_swizzle[0])
        GLSLF("color = color.%s;\n", p->color_swizzle);

    // Pre-colormatrix input gamma correction
    if (cparams.colorspace == MP_CSP_XYZ)
        GLSL(color.rgb = pow(color.rgb, vec3(2.6));) // linear light

    // We always explicitly normalize the range in pass_read_video
    cparams.input_bits = cparams.texture_bits = 0;

    // Conversion to RGB. For RGB itself, this still applies e.g. brightness
    // and contrast controls, or expansion of e.g. LSB-packed 10 bit data.
    struct mp_cmat m = {{{0}}};
    mp_get_csp_matrix(&cparams, &m);
    gl_sc_uniform_mat3(sc, "colormatrix", true, &m.m[0][0]);
    gl_sc_uniform_vec3(sc, "colormatrix_c", m.c);

    GLSL(color.rgb = mat3(colormatrix) * color.rgb + colormatrix_c;)

    if (p->image_params.colorspace == MP_CSP_BT_2020_C) {
        // Conversion for C'rcY'cC'bc via the BT.2020 CL system:
        // C'bc = (B'-Y'c) / 1.9404  | C'bc <= 0
        //      = (B'-Y'c) / 1.5816  | C'bc >  0
        //
        // C'rc = (R'-Y'c) / 1.7184  | C'rc <= 0
        //      = (R'-Y'c) / 0.9936  | C'rc >  0
        //
        // as per the BT.2020 specification, table 4. This is a non-linear
        // transformation because (constant) luminance receives non-equal
        // contributions from the three different channels.
        GLSLF("// constant luminance conversion\n");
        GLSL(color.br = color.br * mix(vec2(1.5816, 0.9936),
                                       vec2(1.9404, 1.7184),
                                       lessThanEqual(color.br, vec2(0)))
                        + color.gg;)
        // Expand channels to camera-linear light. This shader currently just
        // assumes everything uses the BT.2020 12-bit gamma function, since the
        // difference between 10 and 12-bit is negligible for anything other
        // than 12-bit content.
        GLSL(color.rgb = mix(color.rgb / vec3(4.5),
                             pow((color.rgb + vec3(0.0993))/vec3(1.0993), vec3(1.0/0.45)),
                             lessThanEqual(vec3(0.08145), color.rgb));)
        // Calculate the green channel from the expanded RYcB
        // The BT.2020 specification says Yc = 0.2627*R + 0.6780*G + 0.0593*B
        GLSL(color.g = (color.g - 0.2627*color.r - 0.0593*color.b)/0.6780;)
        // Recompress to receive the R'G'B' result, same as other systems
        GLSL(color.rgb = mix(color.rgb * vec3(4.5),
                             vec3(1.0993) * pow(color.rgb, vec3(0.45)) - vec3(0.0993),
                             lessThanEqual(vec3(0.0181), color.rgb));)
    }

    p->components = 3;
    if (!p->has_alpha || p->opts.alpha_mode == 0) { // none
        GLSL(color.a = 1.0;)
    } else if (p->opts.alpha_mode == 2) { // blend against black
        GLSL(color = vec4(color.rgb * color.a, 1.0);)
    } else { // alpha present in image
        p->components = 4;
        GLSL(color = vec4(color.rgb * color.a, color.a);)
    }
}

static void get_scale_factors(struct gl_video *p, bool transpose_rot, double xy[2])
{
    double target_w = p->src_rect.x1 - p->src_rect.x0;
    double target_h = p->src_rect.y1 - p->src_rect.y0;
    if (transpose_rot && p->image_params.rotate % 180 == 90)
        MPSWAP(double, target_w, target_h);
    xy[0] = (p->dst_rect.x1 - p->dst_rect.x0) / target_w;
    xy[1] = (p->dst_rect.y1 - p->dst_rect.y0) / target_h;
}

// Cropping.
static void compute_src_transform(struct gl_video *p, struct gl_transform *tr)
{
    float sx = (p->src_rect.x1 - p->src_rect.x0) / (float)p->texture_w,
          sy = (p->src_rect.y1 - p->src_rect.y0) / (float)p->texture_h,
          ox = p->src_rect.x0,
          oy = p->src_rect.y0;
    struct gl_transform transform = {{{sx, 0}, {0, sy}}, {ox, oy}};

    gl_transform_trans(p->texture_offset, &transform);

    *tr = transform;
}

// Takes care of the main scaling and pre/post-conversions
static void pass_scale_main(struct gl_video *p)
{
    // Figure out the main scaler.
    double xy[2];
    get_scale_factors(p, true, xy);

    // actual scale factor should be divided by the scale factor of prescaling.
    xy[0] /= p->texture_offset.m[0][0];
    xy[1] /= p->texture_offset.m[1][1];

    bool downscaling = xy[0] < 1.0 || xy[1] < 1.0;
    bool upscaling = !downscaling && (xy[0] > 1.0 || xy[1] > 1.0);
    double scale_factor = 1.0;

    struct scaler *scaler = &p->scaler[SCALER_SCALE];
    struct scaler_config scaler_conf = p->opts.scaler[SCALER_SCALE];
    if (p->opts.scaler_resizes_only && !downscaling && !upscaling) {
        scaler_conf.kernel.name = "bilinear";
        // bilinear is going to be used, just remove all sub-pixel offsets.
        p->texture_offset.t[0] = (int)p->texture_offset.t[0];
        p->texture_offset.t[1] = (int)p->texture_offset.t[1];
    }
    if (downscaling && p->opts.scaler[SCALER_DSCALE].kernel.name) {
        scaler_conf = p->opts.scaler[SCALER_DSCALE];
        scaler = &p->scaler[SCALER_DSCALE];
    }

    // When requesting correct-downscaling and the clip is anamorphic, and
    // because only a single scale factor is used for both axes, enable it only
    // when both axes are downscaled, and use the milder of the factors to not
    // end up with too much blur on one axis (even if we end up with sub-optimal
    // scale factor on the other axis). This is better than not respecting
    // correct scaling at all for anamorphic clips.
    double f = MPMAX(xy[0], xy[1]);
    if (p->opts.correct_downscaling && f < 1.0)
        scale_factor = 1.0 / f;

    // Pre-conversion, like linear light/sigmoidization
    GLSLF("// scaler pre-conversion\n");
    if (p->use_linear)
        pass_linearize(p->sc, p->image_params.gamma);

    bool use_sigmoid = p->use_linear && p->opts.sigmoid_upscaling && upscaling;
    float sig_center, sig_slope, sig_offset, sig_scale;
    if (use_sigmoid) {
        // Coefficients for the sigmoidal transform are taken from the
        // formula here: http://www.imagemagick.org/Usage/color_mods/#sigmoidal
        sig_center = p->opts.sigmoid_center;
        sig_slope  = p->opts.sigmoid_slope;
        // This function needs to go through (0,0) and (1,1) so we compute the
        // values at 1 and 0, and then scale/shift them, respectively.
        sig_offset = 1.0/(1+expf(sig_slope * sig_center));
        sig_scale  = 1.0/(1+expf(sig_slope * (sig_center-1))) - sig_offset;
        GLSLF("color.rgb = %f - log(1.0/(color.rgb * %f + %f) - 1.0)/%f;\n",
                sig_center, sig_scale, sig_offset, sig_slope);
    }

    int vp_w = p->dst_rect.x1 - p->dst_rect.x0;
    int vp_h = p->dst_rect.y1 - p->dst_rect.y0;
    struct gl_transform transform;
    compute_src_transform(p, &transform);

    GLSLF("// main scaling\n");
    finish_pass_fbo(p, &p->indirect_fbo, p->texture_w, p->texture_h, 0);
    struct img_tex src = img_tex_fbo(&p->indirect_fbo, transform,
                                     PLANE_RGB, p->components);
    pass_sample(p, src, scaler, &scaler_conf, scale_factor, vp_w, vp_h);

    // Changes the texture size to display size after main scaler.
    p->texture_w = vp_w;
    p->texture_h = vp_h;

    GLSLF("// scaler post-conversion\n");
    if (use_sigmoid) {
        // Inverse of the transformation above
        GLSLF("color.rgb = (1.0/(1.0 + exp(%f * (%f - color.rgb))) - %f) / %f;\n",
                sig_slope, sig_center, sig_offset, sig_scale);
    }
}

// Adapts the colors from the given color space to the display device's native
// gamut.
static void pass_colormanage(struct gl_video *p, enum mp_csp_prim prim_src,
                             enum mp_csp_trc trc_src)
{
    GLSLF("// color management\n");
    enum mp_csp_trc trc_dst = p->opts.target_trc;
    enum mp_csp_prim prim_dst = p->opts.target_prim;

    if (p->use_lut_3d) {
        // The 3DLUT is always generated against the original source space
        enum mp_csp_prim prim_orig = p->image_params.primaries;
        enum mp_csp_trc trc_orig = p->image_params.gamma;

        if (gl_video_get_lut3d(p, prim_orig, trc_orig)) {
            prim_dst = prim_orig;
            trc_dst = trc_orig;
        } else {
            p->use_lut_3d = false;
        }
    }

    if (prim_dst == MP_CSP_PRIM_AUTO)
        prim_dst = prim_src;
    if (trc_dst == MP_CSP_TRC_AUTO) {
        trc_dst = trc_src;
        // Avoid outputting linear light at all costs
        if (trc_dst == MP_CSP_TRC_LINEAR)
            trc_dst = p->image_params.gamma;
        if (trc_dst == MP_CSP_TRC_LINEAR)
            trc_dst = MP_CSP_TRC_GAMMA22;
    }

    bool need_gamma = trc_src != trc_dst || prim_src != prim_dst;
    if (need_gamma)
        pass_linearize(p->sc, trc_src);

    // Adapt to the right colorspace if necessary
    if (prim_src != prim_dst) {
        struct mp_csp_primaries csp_src = mp_get_csp_primaries(prim_src),
                                csp_dst = mp_get_csp_primaries(prim_dst);
        float m[3][3] = {{0}};
        mp_get_cms_matrix(csp_src, csp_dst, MP_INTENT_RELATIVE_COLORIMETRIC, m);
        gl_sc_uniform_mat3(p->sc, "cms_matrix", true, &m[0][0]);
        GLSL(color.rgb = cms_matrix * color.rgb;)
    }

    if (need_gamma)
        pass_delinearize(p->sc, trc_dst);

    if (p->use_lut_3d) {
        gl_sc_uniform_sampler(p->sc, "lut_3d", GL_TEXTURE_3D, TEXUNIT_3DLUT);
        GLSL(color.rgb = texture3D(lut_3d, color.rgb).rgb;)
    }
}

static void pass_dither(struct gl_video *p)
{
    GL *gl = p->gl;

    // Assume 8 bits per component if unknown.
    int dst_depth = gl->fb_g ? gl->fb_g : 8;
    if (p->opts.dither_depth > 0)
        dst_depth = p->opts.dither_depth;

    if (p->opts.dither_depth < 0 || p->opts.dither_algo < 0)
        return;

    if (!p->dither_texture) {
        MP_VERBOSE(p, "Dither to %d.\n", dst_depth);

        int tex_size;
        void *tex_data;
        GLint tex_iformat;
        GLint tex_format;
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

            const struct fmt_entry *fmt = find_tex_format(gl, 2, 1);
            tex_size = size;
            // Prefer R16 texture since they provide higher precision.
            if (fmt->internal_format) {
                tex_iformat = fmt->internal_format;
                tex_format = fmt->format;
            } else {
                tex_iformat = gl_float16_formats[0].internal_format;
                tex_format = gl_float16_formats[0].format;
            }
            tex_type = GL_FLOAT;
            tex_data = p->last_dither_matrix;
        } else {
            assert(sizeof(temp) >= 8 * 8);
            mp_make_ordered_dither_matrix(temp, 8);

            const struct fmt_entry *fmt = find_tex_format(gl, 1, 1);
            tex_size = 8;
            tex_iformat = fmt->internal_format;
            tex_format = fmt->format;
            tex_type = fmt->type;
            tex_data = temp;
        }

        p->dither_size = tex_size;

        gl->ActiveTexture(GL_TEXTURE0 + TEXUNIT_DITHER);
        gl->GenTextures(1, &p->dither_texture);
        gl->BindTexture(GL_TEXTURE_2D, p->dither_texture);
        gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
        gl->TexImage2D(GL_TEXTURE_2D, 0, tex_iformat, tex_size, tex_size, 0,
                    tex_format, tex_type, tex_data);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
        gl->ActiveTexture(GL_TEXTURE0);

        debug_check_gl(p, "dither setup");
    }

    GLSLF("// dithering\n");

    // This defines how many bits are considered significant for output on
    // screen. The superfluous bits will be used for rounding according to the
    // dither matrix. The precision of the source implicitly decides how many
    // dither patterns can be visible.
    int dither_quantization = (1 << dst_depth) - 1;

    gl_sc_uniform_sampler(p->sc, "dither", GL_TEXTURE_2D, TEXUNIT_DITHER);

    GLSLF("vec2 dither_pos = gl_FragCoord.xy / %d.0;\n", p->dither_size);

    if (p->opts.temporal_dither) {
        int phase = (p->frames_rendered / p->opts.temporal_dither_period) % 8u;
        float r = phase * (M_PI / 2); // rotate
        float m = phase < 4 ? 1 : -1; // mirror

        float matrix[2][2] = {{cos(r),     -sin(r)    },
                              {sin(r) * m,  cos(r) * m}};
        gl_sc_uniform_mat2(p->sc, "dither_trafo", true, &matrix[0][0]);

        GLSL(dither_pos = dither_trafo * dither_pos;)
    }

    GLSL(float dither_value = texture(dither, dither_pos).r;)
    GLSLF("color = floor(color * %d.0 + dither_value + 0.5 / %d.0) / %d.0;\n",
          dither_quantization, p->dither_size * p->dither_size,
          dither_quantization);
}

// Draws the OSD, in scene-referred colors.. If cms is true, subtitles are
// instead adapted to the display's gamut.
static void pass_draw_osd(struct gl_video *p, int draw_flags, double pts,
                          struct mp_osd_res rect, int vp_w, int vp_h, int fbo,
                          bool cms)
{
    mpgl_osd_generate(p->osd, rect, pts, p->image_params.stereo_out, draw_flags);

    p->gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        enum sub_bitmap_format fmt = mpgl_osd_get_part_format(p->osd, n);
        if (!fmt)
            continue;
        gl_sc_uniform_sampler(p->sc, "osdtex", GL_TEXTURE_2D, 0);
        switch (fmt) {
        case SUBBITMAP_RGBA: {
            GLSLF("// OSD (RGBA)\n");
            GLSL(color = texture(osdtex, texcoord).bgra;)
            break;
        }
        case SUBBITMAP_LIBASS: {
            GLSLF("// OSD (libass)\n");
            GLSL(color =
                vec4(ass_color.rgb, ass_color.a * texture(osdtex, texcoord).r);)
            break;
        }
        default:
            abort();
        }
        // Subtitle color management, they're assumed to be sRGB by default
        if (cms)
            pass_colormanage(p, MP_CSP_PRIM_BT_709, MP_CSP_TRC_SRGB);
        gl_sc_set_vao(p->sc, mpgl_osd_get_vao(p->osd));
        gl_sc_gen_shader_and_reset(p->sc);
        mpgl_osd_draw_part(p->osd, vp_w, vp_h, n);
    }
    gl_sc_set_vao(p->sc, &p->vao);
}

// Minimal rendering code path, for GLES or OpenGL 2.1 without proper FBOs.
static void pass_render_frame_dumb(struct gl_video *p, int fbo)
{
    p->gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);

    struct img_tex tex[4];
    pass_get_img_tex(p, &p->image, tex);

    struct gl_transform transform;
    compute_src_transform(p, &transform);

    struct gl_transform tchroma = transform;
    tchroma.t[0] /= 1 << p->image_desc.chroma_xs;
    tchroma.t[1] /= 1 << p->image_desc.chroma_ys;

    int index = 0;
    for (int i = 0; i < p->plane_count; i++) {
        gl_transform_trans(tex[i].type == PLANE_CHROMA ? tchroma : transform,
                           &tex[i].transform);
        copy_img_tex(p, &index, tex[i]);
    }

    pass_convert_yuv(p);
}

// The main rendering function, takes care of everything up to and including
// upscaling. p->image is rendered.
static void pass_render_frame(struct gl_video *p)
{
    // initialize the texture parameters
    p->texture_w = p->image_params.w;
    p->texture_h = p->image_params.h;
    p->texture_offset = identity_trans;
    p->components = 0;

    if (p->image_params.rotate % 180 == 90)
        MPSWAP(int, p->texture_w, p->texture_h);

    if (p->dumb_mode)
        return;

    p->use_linear = p->opts.linear_scaling || p->opts.sigmoid_upscaling;
    pass_read_video(p);
    pass_convert_yuv(p);

    // For subtitles
    double vpts = p->image.mpi->pts;
    if (vpts == MP_NOPTS_VALUE)
        vpts = p->osd_pts;

    if (p->osd && p->opts.blend_subs == 2) {
        double scale[2];
        get_scale_factors(p, false, scale);
        struct mp_osd_res rect = {
            .w = p->texture_w, .h = p->texture_h,
            .display_par = scale[1] / scale[0], // counter compensate scaling
        };
        finish_pass_fbo(p, &p->blend_subs_fbo, rect.w, rect.h, 0);
        pass_draw_osd(p, OSD_DRAW_SUB_ONLY, vpts, rect,
                      rect.w, rect.h, p->blend_subs_fbo.fbo, false);
        GLSL(color = texture(texture0, texcoord0);)
        pass_read_fbo(p, &p->blend_subs_fbo);
    }

    apply_shaders(p, p->opts.pre_shaders, p->texture_w, p->texture_h, p->pre_fbo);

    if (p->opts.unsharp != 0.0) {
        finish_pass_fbo(p, &p->unsharp_fbo, p->texture_w, p->texture_h, 0);
        int id = pass_read_fbo(p, &p->unsharp_fbo);
        pass_sample_unsharp(p->sc, id, p->opts.unsharp);
    }

    pass_scale_main(p);

    int vp_w = p->dst_rect.x1 - p->dst_rect.x0,
        vp_h = p->dst_rect.y1 - p->dst_rect.y0;
    if (p->osd && p->opts.blend_subs == 1) {
        // Recreate the real video size from the src/dst rects
        struct mp_osd_res rect = {
            .w = vp_w, .h = vp_h,
            .ml = -p->src_rect.x0, .mr = p->src_rect.x1 - p->image_params.w,
            .mt = -p->src_rect.y0, .mb = p->src_rect.y1 - p->image_params.h,
            .display_par = 1.0,
        };
        // Adjust margins for scale
        double scale[2];
        get_scale_factors(p, true, scale);
        rect.ml *= scale[0]; rect.mr *= scale[0];
        rect.mt *= scale[1]; rect.mb *= scale[1];
        // We should always blend subtitles in non-linear light
        if (p->use_linear)
            pass_delinearize(p->sc, p->image_params.gamma);
        finish_pass_fbo(p, &p->blend_subs_fbo, p->texture_w, p->texture_h,
                        FBOTEX_FUZZY);
        pass_draw_osd(p, OSD_DRAW_SUB_ONLY, vpts, rect,
                      p->texture_w, p->texture_h, p->blend_subs_fbo.fbo, false);
        pass_read_fbo(p, &p->blend_subs_fbo);
        if (p->use_linear)
            pass_linearize(p->sc, p->image_params.gamma);
    }

    apply_shaders(p, p->opts.post_shaders, p->texture_w, p->texture_h, p->post_fbo);
}

static void pass_draw_to_screen(struct gl_video *p, int fbo)
{
    if (p->dumb_mode)
        pass_render_frame_dumb(p, fbo);

    // Adjust the overall gamma before drawing to screen
    if (p->user_gamma != 1) {
        gl_sc_uniform_f(p->sc, "user_gamma", p->user_gamma);
        GLSL(color.rgb = clamp(color.rgb, 0.0, 1.0);)
        GLSL(color.rgb = pow(color.rgb, vec3(user_gamma));)
    }

    pass_colormanage(p, p->image_params.primaries,
                     p->use_linear ? MP_CSP_TRC_LINEAR : p->image_params.gamma);

    // Draw checkerboard pattern to indicate transparency
    if (p->has_alpha && p->opts.alpha_mode == 3) {
        GLSLF("// transparency checkerboard\n");
        GLSL(bvec2 tile = lessThan(fract(gl_FragCoord.xy / 32.0), vec2(0.5));)
        GLSL(vec3 background = vec3(tile.x == tile.y ? 1.0 : 0.75);)
        GLSL(color.rgb = mix(background, color.rgb, color.a);)
    }

    pass_dither(p);
    finish_pass_direct(p, fbo, p->vp_w, p->vp_h, &p->dst_rect);
}

// Draws an interpolate frame to fbo, based on the frame timing in t
static void gl_video_interpolate_frame(struct gl_video *p, struct vo_frame *t,
                                       int fbo)
{
    int vp_w = p->dst_rect.x1 - p->dst_rect.x0,
        vp_h = p->dst_rect.y1 - p->dst_rect.y0;

    // Reset the queue completely if this is a still image, to avoid any
    // interpolation artifacts from surrounding frames when unpausing or
    // framestepping
    if (t->still)
        gl_video_reset_surfaces(p);

    // First of all, figure out if we have a frame availble at all, and draw
    // it manually + reset the queue if not
    if (p->surfaces[p->surface_now].pts == MP_NOPTS_VALUE) {
        gl_video_upload_image(p, t->current);
        pass_render_frame(p);
        finish_pass_fbo(p, &p->surfaces[p->surface_now].fbotex,
                        vp_w, vp_h, FBOTEX_FUZZY);
        p->surfaces[p->surface_now].pts = p->image.mpi->pts;
        p->surface_idx = p->surface_now;
    }

    // Find the right frame for this instant
    if (t->current&& t->current->pts != MP_NOPTS_VALUE) {
        int next = fbosurface_wrap(p->surface_now + 1);
        while (p->surfaces[next].pts != MP_NOPTS_VALUE &&
               p->surfaces[next].pts > p->surfaces[p->surface_now].pts &&
               p->surfaces[p->surface_now].pts < t->current->pts)
        {
            p->surface_now = next;
            next = fbosurface_wrap(next + 1);
        }
    }

    // Figure out the queue size. For illustration, a filter radius of 2 would
    // look like this: _ A [B] C D _
    // A is surface_bse, B is surface_now, C is surface_now+1 and D is
    // surface_end.
    struct scaler *tscale = &p->scaler[SCALER_TSCALE];
    reinit_scaler(p, tscale, &p->opts.scaler[SCALER_TSCALE], 1, tscale_sizes);
    bool oversample = strcmp(tscale->conf.kernel.name, "oversample") == 0;
    int size;

    if (oversample) {
        size = 2;
    } else {
        assert(tscale->kernel && !tscale->kernel->polar);
        size = ceil(tscale->kernel->size);
        assert(size <= TEXUNIT_VIDEO_NUM);
    }

    int radius = size/2;
    int surface_now = p->surface_now;
    int surface_bse = fbosurface_wrap(surface_now - (radius-1));
    int surface_end = fbosurface_wrap(surface_now + radius);
    assert(fbosurface_wrap(surface_bse + size-1) == surface_end);

    // Render new frames while there's room in the queue. Note that technically,
    // this should be done before the step where we find the right frame, but
    // it only barely matters at the very beginning of playback, and this way
    // makes the code much more linear.
    int surface_dst = fbosurface_wrap(p->surface_idx+1);
    for (int i = 0; i < t->num_frames; i++) {
        // Avoid overwriting data we might still need
        if (surface_dst == surface_bse - 1)
            break;

        struct mp_image *f = t->frames[i];
        if (!mp_image_params_equal(&f->params, &p->real_image_params) ||
            f->pts == MP_NOPTS_VALUE)
            continue;

        if (f->pts > p->surfaces[p->surface_idx].pts) {
            gl_video_upload_image(p, f);
            pass_render_frame(p);
            finish_pass_fbo(p, &p->surfaces[surface_dst].fbotex,
                            vp_w, vp_h, FBOTEX_FUZZY);
            p->surfaces[surface_dst].pts = f->pts;
            p->surface_idx = surface_dst;
            surface_dst = fbosurface_wrap(surface_dst+1);
        }
    }

    // Figure out whether the queue is "valid". A queue is invalid if the
    // frames' PTS is not monotonically increasing. Anything else is invalid,
    // so avoid blending incorrect data and just draw the latest frame as-is.
    // Possible causes for failure of this condition include seeks, pausing,
    // end of playback or start of playback.
    bool valid = true;
    for (int i = surface_bse, ii; valid && i != surface_end; i = ii) {
        ii = fbosurface_wrap(i+1);
        if (p->surfaces[i].pts == MP_NOPTS_VALUE ||
            p->surfaces[ii].pts == MP_NOPTS_VALUE)
        {
            valid = false;
        } else if (p->surfaces[ii].pts < p->surfaces[i].pts) {
            valid = false;
            MP_DBG(p, "interpolation queue underrun\n");
        }
    }

    // Update OSD PTS to synchronize subtitles with the displayed frame
    p->osd_pts = p->surfaces[surface_now].pts;

    // Finally, draw the right mix of frames to the screen.
    if (!valid || t->still) {
        // surface_now is guaranteed to be valid, so we can safely use it.
        pass_read_fbo(p, &p->surfaces[surface_now].fbotex);
        p->is_interpolated = false;
    } else {
        double mix = t->vsync_offset / t->ideal_frame_duration;
        // The scaler code always wants the fcoord to be between 0 and 1,
        // so we try to adjust by using the previous set of N frames instead
        // (which requires some extra checking to make sure it's valid)
        if (mix < 0.0) {
            int prev = fbosurface_wrap(surface_bse - 1);
            if (p->surfaces[prev].pts != MP_NOPTS_VALUE &&
                p->surfaces[prev].pts < p->surfaces[surface_bse].pts)
            {
                mix += 1.0;
                surface_bse = prev;
            } else {
                mix = 0.0; // at least don't blow up, this should only
                           // ever happen at the start of playback
            }
        }

        // Blend the frames together
        if (oversample) {
            double vsync_dist = t->vsync_interval / t->ideal_frame_duration,
                   threshold = tscale->conf.kernel.params[0];
            threshold = isnan(threshold) ? 0.0 : threshold;
            mix = (1 - mix) / vsync_dist;
            mix = mix <= 0 + threshold ? 0 : mix;
            mix = mix >= 1 - threshold ? 1 : mix;
            mix = 1 - mix;
            gl_sc_uniform_f(p->sc, "inter_coeff", mix);
            GLSL(color = mix(texture(texture0, texcoord0),
                             texture(texture1, texcoord1),
                             inter_coeff);)
        } else {
            gl_sc_uniform_f(p->sc, "fcoord", mix);
            pass_sample_separated_gen(p->sc, tscale, 0, 0);
        }

        // Load all the required frames
        for (int i = 0; i < size; i++) {
            struct img_tex img =
                img_tex_fbo(&p->surfaces[fbosurface_wrap(surface_bse+i)].fbotex,
                            identity_trans, PLANE_RGB, p->components);
            // Since the code in pass_sample_separated currently assumes
            // the textures are bound in-order and starting at 0, we just
            // assert to make sure this is the case (which it should always be)
            int id = pass_bind(p, img);
            assert(id == i);
        }

        MP_DBG(p, "inter frame dur: %f vsync: %f, mix: %f\n",
               t->ideal_frame_duration, t->vsync_interval, mix);
        p->is_interpolated = true;
    }
    pass_draw_to_screen(p, fbo);

    p->frames_drawn += 1;
}

// (fbo==0 makes BindFramebuffer select the screen backbuffer)
void gl_video_render_frame(struct gl_video *p, struct vo_frame *frame, int fbo)
{
    GL *gl = p->gl;
    struct video_image *vimg = &p->image;

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);

    bool has_frame = frame->current || vimg->mpi;

    if (!has_frame || p->dst_rect.x0 > 0 || p->dst_rect.y0 > 0 ||
        p->dst_rect.x1 < p->vp_w || p->dst_rect.y1 < abs(p->vp_h))
    {
        struct m_color c = p->opts.background;
        gl->ClearColor(c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
        gl->Clear(GL_COLOR_BUFFER_BIT);
    }

    if (has_frame) {
        gl_sc_set_vao(p->sc, &p->vao);

        bool interpolate = p->opts.interpolation && frame->display_synced &&
                           (p->frames_drawn || !frame->still);
        if (interpolate) {
            double ratio = frame->ideal_frame_duration / frame->vsync_interval;
            if (fabs(ratio - 1.0) < p->opts.interpolation_threshold)
                interpolate = false;
        }

        if (interpolate) {
            gl_video_interpolate_frame(p, frame, fbo);
        } else {
            bool is_new = !frame->redraw && !frame->repeat;
            if (is_new || !p->output_fbo_valid) {
                p->output_fbo_valid = false;

                gl_video_upload_image(p, frame->current);
                pass_render_frame(p);

                // For the non-interplation case, we draw to a single "cache"
                // FBO to speed up subsequent re-draws (if any exist)
                int dest_fbo = fbo;
                if (frame->num_vsyncs > 1 && frame->display_synced &&
                    !p->dumb_mode && gl->BlitFramebuffer)
                {
                    fbotex_change(&p->output_fbo, p->gl, p->log,
                                  p->vp_w, abs(p->vp_h),
                                  p->opts.fbo_format, 0);
                    dest_fbo = p->output_fbo.fbo;
                    p->output_fbo_valid = true;
                }
                pass_draw_to_screen(p, dest_fbo);
            }

            // "output fbo valid" and "output fbo needed" are equivalent
            if (p->output_fbo_valid) {
                gl->BindFramebuffer(GL_READ_FRAMEBUFFER, p->output_fbo.fbo);
                gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
                struct mp_rect rc = p->dst_rect;
                if (p->vp_h < 0) {
                    rc.y1 = -p->vp_h - p->dst_rect.y0;
                    rc.y0 = -p->vp_h - p->dst_rect.y1;
                }
                gl->BlitFramebuffer(rc.x0, rc.y0, rc.x1, rc.y1,
                                    rc.x0, rc.y0, rc.x1, rc.y1,
                                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
                gl->BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            }
        }
    }

    debug_check_gl(p, "after video rendering");

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);

    if (p->osd) {
        pass_draw_osd(p, p->opts.blend_subs ? OSD_DRAW_OSD_ONLY : 0,
                      p->osd_pts, p->osd_rect, p->vp_w, p->vp_h, fbo, true);
        debug_check_gl(p, "after OSD rendering");
    }

    gl->UseProgram(0);
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    // The playloop calls this last before waiting some time until it decides
    // to call flip_page(). Tell OpenGL to start execution of the GPU commands
    // while we sleep (this happens asynchronously).
    gl->Flush();

    p->frames_rendered++;
}

// vp_w/vp_h is the implicit size of the target framebuffer.
// vp_h can be negative to flip the screen.
void gl_video_resize(struct gl_video *p, int vp_w, int vp_h,
                     struct mp_rect *src, struct mp_rect *dst,
                     struct mp_osd_res *osd)
{
    p->src_rect = *src;
    p->dst_rect = *dst;
    p->osd_rect = *osd;
    p->vp_w = vp_w;
    p->vp_h = vp_h;

    gl_video_reset_surfaces(p);

    if (p->osd)
        mpgl_osd_resize(p->osd, p->osd_rect, p->image_params.stereo_out);
}

static bool unmap_image(struct gl_video *p, struct mp_image *mpi)
{
    GL *gl = p->gl;
    bool ok = true;
    struct video_image *vimg = &p->image;
    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, plane->gl_buffer);
        ok = gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER) && ok;
        mpi->planes[n] = NULL; // PBO offset 0
    }
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return ok;
}

static bool map_image(struct gl_video *p, struct mp_image *mpi)
{
    GL *gl = p->gl;

    if (!p->opts.pbo)
        return false;

    struct video_image *vimg = &p->image;

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];
        mpi->stride[n] = mp_image_plane_w(mpi, n) * p->image_desc.bytes[n];
        if (!plane->gl_buffer) {
            gl->GenBuffers(1, &plane->gl_buffer);
            gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, plane->gl_buffer);
            size_t buffer_size = mp_image_plane_h(mpi, n) * mpi->stride[n];
            gl->BufferData(GL_PIXEL_UNPACK_BUFFER, buffer_size,
                           NULL, GL_DYNAMIC_DRAW);
        }
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, plane->gl_buffer);
        mpi->planes[n] = gl->MapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        if (!mpi->planes[n]) {
            unmap_image(p, mpi);
            return false;
        }
    }
    memset(mpi->bufs, 0, sizeof(mpi->bufs));
    return true;
}

static void gl_video_upload_image(struct gl_video *p, struct mp_image *mpi)
{
    GL *gl = p->gl;
    struct video_image *vimg = &p->image;

    mpi = mp_image_new_ref(mpi);
    if (!mpi)
        abort();

    talloc_free(vimg->mpi);
    vimg->mpi = mpi;
    p->osd_pts = mpi->pts;
    p->frames_uploaded++;

    if (p->hwdec_active) {
        GLuint imgtex[4] = {0};
        bool ok = p->hwdec->driver->map_image(p->hwdec, vimg->mpi, imgtex) >= 0;
        for (int n = 0; n < p->plane_count; n++)
            vimg->planes[n].gl_texture = ok ? imgtex[n] : -1;
        return;
    }

    assert(mpi->num_planes == p->plane_count);

    mp_image_t pbo_mpi = *mpi;
    bool pbo = map_image(p, &pbo_mpi);
    if (pbo) {
        mp_image_copy(&pbo_mpi, mpi);
        if (unmap_image(p, &pbo_mpi)) {
            mpi = &pbo_mpi;
        } else {
            MP_FATAL(p, "Video PBO upload failed. Disabling PBOs.\n");
            pbo = false;
            p->opts.pbo = 0;
        }
    }

    vimg->image_flipped = mpi->stride[0] < 0;
    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];
        if (pbo)
            gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, plane->gl_buffer);
        gl->ActiveTexture(GL_TEXTURE0 + n);
        gl->BindTexture(plane->gl_target, plane->gl_texture);
        glUploadTex(gl, plane->gl_target, plane->gl_format, plane->gl_type,
                    mpi->planes[n], mpi->stride[n], 0, 0, plane->w, plane->h, 0);
    }
    gl->ActiveTexture(GL_TEXTURE0);
    if (pbo)
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

static bool test_fbo(struct gl_video *p)
{
    GL *gl = p->gl;
    bool success = false;
    MP_VERBOSE(p, "Testing user-set FBO format (0x%x)\n",
                   (unsigned)p->opts.fbo_format);
    struct fbotex fbo = {0};
    if (fbotex_init(&fbo, p->gl, p->log, 16, 16, p->opts.fbo_format)) {
        gl->BindFramebuffer(GL_FRAMEBUFFER, fbo.fbo);
        gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
        success = true;
    }
    fbotex_uninit(&fbo);
    glCheckError(gl, p->log, "FBO test");
    return success;
}

// Return whether dumb-mode can be used without disabling any features.
// Essentially, vo_opengl with mostly default settings will return true.
static bool check_dumb_mode(struct gl_video *p)
{
    struct gl_video_opts *o = &p->opts;
    if (p->use_integer_conversion)
        return false;
    if (o->dumb_mode)
        return true;
    if (o->target_prim || o->target_trc || o->linear_scaling ||
        o->correct_downscaling || o->sigmoid_upscaling || o->interpolation ||
        o->blend_subs || o->deband || o->unsharp || o->prescale_luma)
        return false;
    // check remaining scalers (tscale is already implicitly excluded above)
    for (int i = 0; i < SCALER_COUNT; i++) {
        if (i != SCALER_TSCALE) {
            const char *name = o->scaler[i].kernel.name;
            if (name && strcmp(name, "bilinear") != 0)
                return false;
        }
    }
    if (o->pre_shaders && o->pre_shaders[0])
        return false;
    if (o->post_shaders && o->post_shaders[0])
        return false;
    if (p->use_lut_3d)
        return false;
    return true;
}

// Disable features that are not supported with the current OpenGL version.
static void check_gl_features(struct gl_video *p)
{
    GL *gl = p->gl;
    bool have_float_tex = gl->mpgl_caps & MPGL_CAP_FLOAT_TEX;
    bool have_fbo = gl->mpgl_caps & MPGL_CAP_FB;
    bool have_3d_tex = gl->mpgl_caps & MPGL_CAP_3D_TEX;
    bool have_mix = gl->glsl_version >= 130;
    bool have_texrg = gl->mpgl_caps & MPGL_CAP_TEX_RG;

    if (have_fbo) {
        if (!p->opts.fbo_format) {
            p->opts.fbo_format = GL_RGBA16;
            if (gl->es)
                p->opts.fbo_format = have_float_tex ? GL_RGBA16F : GL_RGB10_A2;
        }
        have_fbo = test_fbo(p);
    }

    if (gl->es && p->opts.pbo) {
        p->opts.pbo = 0;
        MP_WARN(p, "Disabling PBOs (GLES unsupported).\n");
    }

    p->forced_dumb_mode = p->opts.dumb_mode || !have_fbo || !have_texrg;
    bool voluntarily_dumb = check_dumb_mode(p);
    if (p->forced_dumb_mode || voluntarily_dumb) {
        if (voluntarily_dumb) {
            MP_VERBOSE(p, "No advanced processing required. Enabling dumb mode.\n");
        } else if (!p->opts.dumb_mode) {
            MP_WARN(p, "High bit depth FBOs unsupported. Enabling dumb mode.\n"
                       "Most extended features will be disabled.\n");
        }
        p->dumb_mode = true;
        p->use_lut_3d = false;
        // Most things don't work, so whitelist all options that still work.
        struct gl_video_opts new_opts = {
            .gamma = p->opts.gamma,
            .gamma_auto = p->opts.gamma_auto,
            .pbo = p->opts.pbo,
            .fbo_format = p->opts.fbo_format,
            .alpha_mode = p->opts.alpha_mode,
            .use_rectangle = p->opts.use_rectangle,
            .background = p->opts.background,
            .dither_algo = -1,
        };
        for (int n = 0; n < SCALER_COUNT; n++)
            new_opts.scaler[n] = gl_video_opts_def.scaler[n];
        assign_options(&p->opts, &new_opts);
        p->opts.deband_opts = m_config_alloc_struct(NULL, &deband_conf);
        return;
    }
    p->dumb_mode = false;

    // Normally, we want to disable them by default if FBOs are unavailable,
    // because they will be slow (not critically slow, but still slower).
    // Without FP textures, we must always disable them.
    // I don't know if luminance alpha float textures exist, so disregard them.
    for (int n = 0; n < SCALER_COUNT; n++) {
        const struct filter_kernel *kernel =
            mp_find_filter_kernel(p->opts.scaler[n].kernel.name);
        if (kernel) {
            char *reason = NULL;
            if (!have_float_tex)
                reason = "(float tex. missing)";
            if (reason) {
                p->opts.scaler[n].kernel.name = "bilinear";
                MP_WARN(p, "Disabling scaler #%d %s.\n", n, reason);
            }
        }
    }

    // GLES3 doesn't provide filtered 16 bit integer textures
    // GLES2 doesn't even provide 3D textures
    if (p->use_lut_3d && (!have_3d_tex || gl->es)) {
        p->use_lut_3d = false;
        MP_WARN(p, "Disabling color management (GLES unsupported).\n");
    }

    int use_cms = p->opts.target_prim != MP_CSP_PRIM_AUTO ||
                  p->opts.target_trc != MP_CSP_TRC_AUTO || p->use_lut_3d;

    // mix() is needed for some gamma functions
    if (!have_mix && (p->opts.linear_scaling || p->opts.sigmoid_upscaling)) {
        p->opts.linear_scaling = false;
        p->opts.sigmoid_upscaling = false;
        MP_WARN(p, "Disabling linear/sigmoid scaling (GLSL version too old).\n");
    }
    if (!have_mix && use_cms) {
        p->opts.target_prim = MP_CSP_PRIM_AUTO;
        p->opts.target_trc = MP_CSP_TRC_AUTO;
        p->use_lut_3d = false;
        MP_WARN(p, "Disabling color management (GLSL version too old).\n");
    }
    if (!have_mix && p->opts.deband) {
        p->opts.deband = 0;
        MP_WARN(p, "Disabling debanding (GLSL version too old).\n");
    }

    if (p->opts.prescale_luma == 2) {
        if (p->opts.nnedi3_opts->upload == NNEDI3_UPLOAD_UBO) {
            // Check features for uniform buffer objects.
            if (!gl->BindBufferBase || !gl->GetUniformBlockIndex) {
                MP_WARN(p, "Disabling NNEDI3 (%s required).\n",
                        gl->es ? "OpenGL ES 3.0" : "OpenGL 3.1");
                p->opts.prescale_luma = 0;
            }
        } else if (p->opts.nnedi3_opts->upload == NNEDI3_UPLOAD_SHADER) {
            // Check features for hard coding approach.
            if ((!gl->es && gl->glsl_version < 330) ||
                (gl->es && gl->glsl_version < 300))
            {
                MP_WARN(p, "Disabling NNEDI3 (%s required).\n",
                        gl->es ? "OpenGL ES 3.0" : "OpenGL 3.3");
                p->opts.prescale_luma = 0;
            }
        }
    }
}

static void init_gl(struct gl_video *p)
{
    GL *gl = p->gl;

    debug_check_gl(p, "before init_gl");

    MP_VERBOSE(p, "Reported display depth: R=%d, G=%d, B=%d\n",
               gl->fb_r, gl->fb_g, gl->fb_b);

    gl->Disable(GL_DITHER);

    gl_vao_init(&p->vao, gl, sizeof(struct vertex), vertex_vao);

    gl_video_set_gl_state(p);

    // Test whether we can use 10 bit. Hope that testing a single format/channel
    // is good enough (instead of testing all 1-4 channels variants etc.).
    const struct fmt_entry *fmt = find_tex_format(gl, 2, 1);
    if (gl->GetTexLevelParameteriv && fmt->format) {
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
        GLint param = 0;
        if (pname)
            gl->GetTexLevelParameteriv(GL_TEXTURE_2D, 0, pname, &param);
        if (param) {
            MP_VERBOSE(p, "16 bit texture depth: %d.\n", (int)param);
            p->texture_16bit_depth = param;
        }
        gl->DeleteTextures(1, &tex);
    }

    debug_check_gl(p, "after init_gl");
}

void gl_video_uninit(struct gl_video *p)
{
    if (!p)
        return;

    GL *gl = p->gl;

    uninit_video(p);

    gl_sc_destroy(p->sc);

    gl_vao_uninit(&p->vao);

    gl->DeleteTextures(1, &p->lut_3d_texture);

    mpgl_osd_destroy(p->osd);

    gl_set_debug_logger(gl, NULL);

    assign_options(&p->opts, &(struct gl_video_opts){0});
    talloc_free(p);
}

void gl_video_set_gl_state(struct gl_video *p)
{
    GL *gl = p->gl;

    gl->ActiveTexture(GL_TEXTURE0);
    if (gl->mpgl_caps & MPGL_CAP_ROW_LENGTH)
        gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void gl_video_unset_gl_state(struct gl_video *p)
{
    /* nop */
}

void gl_video_reset(struct gl_video *p)
{
    gl_video_reset_surfaces(p);
}

bool gl_video_showing_interpolated_frame(struct gl_video *p)
{
    return p->is_interpolated;
}

// dest = src.<w> (always using 4 components)
static void packed_fmt_swizzle(char w[5], const struct fmt_entry *texfmt,
                               const struct packed_fmt_entry *fmt)
{
    const char *comp = "rgba";

    // Normally, we work with GL_RG
    if (texfmt && texfmt->internal_format == GL_LUMINANCE_ALPHA)
        comp = "ragb";

    for (int c = 0; c < 4; c++)
        w[c] = comp[MPMAX(fmt->components[c] - 1, 0)];
    w[4] = '\0';
}

// Like find_tex_format(), but takes bits (not bytes), and but if no fixed point
// format is available, return an unsigned integer format.
static const struct fmt_entry *find_plane_format(GL *gl, int bytes_per_comp,
                                                 int n_channels)
{
    const struct fmt_entry *e = find_tex_format(gl, bytes_per_comp, n_channels);
    if (e->format || gl->es < 300)
        return e;
    return &gl_ui_byte_formats_gles3[n_channels - 1 + (bytes_per_comp - 1) * 4];
}

static bool init_format(int fmt, struct gl_video *init)
{
    struct GL *gl = init->gl;

    init->hwdec_active = false;
    if (init->hwdec && init->hwdec->driver->imgfmt == fmt) {
        fmt = init->hwdec->converted_imgfmt;
        init->hwdec_active = true;
    }

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (!desc.id)
        return false;

    if (desc.num_planes > 4)
        return false;

    const struct fmt_entry *plane_format[4] = {0};

    init->color_swizzle[0] = '\0';
    init->has_alpha = false;

    // YUV/planar formats
    if (desc.flags & (MP_IMGFLAG_YUV_P | MP_IMGFLAG_RGB_P)) {
        int bits = desc.component_bits;
        if ((desc.flags & MP_IMGFLAG_NE) && bits >= 8 && bits <= 16) {
            init->has_alpha = desc.num_planes > 3;
            plane_format[0] = find_plane_format(gl, (bits + 7) / 8, 1);
            for (int p = 1; p < desc.num_planes; p++)
                plane_format[p] = plane_format[0];
            // RGB/planar
            if (desc.flags & MP_IMGFLAG_RGB_P)
                snprintf(init->color_swizzle, sizeof(init->color_swizzle), "brga");
            goto supported;
        }
    }

    // YUV/half-packed
    if (desc.flags & MP_IMGFLAG_YUV_NV) {
        int bits = desc.component_bits;
        if ((desc.flags & MP_IMGFLAG_NE) && bits >= 8 && bits <= 16) {
            plane_format[0] = find_plane_format(gl, (bits + 7) / 8, 1);
            plane_format[1] = find_plane_format(gl, (bits + 7) / 8, 2);
            if (desc.flags & MP_IMGFLAG_YUV_NV_SWAP)
                snprintf(init->color_swizzle, sizeof(init->color_swizzle), "rbga");
            goto supported;
        }
    }

    // XYZ (same organization as RGB packed, but requires conversion matrix)
    if (fmt == IMGFMT_XYZ12) {
        plane_format[0] = find_tex_format(gl, 2, 3);
        goto supported;
    }

    // Packed RGB special formats
    for (const struct fmt_entry *e = mp_to_gl_formats; e->mp_format; e++) {
        if (!gl->es && e->mp_format == fmt) {
            plane_format[0] = e;
            goto supported;
        }
    }

    // Packed RGB(A) formats
    for (const struct packed_fmt_entry *e = mp_packed_formats; e->fmt; e++) {
        if (e->fmt == fmt) {
            int n_comp = desc.bytes[0] / e->component_size;
            plane_format[0] = find_tex_format(gl, e->component_size, n_comp);
            packed_fmt_swizzle(init->color_swizzle, plane_format[0], e);
            init->has_alpha = e->components[3] != 0;
            goto supported;
        }
    }

    // Packed YUV Apple formats
    if (init->gl->mpgl_caps & MPGL_CAP_APPLE_RGB_422) {
        for (const struct fmt_entry *e = gl_apple_formats; e->mp_format; e++) {
            if (e->mp_format == fmt) {
                init->is_packed_yuv = true;
                snprintf(init->color_swizzle, sizeof(init->color_swizzle),
                         "gbra");
                plane_format[0] = e;
                goto supported;
            }
        }
    }

    // Unsupported format
    return false;

supported:

    if (desc.component_bits > 8 && desc.component_bits < 16) {
        if (init->texture_16bit_depth < 16)
            return false;
    }

    int use_integer = -1;
    for (int p = 0; p < desc.num_planes; p++) {
        if (!plane_format[p]->format)
            return false;
        int use_int_plane = !!is_integer_format(plane_format[p]);
        if (use_integer < 0)
            use_integer = use_int_plane;
        if (use_integer != use_int_plane)
            return false; // mixed planes not supported
    }
    init->use_integer_conversion = use_integer;

    if (init->use_integer_conversion && init->forced_dumb_mode)
        return false;

    for (int p = 0; p < desc.num_planes; p++) {
        struct texplane *plane = &init->image.planes[p];
        const struct fmt_entry *format = plane_format[p];
        assert(format);
        plane->gl_format = format->format;
        plane->gl_internal_format = format->internal_format;
        plane->gl_type = format->type;
        plane->use_integer = init->use_integer_conversion;
    }

    init->is_yuv = desc.flags & MP_IMGFLAG_YUV;
    init->plane_count = desc.num_planes;
    init->image_desc = desc;

    return true;
}

bool gl_video_check_format(struct gl_video *p, int mp_format)
{
    struct gl_video tmp = *p;
    return init_format(mp_format, &tmp);
}

void gl_video_config(struct gl_video *p, struct mp_image_params *params)
{
    mp_image_unrefp(&p->image.mpi);

    if (!mp_image_params_equal(&p->real_image_params, params)) {
        uninit_video(p);
        p->real_image_params = *params;
        p->image_params = *params;
        if (params->imgfmt)
            init_video(p);
    }

    gl_video_reset_surfaces(p);
}

void gl_video_set_osd_source(struct gl_video *p, struct osd_state *osd)
{
    mpgl_osd_destroy(p->osd);
    p->osd = NULL;
    p->osd_state = osd;
    recreate_osd(p);
}

struct gl_video *gl_video_init(GL *gl, struct mp_log *log, struct mpv_global *g,
                               struct gl_lcms *cms)
{
    if (gl->version < 210 && gl->es < 200) {
        mp_err(log, "At least OpenGL 2.1 or OpenGL ES 2.0 required.\n");
        return NULL;
    }

    struct gl_video *p = talloc_ptrtype(NULL, p);
    *p = (struct gl_video) {
        .gl = gl,
        .global = g,
        .log = log,
        .cms = cms,
        .opts = gl_video_opts_def,
        .gl_target = GL_TEXTURE_2D,
        .texture_16bit_depth = 16,
        .sc = gl_sc_create(gl, log),
    };
    for (int n = 0; n < SCALER_COUNT; n++)
        p->scaler[n] = (struct scaler){.index = n};
    gl_video_set_debug(p, true);
    init_gl(p);
    recreate_osd(p);
    return p;
}

// Get static string for scaler shader. If "tscale" is set to true, the
// scaler must be a separable convolution filter.
static const char *handle_scaler_opt(const char *name, bool tscale)
{
    if (name && name[0]) {
        const struct filter_kernel *kernel = mp_find_filter_kernel(name);
        if (kernel && (!tscale || !kernel->polar))
                return kernel->f.name;

        for (const char *const *filter = tscale ? fixed_tscale_filters
                                                : fixed_scale_filters;
             *filter; filter++) {
            if (strcmp(*filter, name) == 0)
                return *filter;
        }
    }
    return NULL;
}

static char **dup_str_array(void *parent, char **src)
{
    if (!src)
        return NULL;

    char **res = talloc_new(parent);
    int num = 0;
    for (int n = 0; src && src[n]; n++)
        MP_TARRAY_APPEND(res, res, num, talloc_strdup(res, src[n]));
    MP_TARRAY_APPEND(res, res, num, NULL);
    return res;
}

static void assign_options(struct gl_video_opts *dst, struct gl_video_opts *src)
{
    talloc_free(dst->scale_shader);
    talloc_free(dst->pre_shaders);
    talloc_free(dst->post_shaders);
    talloc_free(dst->deband_opts);
    talloc_free(dst->superxbr_opts);
    talloc_free(dst->nnedi3_opts);

    *dst = *src;

    if (src->deband_opts)
        dst->deband_opts = m_sub_options_copy(NULL, &deband_conf, src->deband_opts);

    if (src->superxbr_opts) {
        dst->superxbr_opts = m_sub_options_copy(NULL, &superxbr_conf,
                                                src->superxbr_opts);
    }

    if (src->nnedi3_opts) {
        dst->nnedi3_opts = m_sub_options_copy(NULL, &nnedi3_conf,
                                                src->nnedi3_opts);
    }

    for (int n = 0; n < SCALER_COUNT; n++) {
        dst->scaler[n].kernel.name =
            (char *)handle_scaler_opt(dst->scaler[n].kernel.name,
                                      n == SCALER_TSCALE);
    }

    dst->scale_shader = talloc_strdup(NULL, dst->scale_shader);
    dst->pre_shaders = dup_str_array(NULL, dst->pre_shaders);
    dst->post_shaders = dup_str_array(NULL, dst->post_shaders);
}

// Set the options, and possibly update the filter chain too.
// Note: assumes all options are valid and verified by the option parser.
void gl_video_set_options(struct gl_video *p, struct gl_video_opts *opts)
{
    assign_options(&p->opts, opts);

    check_gl_features(p);
    uninit_rendering(p);

    if (p->opts.interpolation && !p->global->opts->video_sync && !p->dsi_warned) {
        MP_WARN(p, "Interpolation now requires enabling display-sync mode.\n"
                   "E.g.: --video-sync=display-resample\n");
        p->dsi_warned = true;
    }
}

void gl_video_configure_queue(struct gl_video *p, struct vo *vo)
{
    int queue_size = 1;

    // Figure out an adequate size for the interpolation queue. The larger
    // the radius, the earlier we need to queue frames.
    if (p->opts.interpolation) {
        const struct filter_kernel *kernel =
            mp_find_filter_kernel(p->opts.scaler[SCALER_TSCALE].kernel.name);
        if (kernel) {
            double radius = kernel->f.radius;
            radius = radius > 0 ? radius : p->opts.scaler[SCALER_TSCALE].radius;
            queue_size += 1 + ceil(radius);
        } else {
            // Oversample case
            queue_size += 2;
        }
    }

    vo_set_queue_params(vo, 0, queue_size);
}

struct mp_csp_equalizer *gl_video_eq_ptr(struct gl_video *p)
{
    return &p->video_eq;
}

// Call when the mp_csp_equalizer returned by gl_video_eq_ptr() was changed.
void gl_video_eq_update(struct gl_video *p)
{
}

static int validate_scaler_opt(struct mp_log *log, const m_option_t *opt,
                               struct bstr name, struct bstr param)
{
    char s[20] = {0};
    int r = 1;
    bool tscale = bstr_equals0(name, "tscale");
    if (bstr_equals0(param, "help")) {
        r = M_OPT_EXIT - 1;
    } else {
        snprintf(s, sizeof(s), "%.*s", BSTR_P(param));
        if (!handle_scaler_opt(s, tscale))
            r = M_OPT_INVALID;
    }
    if (r < 1) {
        mp_info(log, "Available scalers:\n");
        for (const char *const *filter = tscale ? fixed_tscale_filters
                                                : fixed_scale_filters;
             *filter; filter++) {
            mp_info(log, "    %s\n", *filter);
        }
        for (int n = 0; mp_filter_kernels[n].f.name; n++) {
            if (!tscale || !mp_filter_kernels[n].polar)
                mp_info(log, "    %s\n", mp_filter_kernels[n].f.name);
        }
        if (s[0])
            mp_fatal(log, "No scaler named '%s' found!\n", s);
    }
    return r;
}

static int validate_window_opt(struct mp_log *log, const m_option_t *opt,
                               struct bstr name, struct bstr param)
{
    char s[20] = {0};
    int r = 1;
    if (bstr_equals0(param, "help")) {
        r = M_OPT_EXIT - 1;
    } else {
        snprintf(s, sizeof(s), "%.*s", BSTR_P(param));
        const struct filter_window *window = mp_find_filter_window(s);
        if (!window)
            r = M_OPT_INVALID;
    }
    if (r < 1) {
        mp_info(log, "Available windows:\n");
        for (int n = 0; mp_filter_windows[n].name; n++)
            mp_info(log, "    %s\n", mp_filter_windows[n].name);
        if (s[0])
            mp_fatal(log, "No window named '%s' found!\n", s);
    }
    return r;
}

float gl_video_scale_ambient_lux(float lmin, float lmax,
                                 float rmin, float rmax, float lux)
{
    assert(lmax > lmin);

    float num = (rmax - rmin) * (log10(lux) - log10(lmin));
    float den = log10(lmax) - log10(lmin);
    float result = num / den + rmin;

    // clamp the result
    float max = MPMAX(rmax, rmin);
    float min = MPMIN(rmax, rmin);
    return MPMAX(MPMIN(result, max), min);
}

void gl_video_set_ambient_lux(struct gl_video *p, int lux)
{
    if (p->opts.gamma_auto) {
        float gamma = gl_video_scale_ambient_lux(16.0, 64.0, 2.40, 1.961, lux);
        MP_VERBOSE(p, "ambient light changed: %dlux (gamma: %f)\n", lux, gamma);
        p->opts.gamma = MPMIN(1.0, 1.961 / gamma);
        gl_video_eq_update(p);
    }
}

void gl_video_set_hwdec(struct gl_video *p, struct gl_hwdec *hwdec)
{
    p->hwdec = hwdec;
    mp_image_unrefp(&p->image.mpi);
}
