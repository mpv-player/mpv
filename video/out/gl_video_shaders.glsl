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

// Note that this file is not directly passed as shader, but run through some
// text processing functions, and in fact contains multiple vertex and fragment
// shaders.

// inserted at the beginning of all shaders
#!section prelude

// GLSL 1.20 compatibility layer
// texture() should be assumed to always map to texture2D()
#if __VERSION__ >= 130
# define texture1D texture
# define texture3D texture
# define DECLARE_FRAGPARMS \
    out vec4 out_color;
#else
# define texture texture2D
# define DECLARE_FRAGPARMS
# define out_color gl_FragColor
# define in varying
#endif

// Earlier GLSL doesn't support mix() with bvec
#if __VERSION__ >= 130
vec3 srgb_compand(vec3 v)
{
    return mix(v * 12.92, 1.055 * pow(v, vec3(1.0/2.4)) - 0.055,
               lessThanEqual(vec3(0.0031308), v));
}

vec3 bt2020_expand(vec3 v)
{
    return mix(v / 4.5, pow((v + vec3(0.0993))/1.0993, vec3(1/0.45)),
               lessThanEqual(vec3(0.08145), v));
}

vec3 bt2020_compand(vec3 v)
{
    return mix(v * 4.5, 1.0993 * pow(v, vec3(0.45)) - vec3(0.0993),
               lessThanEqual(vec3(0.0181), v));
}
#endif

// Constant matrix for conversion from BT.2020 to sRGB
const mat3 srgb_matrix = mat3(
     1.6604910, -0.1245505, -0.0181508,
    -0.5876411,  1.1328999, -0.1005789,
    -0.0728499, -0.0083494,  1.1187297
);

#!section vertex_all

#if __VERSION__ < 130
# undef in
# define in attribute
# define out varying
#endif

uniform mat3 transform;
uniform sampler3D lut_3d;
uniform mat3 cms_matrix; // transformation from file's gamut to bt.2020

in vec2 vertex_position;
in vec4 vertex_color;
out vec4 color;
in vec2 vertex_texcoord;
out vec2 texcoord;

void main() {
    vec3 position = vec3(vertex_position, 1);
#ifndef FIXED_SCALE
    position = transform * position;
#endif
    gl_Position = vec4(position, 1);
    color = vertex_color;

#ifdef USE_OSD_LINEAR_CONV
    // Although we are not scaling in linear light, both 3DLUT and SRGB still
    // operate on linear light inputs so we have to convert to it before
    // either step can be applied.
    color.rgb = bt2020_expand(color.rgb);
    // NOTE: This always applies the true BT2020, maybe we need to use
    // approx-gamma here too?
#endif
#ifdef USE_OSD_CMS_MATRIX
    // Convert to the right target gamut first (to BT.709 for sRGB,
    // and to BT.2020 for 3DLUT).
    color.rgb = clamp(cms_matrix * color.rgb, 0, 1);
#endif
#ifdef USE_OSD_3DLUT
    color.rgb = pow(color.rgb, vec3(1/2.4)); // linear -> 2.4 3DLUT space
    color = vec4(texture3D(lut_3d, color.rgb).rgb, color.a);
#endif
#ifdef USE_OSD_SRGB
    color.rgb = srgb_compand(clamp(srgb_matrix * color.rgb, 0, 1));
#endif

    texcoord = vertex_texcoord;
}

#!section frag_osd_libass
uniform sampler2D texture0;

in vec2 texcoord;
in vec4 color;
DECLARE_FRAGPARMS

void main() {
    out_color = vec4(color.rgb, color.a * texture(texture0, texcoord).r);
}

#!section frag_osd_rgba
uniform sampler2D texture0;

in vec2 texcoord;
DECLARE_FRAGPARMS

void main() {
    out_color = texture(texture0, texcoord);
}

#!section frag_video
uniform VIDEO_SAMPLER texture0;
uniform VIDEO_SAMPLER texture1;
uniform VIDEO_SAMPLER texture2;
uniform VIDEO_SAMPLER texture3;
uniform vec2 textures_size[4];
uniform vec2 chroma_center_offset;
uniform vec2 chroma_div;
uniform sampler1D lut_c_1d;
uniform sampler1D lut_l_1d;
uniform sampler2D lut_c_2d;
uniform sampler2D lut_l_2d;
uniform sampler3D lut_3d;
uniform sampler2D dither;
uniform mat4x3 colormatrix;
uniform mat3 cms_matrix;
uniform mat2 dither_trafo;
uniform vec3 inv_gamma;
uniform float input_gamma;
uniform float conv_gamma;
uniform float dither_quantization;
uniform float dither_center;
uniform float filter_param1;
uniform vec2 dither_size;

in vec2 texcoord;
DECLARE_FRAGPARMS

#define CONV_NV12 1
#define CONV_PLANAR 2

vec4 sample_bilinear(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord) {
    return texture(tex, texcoord);
}

// Explanation how bicubic scaling with only 4 texel fetches is done:
//   http://www.mate.tue.nl/mate/pdfs/10318.pdf
//   'Efficient GPU-Based Texture Interpolation using Uniform B-Splines'
// Explanation why this algorithm normally always blurs, even with unit scaling:
//   http://bigwww.epfl.ch/preprints/ruijters1001p.pdf
//   'GPU Prefilter for Accurate Cubic B-spline Interpolation'
vec4 calcweights(float s) {
    vec4 t = vec4(-0.5, 0.1666, 0.3333, -0.3333) * s + vec4(1, 0, -0.5, 0.5);
    t = t * s + vec4(0, 0, -0.5, 0.5);
    t = t * s + vec4(-0.6666, 0, 0.8333, 0.1666);
    vec2 a = vec2(1, 1) / vec2(t.z, t.w);
    t.xy = t.xy * a + vec2(1, 1);
    t.x = t.x + s;
    t.y = t.y - s;
    return t;
}

vec4 sample_bicubic_fast(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord) {
    vec2 pt = 1 / texsize;
    vec2 fcoord = fract(texcoord * texsize + vec2(0.5, 0.5));
    vec4 parmx = calcweights(fcoord.x);
    vec4 parmy = calcweights(fcoord.y);
    vec4 cdelta;
    cdelta.xz = parmx.rg * vec2(-pt.x, pt.x);
    cdelta.yw = parmy.rg * vec2(-pt.y, pt.y);
    // first y-interpolation
    vec4 ar = texture(tex, texcoord + cdelta.xy);
    vec4 ag = texture(tex, texcoord + cdelta.xw);
    vec4 ab = mix(ag, ar, parmy.b);
    // second y-interpolation
    vec4 br = texture(tex, texcoord + cdelta.zy);
    vec4 bg = texture(tex, texcoord + cdelta.zw);
    vec4 aa = mix(bg, br, parmy.b);
    // x-interpolation
    return mix(aa, ab, parmx.b);
}

float[2] weights2(sampler1D lookup, float f) {
    vec4 c = texture1D(lookup, f);
    return float[2](c.r, c.g);
}

float[4] weights4(sampler1D lookup, float f) {
    vec4 c = texture1D(lookup, f);
    return float[4](c.r, c.g, c.b, c.a);
}

float[6] weights6(sampler2D lookup, float f) {
    vec4 c1 = texture(lookup, vec2(0.25, f));
    vec4 c2 = texture(lookup, vec2(0.75, f));
    return float[6](c1.r, c1.g, c1.b, c2.r, c2.g, c2.b);
}

float[8] weights8(sampler2D lookup, float f) {
    vec4 c1 = texture(lookup, vec2(0.25, f));
    vec4 c2 = texture(lookup, vec2(0.75, f));
    return float[8](c1.r, c1.g, c1.b, c1.a, c2.r, c2.g, c2.b, c2.a);
}

float[12] weights12(sampler2D lookup, float f) {
    vec4 c1 = texture(lookup, vec2(1.0/6.0, f));
    vec4 c2 = texture(lookup, vec2(0.5, f));
    vec4 c3 = texture(lookup, vec2(5.0/6.0, f));
    return float[12](c1.r, c1.g, c1.b, c1.a,
                     c2.r, c2.g, c2.b, c2.a,
                     c3.r, c3.g, c3.b, c3.a);
}

float[16] weights16(sampler2D lookup, float f) {
    vec4 c1 = texture(lookup, vec2(0.125, f));
    vec4 c2 = texture(lookup, vec2(0.375, f));
    vec4 c3 = texture(lookup, vec2(0.625, f));
    vec4 c4 = texture(lookup, vec2(0.875, f));
    return float[16](c1.r, c1.g, c1.b, c1.a, c2.r, c2.g, c2.b, c2.a,
                     c3.r, c3.g, c3.b, c3.a, c4.r, c4.g, c4.b, c4.a);
}

#define CONVOLUTION_SEP_N(NAME, N)                                          \
    vec4 NAME(VIDEO_SAMPLER tex, vec2 texcoord, vec2 pt, float weights[N]) {\
        vec4 res = vec4(0);                                                 \
        for (int n = 0; n < N; n++) {                                       \
            res += weights[n] * texture(tex, texcoord + pt * n);            \
        }                                                                   \
        return res;                                                         \
    }

CONVOLUTION_SEP_N(convolution_sep2, 2)
CONVOLUTION_SEP_N(convolution_sep4, 4)
CONVOLUTION_SEP_N(convolution_sep6, 6)
CONVOLUTION_SEP_N(convolution_sep8, 8)
CONVOLUTION_SEP_N(convolution_sep12, 12)
CONVOLUTION_SEP_N(convolution_sep16, 16)

// The dir parameter is (0, 1) or (1, 0), and we expect the shader compiler to
// remove all the redundant multiplications and additions.
#define SAMPLE_CONVOLUTION_SEP_N(NAME, N, SAMPLERT, CONV_FUNC, WEIGHTS_FUNC)\
    vec4 NAME(vec2 dir, SAMPLERT lookup, VIDEO_SAMPLER tex, vec2 texsize,   \
              vec2 texcoord) {                                              \
        vec2 pt = (1 / texsize) * dir;                                      \
        float fcoord = dot(fract(texcoord * texsize - 0.5), dir);           \
        vec2 base = texcoord - fcoord * pt;                                 \
        return CONV_FUNC(tex, base - pt * (N / 2 - 1), pt,                  \
                         WEIGHTS_FUNC(lookup, fcoord));                     \
    }

SAMPLE_CONVOLUTION_SEP_N(sample_convolution_sep2, 2, sampler1D, convolution_sep2, weights2)
SAMPLE_CONVOLUTION_SEP_N(sample_convolution_sep4, 4, sampler1D, convolution_sep4, weights4)
SAMPLE_CONVOLUTION_SEP_N(sample_convolution_sep6, 6, sampler2D, convolution_sep6, weights6)
SAMPLE_CONVOLUTION_SEP_N(sample_convolution_sep8, 8, sampler2D, convolution_sep8, weights8)
SAMPLE_CONVOLUTION_SEP_N(sample_convolution_sep12, 12, sampler2D, convolution_sep12, weights12)
SAMPLE_CONVOLUTION_SEP_N(sample_convolution_sep16, 16, sampler2D, convolution_sep16, weights16)


#define CONVOLUTION_N(NAME, N)                                               \
    vec4 NAME(VIDEO_SAMPLER tex, vec2 texcoord, vec2 pt, float taps_x[N],    \
              float taps_y[N]) {                                             \
        vec4 res = vec4(0);                                                  \
        for (int y = 0; y < N; y++) {                                        \
            vec4 line = vec4(0);                                             \
            for (int x = 0; x < N; x++)                                      \
                line += taps_x[x] * texture(tex, texcoord + pt * vec2(x, y));\
            res += taps_y[y] * line;                                         \
        }                                                                    \
        return res;                                                          \
    }

CONVOLUTION_N(convolution2, 2)
CONVOLUTION_N(convolution4, 4)
CONVOLUTION_N(convolution6, 6)
CONVOLUTION_N(convolution8, 8)
CONVOLUTION_N(convolution12, 12)
CONVOLUTION_N(convolution16, 16)

#define SAMPLE_CONVOLUTION_N(NAME, N, SAMPLERT, CONV_FUNC, WEIGHTS_FUNC)    \
    vec4 NAME(SAMPLERT lookup, VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord) {\
        vec2 pt = 1 / texsize;                                              \
        vec2 fcoord = fract(texcoord * texsize - 0.5);                      \
        vec2 base = texcoord - fcoord * pt;                                 \
        return CONV_FUNC(tex, base - pt * (N / 2 - 1), pt,                  \
                         WEIGHTS_FUNC(lookup, fcoord.x),                    \
                         WEIGHTS_FUNC(lookup, fcoord.y));                   \
    }

SAMPLE_CONVOLUTION_N(sample_convolution2, 2, sampler1D, convolution2, weights2)
SAMPLE_CONVOLUTION_N(sample_convolution4, 4, sampler1D, convolution4, weights4)
SAMPLE_CONVOLUTION_N(sample_convolution6, 6, sampler2D, convolution6, weights6)
SAMPLE_CONVOLUTION_N(sample_convolution8, 8, sampler2D, convolution8, weights8)
SAMPLE_CONVOLUTION_N(sample_convolution12, 12, sampler2D, convolution12, weights12)
SAMPLE_CONVOLUTION_N(sample_convolution16, 16, sampler2D, convolution16, weights16)


// Unsharp masking
vec4 sample_sharpen3(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord) {
    vec2 pt = 1 / texsize;
    vec2 st = pt * 0.5;
    vec4 p = texture(tex, texcoord);
    vec4 sum = texture(tex, texcoord + st * vec2(+1, +1))
             + texture(tex, texcoord + st * vec2(+1, -1))
             + texture(tex, texcoord + st * vec2(-1, +1))
             + texture(tex, texcoord + st * vec2(-1, -1));
    return p + (p - 0.25 * sum) * filter_param1;
}

vec4 sample_sharpen5(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord) {
    vec2 pt = 1 / texsize;
    vec2 st1 = pt * 1.2;
    vec4 p = texture(tex, texcoord);
    vec4 sum1 = texture(tex, texcoord + st1 * vec2(+1, +1))
              + texture(tex, texcoord + st1 * vec2(+1, -1))
              + texture(tex, texcoord + st1 * vec2(-1, +1))
              + texture(tex, texcoord + st1 * vec2(-1, -1));
    vec2 st2 = pt * 1.5;
    vec4 sum2 = texture(tex, texcoord + st2 * vec2(+1,  0))
              + texture(tex, texcoord + st2 * vec2( 0, +1))
              + texture(tex, texcoord + st2 * vec2(-1,  0))
              + texture(tex, texcoord + st2 * vec2( 0, -1));
    vec4 t = p * 0.859375 + sum2 * -0.1171875 + sum1 * -0.09765625;
    return p + t * filter_param1;
}

void main() {
    vec2 chr_texcoord = texcoord;
#ifdef USE_RECTANGLE
    chr_texcoord = chr_texcoord * chroma_div;
#else
    // Texture coordinates are [0,1], and chroma plane coordinates are
    // magically rescaled.
#endif
    chr_texcoord = chr_texcoord + chroma_center_offset;
#ifndef USE_CONV
#define USE_CONV 0
#endif
#if USE_CONV == CONV_PLANAR
    vec4 acolor = vec4(SAMPLE_L(texture0, textures_size[0], texcoord).r,
                       SAMPLE_C(texture1, textures_size[1], chr_texcoord).r,
                       SAMPLE_C(texture2, textures_size[2], chr_texcoord).r,
                       1.0);
#elif USE_CONV == CONV_NV12
    vec4 acolor = vec4(SAMPLE_L(texture0, textures_size[0], texcoord).r,
                       SAMPLE_C(texture1, textures_size[1], chr_texcoord).rg,
                       1.0);
#else
    vec4 acolor = SAMPLE_L(texture0, textures_size[0], texcoord);
#endif
#ifdef USE_ALPHA_PLANE
    acolor.a = SAMPLE_L(texture3, textures_size[3], texcoord).r;
#endif
#ifdef USE_COLOR_SWIZZLE
    acolor = acolor. USE_COLOR_SWIZZLE ;
#endif
    vec3 color = acolor.rgb;
    float alpha = acolor.a;
#ifdef USE_YGRAY
    // NOTE: actually slightly wrong for 16 bit input video, and completely
    //       wrong for 9/10 bit input
    color.gb = vec2(128.0/255.0);
#endif
#ifdef USE_INPUT_GAMMA
    // Pre-colormatrix input gamma correction (eg. for MP_IMGFLAG_XYZ)
    color = pow(color, vec3(input_gamma));
#endif
#ifdef USE_COLORMATRIX
    // Conversion from Y'CbCr or other spaces to RGB
    color = mat3(colormatrix) * color + colormatrix[3];
#endif
#ifdef USE_CONV_GAMMA
    // Post-colormatrix converted gamma correction (eg. for MP_IMGFLAG_XYZ)
    color = pow(color, vec3(conv_gamma));
#endif
#ifdef USE_CONST_LUMA
    // Conversion from C'rcY'cC'bc to R'Y'cB' via the BT.2020 CL system:
    // C'bc = (B'-Y'c) / 1.9404  | C'bc <= 0
    //      = (B'-Y'c) / 1.5816  | C'bc >  0
    //
    // C'rc = (R'-Y'c) / 1.7184  | C'rc <= 0
    //      = (R'-Y'c) / 0.9936  | C'rc >  0
    //
    // as per the BT.2020 specification, table 4. This is a non-linear
    // transformation because (constant) luminance receives non-equal
    // contributions from the three different channels.
    color.br = color.br * mix(vec2(1.5816, 0.9936), vec2(1.9404, 1.7184),
                              lessThanEqual(color.br, vec2(0))) + color.gg;
#endif
#ifdef USE_COLORMATRIX
    // Clamp down here to avoid clipping CbCr details before CONST_LUMA
    // has a chance to convert them.
    color = clamp(color, 0, 1);
#endif
    // If we are scaling in linear light (SRGB or 3DLUT option enabled), we
    // expand our source colors before scaling. This shader currently just
    // assumes everything uses the BT.2020 12-bit gamma function, since the
    // difference between this and BT.601, BT.709 and BT.2020 10-bit is well
    // below the rounding error threshold for both 8-bit and even 10-bit
    // content. It only makes a difference for 12-bit sources, so it should be
    // fine to use here.
#ifdef USE_LINEAR_LIGHT_APPROX
    // We differentiate between approximate BT.2020 (gamma 1.95) ...
    color = pow(color, vec3(1.95));
#endif
#ifdef USE_LINEAR_LIGHT_BT2020
    // ... and actual BT.2020 (two-part function)
    color = bt2020_expand(color);
#endif
#ifdef USE_CONST_LUMA
    // Calculate the green channel from the expanded RYcB
    // The BT.2020 specification says Yc = 0.2627*R + 0.6780*G + 0.0593*B
    color.g = (color.g - 0.2627*color.r - 0.0593*color.b)/0.6780;
#endif
    // Image upscaling happens roughly here
#ifdef USE_GAMMA_POW
    // User-defined gamma correction factor (via the gamma sub-option)
    color = pow(color, inv_gamma);
#endif
#ifdef USE_CMS_MATRIX
    // Convert to the right target gamut first (to BT.709 for sRGB,
    // and to BT.2020 for 3DLUT).
    color = cms_matrix * color;
#endif
#ifdef USE_3DLUT
    // For the 3DLUT we are arbitrarily using 2.4 as input gamma to reduce
    // the amount of rounding errors, so we pull up to that space first and
    // then pass it through the 3D texture.
    //
    // The value is clamped to [0,1] first because the gamma function is not
    // well-defined outside it. This should not be a problem because the 3dlut
    // is not defined for values outside its boundaries either way, and no
    // media can possibly exceed its BT.2020 source gamut either way due to
    // that being the biggest taggable color space. This is just to avoid
    // numerical quirks like -1e-30 turning into NaN.
    color = pow(clamp(color, 0, 1), vec3(1/2.4));
    color = texture3D(lut_3d, color).rgb;
#endif
#ifdef USE_SRGB
    // Adapt and compand from the linear BT2020 source to the sRGB output
    color = srgb_compand(clamp(srgb_matrix * color, 0, 1));
#endif
    // If none of these options took care of companding again, we have to do
    // it manually here for the previously-expanded channels. This again
    // comes in two flavours, one for the approximate gamma system and one
    // for the actual gamma system.
#ifdef USE_CONST_LUMA_INV_APPROX
    color = pow(color, vec3(1/1.95));
#endif
#ifdef USE_CONST_LUMA_INV_BT2020
    color = bt2020_compand(color);
#endif
#ifdef USE_DITHER
    vec2 dither_pos = gl_FragCoord.xy / dither_size;
#ifdef USE_TEMPORAL_DITHER
    dither_pos = dither_trafo * dither_pos;
#endif
    float dither_value = texture(dither, dither_pos).r;
    color = floor(color * dither_quantization + dither_value + dither_center) /
                dither_quantization;
#endif
#ifdef USE_ALPHA_BLEND
    color = color * alpha;
#endif
#ifdef USE_ALPHA
    out_color = vec4(color, alpha);
#else
    out_color = vec4(color, 1.0);
#endif
}
