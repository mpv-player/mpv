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
#include "utils.h"
#include "hwdec.h"
#include "osd.h"
#include "ra.h"
#include "stream/stream.h"
#include "video_shaders.h"
#include "user_shaders.h"
#include "video/out/filter_kernels.h"
#include "video/out/aspect.h"
#include "video/out/dither.h"
#include "video/out/vo.h"

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
int tscale_sizes[] = {2, 4, 6, 8, 0};

struct vertex_pt {
    float x, y;
};

struct texplane {
    struct ra_tex *tex;
    int w, h;
    bool flipped;
};

struct video_image {
    struct texplane planes[4];
    struct mp_image *mpi;       // original input image
    uint64_t id;                // unique ID identifying mpi contents
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

static const char *plane_names[] = {
    [PLANE_NONE] = "unknown",
    [PLANE_RGB] = "rgb",
    [PLANE_LUMA] = "luma",
    [PLANE_CHROMA] = "chroma",
    [PLANE_ALPHA] = "alpha",
    [PLANE_XYZ] = "xyz",
};

// A self-contained description of a source image which can be bound to a
// texture unit and sampled from. Contains metadata about how it's to be used
struct image {
    enum plane_type type; // must be set to something non-zero
    int components; // number of relevant coordinates
    float multiplier; // multiplier to be used when sampling
    struct ra_tex *tex;
    int w, h; // logical size (after transformation)
    struct gl_transform transform; // rendering transformation
};

// A named image, for user scripting purposes
struct saved_img {
    const char *name;
    struct image img;
};

// A texture hook. This is some operation that transforms a named texture as
// soon as it's generated
struct tex_hook {
    const char *save_tex;
    const char *hook_tex[SHADER_MAX_HOOKS];
    const char *bind_tex[SHADER_MAX_BINDS];
    int components; // how many components are relevant (0 = same as input)
    void *priv; // this gets talloc_freed when the tex_hook is removed
    void (*hook)(struct gl_video *p, struct image img, // generates GLSL
                 struct gl_transform *trans, void *priv);
    bool (*cond)(struct gl_video *p, struct image img, void *priv);
};

struct surface {
    struct ra_tex *tex;
    uint64_t id;
    double pts;
};

#define SURFACES_MAX 10

struct cached_file {
    char *path;
    struct bstr body;
};

struct pass_info {
    struct bstr desc;
    struct mp_pass_perf perf;
};

struct dr_buffer {
    struct ra_buf *buf;
    // The mpi reference will keep the data from being recycled (or from other
    // references gaining write access) while the GPU is accessing the buffer.
    struct mp_image *mpi;
};

struct gl_video {
    struct ra *ra;

    struct mpv_global *global;
    struct mp_log *log;
    struct gl_video_opts opts;
    struct m_config_cache *opts_cache;
    struct gl_lcms *cms;

    int fb_depth;               // actual bits available in GL main framebuffer
    struct m_color clear_color;
    bool force_clear_color;

    struct gl_shader_cache *sc;

    struct osd_state *osd_state;
    struct mpgl_osd *osd;
    double osd_pts;

    struct ra_tex *lut_3d_texture;
    bool use_lut_3d;
    int lut_3d_size[3];

    struct ra_tex *dither_texture;

    struct mp_image_params real_image_params;   // configured format
    struct mp_image_params image_params;        // texture format (mind hwdec case)
    struct ra_imgfmt_desc ra_format;            // texture format
    int plane_count;

    bool is_gray;
    bool has_alpha;
    char color_swizzle[5];
    bool use_integer_conversion;

    struct video_image image;

    struct dr_buffer *dr_buffers;
    int num_dr_buffers;

    bool using_dr_path;

    bool dumb_mode;
    bool forced_dumb_mode;

    // Cached vertex array, to avoid re-allocation per frame. For simplicity,
    // our vertex format is simply a list of `vertex_pt`s, since this greatly
    // simplifies offset calculation at the cost of (unneeded) flexibility.
    struct vertex_pt *tmp_vertex;
    struct ra_renderpass_input *vao;
    int vao_len;

    const struct ra_format *fbo_format;
    struct ra_tex *merge_tex[4];
    struct ra_tex *scale_tex[4];
    struct ra_tex *integer_tex[4];
    struct ra_tex *indirect_tex;
    struct ra_tex *blend_subs_tex;
    struct ra_tex *screen_tex;
    struct ra_tex *output_tex;
    struct ra_tex *vdpau_deinterleave_tex[2];
    struct ra_tex **hook_textures;
    int num_hook_textures;
    int idx_hook_textures;

    struct ra_buf *hdr_peak_ssbo;
    struct surface surfaces[SURFACES_MAX];

    // user pass descriptions and textures
    struct tex_hook *tex_hooks;
    int num_tex_hooks;
    struct gl_user_shader_tex *user_textures;
    int num_user_textures;

    int surface_idx;
    int surface_now;
    int frames_drawn;
    bool is_interpolated;
    bool output_tex_valid;

    // state for configured scalers
    struct scaler scaler[SCALER_COUNT];

    struct mp_csp_equalizer_state *video_eq;

    struct mp_rect src_rect;    // displayed part of the source video
    struct mp_rect dst_rect;    // video rectangle on output window
    struct mp_osd_res osd_rect; // OSD size/margins

    // temporary during rendering
    struct compute_info pass_compute; // compute shader metadata for this pass
    struct image *pass_imgs;          // bound images for this pass
    int num_pass_imgs;
    struct saved_img *saved_imgs;     // saved (named) images for this frame
    int num_saved_imgs;

    // effective current texture metadata - this will essentially affect the
    // next render pass target, as well as implicitly tracking what needs to
    // be done with the image
    int texture_w, texture_h;
    struct gl_transform texture_offset; // texture transform without rotation
    int components;
    bool use_linear;
    float user_gamma;

    // pass info / metrics
    struct pass_info pass_fresh[VO_PASS_PERF_MAX];
    struct pass_info pass_redraw[VO_PASS_PERF_MAX];
    struct pass_info *pass;
    int pass_idx;
    struct timer_pool *upload_timer;
    struct timer_pool *blit_timer;
    struct timer_pool *osd_timer;

    int frames_uploaded;
    int frames_rendered;
    AVLFG lfg;

    // Cached because computing it can take relatively long
    int last_dither_matrix_size;
    float *last_dither_matrix;

    struct cached_file *files;
    int num_files;

    bool hwdec_interop_loading_done;
    struct ra_hwdec **hwdecs;
    int num_hwdecs;

    struct ra_hwdec_mapper *hwdec_mapper;
    struct ra_hwdec *hwdec_overlay;
    bool hwdec_active;

    bool dsi_warned;
    bool broken_frame; // temporary error state
};

static const struct gl_video_opts gl_video_opts_def = {
    .dither_algo = DITHER_FRUIT,
    .dither_depth = -1,
    .dither_size = 6,
    .temporal_dither_period = 1,
    .fbo_format = "auto",
    .sigmoid_center = 0.75,
    .sigmoid_slope = 6.5,
    .scaler = {
        {{"bilinear", .params={NAN, NAN}}, {.params = {NAN, NAN}},
         .cutoff = 0.001}, // scale
        {{NULL,       .params={NAN, NAN}}, {.params = {NAN, NAN}},
         .cutoff = 0.001}, // dscale
        {{"bilinear", .params={NAN, NAN}}, {.params = {NAN, NAN}},
         .cutoff = 0.001}, // cscale
        {{"mitchell", .params={NAN, NAN}}, {.params = {NAN, NAN}},
         .clamp = 1, }, // tscale
    },
    .scaler_resizes_only = 1,
    .scaler_lut_size = 6,
    .interpolation_threshold = 0.0001,
    .alpha_mode = ALPHA_BLEND_TILES,
    .background = {0, 0, 0, 255},
    .gamma = 1.0f,
    .tone_mapping = TONE_MAPPING_HABLE,
    .tone_mapping_param = NAN,
    .tone_mapping_desat = 0.5,
    .early_flush = -1,
    .hwdec_interop = "auto",
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
    OPT_FLOATRANGE(n"-cutoff", scaler[i].cutoff, 0, 0.0, 1.0),             \
    OPT_FLOATRANGE(n"-taper", scaler[i].kernel.taper, 0, 0.0, 1.0),        \
    OPT_FLOAT(n"-wparam", scaler[i].window.params[0], 0),                  \
    OPT_FLOAT(n"-wblur",  scaler[i].window.blur, 0),                       \
    OPT_FLOATRANGE(n"-wtaper", scaler[i].window.taper, 0, 0.0, 1.0),       \
    OPT_FLOATRANGE(n"-clamp", scaler[i].clamp, 0, 0.0, 1.0),               \
    OPT_FLOATRANGE(n"-radius",    scaler[i].radius, 0, 0.5, 16.0),         \
    OPT_FLOATRANGE(n"-antiring",  scaler[i].antiring, 0, 0.0, 1.0),        \
    OPT_STRING_VALIDATE(n"-window", scaler[i].window.name, 0, validate_window_opt)

const struct m_sub_options gl_video_conf = {
    .opts = (const m_option_t[]) {
        OPT_CHOICE("gpu-dumb-mode", dumb_mode, 0,
                   ({"auto", 0}, {"yes", 1}, {"no", -1})),
        OPT_FLOATRANGE("gamma-factor", gamma, 0, 0.1, 2.0),
        OPT_FLAG("gamma-auto", gamma_auto, 0),
        OPT_CHOICE_C("target-prim", target_prim, 0, mp_csp_prim_names),
        OPT_CHOICE_C("target-trc", target_trc, 0, mp_csp_trc_names),
        OPT_INTRANGE("target-peak", target_peak, 0, 10, 10000),
        OPT_CHOICE("tone-mapping", tone_mapping, 0,
                   ({"clip",     TONE_MAPPING_CLIP},
                    {"mobius",   TONE_MAPPING_MOBIUS},
                    {"reinhard", TONE_MAPPING_REINHARD},
                    {"hable",    TONE_MAPPING_HABLE},
                    {"gamma",    TONE_MAPPING_GAMMA},
                    {"linear",   TONE_MAPPING_LINEAR})),
        OPT_CHOICE("hdr-compute-peak", compute_hdr_peak, 0,
                   ({"auto", 0},
                    {"yes", 1},
                    {"no", -1})),
        OPT_FLOAT("tone-mapping-param", tone_mapping_param, 0),
        OPT_FLOAT("tone-mapping-desaturate", tone_mapping_desat, 0),
        OPT_FLAG("gamut-warning", gamut_warning, 0),
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
        OPT_STRING("fbo-format", fbo_format, 0),
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
        OPT_PATHLIST("glsl-shaders", user_shaders, 0),
        OPT_CLI_ALIAS("glsl-shader", "glsl-shaders-append"),
        OPT_FLAG("deband", deband, 0),
        OPT_SUBSTRUCT("deband", deband_opts, deband_conf, 0),
        OPT_FLOAT("sharpen", unsharp, 0),
        OPT_INTRANGE("gpu-tex-pad-x", tex_pad_x, 0, 0, 4096),
        OPT_INTRANGE("gpu-tex-pad-y", tex_pad_y, 0, 0, 4096),
        OPT_SUBSTRUCT("", icc_opts, mp_icc_conf, 0),
        OPT_STRING("gpu-shader-cache-dir", shader_cache_dir, 0),
        OPT_STRING_VALIDATE("gpu-hwdec-interop", hwdec_interop, 0,
                             ra_hwdec_validate_opt),
        OPT_REPLACED("opengl-hwdec-interop", "gpu-hwdec-interop"),
        OPT_REPLACED("hwdec-preload", "opengl-hwdec-interop"),
        OPT_REPLACED("hdr-tone-mapping", "tone-mapping"),
        OPT_REPLACED("opengl-shaders", "glsl-shaders"),
        OPT_REPLACED("opengl-shader", "glsl-shader"),
        OPT_REPLACED("opengl-shader-cache-dir", "gpu-shader-cache-dir"),
        OPT_REPLACED("opengl-tex-pad-x", "gpu-tex-pad-x"),
        OPT_REPLACED("opengl-tex-pad-y", "gpu-tex-pad-y"),
        OPT_REPLACED("opengl-fbo-format", "fbo-format"),
        OPT_REPLACED("opengl-dumb-mode", "gpu-dumb-mode"),
        OPT_REPLACED("opengl-gamma", "gamma-factor"),
        {0}
    },
    .size = sizeof(struct gl_video_opts),
    .defaults = &gl_video_opts_def,
};

static void uninit_rendering(struct gl_video *p);
static void uninit_scaler(struct gl_video *p, struct scaler *scaler);
static void check_gl_features(struct gl_video *p);
static bool pass_upload_image(struct gl_video *p, struct mp_image *mpi, uint64_t id);
static const char *handle_scaler_opt(const char *name, bool tscale);
static void reinit_from_options(struct gl_video *p);
static void get_scale_factors(struct gl_video *p, bool transpose_rot, double xy[2]);
static void gl_video_setup_hooks(struct gl_video *p);
static void gl_video_update_options(struct gl_video *p);

#define GLSL(x) gl_sc_add(p->sc, #x "\n");
#define GLSLF(...) gl_sc_addf(p->sc, __VA_ARGS__)
#define GLSLHF(...) gl_sc_haddf(p->sc, __VA_ARGS__)
#define PRELUDE(...) gl_sc_paddf(p->sc, __VA_ARGS__)

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
    if (p->ra->fns->debug_marker)
        p->ra->fns->debug_marker(p->ra, msg);
}

static void gl_video_reset_surfaces(struct gl_video *p)
{
    for (int i = 0; i < SURFACES_MAX; i++) {
        p->surfaces[i].id = 0;
        p->surfaces[i].pts = MP_NOPTS_VALUE;
    }
    p->surface_idx = 0;
    p->surface_now = 0;
    p->frames_drawn = 0;
    p->output_tex_valid = false;
}

static void gl_video_reset_hooks(struct gl_video *p)
{
    for (int i = 0; i < p->num_tex_hooks; i++)
        talloc_free(p->tex_hooks[i].priv);

    for (int i = 0; i < p->num_user_textures; i++)
        ra_tex_free(p->ra, &p->user_textures[i].tex);

    p->num_tex_hooks = 0;
    p->num_user_textures = 0;
}

static inline int surface_wrap(int id)
{
    id = id % SURFACES_MAX;
    return id < 0 ? id + SURFACES_MAX : id;
}

static void reinit_osd(struct gl_video *p)
{
    mpgl_osd_destroy(p->osd);
    p->osd = NULL;
    if (p->osd_state)
        p->osd = mpgl_osd_init(p->ra, p->log, p->osd_state);
}

static void uninit_rendering(struct gl_video *p)
{
    for (int n = 0; n < SCALER_COUNT; n++)
        uninit_scaler(p, &p->scaler[n]);

    ra_tex_free(p->ra, &p->dither_texture);

    for (int n = 0; n < 4; n++) {
        ra_tex_free(p->ra, &p->merge_tex[n]);
        ra_tex_free(p->ra, &p->scale_tex[n]);
        ra_tex_free(p->ra, &p->integer_tex[n]);
    }

    ra_tex_free(p->ra, &p->indirect_tex);
    ra_tex_free(p->ra, &p->blend_subs_tex);
    ra_tex_free(p->ra, &p->screen_tex);
    ra_tex_free(p->ra, &p->output_tex);

    for (int n = 0; n < SURFACES_MAX; n++)
        ra_tex_free(p->ra, &p->surfaces[n].tex);

    for (int n = 0; n < p->num_hook_textures; n++)
        ra_tex_free(p->ra, &p->hook_textures[n]);

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
        .sig_peak = p->opts.target_peak / MP_REF_WHITE,
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
    if (!p->use_lut_3d)
        return false;

    struct AVBufferRef *icc = NULL;
    if (p->image.mpi)
        icc = p->image.mpi->icc_profile;

    if (p->lut_3d_texture && !gl_lcms_has_changed(p->cms, prim, trc, icc))
        return true;

    // GLES3 doesn't provide filtered 16 bit integer textures
    // GLES2 doesn't even provide 3D textures
    const struct ra_format *fmt = ra_find_unorm_format(p->ra, 2, 4);
    if (!fmt || !(p->ra->caps & RA_CAP_TEX_3D)) {
        p->use_lut_3d = false;
        MP_WARN(p, "Disabling color management (no RGBA16 3D textures).\n");
        return false;
    }

    struct lut3d *lut3d = NULL;
    if (!fmt || !gl_lcms_get_lut3d(p->cms, &lut3d, prim, trc, icc) || !lut3d) {
        p->use_lut_3d = false;
        return false;
    }

    ra_tex_free(p->ra, &p->lut_3d_texture);

    struct ra_tex_params params = {
        .dimensions = 3,
        .w = lut3d->size[0],
        .h = lut3d->size[1],
        .d = lut3d->size[2],
        .format = fmt,
        .render_src = true,
        .src_linear = true,
        .initial_data = lut3d->data,
    };
    p->lut_3d_texture = ra_tex_create(p->ra, &params);

    debug_check_gl(p, "after 3d lut creation");

    for (int i = 0; i < 3; i++)
        p->lut_3d_size[i] = lut3d->size[i];

    talloc_free(lut3d);

    return true;
}

// Fill an image struct from a ra_tex + some metadata
static struct image image_wrap(struct ra_tex *tex, enum plane_type type,
                               int components)
{
    assert(type != PLANE_NONE);
    return (struct image){
        .type = type,
        .tex = tex,
        .multiplier = 1.0,
        .w = tex ? tex->params.w : 1,
        .h = tex ? tex->params.h : 1,
        .transform = identity_trans,
        .components = components,
    };
}

// Bind an image to a free texture unit and return its ID.
static int pass_bind(struct gl_video *p, struct image img)
{
    int idx = p->num_pass_imgs;
    MP_TARRAY_APPEND(p, p->pass_imgs, p->num_pass_imgs, img);
    return idx;
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
static int chroma_upsize(int size, int pixel)
{
    return (size + pixel - 1) / pixel * pixel;
}

// If a and b are on the same plane, return what plane type should be used.
// If a or b are none, the other type always wins.
// Usually: LUMA/RGB/XYZ > CHROMA > ALPHA
static enum plane_type merge_plane_types(enum plane_type a, enum plane_type b)
{
    if (a == PLANE_NONE)
        return b;
    if (b == PLANE_LUMA || b == PLANE_RGB || b == PLANE_XYZ)
        return b;
    if (b != PLANE_NONE && a == PLANE_ALPHA)
        return b;
    return a;
}

// Places a video_image's image textures + associated metadata into img[]. The
// number of textures is equal to p->plane_count. Any necessary plane offsets
// are stored in off. (e.g. chroma position)
static void pass_get_images(struct gl_video *p, struct video_image *vimg,
                            struct image img[4], struct gl_transform off[4])
{
    assert(vimg->mpi);

    int w = p->image_params.w;
    int h = p->image_params.h;

    // Determine the chroma offset
    float ls_w = 1.0 / p->ra_format.chroma_w;
    float ls_h = 1.0 / p->ra_format.chroma_h;

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

    int msb_valid_bits =
        p->ra_format.component_bits + MPMIN(p->ra_format.component_pad, 0);
    // The existing code assumes we just have a single tex multiplier for
    // all of the planes. This may change in the future
    float tex_mul = 1.0 / mp_get_csp_mul(p->image_params.color.space,
                                         msb_valid_bits,
                                         p->ra_format.component_bits);

    memset(img, 0, 4 * sizeof(img[0]));
    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *t = &vimg->planes[n];

        enum plane_type type = PLANE_NONE;
        for (int i = 0; i < 4; i++) {
            int c = p->ra_format.components[n][i];
            enum plane_type ctype;
            if (c == 0) {
                ctype = PLANE_NONE;
            } else if (c == 4) {
                ctype = PLANE_ALPHA;
            } else if (p->image_params.color.space == MP_CSP_RGB) {
                ctype = PLANE_RGB;
            } else if (p->image_params.color.space == MP_CSP_XYZ) {
                ctype = PLANE_XYZ;
            } else {
                ctype = c == 1 ? PLANE_LUMA : PLANE_CHROMA;
            }
            type = merge_plane_types(type, ctype);
        }

        img[n] = (struct image){
            .type = type,
            .tex = t->tex,
            .multiplier = tex_mul,
            .w = t->w,
            .h = t->h,
        };

        for (int i = 0; i < 4; i++)
            img[n].components += !!p->ra_format.components[n][i];

        get_transform(t->w, t->h, p->image_params.rotate, t->flipped,
                      &img[n].transform);
        if (p->image_params.rotate % 180 == 90)
            MPSWAP(int, img[n].w, img[n].h);

        off[n] = identity_trans;

        if (type == PLANE_CHROMA) {
            struct gl_transform rot;
            get_transform(0, 0, p->image_params.rotate, true, &rot);

            struct gl_transform tr = chroma;
            gl_transform_vec(rot, &tr.t[0], &tr.t[1]);

            float dx = (chroma_upsize(w, p->ra_format.chroma_w) - w) * ls_w;
            float dy = (chroma_upsize(h, p->ra_format.chroma_h) - h) * ls_h;

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

// Return the index of the given component (assuming all non-padding components
// of all planes are concatenated into a linear list).
static int find_comp(struct ra_imgfmt_desc *desc, int component)
{
    int cur = 0;
    for (int n = 0; n < desc->num_planes; n++) {
        for (int i = 0; i < 4; i++) {
            if (desc->components[n][i]) {
                if (desc->components[n][i] == component)
                    return cur;
                cur++;
            }
        }
    }
    return -1;
}

static void init_video(struct gl_video *p)
{
    p->use_integer_conversion = false;

    struct ra_hwdec *hwdec = NULL;
    for (int n = 0; n < p->num_hwdecs; n++) {
        if (ra_hwdec_test_format(p->hwdecs[n], p->image_params.imgfmt)) {
            hwdec = p->hwdecs[n];
            break;
        }
    }

    if (hwdec) {
        if (hwdec->driver->overlay_frame) {
            MP_WARN(p, "Using HW-overlay mode. No GL filtering is performed "
                       "on the video!\n");
            p->hwdec_overlay = hwdec;
        } else {
            p->hwdec_mapper = ra_hwdec_mapper_create(hwdec, &p->image_params);
            if (!p->hwdec_mapper)
                MP_ERR(p, "Initializing texture for hardware decoding failed.\n");
        }
        if (p->hwdec_mapper)
            p->image_params = p->hwdec_mapper->dst_params;
        const char **exts = hwdec->glsl_extensions;
        for (int n = 0; exts && exts[n]; n++)
            gl_sc_enable_extension(p->sc, (char *)exts[n]);
        p->hwdec_active = true;
    }

    p->ra_format = (struct ra_imgfmt_desc){0};
    ra_get_imgfmt_desc(p->ra, p->image_params.imgfmt, &p->ra_format);

    p->plane_count = p->ra_format.num_planes;

    p->has_alpha = false;
    p->is_gray = true;

    for (int n = 0; n < p->ra_format.num_planes; n++) {
        for (int i = 0; i < 4; i++) {
            if (p->ra_format.components[n][i]) {
                p->has_alpha |= p->ra_format.components[n][i] == 4;
                p->is_gray &= p->ra_format.components[n][i] == 1 ||
                              p->ra_format.components[n][i] == 4;
            }
        }
    }

    for (int c = 0; c < 4; c++) {
        int loc = find_comp(&p->ra_format, c + 1);
        p->color_swizzle[c] = "rgba"[loc >= 0 && loc < 4 ? loc : 0];
    }
    p->color_swizzle[4] = '\0';

    mp_image_params_guess_csp(&p->image_params);

    av_lfg_init(&p->lfg, 1);

    debug_check_gl(p, "before video texture creation");

    if (!p->hwdec_active) {
        struct video_image *vimg = &p->image;

        struct mp_image layout = {0};
        mp_image_set_params(&layout, &p->image_params);

        for (int n = 0; n < p->plane_count; n++) {
            struct texplane *plane = &vimg->planes[n];
            const struct ra_format *format = p->ra_format.planes[n];

            plane->w = mp_image_plane_w(&layout, n);
            plane->h = mp_image_plane_h(&layout, n);

            struct ra_tex_params params = {
                .dimensions = 2,
                .w = plane->w + p->opts.tex_pad_x,
                .h = plane->h + p->opts.tex_pad_y,
                .d = 1,
                .format = format,
                .render_src = true,
                .src_linear = format->linear_filter,
                .non_normalized = p->opts.use_rectangle,
                .host_mutable = true,
            };

            MP_VERBOSE(p, "Texture for plane %d: %dx%d\n", n,
                       params.w, params.h);

            plane->tex = ra_tex_create(p->ra, &params);
            if (!plane->tex)
                abort(); // shit happens

            p->use_integer_conversion |= format->ctype == RA_CTYPE_UINT;
        }
    }

    debug_check_gl(p, "after video texture creation");

    // Format-dependent checks.
    check_gl_features(p);

    gl_video_setup_hooks(p);
}

static struct dr_buffer *gl_find_dr_buffer(struct gl_video *p, uint8_t *ptr)
{
   for (int i = 0; i < p->num_dr_buffers; i++) {
       struct dr_buffer *buffer = &p->dr_buffers[i];
        uint8_t *bufptr = buffer->buf->data;
        size_t size = buffer->buf->params.size;
        if (ptr >= bufptr && ptr < bufptr + size)
            return buffer;
    }

    return NULL;
}

static void gc_pending_dr_fences(struct gl_video *p, bool force)
{
again:;
    for (int n = 0; n < p->num_dr_buffers; n++) {
        struct dr_buffer *buffer = &p->dr_buffers[n];
        if (!buffer->mpi)
            continue;

        bool res = p->ra->fns->buf_poll(p->ra, buffer->buf);
        if (res || force) {
            // Unreferencing the image could cause gl_video_dr_free_buffer()
            // to be called by the talloc destructor (if it was the last
            // reference). This will implicitly invalidate the buffer pointer
            // and change the p->dr_buffers array. To make it worse, it could
            // free multiple dr_buffers due to weird theoretical corner cases.
            // This is also why we use the goto to iterate again from the
            // start, because everything gets fucked up. Hail satan!
            struct mp_image *ref = buffer->mpi;
            buffer->mpi = NULL;
            talloc_free(ref);
            goto again;
        }
    }
}

static void unref_current_image(struct gl_video *p)
{
    struct video_image *vimg = &p->image;

    if (vimg->hwdec_mapped) {
        assert(p->hwdec_active && p->hwdec_mapper);
        ra_hwdec_mapper_unmap(p->hwdec_mapper);
        memset(vimg->planes, 0, sizeof(vimg->planes));
        vimg->hwdec_mapped = false;
    }

    vimg->id = 0;

    mp_image_unrefp(&vimg->mpi);

    // While we're at it, also garbage collect pending fences in here to
    // get it out of the way.
    gc_pending_dr_fences(p, false);
}

// If overlay mode is used, make sure to remove the overlay.
// Be careful with this. Removing the overlay and adding another one will
// lead to flickering artifacts.
static void unmap_overlay(struct gl_video *p)
{
    if (p->hwdec_overlay)
        p->hwdec_overlay->driver->overlay_frame(p->hwdec_overlay, NULL, NULL, NULL, true);
}

static void uninit_video(struct gl_video *p)
{
    uninit_rendering(p);

    struct video_image *vimg = &p->image;

    unmap_overlay(p);
    unref_current_image(p);

    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];
        ra_tex_free(p->ra, &plane->tex);
    }
    *vimg = (struct video_image){0};

    // Invalidate image_params to ensure that gl_video_config() will call
    // init_video() on uninitialized gl_video.
    p->real_image_params = (struct mp_image_params){0};
    p->image_params = p->real_image_params;
    p->hwdec_active = false;
    p->hwdec_overlay = NULL;
    ra_hwdec_mapper_free(&p->hwdec_mapper);

    for (int n = 0; n < 2; n++)
        ra_tex_free(p->ra, &p->vdpau_deinterleave_tex[n]);
}

static void pass_record(struct gl_video *p, struct mp_pass_perf perf)
{
    if (!p->pass || p->pass_idx == VO_PASS_PERF_MAX)
        return;

    struct pass_info *pass = &p->pass[p->pass_idx];
    pass->perf = perf;

    if (pass->desc.len == 0)
        bstr_xappend(p, &pass->desc, bstr0("(unknown)"));

    p->pass_idx++;
}

PRINTF_ATTRIBUTE(2, 3)
static void pass_describe(struct gl_video *p, const char *textf, ...)
{
    if (!p->pass || p->pass_idx == VO_PASS_PERF_MAX)
        return;

    struct pass_info *pass = &p->pass[p->pass_idx];

    if (pass->desc.len > 0)
        bstr_xappend(p, &pass->desc, bstr0(" + "));

    va_list ap;
    va_start(ap, textf);
    bstr_xappend_vasprintf(p, &pass->desc, textf, ap);
    va_end(ap);
}

static void pass_info_reset(struct gl_video *p, bool is_redraw)
{
    p->pass = is_redraw ? p->pass_redraw : p->pass_fresh;
    p->pass_idx = 0;

    for (int i = 0; i < VO_PASS_PERF_MAX; i++) {
        p->pass[i].desc.len = 0;
        p->pass[i].perf = (struct mp_pass_perf){0};
    }
}

static void pass_report_performance(struct gl_video *p)
{
    if (!p->pass)
        return;

    for (int i = 0; i < VO_PASS_PERF_MAX; i++) {
        struct pass_info *pass = &p->pass[i];
        if (pass->desc.len) {
            MP_TRACE(p, "pass '%.*s': last %dus avg %dus peak %dus\n",
                     BSTR_P(pass->desc),
                     (int)pass->perf.last/1000,
                     (int)pass->perf.avg/1000,
                     (int)pass->perf.peak/1000);
        }
    }
}

static void pass_prepare_src_tex(struct gl_video *p)
{
    struct gl_shader_cache *sc = p->sc;

    for (int n = 0; n < p->num_pass_imgs; n++) {
        struct image *s = &p->pass_imgs[n];
        if (!s->tex)
            continue;

        char *texture_name = mp_tprintf(32, "texture%d", n);
        char *texture_size = mp_tprintf(32, "texture_size%d", n);
        char *texture_rot = mp_tprintf(32, "texture_rot%d", n);
        char *texture_off = mp_tprintf(32, "texture_off%d", n);
        char *pixel_size = mp_tprintf(32, "pixel_size%d", n);

        gl_sc_uniform_texture(sc, texture_name, s->tex);
        float f[2] = {1, 1};
        if (!s->tex->params.non_normalized) {
            f[0] = s->tex->params.w;
            f[1] = s->tex->params.h;
        }
        gl_sc_uniform_vec2(sc, texture_size, f);
        gl_sc_uniform_mat2(sc, texture_rot, true, (float *)s->transform.m);
        gl_sc_uniform_vec2(sc, texture_off, (float *)s->transform.t);
        gl_sc_uniform_vec2(sc, pixel_size, (float[]){1.0f / f[0],
                                                     1.0f / f[1]});
    }
}

static void cleanup_binds(struct gl_video *p)
{
    p->num_pass_imgs = 0;
}

// Sets the appropriate compute shader metadata for an implicit compute pass
// bw/bh: block size
static void pass_is_compute(struct gl_video *p, int bw, int bh, bool flexible)
{
    if (p->pass_compute.active && flexible) {
        // Avoid overwriting existing block sizes when using a flexible pass
        bw = p->pass_compute.block_w;
        bh = p->pass_compute.block_h;
    }

    p->pass_compute = (struct compute_info){
        .active = true,
        .block_w = bw,
        .block_h = bh,
    };
}

// w/h: the width/height of the compute shader's operating domain (e.g. the
// target target that needs to be written, or the source texture that needs to
// be reduced)
static void dispatch_compute(struct gl_video *p, int w, int h,
                             struct compute_info info)
{
    PRELUDE("layout (local_size_x = %d, local_size_y = %d) in;\n",
            info.threads_w > 0 ? info.threads_w : info.block_w,
            info.threads_h > 0 ? info.threads_h : info.block_h);

    pass_prepare_src_tex(p);

    // Since we don't actually have vertices, we pretend for convenience
    // reasons that we do and calculate the right texture coordinates based on
    // the output sample ID
    gl_sc_uniform_vec2(p->sc, "out_scale", (float[2]){ 1.0 / w, 1.0 / h });
    PRELUDE("#define outcoord(id) (out_scale * (vec2(id) + vec2(0.5)))\n");

    for (int n = 0; n < p->num_pass_imgs; n++) {
        struct image *s = &p->pass_imgs[n];
        if (!s->tex)
            continue;

        // We need to rescale the coordinates to the true texture size
        char *tex_scale = mp_tprintf(32, "tex_scale%d", n);
        gl_sc_uniform_vec2(p->sc, tex_scale, (float[2]){
                (float)s->w / s->tex->params.w,
                (float)s->h / s->tex->params.h,
        });

        PRELUDE("#define texmap%d_raw(id) (tex_scale%d * outcoord(id))\n", n, n);
        PRELUDE("#define texmap%d(id) (texture_rot%d * texmap%d_raw(id) + "
               "pixel_size%d * texture_off%d)\n", n, n, n, n, n);
        PRELUDE("#define texcoord%d texmap%d(gl_GlobalInvocationID)\n", n, n);
    }

    // always round up when dividing to make sure we don't leave off a part of
    // the image
    int num_x = info.block_w > 0 ? (w + info.block_w - 1) / info.block_w : 1,
        num_y = info.block_h > 0 ? (h + info.block_h - 1) / info.block_h : 1;

    if (!(p->ra->caps & RA_CAP_NUM_GROUPS))
        PRELUDE("#define gl_NumWorkGroups uvec3(%d, %d, 1)\n", num_x, num_y);

    pass_record(p, gl_sc_dispatch_compute(p->sc, num_x, num_y, 1));
    cleanup_binds(p);
}

static struct mp_pass_perf render_pass_quad(struct gl_video *p,
                                            struct ra_fbo fbo, bool discard,
                                            const struct mp_rect *dst)
{
    // The first element is reserved for `vec2 position`
    int num_vertex_attribs = 1 + p->num_pass_imgs;
    size_t vertex_stride = num_vertex_attribs * sizeof(struct vertex_pt);

    // Expand the VAO if necessary
    while (p->vao_len < num_vertex_attribs) {
        MP_TARRAY_APPEND(p, p->vao, p->vao_len, (struct ra_renderpass_input) {
            .name = talloc_asprintf(p, "texcoord%d", p->vao_len - 1),
            .type = RA_VARTYPE_FLOAT,
            .dim_v = 2,
            .dim_m = 1,
            .offset = p->vao_len * sizeof(struct vertex_pt),
        });
    }

    int num_vertices = 6; // quad as triangle list
    int num_attribs_total = num_vertices * num_vertex_attribs;
    MP_TARRAY_GROW(p, p->tmp_vertex, num_attribs_total);

    struct gl_transform t;
    gl_transform_ortho_fbo(&t, fbo);

    float x[2] = {dst->x0, dst->x1};
    float y[2] = {dst->y0, dst->y1};
    gl_transform_vec(t, &x[0], &y[0]);
    gl_transform_vec(t, &x[1], &y[1]);

    for (int n = 0; n < 4; n++) {
        struct vertex_pt *vs = &p->tmp_vertex[num_vertex_attribs * n];
        // vec2 position in idx 0
        vs[0].x = x[n / 2];
        vs[0].y = y[n % 2];
        for (int i = 0; i < p->num_pass_imgs; i++) {
            struct image *s = &p->pass_imgs[i];
            if (!s->tex)
                continue;
            struct gl_transform tr = s->transform;
            float tx = (n / 2) * s->w;
            float ty = (n % 2) * s->h;
            gl_transform_vec(tr, &tx, &ty);
            bool rect = s->tex->params.non_normalized;
            // vec2 texcoordN in idx N+1
            vs[i + 1].x = tx / (rect ? 1 : s->tex->params.w);
            vs[i + 1].y = ty / (rect ? 1 : s->tex->params.h);
        }
    }

    memmove(&p->tmp_vertex[num_vertex_attribs * 4],
            &p->tmp_vertex[num_vertex_attribs * 2],
            vertex_stride);

    memmove(&p->tmp_vertex[num_vertex_attribs * 5],
            &p->tmp_vertex[num_vertex_attribs * 1],
            vertex_stride);

    return gl_sc_dispatch_draw(p->sc, fbo.tex, discard, p->vao, num_vertex_attribs,
                               vertex_stride, p->tmp_vertex, num_vertices);
}

static void finish_pass_fbo(struct gl_video *p, struct ra_fbo fbo,
                            bool discard, const struct mp_rect *dst)
{
    pass_prepare_src_tex(p);
    pass_record(p, render_pass_quad(p, fbo, discard, dst));
    debug_check_gl(p, "after rendering");
    cleanup_binds(p);
}

// dst_fbo: this will be used for rendering; possibly reallocating the whole
//          FBO, if the required parameters have changed
// w, h: required FBO target dimension, and also defines the target rectangle
//       used for rasterization
static void finish_pass_tex(struct gl_video *p, struct ra_tex **dst_tex,
                            int w, int h)
{
    if (!ra_tex_resize(p->ra, p->log, dst_tex, w, h, p->fbo_format)) {
        cleanup_binds(p);
        gl_sc_reset(p->sc);
        return;
    }

    // If RA_CAP_PARALLEL_COMPUTE is set, try to prefer compute shaders
    // over fragment shaders wherever possible.
    if (!p->pass_compute.active && (p->ra->caps & RA_CAP_PARALLEL_COMPUTE))
        pass_is_compute(p, 16, 16, true);

    if (p->pass_compute.active) {
        gl_sc_uniform_image2D_wo(p->sc, "out_image", *dst_tex);
        if (!p->pass_compute.directly_writes)
            GLSL(imageStore(out_image, ivec2(gl_GlobalInvocationID), color);)

        dispatch_compute(p, w, h, p->pass_compute);
        p->pass_compute = (struct compute_info){0};

        debug_check_gl(p, "after dispatching compute shader");
    } else {
        struct ra_fbo fbo = { .tex = *dst_tex, };
        finish_pass_fbo(p, fbo, true, &(struct mp_rect){0, 0, w, h});
    }
}

static const char *get_tex_swizzle(struct image *img)
{
    if (!img->tex)
        return "rgba";
    return img->tex->params.format->luminance_alpha ? "raaa" : "rgba";
}

// Copy a texture to the vec4 color, while increasing offset. Also applies
// the texture multiplier to the sampled color
static void copy_image(struct gl_video *p, int *offset, struct image img)
{
    int count = img.components;
    assert(*offset + count <= 4);

    int id = pass_bind(p, img);
    char src[5] = {0};
    char dst[5] = {0};
    const char *tex_fmt = get_tex_swizzle(&img);
    const char *dst_fmt = "rgba";
    for (int i = 0; i < count; i++) {
        src[i] = tex_fmt[i];
        dst[i] = dst_fmt[*offset + i];
    }

    if (img.tex && img.tex->params.format->ctype == RA_CTYPE_UINT) {
        uint64_t tex_max = 1ull << p->ra_format.component_bits;
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
    ra_tex_free(p->ra, &scaler->sep_fbo);
    ra_tex_free(p->ra, &scaler->lut);
    scaler->kernel = NULL;
    scaler->initialized = false;
}

static void hook_prelude(struct gl_video *p, const char *name, int id,
                         struct image img)
{
    GLSLHF("#define %s_raw texture%d\n", name, id);
    GLSLHF("#define %s_pos texcoord%d\n", name, id);
    GLSLHF("#define %s_size texture_size%d\n", name, id);
    GLSLHF("#define %s_rot texture_rot%d\n", name, id);
    GLSLHF("#define %s_pt pixel_size%d\n", name, id);
    GLSLHF("#define %s_map texmap%d\n", name, id);
    GLSLHF("#define %s_mul %f\n", name, img.multiplier);

    // Set up the sampling functions
    GLSLHF("#define %s_tex(pos) (%s_mul * vec4(texture(%s_raw, pos)).%s)\n",
           name, name, name, get_tex_swizzle(&img));

    // Since the extra matrix multiplication impacts performance,
    // skip it unless the texture was actually rotated
    if (gl_transform_eq(img.transform, identity_trans)) {
        GLSLHF("#define %s_texOff(off) %s_tex(%s_pos + %s_pt * vec2(off))\n",
               name, name, name, name);
    } else {
        GLSLHF("#define %s_texOff(off) "
                   "%s_tex(%s_pos + %s_rot * vec2(off)/%s_size)\n",
               name, name, name, name, name);
    }
}

static bool saved_img_find(struct gl_video *p, const char *name,
                           struct image *out)
{
    if (!name || !out)
        return false;

    for (int i = 0; i < p->num_saved_imgs; i++) {
        if (strcmp(p->saved_imgs[i].name, name) == 0) {
            *out = p->saved_imgs[i].img;
            return true;
        }
    }

    return false;
}

static void saved_img_store(struct gl_video *p, const char *name,
                            struct image img)
{
    assert(name);

    for (int i = 0; i < p->num_saved_imgs; i++) {
        if (strcmp(p->saved_imgs[i].name, name) == 0) {
            p->saved_imgs[i].img = img;
            return;
        }
    }

    MP_TARRAY_APPEND(p, p->saved_imgs, p->num_saved_imgs, (struct saved_img) {
        .name = name,
        .img = img
    });
}

static bool pass_hook_setup_binds(struct gl_video *p, const char *name,
                                  struct image img, struct tex_hook *hook)
{
    for (int t = 0; t < SHADER_MAX_BINDS; t++) {
        char *bind_name = (char *)hook->bind_tex[t];

        if (!bind_name)
            continue;

        // This is a special name that means "currently hooked texture"
        if (strcmp(bind_name, "HOOKED") == 0) {
            int id = pass_bind(p, img);
            hook_prelude(p, "HOOKED", id, img);
            hook_prelude(p, name, id, img);
            continue;
        }

        // BIND can also be used to load user-defined textures, in which
        // case we will directly load them as a uniform instead of
        // generating the hook_prelude boilerplate
        for (int u = 0; u < p->num_user_textures; u++) {
            struct gl_user_shader_tex *utex = &p->user_textures[u];
            if (bstr_equals0(utex->name, bind_name)) {
                gl_sc_uniform_texture(p->sc, bind_name, utex->tex);
                goto next_bind;
            }
        }

        struct image bind_img;
        if (!saved_img_find(p, bind_name, &bind_img)) {
            // Clean up texture bindings and move on to the next hook
            MP_TRACE(p, "Skipping hook on %s due to no texture named %s.\n",
                     name, bind_name);
            p->num_pass_imgs -= t;
            return false;
        }

        hook_prelude(p, bind_name, pass_bind(p, bind_img), bind_img);

next_bind: ;
    }

    return true;
}

static struct ra_tex **next_hook_tex(struct gl_video *p)
{
    if (p->idx_hook_textures == p->num_hook_textures)
        MP_TARRAY_APPEND(p, p->hook_textures, p->num_hook_textures, NULL);

    return &p->hook_textures[p->idx_hook_textures++];
}

// Process hooks for a plane, saving the result and returning a new image
// If 'trans' is NULL, the shader is forbidden from transforming img
static struct image pass_hook(struct gl_video *p, const char *name,
                              struct image img, struct gl_transform *trans)
{
    if (!name)
        return img;

    saved_img_store(p, name, img);

    MP_TRACE(p, "Running hooks for %s\n", name);
    for (int i = 0; i < p->num_tex_hooks; i++) {
        struct tex_hook *hook = &p->tex_hooks[i];

        // Figure out if this pass hooks this texture
        for (int h = 0; h < SHADER_MAX_HOOKS; h++) {
            if (hook->hook_tex[h] && strcmp(hook->hook_tex[h], name) == 0)
                goto found;
        }

        continue;

found:
        // Check the hook's condition
        if (hook->cond && !hook->cond(p, img, hook->priv)) {
            MP_TRACE(p, "Skipping hook on %s due to condition.\n", name);
            continue;
        }

        if (!pass_hook_setup_binds(p, name, img, hook))
            continue;

        // Run the actual hook. This generates a series of GLSL shader
        // instructions sufficient for drawing the hook's output
        struct gl_transform hook_off = identity_trans;
        hook->hook(p, img, &hook_off, hook->priv);

        int comps = hook->components ? hook->components : img.components;
        skip_unused(p, comps);

        // Compute the updated FBO dimensions and store the result
        struct mp_rect_f sz = {0, 0, img.w, img.h};
        gl_transform_rect(hook_off, &sz);
        int w = lroundf(fabs(sz.x1 - sz.x0));
        int h = lroundf(fabs(sz.y1 - sz.y0));

        struct ra_tex **tex = next_hook_tex(p);
        finish_pass_tex(p, tex, w, h);
        const char *store_name = hook->save_tex ? hook->save_tex : name;
        struct image saved_img = image_wrap(*tex, img.type, comps);

        // If the texture we're saving overwrites the "current" texture, also
        // update the tex parameter so that the future loop cycles will use the
        // updated values, and export the offset
        if (strcmp(store_name, name) == 0) {
            if (!trans && !gl_transform_eq(hook_off, identity_trans)) {
                MP_ERR(p, "Hook tried changing size of unscalable texture %s!\n",
                       name);
                return img;
            }

            img = saved_img;
            if (trans)
                gl_transform_trans(hook_off, trans);
        }

        saved_img_store(p, store_name, saved_img);
    }

    return img;
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

    for (int i = 0; i < p->num_tex_hooks; i++) {
        struct tex_hook *hook = &p->tex_hooks[i];

        for (int h = 0; h < SHADER_MAX_HOOKS; h++) {
            if (hook->hook_tex[h] && strcmp(hook->hook_tex[h], name) == 0)
                goto found;
        }

        for (int b = 0; b < SHADER_MAX_BINDS; b++) {
            if (hook->bind_tex[b] && strcmp(hook->bind_tex[b], name) == 0)
                goto found;
        }
    }

    // Nothing uses this texture, don't bother storing it
    return;

found: ;
    struct ra_tex **tex = next_hook_tex(p);
    finish_pass_tex(p, tex, p->texture_w, p->texture_h);
    struct image img = image_wrap(*tex, PLANE_RGB, p->components);
    img = pass_hook(p, name, img, tex_trans);
    copy_image(p, &(int){0}, img);
    p->texture_w = img.w;
    p->texture_h = img.h;
    p->components = img.components;
    pass_describe(p, "(remainder pass)");
}

static void load_shader(struct gl_video *p, struct bstr body)
{
    gl_sc_hadd_bstr(p->sc, body);
    gl_sc_uniform_dynamic(p->sc);
    gl_sc_uniform_f(p->sc, "random", (double)av_lfg_get(&p->lfg) / UINT32_MAX);
    gl_sc_uniform_dynamic(p->sc);
    gl_sc_uniform_i(p->sc, "frame", p->frames_uploaded);
    gl_sc_uniform_vec2(p->sc, "input_size",
                       (float[]){(p->src_rect.x1 - p->src_rect.x0) *
                                  p->texture_offset.m[0][0],
                                  (p->src_rect.y1 - p->src_rect.y0) *
                                  p->texture_offset.m[1][1]});
    gl_sc_uniform_vec2(p->sc, "target_size",
                       (float[]){p->dst_rect.x1 - p->dst_rect.x0,
                                 p->dst_rect.y1 - p->dst_rect.y0});
    gl_sc_uniform_vec2(p->sc, "tex_offset",
                       (float[]){p->src_rect.x0 * p->texture_offset.m[0][0] +
                                 p->texture_offset.t[0],
                                 p->src_rect.y0 * p->texture_offset.m[1][1] +
                                 p->texture_offset.t[1]});
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
           a.blur == b.blur &&
           a.taper == b.taper;
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

    if (conf->kernel.taper > 0.0)
        scaler->kernel->f.taper = conf->kernel.taper;
    if (conf->window.taper > 0.0)
        scaler->kernel->w.taper = conf->window.taper;

    if (scaler->kernel->f.resizable && conf->radius > 0.0)
        scaler->kernel->f.radius = conf->radius;

    scaler->kernel->clamp = conf->clamp;
    scaler->kernel->value_cutoff = conf->cutoff;

    scaler->insufficient = !mp_init_filter(scaler->kernel, sizes, scale_factor);

    int size = scaler->kernel->size;
    int num_components = size > 2 ? 4 : size;
    const struct ra_format *fmt = ra_find_float16_format(p->ra, num_components);
    assert(fmt);

    int width = (size + num_components - 1) / num_components; // round up
    int stride = width * num_components;
    assert(size <= stride);

    scaler->lut_size = 1 << p->opts.scaler_lut_size;

    float *weights = talloc_array(NULL, float, scaler->lut_size * stride);
    mp_compute_lut(scaler->kernel, scaler->lut_size, stride, weights);

    bool use_1d = scaler->kernel->polar && (p->ra->caps & RA_CAP_TEX_1D);

    struct ra_tex_params lut_params = {
        .dimensions = use_1d ? 1 : 2,
        .w = use_1d ? scaler->lut_size : width,
        .h = use_1d ? 1 : scaler->lut_size,
        .d = 1,
        .format = fmt,
        .render_src = true,
        .src_linear = true,
        .initial_data = weights,
    };
    scaler->lut = ra_tex_create(p->ra, &lut_params);

    talloc_free(weights);

    debug_check_gl(p, "after initializing scaler");
}

// Special helper for sampling from two separated stages
static void pass_sample_separated(struct gl_video *p, struct image src,
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
    GLSLF("// first pass\n");
    pass_sample_separated_gen(p->sc, scaler, 0, 1);
    GLSLF("color *= %f;\n", src.multiplier);
    finish_pass_tex(p, &scaler->sep_fbo, src.w, h);

    // Second pass (scale only in the x dir)
    src = image_wrap(scaler->sep_fbo, src.type, src.components);
    src.transform = t_x;
    pass_describe(p, "%s second pass", scaler->conf.kernel.name);
    sampler_prelude(p->sc, pass_bind(p, src));
    pass_sample_separated_gen(p->sc, scaler, 1, 0);
}

// Picks either the compute shader version or the regular sampler version
// depending on hardware support
static void pass_dispatch_sample_polar(struct gl_video *p, struct scaler *scaler,
                                       struct image img, int w, int h)
{
    uint64_t reqs = RA_CAP_COMPUTE;
    if ((p->ra->caps & reqs) != reqs)
        goto fallback;

    int bound = ceil(scaler->kernel->radius_cutoff);
    int offset = bound - 1; // padding top/left
    int padding = offset + bound; // total padding

    float ratiox = (float)w / img.w,
          ratioy = (float)h / img.h;

    // For performance we want to load at least as many pixels
    // horizontally as there are threads in a warp (32 for nvidia), as
    // well as enough to take advantage of shmem parallelism
    const int warp_size = 32, threads = 256;
    int bw = warp_size;
    int bh = threads / bw;

    // We need to sample everything from base_min to base_max, so make sure
    // we have enough room in shmem
    int iw = (int)ceil(bw / ratiox) + padding + 1,
        ih = (int)ceil(bh / ratioy) + padding + 1;

    int shmem_req = iw * ih * img.components * sizeof(float);
    if (shmem_req > p->ra->max_shmem)
        goto fallback;

    pass_is_compute(p, bw, bh, false);
    pass_compute_polar(p->sc, scaler, img.components, bw, bh, iw, ih);
    return;

fallback:
    // Fall back to regular polar shader when compute shaders are unsupported
    // or the kernel is too big for shmem
    pass_sample_polar(p->sc, scaler, img.components,
                      p->ra->caps & RA_CAP_GATHER);
}

// Sample from image, with the src rectangle given by it.
// The dst rectangle is implicit by what the caller will do next, but w and h
// must still be what is going to be used (to dimension FBOs correctly).
// This will write the scaled contents to the vec4 "color".
// The scaler unit is initialized by this function; in order to avoid cache
// thrashing, the scaler unit should usually use the same parameters.
static void pass_sample(struct gl_video *p, struct image img,
                        struct scaler *scaler, const struct scaler_config *conf,
                        double scale_factor, int w, int h)
{
    reinit_scaler(p, scaler, conf, scale_factor, filter_sizes);

    // Describe scaler
    const char *scaler_opt[] = {
        [SCALER_SCALE] = "scale",
        [SCALER_DSCALE] = "dscale",
        [SCALER_CSCALE] = "cscale",
        [SCALER_TSCALE] = "tscale",
    };

    pass_describe(p, "%s=%s (%s)", scaler_opt[scaler->index],
                  scaler->conf.kernel.name, plane_names[img.type]);

    bool is_separated = scaler->kernel && !scaler->kernel->polar;

    // Set up the transformation+prelude and bind the texture, for everything
    // other than separated scaling (which does this in the subfunction)
    if (!is_separated)
        sampler_prelude(p->sc, pass_bind(p, img));

    // Dispatch the scaler. They're all wildly different.
    const char *name = scaler->conf.kernel.name;
    if (strcmp(name, "bilinear") == 0) {
        GLSL(color = texture(tex, pos);)
    } else if (strcmp(name, "bicubic_fast") == 0) {
        pass_sample_bicubic_fast(p->sc);
    } else if (strcmp(name, "oversample") == 0) {
        pass_sample_oversample(p->sc, scaler, w, h);
    } else if (scaler->kernel && scaler->kernel->polar) {
        pass_dispatch_sample_polar(p, scaler, img, w, h);
    } else if (scaler->kernel) {
        pass_sample_separated(p, img, scaler, w, h);
    } else {
        // Should never happen
        abort();
    }

    // Apply any required multipliers. Separated scaling already does this in
    // its first stage
    if (!is_separated)
        GLSLF("color *= %f;\n", img.multiplier);

    // Micro-optimization: Avoid scaling unneeded channels
    skip_unused(p, img.components);
}

// Returns true if two images are semantically equivalent (same metadata)
static bool image_equiv(struct image a, struct image b)
{
    return a.type == b.type &&
           a.components == b.components &&
           a.multiplier == b.multiplier &&
           a.tex->params.format == b.tex->params.format &&
           a.tex->params.w == b.tex->params.w &&
           a.tex->params.h == b.tex->params.h &&
           a.w == b.w &&
           a.h == b.h &&
           gl_transform_eq(a.transform, b.transform);
}

static void deband_hook(struct gl_video *p, struct image img,
                        struct gl_transform *trans, void *priv)
{
    pass_describe(p, "debanding (%s)", plane_names[img.type]);
    pass_sample_deband(p->sc, p->opts.deband_opts, &p->lfg,
                       p->image_params.color.gamma);
}

static void unsharp_hook(struct gl_video *p, struct image img,
                         struct gl_transform *trans, void *priv)
{
    pass_describe(p, "unsharp masking");
    pass_sample_unsharp(p->sc, p->opts.unsharp);
}

struct szexp_ctx {
    struct gl_video *p;
    struct image img;
};

static bool szexp_lookup(void *priv, struct bstr var, float size[2])
{
    struct szexp_ctx *ctx = priv;
    struct gl_video *p = ctx->p;

    if (bstr_equals0(var, "NATIVE_CROPPED")) {
        size[0] = (p->src_rect.x1 - p->src_rect.x0) * p->texture_offset.m[0][0];
        size[1] = (p->src_rect.y1 - p->src_rect.y0) * p->texture_offset.m[1][1];
        return true;
    }

    // The size of OUTPUT is determined. It could be useful for certain
    // user shaders to skip passes.
    if (bstr_equals0(var, "OUTPUT")) {
        size[0] = p->dst_rect.x1 - p->dst_rect.x0;
        size[1] = p->dst_rect.y1 - p->dst_rect.y0;
        return true;
    }

    // HOOKED is a special case
    if (bstr_equals0(var, "HOOKED")) {
        size[0] = ctx->img.w;
        size[1] = ctx->img.h;
        return true;
    }

    for (int o = 0; o < p->num_saved_imgs; o++) {
        if (bstr_equals0(var, p->saved_imgs[o].name)) {
            size[0] = p->saved_imgs[o].img.w;
            size[1] = p->saved_imgs[o].img.h;
            return true;
        }
    }

    return false;
}

static bool user_hook_cond(struct gl_video *p, struct image img, void *priv)
{
    struct gl_user_shader_hook *shader = priv;
    assert(shader);

    float res = false;
    struct szexp_ctx ctx = {p, img};
    eval_szexpr(p->log, &ctx, szexp_lookup, shader->cond, &res);
    return res;
}

static void user_hook(struct gl_video *p, struct image img,
                      struct gl_transform *trans, void *priv)
{
    struct gl_user_shader_hook *shader = priv;
    assert(shader);
    load_shader(p, shader->pass_body);

    pass_describe(p, "user shader: %.*s (%s)", BSTR_P(shader->pass_desc),
                  plane_names[img.type]);

    if (shader->compute.active) {
        p->pass_compute = shader->compute;
        GLSLF("hook();\n");
    } else {
        GLSLF("color = hook();\n");
    }

    // Make sure we at least create a legal FBO on failure, since it's better
    // to do this and display an error message than just crash OpenGL
    float w = 1.0, h = 1.0;

    eval_szexpr(p->log, &(struct szexp_ctx){p, img}, szexp_lookup, shader->width, &w);
    eval_szexpr(p->log, &(struct szexp_ctx){p, img}, szexp_lookup, shader->height, &h);

    *trans = (struct gl_transform){{{w / img.w, 0}, {0, h / img.h}}};
    gl_transform_trans(shader->offset, trans);
}

static bool add_user_hook(void *priv, struct gl_user_shader_hook hook)
{
    struct gl_video *p = priv;
    struct gl_user_shader_hook *copy = talloc_ptrtype(p, copy);
    *copy = hook;

    struct tex_hook texhook = {
        .save_tex = bstrdup0(copy, hook.save_tex),
        .components = hook.components,
        .hook = user_hook,
        .cond = user_hook_cond,
        .priv = copy,
    };

    for (int h = 0; h < SHADER_MAX_HOOKS; h++)
        texhook.hook_tex[h] = bstrdup0(copy, hook.hook_tex[h]);
    for (int h = 0; h < SHADER_MAX_BINDS; h++)
        texhook.bind_tex[h] = bstrdup0(copy, hook.bind_tex[h]);

    MP_TARRAY_APPEND(p, p->tex_hooks, p->num_tex_hooks, texhook);
    return true;
}

static bool add_user_tex(void *priv, struct gl_user_shader_tex tex)
{
    struct gl_video *p = priv;

    tex.tex = ra_tex_create(p->ra, &tex.params);
    TA_FREEP(&tex.params.initial_data);

    if (!tex.tex)
        return false;

    MP_TARRAY_APPEND(p, p->user_textures, p->num_user_textures, tex);
    return true;
}

static void load_user_shaders(struct gl_video *p, char **shaders)
{
    if (!shaders)
        return;

    for (int n = 0; shaders[n] != NULL; n++) {
        struct bstr file = load_cached_file(p, shaders[n]);
        parse_user_shader(p->log, p->ra, file, p, add_user_hook, add_user_tex);
    }
}

static void gl_video_setup_hooks(struct gl_video *p)
{
    gl_video_reset_hooks(p);

    if (p->opts.deband) {
        MP_TARRAY_APPEND(p, p->tex_hooks, p->num_tex_hooks, (struct tex_hook) {
            .hook_tex = {"LUMA", "CHROMA", "RGB", "XYZ"},
            .bind_tex = {"HOOKED"},
            .hook = deband_hook,
        });
    }

    if (p->opts.unsharp != 0.0) {
        MP_TARRAY_APPEND(p, p->tex_hooks, p->num_tex_hooks, (struct tex_hook) {
            .hook_tex = {"MAIN"},
            .bind_tex = {"HOOKED"},
            .hook = unsharp_hook,
        });
    }

    load_user_shaders(p, p->opts.user_shaders);
}

// sample from video textures, set "color" variable to yuv value
static void pass_read_video(struct gl_video *p)
{
    struct image img[4];
    struct gl_transform offsets[4];
    pass_get_images(p, &p->image, img, offsets);

    // To keep the code as simple as possibly, we currently run all shader
    // stages even if they would be unnecessary (e.g. no hooks for a texture).
    // In the future, deferred image should optimize this away.

    // Merge semantically identical textures. This loop is done from back
    // to front so that merged textures end up in the right order while
    // simultaneously allowing us to skip unnecessary merges
    for (int n = 3; n >= 0; n--) {
        if (img[n].type == PLANE_NONE)
            continue;

        int first = n;
        int num = 0;

        for (int i = 0; i < n; i++) {
            if (image_equiv(img[n], img[i]) &&
                gl_transform_eq(offsets[n], offsets[i]))
            {
                GLSLF("// merging plane %d ...\n", i);
                copy_image(p, &num, img[i]);
                first = MPMIN(first, i);
                img[i] = (struct image){0};
            }
        }

        if (num > 0) {
            GLSLF("// merging plane %d ... into %d\n", n, first);
            copy_image(p, &num, img[n]);
            pass_describe(p, "merging planes");
            finish_pass_tex(p, &p->merge_tex[n], img[n].w, img[n].h);
            img[first] = image_wrap(p->merge_tex[n], img[n].type, num);
            img[n] = (struct image){0};
        }
    }

    // If any textures are still in integer format by this point, we need
    // to introduce an explicit conversion pass to avoid breaking hooks/scaling
    for (int n = 0; n < 4; n++) {
        if (img[n].tex && img[n].tex->params.format->ctype == RA_CTYPE_UINT) {
            GLSLF("// use_integer fix for plane %d\n", n);
            copy_image(p, &(int){0}, img[n]);
            pass_describe(p, "use_integer fix");
            finish_pass_tex(p, &p->integer_tex[n], img[n].w, img[n].h);
            img[n] = image_wrap(p->integer_tex[n], img[n].type,
                                img[n].components);
        }
    }

    // Dispatch the hooks for all of these textures, saving and perhaps
    // modifying them in the process
    for (int n = 0; n < 4; n++) {
        const char *name;
        switch (img[n].type) {
        case PLANE_RGB:    name = "RGB";    break;
        case PLANE_LUMA:   name = "LUMA";   break;
        case PLANE_CHROMA: name = "CHROMA"; break;
        case PLANE_ALPHA:  name = "ALPHA";  break;
        case PLANE_XYZ:    name = "XYZ";    break;
        default: continue;
        }

        img[n] = pass_hook(p, name, img[n], &offsets[n]);
    }

    // At this point all planes are finalized but they may not be at the
    // required size yet. Furthermore, they may have texture offsets that
    // require realignment. For lack of something better to do, we assume
    // the rgb/luma texture is the "reference" and scale everything else
    // to match.
    for (int n = 0; n < 4; n++) {
        switch (img[n].type) {
        case PLANE_RGB:
        case PLANE_XYZ:
        case PLANE_LUMA: break;
        default: continue;
        }

        p->texture_w = img[n].w;
        p->texture_h = img[n].h;
        p->texture_offset = offsets[n];
        break;
    }

    // Compute the reference rect
    struct mp_rect_f src = {0.0, 0.0, p->image_params.w, p->image_params.h};
    struct mp_rect_f ref = src;
    gl_transform_rect(p->texture_offset, &ref);

    // Explicitly scale all of the textures that don't match
    for (int n = 0; n < 4; n++) {
        if (img[n].type == PLANE_NONE)
            continue;

        // If the planes are aligned identically, we will end up with the
        // exact same source rectangle.
        struct mp_rect_f rect = src;
        gl_transform_rect(offsets[n], &rect);
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
            .m = {{(float)img[n].w / p->texture_w, 0.0},
                  {0.0, (float)img[n].h / p->texture_h}},
            .t = {-rect.x0, -rect.y0},
        };
        if (p->image_params.rotate % 180 == 90)
            MPSWAP(double, scale.m[0][0], scale.m[1][1]);

        gl_transform_trans(scale, &fix);

        // Since the texture transform is a function of the texture coordinates
        // to texture space, rather than the other way around, we have to
        // actually apply the *inverse* of this. Fortunately, calculating
        // the inverse is relatively easy here.
        fix.m[0][0] = 1.0 / fix.m[0][0];
        fix.m[1][1] = 1.0 / fix.m[1][1];
        fix.t[0] = fix.m[0][0] * -fix.t[0];
        fix.t[1] = fix.m[1][1] * -fix.t[1];
        gl_transform_trans(fix, &img[n].transform);

        int scaler_id = -1;
        const char *name = NULL;
        switch (img[n].type) {
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
            pass_sample(p, img[n], scaler, conf, 1.0, p->texture_w, p->texture_h);
            finish_pass_tex(p, &p->scale_tex[n], p->texture_w, p->texture_h);
            img[n] = image_wrap(p->scale_tex[n], img[n].type, img[n].components);
        }

        // Run any post-scaling hooks
        img[n] = pass_hook(p, name, img[n], NULL);
    }

    // All planes are of the same size and properly aligned at this point
    pass_describe(p, "combining planes");
    int coord = 0;
    for (int i = 0; i < 4; i++) {
        if (img[i].type != PLANE_NONE)
            copy_image(p, &coord, img[i]);
    }
    p->components = coord;
}

// Utility function that simply binds a texture and reads from it, without any
// transformations.
static void pass_read_tex(struct gl_video *p, struct ra_tex *tex)
{
    struct image img = image_wrap(tex, PLANE_RGB, p->components);
    copy_image(p, &(int){0}, img);
}

// yuv conversion, and any other conversions before main up/down-scaling
static void pass_convert_yuv(struct gl_video *p)
{
    struct gl_shader_cache *sc = p->sc;

    struct mp_csp_params cparams = MP_CSP_PARAMS_DEFAULTS;
    cparams.gray = p->is_gray;
    mp_csp_set_image_params(&cparams, &p->image_params);
    mp_csp_equalizer_state_get(p->video_eq, &cparams);
    p->user_gamma = 1.0 / (cparams.gamma * p->opts.gamma);

    pass_describe(p, "color conversion");

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
        GLSL(color.rgb = mix(color.rgb * vec3(1.0/4.5),
                             pow((color.rgb + vec3(0.0993))*vec3(1.0/1.0993),
                                 vec3(1.0/0.45)),
                             lessThanEqual(vec3(0.08145), color.rgb));)
        // Calculate the green channel from the expanded RYcB
        // The BT.2020 specification says Yc = 0.2627*R + 0.6780*G + 0.0593*B
        GLSL(color.g = (color.g - 0.2627*color.r - 0.0593*color.b)*1.0/0.6780;)
        // Recompress to receive the R'G'B' result, same as other systems
        GLSL(color.rgb = mix(color.rgb * vec3(4.5),
                             vec3(1.0993) * pow(color.rgb, vec3(0.45)) - vec3(0.0993),
                             lessThanEqual(vec3(0.0181), color.rgb));)
    }

    p->components = 3;
    if (!p->has_alpha || p->opts.alpha_mode == ALPHA_NO) {
        GLSL(color.a = 1.0;)
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
    bool use_linear = p->opts.linear_scaling || p->opts.sigmoid_upscaling;

    // Linear light downscaling results in nasty artifacts for HDR curves due
    // to the potentially extreme brightness differences severely compounding
    // any ringing. So just scale in gamma light instead.
    if (mp_trc_is_hdr(p->image_params.color.gamma) && downscaling)
        use_linear = false;

    if (use_linear) {
        p->use_linear = true;
        pass_linearize(p->sc, p->image_params.color.gamma);
        pass_opt_hook_point(p, "LINEAR", NULL);
    }

    bool use_sigmoid = use_linear && p->opts.sigmoid_upscaling && upscaling;
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
        GLSLF("color.rgb = %f - log(1.0/(color.rgb * %f + %f) - 1.0) * 1.0/%f;\n",
                sig_center, sig_scale, sig_offset, sig_slope);
        pass_opt_hook_point(p, "SIGMOID", NULL);
    }

    pass_opt_hook_point(p, "PREKERNEL", NULL);

    int vp_w = p->dst_rect.x1 - p->dst_rect.x0;
    int vp_h = p->dst_rect.y1 - p->dst_rect.y0;
    struct gl_transform transform;
    compute_src_transform(p, &transform);

    GLSLF("// main scaling\n");
    finish_pass_tex(p, &p->indirect_tex, p->texture_w, p->texture_h);
    struct image src = image_wrap(p->indirect_tex, PLANE_RGB, p->components);
    gl_transform_trans(transform, &src.transform);
    pass_sample(p, src, scaler, &scaler_conf, scale_factor, vp_w, vp_h);

    // Changes the texture size to display size after main scaler.
    p->texture_w = vp_w;
    p->texture_h = vp_h;

    pass_opt_hook_point(p, "POSTKERNEL", NULL);

    GLSLF("// scaler post-conversion\n");
    if (use_sigmoid) {
        // Inverse of the transformation above
        GLSLF("color.rgb = (1.0/(1.0 + exp(%f * (%f - color.rgb))) - %f) * 1.0/%f;\n",
                sig_slope, sig_center, sig_offset, sig_scale);
    }
}

// Adapts the colors to the right output color space. (Final pass during
// rendering)
// If OSD is true, ignore any changes that may have been made to the video
// by previous passes (i.e. linear scaling)
static void pass_colormanage(struct gl_video *p, struct mp_colorspace src, bool osd)
{
    struct ra *ra = p->ra;

    // Figure out the target color space from the options, or auto-guess if
    // none were set
    struct mp_colorspace dst = {
        .gamma = p->opts.target_trc,
        .primaries = p->opts.target_prim,
        .light = MP_CSP_LIGHT_DISPLAY,
        .sig_peak = p->opts.target_peak / MP_REF_WHITE,
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
        if (mp_trc_is_hdr(trc_orig))
            trc_orig = MP_CSP_TRC_GAMMA22;

        if (gl_video_get_lut3d(p, prim_orig, trc_orig)) {
            dst.primaries = prim_orig;
            dst.gamma = trc_orig;
            assert(dst.primaries && dst.gamma);
        }
    }

    if (dst.primaries == MP_CSP_PRIM_AUTO) {
        // The vast majority of people are on sRGB or BT.709 displays, so pick
        // this as the default output color space.
        dst.primaries = MP_CSP_PRIM_BT_709;

        if (src.primaries == MP_CSP_PRIM_BT_601_525 ||
            src.primaries == MP_CSP_PRIM_BT_601_625)
        {
            // Since we auto-pick BT.601 and BT.709 based on the dimensions,
            // combined with the fact that they're very similar to begin with,
            // and to avoid confusing the average user, just don't adapt BT.601
            // content automatically at all.
            dst.primaries = src.primaries;
        }
    }

    if (dst.gamma == MP_CSP_TRC_AUTO) {
        // Most people seem to complain when the image is darker or brighter
        // than what they're "used to", so just avoid changing the gamma
        // altogether by default. The only exceptions to this rule apply to
        // very unusual TRCs, which even hardcode technoluddites would probably
        // not enjoy viewing unaltered.
        dst.gamma = src.gamma;

        // Avoid outputting linear light or HDR content "by default". For these
        // just pick gamma 2.2 as a default, since it's a good estimate for
        // the response of typical displays
        if (dst.gamma == MP_CSP_TRC_LINEAR || mp_trc_is_hdr(dst.gamma))
            dst.gamma = MP_CSP_TRC_GAMMA22;
    }

    // If there's no specific signal peak known for the output display, infer
    // it from the chosen transfer function
    if (!dst.sig_peak)
        dst.sig_peak = mp_trc_nom_peak(dst.gamma);

    bool detect_peak = p->opts.compute_hdr_peak >= 0 && mp_trc_is_hdr(src.gamma);
    if (detect_peak && !p->hdr_peak_ssbo) {
        struct {
            uint32_t counter;
            uint32_t frame_idx;
            uint32_t frame_num;
            uint32_t frame_max[PEAK_DETECT_FRAMES+1];
            uint32_t frame_sum[PEAK_DETECT_FRAMES+1];
            uint32_t total_max;
            uint32_t total_sum;
        } peak_ssbo = {0};

        struct ra_buf_params params = {
            .type = RA_BUF_TYPE_SHADER_STORAGE,
            .size = sizeof(peak_ssbo),
            .initial_data = &peak_ssbo,
        };

        p->hdr_peak_ssbo = ra_buf_create(ra, &params);
        if (!p->hdr_peak_ssbo) {
            MP_WARN(p, "Failed to create HDR peak detection SSBO, disabling.\n");
            detect_peak = false;
            p->opts.compute_hdr_peak = -1;
        }
    }

    if (detect_peak) {
        pass_describe(p, "detect HDR peak");
        pass_is_compute(p, 8, 8, true); // 8x8 is good for performance
        gl_sc_ssbo(p->sc, "PeakDetect", p->hdr_peak_ssbo,
            "uint counter;"
            "uint frame_idx;"
            "uint frame_num;"
            "uint frame_max[%d];"
            "uint frame_avg[%d];"
            "uint total_max;"
            "uint total_avg;",
            PEAK_DETECT_FRAMES + 1,
            PEAK_DETECT_FRAMES + 1
        );
    }

    // Adapt from src to dst as necessary
    pass_color_map(p->sc, src, dst, p->opts.tone_mapping,
                   p->opts.tone_mapping_param, p->opts.tone_mapping_desat,
                   detect_peak, p->opts.gamut_warning, p->use_linear && !osd);

    if (p->use_lut_3d) {
        gl_sc_uniform_texture(p->sc, "lut_3d", p->lut_3d_texture);
        GLSL(vec3 cpos;)
        for (int i = 0; i < 3; i++)
            GLSLF("cpos[%d] = LUT_POS(color[%d], %d.0);\n", i, i, p->lut_3d_size[i]);
        GLSL(color.rgb = tex3D(lut_3d, cpos).rgb;)
    }
}

void gl_video_set_fb_depth(struct gl_video *p, int fb_depth)
{
    p->fb_depth = fb_depth;
}

static void pass_dither(struct gl_video *p)
{
    // Assume 8 bits per component if unknown.
    int dst_depth = p->fb_depth > 0 ? p->fb_depth : 8;
    if (p->opts.dither_depth > 0)
        dst_depth = p->opts.dither_depth;

    if (p->opts.dither_depth < 0 || p->opts.dither_algo == DITHER_NONE)
        return;

    if (!p->dither_texture) {
        MP_VERBOSE(p, "Dither to %d.\n", dst_depth);

        int tex_size = 0;
        void *tex_data = NULL;
        const struct ra_format *fmt = NULL;
        void *temp = NULL;

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
            fmt = ra_find_unorm_format(p->ra, 2, 1);
            if (!fmt)
                fmt = ra_find_float16_format(p->ra, 1);
            if (fmt) {
                tex_size = size;
                tex_data = p->last_dither_matrix;
                if (fmt->ctype == RA_CTYPE_UNORM) {
                    uint16_t *t = temp = talloc_array(NULL, uint16_t, size * size);
                    for (int n = 0; n < size * size; n++)
                        t[n] = p->last_dither_matrix[n] * UINT16_MAX;
                    tex_data = t;
                }
            } else {
                MP_VERBOSE(p, "GL too old. Falling back to ordered dither.\n");
                p->opts.dither_algo = DITHER_ORDERED;
            }
        }

        if (p->opts.dither_algo == DITHER_ORDERED) {
            temp = talloc_array(NULL, char, 8 * 8);
            mp_make_ordered_dither_matrix(temp, 8);

            fmt = ra_find_unorm_format(p->ra, 1, 1);
            tex_size = 8;
            tex_data = temp;
        }

        struct ra_tex_params params = {
            .dimensions = 2,
            .w = tex_size,
            .h = tex_size,
            .d = 1,
            .format = fmt,
            .render_src = true,
            .src_repeat = true,
            .initial_data = tex_data,
        };
        p->dither_texture = ra_tex_create(p->ra, &params);

        debug_check_gl(p, "dither setup");

        talloc_free(temp);
    }

    GLSLF("// dithering\n");

    // This defines how many bits are considered significant for output on
    // screen. The superfluous bits will be used for rounding according to the
    // dither matrix. The precision of the source implicitly decides how many
    // dither patterns can be visible.
    int dither_quantization = (1 << dst_depth) - 1;
    int dither_size = p->dither_texture->params.w;

    gl_sc_uniform_texture(p->sc, "dither", p->dither_texture);

    GLSLF("vec2 dither_pos = gl_FragCoord.xy * 1.0/%d.0;\n", dither_size);

    if (p->opts.temporal_dither) {
        int phase = (p->frames_rendered / p->opts.temporal_dither_period) % 8u;
        float r = phase * (M_PI / 2); // rotate
        float m = phase < 4 ? 1 : -1; // mirror

        float matrix[2][2] = {{cos(r),     -sin(r)    },
                              {sin(r) * m,  cos(r) * m}};
        gl_sc_uniform_dynamic(p->sc);
        gl_sc_uniform_mat2(p->sc, "dither_trafo", true, &matrix[0][0]);

        GLSL(dither_pos = dither_trafo * dither_pos;)
    }

    GLSL(float dither_value = texture(dither, dither_pos).r;)
    GLSLF("color = floor(color * %d.0 + dither_value + 0.5 / %d.0) * 1.0/%d.0;\n",
          dither_quantization, dither_size * dither_size, dither_quantization);
}

// Draws the OSD, in scene-referred colors.. If cms is true, subtitles are
// instead adapted to the display's gamut.
static void pass_draw_osd(struct gl_video *p, int draw_flags, double pts,
                          struct mp_osd_res rect, struct ra_fbo fbo, bool cms)
{
    if ((draw_flags & OSD_DRAW_SUB_ONLY) && (draw_flags & OSD_DRAW_OSD_ONLY))
        return;

    mpgl_osd_generate(p->osd, rect, pts, p->image_params.stereo3d, draw_flags);

    timer_pool_start(p->osd_timer);
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        // (This returns false if this part is empty with nothing to draw.)
        if (!mpgl_osd_draw_prepare(p->osd, n, p->sc))
            continue;
        // When subtitles need to be color managed, assume they're in sRGB
        // (for lack of anything saner to do)
        if (cms) {
            static const struct mp_colorspace csp_srgb = {
                .primaries = MP_CSP_PRIM_BT_709,
                .gamma = MP_CSP_TRC_SRGB,
                .light = MP_CSP_LIGHT_DISPLAY,
            };

            pass_colormanage(p, csp_srgb, true);
        }
        mpgl_osd_draw_finish(p->osd, n, p->sc, fbo);
    }

    timer_pool_stop(p->osd_timer);
    pass_describe(p, "drawing osd");
    pass_record(p, timer_pool_measure(p->osd_timer));
}

static float chroma_realign(int size, int pixel)
{
    return size / (float)chroma_upsize(size, pixel);
}

// Minimal rendering code path, for GLES or OpenGL 2.1 without proper FBOs.
static void pass_render_frame_dumb(struct gl_video *p)
{
    struct image img[4];
    struct gl_transform off[4];
    pass_get_images(p, &p->image, img, off);

    struct gl_transform transform;
    compute_src_transform(p, &transform);

    int index = 0;
    for (int i = 0; i < p->plane_count; i++) {
        int cw = img[i].type == PLANE_CHROMA ? p->ra_format.chroma_w : 1;
        int ch = img[i].type == PLANE_CHROMA ? p->ra_format.chroma_h : 1;
        if (p->image_params.rotate % 180 == 90)
            MPSWAP(int, cw, ch);

        struct gl_transform t = transform;
        t.m[0][0] *= chroma_realign(p->texture_w, cw);
        t.m[1][1] *= chroma_realign(p->texture_h, ch);

        t.t[0] /= cw;
        t.t[1] /= ch;

        t.t[0] += off[i].t[0];
        t.t[1] += off[i].t[1];

        gl_transform_trans(img[i].transform, &t);
        img[i].transform = t;

        copy_image(p, &index, img[i]);
    }

    pass_convert_yuv(p);
}

// The main rendering function, takes care of everything up to and including
// upscaling. p->image is rendered.
// flags: bit set of RENDER_FRAME_* flags
static bool pass_render_frame(struct gl_video *p, struct mp_image *mpi,
                              uint64_t id, int flags)
{
    // initialize the texture parameters and temporary variables
    p->texture_w = p->image_params.w;
    p->texture_h = p->image_params.h;
    p->texture_offset = identity_trans;
    p->components = 0;
    p->num_saved_imgs = 0;
    p->idx_hook_textures = 0;
    p->use_linear = false;

    // try uploading the frame
    if (!pass_upload_image(p, mpi, id))
        return false;

    if (p->image_params.rotate % 180 == 90)
        MPSWAP(int, p->texture_w, p->texture_h);

    if (p->dumb_mode)
        return true;

    pass_read_video(p);
    pass_opt_hook_point(p, "NATIVE", &p->texture_offset);
    pass_convert_yuv(p);
    pass_opt_hook_point(p, "MAINPRESUB", &p->texture_offset);

    // For subtitles
    double vpts = p->image.mpi->pts;
    if (vpts == MP_NOPTS_VALUE)
        vpts = p->osd_pts;

    if (p->osd && p->opts.blend_subs == BLEND_SUBS_VIDEO &&
        (flags & RENDER_FRAME_SUBS))
    {
        double scale[2];
        get_scale_factors(p, false, scale);
        struct mp_osd_res rect = {
            .w = p->texture_w, .h = p->texture_h,
            .display_par = scale[1] / scale[0], // counter compensate scaling
        };
        finish_pass_tex(p, &p->blend_subs_tex, rect.w, rect.h);
        struct ra_fbo fbo = { p->blend_subs_tex };
        pass_draw_osd(p, OSD_DRAW_SUB_ONLY, vpts, rect, fbo, false);
        pass_read_tex(p, p->blend_subs_tex);
        pass_describe(p, "blend subs video");
    }
    pass_opt_hook_point(p, "MAIN", &p->texture_offset);

    pass_scale_main(p);

    int vp_w = p->dst_rect.x1 - p->dst_rect.x0,
        vp_h = p->dst_rect.y1 - p->dst_rect.y0;
    if (p->osd && p->opts.blend_subs == BLEND_SUBS_YES &&
        (flags & RENDER_FRAME_SUBS))
    {
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
        finish_pass_tex(p, &p->blend_subs_tex, p->texture_w, p->texture_h);
        struct ra_fbo fbo = { p->blend_subs_tex };
        pass_draw_osd(p, OSD_DRAW_SUB_ONLY, vpts, rect, fbo, false);
        pass_read_tex(p, p->blend_subs_tex);
        pass_describe(p, "blend subs");
    }

    pass_opt_hook_point(p, "SCALED", NULL);

    return true;
}

static void pass_draw_to_screen(struct gl_video *p, struct ra_fbo fbo)
{
    if (p->dumb_mode)
        pass_render_frame_dumb(p);

    // Adjust the overall gamma before drawing to screen
    if (p->user_gamma != 1) {
        gl_sc_uniform_f(p->sc, "user_gamma", p->user_gamma);
        GLSL(color.rgb = clamp(color.rgb, 0.0, 1.0);)
        GLSL(color.rgb = pow(color.rgb, vec3(user_gamma));)
    }

    pass_colormanage(p, p->image_params.color, false);

    // Since finish_pass_fbo doesn't work with compute shaders, and neither
    // does the checkerboard/dither code, we may need an indirection via
    // p->screen_tex here.
    if (p->pass_compute.active) {
        int o_w = p->dst_rect.x1 - p->dst_rect.x0,
            o_h = p->dst_rect.y1 - p->dst_rect.y0;
        finish_pass_tex(p, &p->screen_tex, o_w, o_h);
        struct image tmp = image_wrap(p->screen_tex, PLANE_RGB, p->components);
        copy_image(p, &(int){0}, tmp);
    }

    if (p->has_alpha){
        if (p->opts.alpha_mode == ALPHA_BLEND_TILES) {
            // Draw checkerboard pattern to indicate transparency
            GLSLF("// transparency checkerboard\n");
            GLSL(bvec2 tile = lessThan(fract(gl_FragCoord.xy * 1.0/32.0), vec2(0.5));)
            GLSL(vec3 background = vec3(tile.x == tile.y ? 0.93 : 0.87);)
            GLSL(color.rgb += background.rgb * (1.0 - color.a);)
            GLSL(color.a = 1.0;)
        } else if (p->opts.alpha_mode == ALPHA_BLEND) {
            // Blend into background color (usually black)
            struct m_color c = p->opts.background;
            GLSLF("vec4 background = vec4(%f, %f, %f, %f);\n",
                  c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
            GLSL(color.rgb += background.rgb * (1.0 - color.a);)
            GLSL(color.a = background.a;)
        }
    }

    pass_opt_hook_point(p, "OUTPUT", NULL);

    pass_dither(p);
    pass_describe(p, "output to screen");
    finish_pass_fbo(p, fbo, false, &p->dst_rect);
}

// flags: bit set of RENDER_FRAME_* flags
static bool update_surface(struct gl_video *p, struct mp_image *mpi,
                           uint64_t id, struct surface *surf, int flags)
{
    int vp_w = p->dst_rect.x1 - p->dst_rect.x0,
        vp_h = p->dst_rect.y1 - p->dst_rect.y0;

    pass_info_reset(p, false);
    if (!pass_render_frame(p, mpi, id, flags))
        return false;

    // Frame blending should always be done in linear light to preserve the
    // overall brightness, otherwise this will result in flashing dark frames
    // because mixing in compressed light artificially darkens the results
    if (!p->use_linear) {
        p->use_linear = true;
        pass_linearize(p->sc, p->image_params.color.gamma);
    }

    finish_pass_tex(p, &surf->tex, vp_w, vp_h);
    surf->id  = id;
    surf->pts = mpi->pts;
    return true;
}

// Draws an interpolate frame to fbo, based on the frame timing in t
// flags: bit set of RENDER_FRAME_* flags
static void gl_video_interpolate_frame(struct gl_video *p, struct vo_frame *t,
                                       struct ra_fbo fbo, int flags)
{
    bool is_new = false;

    // Reset the queue completely if this is a still image, to avoid any
    // interpolation artifacts from surrounding frames when unpausing or
    // framestepping
    if (t->still)
        gl_video_reset_surfaces(p);

    // First of all, figure out if we have a frame available at all, and draw
    // it manually + reset the queue if not
    if (p->surfaces[p->surface_now].id == 0) {
        struct surface *now = &p->surfaces[p->surface_now];
        if (!update_surface(p, t->current, t->frame_id, now, flags))
            return;
        p->surface_idx = p->surface_now;
        is_new = true;
    }

    // Find the right frame for this instant
    if (t->current) {
        int next = surface_wrap(p->surface_now + 1);
        while (p->surfaces[next].id &&
               p->surfaces[next].id > p->surfaces[p->surface_now].id &&
               p->surfaces[p->surface_now].id < t->frame_id)
        {
            p->surface_now = next;
            next = surface_wrap(next + 1);
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
    }

    int radius = size/2;
    int surface_now = p->surface_now;
    int surface_bse = surface_wrap(surface_now - (radius-1));
    int surface_end = surface_wrap(surface_now + radius);
    assert(surface_wrap(surface_bse + size-1) == surface_end);

    // Render new frames while there's room in the queue. Note that technically,
    // this should be done before the step where we find the right frame, but
    // it only barely matters at the very beginning of playback, and this way
    // makes the code much more linear.
    int surface_dst = surface_wrap(p->surface_idx + 1);
    for (int i = 0; i < t->num_frames; i++) {
        // Avoid overwriting data we might still need
        if (surface_dst == surface_bse - 1)
            break;

        struct mp_image *f = t->frames[i];
        uint64_t f_id = t->frame_id + i;
        if (!mp_image_params_equal(&f->params, &p->real_image_params))
            continue;

        if (f_id > p->surfaces[p->surface_idx].id) {
            struct surface *dst = &p->surfaces[surface_dst];
            if (!update_surface(p, f, f_id, dst, flags))
                return;
            p->surface_idx = surface_dst;
            surface_dst = surface_wrap(surface_dst + 1);
            is_new = true;
        }
    }

    // Figure out whether the queue is "valid". A queue is invalid if the
    // frames' PTS is not monotonically increasing. Anything else is invalid,
    // so avoid blending incorrect data and just draw the latest frame as-is.
    // Possible causes for failure of this condition include seeks, pausing,
    // end of playback or start of playback.
    bool valid = true;
    for (int i = surface_bse, ii; valid && i != surface_end; i = ii) {
        ii = surface_wrap(i + 1);
        if (p->surfaces[i].id == 0 || p->surfaces[ii].id == 0) {
            valid = false;
        } else if (p->surfaces[ii].id < p->surfaces[i].id) {
            valid = false;
            MP_DBG(p, "interpolation queue underrun\n");
        }
    }

    // Update OSD PTS to synchronize subtitles with the displayed frame
    p->osd_pts = p->surfaces[surface_now].pts;

    // Finally, draw the right mix of frames to the screen.
    if (!is_new)
        pass_info_reset(p, true);
    pass_describe(p, "interpolation");
    if (!valid || t->still) {
        // surface_now is guaranteed to be valid, so we can safely use it.
        pass_read_tex(p, p->surfaces[surface_now].tex);
        p->is_interpolated = false;
    } else {
        double mix = t->vsync_offset / t->ideal_frame_duration;
        // The scaler code always wants the fcoord to be between 0 and 1,
        // so we try to adjust by using the previous set of N frames instead
        // (which requires some extra checking to make sure it's valid)
        if (mix < 0.0) {
            int prev = surface_wrap(surface_bse - 1);
            if (p->surfaces[prev].id != 0 &&
                p->surfaces[prev].id < p->surfaces[surface_bse].id)
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
            gl_sc_uniform_dynamic(p->sc);
            gl_sc_uniform_f(p->sc, "inter_coeff", mix);
            GLSL(color = mix(texture(texture0, texcoord0),
                             texture(texture1, texcoord1),
                             inter_coeff);)
        } else {
            gl_sc_uniform_dynamic(p->sc);
            gl_sc_uniform_f(p->sc, "fcoord", mix);
            pass_sample_separated_gen(p->sc, tscale, 0, 0);
        }

        // Load all the required frames
        for (int i = 0; i < size; i++) {
            struct image img =
                image_wrap(p->surfaces[surface_wrap(surface_bse+i)].tex,
                           PLANE_RGB, p->components);
            // Since the code in pass_sample_separated currently assumes
            // the textures are bound in-order and starting at 0, we just
            // assert to make sure this is the case (which it should always be)
            int id = pass_bind(p, img);
            assert(id == i);
        }

        MP_TRACE(p, "inter frame dur: %f vsync: %f, mix: %f\n",
                 t->ideal_frame_duration, t->vsync_interval, mix);
        p->is_interpolated = true;
    }
    pass_draw_to_screen(p, fbo);

    p->frames_drawn += 1;
}

void gl_video_render_frame(struct gl_video *p, struct vo_frame *frame,
                           struct ra_fbo fbo, int flags)
{
    gl_video_update_options(p);

    struct mp_rect target_rc = {0, 0, fbo.tex->params.w, fbo.tex->params.h};

    p->broken_frame = false;

    bool has_frame = !!frame->current;

    if (!has_frame || !mp_rect_equals(&p->dst_rect, &target_rc)) {
        struct m_color c = p->clear_color;
        float color[4] = {c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0};
        p->ra->fns->clear(p->ra, fbo.tex, color, &target_rc);
    }

    if (p->hwdec_overlay) {
        if (has_frame) {
            float *color = p->hwdec_overlay->overlay_colorkey;
            p->ra->fns->clear(p->ra, fbo.tex, color, &p->dst_rect);
        }

        p->hwdec_overlay->driver->overlay_frame(p->hwdec_overlay, frame->current,
                                                &p->src_rect, &p->dst_rect,
                                                frame->frame_id != p->image.id);

        if (frame->current)
            p->osd_pts = frame->current->pts;

        // Disable GL rendering
        has_frame = false;
    }

    if (has_frame) {
        bool interpolate = p->opts.interpolation && frame->display_synced &&
                           (p->frames_drawn || !frame->still);
        if (interpolate) {
            double ratio = frame->ideal_frame_duration / frame->vsync_interval;
            if (fabs(ratio - 1.0) < p->opts.interpolation_threshold)
                interpolate = false;
        }

        if (interpolate) {
            gl_video_interpolate_frame(p, frame, fbo, flags);
        } else {
            bool is_new = frame->frame_id != p->image.id;

            // Redrawing a frame might update subtitles.
            if (frame->still && p->opts.blend_subs)
                is_new = true;

            if (is_new || !p->output_tex_valid) {
                p->output_tex_valid = false;

                pass_info_reset(p, !is_new);
                if (!pass_render_frame(p, frame->current, frame->frame_id, flags))
                    goto done;

                // For the non-interpolation case, we draw to a single "cache"
                // texture to speed up subsequent re-draws (if any exist)
                struct ra_fbo dest_fbo = fbo;
                if (frame->num_vsyncs > 1 && frame->display_synced &&
                    !p->dumb_mode && (p->ra->caps & RA_CAP_BLIT) &&
                    fbo.tex->params.blit_dst)
                {
                    // Attempt to use the same format as the destination FBO
                    // if possible. Some RAs use a wrapped dummy format here,
                    // so fall back to the fbo_format in that case.
                    const struct ra_format *fmt = fbo.tex->params.format;
                    if (fmt->dummy_format)
                        fmt = p->fbo_format;
                    bool r = ra_tex_resize(p->ra, p->log, &p->output_tex,
                                           fbo.tex->params.w, fbo.tex->params.h,
                                           fmt);
                    if (r) {
                        dest_fbo = (struct ra_fbo) { p->output_tex };
                        p->output_tex_valid = true;
                    }
                }
                pass_draw_to_screen(p, dest_fbo);
            }

            // "output tex valid" and "output tex needed" are equivalent
            if (p->output_tex_valid && fbo.tex->params.blit_dst) {
                pass_info_reset(p, true);
                pass_describe(p, "redraw cached frame");
                struct mp_rect src = p->dst_rect;
                struct mp_rect dst = src;
                if (fbo.flip) {
                    dst.y0 = fbo.tex->params.h - src.y0;
                    dst.y1 = fbo.tex->params.h - src.y1;
                }
                timer_pool_start(p->blit_timer);
                p->ra->fns->blit(p->ra, fbo.tex, p->output_tex, &dst, &src);
                timer_pool_stop(p->blit_timer);
                pass_record(p, timer_pool_measure(p->blit_timer));
            }
        }
    }

done:

    debug_check_gl(p, "after video rendering");

    if (p->osd && (flags & (RENDER_FRAME_SUBS | RENDER_FRAME_OSD))) {
        // If we haven't actually drawn anything so far, then we technically
        // need to consider this the start of a new pass. Let's call it a
        // redraw just because, since it's basically a blank frame anyway
        if (!has_frame)
            pass_info_reset(p, true);

        int osd_flags = p->opts.blend_subs ? OSD_DRAW_OSD_ONLY : 0;
        if (!(flags & RENDER_FRAME_SUBS))
            osd_flags |= OSD_DRAW_OSD_ONLY;
        if (!(flags & RENDER_FRAME_OSD))
            osd_flags |= OSD_DRAW_SUB_ONLY;

        pass_draw_osd(p, osd_flags, p->osd_pts, p->osd_rect, fbo, true);
        debug_check_gl(p, "after OSD rendering");
    }

    p->broken_frame |= gl_sc_error_state(p->sc);
    if (p->broken_frame) {
        // Make the screen solid blue to make it visually clear that an
        // error has occurred
        float color[4] = {0.0, 0.05, 0.5, 1.0};
        p->ra->fns->clear(p->ra, fbo.tex, color, &target_rc);
    }

    p->frames_rendered++;
    pass_report_performance(p);
}

void gl_video_screenshot(struct gl_video *p, struct vo_frame *frame,
                         struct voctrl_screenshot *args)
{
    if (!p->ra->fns->tex_download)
        return;

    bool ok = false;
    struct mp_image *res = NULL;
    struct ra_tex *target = NULL;
    struct mp_rect old_src = p->src_rect;
    struct mp_rect old_dst = p->dst_rect;
    struct mp_osd_res old_osd = p->osd_rect;
    struct vo_frame *nframe = vo_frame_ref(frame);

    // Disable interpolation and such.
    nframe->redraw = true;
    nframe->repeat = false;
    nframe->still = true;
    nframe->pts = 0;
    nframe->duration = -1;

    if (!args->scaled) {
        int w, h;
        mp_image_params_get_dsize(&p->image_params, &w, &h);
        if (w < 1 || h < 1)
            return;

        if (p->image_params.rotate % 180 == 90)
            MPSWAP(int, w, h);

        struct mp_rect src = {0, 0, p->image_params.w, p->image_params.h};
        struct mp_rect dst = {0, 0, w, h};
        struct mp_osd_res osd = {.w = w, .h = h, .display_par = 1.0};
        gl_video_resize(p, &src, &dst, &osd);
    }

    gl_video_reset_surfaces(p);

    struct ra_tex_params params = {
        .dimensions = 2,
        .downloadable = true,
        .w = p->osd_rect.w,
        .h = p->osd_rect.h,
        .render_dst = true,
    };

    params.format = ra_find_unorm_format(p->ra, 1, 4);
    int mpfmt = IMGFMT_RGB0;
    if (args->high_bit_depth && p->ra_format.component_bits > 8) {
        const struct ra_format *fmt = ra_find_unorm_format(p->ra, 2, 4);
        if (fmt && fmt->renderable) {
            params.format = fmt;
            mpfmt = IMGFMT_RGBA64;
        }
    }

    if (!params.format || !params.format->renderable)
        goto done;
    target = ra_tex_create(p->ra, &params);
    if (!target)
        goto done;

    int flags = 0;
    if (args->subs)
        flags |= RENDER_FRAME_SUBS;
    if (args->osd)
        flags |= RENDER_FRAME_OSD;
    gl_video_render_frame(p, nframe, (struct ra_fbo){target}, flags);

    res = mp_image_alloc(mpfmt, params.w, params.h);
    if (!res)
        goto done;

    struct ra_tex_download_params download_params = {
        .tex = target,
        .dst = res->planes[0],
        .stride = res->stride[0],
    };
    if (!p->ra->fns->tex_download(p->ra, &download_params))
        goto done;

    if (p->broken_frame)
        goto done;

    ok = true;
done:
    talloc_free(nframe);
    ra_tex_free(p->ra, &target);
    gl_video_resize(p, &old_src, &old_dst, &old_osd);
    if (!ok)
        TA_FREEP(&res);
    args->res = res;
}

// Use this color instead of the global option.
void gl_video_set_clear_color(struct gl_video *p, struct m_color c)
{
    p->force_clear_color = true;
    p->clear_color = c;
}

void gl_video_set_osd_pts(struct gl_video *p, double pts)
{
    p->osd_pts = pts;
}

bool gl_video_check_osd_change(struct gl_video *p, struct mp_osd_res *res,
                               double pts)
{
    return p->osd ? mpgl_osd_check_change(p->osd, res, pts) : false;
}

void gl_video_resize(struct gl_video *p,
                     struct mp_rect *src, struct mp_rect *dst,
                     struct mp_osd_res *osd)
{
    if (mp_rect_equals(&p->src_rect, src) &&
        mp_rect_equals(&p->dst_rect, dst) &&
        osd_res_equals(p->osd_rect, *osd))
        return;

    p->src_rect = *src;
    p->dst_rect = *dst;
    p->osd_rect = *osd;

    gl_video_reset_surfaces(p);

    if (p->osd)
        mpgl_osd_resize(p->osd, p->osd_rect, p->image_params.stereo3d);
}

static void frame_perf_data(struct pass_info pass[], struct mp_frame_perf *out)
{
    for (int i = 0; i < VO_PASS_PERF_MAX; i++) {
        if (!pass[i].desc.len)
            break;
        out->perf[out->count] = pass[i].perf;
        out->desc[out->count] = pass[i].desc.start;
        out->count++;
    }
}

void gl_video_perfdata(struct gl_video *p, struct voctrl_performance_data *out)
{
    *out = (struct voctrl_performance_data){0};
    frame_perf_data(p->pass_fresh,  &out->fresh);
    frame_perf_data(p->pass_redraw, &out->redraw);
}

// This assumes nv12, with textures set to GL_NEAREST filtering.
static void reinterleave_vdpau(struct gl_video *p,
                               struct ra_tex *input[4], struct ra_tex *output[2])
{
    for (int n = 0; n < 2; n++) {
        struct ra_tex **tex = &p->vdpau_deinterleave_tex[n];
        // This is an array of the 2 to-merge planes.
        struct ra_tex **src = &input[n * 2];
        int w = src[0]->params.w;
        int h = src[0]->params.h;
        int ids[2];
        for (int t = 0; t < 2; t++) {
            ids[t] = pass_bind(p, (struct image){
                .tex = src[t],
                .multiplier = 1.0,
                .transform = identity_trans,
                .w = w,
                .h = h,
            });
        }

        pass_describe(p, "vdpau reinterleaving");
        GLSLF("color = fract(gl_FragCoord.y * 0.5) < 0.5\n");
        GLSLF("      ? texture(texture%d, texcoord%d)\n", ids[0], ids[0]);
        GLSLF("      : texture(texture%d, texcoord%d);", ids[1], ids[1]);

        int comps = n == 0 ? 1 : 2;
        const struct ra_format *fmt = ra_find_unorm_format(p->ra, 1, comps);
        ra_tex_resize(p->ra, p->log, tex, w, h * 2, fmt);
        struct ra_fbo fbo = { *tex };
        finish_pass_fbo(p, fbo, true, &(struct mp_rect){0, 0, w, h * 2});

        output[n] = *tex;
    }
}

// Returns false on failure.
static bool pass_upload_image(struct gl_video *p, struct mp_image *mpi, uint64_t id)
{
    struct video_image *vimg = &p->image;

    if (vimg->id == id)
        return true;

    unref_current_image(p);

    mpi = mp_image_new_ref(mpi);
    if (!mpi)
        goto error;

    vimg->mpi = mpi;
    vimg->id = id;
    p->osd_pts = mpi->pts;
    p->frames_uploaded++;

    if (p->hwdec_active) {
        // Hardware decoding

        if (!p->hwdec_mapper)
            goto error;

        pass_describe(p, "map frame (hwdec)");
        timer_pool_start(p->upload_timer);
        bool ok = ra_hwdec_mapper_map(p->hwdec_mapper, vimg->mpi) >= 0;
        timer_pool_stop(p->upload_timer);
        pass_record(p, timer_pool_measure(p->upload_timer));

        vimg->hwdec_mapped = true;
        if (ok) {
            struct mp_image layout = {0};
            mp_image_set_params(&layout, &p->image_params);
            struct ra_tex **tex = p->hwdec_mapper->tex;
            struct ra_tex *tmp[4] = {0};
            if (p->hwdec_mapper->vdpau_fields) {
                reinterleave_vdpau(p, tex, tmp);
                tex = tmp;
            }
            for (int n = 0; n < p->plane_count; n++) {
                vimg->planes[n] = (struct texplane){
                    .w = mp_image_plane_w(&layout, n),
                    .h = mp_image_plane_h(&layout, n),
                    .tex = tex[n],
                };
            }
        } else {
            MP_FATAL(p, "Mapping hardware decoded surface failed.\n");
            goto error;
        }
        return true;
    }

    // Software decoding
    assert(mpi->num_planes == p->plane_count);

    timer_pool_start(p->upload_timer);
    for (int n = 0; n < p->plane_count; n++) {
        struct texplane *plane = &vimg->planes[n];

        struct ra_tex_upload_params params = {
            .tex = plane->tex,
            .src = mpi->planes[n],
            .invalidate = true,
            .stride = mpi->stride[n],
        };

        plane->flipped = params.stride < 0;
        if (plane->flipped) {
            int h = mp_image_plane_h(mpi, n);
            params.src = (char *)params.src + (h - 1) * params.stride;
            params.stride = -params.stride;
        }

        struct dr_buffer *mapped = gl_find_dr_buffer(p, mpi->planes[n]);
        if (mapped) {
            params.buf = mapped->buf;
            params.buf_offset = (uintptr_t)params.src -
                                (uintptr_t)mapped->buf->data;
            params.src = NULL;
        }

        if (p->using_dr_path != !!mapped) {
            p->using_dr_path = !!mapped;
            MP_VERBOSE(p, "DR enabled: %s\n", p->using_dr_path ? "yes" : "no");
        }

        if (!p->ra->fns->tex_upload(p->ra, &params)) {
            timer_pool_stop(p->upload_timer);
            goto error;
        }

        if (mapped && !mapped->mpi)
            mapped->mpi = mp_image_new_ref(mpi);
    }
    timer_pool_stop(p->upload_timer);

    bool using_pbo = p->ra->use_pbo || !(p->ra->caps & RA_CAP_DIRECT_UPLOAD);
    const char *mode = p->using_dr_path ? "DR" : using_pbo ? "PBO" : "naive";
    pass_describe(p, "upload frame (%s)", mode);
    pass_record(p, timer_pool_measure(p->upload_timer));

    return true;

error:
    unref_current_image(p);
    p->broken_frame = true;
    return false;
}

static bool test_fbo(struct gl_video *p, const struct ra_format *fmt)
{
    MP_VERBOSE(p, "Testing FBO format %s\n", fmt->name);
    struct ra_tex *tex = NULL;
    bool success = ra_tex_resize(p->ra, p->log, &tex, 16, 16, fmt);
    ra_tex_free(p->ra, &tex);
    return success;
}

// Return whether dumb-mode can be used without disabling any features.
// Essentially, vo_gpu with mostly default settings will return true.
static bool check_dumb_mode(struct gl_video *p)
{
    struct gl_video_opts *o = &p->opts;
    if (p->use_integer_conversion)
        return false;
    if (o->dumb_mode > 0) // requested by user
        return true;
    if (o->dumb_mode < 0) // disabled by user
        return false;

    // otherwise, use auto-detection
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
    struct ra *ra = p->ra;
    bool have_float_tex = !!ra_find_float16_format(ra, 1);
    bool have_mglsl = ra->glsl_version >= 130; // modern GLSL
    const struct ra_format *rg_tex = ra_find_unorm_format(p->ra, 1, 2);
    bool have_texrg = rg_tex && !rg_tex->luminance_alpha;
    bool have_compute = ra->caps & RA_CAP_COMPUTE;
    bool have_ssbo = ra->caps & RA_CAP_BUF_RW;
    bool have_fragcoord = ra->caps & RA_CAP_FRAGCOORD;

    const char *auto_fbo_fmts[] = {"rgba16f", "rgba16hf", "rgba16",
                                   "rgb10_a2", "rgba8", 0};
    const char *user_fbo_fmts[] = {p->opts.fbo_format, 0};
    const char **fbo_fmts = user_fbo_fmts[0] && strcmp(user_fbo_fmts[0], "auto")
                          ? user_fbo_fmts : auto_fbo_fmts;
    bool user_specified_fbo_fmt = fbo_fmts == user_fbo_fmts;
    bool fbo_test_result = false;
    bool have_fbo = false;
    p->fbo_format = NULL;
    for (int n = 0; fbo_fmts[n]; n++) {
        const char *fmt = fbo_fmts[n];
        const struct ra_format *f = ra_find_named_format(p->ra, fmt);
        if (!f && user_specified_fbo_fmt)
            MP_WARN(p, "FBO format '%s' not found!\n", fmt);
        if (f && f->renderable && f->linear_filter &&
            (fbo_test_result = test_fbo(p, f))) {
            MP_VERBOSE(p, "Using FBO format %s.\n", f->name);
            have_fbo = true;
            p->fbo_format = f;
            break;
        }

        if (user_specified_fbo_fmt) {
            MP_WARN(p, "User-specified FBO format '%s' failed to initialize! "
                       "(exists=%d, renderable=%d, linear_filter=%d, "
                       "fbo_test_result=%d)\n",
                    fmt, !!f, f ? f->renderable : 0,  f ? f->linear_filter : 0,
                    fbo_test_result);
        }
    }

    if (!have_fragcoord && p->opts.dither_depth >= 0 &&
        p->opts.dither_algo != DITHER_NONE)
    {
        p->opts.dither_algo = DITHER_NONE;
        MP_WARN(p, "Disabling dithering (no gl_FragCoord).\n");
    }
    if (!have_fragcoord && p->opts.alpha_mode == ALPHA_BLEND_TILES) {
        p->opts.alpha_mode = ALPHA_BLEND;
        // Verbose, since this is the default setting
        MP_VERBOSE(p, "Disabling alpha checkerboard (no gl_FragCoord).\n");
    }
    if (!have_fbo && have_compute) {
        have_compute = false;
        MP_WARN(p, "Force-disabling compute shaders as an FBO format was not "
                   "available! See your FBO format configuration!\n");
    }

    bool have_compute_peak = have_compute && have_ssbo;
    if (!have_compute_peak && p->opts.compute_hdr_peak >= 0) {
        int msgl = p->opts.compute_hdr_peak == 1 ? MSGL_WARN : MSGL_V;
        MP_MSG(p, msgl, "Disabling HDR peak computation (one or more of the "
                        "following is not supported: compute shaders=%d, "
                        "SSBO=%d).\n", have_compute, have_ssbo);
        p->opts.compute_hdr_peak = -1;
    }

    p->forced_dumb_mode = p->opts.dumb_mode > 0 || !have_fbo || !have_texrg;
    bool voluntarily_dumb = check_dumb_mode(p);
    if (p->forced_dumb_mode || voluntarily_dumb) {
        if (voluntarily_dumb) {
            MP_VERBOSE(p, "No advanced processing required. Enabling dumb mode.\n");
        } else if (p->opts.dumb_mode <= 0) {
            MP_WARN(p, "High bit depth FBOs unsupported. Enabling dumb mode.\n"
                       "Most extended features will be disabled.\n");
        }
        p->dumb_mode = true;
        // Most things don't work, so whitelist all options that still work.
        p->opts = (struct gl_video_opts){
            .gamma = p->opts.gamma,
            .gamma_auto = p->opts.gamma_auto,
            .pbo = p->opts.pbo,
            .fbo_format = p->opts.fbo_format,
            .alpha_mode = p->opts.alpha_mode,
            .use_rectangle = p->opts.use_rectangle,
            .background = p->opts.background,
            .compute_hdr_peak = p->opts.compute_hdr_peak,
            .dither_algo = p->opts.dither_algo,
            .dither_depth = p->opts.dither_depth,
            .dither_size = p->opts.dither_size,
            .temporal_dither = p->opts.temporal_dither,
            .temporal_dither_period = p->opts.temporal_dither_period,
            .tex_pad_x = p->opts.tex_pad_x,
            .tex_pad_y = p->opts.tex_pad_y,
            .tone_mapping = p->opts.tone_mapping,
            .tone_mapping_param = p->opts.tone_mapping_param,
            .tone_mapping_desat = p->opts.tone_mapping_desat,
            .early_flush = p->opts.early_flush,
            .icc_opts = p->opts.icc_opts,
            .hwdec_interop = p->opts.hwdec_interop,
        };
        for (int n = 0; n < SCALER_COUNT; n++)
            p->opts.scaler[n] = gl_video_opts_def.scaler[n];
        if (!have_fbo)
            p->use_lut_3d = false;
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
    debug_check_gl(p, "before init_gl");

    p->upload_timer = timer_pool_create(p->ra);
    p->blit_timer = timer_pool_create(p->ra);
    p->osd_timer = timer_pool_create(p->ra);

    debug_check_gl(p, "after init_gl");

    ra_dump_tex_formats(p->ra, MSGL_DEBUG);
    ra_dump_img_formats(p->ra, MSGL_DEBUG);
}

void gl_video_uninit(struct gl_video *p)
{
    if (!p)
        return;

    uninit_video(p);

    for (int n = 0; n < p->num_hwdecs; n++)
        ra_hwdec_uninit(p->hwdecs[n]);
    p->num_hwdecs = 0;

    gl_sc_destroy(p->sc);

    ra_tex_free(p->ra, &p->lut_3d_texture);
    ra_buf_free(p->ra, &p->hdr_peak_ssbo);

    timer_pool_destroy(p->upload_timer);
    timer_pool_destroy(p->blit_timer);
    timer_pool_destroy(p->osd_timer);

    for (int i = 0; i < VO_PASS_PERF_MAX; i++) {
        talloc_free(p->pass_fresh[i].desc.start);
        talloc_free(p->pass_redraw[i].desc.start);
    }

    mpgl_osd_destroy(p->osd);

    // Forcibly destroy possibly remaining image references. This should also
    // cause gl_video_dr_free_buffer() to be called for the remaining buffers.
    gc_pending_dr_fences(p, true);

    // Should all have been unreffed already.
    assert(!p->num_dr_buffers);

    talloc_free(p);
}

void gl_video_reset(struct gl_video *p)
{
    gl_video_reset_surfaces(p);
}

bool gl_video_showing_interpolated_frame(struct gl_video *p)
{
    return p->is_interpolated;
}

static bool is_imgfmt_desc_supported(struct gl_video *p,
                                     const struct ra_imgfmt_desc *desc)
{
    if (!desc->num_planes)
        return false;

    if (desc->planes[0]->ctype == RA_CTYPE_UINT && p->forced_dumb_mode)
        return false;

    return true;
}

bool gl_video_check_format(struct gl_video *p, int mp_format)
{
    struct ra_imgfmt_desc desc;
    if (ra_get_imgfmt_desc(p->ra, mp_format, &desc) &&
        is_imgfmt_desc_supported(p, &desc))
        return true;
    for (int n = 0; n < p->num_hwdecs; n++) {
        if (ra_hwdec_test_format(p->hwdecs[n], mp_format))
            return true;
    }
    return false;
}

void gl_video_config(struct gl_video *p, struct mp_image_params *params)
{
    unmap_overlay(p);
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

struct gl_video *gl_video_init(struct ra *ra, struct mp_log *log,
                               struct mpv_global *g)
{
    struct gl_video *p = talloc_ptrtype(NULL, p);
    *p = (struct gl_video) {
        .ra = ra,
        .global = g,
        .log = log,
        .sc = gl_sc_create(ra, g, log),
        .video_eq = mp_csp_equalizer_create(p, g),
        .opts_cache = m_config_cache_alloc(p, g, &gl_video_conf),
    };
    // make sure this variable is initialized to *something*
    p->pass = p->pass_fresh;
    struct gl_video_opts *opts = p->opts_cache->opts;
    p->cms = gl_lcms_init(p, log, g, opts->icc_opts),
    p->opts = *opts;
    for (int n = 0; n < SCALER_COUNT; n++)
        p->scaler[n] = (struct scaler){.index = n};
    // our VAO always has the vec2 position as the first element
    MP_TARRAY_APPEND(p, p->vao, p->vao_len, (struct ra_renderpass_input) {
        .name = "position",
        .type = RA_VARTYPE_FLOAT,
        .dim_v = 2,
        .dim_m = 1,
        .offset = 0,
    });
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

static void gl_video_update_options(struct gl_video *p)
{
    if (m_config_cache_update(p->opts_cache)) {
        gl_lcms_update_options(p->cms);
        reinit_from_options(p);
    }

    if (mp_csp_equalizer_state_changed(p->video_eq))
        p->output_tex_valid = false;
}

static void reinit_from_options(struct gl_video *p)
{
    p->use_lut_3d = gl_lcms_has_profile(p->cms);

    // Copy the option fields, so that check_gl_features() can mutate them.
    // This works only for the fields themselves of course, not for any memory
    // referenced by them.
    p->opts = *(struct gl_video_opts *)p->opts_cache->opts;

    if (!p->force_clear_color)
        p->clear_color = p->opts.background;

    check_gl_features(p);
    uninit_rendering(p);
    gl_sc_set_cache_dir(p->sc, p->opts.shader_cache_dir);
    p->ra->use_pbo = p->opts.pbo;
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
    gl_video_update_options(p);

    int queue_size = 1;

    // Figure out an adequate size for the interpolation queue. The larger
    // the radius, the earlier we need to queue frames.
    if (p->opts.interpolation) {
        const struct filter_kernel *kernel =
            mp_find_filter_kernel(p->opts.scaler[SCALER_TSCALE].kernel.name);
        if (kernel) {
            // filter_scale wouldn't be correctly initialized were we to use it here.
            // This is fine since we're always upsampling, but beware if downsampling
            // is added!
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
        p->opts.gamma = gl_video_scale_ambient_lux(16.0, 256.0, 1.0, 1.2, lux);
        MP_TRACE(p, "ambient light changed: %d lux (gamma: %f)\n", lux,
                 p->opts.gamma);
    }
}

static void *gl_video_dr_alloc_buffer(struct gl_video *p, size_t size)
{
    struct ra_buf_params params = {
        .type = RA_BUF_TYPE_TEX_UPLOAD,
        .host_mapped = true,
        .size = size,
    };

    struct ra_buf *buf = ra_buf_create(p->ra, &params);
    if (!buf)
        return NULL;

    MP_TARRAY_GROW(p, p->dr_buffers, p->num_dr_buffers);
    p->dr_buffers[p->num_dr_buffers++] = (struct dr_buffer){ .buf = buf };

    return buf->data;
};

static void gl_video_dr_free_buffer(void *opaque, uint8_t *data)
{
    struct gl_video *p = opaque;

    for (int n = 0; n < p->num_dr_buffers; n++) {
        struct dr_buffer *buffer = &p->dr_buffers[n];
        if (buffer->buf->data == data) {
            assert(!buffer->mpi); // can't be freed while it has a ref
            ra_buf_free(p->ra, &buffer->buf);
            MP_TARRAY_REMOVE_AT(p->dr_buffers, p->num_dr_buffers, n);
            return;
        }
    }
    // not found - must not happen
    assert(0);
}

struct mp_image *gl_video_get_image(struct gl_video *p, int imgfmt, int w, int h,
                                    int stride_align)
{
    if (!gl_video_check_format(p, imgfmt))
        return NULL;

    int size = mp_image_get_alloc_size(imgfmt, w, h, stride_align);
    if (size < 0)
        return NULL;

    int alloc_size = size + stride_align;
    void *ptr = gl_video_dr_alloc_buffer(p, alloc_size);
    if (!ptr)
        return NULL;

    // (we expect vo.c to proxy the free callback, so it happens in the same
    // thread it was allocated in, removing the need for synchronization)
    struct mp_image *res = mp_image_from_buffer(imgfmt, w, h, stride_align,
                                                ptr, alloc_size, p,
                                                gl_video_dr_free_buffer);
    if (!res)
        gl_video_dr_free_buffer(p, ptr);
    return res;
}

static void load_add_hwdec(struct gl_video *p, struct mp_hwdec_devices *devs,
                           const struct ra_hwdec_driver *drv, bool is_auto)
{
    struct ra_hwdec *hwdec =
        ra_hwdec_load_driver(p->ra, p->log, p->global, devs, drv, is_auto);
    if (hwdec)
        MP_TARRAY_APPEND(p, p->hwdecs, p->num_hwdecs, hwdec);
}

void gl_video_load_hwdecs(struct gl_video *p, struct mp_hwdec_devices *devs,
                          bool load_all_by_default)
{
    char *type = p->opts.hwdec_interop;
    if (!type || !type[0] || strcmp(type, "auto") == 0) {
        if (!load_all_by_default)
            return;
        type = "all";
    }
    if (strcmp(type, "no") == 0) {
        // do nothing, just block further loading
    } else if (strcmp(type, "all") == 0) {
        gl_video_load_hwdecs_all(p, devs);
    } else {
        for (int n = 0; ra_hwdec_drivers[n]; n++) {
            const struct ra_hwdec_driver *drv = ra_hwdec_drivers[n];
            if (strcmp(type, drv->name) == 0) {
                load_add_hwdec(p, devs, drv, false);
                break;
            }
        }
    }
    p->hwdec_interop_loading_done = true;
}

void gl_video_load_hwdecs_all(struct gl_video *p, struct mp_hwdec_devices *devs)
{
    if (!p->hwdec_interop_loading_done) {
        for (int n = 0; ra_hwdec_drivers[n]; n++)
            load_add_hwdec(p, devs, ra_hwdec_drivers[n], true);
        p->hwdec_interop_loading_done = true;
    }
}
