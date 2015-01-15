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

#ifdef GL_ES
precision mediump float;
#endif

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

#if HAVE_RG
#define RG rg
#else
#define RG ra
#endif

// Earlier GLSL doesn't support mix() with bvec
#if __VERSION__ >= 130
vec3 srgb_expand(vec3 v)
{
    return mix(v / vec3(12.92), pow((v + vec3(0.055))/vec3(1.055), vec3(2.4)),
               lessThanEqual(vec3(0.04045), v));
}

vec3 srgb_compand(vec3 v)
{
    return mix(v * vec3(12.92), vec3(1.055) * pow(v, vec3(1.0/2.4)) - vec3(0.055),
               lessThanEqual(vec3(0.0031308), v));
}

vec3 bt2020_expand(vec3 v)
{
    return mix(v / vec3(4.5), pow((v + vec3(0.0993))/vec3(1.0993), vec3(1.0/0.45)),
               lessThanEqual(vec3(0.08145), v));
}

vec3 bt2020_compand(vec3 v)
{
    return mix(v * vec3(4.5), vec3(1.0993) * pow(v, vec3(0.45)) - vec3(0.0993),
               lessThanEqual(vec3(0.0181), v));
}
#endif

#!section vertex_all

#if __VERSION__ < 130
# undef in
# define in attribute
# define out varying
#endif

uniform mat3 transform;
uniform vec3 translation;
#if HAVE_3DTEX
uniform sampler3D lut_3d;
#endif
uniform mat3 cms_matrix; // transformation from file's gamut to bt.2020

in vec2 vertex_position;
in vec4 vertex_color;
out vec4 color;
in vec2 vertex_texcoord;
out vec2 texcoord;

void main() {
    vec3 position = vec3(vertex_position, 1) + translation;
#ifndef FIXED_SCALE
    position = transform * position;
#endif
    gl_Position = vec4(position, 1);
    color = vertex_color;

    // Although we are not scaling in linear light, both 3DLUT and SRGB still
    // operate on linear light inputs so we have to convert to it before
    // either step can be applied.
#ifdef USE_OSD_LINEAR_CONV_APPROX
    color.rgb = pow(color.rgb, vec3(1.95));
#endif
#ifdef USE_OSD_LINEAR_CONV_BT2020
    color.rgb = bt2020_expand(color.rgb);
#endif
#ifdef USE_OSD_LINEAR_CONV_SRGB
    color.rgb = srgb_expand(color.rgb);
#endif
#ifdef USE_OSD_CMS_MATRIX
    // Convert to the right target gamut first (to BT.709 for sRGB,
    // and to BT.2020 for 3DLUT). Normal clamping here as perceptually
    // accurate colorimetry is probably not worth the performance trade-off
    // here.
    color.rgb = clamp(cms_matrix * color.rgb, 0.0, 1.0);
#endif
#ifdef USE_OSD_3DLUT
    color.rgb = pow(color.rgb, vec3(1.0/2.4)); // linear -> 2.4 3DLUT space
    color = vec4(texture3D(lut_3d, color.rgb).rgb, color.a);
#endif
#ifdef USE_OSD_SRGB
    color.rgb = srgb_compand(color.rgb);
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
    out_color = texture(texture0, texcoord).bgra;
}

#!section frag_video
uniform VIDEO_SAMPLER texture0;
uniform VIDEO_SAMPLER texture1;
uniform VIDEO_SAMPLER texture2;
uniform VIDEO_SAMPLER texture3;
uniform vec2 textures_size[4];
uniform vec2 chroma_center_offset;
uniform vec2 chroma_div;
uniform sampler2D lut_c;
uniform sampler2D lut_l;
uniform sampler1D lut_polar_c;
uniform sampler1D lut_polar_l;
#if HAVE_3DTEX
uniform sampler3D lut_3d;
#endif
uniform sampler2D dither;
uniform mat3 colormatrix;
uniform vec3 colormatrix_c;
uniform mat3 cms_matrix;
uniform mat2 dither_trafo;
uniform vec3 inv_gamma;
uniform float input_gamma;
uniform float conv_gamma;
uniform float sig_center;
uniform float sig_slope;
uniform float sig_scale;
uniform float sig_offset;
uniform float dither_quantization;
uniform float dither_center;
uniform float filter_param1_l;
uniform float filter_param1_c;
uniform vec2 dither_size;

in vec2 texcoord;
DECLARE_FRAGPARMS

#define CONV_NV12 1
#define CONV_PLANAR 2

vec4 sample_bilinear(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord, float param1) {
    return texture(tex, texcoord);
}

#define SAMPLE_BILINEAR(p0, p1, p2) sample_bilinear(p0, p1, p2, 0.0)

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

vec4 sample_bicubic_fast(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord, float param1) {
    vec2 pt = 1.0 / texsize;
    vec2 fcoord = fract(texcoord * texsize + vec2(0.5, 0.5));
    vec4 parmx = calcweights(fcoord.x);
    vec4 parmy = calcweights(fcoord.y);
    vec4 cdelta;
    cdelta.xz = parmx.RG * vec2(-pt.x, pt.x);
    cdelta.yw = parmy.RG * vec2(-pt.y, pt.y);
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

#if HAVE_ARRAYS
float[2] weights2(sampler2D lookup, float f) {
    vec2 c = texture(lookup, vec2(0.5, f)).RG;
    return float[2](c.r, c.g);
}
float[6] weights6(sampler2D lookup, float f) {
    vec4 c1 = texture(lookup, vec2(0.25, f));
    vec4 c2 = texture(lookup, vec2(0.75, f));
    return float[6](c1.r, c1.g, c1.b, c2.r, c2.g, c2.b);
}
#endif

// For N=n*4 with n>1.
#define WEIGHTS_N(NAME, N)                          \
    float[N] NAME(sampler2D lookup, float f) {      \
        float r[N];                                 \
        for (int n = 0; n < N / 4; n++) {           \
            vec4 c = texture(lookup,                \
                vec2(1.0 / (N / 2) + n / float(N / 4), f)); \
            r[n * 4 + 0] = c.r;                     \
            r[n * 4 + 1] = c.g;                     \
            r[n * 4 + 2] = c.b;                     \
            r[n * 4 + 3] = c.a;                     \
        }                                           \
        return r;                                   \
    }

// The DIR parameter is (0, 1) or (1, 0), and we expect the shader compiler to
// remove all the redundant multiplications and additions.
#define SAMPLE_CONVOLUTION_SEP_N(NAME, DIR, N, LUT, WEIGHTS_FUNC)           \
    vec4 NAME(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord) {             \
        vec2 pt = (vec2(1.0) / texsize) * DIR;                              \
        float fcoord = dot(fract(texcoord * texsize - vec2(0.5)), DIR);     \
        vec2 base = texcoord - fcoord * pt - pt * vec2(N / 2 - 1);          \
        float weights[N] = WEIGHTS_FUNC(LUT, fcoord);                       \
        vec4 res = vec4(0);                                                 \
        for (int n = 0; n < N; n++) {                                       \
            res += vec4(weights[n]) * texture(tex, base + pt * vec2(n));    \
        }                                                                   \
        return res;                                                         \
    }

#define SAMPLE_CONVOLUTION_N(NAME, N, LUT, WEIGHTS_FUNC)                    \
    vec4 NAME(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord) {             \
        vec2 pt = vec2(1.0) / texsize;                                      \
        vec2 fcoord = fract(texcoord * texsize - vec2(0.5));                \
        vec2 base = texcoord - fcoord * pt - pt * vec2(N / 2 - 1);          \
        vec4 res = vec4(0);                                                 \
        float w_x[N] = WEIGHTS_FUNC(LUT, fcoord.x);                         \
        float w_y[N] = WEIGHTS_FUNC(LUT, fcoord.y);                         \
        for (int y = 0; y < N; y++) {                                       \
            vec4 line = vec4(0);                                            \
            for (int x = 0; x < N; x++)                                     \
                line += vec4(w_x[x]) * texture(tex, base + pt * vec2(x, y));\
            res += vec4(w_y[y]) * line;                                     \
        }                                                                   \
        return res;                                                         \
    }


#define SAMPLE_CONVOLUTION_POLAR_R(NAME, R, LUT)                            \
    vec4 NAME(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord) {             \
        vec2 pt = vec2(1.0) / texsize;                                      \
        vec2 fcoord = fract(texcoord * texsize - vec2(0.5));                \
        vec2 base = texcoord - fcoord * pt;                                 \
        vec4 res = vec4(0);                                                 \
        float wsum = 0;                                                     \
        for (int y = 1-R; y <= R; y++) {                                    \
            for (int x = 1-R; x <= R; x++) {                                \
                vec2 d = vec2(x,y) - fcoord;                                \
                float w = texture1D(LUT, sqrt(d.x*d.x + d.y*d.y)/R).r;      \
                wsum += w;                                                  \
                res += w * texture(tex, base + pt * vec2(x, y));            \
            }                                                               \
        }                                                                   \
        return res / wsum;                                                  \
    }

#ifdef DEF_SCALER0
DEF_SCALER0
#endif
#ifdef DEF_SCALER1
DEF_SCALER1
#endif

// Unsharp masking
vec4 sample_sharpen3(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord, float param1) {
    vec2 pt = 1.0 / texsize;
    vec2 st = pt * 0.5;
    vec4 p = texture(tex, texcoord);
    vec4 sum = texture(tex, texcoord + st * vec2(+1, +1))
             + texture(tex, texcoord + st * vec2(+1, -1))
             + texture(tex, texcoord + st * vec2(-1, +1))
             + texture(tex, texcoord + st * vec2(-1, -1));
    return p + (p - 0.25 * sum) * param1;
}

vec4 sample_sharpen5(VIDEO_SAMPLER tex, vec2 texsize, vec2 texcoord, float param1) {
    vec2 pt = 1.0 / texsize;
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
    return p + t * param1;
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
                       SAMPLE_C(texture1, textures_size[1], chr_texcoord).RG,
                       1.0);
#else
    vec4 acolor = SAMPLE_L(texture0, textures_size[0], texcoord);
#endif
#ifdef USE_COLOR_SWIZZLE
    acolor = acolor. USE_COLOR_SWIZZLE ;
#endif
#ifdef USE_ALPHA_PLANE
    acolor.a = SAMPLE_L(texture3, textures_size[3], texcoord).r;
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
    color = mat3(colormatrix) * color + colormatrix_c;
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
    // CONST_LUMA involves numbers outside the [0,1] range so we make sure
    // to clip here, after the (possible) USE_CONST_LUMA calculations are done,
    // instead of immediately after the colormatrix conversion.
    color = clamp(color, 0.0, 1.0);
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
#ifdef USE_LINEAR_LIGHT_SRGB
    // This is not needed for most sRGB content since we can use GL_SRGB to
    // directly sample RGB texture in linear light, but for things which are
    // also sRGB but in a different format (such as JPEG's YUV), we need
    // to convert to linear light manually.
    color = srgb_expand(color);
#endif
#ifdef USE_CONST_LUMA
    // Calculate the green channel from the expanded RYcB
    // The BT.2020 specification says Yc = 0.2627*R + 0.6780*G + 0.0593*B
    color.g = (color.g - 0.2627*color.r - 0.0593*color.b)/0.6780;
#endif
#ifdef USE_SIGMOID
    color = sig_center - log(1.0/(color * sig_scale + sig_offset) - 1.0)/sig_slope;
#endif
    // Image upscaling happens roughly here
#ifdef USE_SIGMOID_INV
    // Inverse of USE_SIGMOID
    color = (1.0/(1.0 + exp(sig_slope * (sig_center - color))) - sig_offset) / sig_scale;
#endif
#ifdef USE_GAMMA_POW
    // User-defined gamma correction factor (via the gamma sub-option)
    color = pow(color, inv_gamma);
#endif
#ifdef USE_CMS_MATRIX
    // Convert to the right target gamut first (to BT.709 for sRGB,
    // and to BT.2020 for 3DLUT).
    color = cms_matrix * color;

    // Clamp to the target gamut. This clamp is needed because the gamma
    // functions are not well-defined outside this range, which is related to
    // the fact that they're not representable on the target device.
    // TODO: Desaturate colorimetrically; this happens automatically for
    // 3dlut targets but not for sRGB mode. Not sure if this is a requirement.
    color = clamp(color, 0.0, 1.0);
#endif
#ifdef USE_3DLUT
    // For the 3DLUT we are arbitrarily using 2.4 as input gamma to reduce
    // the amount of rounding errors, so we pull up to that space first and
    // then pass it through the 3D texture.
    color = pow(color, vec3(1.0/2.4));
    color = texture3D(lut_3d, color).rgb;
#endif
#ifdef USE_SRGB
    // Adapt and compand from the linear BT2020 source to the sRGB output
    color = srgb_compand(color);
#endif
    // If none of these options took care of companding again (ie. CMS is
    // disabled), we still need to re-compand const luma signals, because
    // they always come out as linear light (and we can't simply output that).
#ifdef USE_CONST_LUMA_INV
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
#ifdef USE_CUSTOM_SHADER
    out_color = custom_shader(out_color);
#endif
}
