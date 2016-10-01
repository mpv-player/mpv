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
#include <stdarg.h>
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
#include "formats.h"
#include "utils.h"
#include "hwdec.h"
#include "osd.h"
#include "stream/stream.h"
#include "video_shaders.h"
#include "user_shaders.h"
#include "video/out/filter_kernels.h"
#include "video/out/aspect.h"
#include "video/out/dither.h"
#include "video/out/vo.h"

// Maximal number of saved textures (for user script purposes)
#define MAX_TEXTURE_HOOKS 16
#define MAX_SAVED_TEXTURES 32

// scale/cscale arguments that map directly to shader filter routines.
// Note that the convolution filters are not included in this list.
static const char *const fixed_scale_filters[] = {
    "bilinear",
    "bicubic_fast",
    "oversample",
    NULL
};
static const char *const fixed_tscale_filters[] = {
    "oversample",
    "linear",
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
    int tex_w, tex_h;
    GLint gl_internal_format;
    GLenum gl_target;
    bool use_integer;
    GLenum gl_format;
    GLenum gl_type;
    GLuint gl_texture;
    char swizzle[5];
    bool flipped;
    struct gl_pbo_upload pbo;
};

struct video_image {
    struct texplane planes[4];
    struct mp_image *mpi;       // original input image
    bool hwdec_mapped;
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
    int w, h; // logical size (after transformation)
    struct gl_transform transform; // rendering transformation
    char swizzle[5];
};

// A named img_tex, for user scripting purposes
struct saved_tex {
    const char *name;
    struct img_tex tex;
};

// A texture hook. This is some operation that transforms a named texture as
// soon as it's generated
struct tex_hook {
    char *hook_tex;
    char *save_tex;
    char *bind_tex[TEXUNIT_VIDEO_NUM];
    int components; // how many components are relevant (0 = same as input)
    void *priv; // this can be set to whatever the hook wants
    void (*hook)(struct gl_video *p, struct img_tex tex, // generates GLSL
                 struct gl_transform *trans, void *priv);
    void (*free)(struct tex_hook *hook);
    bool (*cond)(struct gl_video *p, struct img_tex tex, void *priv);
};

struct fbosurface {
    struct fbotex fbotex;
    double pts;
};

#define FBOSURFACES_MAX 10

struct cached_file {
    char *path;
    struct bstr body;
};

struct gl_video {
    GL *gl;

    struct mpv_global *global;
    struct mp_log *log;
    struct gl_video_opts opts;
    struct m_config_cache *opts_cache;
    struct gl_lcms *cms;
    bool gl_debug;

    int texture_16bit_depth;    // actual bits available in 16 bit textures
    int fb_depth;               // actual bits available in GL main framebuffer

    struct gl_shader_cache *sc;

    struct gl_vao vao;

    struct osd_state *osd_state;
    struct mpgl_osd *osd;
    double osd_pts;

    GLuint lut_3d_texture;
    bool use_lut_3d;
    int lut_3d_size[3];

    GLuint dither_texture;
    int dither_size;

    struct gl_timer *upload_timer;
    struct gl_timer *render_timer;
    struct gl_timer *present_timer;

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
    struct fbotex scale_fbo[4];
    struct fbotex integer_fbo[4];
    struct fbotex indirect_fbo;
    struct fbotex blend_subs_fbo;
    struct fbotex output_fbo;
    struct fbosurface surfaces[FBOSURFACES_MAX];
    struct fbotex vdpau_deinterleave_fbo[2];

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

    // hooks and saved textures
    struct saved_tex saved_tex[MAX_SAVED_TEXTURES];
    int saved_tex_num;
    struct tex_hook tex_hooks[MAX_TEXTURE_HOOKS];
    int tex_hook_num;
    struct fbotex hook_fbos[MAX_SAVED_TEXTURES];
    int hook_fbo_num;

    int frames_uploaded;
    int frames_rendered;
    AVLFG lfg;

    // Cached because computing it can take relatively long
    int last_dither_matrix_size;
    float *last_dither_matrix;

    struct cached_file *files;
    int num_files;

    struct gl_hwdec *hwdec;
    bool hwdec_active;

    bool dsi_warned;
    bool broken_frame; // temporary error state
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

static const struct gl_video_opts gl_video_opts_def = {
    .dither_algo = DITHER_FRUIT,
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
    .alpha_mode = ALPHA_BLEND_TILES,
    .background = {0, 0, 0, 255},
    .gamma = 1.0f,
    .target_brightness = 250,
    .hdr_tone_mapping = TONE_MAPPING_HABLE,
    .tone_mapping_param = NAN,
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
        OPT_FLAG("opengl-dumb-mode", dumb_mode, 0),
        OPT_FLOATRANGE("opengl-gamma", gamma, 0, 0.1, 2.0),
        OPT_FLAG("gamma-auto", gamma_auto, 0),
        OPT_CHOICE_C("target-prim", target_prim, 0, mp_csp_prim_names),
        OPT_CHOICE_C("target-trc", target_trc, 0, mp_csp_trc_names),
        OPT_INTRANGE("target-brightness", target_brightness, 0, 1, 100000),
        OPT_CHOICE("hdr-tone-mapping", hdr_tone_mapping, 0,
                   ({"clip",     TONE_MAPPING_CLIP},
                    {"reinhard", TONE_MAPPING_REINHARD},
                    {"hable",    TONE_MAPPING_HABLE},
                    {"gamma",    TONE_MAPPING_GAMMA},
                    {"linear",   TONE_MAPPING_LINEAR})),
        OPT_FLOAT("tone-mapping-param", tone_mapping_param, 0),
        OPT_FLAG("opengl-pbo", pbo, 0),
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
        OPT_CHOICE("opengl-fbo-format", fbo_format, 0,
                   ({"rgb8",   GL_RGB8},
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
                   ({"fruit", DITHER_FRUIT},
                    {"ordered", DITHER_ORDERED},
                    {"no", DITHER_NONE})),
        OPT_INTRANGE("dither-size-fruit", dither_size, 0, 2, 8),
        OPT_FLAG("temporal-dither", temporal_dither, 0),
        OPT_INTRANGE("temporal-dither-period", temporal_dither_period, 0, 1, 128),
        OPT_CHOICE("alpha", alpha_mode, 0,
                   ({"no", ALPHA_NO},
                    {"yes", ALPHA_YES},
                    {"blend", ALPHA_BLEND},
                    {"blend-tiles", ALPHA_BLEND_TILES})),
        OPT_FLAG("opengl-rectangle-textures", use_rectangle, 0),
        OPT_COLOR("background", background, 0),
        OPT_FLAG("interpolation", interpolation, 0),
        OPT_FLOAT("interpolation-threshold", interpolation_threshold, 0),
        OPT_CHOICE("blend-subtitles", blend_subs, 0,
                   ({"no", BLEND_SUBS_NO},
                    {"yes", BLEND_SUBS_YES},
                    {"video", BLEND_SUBS_VIDEO})),
        OPT_STRINGLIST("opengl-shaders", user_shaders, 0),
        OPT_FLAG("deband", deband, 0),
        OPT_SUBSTRUCT("deband", deband_opts, deband_conf, 0),
        OPT_FLOAT("sharpen", unsharp, 0),
        OPT_INTRANGE("opengl-tex-pad-x", tex_pad_x, 0, 0, 4096),
        OPT_INTRANGE("opengl-tex-pad-y", tex_pad_y, 0, 0, 4096),
        OPT_SUBSTRUCT("", icc_opts, mp_icc_conf, 0),

        {0}
    },
    .size = sizeof(struct gl_video_opts),
    .defaults = &gl_video_opts_def,
    .change_flags = UPDATE_RENDERER,
};

#define LEGACY_SCALER_OPTS(n)                \
    OPT_SUBOPT_LEGACY(n, n),                           \
    OPT_SUBOPT_LEGACY(n"-param1", n"-param1"),         \
    OPT_SUBOPT_LEGACY(n"-param2", n"-param2"),         \
    OPT_SUBOPT_LEGACY(n"-blur",   n"-blur"),           \
    OPT_SUBOPT_LEGACY(n"-wparam", n"-wparam"),         \
    OPT_SUBOPT_LEGACY(n"-clamp",  n"-clamp"),          \
    OPT_SUBOPT_LEGACY(n"-radius", n"-radius"),         \
    OPT_SUBOPT_LEGACY(n"-antiring", n"-antiring"),     \
    OPT_SUBOPT_LEGACY(n"-window", n"-window")

const struct m_sub_options gl_video_conf_legacy = {
    .opts = (const m_option_t[]) {
        OPT_SUBOPT_LEGACY("dumb-mode", "opengl-dumb-mode"),
        OPT_SUBOPT_LEGACY("gamma", "opengl-gamma"),
        OPT_SUBOPT_LEGACY("gamma-auto", "gamma-auto"),
        OPT_SUBOPT_LEGACY("target-prim", "target-prim"),
        OPT_SUBOPT_LEGACY("target-trc", "target-trc"),
        OPT_SUBOPT_LEGACY("target-brightness", "target-brightness"),
        OPT_SUBOPT_LEGACY("hdr-tone-mapping", "hdr-tone-mapping"),
        OPT_SUBOPT_LEGACY("tone-mapping-param", "tone-mapping-param"),
        OPT_SUBOPT_LEGACY("pbo", "opengl-pbo"),
        LEGACY_SCALER_OPTS("scale"),
        LEGACY_SCALER_OPTS("dscale"),
        LEGACY_SCALER_OPTS("cscale"),
        LEGACY_SCALER_OPTS("tscale"),
        OPT_SUBOPT_LEGACY("scaler-lut-size", "scaler-lut-size"),
        OPT_SUBOPT_LEGACY("scaler-resizes-only", "scaler-resizes-only"),
        OPT_SUBOPT_LEGACY("linear-scaling", "linear-scaling"),
        OPT_SUBOPT_LEGACY("correct-downscaling", "correct-downscaling"),
        OPT_SUBOPT_LEGACY("sigmoid-upscaling", "sigmoid-upscaling"),
        OPT_SUBOPT_LEGACY("sigmoid-center", "sigmoid-center"),
        OPT_SUBOPT_LEGACY("sigmoid-slope", "sigmoid-slope"),
        OPT_SUBOPT_LEGACY("fbo-format", "opengl-fbo-format"),
        OPT_SUBOPT_LEGACY("dither-depth", "dither-depth"),
        OPT_SUBOPT_LEGACY("dither", "dither"),
        OPT_SUBOPT_LEGACY("dither-size-fruit", "dither-size-fruit"),
        OPT_SUBOPT_LEGACY("temporal-dither", "temporal-dither"),
        OPT_SUBOPT_LEGACY("temporal-dither-period", "temporal-dither-period"),
        OPT_SUBOPT_LEGACY("alpha", "alpha"),
        OPT_SUBOPT_LEGACY("rectangle-textures", "opengl-rectangle-textures"),
        OPT_SUBOPT_LEGACY("background", "background"),
        OPT_SUBOPT_LEGACY("interpolation", "interpolation"),
        OPT_SUBOPT_LEGACY("interpolation-threshold", "interpolation-threshold"),
        OPT_SUBOPT_LEGACY("blend-subtitles", "blend-subtitles"),
        OPT_SUBOPT_LEGACY("user-shaders", "opengl-shaders"),
        OPT_SUBOPT_LEGACY("deband", "deband"),
        OPT_SUBOPT_LEGACY("deband-iterations", "deband-iterations"),
        OPT_SUBOPT_LEGACY("deband-threshold", "deband-threshold"),
        OPT_SUBOPT_LEGACY("deband-range", "deband-range"),
        OPT_SUBOPT_LEGACY("deband-grain", "deband-grain"),
        OPT_SUBOPT_LEGACY("sharpen", "sharpen"),
        OPT_SUBOPT_LEGACY("icc-profile", "icc-profile"),
        OPT_SUBOPT_LEGACY("icc-profile-auto", "icc-profile-auto"),
        OPT_SUBOPT_LEGACY("icc-cache-dir", "icc-cache-dir"),
        OPT_SUBOPT_LEGACY("icc-intent", "icc-intent"),
        OPT_SUBOPT_LEGACY("icc-contrast", "icc-contrast"),
        OPT_SUBOPT_LEGACY("3dlut-size", "icc-3dlut-size"),

        OPT_REMOVED("approx-gamma", "this is always enabled now"),
        OPT_REMOVED("cscale-down", "chroma is never downscaled"),
        OPT_REMOVED("scale-sep", "this is set automatically whenever sane"),
        OPT_REMOVED("indirect", "this is set automatically whenever sane"),
        OPT_REMOVED("srgb", "use target-prim=bt709:target-trc=srgb instead"),
        OPT_REMOVED("source-shader", "use :deband to enable debanding"),
        OPT_REMOVED("prescale-luma", "use opengl-shaders for prescaling"),
        OPT_REMOVED("scale-shader", "use opengl-shaders instead"),
        OPT_REMOVED("pre-shaders", "use opengl-shaders instead"),
        OPT_REMOVED("post-shaders", "use opengl-shaders instead"),

        OPT_SUBOPT_LEGACY("lscale", "scale"),
        OPT_SUBOPT_LEGACY("lscale-down", "scale-down"),
        OPT_SUBOPT_LEGACY("lparam1", "scale-param1"),
        OPT_SUBOPT_LEGACY("lparam2", "scale-param2"),
        OPT_SUBOPT_LEGACY("lradius", "scale-radius"),
        OPT_SUBOPT_LEGACY("lantiring", "scale-antiring"),
        OPT_SUBOPT_LEGACY("cparam1", "cscale-param1"),
        OPT_SUBOPT_LEGACY("cparam2", "cscale-param2"),
        OPT_SUBOPT_LEGACY("cradius", "cscale-radius"),
        OPT_SUBOPT_LEGACY("cantiring", "cscale-antiring"),
        OPT_SUBOPT_LEGACY("smoothmotion", "interpolation"),
        OPT_SUBOPT_LEGACY("smoothmotion-threshold", "tscale-param1"),
        OPT_SUBOPT_LEGACY("scale-down", "dscale"),
        OPT_SUBOPT_LEGACY("fancy-downscaling", "correct-downscaling"),

        {0}
    },
};

static void uninit_rendering(struct gl_video *p);
static void uninit_scaler(struct gl_video *p, struct scaler *scaler);
static void check_gl_features(struct gl_video *p);
static bool init_format(struct gl_video *p, int fmt, bool test_only);
static void init_image_desc(struct gl_video *p, int fmt);
static bool gl_video_upload_image(struct gl_video *p, struct mp_image *mpi);
static const char *handle_scaler_opt(const char *name, bool tscale);
static void reinit_from_options(struct gl_video *p);
static void get_scale_factors(struct gl_video *p, bool transpose_rot, double xy[2]);
static void gl_video_setup_hooks(struct gl_video *p);

#define GLSL(x) gl_sc_add(p->sc, #x "\n");
#define GLSLF(...) gl_sc_addf(p->sc, __VA_ARGS__)
#define GLSLHF(...) gl_sc_haddf(p->sc, __VA_ARGS__)

static struct bstr load_cached_file(struct gl_video *p, const char *path)
{
    if (!path || !path[0])
        return (struct bstr){0};
    for (int n = 0; n < p->num_files; n++) {
        if (strcmp(p->files[n].path, path) == 0)
            return p->files[n].body;
    }
    // not found -> load it
    struct bstr s = stream_read_file(path, p, p->global, 1024000); // 1024 kB
    if (s.len) {
        struct cached_file new = {
            .path = talloc_strdup(p, path),
            .body = s,
        };
        MP_TARRAY_APPEND(p, p->files, p->num_files, new);
        return new.body;
    }
    return (struct bstr){0};
}

static void debug_check_gl(struct gl_video *p, const char *msg)
{
    if (p->gl_debug)
        gl_check_error(p->gl, p->log, msg);
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

static void gl_video_reset_hooks(struct gl_video *p)
{
    for (int i = 0; i < p->tex_hook_num; i++) {
        if (p->tex_hooks[i].free)
            p->tex_hooks[i].free(&p->tex_hooks[i]);
    }

    p->tex_hook_num = 0;
}

static inline int fbosurface_wrap(int id)
{
    id = id % FBOSURFACES_MAX;
    return id < 0 ? id + FBOSURFACES_MAX : id;
}

static void reinit_osd(struct gl_video *p)
{
    mpgl_osd_destroy(p->osd);
    p->osd = NULL;
    if (p->osd_state) {
        p->osd = mpgl_osd_init(p->gl, p->log, p->osd_state);
        mpgl_osd_set_options(p->osd, p->opts.pbo);
    }
}

static void uninit_rendering(struct gl_video *p)
{
    GL *gl = p->gl;

    for (int n = 0; n < SCALER_COUNT; n++)
        uninit_scaler(p, &p->scaler[n]);

    gl->DeleteTextures(1, &p->dither_texture);
    p->dither_texture = 0;

    for (int n = 0; n < 4; n++) {
        fbotex_uninit(&p->merge_fbo[n]);
        fbotex_uninit(&p->scale_fbo[n]);
        fbotex_uninit(&p->integer_fbo[n]);
    }

    fbotex_uninit(&p->indirect_fbo);
    fbotex_uninit(&p->blend_subs_fbo);

    for (int n = 0; n < FBOSURFACES_MAX; n++)
        fbotex_uninit(&p->surfaces[n].fbotex);

    for (int n = 0; n < MAX_SAVED_TEXTURES; n++)
        fbotex_uninit(&p->hook_fbos[n]);

    for (int n = 0; n < 2; n++)
        fbotex_uninit(&p->vdpau_deinterleave_fbo[n]);

    gl_video_reset_surfaces(p);
    gl_video_reset_hooks(p);

    gl_sc_reset_error(p->sc);
}

bool gl_video_gamma_auto_enabled(struct gl_video *p)
{
    return p->opts.gamma_auto;
}

struct mp_colorspace gl_video_get_output_colorspace(struct gl_video *p)
{
    return (struct mp_colorspace) {
        .primaries = p->opts.target_prim,
        .gamma = p->opts.target_trc,
    };
}

// Warning: profile.start must point to a ta allocation, and the function
//          takes over ownership.
void gl_video_set_icc_profile(struct gl_video *p, bstr icc_data)
{
    if (gl_lcms_set_memory_profile(p->cms, icc_data))
        reinit_from_options(p);
}

bool gl_video_icc_auto_enabled(struct gl_video *p)
{
    return p->opts.icc_opts ? p->opts.icc_opts->profile_auto : false;
}

static bool gl_video_get_lut3d(struct gl_video *p, enum mp_csp_prim prim,
                               enum mp_csp_trc trc)
{
    GL *gl = p->gl;

    if (!p->use_lut_3d)
        return false;

    if (p->lut_3d_texture && !gl_lcms_has_changed(p->cms, prim, trc))
        return true;

    struct lut3d *lut3d = NULL;
    if (!gl_lcms_get_lut3d(p->cms, &lut3d, prim, trc) || !lut3d) {
        p->use_lut_3d = false;
        return false;
    }

    if (!p->lut_3d_texture)
        gl->GenTextures(1, &p->lut_3d_texture);

    gl->BindTexture(GL_TEXTURE_3D, p->lut_3d_texture);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->TexImage3D(GL_TEXTURE_3D, 0, GL_RGB16, lut3d->size[0], lut3d->size[1],
                   lut3d->size[2], 0, GL_RGB, GL_UNSIGNED_SHORT, lut3d->data);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_3D, 0);

    debug_check_gl(p, "after 3d lut creation");

    for (int i = 0; i < 3; i++)
        p->lut_3d_size[i] = lut3d->size[i];

    talloc_free(lut3d);

    return true;
}

// Fill an img_tex struct from an FBO + some metadata
static struct img_tex img_tex_fbo(struct fbotex *fbo, enum plane_type type,
                                  int components)
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
        .transform = identity_trans,
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
// w/h is used for recentering.
static void get_transform(float w, float h, int rotate, bool flip,
                          struct gl_transform *out_tr)
{
    int a = rotate % 90 ? 0 : rotate / 90;
    int sin90[4] = {0, 1, 0, -1}; // just to avoid rounding issues etc.
    int cos90[4] = {1, 0, -1, 0};
    struct gl_transform tr = {{{ cos90[a], sin90[a]},
                               {-sin90[a], cos90[a]}}};

    // basically, recenter to keep the whole image in view
    float b[2] = {1, 1};
    gl_transform_vec(tr, &b[0], &b[1]);
    tr.t[0] += b[0] < 0 ? w : 0;
    tr.t[1] += b[1] < 0 ? h : 0;

    if (flip) {
        struct gl_transform fliptr = {{{1, 0}, {0, -1}}, {0, h}};
        gl_transform_trans(fliptr, &tr);
    }

    *out_tr = tr;
}

// Return the chroma plane upscaled to luma size, but with additional padding
// for image sizes not aligned to subsampling.
static int chroma_upsize(int size, int shift)
{
    return mp_chroma_div_up(size, shift) << shift;
}

// Places a video_image's image textures + associated metadata into tex[]. The
// number of textures is equal to p->plane_count. Any necessary plane offsets
// are stored in off. (e.g. chroma position)
static void pass_get_img_tex(struct gl_video *p, struct video_image *vimg,
                             struct img_tex tex[4], struct gl_transform off[4])
{
    assert(vimg->mpi);

    int w = p->image_params.w;
    int h = p->image_params.h;

    // Determine the chroma offset
    float ls_w = 1.0 / (1 << p->image_desc.chroma_xs);
    float ls_h = 1.0 / (1 << p->image_desc.chroma_ys);

    struct gl_transform chroma = {{{ls_w, 0.0}, {0.0, ls_h}}};

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

    // The existing code assumes we just have a single tex multiplier for
    // all of the planes. This may change in the future
    float tex_mul = 1.0 / mp_get_csp_mul(p->image_params.color.space,
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
            .tex_w = t->tex_w,
            .tex_h = t->tex_h,
            .w = t->w,
            .h = t->h,
            .components = p->image_desc.components[n],
        };
        snprintf(tex[n].swizzle, sizeof(tex[n].swizzle), "%s", t->swizzle);
        get_transform(t->w, t->h, p->image_params.rotate, t->flipped,
                      &tex[n].transform);
        if (p->image_params.rotate % 180 == 90)
            MPSWAP(int, tex[n].w, tex[n].h);

        off[n] = identity_trans;

        if (type == PLANE_CHROMA) {
            struct gl_transform rot;
            get_transform(0, 0, p->image_params.rotate, true, &rot);

            struct gl_transform tr = chroma;
            gl_transform_vec(rot, &tr.t[0], &tr.t[1]);

            float dx = (chroma_upsize(w, p->image_desc.xs[n]) - w) * ls_w;
            float dy = (chroma_upsize(h, p->image_desc.ys[n]) - h) * ls_h;

            // Adjust the chroma offset if the real chroma size is fractional
            // due image sizes not aligned to chroma subsampling.
            struct gl_transform rot2;
            get_transform(0, 0, p->image_params.rotate, t->flipped, &rot2);
            if (rot2.m[0][0] < 0)
                tr.t[0] += dx;
            if (rot2.m[1][0] < 0)
                tr.t[0] += dy;
            if (rot2.m[0][1] < 0)
                tr.t[1] += dx;
            if (rot2.m[1][1] < 0)
                tr.t[1] += dy;

            off[n] = tr;
        }
    }
}

static void init_video(struct gl_video *p)
{
    GL *gl = p->gl;

    if (p->hwdec && gl_hwdec_test_format(p->hwdec, p->image_params.imgfmt)) {
        if (p->hwdec->driver->reinit(p->hwdec, &p->image_params) < 0)
            MP_ERR(p, "Initializing texture for hardware decoding failed.\n");
        init_image_desc(p, p->image_params.imgfmt);
        const char **exts = p->hwdec->glsl_extensions;
        for (int n = 0; exts && exts[n]; n++)
            gl_sc_enable_extension(p->sc, (char *)exts[n]);
        p->hwdec_active = true;
        if (p->hwdec->driver->overlay_frame) {
            MP_WARN(p, "Using HW-overlay mode. No GL filtering is performed "
                       "on the video!\n");
        }
    } else {
        init_format(p, p->image_params.imgfmt, false);
    }

    // Format-dependent checks.
    check_gl_features(p);

    mp_image_params_guess_csp(&p->image_params);

    int eq_caps = MP_CSP_EQ_CAPS_GAMMA;
    if (p->image_params.color.space != MP_CSP_BT_2020_C)
        eq_caps |= MP_CSP_EQ_CAPS_COLORMATRIX;
    if (p->image_desc.flags & MP_IMGFLAG_XYZ)
        eq_caps |= MP_CSP_EQ_CAPS_BRIGHTNESS;
    p->video_eq.capabilities = eq_caps;

    av_lfg_init(&p->lfg, 1);

    debug_check_gl(p, "before video texture creation");

    if (!p->hwdec_active) {
        struct video_image *vimg = &p->image;

        GLenum gl_target =
            p->opts.use_rectangle ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

        struct mp_image layout = {0};
        mp_image_set_params(&layout, &p->image_params);

        for (int n = 0; n < p->plane_count; n++) {
            struct texplane *plane = &vimg->planes[n];

            plane->gl_target = gl_target;

            plane->w = mp_image_plane_w(&layout, n);
            plane->h = mp_image_plane_h(&layout, n);
            plane->tex_w = plane->w + p->opts.tex_pad_x;
            plane->tex_h = plane->h + p->opts.tex_pad_y;

            gl->GenTextures(1, &plane->gl_texture);
            gl->BindTexture(gl_target, plane->gl_texture);

            gl->TexImage2D(gl_target, 0, plane->gl_internal_format,
                           plane->tex_w, plane->tex_h, 0,
                           plane->gl_format, plane->gl_type, NULL);

            int filter = plane->use_integer ? GL_NEAREST : GL_LINEAR;
            gl->TexParameteri(gl_target, GL_TEXTURE_MIN_FILTER, filter);
            gl->TexParameteri(gl_target, GL_TEXTURE_MAG_FILTER, filter);
            gl->TexParameteri(gl_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl->TexParameteri(gl_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            gl->BindTexture(gl_target, 0);

            MP_VERBOSE(p, "Texture for plane %d: %dx%d\n", n,
                       plane->tex_w, plane->tex_h);
        }
    }

    debug_check_gl(p, "after video texture creation");

    gl_video_setup_hooks(p);
}

// Release any texture mappings associated with the current frame.
static void unmap_current_image(struct gl_video *p)
{
    struct video_image *vimg = &p->image;

    if (vimg->hwdec_mapped) {
        assert(p->hwdec_active);
        if (p->hwdec->driver->unmap)
            p->hwdec->driver->unmap(p->hwdec);
        memset(vimg->planes, 0, sizeof(vimg->planes));
        vimg->hwdec_mapped = false;
    }
}

static void unref_current_image(struct gl_video *p)
{
    unmap_current_image(p);
    mp_image_unrefp(&p->image.mpi);
}

static void uninit_video(struct gl_video *p)
{
    GL *gl = p->gl;

    uninit_rendering(p);

    struct video_image *vimg = &p->image;

    unref_current_image(p);

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];

        gl->DeleteTextures(1, &plane->gl_texture);
        gl_pbo_upload_uninit(&plane->pbo);
    }
    *vimg = (struct video_image){0};

    // Invalidate image_params to ensure that gl_video_config() will call
    // init_video() on uninitialized gl_video.
    p->real_image_params = (struct mp_image_params){0};
    p->image_params = p->real_image_params;
    p->hwdec_active = false;
}

static void pass_prepare_src_tex(struct gl_video *p)
{
    struct gl_shader_cache *sc = p->sc;

    for (int n = 0; n < p->pass_tex_num; n++) {
        struct img_tex *s = &p->pass_tex[n];
        if (!s->gl_tex)
            continue;

        char texture_name[32];
        char texture_size[32];
        char texture_rot[32];
        char pixel_size[32];
        snprintf(texture_name, sizeof(texture_name), "texture%d", n);
        snprintf(texture_size, sizeof(texture_size), "texture_size%d", n);
        snprintf(texture_rot, sizeof(texture_rot), "texture_rot%d", n);
        snprintf(pixel_size, sizeof(pixel_size), "pixel_size%d", n);

        if (s->use_integer) {
            gl_sc_uniform_tex_ui(sc, texture_name, s->gl_tex);
        } else {
            gl_sc_uniform_tex(sc, texture_name, s->gl_target, s->gl_tex);
        }
        float f[2] = {1, 1};
        if (s->gl_target != GL_TEXTURE_RECTANGLE) {
            f[0] = s->tex_w;
            f[1] = s->tex_h;
        }
        gl_sc_uniform_vec2(sc, texture_size, f);
        gl_sc_uniform_mat2(sc, texture_rot, true, (float *)s->transform.m);
        gl_sc_uniform_vec2(sc, pixel_size, (GLfloat[]){1.0f / f[0],
                                                       1.0f / f[1]});
    }
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

static void finish_pass_direct(struct gl_video *p, GLint fbo, int vp_w, int vp_h,
                               const struct mp_rect *dst)
{
    GL *gl = p->gl;
    pass_prepare_src_tex(p);
    gl_sc_generate(p->sc);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    render_pass_quad(p, vp_w, vp_h, dst);
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl_sc_reset(p->sc);
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

// Copy a texture to the vec4 color, while increasing offset. Also applies
// the texture multiplier to the sampled color
static void copy_img_tex(struct gl_video *p, int *offset, struct img_tex img)
{
    int count = img.components;
    assert(*offset + count <= 4);

    int id = pass_bind(p, img);
    char src[5] = {0};
    char dst[5] = {0};
    const char *tex_fmt = img.swizzle[0] ? img.swizzle : "rgba";
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

static void hook_prelude(struct gl_video *p, const char *name, int id,
                         struct img_tex tex)
{
    GLSLHF("#define %s_raw texture%d\n", name, id);
    GLSLHF("#define %s_pos texcoord%d\n", name, id);
    GLSLHF("#define %s_size texture_size%d\n", name, id);
    GLSLHF("#define %s_rot texture_rot%d\n", name, id);
    GLSLHF("#define %s_pt pixel_size%d\n", name, id);

    // Set up the sampling functions
    GLSLHF("#define %s_tex(pos) (%f * vec4(texture(%s_raw, pos)).%s)\n",
           name, tex.multiplier, name, tex.swizzle[0] ? tex.swizzle : "rgba");

    // Since the extra matrix multiplication impacts performance,
    // skip it unless the texture was actually rotated
    if (gl_transform_eq(tex.transform, identity_trans)) {
        GLSLHF("#define %s_texOff(off) %s_tex(%s_pos + %s_pt * vec2(off))\n",
               name, name, name, name);
    } else {
        GLSLHF("#define %s_texOff(off) "
                   "%s_tex(%s_pos + %s_rot * vec2(off)/%s_size)\n",
               name, name, name, name, name);
    }
}

static bool saved_tex_find(struct gl_video *p, const char *name,
                           struct img_tex *out)
{
    if (!name || !out)
        return false;

    for (int i = 0; i < p->saved_tex_num; i++) {
        if (strcmp(p->saved_tex[i].name, name) == 0) {
            *out = p->saved_tex[i].tex;
            return true;
        }
    }

    return false;
}

static void saved_tex_store(struct gl_video *p, const char *name,
                            struct img_tex tex)
{
    assert(name);

    for (int i = 0; i < p->saved_tex_num; i++) {
        if (strcmp(p->saved_tex[i].name, name) == 0) {
            p->saved_tex[i].tex = tex;
            return;
        }
    }

    assert(p->saved_tex_num < MAX_SAVED_TEXTURES);
    p->saved_tex[p->saved_tex_num++] = (struct saved_tex) {
        .name = name,
        .tex = tex
    };
}

// Process hooks for a plane, saving the result and returning a new img_tex
// If 'trans' is NULL, the shader is forbidden from transforming tex
static struct img_tex pass_hook(struct gl_video *p, const char *name,
                                struct img_tex tex, struct gl_transform *trans)
{
    if (!name)
        return tex;

    saved_tex_store(p, name, tex);

    MP_DBG(p, "Running hooks for %s\n", name);
    for (int i = 0; i < p->tex_hook_num; i++) {
        struct tex_hook *hook = &p->tex_hooks[i];

        if (strcmp(hook->hook_tex, name) != 0)
            continue;

        // Check the hook's condition
        if (hook->cond && !hook->cond(p, tex, hook->priv)) {
            MP_DBG(p, "Skipping hook on %s due to condition.\n", name);
            continue;
        }

        // Bind all necessary textures and add them to the prelude
        for (int t = 0; t < TEXUNIT_VIDEO_NUM; t++) {
            const char *bind_name = hook->bind_tex[t];
            struct img_tex bind_tex;

            if (!bind_name)
                continue;

            // This is a special name that means "currently hooked texture"
            if (strcmp(bind_name, "HOOKED") == 0) {
                int id = pass_bind(p, tex);
                hook_prelude(p, "HOOKED", id, tex);
                hook_prelude(p, name, id, tex);
                continue;
            }

            if (!saved_tex_find(p, bind_name, &bind_tex)) {
                // Clean up texture bindings and move on to the next hook
                MP_DBG(p, "Skipping hook on %s due to no texture named %s.\n",
                       name, bind_name);
                p->pass_tex_num -= t;
                goto next_hook;
            }

            hook_prelude(p, bind_name, pass_bind(p, bind_tex), bind_tex);
        }

        // Run the actual hook. This generates a series of GLSL shader
        // instructions sufficient for drawing the hook's output
        struct gl_transform hook_off = identity_trans;
        hook->hook(p, tex, &hook_off, hook->priv);

        int comps = hook->components ? hook->components : tex.components;
        skip_unused(p, comps);

        // Compute the updated FBO dimensions and store the result
        struct mp_rect_f sz = {0, 0, tex.w, tex.h};
        gl_transform_rect(hook_off, &sz);
        int w = lroundf(fabs(sz.x1 - sz.x0));
        int h = lroundf(fabs(sz.y1 - sz.y0));

        assert(p->hook_fbo_num < MAX_SAVED_TEXTURES);
        struct fbotex *fbo = &p->hook_fbos[p->hook_fbo_num++];
        finish_pass_fbo(p, fbo, w, h, 0);

        const char *store_name = hook->save_tex ? hook->save_tex : name;
        struct img_tex saved_tex = img_tex_fbo(fbo, tex.type, comps);

        // If the texture we're saving overwrites the "current" texture, also
        // update the tex parameter so that the future loop cycles will use the
        // updated values, and export the offset
        if (strcmp(store_name, name) == 0) {
            if (!trans && !gl_transform_eq(hook_off, identity_trans)) {
                MP_ERR(p, "Hook tried changing size of unscalable texture %s!\n",
                       name);
                return tex;
            }

            tex = saved_tex;
            if (trans)
                gl_transform_trans(hook_off, trans);
        }

        saved_tex_store(p, store_name, saved_tex);

next_hook: ;
    }

    return tex;
}

// This can be used at any time in the middle of rendering to specify an
// optional hook point, which if triggered will render out to a new FBO and
// load the result back into vec4 color. Offsets applied by the hooks are
// accumulated in tex_trans, and the FBO is dimensioned according
// to p->texture_w/h
static void pass_opt_hook_point(struct gl_video *p, const char *name,
                                struct gl_transform *tex_trans)
{
    if (!name)
        return;

    for (int i = 0; i < p->tex_hook_num; i++) {
        struct tex_hook *hook = &p->tex_hooks[i];

        if (strcmp(hook->hook_tex, name) == 0)
            goto found;

        for (int b = 0; b < TEXUNIT_VIDEO_NUM; b++) {
            if (hook->bind_tex[b] && strcmp(hook->bind_tex[b], name) == 0)
                goto found;
        }
    }

    // Nothing uses this texture, don't bother storing it
    return;

found:
    assert(p->hook_fbo_num < MAX_SAVED_TEXTURES);
    struct fbotex *fbo = &p->hook_fbos[p->hook_fbo_num++];
    finish_pass_fbo(p, fbo, p->texture_w, p->texture_h, 0);

    struct img_tex img = img_tex_fbo(fbo, PLANE_RGB, p->components);
    img = pass_hook(p, name, img, tex_trans);
    copy_img_tex(p, &(int){0}, img);
    p->texture_w = img.w;
    p->texture_h = img.h;
    p->components = img.components;
}

static void load_shader(struct gl_video *p, struct bstr body)
{
    gl_sc_hadd_bstr(p->sc, body);
    gl_sc_uniform_f(p->sc, "random", (double)av_lfg_get(&p->lfg) / UINT32_MAX);
    gl_sc_uniform_f(p->sc, "frame", p->frames_uploaded);
    gl_sc_uniform_vec2(p->sc, "image_size", (GLfloat[]){p->image_params.w,
                                                        p->image_params.h});
    gl_sc_uniform_vec2(p->sc, "target_size",
                       (GLfloat[]){p->dst_rect.x1 - p->dst_rect.x0,
                                   p->dst_rect.y1 - p->dst_rect.y0});
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
    bool is_tscale = scaler->index == SCALER_TSCALE;
    scaler->conf.kernel.name = (char *)handle_scaler_opt(conf->kernel.name, is_tscale);
    scaler->conf.window.name = (char *)handle_scaler_opt(conf->window.name, is_tscale);
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
    const struct gl_format *fmt = gl_find_float16_format(gl, elems_per_pixel);
    GLenum target = scaler->gl_target;

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

    gl->BindTexture(target, 0);

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
    src = img_tex_fbo(&scaler->sep_fbo, src.type, src.components);
    src.transform = t_x;
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

// Returns true if two img_texs are semantically equivalent (same metadata)
static bool img_tex_equiv(struct img_tex a, struct img_tex b)
{
    return a.type == b.type &&
           a.components == b.components &&
           a.multiplier == b.multiplier &&
           a.gl_target == b.gl_target &&
           a.use_integer == b.use_integer &&
           a.tex_w == b.tex_w &&
           a.tex_h == b.tex_h &&
           a.w == b.w &&
           a.h == b.h &&
           gl_transform_eq(a.transform, b.transform) &&
           strcmp(a.swizzle, b.swizzle) == 0;
}

static void pass_add_hook(struct gl_video *p, struct tex_hook hook)
{
    if (p->tex_hook_num < MAX_TEXTURE_HOOKS) {
        p->tex_hooks[p->tex_hook_num++] = hook;
    } else {
        MP_ERR(p, "Too many hooks! Limit is %d.\n", MAX_TEXTURE_HOOKS);

        if (hook.free)
            hook.free(&hook);
    }
}

// Adds a hook multiple times, one per name. The last name must be NULL to
// signal the end of the argument list.
#define HOOKS(...) ((char*[]){__VA_ARGS__, NULL})
static void pass_add_hooks(struct gl_video *p, struct tex_hook hook,
                           char **names)
{
    for (int i = 0; names[i] != NULL; i++) {
        hook.hook_tex = names[i];
        pass_add_hook(p, hook);
    }
}

static void deband_hook(struct gl_video *p, struct img_tex tex,
                        struct gl_transform *trans, void *priv)
{
    pass_sample_deband(p->sc, p->opts.deband_opts, &p->lfg);
}

static void unsharp_hook(struct gl_video *p, struct img_tex tex,
                         struct gl_transform *trans, void *priv)
{
    GLSLF("#define tex HOOKED\n");
    GLSLF("#define pos HOOKED_pos\n");
    GLSLF("#define pt HOOKED_pt\n");
    pass_sample_unsharp(p->sc, p->opts.unsharp);
}

struct szexp_ctx {
    struct gl_video *p;
    struct img_tex tex;
};

static bool szexp_lookup(void *priv, struct bstr var, float size[2])
{
    struct szexp_ctx *ctx = priv;
    struct gl_video *p = ctx->p;

    // The size of OUTPUT is determined. It could be useful for certain
    // user shaders to skip passes.
    if (bstr_equals0(var, "OUTPUT")) {
        size[0] = p->dst_rect.x1 - p->dst_rect.x0;
        size[1] = p->dst_rect.y1 - p->dst_rect.y0;
        return true;
    }

    // HOOKED is a special case
    if (bstr_equals0(var, "HOOKED")) {
        size[0] = ctx->tex.w;
        size[1] = ctx->tex.h;
        return true;
    }

    for (int o = 0; o < p->saved_tex_num; o++) {
        if (bstr_equals0(var, p->saved_tex[o].name)) {
            size[0] = p->saved_tex[o].tex.w;
            size[1] = p->saved_tex[o].tex.h;
            return true;
        }
    }

    return false;
}

static bool user_hook_cond(struct gl_video *p, struct img_tex tex, void *priv)
{
    struct gl_user_shader *shader = priv;
    assert(shader);

    float res = false;
    eval_szexpr(p->log, &(struct szexp_ctx){p, tex}, szexp_lookup, shader->cond, &res);
    return res;
}

static void user_hook(struct gl_video *p, struct img_tex tex,
                      struct gl_transform *trans, void *priv)
{
    struct gl_user_shader *shader = priv;
    assert(shader);

    load_shader(p, shader->pass_body);
    GLSLF("// custom hook\n");
    GLSLF("color = hook();\n");

    // Make sure we at least create a legal FBO on failure, since it's better
    // to do this and display an error message than just crash OpenGL
    float w = 1.0, h = 1.0;

    eval_szexpr(p->log, &(struct szexp_ctx){p, tex}, szexp_lookup, shader->width, &w);
    eval_szexpr(p->log, &(struct szexp_ctx){p, tex}, szexp_lookup, shader->height, &h);

    *trans = (struct gl_transform){{{w / tex.w, 0}, {0, h / tex.h}}};
    gl_transform_trans(shader->offset, trans);
}

static void user_hook_free(struct tex_hook *hook)
{
    talloc_free(hook->hook_tex);
    talloc_free(hook->save_tex);
    for (int i = 0; i < TEXUNIT_VIDEO_NUM; i++)
        talloc_free(hook->bind_tex[i]);
    talloc_free(hook->priv);
}

static void pass_hook_user_shaders(struct gl_video *p, char **shaders)
{
    if (!shaders)
        return;

    for (int n = 0; shaders[n] != NULL; n++) {
        struct bstr file = load_cached_file(p, shaders[n]);
        struct gl_user_shader out;
        while (parse_user_shader_pass(p->log, &file, &out)) {
            struct tex_hook hook = {
                .components = out.components,
                .hook = user_hook,
                .free = user_hook_free,
                .cond = user_hook_cond,
            };

            for (int i = 0; i < SHADER_MAX_HOOKS; i++) {
                hook.hook_tex = bstrdup0(p, out.hook_tex[i]);
                if (!hook.hook_tex)
                    continue;

                struct gl_user_shader *out_copy = talloc_ptrtype(p, out_copy);
                *out_copy = out;
                hook.priv = out_copy;
                for (int o = 0; o < SHADER_MAX_BINDS; o++)
                    hook.bind_tex[o] = bstrdup0(p, out.bind_tex[o]);
                hook.save_tex = bstrdup0(p, out.save_tex),
                pass_add_hook(p, hook);
            }
        }
    }
}

static void gl_video_setup_hooks(struct gl_video *p)
{
    gl_video_reset_hooks(p);

    if (p->opts.deband) {
        pass_add_hooks(p, (struct tex_hook) {.hook = deband_hook,
                                             .bind_tex = {"HOOKED"}},
                       HOOKS("LUMA", "CHROMA", "RGB", "XYZ"));
    }

    if (p->opts.unsharp != 0.0) {
        pass_add_hook(p, (struct tex_hook) {
            .hook_tex = "MAIN",
            .bind_tex = {"HOOKED"},
            .hook = unsharp_hook,
        });
    }

    pass_hook_user_shaders(p, p->opts.user_shaders);
}

// sample from video textures, set "color" variable to yuv value
static void pass_read_video(struct gl_video *p)
{
    struct img_tex tex[4];
    struct gl_transform offsets[4];
    pass_get_img_tex(p, &p->image, tex, offsets);

    // To keep the code as simple as possibly, we currently run all shader
    // stages even if they would be unnecessary (e.g. no hooks for a texture).
    // In the future, deferred img_tex should optimize this away.

    // Merge semantically identical textures. This loop is done from back
    // to front so that merged textures end up in the right order while
    // simultaneously allowing us to skip unnecessary merges
    for (int n = 3; n >= 0; n--) {
        if (tex[n].type == PLANE_NONE)
            continue;

        int first = n;
        int num = 0;

        for (int i = 0; i < n; i++) {
            if (img_tex_equiv(tex[n], tex[i]) &&
                gl_transform_eq(offsets[n], offsets[i]))
            {
                GLSLF("// merging plane %d ...\n", i);
                copy_img_tex(p, &num, tex[i]);
                first = MPMIN(first, i);
                memset(&tex[i], 0, sizeof(tex[i]));
            }
        }

        if (num > 0) {
            GLSLF("// merging plane %d ... into %d\n", n, first);
            copy_img_tex(p, &num, tex[n]);
            finish_pass_fbo(p, &p->merge_fbo[n], tex[n].w, tex[n].h, 0);
            tex[first] = img_tex_fbo(&p->merge_fbo[n], tex[n].type, num);
            memset(&tex[n], 0, sizeof(tex[n]));
        }
    }

    // If any textures are still in integer format by this point, we need
    // to introduce an explicit conversion pass to avoid breaking hooks/scaling
    for (int n = 0; n < 4; n++) {
        if (tex[n].use_integer) {
            GLSLF("// use_integer fix for plane %d\n", n);

            copy_img_tex(p, &(int){0}, tex[n]);
            finish_pass_fbo(p, &p->integer_fbo[n], tex[n].w, tex[n].h, 0);
            tex[n] = img_tex_fbo(&p->integer_fbo[n], tex[n].type,
                                 tex[n].components);
        }
    }

    // Dispatch the hooks for all of these textures, saving and perhaps
    // modifying them in the process
    for (int n = 0; n < 4; n++) {
        const char *name;
        switch (tex[n].type) {
        case PLANE_RGB:    name = "RGB";    break;
        case PLANE_LUMA:   name = "LUMA";   break;
        case PLANE_CHROMA: name = "CHROMA"; break;
        case PLANE_ALPHA:  name = "ALPHA";  break;
        case PLANE_XYZ:    name = "XYZ";    break;
        default: continue;
        }

        tex[n] = pass_hook(p, name, tex[n], &offsets[n]);
    }

    // At this point all planes are finalized but they may not be at the
    // required size yet. Furthermore, they may have texture offsets that
    // require realignment. For lack of something better to do, we assume
    // the rgb/luma texture is the "reference" and scale everything else
    // to match.
    for (int n = 0; n < 4; n++) {
        switch (tex[n].type) {
        case PLANE_RGB:
        case PLANE_XYZ:
        case PLANE_LUMA: break;
        default: continue;
        }

        p->texture_w = tex[n].w;
        p->texture_h = tex[n].h;
        p->texture_offset = offsets[n];
        break;
    }

    // Compute the reference rect
    struct mp_rect_f src = {0.0, 0.0, p->image_params.w, p->image_params.h};
    struct mp_rect_f ref = src;
    gl_transform_rect(p->texture_offset, &ref);
    MP_DBG(p, "ref rect: {%f %f} {%f %f}\n", ref.x0, ref.y0, ref.x1, ref.y1);

    // Explicitly scale all of the textures that don't match
    for (int n = 0; n < 4; n++) {
        if (tex[n].type == PLANE_NONE)
            continue;

        // If the planes are aligned identically, we will end up with the
        // exact same source rectangle.
        struct mp_rect_f rect = src;
        gl_transform_rect(offsets[n], &rect);
        MP_DBG(p, "rect[%d]: {%f %f} {%f %f}\n", n,
               rect.x0, rect.y0, rect.x1, rect.y1);

        if (mp_rect_f_seq(ref, rect))
            continue;

        // If the rectangles differ, then our planes have a different
        // alignment and/or size. First of all, we have to compute the
        // corrections required to meet the target rectangle
        struct gl_transform fix = {
            .m = {{(ref.x1 - ref.x0) / (rect.x1 - rect.x0), 0.0},
                  {0.0, (ref.y1 - ref.y0) / (rect.y1 - rect.y0)}},
            .t = {ref.x0, ref.y0},
        };

        // Since the scale in texture space is different from the scale in
        // absolute terms, we have to scale the coefficients down to be
        // relative to the texture's physical dimensions and local offset
        struct gl_transform scale = {
            .m = {{(float)tex[n].w / p->texture_w, 0.0},
                  {0.0, (float)tex[n].h / p->texture_h}},
            .t = {-rect.x0, -rect.y0},
        };
        gl_transform_trans(scale, &fix);
        MP_DBG(p, "-> fix[%d] = {%f %f} + off {%f %f}\n", n,
               fix.m[0][0], fix.m[1][1], fix.t[0], fix.t[1]);

        // Since the texture transform is a function of the texture coordinates
        // to texture space, rather than the other way around, we have to
        // actually apply the *inverse* of this. Fortunately, calculating
        // the inverse is relatively easy here.
        fix.m[0][0] = 1.0 / fix.m[0][0];
        fix.m[1][1] = 1.0 / fix.m[1][1];
        fix.t[0] = fix.m[0][0] * -fix.t[0];
        fix.t[1] = fix.m[1][1] * -fix.t[1];
        gl_transform_trans(fix, &tex[n].transform);

        int scaler_id = -1;
        const char *name = NULL;
        switch (tex[n].type) {
        case PLANE_RGB:
        case PLANE_LUMA:
        case PLANE_XYZ:
            scaler_id = SCALER_SCALE;
            // these aren't worth hooking, fringe hypothetical cases only
            break;
        case PLANE_CHROMA:
            scaler_id = SCALER_CSCALE;
            name = "CHROMA_SCALED";
            break;
        case PLANE_ALPHA:
            // alpha always uses bilinear
            name = "ALPHA_SCALED";
        }

        if (scaler_id < 0)
            continue;

        const struct scaler_config *conf = &p->opts.scaler[scaler_id];
        struct scaler *scaler = &p->scaler[scaler_id];

        // bilinear scaling is a free no-op thanks to GPU sampling
        if (strcmp(conf->kernel.name, "bilinear") != 0) {
            GLSLF("// upscaling plane %d\n", n);
            pass_sample(p, tex[n], scaler, conf, 1.0, p->texture_w, p->texture_h);
            finish_pass_fbo(p, &p->scale_fbo[n], p->texture_w, p->texture_h,
                            FBOTEX_FUZZY);
            tex[n] = img_tex_fbo(&p->scale_fbo[n], tex[n].type, tex[n].components);
        }

        // Run any post-scaling hooks
        tex[n] = pass_hook(p, name, tex[n], NULL);
    }

    // All planes are of the same size and properly aligned at this point
    GLSLF("// combining planes\n");
    int coord = 0;
    for (int i = 0; i < 4; i++) {
        if (tex[i].type != PLANE_NONE)
            copy_img_tex(p, &coord, tex[i]);
    }
    p->components = coord;
}

// Utility function that simply binds an FBO and reads from it, without any
// transformations. Returns the ID of the texture unit it was bound to
static int pass_read_fbo(struct gl_video *p, struct fbotex *fbo)
{
    struct img_tex tex = img_tex_fbo(fbo, PLANE_RGB, p->components);
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
    if (cparams.color.space == MP_CSP_XYZ)
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

    if (p->image_params.color.space == MP_CSP_BT_2020_C) {
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
    if (!p->has_alpha || p->opts.alpha_mode == ALPHA_NO) {
        GLSL(color.a = 1.0;)
    } else if (p->opts.alpha_mode == ALPHA_BLEND) {
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
        // For scaler-resizes-only, we round the texture offset to
        // the nearest round value in order to prevent ugly blurriness
        // (in exchange for slightly shifting the image by up to half a
        // subpixel)
        p->texture_offset.t[0] = roundf(p->texture_offset.t[0]);
        p->texture_offset.t[1] = roundf(p->texture_offset.t[1]);
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
    if (p->use_linear) {
        pass_linearize(p->sc, p->image_params.color.gamma);
        pass_opt_hook_point(p, "LINEAR", NULL);
    }

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
        pass_opt_hook_point(p, "SIGMOID", NULL);
    }

    pass_opt_hook_point(p, "PREKERNEL", NULL);

    int vp_w = p->dst_rect.x1 - p->dst_rect.x0;
    int vp_h = p->dst_rect.y1 - p->dst_rect.y0;
    struct gl_transform transform;
    compute_src_transform(p, &transform);

    GLSLF("// main scaling\n");
    finish_pass_fbo(p, &p->indirect_fbo, p->texture_w, p->texture_h, 0);
    struct img_tex src = img_tex_fbo(&p->indirect_fbo, PLANE_RGB, p->components);
    gl_transform_trans(transform, &src.transform);
    pass_sample(p, src, scaler, &scaler_conf, scale_factor, vp_w, vp_h);

    // Changes the texture size to display size after main scaler.
    p->texture_w = vp_w;
    p->texture_h = vp_h;

    pass_opt_hook_point(p, "POSTKERNEL", NULL);

    GLSLF("// scaler post-conversion\n");
    if (use_sigmoid) {
        // Inverse of the transformation above
        GLSLF("color.rgb = (1.0/(1.0 + exp(%f * (%f - color.rgb))) - %f) / %f;\n",
                sig_slope, sig_center, sig_offset, sig_scale);
    }
}

// Adapts the colors to the right output color space. (Final pass during
// rendering)
// If OSD is true, ignore any changes that may have been made to the video
// by previous passes (i.e. linear scaling)
static void pass_colormanage(struct gl_video *p, struct mp_colorspace src, bool osd)
{
    struct mp_colorspace ref = src;

    if (p->use_linear && !osd)
        src.gamma = MP_CSP_TRC_LINEAR;

    // Figure out the target color space from the options, or auto-guess if
    // none were set
    struct mp_colorspace dst = {
        .gamma = p->opts.target_trc,
        .primaries = p->opts.target_prim,
        .nom_peak = mp_csp_trc_nom_peak(p->opts.target_trc, p->opts.target_brightness),
    };

    if (p->use_lut_3d) {
        // The 3DLUT is always generated against the video's original source
        // space, *not* the reference space. (To avoid having to regenerate
        // the 3DLUT for the OSD on every frame)
        enum mp_csp_prim prim_orig = p->image_params.color.primaries;
        enum mp_csp_trc trc_orig = p->image_params.color.gamma;

        // One exception: HDR is not implemented by LittleCMS for technical
        // limitation reasons, so we use a gamma 2.2 input curve here instead.
        // We could pick any value we want here, the difference is just coding
        // efficiency.
        if (trc_orig == MP_CSP_TRC_SMPTE_ST2084 ||
            trc_orig == MP_CSP_TRC_ARIB_STD_B67 ||
            trc_orig == MP_CSP_TRC_V_LOG)
        {
            trc_orig = MP_CSP_TRC_GAMMA22;
        }

        if (gl_video_get_lut3d(p, prim_orig, trc_orig)) {
            dst.primaries = prim_orig;
            dst.gamma = trc_orig;
        }
    }

    if (dst.primaries == MP_CSP_PRIM_AUTO) {
        // The vast majority of people are on sRGB or BT.709 displays, so pick
        // this as the default output color space.
        dst.primaries = MP_CSP_PRIM_BT_709;

        if (ref.primaries == MP_CSP_PRIM_BT_601_525 ||
            ref.primaries == MP_CSP_PRIM_BT_601_625)
        {
            // Since we auto-pick BT.601 and BT.709 based on the dimensions,
            // combined with the fact that they're very similar to begin with,
            // and to avoid confusing the average user, just don't adapt BT.601
            // content automatically at all.
            dst.primaries = ref.primaries;
        }
    }

    if (dst.gamma == MP_CSP_TRC_AUTO) {
        // Most people seem to complain when the image is darker or brighter
        // than what they're "used to", so just avoid changing the gamma
        // altogether by default. The only exceptions to this rule apply to
        // very unusual TRCs, which even hardcode technoluddites would probably
        // not enjoy viewing unaltered.
        dst.gamma = ref.gamma;

        // Avoid outputting linear light or HDR content "by default". For these
        // just pick gamma 2.2 as a default, since it's a good estimate for
        // the response of typical displays
        if (dst.gamma == MP_CSP_TRC_LINEAR || mp_trc_is_hdr(dst.gamma))
            dst.gamma = MP_CSP_TRC_GAMMA22;
    }

    // For the src peaks, the correct brightness metadata may be present for
    // sig_peak, nom_peak, both, or neither. To handle everything in a generic
    // way, it's important to never automatically infer a sig_peak that is
    // below the nom_peak (since we don't know what bits the image contains,
    // doing so would potentially badly clip). The only time in which this
    // may be the case is when the mastering metadata explicitly says so, i.e.
    // the sig_peak was already set. So to simplify the logic as much as
    // possible, make sure the nom_peak is present and correct first, and just
    // set sig_peak = nom_peak if missing.
    if (!src.nom_peak) {
        // For display-referred colorspaces, we treat it as relative to
        // target_brightness
        src.nom_peak = mp_csp_trc_nom_peak(src.gamma, p->opts.target_brightness);
    }

    if (!src.sig_peak)
        src.sig_peak = src.nom_peak;

    MP_DBG(p, "HDR src nom: %f sig: %f, dst: %f\n",
           src.nom_peak, src.sig_peak, dst.nom_peak);

    // Adapt from src to dst as necessary
    pass_color_map(p->sc, src, dst, p->opts.hdr_tone_mapping,
                   p->opts.tone_mapping_param);

    if (p->use_lut_3d) {
        gl_sc_uniform_tex(p->sc, "lut_3d", GL_TEXTURE_3D, p->lut_3d_texture);
        GLSL(vec3 cpos;)
        for (int i = 0; i < 3; i++)
            GLSLF("cpos[%d] = LUT_POS(color[%d], %d.0);\n", i, i, p->lut_3d_size[i]);
        GLSL(color.rgb = texture3D(lut_3d, cpos).rgb;)
    }
}

static void pass_dither(struct gl_video *p)
{
    GL *gl = p->gl;

    // Assume 8 bits per component if unknown.
    int dst_depth = p->fb_depth;
    if (p->opts.dither_depth > 0)
        dst_depth = p->opts.dither_depth;

    if (p->opts.dither_depth < 0 || p->opts.dither_algo == DITHER_NONE)
        return;

    if (!p->dither_texture) {
        MP_VERBOSE(p, "Dither to %d.\n", dst_depth);

        int tex_size;
        void *tex_data;
        GLint tex_iformat = 0;
        GLint tex_format = 0;
        GLenum tex_type;
        unsigned char temp[256];

        if (p->opts.dither_algo == DITHER_FRUIT) {
            int sizeb = p->opts.dither_size;
            int size = 1 << sizeb;

            if (p->last_dither_matrix_size != size) {
                p->last_dither_matrix = talloc_realloc(p, p->last_dither_matrix,
                                                       float, size * size);
                mp_make_fruit_dither_matrix(p->last_dither_matrix, sizeb);
                p->last_dither_matrix_size = size;
            }

            // Prefer R16 texture since they provide higher precision.
            const struct gl_format *fmt = gl_find_unorm_format(gl, 2, 1);
            if (!fmt || gl->es)
                fmt = gl_find_float16_format(gl, 1);
            tex_size = size;
            if (fmt) {
                tex_iformat = fmt->internal_format;
                tex_format = fmt->format;
            }
            tex_type = GL_FLOAT;
            tex_data = p->last_dither_matrix;
        } else {
            assert(sizeof(temp) >= 8 * 8);
            mp_make_ordered_dither_matrix(temp, 8);

            const struct gl_format *fmt = gl_find_unorm_format(gl, 1, 1);
            tex_size = 8;
            tex_iformat = fmt->internal_format;
            tex_format = fmt->format;
            tex_type = fmt->type;
            tex_data = temp;
        }

        p->dither_size = tex_size;

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
        gl->BindTexture(GL_TEXTURE_2D, 0);

        debug_check_gl(p, "dither setup");
    }

    GLSLF("// dithering\n");

    // This defines how many bits are considered significant for output on
    // screen. The superfluous bits will be used for rounding according to the
    // dither matrix. The precision of the source implicitly decides how many
    // dither patterns can be visible.
    int dither_quantization = (1 << dst_depth) - 1;

    gl_sc_uniform_tex(p->sc, "dither", GL_TEXTURE_2D, p->dither_texture);

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
        // When subtitles need to be color managed, assume they're in sRGB
        // (for lack of anything saner to do)
        if (cms) {
            static const struct mp_colorspace csp_srgb = {
                .primaries = MP_CSP_PRIM_BT_709,
                .gamma = MP_CSP_TRC_SRGB,
            };

            pass_colormanage(p, csp_srgb, true);
        }
        gl_sc_set_vao(p->sc, mpgl_osd_get_vao(p->osd));
        gl_sc_generate(p->sc);
        mpgl_osd_draw_part(p->osd, vp_w, vp_h, n);
        gl_sc_reset(p->sc);
    }
    gl_sc_set_vao(p->sc, &p->vao);
}

static float chroma_realign(int size, int shift)
{
    return size / (float)(mp_chroma_div_up(size, shift) << shift);
}

// Minimal rendering code path, for GLES or OpenGL 2.1 without proper FBOs.
static void pass_render_frame_dumb(struct gl_video *p, int fbo)
{
    p->gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);

    struct img_tex tex[4];
    struct gl_transform off[4];
    pass_get_img_tex(p, &p->image, tex, off);

    struct gl_transform transform;
    compute_src_transform(p, &transform);

    int index = 0;
    for (int i = 0; i < p->plane_count; i++) {
        int xs = p->image_desc.xs[i];
        int ys = p->image_desc.ys[i];
        if (p->image_params.rotate % 180 == 90)
            MPSWAP(int, xs, ys);

        struct gl_transform t = transform;
        t.m[0][0] *= chroma_realign(p->texture_w, xs);
        t.m[1][1] *= chroma_realign(p->texture_h, ys);

        t.t[0] /= 1 << xs;
        t.t[1] /= 1 << ys;

        t.t[0] += off[i].t[0];
        t.t[1] += off[i].t[1];

        gl_transform_trans(tex[i].transform, &t);
        tex[i].transform = t;

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
    p->saved_tex_num = 0;
    p->hook_fbo_num = 0;
    p->use_linear = false;

    if (p->image_params.rotate % 180 == 90)
        MPSWAP(int, p->texture_w, p->texture_h);

    if (p->dumb_mode)
        return;

    // start the render timer here. it will continue to the end of this
    // function, to render the time needed to draw (excluding screen
    // presentation)
    gl_timer_start(p->render_timer);

    p->use_linear = p->opts.linear_scaling || p->opts.sigmoid_upscaling;
    pass_read_video(p);
    pass_opt_hook_point(p, "NATIVE", &p->texture_offset);
    pass_convert_yuv(p);
    pass_opt_hook_point(p, "MAINPRESUB", &p->texture_offset);

    // For subtitles
    double vpts = p->image.mpi->pts;
    if (vpts == MP_NOPTS_VALUE)
        vpts = p->osd_pts;

    if (p->osd && p->opts.blend_subs == BLEND_SUBS_VIDEO) {
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
    pass_opt_hook_point(p, "MAIN", &p->texture_offset);

    pass_scale_main(p);

    int vp_w = p->dst_rect.x1 - p->dst_rect.x0,
        vp_h = p->dst_rect.y1 - p->dst_rect.y0;
    if (p->osd && p->opts.blend_subs == BLEND_SUBS_YES) {
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
        if (p->use_linear) {
            pass_delinearize(p->sc, p->image_params.color.gamma);
            p->use_linear = false;
        }
        finish_pass_fbo(p, &p->blend_subs_fbo, p->texture_w, p->texture_h,
                        FBOTEX_FUZZY);
        pass_draw_osd(p, OSD_DRAW_SUB_ONLY, vpts, rect,
                      p->texture_w, p->texture_h, p->blend_subs_fbo.fbo, false);
        pass_read_fbo(p, &p->blend_subs_fbo);
    }

    pass_opt_hook_point(p, "SCALED", NULL);

    gl_timer_stop(p->render_timer);
}

static void pass_draw_to_screen(struct gl_video *p, int fbo)
{
    gl_timer_start(p->present_timer);

    if (p->dumb_mode)
        pass_render_frame_dumb(p, fbo);

    // Adjust the overall gamma before drawing to screen
    if (p->user_gamma != 1) {
        gl_sc_uniform_f(p->sc, "user_gamma", p->user_gamma);
        GLSL(color.rgb = clamp(color.rgb, 0.0, 1.0);)
        GLSL(color.rgb = pow(color.rgb, vec3(user_gamma));)
    }

    pass_colormanage(p, p->image_params.color, false);

    // Draw checkerboard pattern to indicate transparency
    if (p->has_alpha && p->opts.alpha_mode == ALPHA_BLEND_TILES) {
        GLSLF("// transparency checkerboard\n");
        GLSL(bvec2 tile = lessThan(fract(gl_FragCoord.xy / 32.0), vec2(0.5));)
        GLSL(vec3 background = vec3(tile.x == tile.y ? 1.0 : 0.75);)
        GLSL(color.rgb = mix(background, color.rgb, color.a);)
    }

    pass_opt_hook_point(p, "OUTPUT", NULL);

    pass_dither(p);
    finish_pass_direct(p, fbo, p->vp_w, p->vp_h, &p->dst_rect);

    gl_timer_stop(p->present_timer);
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

    // First of all, figure out if we have a frame available at all, and draw
    // it manually + reset the queue if not
    if (p->surfaces[p->surface_now].pts == MP_NOPTS_VALUE) {
        if (!gl_video_upload_image(p, t->current))
            return;
        pass_render_frame(p);
        finish_pass_fbo(p, &p->surfaces[p->surface_now].fbotex,
                        vp_w, vp_h, FBOTEX_FUZZY);
        p->surfaces[p->surface_now].pts = p->image.mpi->pts;
        p->surface_idx = p->surface_now;
    }

    // Find the right frame for this instant
    if (t->current && t->current->pts != MP_NOPTS_VALUE) {
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
    bool linear = strcmp(tscale->conf.kernel.name, "linear") == 0;
    int size;

    if (oversample || linear) {
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
    int surface_dst = fbosurface_wrap(p->surface_idx + 1);
    for (int i = 0; i < t->num_frames; i++) {
        // Avoid overwriting data we might still need
        if (surface_dst == surface_bse - 1)
            break;

        struct mp_image *f = t->frames[i];
        if (!mp_image_params_equal(&f->params, &p->real_image_params) ||
            f->pts == MP_NOPTS_VALUE)
            continue;

        if (f->pts > p->surfaces[p->surface_idx].pts) {
            if (!gl_video_upload_image(p, f))
                return;
            pass_render_frame(p);
            finish_pass_fbo(p, &p->surfaces[surface_dst].fbotex,
                            vp_w, vp_h, FBOTEX_FUZZY);
            p->surfaces[surface_dst].pts = f->pts;
            p->surface_idx = surface_dst;
            surface_dst = fbosurface_wrap(surface_dst + 1);
        }
    }

    // Figure out whether the queue is "valid". A queue is invalid if the
    // frames' PTS is not monotonically increasing. Anything else is invalid,
    // so avoid blending incorrect data and just draw the latest frame as-is.
    // Possible causes for failure of this condition include seeks, pausing,
    // end of playback or start of playback.
    bool valid = true;
    for (int i = surface_bse, ii; valid && i != surface_end; i = ii) {
        ii = fbosurface_wrap(i + 1);
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

        if (oversample) {
            // Oversample uses the frame area as mix ratio, not the the vsync
            // position itself
            double vsync_dist = t->vsync_interval / t->ideal_frame_duration,
                   threshold = tscale->conf.kernel.params[0];
            threshold = isnan(threshold) ? 0.0 : threshold;
            mix = (1 - mix) / vsync_dist;
            mix = mix <= 0 + threshold ? 0 : mix;
            mix = mix >= 1 - threshold ? 1 : mix;
            mix = 1 - mix;
        }

        // Blend the frames together
        if (oversample || linear) {
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
                            PLANE_RGB, p->components);
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

static void timer_dbg(struct gl_video *p, const char *name, struct gl_timer *t)
{
    if (gl_timer_sample_count(t) > 0) {
        MP_DBG(p, "%s time: last %dus avg %dus peak %dus\n", name,
               (int)gl_timer_last_us(t),
               (int)gl_timer_avg_us(t),
               (int)gl_timer_peak_us(t));
    }
}

// (fbo==0 makes BindFramebuffer select the screen backbuffer)
void gl_video_render_frame(struct gl_video *p, struct vo_frame *frame, int fbo)
{
    GL *gl = p->gl;

    if (fbo && !(gl->mpgl_caps & MPGL_CAP_FB)) {
        MP_FATAL(p, "Rendering to FBO requested, but no FBO extension found!\n");
        return;
    }

    p->broken_frame = false;

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);

    bool has_frame = !!frame->current;
    bool is_new = has_frame && !frame->redraw && !frame->repeat;

    if (!has_frame || p->dst_rect.x0 > 0 || p->dst_rect.y0 > 0 ||
        p->dst_rect.x1 < p->vp_w || p->dst_rect.y1 < abs(p->vp_h))
    {
        struct m_color c = p->opts.background;
        gl->ClearColor(c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
        gl->Clear(GL_COLOR_BUFFER_BIT);
    }

    if (p->hwdec_active && p->hwdec->driver->overlay_frame) {
        if (has_frame) {
            float *c = p->hwdec->overlay_colorkey;
            gl->Scissor(p->dst_rect.x0, p->dst_rect.y0,
                        p->dst_rect.x1 - p->dst_rect.x0,
                        p->dst_rect.y1 - p->dst_rect.y0);
            gl->Enable(GL_SCISSOR_TEST);
            gl->ClearColor(c[0], c[1], c[2], c[3]);
            gl->Clear(GL_COLOR_BUFFER_BIT);
            gl->Disable(GL_SCISSOR_TEST);
        }

        if (is_new || !frame->current)
            p->hwdec->driver->overlay_frame(p->hwdec, frame->current);

        if (frame->current)
            p->osd_pts = frame->current->pts;

        // Disable GL rendering
        has_frame = false;
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
            if (is_new || !p->output_fbo_valid) {
                p->output_fbo_valid = false;

                if (!gl_video_upload_image(p, frame->current))
                    goto done;
                pass_render_frame(p);

                // For the non-interpolation case, we draw to a single "cache"
                // FBO to speed up subsequent re-draws (if any exist)
                int dest_fbo = fbo;
                if (frame->num_vsyncs > 1 && frame->display_synced &&
                    !p->dumb_mode && gl->BlitFramebuffer)
                {
                    fbotex_change(&p->output_fbo, p->gl, p->log,
                                  p->vp_w, abs(p->vp_h),
                                  p->opts.fbo_format, FBOTEX_FUZZY);
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

done:

    unmap_current_image(p);

    debug_check_gl(p, "after video rendering");

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);

    if (p->osd) {
        pass_draw_osd(p, p->opts.blend_subs ? OSD_DRAW_OSD_ONLY : 0,
                      p->osd_pts, p->osd_rect, p->vp_w, p->vp_h, fbo, true);
        debug_check_gl(p, "after OSD rendering");
    }
    gl->UseProgram(0);

    if (gl_sc_error_state(p->sc) || p->broken_frame) {
        // Make the screen solid blue to make it visually clear that an
        // error has occurred
        gl->ClearColor(0.0, 0.05, 0.5, 1.0);
        gl->Clear(GL_COLOR_BUFFER_BIT);
    }

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    // The playloop calls this last before waiting some time until it decides
    // to call flip_page(). Tell OpenGL to start execution of the GPU commands
    // while we sleep (this happens asynchronously).
    gl->Flush();

    p->frames_rendered++;

    // Report performance metrics
    timer_dbg(p, "upload", p->upload_timer);
    timer_dbg(p, "render", p->render_timer);
    timer_dbg(p, "present", p->present_timer);
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
    gl_video_setup_hooks(p);

    if (p->osd)
        mpgl_osd_resize(p->osd, p->osd_rect, p->image_params.stereo_out);

    if (p->hwdec && p->hwdec->driver->overlay_adjust)
        p->hwdec->driver->overlay_adjust(p->hwdec, vp_w, abs(vp_h), src, dst);
}

static struct voctrl_performance_entry gl_video_perfentry(struct gl_timer *t)
{
    return (struct voctrl_performance_entry) {
        .last = gl_timer_last_us(t),
        .avg  = gl_timer_avg_us(t),
        .peak = gl_timer_peak_us(t),
    };
}

struct voctrl_performance_data gl_video_perfdata(struct gl_video *p)
{
    return (struct voctrl_performance_data) {
        .upload = gl_video_perfentry(p->upload_timer),
        .render = gl_video_perfentry(p->render_timer),
        .present = gl_video_perfentry(p->present_timer),
    };
}

// This assumes nv12, with textures set to GL_NEAREST filtering.
static void reinterleave_vdpau(struct gl_video *p, struct gl_hwdec_frame *frame)
{
    struct gl_hwdec_frame res = {0};
    for (int n = 0; n < 2; n++) {
        struct fbotex *fbo = &p->vdpau_deinterleave_fbo[n];
        // This is an array of the 2 to-merge planes.
        struct gl_hwdec_plane *src = &frame->planes[n * 2];
        int w = src[0].tex_w;
        int h = src[0].tex_h;
        int ids[2];
        for (int t = 0; t < 2; t++) {
            ids[t] = pass_bind(p, (struct img_tex){
                .gl_tex = src[t].gl_texture,
                .gl_target = src[t].gl_target,
                .multiplier = 1.0,
                .transform = identity_trans,
                .tex_w = w,
                .tex_h = h,
                .w = w,
                .h = h,
            });
        }

        GLSLF("color = fract(gl_FragCoord.y / 2) < 0.5\n");
        GLSLF("      ? texture(texture%d, texcoord%d)\n", ids[0], ids[0]);
        GLSLF("      : texture(texture%d, texcoord%d);", ids[1], ids[1]);

        fbotex_change(fbo, p->gl, p->log, w, h * 2, n == 0 ? GL_R8 : GL_RG8, 0);

        finish_pass_direct(p, fbo->fbo, fbo->rw, fbo->rh,
                           &(struct mp_rect){0, 0, w, h * 2});

        res.planes[n] = (struct gl_hwdec_plane){
            .gl_texture = fbo->texture,
            .gl_target = GL_TEXTURE_2D,
            .tex_w = w,
            .tex_h = h * 2,
        };
    }
    *frame = res;
}

// Returns false on failure.
static bool gl_video_upload_image(struct gl_video *p, struct mp_image *mpi)
{
    GL *gl = p->gl;
    struct video_image *vimg = &p->image;

    unref_current_image(p);

    mpi = mp_image_new_ref(mpi);
    if (!mpi)
        goto error;

    vimg->mpi = mpi;
    p->osd_pts = mpi->pts;
    p->frames_uploaded++;

    if (p->hwdec_active) {
        // Hardware decoding
        struct gl_hwdec_frame gl_frame = {0};
        gl_timer_start(p->upload_timer);
        bool ok = p->hwdec->driver->map_frame(p->hwdec, vimg->mpi, &gl_frame) >= 0;
        gl_timer_stop(p->upload_timer);
        vimg->hwdec_mapped = true;
        if (ok) {
            struct mp_image layout = {0};
            mp_image_set_params(&layout, &p->image_params);
            if (gl_frame.vdpau_fields)
                reinterleave_vdpau(p, &gl_frame);
            for (int n = 0; n < p->plane_count; n++) {
                struct gl_hwdec_plane *plane = &gl_frame.planes[n];
                vimg->planes[n] = (struct texplane){
                    .w = mp_image_plane_w(&layout, n),
                    .h = mp_image_plane_h(&layout, n),
                    .tex_w = plane->tex_w,
                    .tex_h = plane->tex_h,
                    .gl_target = plane->gl_target,
                    .gl_texture = plane->gl_texture,
                };
                snprintf(vimg->planes[n].swizzle, sizeof(vimg->planes[n].swizzle),
                         "%s", plane->swizzle);
            }
        } else {
            MP_FATAL(p, "Mapping hardware decoded surface failed.\n");
            goto error;
        }
        return true;
    }

    // Software decoding
    assert(mpi->num_planes == p->plane_count);

    gl_timer_start(p->upload_timer);


    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];

        plane->flipped = mpi->stride[0] < 0;

        gl->BindTexture(plane->gl_target, plane->gl_texture);
        gl_pbo_upload_tex(&plane->pbo, gl, p->opts.pbo, plane->gl_target,
                          plane->gl_format, plane->gl_type, plane->w, plane->h,
                          mpi->planes[n], mpi->stride[n],
                          0, 0, plane->w, plane->h);
        gl->BindTexture(plane->gl_target, 0);
    }

    gl_timer_stop(p->upload_timer);

    return true;

error:
    unref_current_image(p);
    p->broken_frame = true;
    return false;
}

static bool test_fbo(struct gl_video *p, GLint format)
{
    GL *gl = p->gl;
    bool success = false;
    MP_VERBOSE(p, "Testing FBO format 0x%x\n", (unsigned)format);
    struct fbotex fbo = {0};
    if (fbotex_init(&fbo, p->gl, p->log, 16, 16, format)) {
        gl->BindFramebuffer(GL_FRAMEBUFFER, fbo.fbo);
        gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
        success = true;
    }
    fbotex_uninit(&fbo);
    gl_check_error(gl, p->log, "FBO test");
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
        o->blend_subs || o->deband || o->unsharp)
        return false;
    // check remaining scalers (tscale is already implicitly excluded above)
    for (int i = 0; i < SCALER_COUNT; i++) {
        if (i != SCALER_TSCALE) {
            const char *name = o->scaler[i].kernel.name;
            if (name && strcmp(name, "bilinear") != 0)
                return false;
        }
    }
    if (o->user_shaders && o->user_shaders[0])
        return false;
    if (p->use_lut_3d)
        return false;
    return true;
}

// Disable features that are not supported with the current OpenGL version.
static void check_gl_features(struct gl_video *p)
{
    GL *gl = p->gl;
    bool have_float_tex = !!gl_find_float16_format(gl, 1);
    bool have_3d_tex = gl->mpgl_caps & MPGL_CAP_3D_TEX;
    bool have_mglsl = gl->glsl_version >= 130; // modern GLSL (1st class arrays etc.)
    bool have_texrg = gl->mpgl_caps & MPGL_CAP_TEX_RG;
    bool have_tex16 = !gl->es || (gl->mpgl_caps & MPGL_CAP_EXT16);

    const GLint auto_fbo_fmts[] = {GL_RGBA16, GL_RGBA16F, GL_RGB10_A2,
                                   GL_RGBA8, 0};
    GLint user_fbo_fmts[] = {p->opts.fbo_format, 0};
    const GLint *fbo_fmts = user_fbo_fmts[0] ? user_fbo_fmts : auto_fbo_fmts;
    bool have_fbo = false;
    for (int n = 0; fbo_fmts[n]; n++) {
        GLint fmt = fbo_fmts[n];
        const struct gl_format *f = gl_find_internal_format(gl, fmt);
        if (f && (f->flags & F_CF) == F_CF && test_fbo(p, fmt)) {
            MP_VERBOSE(p, "Using FBO format 0x%x.\n", (unsigned)fmt);
            have_fbo = true;
            p->opts.fbo_format = fmt;
            break;
        }
    }

    if (!gl->MapBufferRange && p->opts.pbo) {
        p->opts.pbo = 0;
        MP_WARN(p, "Disabling PBOs (GL2.1/GLES2 unsupported).\n");
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
        p->opts = (struct gl_video_opts){
            .gamma = p->opts.gamma,
            .gamma_auto = p->opts.gamma_auto,
            .pbo = p->opts.pbo,
            .fbo_format = p->opts.fbo_format,
            .alpha_mode = p->opts.alpha_mode,
            .use_rectangle = p->opts.use_rectangle,
            .background = p->opts.background,
            .dither_algo = DITHER_NONE,
            .target_brightness = p->opts.target_brightness,
            .hdr_tone_mapping = p->opts.hdr_tone_mapping,
            .tone_mapping_param = p->opts.tone_mapping_param,
        };
        for (int n = 0; n < SCALER_COUNT; n++)
            p->opts.scaler[n] = gl_video_opts_def.scaler[n];
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
            if (!have_mglsl)
                reason = "(GLSL version too old)";
            if (reason) {
                MP_WARN(p, "Disabling scaler #%d %s %s.\n", n,
                        p->opts.scaler[n].kernel.name, reason);
                // p->opts is a copy => we can just mess with it.
                p->opts.scaler[n].kernel.name = "bilinear";
                if (n == SCALER_TSCALE)
                    p->opts.interpolation = 0;
            }
        }
    }

    // GLES3 doesn't provide filtered 16 bit integer textures
    // GLES2 doesn't even provide 3D textures
    if (p->use_lut_3d && (!have_3d_tex || !have_tex16)) {
        p->use_lut_3d = false;
        MP_WARN(p, "Disabling color management (no RGB16 3D textures).\n");
    }

    int use_cms = p->opts.target_prim != MP_CSP_PRIM_AUTO ||
                  p->opts.target_trc != MP_CSP_TRC_AUTO || p->use_lut_3d;

    // mix() is needed for some gamma functions
    if (!have_mglsl && (p->opts.linear_scaling || p->opts.sigmoid_upscaling)) {
        p->opts.linear_scaling = false;
        p->opts.sigmoid_upscaling = false;
        MP_WARN(p, "Disabling linear/sigmoid scaling (GLSL version too old).\n");
    }
    if (!have_mglsl && use_cms) {
        p->opts.target_prim = MP_CSP_PRIM_AUTO;
        p->opts.target_trc = MP_CSP_TRC_AUTO;
        p->use_lut_3d = false;
        MP_WARN(p, "Disabling color management (GLSL version too old).\n");
    }
    if (!have_mglsl && p->opts.deband) {
        p->opts.deband = 0;
        MP_WARN(p, "Disabling debanding (GLSL version too old).\n");
    }
}

static void init_gl(struct gl_video *p)
{
    GL *gl = p->gl;

    debug_check_gl(p, "before init_gl");

    gl->Disable(GL_DITHER);

    gl_vao_init(&p->vao, gl, sizeof(struct vertex), vertex_vao);

    gl_video_set_gl_state(p);

    // Test whether we can use 10 bit. Hope that testing a single format/channel
    // is good enough (instead of testing all 1-4 channels variants etc.).
    const struct gl_format *fmt = gl_find_unorm_format(gl, 2, 1);
    if (gl->GetTexLevelParameteriv && fmt) {
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

    if ((gl->es >= 300 || gl->version) && (gl->mpgl_caps & MPGL_CAP_FB)) {
        gl->BindFramebuffer(GL_FRAMEBUFFER, gl->main_fb);

        debug_check_gl(p, "before retrieving framebuffer depth");

        GLenum obj = gl->version ? GL_BACK_LEFT : GL_BACK;
        if (gl->main_fb)
            obj = GL_COLOR_ATTACHMENT0;

        GLint depth_r = -1, depth_g = -1, depth_b = -1;

        gl->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, obj,
                            GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &depth_r);
        gl->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, obj,
                            GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &depth_g);
        gl->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, obj,
                            GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &depth_b);

        debug_check_gl(p, "retrieving framebuffer depth");

        MP_VERBOSE(p, "Reported display depth: R=%d, G=%d, B=%d\n",
                   depth_r, depth_g, depth_b);

        p->fb_depth = depth_g > 0 ? depth_g : 8;

        gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    p->upload_timer = gl_timer_create(p->gl);
    p->render_timer = gl_timer_create(p->gl);
    p->present_timer = gl_timer_create(p->gl);

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

    gl_timer_free(p->upload_timer);
    gl_timer_free(p->render_timer);
    gl_timer_free(p->present_timer);

    mpgl_osd_destroy(p->osd);

    gl_set_debug_logger(gl, NULL);

    talloc_free(p);
}

void gl_video_set_gl_state(struct gl_video *p)
{
    // This resets certain important state to defaults.
    gl_video_unset_gl_state(p);
}

void gl_video_unset_gl_state(struct gl_video *p)
{
    GL *gl = p->gl;

    gl->ActiveTexture(GL_TEXTURE0);
    if (gl->mpgl_caps & MPGL_CAP_ROW_LENGTH)
        gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);
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
static void packed_fmt_swizzle(char w[5], const struct packed_fmt_entry *fmt)
{
    for (int c = 0; c < 4; c++)
        w[c] = "rgba"[MPMAX(fmt->components[c] - 1, 0)];
    w[4] = '\0';
}

// Like gl_find_unorm_format(), but takes bits (not bytes), and if no fixed
// point format is available, return an unsigned integer format.
static const struct gl_format *find_plane_format(GL *gl, int bits, int n_channels)
{
    int bytes = (bits + 7) / 8;
    const struct gl_format *f = gl_find_unorm_format(gl, bytes, n_channels);
    if (f)
        return f;
    return gl_find_uint_format(gl, bytes, n_channels);
}

static void init_image_desc(struct gl_video *p, int fmt)
{
    p->image_desc = mp_imgfmt_get_desc(fmt);

    p->plane_count = p->image_desc.num_planes;
    p->is_yuv = p->image_desc.flags & MP_IMGFLAG_YUV;
    p->has_alpha = p->image_desc.flags & MP_IMGFLAG_ALPHA;
    p->use_integer_conversion = false;
    p->color_swizzle[0] = '\0';
    p->is_packed_yuv = fmt == IMGFMT_UYVY || fmt == IMGFMT_YUYV;
    p->hwdec_active = false;
}

// test_only=true checks if the format is supported
// test_only=false also initializes some rendering parameters accordingly
static bool init_format(struct gl_video *p, int fmt, bool test_only)
{
    struct GL *gl = p->gl;

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (!desc.id)
        return false;

    if (desc.num_planes > 4)
        return false;

    const struct gl_format *plane_format[4] = {0};
    char color_swizzle[5] = "";
    const struct packed_fmt_entry *packed_format = {0};

    // YUV/planar formats
    if (desc.flags & (MP_IMGFLAG_YUV_P | MP_IMGFLAG_RGB_P)) {
        int bits = desc.component_bits;
        if ((desc.flags & MP_IMGFLAG_NE) && bits >= 8 && bits <= 16) {
            plane_format[0] = find_plane_format(gl, bits, 1);
            for (int n = 1; n < desc.num_planes; n++)
                plane_format[n] = plane_format[0];
            // RGB/planar
            if (desc.flags & MP_IMGFLAG_RGB_P)
                snprintf(color_swizzle, sizeof(color_swizzle), "brga");
            goto supported;
        }
    }

    // YUV/half-packed
    if (desc.flags & MP_IMGFLAG_YUV_NV) {
        int bits = desc.component_bits;
        if ((desc.flags & MP_IMGFLAG_NE) && bits >= 8 && bits <= 16) {
            plane_format[0] = find_plane_format(gl, bits, 1);
            plane_format[1] = find_plane_format(gl, bits, 2);
            if (desc.flags & MP_IMGFLAG_YUV_NV_SWAP)
                snprintf(color_swizzle, sizeof(color_swizzle), "rbga");
            goto supported;
        }
    }

    // XYZ (same organization as RGB packed, but requires conversion matrix)
    if (fmt == IMGFMT_XYZ12) {
        plane_format[0] = gl_find_unorm_format(gl, 2, 3);
        goto supported;
    }

    // Packed RGB(A) formats
    for (const struct packed_fmt_entry *e = mp_packed_formats; e->fmt; e++) {
        if (e->fmt == fmt) {
            int n_comp = desc.bytes[0] / e->component_size;
            plane_format[0] = gl_find_unorm_format(gl, e->component_size, n_comp);
            packed_format = e;
            goto supported;
        }
    }

    // Special formats for which OpenGL happens to have direct support.
    plane_format[0] = gl_find_special_format(gl, fmt);
    if (plane_format[0]) {
        // Packed YUV Apple formats color permutation
        if (plane_format[0]->format == GL_RGB_422_APPLE)
            snprintf(color_swizzle, sizeof(color_swizzle), "gbra");
        goto supported;
    }

    // Unsupported format
    return false;

supported:

    if (desc.component_bits > 8 && desc.component_bits < 16) {
        if (p->texture_16bit_depth < 16)
            return false;
    }

    int use_integer = -1;
    for (int n = 0; n < desc.num_planes; n++) {
        if (!plane_format[n])
            return false;
        int use_int_plane = !!gl_integer_format_to_base(plane_format[n]->format);
        if (use_integer < 0)
            use_integer = use_int_plane;
        if (use_integer != use_int_plane)
            return false; // mixed planes not supported
    }

    if (use_integer && p->forced_dumb_mode)
        return false;

    if (!test_only) {
        for (int n = 0; n < desc.num_planes; n++) {
            struct texplane *plane = &p->image.planes[n];
            const struct gl_format *format = plane_format[n];
            assert(format);
            plane->gl_format = format->format;
            plane->gl_internal_format = format->internal_format;
            plane->gl_type = format->type;
            plane->use_integer = use_integer;
            snprintf(plane->swizzle, sizeof(plane->swizzle), "rgba");
            if (packed_format)
                packed_fmt_swizzle(plane->swizzle, packed_format);
            if (plane->gl_format == GL_LUMINANCE_ALPHA)
                MPSWAP(char, plane->swizzle[1], plane->swizzle[3]);
        }

        init_image_desc(p, fmt);

        p->use_integer_conversion = use_integer;
        snprintf(p->color_swizzle, sizeof(p->color_swizzle), "%s", color_swizzle);
    }

    return true;
}

bool gl_video_check_format(struct gl_video *p, int mp_format)
{
    if (init_format(p, mp_format, true))
        return true;
    if (p->hwdec && gl_hwdec_test_format(p->hwdec, mp_format))
        return true;
    return false;
}

void gl_video_config(struct gl_video *p, struct mp_image_params *params)
{
    unref_current_image(p);

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
    reinit_osd(p);
}

struct gl_video *gl_video_init(GL *gl, struct mp_log *log, struct mpv_global *g)
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
        .texture_16bit_depth = 16,
        .sc = gl_sc_create(gl, log),
        .opts_cache = m_config_cache_alloc(p, g, &gl_video_conf),
    };
    struct gl_video_opts *opts = p->opts_cache->opts;
    p->cms = gl_lcms_init(p, log, g, opts->icc_opts),
    p->opts = *opts;
    for (int n = 0; n < SCALER_COUNT; n++)
        p->scaler[n] = (struct scaler){.index = n};
    gl_video_set_debug(p, true);
    init_gl(p);
    reinit_from_options(p);
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

void gl_video_update_options(struct gl_video *p)
{
    if (m_config_cache_update(p->opts_cache)) {
        gl_lcms_update_options(p->cms);
        reinit_from_options(p);
    }
}

static void reinit_from_options(struct gl_video *p)
{
    p->use_lut_3d = gl_lcms_has_profile(p->cms);

    // Copy the option fields, so that check_gl_features() can mutate them.
    // This works only for the fields themselves of course, not for any memory
    // referenced by them.
    p->opts = *(struct gl_video_opts *)p->opts_cache->opts;

    check_gl_features(p);
    uninit_rendering(p);
    gl_video_setup_hooks(p);
    reinit_osd(p);

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
            // Oversample/linear case
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
        r = M_OPT_EXIT;
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
        r = M_OPT_EXIT;
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
    unref_current_image(p);
}
