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
#include <ctype.h>
#include <assert.h>

#include "config.h"
#include "talloc.h"
#include "core/mp_msg.h"
#include "core/subopt-helper.h"
#include "vo.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "sub/sub.h"

#include "gl_common.h"
#include "gl_osd.h"
#include "video/memcpy_pic.h"
#include "pnm_loader.h"

//for gl_priv.use_yuv
#define MASK_ALL_YUV (~(1 << YUV_CONVERSION_NONE))
#define MASK_NOT_COMBINERS (~((1 << YUV_CONVERSION_NONE) | (1 << YUV_CONVERSION_COMBINERS)))
#define MASK_GAMMA_SUPPORT (MASK_NOT_COMBINERS & ~(1 << YUV_CONVERSION_FRAGMENT))

struct gl_priv {
    MPGLContext *glctx;
    GL *gl;

    int allow_sw;

    int scaled_osd;
    struct mpgl_osd *osd;
    int osd_color;

    int use_ycbcr;
    int use_yuv;
    struct mp_csp_details colorspace;
    int is_yuv;
    int lscale;
    int cscale;
    float filter_strength;
    float noise_strength;
    int yuvconvtype;
    int use_rectangle;
    int err_shown;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_format;
    int many_fmts;
    int have_texture_rg;
    int max_tex_component_size;
    int ati_hack;
    int force_pbo;
    int use_glFinish;
    int swap_interval;
    GLenum target;
    GLint texfmt;
    GLenum gl_format;
    GLenum gl_type;
    GLuint buffer;
    GLuint buffer_uv[2];
    int buffersize;
    int buffersize_uv;
    void *bufferptr;
    void *bufferptr_uv[2];
    GLuint fragprog;
    GLuint default_texs[22];
    char *custom_prog;
    char *custom_tex;
    int custom_tlin;
    int custom_trect;
    int mipmap_gen;
    int stereo_mode;

    struct mp_csp_equalizer video_eq;

    int texture_width;
    int texture_height;
    int mpi_flipped;
    int vo_flipped;

    struct mp_rect src_rect;    // displayed part of the source video
    struct mp_rect dst_rect;    // video rectangle on output window
    struct mp_osd_res osd_res;

    unsigned int slice_height;
};

static int glFindFormat(uint32_t format, int have_texture_rg, int *bpp,
                        GLint *gl_texfmt, GLenum *gl_format, GLenum *gl_type);
static void glCreateClearTex(GL *gl, GLenum target, GLenum fmt, GLenum format,
                             GLenum type, GLint filter, int w, int h,
                             unsigned char val);
static int glCreatePPMTex(GL *gl, GLenum target, GLenum fmt, GLint filter,
                          FILE *f, int *width, int *height, int *maxval);
static void glDrawTex(GL *gl, GLfloat x, GLfloat y, GLfloat w, GLfloat h,
                      GLfloat tx, GLfloat ty, GLfloat tw, GLfloat th,
                      int sx, int sy, int rect_tex, int is_yv12, int flip);
static int loadGPUProgram(GL *gl, GLenum target, char *prog);
//! do not use YUV conversion, this should always stay 0
#define YUV_CONVERSION_NONE 0
//! use nVidia specific register combiners for YUV conversion
//! implementation has been removed
#define YUV_CONVERSION_COMBINERS 1
//! use a fragment program for YUV conversion
#define YUV_CONVERSION_FRAGMENT 2
//! use a fragment program for YUV conversion with gamma using POW
#define YUV_CONVERSION_FRAGMENT_POW 3
//! use a fragment program with additional table lookup for YUV conversion
#define YUV_CONVERSION_FRAGMENT_LOOKUP 4
//! use ATI specific register combiners ("fragment program")
#define YUV_CONVERSION_COMBINERS_ATI 5
//! use a fragment program with 3D table lookup for YUV conversion
#define YUV_CONVERSION_FRAGMENT_LOOKUP3D 6
//! use ATI specific "text" register combiners ("fragment program")
#define YUV_CONVERSION_TEXT_FRAGMENT 7
//! use normal bilinear scaling for textures
#define YUV_SCALER_BILIN 0
//! use higher quality bicubic scaling for textures
#define YUV_SCALER_BICUB 1
//! use cubic scaling in X and normal linear scaling in Y direction
#define YUV_SCALER_BICUB_X 2
//! use cubic scaling without additional lookup texture
#define YUV_SCALER_BICUB_NOTEX 3
#define YUV_SCALER_UNSHARP 4
#define YUV_SCALER_UNSHARP2 5
//! mask for conversion type
#define YUV_CONVERSION_MASK 0xF
//! mask for scaler type
#define YUV_SCALER_MASK 0xF
//! shift value for luminance scaler type
#define YUV_LUM_SCALER_SHIFT 8
//! shift value for chrominance scaler type
#define YUV_CHROM_SCALER_SHIFT 12
//! extract conversion out of type
#define YUV_CONVERSION(t) ((t) & YUV_CONVERSION_MASK)
//! extract luminance scaler out of type
#define YUV_LUM_SCALER(t) (((t) >> YUV_LUM_SCALER_SHIFT) & YUV_SCALER_MASK)
//! extract chrominance scaler out of type
#define YUV_CHROM_SCALER(t) (((t) >> YUV_CHROM_SCALER_SHIFT) & YUV_SCALER_MASK)
#define SET_YUV_CONVERSION(c)   ((c) & YUV_CONVERSION_MASK)
#define SET_YUV_LUM_SCALER(s)   (((s) & YUV_SCALER_MASK) << YUV_LUM_SCALER_SHIFT)
#define SET_YUV_CHROM_SCALER(s) (((s) & YUV_SCALER_MASK) << YUV_CHROM_SCALER_SHIFT)
//! returns whether the yuv conversion supports large brightness range etc.
static inline int glYUVLargeRange(int conv)
{
    switch (conv) {
    case YUV_CONVERSION_NONE:
    case YUV_CONVERSION_COMBINERS_ATI:
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
    case YUV_CONVERSION_TEXT_FRAGMENT:
        return 0;
    }
    return 1;
}
typedef struct {
    GLenum target;
    int type;
    struct mp_csp_params csp_params;
    int texw;
    int texh;
    int chrom_texw;
    int chrom_texh;
    float filter_strength;
    float noise_strength;
} gl_conversion_params_t;

static int glAutodetectYUVConversion(GL *gl);
static void glSetupYUVConversion(GL *gl, gl_conversion_params_t *params);
static void glEnableYUVConversion(GL *gl, GLenum target, int type);
static void glDisableYUVConversion(GL *gl, GLenum target, int type);

//! always return this format as internal texture format in glFindFormat
#define TEXTUREFORMAT_ALWAYS GL_RGB8
#undef TEXTUREFORMAT_ALWAYS

/**
 * \brief find the OpenGL settings coresponding to format.
 *
 * All parameters may be NULL.
 * \param fmt MPlayer format to analyze.
 * \param dummy reserved
 * \param gl_texfmt [OUT] internal texture format that fits the
 * image format, not necessarily the best for performance.
 * \param gl_format [OUT] OpenGL format for this image format.
 * \param gl_type [OUT] OpenGL type for this image format.
 * \return 1 if format is supported by OpenGL, 0 if not.
 * \ingroup gltexture
 */
static int glFindFormat(uint32_t fmt, int have_texture_rg, int *dummy,
                        GLint *gl_texfmt, GLenum *gl_format, GLenum *gl_type)
{
    int supported = 1;
    GLenum dummy2;
    GLint dummy3;
    if (!gl_texfmt)
        gl_texfmt = &dummy3;
    if (!gl_format)
        gl_format = &dummy2;
    if (!gl_type)
        gl_type = &dummy2;

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (desc.flags & MP_IMGFLAG_YUV_P) {
        // reduce the possible cases a bit
        if (desc.plane_bits > 8)
            fmt = IMGFMT_420P16;
        else
            fmt = IMGFMT_420P;
    }

    *gl_texfmt = 3;
    switch (fmt) {
    case IMGFMT_RGB48:
        *gl_format = GL_RGB;
        *gl_type = GL_UNSIGNED_SHORT;
        break;
    case IMGFMT_RGB24:
        *gl_format = GL_RGB;
        *gl_type = GL_UNSIGNED_BYTE;
        break;
    case IMGFMT_RGBA:
        *gl_texfmt = 4;
        *gl_format = GL_RGBA;
        *gl_type = GL_UNSIGNED_BYTE;
        break;
    case IMGFMT_420P16:
        supported = 0; // no native YUV support
        *gl_texfmt = have_texture_rg ? GL_R16 : GL_LUMINANCE16;
        *gl_format = have_texture_rg ? GL_RED : GL_LUMINANCE;
        *gl_type = GL_UNSIGNED_SHORT;
        break;
    case IMGFMT_420P:
        supported = 0; // no native YV12 support
    case IMGFMT_Y8:
        *gl_texfmt = 1;
        *gl_format = GL_LUMINANCE;
        *gl_type = GL_UNSIGNED_BYTE;
        break;
    case IMGFMT_UYVY:
        *gl_texfmt = GL_YCBCR_MESA;
        *gl_format = GL_YCBCR_MESA;
        *gl_type = fmt == IMGFMT_UYVY ? GL_UNSIGNED_SHORT_8_8 : GL_UNSIGNED_SHORT_8_8_REV;
        break;
#if 0
    // we do not support palettized formats, although the format the
    // swscale produces works
    case IMGFMT_RGB8:
        *gl_format = GL_RGB;
        *gl_type = GL_UNSIGNED_BYTE_2_3_3_REV;
        break;
#endif
    case IMGFMT_RGB15:
        *gl_format = GL_RGBA;
        *gl_type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
        break;
    case IMGFMT_RGB16:
        *gl_format = GL_RGB;
        *gl_type = GL_UNSIGNED_SHORT_5_6_5_REV;
        break;
#if 0
    case IMGFMT_BGR8:
        // special case as red and blue have a different number of bits.
        // GL_BGR and GL_UNSIGNED_BYTE_3_3_2 isn't supported at least
        // by nVidia drivers, and in addition would give more bits to
        // blue than to red, which isn't wanted
        *gl_format = GL_RGB;
        *gl_type = GL_UNSIGNED_BYTE_3_3_2;
        break;
#endif
    case IMGFMT_BGR15:
        *gl_format = GL_BGRA;
        *gl_type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
        break;
    case IMGFMT_BGR16:
        *gl_format = GL_RGB;
        *gl_type = GL_UNSIGNED_SHORT_5_6_5;
        break;
    case IMGFMT_BGR24:
        *gl_format = GL_BGR;
        *gl_type = GL_UNSIGNED_BYTE;
        break;
    case IMGFMT_BGRA:
        *gl_texfmt = 4;
        *gl_format = GL_BGRA;
        *gl_type = GL_UNSIGNED_BYTE;
        break;
    default:
        *gl_texfmt = 4;
        *gl_format = GL_RGBA;
        *gl_type = GL_UNSIGNED_BYTE;
        supported = 0;
    }
#ifdef TEXTUREFORMAT_ALWAYS
    *gl_texfmt = TEXTUREFORMAT_ALWAYS;
#endif
    return supported;
}

/**
 * \brief create a texture and set some defaults
 * \param target texture taget, usually GL_TEXTURE_2D
 * \param fmt internal texture format
 * \param format texture host data format
 * \param type texture host data type
 * \param filter filter used for scaling, e.g. GL_LINEAR
 * \param w texture width
 * \param h texture height
 * \param val luminance value to fill texture with
 * \ingroup gltexture
 */
static void glCreateClearTex(GL *gl, GLenum target, GLenum fmt, GLenum format,
                             GLenum type, GLint filter, int w, int h,
                             unsigned char val)
{
    GLfloat fval = (GLfloat)val / 255.0;
    GLfloat border[4] = {
        fval, fval, fval, fval
    };
    int stride;
    char *init;
    if (w == 0)
        w = 1;
    if (h == 0)
        h = 1;
    stride = w * glFmt2bpp(format, type);
    if (!stride)
        return;
    init = malloc(stride * h);
    memset(init, val, stride * h);
    glAdjustAlignment(gl, stride);
    gl->PixelStorei(GL_UNPACK_ROW_LENGTH, w);
    gl->TexImage2D(target, 0, fmt, w, h, 0, format, type, init);
    gl->TexParameterf(target, GL_TEXTURE_PRIORITY, 1.0);
    gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    gl->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Border texels should not be used with CLAMP_TO_EDGE
    // We set a sane default anyway.
    gl->TexParameterfv(target, GL_TEXTURE_BORDER_COLOR, border);
    free(init);
}

static GLint detect_hqtexfmt(GL *gl)
{
    const char *extensions = (const char *)gl->GetString(GL_EXTENSIONS);
    if (strstr(extensions, "_texture_float"))
        return GL_RGB32F;
    else if (strstr(extensions, "NV_float_buffer"))
        return GL_FLOAT_RGB32_NV;
    return GL_RGB16;
}

/**
 * \brief creates a texture from a PPM file
 * \param target texture taget, usually GL_TEXTURE_2D
 * \param fmt internal texture format, 0 for default
 * \param filter filter used for scaling, e.g. GL_LINEAR
 * \param f file to read PPM from
 * \param width [out] width of texture
 * \param height [out] height of texture
 * \param maxval [out] maxval value from PPM file
 * \return 0 on error, 1 otherwise
 * \ingroup gltexture
 */
static int glCreatePPMTex(GL *gl, GLenum target, GLenum fmt, GLint filter,
                          FILE *f, int *width, int *height, int *maxval)
{
    int w, h, m, bpp;
    GLenum type;
    uint8_t *data = read_pnm(f, &w, &h, &bpp, &m);
    GLint hqtexfmt = detect_hqtexfmt(gl);
    if (!data || (bpp != 3 && bpp != 6)) {
        free(data);
        return 0;
    }
    if (!fmt) {
        fmt = bpp == 6 ? hqtexfmt : 3;
        if (fmt == GL_FLOAT_RGB32_NV && target != GL_TEXTURE_RECTANGLE)
            fmt = GL_RGB16;
    }
    type = bpp == 6 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;
    glCreateClearTex(gl, target, fmt, GL_RGB, type, filter, w, h, 0);
    glUploadTex(gl, target, GL_RGB, type,
                data, w * bpp, 0, 0, w, h, 0);
    free(data);
    if (width)
        *width = w;
    if (height)
        *height = h;
    if (maxval)
        *maxval = m;
    return 1;
}


/**
 * \brief Setup ATI version of register combiners for YUV to RGB conversion.
 * \param csp_params parameters used for colorspace conversion
 * \param text if set use the GL_ATI_text_fragment_shader API as
 *             used on OS X.
 */
static void glSetupYUVFragmentATI(GL *gl, struct mp_csp_params *csp_params,
                                  int text)
{
    GLint i;
    float yuv2rgb[3][4];

    gl->GetIntegerv(GL_MAX_TEXTURE_UNITS, &i);
    if (i < 3)
        mp_msg(MSGT_VO, MSGL_ERR,
               "[gl] 3 texture units needed for YUV combiner (ATI) support (found %i)\n", i);

    mp_get_yuv2rgb_coeffs(csp_params, yuv2rgb);
    for (i = 0; i < 3; i++) {
        int j;
        yuv2rgb[i][3] -= -0.5 * (yuv2rgb[i][1] + yuv2rgb[i][2]);
        for (j = 0; j < 4; j++) {
            yuv2rgb[i][j] *= 0.125;
            yuv2rgb[i][j] += 0.5;
            if (yuv2rgb[i][j] > 1)
                yuv2rgb[i][j] = 1;
            if (yuv2rgb[i][j] < 0)
                yuv2rgb[i][j] = 0;
        }
    }
    if (text == 0) {
        GLfloat c0[4] = { yuv2rgb[0][0], yuv2rgb[1][0], yuv2rgb[2][0] };
        GLfloat c1[4] = { yuv2rgb[0][1], yuv2rgb[1][1], yuv2rgb[2][1] };
        GLfloat c2[4] = { yuv2rgb[0][2], yuv2rgb[1][2], yuv2rgb[2][2] };
        GLfloat c3[4] = { yuv2rgb[0][3], yuv2rgb[1][3], yuv2rgb[2][3] };
        if (!gl->BeginFragmentShader || !gl->EndFragmentShader ||
            !gl->SetFragmentShaderConstant || !gl->SampleMap ||
            !gl->ColorFragmentOp2 || !gl->ColorFragmentOp3) {
            mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Combiner (ATI) functions missing!\n");
            return;
        }
        gl->GetIntegerv(GL_NUM_FRAGMENT_REGISTERS_ATI, &i);
        if (i < 3)
            mp_msg(MSGT_VO, MSGL_ERR,
                   "[gl] 3 registers needed for YUV combiner (ATI) support (found %i)\n", i);
        gl->BeginFragmentShader();
        gl->SetFragmentShaderConstant(GL_CON_0_ATI, c0);
        gl->SetFragmentShaderConstant(GL_CON_1_ATI, c1);
        gl->SetFragmentShaderConstant(GL_CON_2_ATI, c2);
        gl->SetFragmentShaderConstant(GL_CON_3_ATI, c3);
        gl->SampleMap(GL_REG_0_ATI, GL_TEXTURE0, GL_SWIZZLE_STR_ATI);
        gl->SampleMap(GL_REG_1_ATI, GL_TEXTURE1, GL_SWIZZLE_STR_ATI);
        gl->SampleMap(GL_REG_2_ATI, GL_TEXTURE2, GL_SWIZZLE_STR_ATI);
        gl->ColorFragmentOp2(GL_MUL_ATI, GL_REG_1_ATI, GL_NONE, GL_NONE,
                             GL_REG_1_ATI, GL_NONE, GL_BIAS_BIT_ATI,
                             GL_CON_1_ATI, GL_NONE, GL_BIAS_BIT_ATI);
        gl->ColorFragmentOp3(GL_MAD_ATI, GL_REG_2_ATI, GL_NONE, GL_NONE,
                             GL_REG_2_ATI, GL_NONE, GL_BIAS_BIT_ATI,
                             GL_CON_2_ATI, GL_NONE, GL_BIAS_BIT_ATI,
                             GL_REG_1_ATI, GL_NONE, GL_NONE);
        gl->ColorFragmentOp3(GL_MAD_ATI, GL_REG_0_ATI, GL_NONE, GL_NONE,
                             GL_REG_0_ATI, GL_NONE, GL_NONE,
                             GL_CON_0_ATI, GL_NONE, GL_BIAS_BIT_ATI,
                             GL_REG_2_ATI, GL_NONE, GL_NONE);
        gl->ColorFragmentOp2(GL_ADD_ATI, GL_REG_0_ATI, GL_NONE, GL_8X_BIT_ATI,
                             GL_REG_0_ATI, GL_NONE, GL_NONE,
                             GL_CON_3_ATI, GL_NONE, GL_BIAS_BIT_ATI);
        gl->EndFragmentShader();
    } else {
        static const char template[] =
            "!!ATIfs1.0\n"
            "StartConstants;\n"
            "  CONSTANT c0 = {%e, %e, %e};\n"
            "  CONSTANT c1 = {%e, %e, %e};\n"
            "  CONSTANT c2 = {%e, %e, %e};\n"
            "  CONSTANT c3 = {%e, %e, %e};\n"
            "EndConstants;\n"
            "StartOutputPass;\n"
            "  SampleMap r0, t0.str;\n"
            "  SampleMap r1, t1.str;\n"
            "  SampleMap r2, t2.str;\n"
            "  MUL r1.rgb, r1.bias, c1.bias;\n"
            "  MAD r2.rgb, r2.bias, c2.bias, r1;\n"
            "  MAD r0.rgb, r0, c0.bias, r2;\n"
            "  ADD r0.rgb.8x, r0, c3.bias;\n"
            "EndPass;\n";
        char buffer[512];
        snprintf(buffer, sizeof(buffer), template,
                 yuv2rgb[0][0], yuv2rgb[1][0], yuv2rgb[2][0],
                 yuv2rgb[0][1], yuv2rgb[1][1], yuv2rgb[2][1],
                 yuv2rgb[0][2], yuv2rgb[1][2], yuv2rgb[2][2],
                 yuv2rgb[0][3], yuv2rgb[1][3], yuv2rgb[2][3]);
        mp_msg(MSGT_VO, MSGL_DBG2, "[gl] generated fragment program:\n%s\n",
               buffer);
        loadGPUProgram(gl, GL_TEXT_FRAGMENT_SHADER_ATI, buffer);
    }
}

// Replace all occurances of variables named "$"+name (e.g. $foo) in *text with
// replace, and return the result. *text must have been allocated with talloc.
static void replace_var_str(char **text, const char *name, const char *replace)
{
    size_t namelen = strlen(name);
    char *nextvar = *text;
    void *parent = talloc_parent(*text);
    for (;;) {
        nextvar = strchr(nextvar, '$');
        if (!nextvar)
            break;
        char *until = nextvar;
        nextvar++;
        if (strncmp(nextvar, name, namelen) != 0)
            continue;
        nextvar += namelen;
        // try not to replace prefixes of other vars (e.g. $foo vs. $foo_bar)
        char term = nextvar[0];
        if (isalnum(term) || term == '_')
            continue;
        int prelength = until - *text;
        int postlength = nextvar - *text;
        char *n = talloc_asprintf(parent, "%.*s%s%s", prelength, *text, replace,
                                  nextvar);
        talloc_free(*text);
        *text = n;
        nextvar = *text + postlength;
    }
}

static void replace_var_float(char **text, const char *name, float replace)
{
    char *s = talloc_asprintf(NULL, "%e", replace);
    replace_var_str(text, name, s);
    talloc_free(s);
}

static void replace_var_char(char **text, const char *name, char replace)
{
    char s[2] = { replace, '\0' };
    replace_var_str(text, name, s);
}

// Append template to *text. Possibly initialize *text if it's NULL.
static void append_template(char **text, const char* template)
{
    if (!*text)
        *text = talloc_strdup(NULL, template);
    else
        *text = talloc_strdup_append(*text, template);
}

/**
 * \brief helper function for gen_spline_lookup_tex
 * \param x subpixel-position ((0,1) range) to calculate weights for
 * \param dst where to store transformed weights, must provide space for 4 GLfloats
 *
 * calculates the weights and stores them after appropriate transformation
 * for the scaler fragment program.
 */
static void store_weights(float x, GLfloat *dst)
{
    float w0 = (((-1 * x + 3) * x - 3) * x + 1) / 6;
    float w1 = (((3 * x - 6) * x + 0) * x + 4) / 6;
    float w2 = (((-3 * x + 3) * x + 3) * x + 1) / 6;
    float w3 = (((1 * x + 0) * x + 0) * x + 0) / 6;
    *dst++ = 1 + x - w1 / (w0 + w1);
    *dst++ = 1 - x + w3 / (w2 + w3);
    *dst++ = w0 + w1;
    *dst++ = 0;
}

//! to avoid artefacts this should be rather large
#define LOOKUP_BSPLINE_RES (2 * 1024)
/**
 * \brief creates the 1D lookup texture needed for fast higher-order filtering
 * \param unit texture unit to attach texture to
 */
static void gen_spline_lookup_tex(GL *gl, GLenum unit)
{
    GLfloat *tex = calloc(4 * LOOKUP_BSPLINE_RES, sizeof(*tex));
    GLfloat *tp = tex;
    int i;
    for (i = 0; i < LOOKUP_BSPLINE_RES; i++) {
        float x = (float)(i + 0.5) / LOOKUP_BSPLINE_RES;
        store_weights(x, tp);
        tp += 4;
    }
    store_weights(0, tex);
    store_weights(1, &tex[4 * (LOOKUP_BSPLINE_RES - 1)]);
    gl->ActiveTexture(unit);
    gl->TexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16, LOOKUP_BSPLINE_RES, 0, GL_RGBA,
                   GL_FLOAT, tex);
    gl->TexParameterf(GL_TEXTURE_1D, GL_TEXTURE_PRIORITY, 1.0);
    gl->TexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl->ActiveTexture(GL_TEXTURE0);
    free(tex);
}

#define NOISE_RES 2048

/**
 * \brief creates the 1D lookup texture needed to generate pseudo-random numbers.
 * \param unit texture unit to attach texture to
 */
static void gen_noise_lookup_tex(GL *gl, GLenum unit) {
    GLfloat *tex = calloc(NOISE_RES, sizeof(*tex));
    uint32_t lcg = 0x79381c11;
    int i;
    for (i = 0; i < NOISE_RES; i++)
        tex[i] = (double)i / (NOISE_RES - 1);
    for (i = 0; i < NOISE_RES - 1; i++) {
        int remain = NOISE_RES - i;
        int idx = i + (lcg >> 16) % remain;
        GLfloat tmp = tex[i];
        tex[i] = tex[idx];
        tex[idx] = tmp;
        lcg = lcg * 1664525 + 1013904223;
    }
    gl->ActiveTexture(unit);
    gl->TexImage1D(GL_TEXTURE_1D, 0, 1, NOISE_RES, 0, GL_RED, GL_FLOAT, tex);
    gl->TexParameterf(GL_TEXTURE_1D, GL_TEXTURE_PRIORITY, 1.0);
    gl->TexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl->ActiveTexture(GL_TEXTURE0);
    free(tex);
}

#define SAMPLE(dest, coord, texture) \
    "TEX textemp, " coord ", " texture ", $tex_type;\n" \
    "MOV " dest ", textemp.r;\n"

static const char bilin_filt_template[] =
    SAMPLE("yuv.$out_comp","fragment.texcoord[$in_tex]","texture[$in_tex]");

#define BICUB_FILT_MAIN \
    /* first y-interpolation */ \
    "ADD coord, fragment.texcoord[$in_tex].xyxy, cdelta.xyxw;\n" \
    "ADD coord2, fragment.texcoord[$in_tex].xyxy, cdelta.zyzw;\n" \
    SAMPLE("a.r","coord.xyxy","texture[$in_tex]") \
    SAMPLE("a.g","coord.zwzw","texture[$in_tex]") \
    /* second y-interpolation */ \
    SAMPLE("b.r","coord2.xyxy","texture[$in_tex]") \
    SAMPLE("b.g","coord2.zwzw","texture[$in_tex]") \
    "LRP a.b, parmy.b, a.rrrr, a.gggg;\n" \
    "LRP a.a, parmy.b, b.rrrr, b.gggg;\n" \
    /* x-interpolation */ \
    "LRP yuv.$out_comp, parmx.b, a.bbbb, a.aaaa;\n"

static const char bicub_filt_template_2D[] =
    "MAD coord.xy, fragment.texcoord[$in_tex], {$texw, $texh}, {0.5, 0.5};\n"
    "TEX parmx, coord.x, texture[$texs], 1D;\n"
    "MUL cdelta.xz, parmx.rrgg, {-$ptw, 0, $ptw, 0};\n"
    "TEX parmy, coord.y, texture[$texs], 1D;\n"
    "MUL cdelta.yw, parmy.rrgg, {0, -$pth, 0, $pth};\n"
    BICUB_FILT_MAIN;

static const char bicub_filt_template_RECT[] =
    "ADD coord, fragment.texcoord[$in_tex], {0.5, 0.5};\n"
    "TEX parmx, coord.x, texture[$texs], 1D;\n"
    "MUL cdelta.xz, parmx.rrgg, {-1, 0, 1, 0};\n"
    "TEX parmy, coord.y, texture[$texs], 1D;\n"
    "MUL cdelta.yw, parmy.rrgg, {0, -1, 0, 1};\n"
    BICUB_FILT_MAIN;

#define CALCWEIGHTS(t, s) \
    "MAD "t ", {-0.5, 0.1666, 0.3333, -0.3333}, "s ", {1, 0, -0.5, 0.5};\n" \
    "MAD "t ", "t ", "s ", {0, 0, -0.5, 0.5};\n" \
    "MAD "t ", "t ", "s ", {-0.6666, 0, 0.8333, 0.1666};\n" \
    "RCP a.x, "t ".z;\n" \
    "RCP a.y, "t ".w;\n" \
    "MAD "t ".xy, "t ".xyxy, a.xyxy, {1, 1, 0, 0};\n" \
    "ADD "t ".x, "t ".xxxx, "s ";\n" \
    "SUB "t ".y, "t ".yyyy, "s ";\n"

static const char bicub_notex_filt_template_2D[] =
    "MAD coord.xy, fragment.texcoord[$in_tex], {$texw, $texh}, {0.5, 0.5};\n"
    "FRC coord.xy, coord.xyxy;\n"
    CALCWEIGHTS("parmx", "coord.xxxx")
    "MUL cdelta.xz, parmx.rrgg, {-$ptw, 0, $ptw, 0};\n"
    CALCWEIGHTS("parmy", "coord.yyyy")
    "MUL cdelta.yw, parmy.rrgg, {0, -$pth, 0, $pth};\n"
    BICUB_FILT_MAIN;

static const char bicub_notex_filt_template_RECT[] =
    "ADD coord, fragment.texcoord[$in_tex], {0.5, 0.5};\n"
    "FRC coord.xy, coord.xyxy;\n"
    CALCWEIGHTS("parmx", "coord.xxxx")
    "MUL cdelta.xz, parmx.rrgg, {-1, 0, 1, 0};\n"
    CALCWEIGHTS("parmy", "coord.yyyy")
    "MUL cdelta.yw, parmy.rrgg, {0, -1, 0, 1};\n"
    BICUB_FILT_MAIN;

#define BICUB_X_FILT_MAIN \
    "ADD coord.xy, fragment.texcoord[$in_tex].xyxy, cdelta.xyxy;\n" \
    "ADD coord2.xy, fragment.texcoord[$in_tex].xyxy, cdelta.zyzy;\n" \
    SAMPLE("a.r","coord","texture[$in_tex]") \
    SAMPLE("b.r","coord2","texture[$in_tex]") \
    /* x-interpolation */ \
    "LRP yuv.$out_comp, parmx.b, a.rrrr, b.rrrr;\n"

static const char bicub_x_filt_template_2D[] =
    "MAD coord.x, fragment.texcoord[$in_tex], {$texw}, {0.5};\n"
    "TEX parmx, coord, texture[$texs], 1D;\n"
    "MUL cdelta.xyz, parmx.rrgg, {-$ptw, 0, $ptw};\n"
    BICUB_X_FILT_MAIN;

static const char bicub_x_filt_template_RECT[] =
    "ADD coord.x, fragment.texcoord[$in_tex], {0.5};\n"
    "TEX parmx, coord, texture[$texs], 1D;\n"
    "MUL cdelta.xyz, parmx.rrgg, {-1, 0, 1};\n"
    BICUB_X_FILT_MAIN;

static const char unsharp_filt_template[] =
    "PARAM dcoord$out_comp = {$ptw_05, $pth_05, $ptw_05, -$pth_05};\n"
    "ADD coord, fragment.texcoord[$in_tex].xyxy, dcoord$out_comp;\n"
    "SUB coord2, fragment.texcoord[$in_tex].xyxy, dcoord$out_comp;\n"
    SAMPLE("a.r","fragment.texcoord[$in_tex]","texture[$in_tex]")
    SAMPLE("b.r","coord.xyxy","texture[$in_tex]")
    SAMPLE("b.g","coord.zwzw","texture[$in_tex]")
    "ADD b.r, b.r, b.g;\n"
    SAMPLE("b.b","coord2.xyxy","texture[$in_tex]")
    SAMPLE("b.g","coord2.zwzw","texture[$in_tex]")
    "DP3 b, b, {0.25, 0.25, 0.25};\n"
    "SUB b.r, a.r, b.r;\n"
    "MAD textemp.r, b.r, {$strength}, a.r;\n"
    "MOV yuv.$out_comp, textemp.r;\n";

static const char unsharp_filt_template2[] =
    "PARAM dcoord$out_comp = {$ptw_12, $pth_12, $ptw_12, -$pth_12};\n"
    "PARAM dcoord2$out_comp = {$ptw_15, 0, 0, $pth_15};\n"
    "ADD coord, fragment.texcoord[$in_tex].xyxy, dcoord$out_comp;\n"
    "SUB coord2, fragment.texcoord[$in_tex].xyxy, dcoord$out_comp;\n"
    SAMPLE("a.r","fragment.texcoord[$in_tex]","texture[$in_tex]")
    SAMPLE("b.r","coord.xyxy","texture[$in_tex]")
    SAMPLE("b.g","coord.zwzw","texture[$in_tex]")
    "ADD b.r, b.r, b.g;\n"
    SAMPLE("b.b","coord2.xyxy","texture[$in_tex]")
    SAMPLE("b.g","coord2.zwzw","texture[$in_tex]")
    "ADD b.r, b.r, b.b;\n"
    "ADD b.a, b.r, b.g;\n"
    "ADD coord, fragment.texcoord[$in_tex].xyxy, dcoord2$out_comp;\n"
    "SUB coord2, fragment.texcoord[$in_tex].xyxy, dcoord2$out_comp;\n"
    SAMPLE("b.r","coord.xyxy","texture[$in_tex]")
    SAMPLE("b.g","coord.zwzw","texture[$in_tex]")
    "ADD b.r, b.r, b.g;\n"
    SAMPLE("b.b","coord2.xyxy","texture[$in_tex]")
    SAMPLE("b.g","coord2.zwzw","texture[$in_tex]")
    "DP4 b.r, b, {-0.1171875, -0.1171875, -0.1171875, -0.09765625};\n"
    "MAD b.r, a.r, {0.859375}, b.r;\n"
    "MAD textemp.r, b.r, {$strength}, a.r;\n"
    "MOV yuv.$out_comp, textemp.r;\n";

static const char yuv_prog_template[] =
    "PARAM ycoef = {$cm11, $cm21, $cm31};\n"
    "PARAM ucoef = {$cm12, $cm22, $cm32};\n"
    "PARAM vcoef = {$cm13, $cm23, $cm33};\n"
    "PARAM offsets = {$cm14, $cm24, $cm34};\n"
    "TEMP res;\n"
    "MAD res.rgb, yuv.rrrr, ycoef, offsets;\n"
    "MAD res.rgb, yuv.gggg, ucoef, res;\n"
    "MAD res.rgb, yuv.bbbb, vcoef, res;\n";

static const char yuv_pow_prog_template[] =
    "PARAM ycoef = {$cm11, $cm21, $cm31};\n"
    "PARAM ucoef = {$cm12, $cm22, $cm32};\n"
    "PARAM vcoef = {$cm13, $cm23, $cm33};\n"
    "PARAM offsets = {$cm14, $cm24, $cm34};\n"
    "PARAM gamma = {$gamma_r, $gamma_g, $gamma_b};\n"
    "TEMP res;\n"
    "MAD res.rgb, yuv.rrrr, ycoef, offsets;\n"
    "MAD res.rgb, yuv.gggg, ucoef, res;\n"
    "MAD_SAT res.rgb, yuv.bbbb, vcoef, res;\n"
    "POW res.r, res.r, gamma.r;\n"
    "POW res.g, res.g, gamma.g;\n"
    "POW res.b, res.b, gamma.b;\n";

static const char yuv_lookup_prog_template[] =
    "PARAM ycoef = {$cm11, $cm21, $cm31, 0};\n"
    "PARAM ucoef = {$cm12, $cm22, $cm32, 0};\n"
    "PARAM vcoef = {$cm13, $cm23, $cm33, 0};\n"
    "PARAM offsets = {$cm14, $cm24, $cm34, 0.125};\n"
    "TEMP res;\n"
    "MAD res, yuv.rrrr, ycoef, offsets;\n"
    "MAD res.rgb, yuv.gggg, ucoef, res;\n"
    "MAD res.rgb, yuv.bbbb, vcoef, res;\n"
    "TEX res.r, res.raaa, texture[$conv_tex0], 2D;\n"
    "ADD res.a, res.a, 0.25;\n"
    "TEX res.g, res.gaaa, texture[$conv_tex0], 2D;\n"
    "ADD res.a, res.a, 0.25;\n"
    "TEX res.b, res.baaa, texture[$conv_tex0], 2D;\n";

static const char yuv_lookup3d_prog_template[] =
    "TEMP res;\n"
    "TEX res, yuv, texture[$conv_tex0], 3D;\n";

static const char noise_filt_template[] =
    "MUL coord.xy, fragment.texcoord[0], {$noise_sx, $noise_sy};\n"
    "TEMP rand;\n"
    "TEX rand.r, coord.x, texture[$noise_filt_tex], 1D;\n"
    "ADD rand.r, rand.r, coord.y;\n"
    "TEX rand.r, rand.r, texture[$noise_filt_tex], 1D;\n"
    "MAD res.rgb, rand.rrrr, {$noise_str, $noise_str, $noise_str}, res;\n";

/**
 * \brief creates and initializes helper textures needed for scaling texture read
 * \param scaler scaler type to create texture for
 * \param texu contains next free texture unit number
 * \param texs texture unit ids for the scaler are stored in this array
 */
static void create_scaler_textures(GL *gl, int scaler, int *texu, char *texs)
{
    switch (scaler) {
    case YUV_SCALER_BILIN:
    case YUV_SCALER_BICUB_NOTEX:
    case YUV_SCALER_UNSHARP:
    case YUV_SCALER_UNSHARP2:
        break;
    case YUV_SCALER_BICUB:
    case YUV_SCALER_BICUB_X:
        texs[0] = (*texu)++;
        gen_spline_lookup_tex(gl, GL_TEXTURE0 + texs[0]);
        texs[0] += '0';
        break;
    default:
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] unknown scaler type %i\n", scaler);
    }
}

//! resolution of texture for gamma lookup table
#define LOOKUP_RES 512
//! resolution for 3D yuv->rgb conversion lookup table
#define LOOKUP_3DRES 32
/**
 * \brief creates and initializes helper textures needed for yuv conversion
 * \param params struct containing parameters like brightness, gamma, ...
 * \param texu contains next free texture unit number
 * \param texs texture unit ids for the conversion are stored in this array
 */
static void create_conv_textures(GL *gl, gl_conversion_params_t *params,
                                 int *texu, char *texs)
{
    unsigned char *lookup_data = NULL;
    int conv = YUV_CONVERSION(params->type);
    switch (conv) {
    case YUV_CONVERSION_FRAGMENT:
    case YUV_CONVERSION_FRAGMENT_POW:
        break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
        texs[0] = (*texu)++;
        gl->ActiveTexture(GL_TEXTURE0 + texs[0]);
        lookup_data = malloc(4 * LOOKUP_RES);
        mp_gen_gamma_map(lookup_data, LOOKUP_RES, params->csp_params.rgamma);
        mp_gen_gamma_map(&lookup_data[LOOKUP_RES], LOOKUP_RES,
                         params->csp_params.ggamma);
        mp_gen_gamma_map(&lookup_data[2 * LOOKUP_RES], LOOKUP_RES,
                         params->csp_params.bgamma);
        glCreateClearTex(gl, GL_TEXTURE_2D, GL_LUMINANCE8, GL_LUMINANCE,
                         GL_UNSIGNED_BYTE, GL_LINEAR, LOOKUP_RES, 4, 0);
        glUploadTex(gl, GL_TEXTURE_2D, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                    lookup_data, LOOKUP_RES, 0, 0, LOOKUP_RES, 4, 0);
        gl->ActiveTexture(GL_TEXTURE0);
        texs[0] += '0';
        break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
    {
        int sz = LOOKUP_3DRES + 2; // texture size including borders
        if (!gl->TexImage3D) {
            mp_msg(MSGT_VO, MSGL_ERR, "[gl] Missing 3D texture function!\n");
            break;
        }
        texs[0] = (*texu)++;
        gl->ActiveTexture(GL_TEXTURE0 + texs[0]);
        lookup_data = malloc(3 * sz * sz * sz);
        mp_gen_yuv2rgb_map(&params->csp_params, lookup_data, LOOKUP_3DRES);
        glAdjustAlignment(gl, sz);
        gl->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        gl->TexImage3D(GL_TEXTURE_3D, 0, 3, sz, sz, sz, 1,
                       GL_RGB, GL_UNSIGNED_BYTE, lookup_data);
        gl->TexParameterf(GL_TEXTURE_3D, GL_TEXTURE_PRIORITY, 1.0);
        gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        gl->TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
        gl->ActiveTexture(GL_TEXTURE0);
        texs[0] += '0';
    }
    break;
    default:
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] unknown conversion type %i\n", conv);
    }
    free(lookup_data);
}

/**
 * \brief adds a scaling texture read at the current fragment program position
 * \param scaler type of scaler to insert
 * \param prog pointer to fragment program so far
 * \param texs array containing the texture unit identifiers for this scaler
 * \param in_tex texture unit the scaler should read from
 * \param out_comp component of the yuv variable the scaler stores the result in
 * \param rect if rectangular (pixel) adressing should be used for in_tex
 * \param texw width of the in_tex texture
 * \param texh height of the in_tex texture
 * \param strength strength of filter effect if the scaler does some kind of filtering
 */
static void add_scaler(int scaler, char **prog, char *texs,
                       char in_tex, char out_comp, int rect, int texw, int texh,
                       double strength)
{
    const char *ttype = rect ? "RECT" : "2D";
    const float ptw = rect ? 1.0 : 1.0 / texw;
    const float pth = rect ? 1.0 : 1.0 / texh;
    switch (scaler) {
    case YUV_SCALER_BILIN:
        append_template(prog, bilin_filt_template);
        break;
    case YUV_SCALER_BICUB:
        if (rect)
            append_template(prog, bicub_filt_template_RECT);
        else
            append_template(prog, bicub_filt_template_2D);
        break;
    case YUV_SCALER_BICUB_X:
        if (rect)
            append_template(prog, bicub_x_filt_template_RECT);
        else
            append_template(prog, bicub_x_filt_template_2D);
        break;
    case YUV_SCALER_BICUB_NOTEX:
        if (rect)
            append_template(prog, bicub_notex_filt_template_RECT);
        else
            append_template(prog, bicub_notex_filt_template_2D);
        break;
    case YUV_SCALER_UNSHARP:
        append_template(prog, unsharp_filt_template);
        break;
    case YUV_SCALER_UNSHARP2:
        append_template(prog, unsharp_filt_template2);
        break;
    }

    replace_var_char(prog, "texs", texs[0]);
    replace_var_char(prog, "in_tex", in_tex);
    replace_var_char(prog, "out_comp", out_comp);
    replace_var_str(prog, "tex_type", ttype);
    replace_var_float(prog, "texw", texw);
    replace_var_float(prog, "texh", texh);
    replace_var_float(prog, "ptw", ptw);
    replace_var_float(prog, "pth", pth);

    // this is silly, not sure if that couldn't be in the shader source instead
    replace_var_float(prog, "ptw_05", ptw * 0.5);
    replace_var_float(prog, "pth_05", pth * 0.5);
    replace_var_float(prog, "ptw_15", ptw * 1.5);
    replace_var_float(prog, "pth_15", pth * 1.5);
    replace_var_float(prog, "ptw_12", ptw * 1.2);
    replace_var_float(prog, "pth_12", pth * 1.2);

    replace_var_float(prog, "strength", strength);
}

static const struct {
    const char *name;
    GLenum cur;
    GLenum max;
} progstats[] = {
    {"instructions", 0x88A0, 0x88A1},
    {"native instructions", 0x88A2, 0x88A3},
    {"temporaries", 0x88A4, 0x88A5},
    {"native temporaries", 0x88A6, 0x88A7},
    {"parameters", 0x88A8, 0x88A9},
    {"native parameters", 0x88AA, 0x88AB},
    {"attribs", 0x88AC, 0x88AD},
    {"native attribs", 0x88AE, 0x88AF},
    {"ALU instructions", 0x8805, 0x880B},
    {"TEX instructions", 0x8806, 0x880C},
    {"TEX indirections", 0x8807, 0x880D},
    {"native ALU instructions", 0x8808, 0x880E},
    {"native TEX instructions", 0x8809, 0x880F},
    {"native TEX indirections", 0x880A, 0x8810},
    {NULL, 0, 0}
};

/**
 * \brief load the specified GPU Program
 * \param target program target to load into, only GL_FRAGMENT_PROGRAM is tested
 * \param prog program string
 * \return 1 on success, 0 otherwise
 */
static int loadGPUProgram(GL *gl, GLenum target, char *prog)
{
    int i;
    GLint cur = 0, max = 0, err = 0;
    if (!gl->ProgramString) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] Missing GPU program function\n");
        return 0;
    }
    gl->ProgramString(target, GL_PROGRAM_FORMAT_ASCII, strlen(prog), prog);
    gl->GetIntegerv(GL_PROGRAM_ERROR_POSITION, &err);
    if (err != -1) {
        mp_msg(MSGT_VO, MSGL_ERR,
               "[gl] Error compiling fragment program, make sure your card supports\n"
               "[gl]   GL_ARB_fragment_program (use glxinfo to check).\n"
               "[gl]   Error message:\n  %s at %.10s\n",
               gl->GetString(GL_PROGRAM_ERROR_STRING), &prog[err]);
        return 0;
    }
    if (!gl->GetProgramivARB || !mp_msg_test(MSGT_VO, MSGL_DBG2))
        return 1;
    mp_msg(MSGT_VO, MSGL_V, "[gl] Program statistics:\n");
    for (i = 0; progstats[i].name; i++) {
        gl->GetProgramivARB(target, progstats[i].cur, &cur);
        gl->GetProgramivARB(target, progstats[i].max, &max);
        mp_msg(MSGT_VO, MSGL_V, "[gl]   %s: %i/%i\n", progstats[i].name, cur,
               max);
    }
    return 1;
}

#define MAX_PROGSZ (1024 * 1024)

/**
 * \brief setup a fragment program that will do YUV->RGB conversion
 * \param parms struct containing parameters like conversion and scaler type,
 *              brightness, ...
 */
static void glSetupYUVFragprog(GL *gl, gl_conversion_params_t *params)
{
    int type = params->type;
    int texw = params->texw;
    int texh = params->texh;
    int rect = params->target == GL_TEXTURE_RECTANGLE;
    static const char prog_hdr[] =
        "!!ARBfp1.0\n"
        "OPTION ARB_precision_hint_fastest;\n"
        // all scaler variables must go here so they aren't defined
        // multiple times when the same scaler is used more than once
        "TEMP coord, coord2, cdelta, parmx, parmy, a, b, yuv, textemp;\n";
    char *yuv_prog = NULL;
    char **prog = &yuv_prog;
    int cur_texu = 3;
    char lum_scale_texs[1] = {0};
    char chrom_scale_texs[1] = {0};
    char conv_texs[1];
    char filt_texs[1] = {0};
    GLint i;
    // this is the conversion matrix, with y, u, v factors
    // for red, green, blue and the constant offsets
    float yuv2rgb[3][4];
    int noise = params->noise_strength != 0;
    create_conv_textures(gl, params, &cur_texu, conv_texs);
    create_scaler_textures(gl, YUV_LUM_SCALER(type), &cur_texu, lum_scale_texs);
    if (YUV_CHROM_SCALER(type) == YUV_LUM_SCALER(type))
        memcpy(chrom_scale_texs, lum_scale_texs, sizeof(chrom_scale_texs));
    else
        create_scaler_textures(gl, YUV_CHROM_SCALER(type), &cur_texu,
                               chrom_scale_texs);

    if (noise) {
        gen_noise_lookup_tex(gl, cur_texu);
        filt_texs[0] = '0' + cur_texu++;
    }

    gl->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &i);
    if (i < cur_texu)
        mp_msg(MSGT_VO, MSGL_ERR,
               "[gl] %i texture units needed for this type of YUV fragment support (found %i)\n",
               cur_texu, i);
    if (!gl->ProgramString) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] ProgramString function missing!\n");
        return;
    }
    append_template(prog, prog_hdr);
    add_scaler(YUV_LUM_SCALER(type), prog, lum_scale_texs,
               '0', 'r', rect, texw, texh, params->filter_strength);
    add_scaler(YUV_CHROM_SCALER(type), prog,
               chrom_scale_texs, '1', 'g', rect, params->chrom_texw,
               params->chrom_texh, params->filter_strength);
    add_scaler(YUV_CHROM_SCALER(type), prog,
               chrom_scale_texs, '2', 'b', rect, params->chrom_texw,
               params->chrom_texh, params->filter_strength);
    mp_get_yuv2rgb_coeffs(&params->csp_params, yuv2rgb);
    switch (YUV_CONVERSION(type)) {
    case YUV_CONVERSION_FRAGMENT:
        append_template(prog, yuv_prog_template);
        break;
    case YUV_CONVERSION_FRAGMENT_POW:
        append_template(prog, yuv_pow_prog_template);
        break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
        append_template(prog, yuv_lookup_prog_template);
        break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
        append_template(prog, yuv_lookup3d_prog_template);
        break;
    default:
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] unknown conversion type %i\n",
               YUV_CONVERSION(type));
        break;
    }
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            // "cmRC"
            char var[] = { 'c', 'm', '1' + r, '1' + c, '\0' };
            replace_var_float(prog, var, yuv2rgb[r][c]);
        }
    }
    replace_var_float(prog, "gamma_r", (float)1.0 / params->csp_params.rgamma);
    replace_var_float(prog, "gamma_g", (float)1.0 / params->csp_params.ggamma);
    replace_var_float(prog, "gamma_b", (float)1.0 / params->csp_params.bgamma);
    replace_var_char(prog, "conv_tex0", conv_texs[0]);

    if (noise) {
        // 1.0 strength is suitable for dithering 8 to 6 bit
        double str = params->noise_strength * (1.0 / 64);
        double scale_x = (double)NOISE_RES / texw;
        double scale_y = (double)NOISE_RES / texh;
        if (rect) {
            scale_x /= texw;
            scale_y /= texh;
        }
        append_template(prog, noise_filt_template);
        replace_var_float(prog, "noise_sx", scale_x);
        replace_var_float(prog, "noise_sy", scale_y);
        replace_var_char(prog, "noise_filt_tex", filt_texs[0]);
        replace_var_float(prog, "noise_str", str);
    }

    append_template(prog, "MOV result.color.rgb, res;\nEND");

    mp_msg(MSGT_VO, MSGL_DBG2, "[gl] generated fragment program:\n%s\n",
           yuv_prog);
    loadGPUProgram(gl, GL_FRAGMENT_PROGRAM, yuv_prog);
    talloc_free(yuv_prog);
}

/**
 * \brief detect the best YUV->RGB conversion method available
 */
static int glAutodetectYUVConversion(GL *gl)
{
    const char *extensions = gl->GetString(GL_EXTENSIONS);
    if (!extensions || !gl->MultiTexCoord2f)
        return YUV_CONVERSION_NONE;
    if (strstr(extensions, "GL_ARB_fragment_program"))
        return YUV_CONVERSION_FRAGMENT;
    if (strstr(extensions, "GL_ATI_text_fragment_shader"))
        return YUV_CONVERSION_TEXT_FRAGMENT;
    if (strstr(extensions, "GL_ATI_fragment_shader"))
        return YUV_CONVERSION_COMBINERS_ATI;
    return YUV_CONVERSION_NONE;
}

/**
 * \brief setup YUV->RGB conversion
 * \param parms struct containing parameters like conversion and scaler type,
 *              brightness, ...
 * \ingroup glconversion
 */
static void glSetupYUVConversion(GL *gl, gl_conversion_params_t *params)
{
    if (params->chrom_texw == 0)
        params->chrom_texw = 1;
    if (params->chrom_texh == 0)
        params->chrom_texh = 1;
    switch (YUV_CONVERSION(params->type)) {
    case YUV_CONVERSION_COMBINERS_ATI:
        glSetupYUVFragmentATI(gl, &params->csp_params, 0);
        break;
    case YUV_CONVERSION_TEXT_FRAGMENT:
        glSetupYUVFragmentATI(gl, &params->csp_params, 1);
        break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
    case YUV_CONVERSION_FRAGMENT:
    case YUV_CONVERSION_FRAGMENT_POW:
        glSetupYUVFragprog(gl, params);
        break;
    case YUV_CONVERSION_NONE:
        break;
    default:
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] unknown conversion type %i\n",
               YUV_CONVERSION(params->type));
    }
}

/**
 * \brief enable the specified YUV conversion
 * \param target texture target for Y, U and V textures (e.g. GL_TEXTURE_2D)
 * \param type type of YUV conversion
 * \ingroup glconversion
 */
static void glEnableYUVConversion(GL *gl, GLenum target, int type)
{
    switch (YUV_CONVERSION(type)) {
    case YUV_CONVERSION_COMBINERS_ATI:
        gl->ActiveTexture(GL_TEXTURE1);
        gl->Enable(target);
        gl->ActiveTexture(GL_TEXTURE2);
        gl->Enable(target);
        gl->ActiveTexture(GL_TEXTURE0);
        gl->Enable(GL_FRAGMENT_SHADER_ATI);
        break;
    case YUV_CONVERSION_TEXT_FRAGMENT:
        gl->ActiveTexture(GL_TEXTURE1);
        gl->Enable(target);
        gl->ActiveTexture(GL_TEXTURE2);
        gl->Enable(target);
        gl->ActiveTexture(GL_TEXTURE0);
        gl->Enable(GL_TEXT_FRAGMENT_SHADER_ATI);
        break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
    case YUV_CONVERSION_FRAGMENT_POW:
    case YUV_CONVERSION_FRAGMENT:
    case YUV_CONVERSION_NONE:
        gl->Enable(GL_FRAGMENT_PROGRAM);
        break;
    }
}

/**
 * \brief disable the specified YUV conversion
 * \param target texture target for Y, U and V textures (e.g. GL_TEXTURE_2D)
 * \param type type of YUV conversion
 * \ingroup glconversion
 */
static void glDisableYUVConversion(GL *gl, GLenum target, int type)
{
    switch (YUV_CONVERSION(type)) {
    case YUV_CONVERSION_COMBINERS_ATI:
        gl->ActiveTexture(GL_TEXTURE1);
        gl->Disable(target);
        gl->ActiveTexture(GL_TEXTURE2);
        gl->Disable(target);
        gl->ActiveTexture(GL_TEXTURE0);
        gl->Disable(GL_FRAGMENT_SHADER_ATI);
        break;
    case YUV_CONVERSION_TEXT_FRAGMENT:
        gl->Disable(GL_TEXT_FRAGMENT_SHADER_ATI);
        // HACK: at least the Mac OS X 10.5 PPC Radeon drivers are broken and
        // without this disable the texture units while the program is still
        // running (10.4 PPC seems to work without this though).
        gl->Flush();
        gl->ActiveTexture(GL_TEXTURE1);
        gl->Disable(target);
        gl->ActiveTexture(GL_TEXTURE2);
        gl->Disable(target);
        gl->ActiveTexture(GL_TEXTURE0);
        break;
    case YUV_CONVERSION_FRAGMENT_LOOKUP3D:
    case YUV_CONVERSION_FRAGMENT_LOOKUP:
    case YUV_CONVERSION_FRAGMENT_POW:
    case YUV_CONVERSION_FRAGMENT:
    case YUV_CONVERSION_NONE:
        gl->Disable(GL_FRAGMENT_PROGRAM);
        break;
    }
}


/**
 * \brief draw a texture part at given 2D coordinates
 * \param x screen top coordinate
 * \param y screen left coordinate
 * \param w screen width coordinate
 * \param h screen height coordinate
 * \param tx texture top coordinate in pixels
 * \param ty texture left coordinate in pixels
 * \param tw texture part width in pixels
 * \param th texture part height in pixels
 * \param sx width of texture in pixels
 * \param sy height of texture in pixels
 * \param rect_tex whether this texture uses texture_rectangle extension
 * \param is_yv12 if != 0, also draw the textures from units 1 and 2,
 *                bits 8 - 15 and 16 - 23 specify the x and y scaling of those textures
 * \param flip flip the texture upside down
 * \ingroup gltexture
 */
static void glDrawTex(GL *gl, GLfloat x, GLfloat y, GLfloat w, GLfloat h,
                      GLfloat tx, GLfloat ty, GLfloat tw, GLfloat th,
                      int sx, int sy, int rect_tex, int is_yv12, int flip)
{
    int chroma_x_shift = (is_yv12 >>  8) & 31;
    int chroma_y_shift = (is_yv12 >> 16) & 31;
    GLfloat xscale = 1 << chroma_x_shift;
    GLfloat yscale = 1 << chroma_y_shift;
    GLfloat tx2 = tx / xscale, ty2 = ty / yscale, tw2 = tw / xscale, th2 = th / yscale;
    if (!rect_tex) {
        tx /= sx;
        ty /= sy;
        tw /= sx;
        th /= sy;
        tx2 = tx, ty2 = ty, tw2 = tw, th2 = th;
    }
    if (flip) {
        y += h;
        h = -h;
    }
    gl->Begin(GL_QUADS);
    gl->TexCoord2f(tx, ty);
    if (is_yv12) {
        gl->MultiTexCoord2f(GL_TEXTURE1, tx2, ty2);
        gl->MultiTexCoord2f(GL_TEXTURE2, tx2, ty2);
    }
    gl->Vertex2f(x, y);
    gl->TexCoord2f(tx, ty + th);
    if (is_yv12) {
        gl->MultiTexCoord2f(GL_TEXTURE1, tx2, ty2 + th2);
        gl->MultiTexCoord2f(GL_TEXTURE2, tx2, ty2 + th2);
    }
    gl->Vertex2f(x, y + h);
    gl->TexCoord2f(tx + tw, ty + th);
    if (is_yv12) {
        gl->MultiTexCoord2f(GL_TEXTURE1, tx2 + tw2, ty2 + th2);
        gl->MultiTexCoord2f(GL_TEXTURE2, tx2 + tw2, ty2 + th2);
    }
    gl->Vertex2f(x + w, y + h);
    gl->TexCoord2f(tx + tw, ty);
    if (is_yv12) {
        gl->MultiTexCoord2f(GL_TEXTURE1, tx2 + tw2, ty2);
        gl->MultiTexCoord2f(GL_TEXTURE2, tx2 + tw2, ty2);
    }
    gl->Vertex2f(x + w, y);
    gl->End();
}

static void resize(struct vo *vo, int x, int y)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    mp_msg(MSGT_VO, MSGL_V, "[gl] Resize: %dx%d\n", x, y);
    gl->Viewport(0, 0, x, y);

    vo_get_src_dst_rects(vo, &p->src_rect, &p->dst_rect, &p->osd_res);

    gl->MatrixMode(GL_MODELVIEW);
    gl->LoadIdentity();
    gl->Ortho(0, vo->dwidth, vo->dheight, 0, -1, 1);

    gl->Clear(GL_COLOR_BUFFER_BIT);
    vo->want_redraw = true;
}

static void texSize(struct vo *vo, int w, int h, int *texw, int *texh)
{
    struct gl_priv *p = vo->priv;

    if (p->use_rectangle) {
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
    if (p->ati_hack)
        *texw = (*texw + 511) & ~511;
}

//! maximum size of custom fragment program
#define MAX_CUSTOM_PROG_SIZE (1024 * 1024)
static void update_yuvconv(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    struct mp_csp_params cparams = { .colorspace = p->colorspace };
    mp_csp_copy_equalizer_values(&cparams, &p->video_eq);
    gl_conversion_params_t params = {
        p->target, p->yuvconvtype, cparams,
        p->texture_width, p->texture_height, 0, 0, p->filter_strength,
        p->noise_strength
    };
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(p->image_format);
    int depth = desc.plane_bits;
    params.chrom_texw = params.texw >> desc.chroma_xs;
    params.chrom_texh = params.texh >> desc.chroma_ys;
    params.csp_params.input_bits = depth;
    params.csp_params.texture_bits = depth+7 & ~7;
    glSetupYUVConversion(gl, &params);
    if (p->custom_prog) {
        FILE *f = fopen(p->custom_prog, "rb");
        if (!f) {
            mp_msg(MSGT_VO, MSGL_WARN,
                   "[gl] Could not read customprog %s\n", p->custom_prog);
        } else {
            char *prog = calloc(1, MAX_CUSTOM_PROG_SIZE + 1);
            fread(prog, 1, MAX_CUSTOM_PROG_SIZE, f);
            fclose(f);
            loadGPUProgram(gl, GL_FRAGMENT_PROGRAM, prog);
            free(prog);
        }
        gl->ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM, 0,
                                  1.0 / p->texture_width,
                                  1.0 / p->texture_height,
                                  p->texture_width, p->texture_height);
    }
    if (p->custom_tex) {
        FILE *f = fopen(p->custom_tex, "rb");
        if (!f) {
            mp_msg(MSGT_VO, MSGL_WARN,
                   "[gl] Could not read customtex %s\n", p->custom_tex);
        } else {
            int width, height, maxval;
            gl->ActiveTexture(GL_TEXTURE3);
            if (glCreatePPMTex(gl, p->custom_trect ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D,
                               0, p->custom_tlin ? GL_LINEAR : GL_NEAREST,
                               f, &width, &height, &maxval)) {
                gl->ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM, 1,
                                          1.0 / width, 1.0 / height,
                                          width, height);
            } else
                mp_msg(MSGT_VO, MSGL_WARN,
                       "[gl] Error parsing customtex %s\n", p->custom_tex);
            fclose(f);
            gl->ActiveTexture(GL_TEXTURE0);
        }
    }
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    if (!p->osd)
        return;

    struct mp_osd_res res = p->osd_res;

    if (p->scaled_osd) {
        res = (struct mp_osd_res) {
            .w = p->image_width,
            .h = p->image_height,
            .display_par = 1.0 / p->osd_res.video_par,
            .video_par = p->osd_res.video_par,
        };
        gl->MatrixMode(GL_MODELVIEW);
        gl->PushMatrix();
        // Setup image space -> screen space (assumes osd_res in screen space)
        int w = vo->dwidth - (p->osd_res.mr + p->osd_res.ml);
        int h = vo->dheight - (p->osd_res.mt + p->osd_res.mb);
        gl->Translated(p->osd_res.mr, p->osd_res.mt, 0);
        gl->Scaled(1.0 / res.w * w, 1.0 / res.h * h, 1);
    }

    gl->Color4ub((p->osd_color >> 16) & 0xff, (p->osd_color >> 8) & 0xff,
                 p->osd_color & 0xff, 0xff - (p->osd_color >> 24));

    mpgl_osd_draw_legacy(p->osd, osd, res);

    if (p->scaled_osd)
        gl->PopMatrix();
}

/**
 * \brief uninitialize OpenGL context, freeing textures, buffers etc.
 */
static void uninitGl(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    if (!gl)
        return;

    int i = 0;
    if (gl->DeletePrograms && p->fragprog)
        gl->DeletePrograms(1, &p->fragprog);
    p->fragprog = 0;
    while (p->default_texs[i] != 0)
        i++;
    if (i)
        gl->DeleteTextures(i, p->default_texs);
    p->default_texs[0] = 0;
    if (p->osd)
        mpgl_osd_destroy(p->osd);
    p->osd = NULL;
    p->buffer = 0;
    p->buffersize = 0;
    p->bufferptr = NULL;
    if (gl->DeleteBuffers && p->buffer_uv[0])
        gl->DeleteBuffers(2, p->buffer_uv);
    p->buffer_uv[0] = p->buffer_uv[1] = 0;
    p->buffersize_uv = 0;
    p->bufferptr_uv[0] = p->bufferptr_uv[1] = 0;
    p->err_shown = 0;
}

static void autodetectGlExtensions(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    const char *extensions = gl->GetString(GL_EXTENSIONS);
    const char *vendor     = gl->GetString(GL_VENDOR);
    const char *version    = gl->GetString(GL_VERSION);
    const char *renderer   = gl->GetString(GL_RENDERER);
    int is_ati = vendor && strstr(vendor, "ATI") != NULL;
    int ati_broken_pbo = 0;
    mp_msg(MSGT_VO, MSGL_V, "[gl] Running on OpenGL '%s' by '%s', version '%s'\n",
           renderer, vendor, version);
    if (is_ati && strncmp(version, "2.1.", 4) == 0) {
        int ver = atoi(version + 4);
        mp_msg(MSGT_VO, MSGL_V, "[gl] Detected ATI driver version: %i\n", ver);
        ati_broken_pbo = ver && ver < 8395;
    }
    if (p->ati_hack == -1)
        p->ati_hack = ati_broken_pbo;
    if (p->force_pbo == -1) {
        p->force_pbo = 0;
        if (extensions && strstr(extensions, "_pixel_buffer_object"))
            p->force_pbo = is_ati;
    }
    p->have_texture_rg = extensions && strstr(extensions, "GL_ARB_texture_rg");
    if (p->use_rectangle == -1) {
        p->use_rectangle = 0;
        if (extensions) {
//      if (strstr(extensions, "_texture_non_power_of_two"))
            if (strstr(extensions, "_texture_rectangle"))
                p->use_rectangle = renderer
                    && strstr(renderer, "Mesa DRI R200") ? 1 : 0;
        }
    }
    if (p->use_yuv == -1)
        p->use_yuv = glAutodetectYUVConversion(gl);

    int eq_caps = 0;
    int yuv_mask = (1 << p->use_yuv);
    if (!(yuv_mask & MASK_NOT_COMBINERS)) {
        // combiners
        eq_caps = (1 << MP_CSP_EQ_HUE) | (1 << MP_CSP_EQ_SATURATION);
    } else if (yuv_mask & MASK_ALL_YUV) {
        eq_caps = MP_CSP_EQ_CAPS_COLORMATRIX;
        if (yuv_mask & MASK_GAMMA_SUPPORT)
            eq_caps |= MP_CSP_EQ_CAPS_GAMMA;
    }
    p->video_eq.capabilities = eq_caps;

    {
        int target = p->use_rectangle == 1 ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
        GLint gl_texfmt;
        GLenum gl_format, gl_type;
        glFindFormat(IMGFMT_420P16, p->have_texture_rg, NULL, &gl_texfmt,
                     &gl_format, &gl_type);
        glCreateClearTex(gl, target, gl_texfmt, gl_format, gl_type,
                         GL_LINEAR, 64, 64, 0);
        int tex_size_token = p->have_texture_rg ? GL_TEXTURE_RED_SIZE
                                                : GL_TEXTURE_INTENSITY_SIZE;
        GLint size = 8;
        gl->GetTexLevelParameteriv(target, 0, tex_size_token, &size);
        mp_msg(MSGT_VO, MSGL_V, "[gl] 16 bit texture depth: %d.\n", size);
        p->max_tex_component_size = size;
    }

    if (is_ati && (p->lscale == 1 || p->lscale == 2 || p->cscale == 1 || p->cscale == 2))
        mp_msg(MSGT_VO, MSGL_WARN, "[gl] Selected scaling mode may be broken on"
               " ATI cards.\n"
               "Tell _them_ to fix GL_REPEAT if you have issues.\n");
    mp_msg(MSGT_VO, MSGL_V, "[gl] Settings after autodetection: ati-hack = %i, "
           "force-pbo = %i, rectangle = %i, yuv = %i\n",
           p->ati_hack, p->force_pbo, p->use_rectangle, p->use_yuv);
}

static GLint get_scale_type(struct vo *vo, int chroma)
{
    struct gl_priv *p = vo->priv;

    int nearest = (chroma ? p->cscale : p->lscale) & 64;
    if (nearest)
        return p->mipmap_gen ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
    return p->mipmap_gen ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR;
}

// Return the high byte of the value that represents white in chroma (U/V)
static int get_chroma_clear_val(int bit_depth)
{
    return 1 << (bit_depth - 1 & 7);
}

/**
 * \brief Initialize a (new or reused) OpenGL context.
 * set global gl-related variables to their default values
 */
static int initGl(struct vo *vo, uint32_t d_width, uint32_t d_height)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    GLint scale_type = get_scale_type(vo, 0);
    autodetectGlExtensions(vo);
    p->target = p->use_rectangle == 1 ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
    p->yuvconvtype = SET_YUV_CONVERSION(p->use_yuv) |
                     SET_YUV_LUM_SCALER(p->lscale) |
                     SET_YUV_CHROM_SCALER(p->cscale);

    texSize(vo, p->image_width, p->image_height,
            &p->texture_width, &p->texture_height);

    gl->Disable(GL_BLEND);
    gl->Disable(GL_DEPTH_TEST);
    gl->DepthMask(GL_FALSE);
    gl->Disable(GL_CULL_FACE);
    gl->Enable(p->target);
    gl->DrawBuffer(GL_BACK);
    gl->TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    mp_msg(MSGT_VO, MSGL_V, "[gl] Creating %dx%d texture...\n",
           p->texture_width, p->texture_height);

    glCreateClearTex(gl, p->target, p->texfmt, p->gl_format,
                     p->gl_type, scale_type,
                     p->texture_width, p->texture_height, 0);

    if (p->mipmap_gen)
        gl->TexParameteri(p->target, GL_GENERATE_MIPMAP, GL_TRUE);

    if (p->is_yuv) {
        struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(p->image_format);
        int i;
        int xs = desc.chroma_xs, ys = desc.chroma_ys, depth = desc.plane_bits;
        scale_type = get_scale_type(vo, 1);
        int clear = get_chroma_clear_val(depth);
        gl->GenTextures(21, p->default_texs);
        p->default_texs[21] = 0;
        for (i = 0; i < 7; i++) {
            gl->ActiveTexture(GL_TEXTURE1 + i);
            gl->BindTexture(GL_TEXTURE_2D, p->default_texs[i]);
            gl->BindTexture(GL_TEXTURE_RECTANGLE, p->default_texs[i + 7]);
            gl->BindTexture(GL_TEXTURE_3D, p->default_texs[i + 14]);
        }
        gl->ActiveTexture(GL_TEXTURE1);
        glCreateClearTex(gl, p->target, p->texfmt, p->gl_format,
                         p->gl_type, scale_type,
                         p->texture_width >> xs, p->texture_height >> ys,
                         clear);
        if (p->mipmap_gen)
            gl->TexParameteri(p->target, GL_GENERATE_MIPMAP, GL_TRUE);
        gl->ActiveTexture(GL_TEXTURE2);
        glCreateClearTex(gl, p->target, p->texfmt, p->gl_format,
                         p->gl_type, scale_type,
                         p->texture_width >> xs, p->texture_height >> ys,
                         clear);
        if (p->mipmap_gen)
            gl->TexParameteri(p->target, GL_GENERATE_MIPMAP, GL_TRUE);
        gl->ActiveTexture(GL_TEXTURE0);
        gl->BindTexture(p->target, 0);
    }
    if (p->is_yuv || p->custom_prog) {
        if ((MASK_NOT_COMBINERS & (1 << p->use_yuv)) || p->custom_prog) {
            if (!gl->GenPrograms || !gl->BindProgram)
                mp_msg(MSGT_VO, MSGL_ERR,
                       "[gl] fragment program functions missing!\n");
            else {
                gl->GenPrograms(1, &p->fragprog);
                gl->BindProgram(GL_FRAGMENT_PROGRAM, p->fragprog);
            }
        }
        update_yuvconv(vo);
    }

    if (gl->BindTexture) {
        p->osd = mpgl_osd_init(gl, true);
        p->osd->scaled = p->scaled_osd;
    }

    resize(vo, d_width, d_height);

    gl->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    if (gl->SwapInterval && p->swap_interval >= 0)
        gl->SwapInterval(p->swap_interval);
    return 1;
}

static bool config_window(struct vo *vo, uint32_t d_width, uint32_t d_height,
                          uint32_t flags)
{
    struct gl_priv *p = vo->priv;

    if (p->stereo_mode == GL_3D_QUADBUFFER)
        flags |= VOFLAG_STEREO;

    int mpgl_caps = MPGL_CAP_GL_LEGACY;
    if (!p->allow_sw)
        mpgl_caps |= MPGL_CAP_NO_SW;
    return mpgl_config_window(p->glctx, mpgl_caps, d_width, d_height, flags);
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct gl_priv *p = vo->priv;

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(format);

    p->image_height = height;
    p->image_width = width;
    p->image_format = format;
    p->is_yuv = !!(desc.flags & MP_IMGFLAG_YUV_P);
    p->is_yuv |= (desc.chroma_xs << 8) | (desc.chroma_ys << 16);
    if (format == IMGFMT_Y8)
        p->is_yuv = 0;
    glFindFormat(format, p->have_texture_rg, NULL, &p->texfmt, &p->gl_format,
                 &p->gl_type);

    p->vo_flipped = !!(flags & VOFLAG_FLIPPING);

    if (vo->config_count)
        uninitGl(vo);

    if (!config_window(vo, d_width, d_height, flags))
        return -1;

    initGl(vo, vo->dwidth, vo->dheight);

    return 0;
}

static void check_events(struct vo *vo)
{
    struct gl_priv *p = vo->priv;

    int e = p->glctx->check_events(vo);
    if (e & VO_EVENT_RESIZE)
        resize(vo, vo->dwidth, vo->dheight);
    if (e & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
}

static void do_render(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

//  Enable(GL_TEXTURE_2D);
//  BindTexture(GL_TEXTURE_2D, texture_id);

    gl->Color4f(1, 1, 1, 1);
    if (p->is_yuv || p->custom_prog)
        glEnableYUVConversion(gl, p->target, p->yuvconvtype);
    int src_w = p->src_rect.x1 - p->src_rect.x0;
    int src_h = p->src_rect.y1 - p->src_rect.y0;
    int dst_w = p->dst_rect.x1 - p->dst_rect.x0;
    int dst_h = p->dst_rect.y1 - p->dst_rect.y0;
    if (p->stereo_mode) {
        glEnable3DLeft(gl, p->stereo_mode);
        glDrawTex(gl,
                  p->dst_rect.x0, p->dst_rect.y0, dst_w, dst_h,
                  p->src_rect.x0 / 2, p->src_rect.y0, src_w / 2, src_h,
                  p->texture_width, p->texture_height,
                  p->use_rectangle == 1, p->is_yuv,
                  p->mpi_flipped ^ p->vo_flipped);
        glEnable3DRight(gl, p->stereo_mode);
        glDrawTex(gl,
                  p->dst_rect.x0, p->dst_rect.y0, dst_w, dst_h,
                  p->src_rect.x0 / 2 + p->image_width / 2, p->src_rect.y0,
                  src_w / 2, src_h,
                  p->texture_width, p->texture_height,
                  p->use_rectangle == 1, p->is_yuv,
                  p->mpi_flipped ^ p->vo_flipped);
        glDisable3D(gl, p->stereo_mode);
    } else {
        glDrawTex(gl,
                  p->dst_rect.x0, p->dst_rect.y0, dst_w, dst_h,
                  p->src_rect.x0, p->src_rect.y0, src_w, src_h,
                  p->texture_width, p->texture_height,
                  p->use_rectangle == 1, p->is_yuv,
                  p->mpi_flipped ^ p->vo_flipped);
    }
    if (p->is_yuv || p->custom_prog)
        glDisableYUVConversion(gl, p->target, p->yuvconvtype);
}

static void flip_page(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    if (p->use_glFinish)
        gl->Finish();
    p->glctx->swapGlBuffers(p->glctx);

    if (p->dst_rect.x0 > 0|| p->dst_rect.y0 > 0 ||
        p->dst_rect.x1 < vo->dwidth || p->dst_rect.y1 < vo->dheight)
    {
        gl->Clear(GL_COLOR_BUFFER_BIT);
    }
}

static bool get_image(struct vo *vo, mp_image_t *mpi, int *th, bool *cplane)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    bool common_plane = false;

    int needed_size;
    if (!gl->GenBuffers || !gl->BindBuffer || !gl->BufferData || !gl->MapBuffer) {
        if (!p->err_shown)
            mp_msg(MSGT_VO, MSGL_ERR, "[gl] extensions missing for dr\n"
                   "Expect a _major_ speed penalty\n");
        p->err_shown = 1;
        return false;
    }
    int width = mpi->w, height = mpi->h;
    if (p->ati_hack) {
        width = p->texture_width;
        height = p->texture_height;
    }
    int avgbpp16 = 0;
    for (int p = 0; p < 4; p++)
        avgbpp16 += (16 * mpi->fmt.bpp[p]) >> mpi->fmt.xs[p] >> mpi->fmt.ys[p];
    int avgbpp = avgbpp16 / 16;
    mpi->stride[0] = width * avgbpp / 8;
    needed_size = mpi->stride[0] * height;
    if (!p->buffer)
        gl->GenBuffers(1, &p->buffer);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer);
    if (needed_size > p->buffersize) {
        p->buffersize = needed_size;
        gl->BufferData(GL_PIXEL_UNPACK_BUFFER, p->buffersize,
                        NULL, GL_DYNAMIC_DRAW);
    }
    if (!p->bufferptr)
        p->bufferptr = gl->MapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    mpi->planes[0] = p->bufferptr;
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    if (!mpi->planes[0]) {
        if (!p->err_shown)
            mp_msg(MSGT_VO, MSGL_ERR, "[gl] could not acquire buffer for dr\n"
                   "Expect a _major_ speed penalty\n");
        p->err_shown = 1;
        return false;
    }
    if (p->is_yuv) {
        // planar YUV
        struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(p->image_format);
        int xs = desc.chroma_xs, ys = desc.chroma_ys, depth = desc.plane_bits;
        int bp = (depth + 7) / 8;
        common_plane = true;
        mpi->stride[0] = width * bp;
        mpi->planes[1] = mpi->planes[0] + mpi->stride[0] * height;
        mpi->stride[1] = (width >> xs) * bp;
        mpi->planes[2] = mpi->planes[1] + mpi->stride[1] * (height >> ys);
        mpi->stride[2] = (width >> xs) * bp;
        if (p->ati_hack) {
            common_plane = false;
            if (!p->buffer_uv[0])
                gl->GenBuffers(2, p->buffer_uv);
            int buffer_size = mpi->stride[1] * height;
            if (buffer_size > p->buffersize_uv) {
                gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[0]);
                gl->BufferData(GL_PIXEL_UNPACK_BUFFER, buffer_size, NULL,
                               GL_DYNAMIC_DRAW);
                gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[1]);
                gl->BufferData(GL_PIXEL_UNPACK_BUFFER, buffer_size, NULL,
                               GL_DYNAMIC_DRAW);
                p->buffersize_uv = buffer_size;
            }
            if (!p->bufferptr_uv[0]) {
                gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[0]);
                p->bufferptr_uv[0] = gl->MapBuffer(GL_PIXEL_UNPACK_BUFFER,
                                                   GL_WRITE_ONLY);
                gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[1]);
                p->bufferptr_uv[1] = gl->MapBuffer(GL_PIXEL_UNPACK_BUFFER,
                                                   GL_WRITE_ONLY);
            }
            mpi->planes[1] = p->bufferptr_uv[0];
            mpi->planes[2] = p->bufferptr_uv[1];
        }
    }
    *th = height;
    *cplane = common_plane;
    return true;
}

static void clear_border(struct vo *vo, uint8_t *dst, int start, int stride,
                         int height, int full_height, int value)
{
    int right_border = stride - start;
    int bottom_border = full_height - height;
    while (height > 0) {
        if (right_border > 0)
            memset(dst + start, value, right_border);
        dst += stride;
        height--;
    }
    if (bottom_border > 0)
        memset(dst, value, stride * bottom_border);
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    int slice = p->slice_height;
    int stride[3];
    unsigned char *planes[3];
    mp_image_t mpi2 = *mpi;
    int w = mpi->w, h = mpi->h;
    int th = h;
    bool common_plane = false;
    bool pbo = false;
    mpi2.flags = 0;
    if (p->force_pbo && !p->bufferptr
        && get_image(vo, &mpi2, &th, &common_plane))
    {
        struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(p->image_format);
        int bp = desc.bytes[0];
        int xs = desc.chroma_xs, ys = desc.chroma_ys, depth = desc.plane_bits;
        memcpy_pic(mpi2.planes[0], mpi->planes[0], mpi->w * bp, mpi->h,
                   mpi2.stride[0], mpi->stride[0]);
        int uv_bytes = (mpi->w >> xs) * bp;
        if (p->is_yuv) {
            memcpy_pic(mpi2.planes[1], mpi->planes[1], uv_bytes, mpi->h >> ys,
                       mpi2.stride[1], mpi->stride[1]);
            memcpy_pic(mpi2.planes[2], mpi->planes[2], uv_bytes, mpi->h >> ys,
                       mpi2.stride[2], mpi->stride[2]);
        }
        if (p->ati_hack) {
            // since we have to do a full upload we need to clear the borders
            clear_border(vo, mpi2.planes[0], mpi->w * bp, mpi2.stride[0],
                         mpi->h, th, 0);
            if (p->is_yuv) {
                int clear = get_chroma_clear_val(depth);
                clear_border(vo, mpi2.planes[1], uv_bytes, mpi2.stride[1],
                             mpi->h >> ys, th >> ys, clear);
                clear_border(vo, mpi2.planes[2], uv_bytes, mpi2.stride[2],
                             mpi->h >> ys, th >> ys, clear);
            }
        }
        mpi = &mpi2;
        pbo = true;
    }
    stride[0] = mpi->stride[0];
    stride[1] = mpi->stride[1];
    stride[2] = mpi->stride[2];
    planes[0] = mpi->planes[0];
    planes[1] = mpi->planes[1];
    planes[2] = mpi->planes[2];
    p->mpi_flipped = stride[0] < 0;
    if (pbo) {
        intptr_t base = (intptr_t)planes[0];
        if (p->ati_hack) {
            w = p->texture_width;
            h = p->texture_height;
        }
        if (p->mpi_flipped)
            base += (mpi->h - 1) * stride[0];
        planes[0] -= base;
        planes[1] -= base;
        planes[2] -= base;
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer);
        gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        p->bufferptr = NULL;
        if (!common_plane)
            planes[0] = planes[1] = planes[2] = NULL;
        slice = 0; // always "upload" full texture
    }
    glUploadTex(gl, p->target, p->gl_format, p->gl_type, planes[0],
                stride[0], 0, 0, w, h, slice);
    if (p->is_yuv) {
        struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(p->image_format);
        int xs = desc.chroma_xs, ys = desc.chroma_ys;
        if (pbo && !common_plane) {
            gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[0]);
            gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            p->bufferptr_uv[0] = NULL;
        }
        gl->ActiveTexture(GL_TEXTURE1);
        glUploadTex(gl, p->target, p->gl_format, p->gl_type, planes[1],
                    stride[1], 0, 0, w >> xs, h >> ys, slice);
        if (pbo && !common_plane) {
            gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[1]);
            gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            p->bufferptr_uv[1] = NULL;
        }
        gl->ActiveTexture(GL_TEXTURE2);
        glUploadTex(gl, p->target, p->gl_format, p->gl_type, planes[2],
                    stride[2], 0, 0, w >> xs, h >> ys, slice);
        gl->ActiveTexture(GL_TEXTURE0);
    }
    if (pbo) {
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    do_render(vo);
}

static mp_image_t *get_screenshot(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    mp_image_t *image = mp_image_alloc(p->image_format, p->texture_width,
                                                        p->texture_height);

    glDownloadTex(gl, p->target, p->gl_format, p->gl_type, image->planes[0],
                  image->stride[0]);

    if (p->is_yuv) {
        gl->ActiveTexture(GL_TEXTURE1);
        glDownloadTex(gl, p->target, p->gl_format, p->gl_type, image->planes[1],
                      image->stride[1]);
        gl->ActiveTexture(GL_TEXTURE2);
        glDownloadTex(gl, p->target, p->gl_format, p->gl_type, image->planes[2],
                      image->stride[2]);
        gl->ActiveTexture(GL_TEXTURE0);
    }
    mp_image_set_size(image, p->image_width, p->image_height);
    mp_image_set_display_size(image, vo->aspdat.prew, vo->aspdat.preh);

    mp_image_set_colorspace_details(image, &p->colorspace);

    return image;
}

static int query_format(struct vo *vo, uint32_t format)
{
    struct gl_priv *p = vo->priv;

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(format);

    int depth = desc.plane_bits;
    int caps = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_FLIP;
    if (format == IMGFMT_RGB24 || format == IMGFMT_RGBA)
        return caps;
    if (p->use_yuv && (desc.flags & MP_IMGFLAG_YUV_P) &&
        (depth == 8 || depth == 16 ||
         p->max_tex_component_size >= 16 && glYUVLargeRange(p->use_yuv)) &&
        (depth <= 16 && (desc.flags & MP_IMGFLAG_NE)))
        return caps;
    // HACK, otherwise we get only b&w with some filters (e.g. -vf eq)
    // ideally MPlayer should be fixed instead not to use Y800 when it has the choice
    if (!p->use_yuv && (format == IMGFMT_Y8))
        return 0;
    if (!p->use_ycbcr && (format == IMGFMT_UYVY))
        return 0;
    if (p->many_fmts &&
        glFindFormat(format, p->have_texture_rg, NULL, NULL, NULL, NULL))
        return caps;
    return 0;
}

static void uninit(struct vo *vo)
{
    struct gl_priv *p = vo->priv;

    uninitGl(vo);
    free(p->custom_prog);
    p->custom_prog = NULL;
    free(p->custom_tex);
    p->custom_tex = NULL;
    mpgl_uninit(p->glctx);
    p->glctx = NULL;
    p->gl = NULL;
}

static int backend_valid(void *arg)
{
    return mpgl_find_backend(*(const char **)arg) >= -1;
}

static int preinit(struct vo *vo, const char *arg)
{
    struct gl_priv *p = talloc_zero(vo, struct gl_priv);
    vo->priv = p;

    *p = (struct gl_priv) {
        .many_fmts = 1,
        .use_yuv = -1,
        .colorspace = MP_CSP_DETAILS_DEFAULTS,
        .filter_strength = 0.5,
        .use_rectangle = -1,
        .ati_hack = -1,
        .force_pbo = -1,
        .swap_interval = 1,
        .custom_prog = NULL,
        .custom_tex = NULL,
        .custom_tlin = 1,
        .osd_color = 0xffffff,
    };

    char *backend_arg = NULL;

    //essentially unused; for legacy warnings only
    int user_colorspace = 0;
    int levelconv = -1;
    int aspect = -1;

    const opt_t subopts[] = {
        {"manyfmts",     OPT_ARG_BOOL, &p->many_fmts,    NULL},
        {"scaled-osd",   OPT_ARG_BOOL, &p->scaled_osd,   NULL},
        {"ycbcr",        OPT_ARG_BOOL, &p->use_ycbcr,    NULL},
        {"slice-height", OPT_ARG_INT,  &p->slice_height, int_non_neg},
        {"rectangle",    OPT_ARG_INT,  &p->use_rectangle,int_non_neg},
        {"yuv",          OPT_ARG_INT,  &p->use_yuv,      int_non_neg},
        {"lscale",       OPT_ARG_INT,  &p->lscale,       int_non_neg},
        {"cscale",       OPT_ARG_INT,  &p->cscale,       int_non_neg},
        {"filter-strength", OPT_ARG_FLOAT, &p->filter_strength, NULL},
        {"noise-strength", OPT_ARG_FLOAT, &p->noise_strength, NULL},
        {"ati-hack",     OPT_ARG_BOOL, &p->ati_hack,     NULL},
        {"force-pbo",    OPT_ARG_BOOL, &p->force_pbo,    NULL},
        {"glfinish",     OPT_ARG_BOOL, &p->use_glFinish, NULL},
        {"swapinterval", OPT_ARG_INT,  &p->swap_interval,NULL},
        {"customprog",   OPT_ARG_MSTRZ,&p->custom_prog,  NULL},
        {"customtex",    OPT_ARG_MSTRZ,&p->custom_tex,   NULL},
        {"customtlin",   OPT_ARG_BOOL, &p->custom_tlin,  NULL},
        {"customtrect",  OPT_ARG_BOOL, &p->custom_trect, NULL},
        {"mipmapgen",    OPT_ARG_BOOL, &p->mipmap_gen,   NULL},
        {"osdcolor",     OPT_ARG_INT,  &p->osd_color,    NULL},
        {"stereo",       OPT_ARG_INT,  &p->stereo_mode,  NULL},
        {"sw",           OPT_ARG_BOOL, &p->allow_sw,     NULL},
        {"backend",      OPT_ARG_MSTRZ,&backend_arg,     backend_valid},
        // Removed options.
        // They are only parsed to notify the user about the replacements.
        {"aspect",       OPT_ARG_BOOL, &aspect,          NULL},
        {"colorspace",   OPT_ARG_INT,  &user_colorspace, NULL},
        {"levelconv",    OPT_ARG_INT,  &levelconv,       NULL},
        {NULL}
    };

    if (subopt_parse(arg, subopts) != 0) {
        mp_msg(MSGT_VO, MSGL_FATAL,
               "\n-vo opengl_old command line help:\n"
               "Example: mpv -vo opengl_old:slice-height=4\n"
               "\nOptions:\n"
               "  nomanyfmts\n"
               "    Disable extended color formats for OpenGL 1.2 and later\n"
               "  slice-height=<0-...>\n"
               "    Slice size for texture transfer, 0 for whole image\n"
               "  scaled-osd\n"
               "    Render OSD at movie resolution and scale it\n"
               "  rectangle=<0,1,2>\n"
               "    0: use power-of-two textures\n"
               "    1: use texture_rectangle\n"
               "    2: use texture_non_power_of_two\n"
               "  ati-hack\n"
               "    Workaround ATI bug with PBOs\n"
               "  force-pbo\n"
               "    Force use of PBO even if this involves an extra memcpy\n"
               "  glfinish\n"
               "    Call glFinish() before swapping buffers\n"
               "  swapinterval=<n>\n"
               "    Interval in displayed frames between to buffer swaps.\n"
               "    1 is equivalent to enable VSYNC, 0 to disable VSYNC.\n"
               "    Requires GLX_SGI_swap_control support to work.\n"
               "  ycbcr\n"
               "    also try to use the GL_MESA_ycbcr_texture extension\n"
               "  yuv=<n>\n"
               "    0: use software YUV to RGB conversion.\n"
               "    1: deprecated, will use yuv=2 (used to be nVidia register combiners).\n"
               "    2: use fragment program.\n"
               "    3: use fragment program with gamma correction.\n"
               "    4: use fragment program with gamma correction via lookup.\n"
               "    5: use ATI-specific method (for older cards).\n"
               "    6: use lookup via 3D texture.\n"
               "  lscale=<n>\n"
               "    0: use standard bilinear scaling for luma.\n"
               "    1: use improved bicubic scaling for luma.\n"
               "    2: use cubic in X, linear in Y direction scaling for luma.\n"
               "    3: as 1 but without using a lookup texture.\n"
               "    4: experimental unsharp masking (sharpening).\n"
               "    5: experimental unsharp masking (sharpening) with larger radius.\n"
               "  cscale=<n>\n"
               "    as lscale but for chroma (2x slower with little visible effect).\n"
               "  filter-strength=<value>\n"
               "    set the effect strength for some lscale/cscale filters\n"
               "  noise-strength=<value>\n"
               "    set how much noise to add. 1.0 is suitable for dithering to 6 bit.\n"
               "  customprog=<filename>\n"
               "    use a custom YUV conversion program\n"
               "  customtex=<filename>\n"
               "    use a custom YUV conversion lookup texture\n"
               "  nocustomtlin\n"
               "    use GL_NEAREST scaling for customtex texture\n"
               "  customtrect\n"
               "    use texture_rectangle for customtex texture\n"
               "  mipmapgen\n"
               "    generate mipmaps for the video image (use with TXB in customprog)\n"
               "  osdcolor=<0xAARRGGBB>\n"
               "    use the given color for the OSD\n"
               "  stereo=<n>\n"
               "    0: normal display\n"
               "    1: side-by-side to red-cyan stereo\n"
               "    2: side-by-side to green-magenta stereo\n"
               "    3: side-by-side to quadbuffer stereo\n"
               "  sw\n"
               "    allow using a software renderer, if such is detected\n"
               "  backend=<sys>\n"
               "    auto: auto-select (default)\n"
               "    cocoa: Cocoa/OSX\n"
               "    win: Win32/WGL\n"
               "    x11: X11/GLX\n"
               "    wayland: Wayland/EGL\n"
               "\n");
        return -1;
    }
    if (user_colorspace != 0 || levelconv != -1) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] \"colorspace\" and \"levelconv\" "
               "suboptions have been removed. Use options --colormatrix and"
               " --colormatrix-input-range/--colormatrix-output-range instead.\n");
        return -1;
    }
    if (aspect != -1) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] \"noaspect\" suboption has been "
               "removed. Use --noaspect instead.\n");
        return -1;
    }
    if (p->use_yuv == 1) {
        mp_msg(MSGT_VO, MSGL_WARN, "[gl] yuv=1 (nVidia register combiners) have"
               " been removed, using yuv=2 instead.\n");
        p->use_yuv = 2;
    }

    char *backend = talloc_strdup(vo, backend_arg);
    free(backend_arg);

    p->glctx = mpgl_init(vo, backend);
    if (!p->glctx)
        goto err_out;
    p->gl = p->glctx->gl;

    if (p->use_yuv == -1) {
        if (!config_window(vo, 320, 200, VOFLAG_HIDDEN))
            goto err_out;
        autodetectGlExtensions(vo);
    }
    mp_msg(MSGT_VO, MSGL_V, "[gl] Using %d as slice height "
           "(0 means image height).\n", p->slice_height);

    return 0;

err_out:
    uninit(vo);
    return -1;
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
    case VOCTRL_FULLSCREEN:
        p->glctx->fullscreen(vo);
        resize(vo, vo->dwidth, vo->dheight);
        return VO_TRUE;
    case VOCTRL_BORDER:
        if (!p->glctx->border)
            break;
        p->glctx->border(vo);
        resize(vo, vo->dwidth, vo->dheight);
        return VO_TRUE;
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        resize(vo, vo->dwidth, vo->dheight);
        return VO_TRUE;
    case VOCTRL_GET_EQUALIZER:
        if (p->is_yuv) {
            struct voctrl_get_equalizer_args *args = data;
            return mp_csp_equalizer_get(&p->video_eq, args->name, args->valueptr)
                   >= 0 ? VO_TRUE : VO_NOTIMPL;
        }
        break;
    case VOCTRL_SET_EQUALIZER:
        if (p->is_yuv) {
            struct voctrl_set_equalizer_args *args = data;
            if (mp_csp_equalizer_set(&p->video_eq, args->name, args->value) < 0)
                return VO_NOTIMPL;
            update_yuvconv(vo);
            vo->want_redraw = true;
            return VO_TRUE;
        }
        break;
    case VOCTRL_SET_YUV_COLORSPACE: {
        bool supports_csp = (1 << p->use_yuv) & MASK_NOT_COMBINERS;
        if (vo->config_count && supports_csp) {
            p->colorspace = *(struct mp_csp_details *)data;
            update_yuvconv(vo);
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
    case VOCTRL_REDRAW_FRAME:
        do_render(vo);
        return true;
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
    case VOCTRL_SCREENSHOT: {
        struct voctrl_screenshot_args *args = data;
        if (args->full_window)
            args->out_image = glGetWindowScreenshot(p->gl);
        else
            args->out_image = get_screenshot(vo);
        return true;
    }
    }
    return VO_NOTIMPL;
}

const struct vo_driver video_out_opengl_old = {
    .info = &(const vo_info_t) {
        "OpenGL",
        "opengl-old",
        "Reimar Doeffinger <Reimar.Doeffinger@gmx.de>",
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
