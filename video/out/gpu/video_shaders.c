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

#include <math.h>

#include "video_shaders.h"
#include "video.h"

#define GLSL(x) gl_sc_add(sc, #x "\n");
#define GLSLF(...) gl_sc_addf(sc, __VA_ARGS__)
#define GLSLH(x) gl_sc_hadd(sc, #x "\n");
#define GLSLHF(...) gl_sc_haddf(sc, __VA_ARGS__)

// Set up shared/commonly used variables and macros
void sampler_prelude(struct gl_shader_cache *sc, int tex_num)
{
    GLSLF("#undef tex\n");
    GLSLF("#undef texmap\n");
    GLSLF("#define tex texture%d\n", tex_num);
    GLSLF("#define texmap texmap%d\n", tex_num);
    GLSLF("vec2 pos = texcoord%d;\n", tex_num);
    GLSLF("vec2 size = texture_size%d;\n", tex_num);
    GLSLF("vec2 pt = pixel_size%d;\n", tex_num);
}

static void pass_sample_separated_get_weights(struct gl_shader_cache *sc,
                                              struct scaler *scaler)
{
    gl_sc_uniform_texture(sc, "lut", scaler->lut);
    GLSLF("float ypos = LUT_POS(fcoord, %d.0);\n", scaler->lut_size);

    int N = scaler->kernel->size;
    int width = (N + 3) / 4; // round up

    GLSLF("float weights[%d];\n", N);
    for (int i = 0; i < N; i++) {
        if (i % 4 == 0)
            GLSLF("c = texture(lut, vec2(%f, ypos));\n", (i / 4 + 0.5) / width);
        GLSLF("weights[%d] = c[%d];\n", i, i % 4);
    }
}

// Handle a single pass (either vertical or horizontal). The direction is given
// by the vector (d_x, d_y). If the vector is 0, then planar interpolation is
// used instead (samples from texture0 through textureN)
void pass_sample_separated_gen(struct gl_shader_cache *sc, struct scaler *scaler,
                               int d_x, int d_y)
{
    int N = scaler->kernel->size;
    bool use_ar = scaler->conf.antiring > 0;
    bool planar = d_x == 0 && d_y == 0;
    GLSL(color = vec4(0.0);)
    GLSLF("{\n");
    if (!planar) {
        GLSLF("vec2 dir = vec2(%d.0, %d.0);\n", d_x, d_y);
        GLSL(pt *= dir;)
        GLSL(float fcoord = dot(fract(pos * size - vec2(0.5)), dir);)
        GLSLF("vec2 base = pos - fcoord * pt - pt * vec2(%d.0);\n", N / 2 - 1);
    }
    GLSL(vec4 c;)
    if (use_ar) {
        GLSL(vec4 hi = vec4(0.0);)
        GLSL(vec4 lo = vec4(1.0);)
    }
    pass_sample_separated_get_weights(sc, scaler);
    GLSLF("// scaler samples\n");
    for (int n = 0; n < N; n++) {
        if (planar) {
            GLSLF("c = texture(texture%d, texcoord%d);\n", n, n);
        } else {
            GLSLF("c = texture(tex, base + pt * vec2(%d.0));\n", n);
        }
        GLSLF("color += vec4(weights[%d]) * c;\n", n);
        if (use_ar && (n == N/2-1 || n == N/2)) {
            GLSL(lo = min(lo, c);)
            GLSL(hi = max(hi, c);)
        }
    }
    if (use_ar)
        GLSLF("color = mix(color, clamp(color, lo, hi), %f);\n",
              scaler->conf.antiring);
    GLSLF("}\n");
}

// Subroutine for computing and adding an individual texel contribution
// If planar is false, samples directly
// If planar is true, takes the pixel from inX[idx] where X is the component and
// `idx` must be defined by the caller
static void polar_sample(struct gl_shader_cache *sc, struct scaler *scaler,
                         int x, int y, int components, bool planar)
{
    double radius = scaler->kernel->f.radius * scaler->kernel->filter_scale;
    double radius_cutoff = scaler->kernel->radius_cutoff;

    // Since we can't know the subpixel position in advance, assume a
    // worst case scenario
    int yy = y > 0 ? y-1 : y;
    int xx = x > 0 ? x-1 : x;
    double dmax = sqrt(xx*xx + yy*yy);
    // Skip samples definitely outside the radius
    if (dmax >= radius_cutoff)
        return;
    GLSLF("d = length(vec2(%d.0, %d.0) - fcoord);\n", x, y);
    // Check for samples that might be skippable
    bool maybe_skippable = dmax >= radius_cutoff - M_SQRT2;
    if (maybe_skippable)
        GLSLF("if (d < %f) {\n", radius_cutoff);

    // get the weight for this pixel
    if (scaler->lut->params.dimensions == 1) {
        GLSLF("w = tex1D(lut, LUT_POS(d * 1.0/%f, %d.0)).r;\n",
              radius, scaler->lut_size);
    } else {
        GLSLF("w = texture(lut, vec2(0.5, LUT_POS(d * 1.0/%f, %d.0))).r;\n",
              radius, scaler->lut_size);
    }
    GLSL(wsum += w;)

    if (planar) {
        for (int n = 0; n < components; n++)
            GLSLF("color[%d] += w * in%d[idx];\n", n, n);
    } else {
        GLSLF("in0 = texture(tex, base + pt * vec2(%d.0, %d.0));\n", x, y);
        GLSL(color += vec4(w) * in0;)
    }

    if (maybe_skippable)
        GLSLF("}\n");
}

void pass_sample_polar(struct gl_shader_cache *sc, struct scaler *scaler,
                       int components, bool sup_gather)
{
    GLSL(color = vec4(0.0);)
    GLSLF("{\n");
    GLSL(vec2 fcoord = fract(pos * size - vec2(0.5));)
    GLSL(vec2 base = pos - fcoord * pt;)
    GLSLF("float w, d, wsum = 0.0;\n");
    for (int n = 0; n < components; n++)
        GLSLF("vec4 in%d;\n", n);
    GLSL(int idx;)

    gl_sc_uniform_texture(sc, "lut", scaler->lut);

    GLSLF("// scaler samples\n");
    int bound = ceil(scaler->kernel->radius_cutoff);
    for (int y = 1-bound; y <= bound; y += 2) {
        for (int x = 1-bound; x <= bound; x += 2) {
            // First we figure out whether it's more efficient to use direct
            // sampling or gathering. The problem is that gathering 4 texels
            // only to discard some of them is very wasteful, so only do it if
            // we suspect it will be a win rather than a loss. This is the case
            // exactly when all four texels are within bounds
            bool use_gather = sqrt(x*x + y*y) < scaler->kernel->radius_cutoff;

            if (!sup_gather)
                use_gather = false;

            if (use_gather) {
                // Gather the four surrounding texels simultaneously
                for (int n = 0; n < components; n++) {
                    GLSLF("in%d = textureGatherOffset(tex, base, "
                          "ivec2(%d, %d), %d);\n", n, x, y, n);
                }

                // Mix in all of the points with their weights
                for (int p = 0; p < 4; p++) {
                    // The four texels are gathered counterclockwise starting
                    // from the bottom left
                    static const int xo[4] = {0, 1, 1, 0};
                    static const int yo[4] = {1, 1, 0, 0};
                    if (x+xo[p] > bound || y+yo[p] > bound)
                        continue;
                    GLSLF("idx = %d;\n", p);
                    polar_sample(sc, scaler, x+xo[p], y+yo[p], components, true);
                }
            } else {
                // switch to direct sampling instead, for efficiency/compatibility
                for (int yy = y; yy <= bound && yy <= y+1; yy++) {
                    for (int xx = x; xx <= bound && xx <= x+1; xx++)
                        polar_sample(sc, scaler, xx, yy, components, false);
                }
            }
        }
    }

    GLSL(color = color / vec4(wsum);)
    GLSLF("}\n");
}

// bw/bh: block size
// iw/ih: input size (pre-calculated to fit all required texels)
void pass_compute_polar(struct gl_shader_cache *sc, struct scaler *scaler,
                        int components, int bw, int bh, int iw, int ih)
{
    int bound = ceil(scaler->kernel->radius_cutoff);
    int offset = bound - 1; // padding top/left

    GLSL(color = vec4(0.0);)
    GLSLF("{\n");
    GLSL(vec2 wpos = texmap(gl_WorkGroupID * gl_WorkGroupSize);)
    GLSL(vec2 wbase = wpos - pt * fract(wpos * size - vec2(0.5));)
    GLSL(vec2 fcoord = fract(pos * size - vec2(0.5));)
    GLSL(vec2 base = pos - pt * fcoord;)
    GLSL(ivec2 rel = ivec2(round((base - wbase) * size));)
    GLSL(int idx;)
    GLSLF("float w, d, wsum = 0.0;\n");
    gl_sc_uniform_texture(sc, "lut", scaler->lut);

    // Load all relevant texels into shmem
    for (int c = 0; c < components; c++)
        GLSLHF("shared float in%d[%d];\n", c, ih * iw);

    GLSL(vec4 c;)
    GLSLF("for (int y = int(gl_LocalInvocationID.y); y < %d; y += %d) {\n", ih, bh);
    GLSLF("for (int x = int(gl_LocalInvocationID.x); x < %d; x += %d) {\n", iw, bw);
    GLSLF("c = texture(tex, wbase + pt * vec2(x - %d, y - %d));\n", offset, offset);
    for (int c = 0; c < components; c++)
        GLSLF("in%d[%d * y + x] = c[%d];\n", c, iw, c);
    GLSLF("}}\n");
    GLSL(groupMemoryBarrier();)
    GLSL(barrier();)

    // Dispatch the actual samples
    GLSLF("// scaler samples\n");
    for (int y = 1-bound; y <= bound; y++) {
        for (int x = 1-bound; x <= bound; x++) {
            GLSLF("idx = %d * rel.y + rel.x + %d;\n", iw,
                  iw * (y + offset) + x + offset);
            polar_sample(sc, scaler, x, y, components, true);
        }
    }

    GLSL(color = color / vec4(wsum);)
    GLSLF("}\n");
}

static void bicubic_calcweights(struct gl_shader_cache *sc, const char *t, const char *s)
{
    // Explanation of how bicubic scaling with only 4 texel fetches is done:
    //   http://www.mate.tue.nl/mate/pdfs/10318.pdf
    //   'Efficient GPU-Based Texture Interpolation using Uniform B-Splines'
    // Explanation why this algorithm normally always blurs, even with unit
    // scaling:
    //   http://bigwww.epfl.ch/preprints/ruijters1001p.pdf
    //   'GPU Prefilter for Accurate Cubic B-spline Interpolation'
    GLSLF("vec4 %s = vec4(-0.5, 0.1666, 0.3333, -0.3333) * %s"
                " + vec4(1, 0, -0.5, 0.5);\n", t, s);
    GLSLF("%s = %s * %s + vec4(0, 0, -0.5, 0.5);\n", t, t, s);
    GLSLF("%s = %s * %s + vec4(-0.6666, 0, 0.8333, 0.1666);\n", t, t, s);
    GLSLF("%s.xy *= vec2(1, 1) / vec2(%s.z, %s.w);\n", t, t, t);
    GLSLF("%s.xy += vec2(1.0 + %s, 1.0 - %s);\n", t, s, s);
}

void pass_sample_bicubic_fast(struct gl_shader_cache *sc)
{
    GLSLF("{\n");
    GLSL(vec2 fcoord = fract(pos * size + vec2(0.5, 0.5));)
    bicubic_calcweights(sc, "parmx", "fcoord.x");
    bicubic_calcweights(sc, "parmy", "fcoord.y");
    GLSL(vec4 cdelta;)
    GLSL(cdelta.xz = parmx.rg * vec2(-pt.x, pt.x);)
    GLSL(cdelta.yw = parmy.rg * vec2(-pt.y, pt.y);)
    // first y-interpolation
    GLSL(vec4 ar = texture(tex, pos + cdelta.xy);)
    GLSL(vec4 ag = texture(tex, pos + cdelta.xw);)
    GLSL(vec4 ab = mix(ag, ar, parmy.b);)
    // second y-interpolation
    GLSL(vec4 br = texture(tex, pos + cdelta.zy);)
    GLSL(vec4 bg = texture(tex, pos + cdelta.zw);)
    GLSL(vec4 aa = mix(bg, br, parmy.b);)
    // x-interpolation
    GLSL(color = mix(aa, ab, parmx.b);)
    GLSLF("}\n");
}

void pass_sample_oversample(struct gl_shader_cache *sc, struct scaler *scaler,
                                   int w, int h)
{
    GLSLF("{\n");
    GLSL(vec2 pos = pos - vec2(0.5) * pt;) // round to nearest
    GLSL(vec2 fcoord = fract(pos * size - vec2(0.5));)
    // Determine the mixing coefficient vector
    gl_sc_uniform_vec2(sc, "output_size", (float[2]){w, h});
    GLSL(vec2 coeff = fcoord * output_size/size;)
    float threshold = scaler->conf.kernel.params[0];
    threshold = isnan(threshold) ? 0.0 : threshold;
    GLSLF("coeff = (coeff - %f) * 1.0/%f;\n", threshold, 1.0 - 2 * threshold);
    GLSL(coeff = clamp(coeff, 0.0, 1.0);)
    // Compute the right blend of colors
    GLSL(color = texture(tex, pos + pt * (coeff - fcoord));)
    GLSLF("}\n");
}

// Common constants for SMPTE ST.2084 (HDR)
static const float PQ_M1 = 2610./4096 * 1./4,
                   PQ_M2 = 2523./4096 * 128,
                   PQ_C1 = 3424./4096,
                   PQ_C2 = 2413./4096 * 32,
                   PQ_C3 = 2392./4096 * 32;

// Common constants for ARIB STD-B67 (HLG)
static const float HLG_A = 0.17883277,
                   HLG_B = 0.28466892,
                   HLG_C = 0.55991073;

// Common constants for Panasonic V-Log
static const float VLOG_B = 0.00873,
                   VLOG_C = 0.241514,
                   VLOG_D = 0.598206;

// Common constants for Sony S-Log
static const float SLOG_A = 0.432699,
                   SLOG_B = 0.037584,
                   SLOG_C = 0.616596 + 0.03,
                   SLOG_P = 3.538813,
                   SLOG_Q = 0.030001,
                   SLOG_K2 = 155.0 / 219.0;

// Linearize (expand), given a TRC as input. In essence, this is the ITU-R
// EOTF, calculated on an idealized (reference) monitor with a white point of
// MP_REF_WHITE and infinite contrast.
//
// These functions always output to a normalized scale of [0,1], for
// convenience of the video.c code that calls it. To get the values in an
// absolute scale, multiply the result by `mp_trc_nom_peak(trc)`
void pass_linearize(struct gl_shader_cache *sc, enum mp_csp_trc trc)
{
    if (trc == MP_CSP_TRC_LINEAR)
        return;

    GLSLF("// linearize\n");

    // Note that this clamp may technically violate the definition of
    // ITU-R BT.2100, which allows for sub-blacks and super-whites to be
    // displayed on the display where such would be possible. That said, the
    // problem is that not all gamma curves are well-defined on the values
    // outside this range, so we ignore it and just clip anyway for sanity.
    GLSL(color.rgb = clamp(color.rgb, 0.0, 1.0);)

    switch (trc) {
    case MP_CSP_TRC_SRGB:
        GLSLF("color.rgb = mix(color.rgb * vec3(1.0/12.92),             \n"
              "                pow((color.rgb + vec3(0.055))/vec3(1.055), vec3(2.4)), \n"
              "                %s(lessThan(vec3(0.04045), color.rgb))); \n",
              gl_sc_bvec(sc, 3));
        break;
    case MP_CSP_TRC_BT_1886:
        GLSL(color.rgb = pow(color.rgb, vec3(2.4));)
        break;
    case MP_CSP_TRC_GAMMA18:
        GLSL(color.rgb = pow(color.rgb, vec3(1.8));)
        break;
    case MP_CSP_TRC_GAMMA20:
        GLSL(color.rgb = pow(color.rgb, vec3(2.0));)
        break;
    case MP_CSP_TRC_GAMMA22:
        GLSL(color.rgb = pow(color.rgb, vec3(2.2));)
        break;
    case MP_CSP_TRC_GAMMA24:
        GLSL(color.rgb = pow(color.rgb, vec3(2.4));)
        break;
    case MP_CSP_TRC_GAMMA26:
        GLSL(color.rgb = pow(color.rgb, vec3(2.6));)
        break;
    case MP_CSP_TRC_GAMMA28:
        GLSL(color.rgb = pow(color.rgb, vec3(2.8));)
        break;
    case MP_CSP_TRC_PRO_PHOTO:
        GLSLF("color.rgb = mix(color.rgb * vec3(1.0/16.0),              \n"
              "                pow(color.rgb, vec3(1.8)),               \n"
              "                %s(lessThan(vec3(0.03125), color.rgb))); \n",
              gl_sc_bvec(sc, 3));
        break;
    case MP_CSP_TRC_PQ:
        GLSLF("color.rgb = pow(color.rgb, vec3(1.0/%f));\n", PQ_M2);
        GLSLF("color.rgb = max(color.rgb - vec3(%f), vec3(0.0)) \n"
              "             / (vec3(%f) - vec3(%f) * color.rgb);\n",
              PQ_C1, PQ_C2, PQ_C3);
        GLSLF("color.rgb = pow(color.rgb, vec3(%f));\n", 1.0 / PQ_M1);
        // PQ's output range is 0-10000, but we need it to be relative to to
        // MP_REF_WHITE instead, so rescale
        GLSLF("color.rgb *= vec3(%f);\n", 10000 / MP_REF_WHITE);
        break;
    case MP_CSP_TRC_HLG:
        GLSLF("color.rgb = mix(vec3(4.0) * color.rgb * color.rgb,\n"
              "                exp((color.rgb - vec3(%f)) * vec3(1.0/%f)) + vec3(%f),\n"
              "                %s(lessThan(vec3(0.5), color.rgb)));\n",
              HLG_C, HLG_A, HLG_B, gl_sc_bvec(sc, 3));
        GLSLF("color.rgb *= vec3(1.0/%f);\n", MP_REF_WHITE_HLG);
        break;
    case MP_CSP_TRC_V_LOG:
        GLSLF("color.rgb = mix((color.rgb - vec3(0.125)) * vec3(1.0/5.6), \n"
              "    pow(vec3(10.0), (color.rgb - vec3(%f)) * vec3(1.0/%f)) \n"
              "              - vec3(%f),                                  \n"
              "    %s(lessThanEqual(vec3(0.181), color.rgb)));            \n",
              VLOG_D, VLOG_C, VLOG_B, gl_sc_bvec(sc, 3));
        break;
    case MP_CSP_TRC_S_LOG1:
        GLSLF("color.rgb = pow(vec3(10.0), (color.rgb - vec3(%f)) * vec3(1.0/%f))\n"
              "            - vec3(%f);\n",
              SLOG_C, SLOG_A, SLOG_B);
        break;
    case MP_CSP_TRC_S_LOG2:
        GLSLF("color.rgb = mix((color.rgb - vec3(%f)) * vec3(1.0/%f),      \n"
              "    (pow(vec3(10.0), (color.rgb - vec3(%f)) * vec3(1.0/%f)) \n"
              "              - vec3(%f)) * vec3(1.0/%f),                   \n"
              "    %s(lessThanEqual(vec3(%f), color.rgb)));                \n",
              SLOG_Q, SLOG_P, SLOG_C, SLOG_A, SLOG_B, SLOG_K2, gl_sc_bvec(sc, 3), SLOG_Q);
        break;
    default:
        abort();
    }

    // Rescale to prevent clipping on non-float textures
    GLSLF("color.rgb *= vec3(1.0/%f);\n", mp_trc_nom_peak(trc));
}

// Delinearize (compress), given a TRC as output. This corresponds to the
// inverse EOTF (not the OETF) in ITU-R terminology, again assuming a
// reference monitor.
//
// Like pass_linearize, this functions ingests values on an normalized scale
void pass_delinearize(struct gl_shader_cache *sc, enum mp_csp_trc trc)
{
    if (trc == MP_CSP_TRC_LINEAR)
        return;

    GLSLF("// delinearize\n");
    GLSL(color.rgb = clamp(color.rgb, 0.0, 1.0);)
    GLSLF("color.rgb *= vec3(%f);\n", mp_trc_nom_peak(trc));

    switch (trc) {
    case MP_CSP_TRC_SRGB:
        GLSLF("color.rgb = mix(color.rgb * vec3(12.92),                       \n"
              "               vec3(1.055) * pow(color.rgb, vec3(1.0/2.4))     \n"
              "                   - vec3(0.055),                              \n"
              "               %s(lessThanEqual(vec3(0.0031308), color.rgb))); \n",
              gl_sc_bvec(sc, 3));
        break;
    case MP_CSP_TRC_BT_1886:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.4));)
        break;
    case MP_CSP_TRC_GAMMA18:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/1.8));)
        break;
    case MP_CSP_TRC_GAMMA20:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.0));)
        break;
    case MP_CSP_TRC_GAMMA22:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.2));)
        break;
    case MP_CSP_TRC_GAMMA24:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.4));)
        break;
    case MP_CSP_TRC_GAMMA26:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.6));)
        break;
    case MP_CSP_TRC_GAMMA28:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.8));)
        break;
    case MP_CSP_TRC_PRO_PHOTO:
        GLSLF("color.rgb = mix(color.rgb * vec3(16.0),                        \n"
              "                pow(color.rgb, vec3(1.0/1.8)),                 \n"
              "                %s(lessThanEqual(vec3(0.001953), color.rgb))); \n",
              gl_sc_bvec(sc, 3));
        break;
    case MP_CSP_TRC_PQ:
        GLSLF("color.rgb *= vec3(1.0/%f);\n", 10000 / MP_REF_WHITE);
        GLSLF("color.rgb = pow(color.rgb, vec3(%f));\n", PQ_M1);
        GLSLF("color.rgb = (vec3(%f) + vec3(%f) * color.rgb) \n"
              "             / (vec3(1.0) + vec3(%f) * color.rgb);\n",
              PQ_C1, PQ_C2, PQ_C3);
        GLSLF("color.rgb = pow(color.rgb, vec3(%f));\n", PQ_M2);
        break;
    case MP_CSP_TRC_HLG:
        GLSLF("color.rgb *= vec3(%f);\n", MP_REF_WHITE_HLG);
        GLSLF("color.rgb = mix(vec3(0.5) * sqrt(color.rgb),\n"
              "                vec3(%f) * log(color.rgb - vec3(%f)) + vec3(%f),\n"
              "                %s(lessThan(vec3(1.0), color.rgb)));\n",
              HLG_A, HLG_B, HLG_C, gl_sc_bvec(sc, 3));
        break;
    case MP_CSP_TRC_V_LOG:
        GLSLF("color.rgb = mix(vec3(5.6) * color.rgb + vec3(0.125),   \n"
              "                vec3(%f) * log(color.rgb + vec3(%f))   \n"
              "                    + vec3(%f),                        \n"
              "                %s(lessThanEqual(vec3(0.01), color.rgb))); \n",
              VLOG_C / M_LN10, VLOG_B, VLOG_D, gl_sc_bvec(sc, 3));
        break;
    case MP_CSP_TRC_S_LOG1:
        GLSLF("color.rgb = vec3(%f) * log(color.rgb + vec3(%f)) + vec3(%f);\n",
              SLOG_A / M_LN10, SLOG_B, SLOG_C);
        break;
    case MP_CSP_TRC_S_LOG2:
        GLSLF("color.rgb = mix(vec3(%f) * color.rgb + vec3(%f),                \n"
              "                vec3(%f) * log(vec3(%f) * color.rgb + vec3(%f)) \n"
              "                    + vec3(%f),                                 \n"
              "                %s(lessThanEqual(vec3(0.0), color.rgb)));       \n",
              SLOG_P, SLOG_Q, SLOG_A / M_LN10, SLOG_K2, SLOG_B, SLOG_C, gl_sc_bvec(sc, 3));
        break;
    default:
        abort();
    }
}

// Apply the OOTF mapping from a given light type to display-referred light.
// Assumes absolute scale values. `peak` is used to tune the OOTF where
// applicable (currently only HLG).
static void pass_ootf(struct gl_shader_cache *sc, enum mp_csp_light light,
                      float peak)
{
    if (light == MP_CSP_LIGHT_DISPLAY)
        return;

    GLSLF("// apply ootf\n");

    switch (light)
    {
    case MP_CSP_LIGHT_SCENE_HLG: {
        // HLG OOTF from BT.2100, scaled to the chosen display peak
        float gamma = MPMAX(1.0, 1.2 + 0.42 * log10(peak * MP_REF_WHITE / 1000.0));
        GLSLF("color.rgb *= vec3(%f * pow(dot(src_luma, color.rgb), %f));\n",
              peak / pow(12.0 / MP_REF_WHITE_HLG, gamma), gamma - 1.0);
        break;
    }
    case MP_CSP_LIGHT_SCENE_709_1886:
        // This OOTF is defined by encoding the result as 709 and then decoding
        // it as 1886; although this is called 709_1886 we actually use the
        // more precise (by one decimal) values from BT.2020 instead
        GLSLF("color.rgb = mix(color.rgb * vec3(4.5),                  \n"
              "                vec3(1.0993) * pow(color.rgb, vec3(0.45)) - vec3(0.0993), \n"
              "                %s(lessThan(vec3(0.0181), color.rgb))); \n",
              gl_sc_bvec(sc, 3));
        GLSL(color.rgb = pow(color.rgb, vec3(2.4));)
        break;
    case MP_CSP_LIGHT_SCENE_1_2:
        GLSL(color.rgb = pow(color.rgb, vec3(1.2));)
        break;
    default:
        abort();
    }
}

// Inverse of the function pass_ootf, for completeness' sake.
static void pass_inverse_ootf(struct gl_shader_cache *sc, enum mp_csp_light light,
                              float peak)
{
    if (light == MP_CSP_LIGHT_DISPLAY)
        return;

    GLSLF("// apply inverse ootf\n");

    switch (light)
    {
    case MP_CSP_LIGHT_SCENE_HLG: {
        float gamma = MPMAX(1.0, 1.2 + 0.42 * log10(peak * MP_REF_WHITE / 1000.0));
        GLSLF("color.rgb *= vec3(1.0/%f);\n", peak / pow(12.0 / MP_REF_WHITE_HLG, gamma));
        GLSLF("color.rgb /= vec3(max(1e-6, pow(dot(src_luma, color.rgb), %f)));\n",
              (gamma - 1.0) / gamma);
        break;
    }
    case MP_CSP_LIGHT_SCENE_709_1886:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.4));)
        GLSLF("color.rgb = mix(color.rgb * vec3(1.0/4.5),               \n"
              "                pow((color.rgb + vec3(0.0993)) * vec3(1.0/1.0993), \n"
              "                    vec3(1/0.45)),                       \n"
              "                %s(lessThan(vec3(0.08145), color.rgb))); \n",
              gl_sc_bvec(sc, 3));
        break;
    case MP_CSP_LIGHT_SCENE_1_2:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/1.2));)
        break;
    default:
        abort();
    }
}

// Average light level for SDR signals. This is equal to a signal level of 0.5
// under a typical presentation gamma of about 2.0.
static const float sdr_avg = 0.25;

static void hdr_update_peak(struct gl_shader_cache *sc,
                            const struct gl_tone_map_opts *opts)
{
    // Update the sig_peak/sig_avg from the old SSBO state
    GLSL(if (average.y > 0.0) {)
    GLSL(    sig_avg  = max(1e-3, average.x);)
    GLSL(    sig_peak = max(1.00, average.y);)
    GLSL(})

    // Chosen to avoid overflowing on an 8K buffer
    const float log_min = 1e-3, log_scale = 400.0, sig_scale = 10000.0;

    // For performance, and to avoid overflows, we tally up the sub-results per
    // pixel using shared memory first
    GLSLH(shared int wg_sum;)
    GLSLH(shared uint wg_max;)
    GLSL(wg_sum = 0; wg_max = 0;)
    GLSL(barrier();)
    GLSLF("float sig_log = log(max(sig_max, %f));\n", log_min);
    GLSLF("atomicAdd(wg_sum, int(sig_log * %f));\n", log_scale);
    GLSLF("atomicMax(wg_max, uint(sig_max * %f));\n", sig_scale);

    // Have one thread per work group update the global atomics
    GLSL(memoryBarrierShared();)
    GLSL(barrier();)
    GLSL(if (gl_LocalInvocationIndex == 0) {)
    GLSL(    int wg_avg = wg_sum / int(gl_WorkGroupSize.x * gl_WorkGroupSize.y);)
    GLSL(    atomicAdd(frame_sum, wg_avg);)
    GLSL(    atomicMax(frame_max, wg_max);)
    GLSL(    memoryBarrierBuffer();)
    GLSL(})
    GLSL(barrier();)

    // Finally, to update the global state, we increment a counter per dispatch
    GLSL(uint num_wg = gl_NumWorkGroups.x * gl_NumWorkGroups.y;)
    GLSL(if (gl_LocalInvocationIndex == 0 && atomicAdd(counter, 1) == num_wg - 1) {)
    GLSL(    counter = 0;)
    GLSL(    vec2 cur = vec2(float(frame_sum) / float(num_wg), frame_max);)
    GLSLF("  cur *= vec2(1.0/%f, 1.0/%f);\n", log_scale, sig_scale);
    GLSL(    cur.x = exp(cur.x);)
    GLSL(    if (average.y == 0.0))
    GLSL(        average = cur;)

    // Use an IIR low-pass filter to smooth out the detected values, with a
    // configurable decay rate based on the desired time constant (tau)
    float a = 1.0 - cos(1.0 / opts->decay_rate);
    float decay = sqrt(a*a + 2*a) - a;
    GLSLF("  average += %f * (cur - average);\n", decay);

    // Scene change hysteresis
    float log_db = 10.0 / log(10.0);
    GLSLF("  float weight = smoothstep(%f, %f, abs(log(cur.x / average.x)));\n",
          opts->scene_threshold_low / log_db,
          opts->scene_threshold_high / log_db);
    GLSL(    average = mix(average, cur, weight);)

    // Reset SSBO state for the next frame
    GLSL(    frame_sum = 0; frame_max = 0;)
    GLSL(    memoryBarrierBuffer();)
    GLSL(})
}

static inline float pq_delinearize(float x)
{
    x *= MP_REF_WHITE / 10000.0;
    x = powf(x, PQ_M1);
    x = (PQ_C1 + PQ_C2 * x) / (1.0 + PQ_C3 * x);
    x = pow(x, PQ_M2);
    return x;
}

// Tone map from a known peak brightness to the range [0,1]. If ref_peak
// is 0, we will use peak detection instead
static void pass_tone_map(struct gl_shader_cache *sc,
                          float src_peak, float dst_peak,
                          const struct gl_tone_map_opts *opts)
{
    GLSLF("// HDR tone mapping\n");

    // To prevent discoloration due to out-of-bounds clipping, we need to make
    // sure to reduce the value range as far as necessary to keep the entire
    // signal in range, so tone map based on the brightest component.
    GLSL(int sig_idx = 0;)
    GLSL(if (color[1] > color[sig_idx]) sig_idx = 1;)
    GLSL(if (color[2] > color[sig_idx]) sig_idx = 2;)
    GLSL(float sig_max = color[sig_idx];)
    GLSLF("float sig_peak = %f;\n", src_peak);
    GLSLF("float sig_avg = %f;\n", sdr_avg);

    if (opts->compute_peak >= 0)
        hdr_update_peak(sc, opts);

    // Always hard-clip the upper bound of the signal range to avoid functions
    // exploding on inputs greater than 1.0
    GLSLF("vec3 sig = min(color.rgb, sig_peak);\n");

    // This function always operates on an absolute scale, so ignore the
    // dst_peak normalization for it
    float dst_scale = dst_peak;
    if (opts->curve == TONE_MAPPING_BT_2390)
        dst_scale = 1.0;

    // Rescale the variables in order to bring it into a representation where
    // 1.0 represents the dst_peak. This is because all of the tone mapping
    // algorithms are defined in such a way that they map to the range [0.0, 1.0].
    if (dst_scale > 1.0) {
        GLSLF("sig *= 1.0/%f;\n", dst_scale);
        GLSLF("sig_peak *= 1.0/%f;\n", dst_scale);
    }

    GLSL(float sig_orig = sig[sig_idx];)
    GLSLF("float slope = min(%f, %f / sig_avg);\n", opts->max_boost, sdr_avg);
    GLSL(sig *= slope;)
    GLSL(sig_peak *= slope;)

    float param = opts->curve_param;
    switch (opts->curve) {
    case TONE_MAPPING_CLIP:
        GLSLF("sig = min(%f * sig, 1.0);\n", isnan(param) ? 1.0 : param);
        break;

    case TONE_MAPPING_MOBIUS:
        GLSLF("if (sig_peak > (1.0 + 1e-6)) {\n");
        GLSLF("const float j = %f;\n", isnan(param) ? 0.3 : param);
        // solve for M(j) = j; M(sig_peak) = 1.0; M'(j) = 1.0
        // where M(x) = scale * (x+a)/(x+b)
        GLSLF("float a = -j*j * (sig_peak - 1.0) / (j*j - 2.0*j + sig_peak);\n");
        GLSLF("float b = (j*j - 2.0*j*sig_peak + sig_peak) / "
              "max(1e-6, sig_peak - 1.0);\n");
        GLSLF("float scale = (b*b + 2.0*b*j + j*j) / (b-a);\n");
        GLSLF("sig = mix(sig, scale * (sig + vec3(a)) / (sig + vec3(b)),"
              "          %s(greaterThan(sig, vec3(j))));\n",
              gl_sc_bvec(sc, 3));
        GLSLF("}\n");
        break;

    case TONE_MAPPING_REINHARD: {
        float contrast = isnan(param) ? 0.5 : param,
              offset = (1.0 - contrast) / contrast;
        GLSLF("sig = sig / (sig + vec3(%f));\n", offset);
        GLSLF("float scale = (sig_peak + %f) / sig_peak;\n", offset);
        GLSL(sig *= scale;)
        break;
    }

    case TONE_MAPPING_HABLE: {
        float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
        GLSLHF("vec3 hable(vec3 x) {\n");
        GLSLHF("return (x * (%f*x + vec3(%f)) + vec3(%f)) / "
               "       (x * (%f*x + vec3(%f)) + vec3(%f)) "
               "       - vec3(%f);\n",
               A, C*B, D*E,
               A, B, D*F,
               E/F);
        GLSLHF("}\n");
        GLSLF("sig = hable(max(vec3(0.0), sig)) / hable(vec3(sig_peak)).x;\n");
        break;
    }

    case TONE_MAPPING_GAMMA: {
        float gamma = isnan(param) ? 1.8 : param;
        GLSLF("const float cutoff = 0.05, gamma = 1.0/%f;\n", gamma);
        GLSL(float scale = pow(cutoff / sig_peak, gamma.x) / cutoff;)
        GLSLF("sig = mix(scale * sig,"
              "          pow(sig / sig_peak, vec3(gamma)),"
              "          %s(greaterThan(sig, vec3(cutoff))));\n",
              gl_sc_bvec(sc, 3));
        break;
    }

    case TONE_MAPPING_LINEAR: {
        float coeff = isnan(param) ? 1.0 : param;
        GLSLF("sig = min(%f / sig_peak, 1.0) * sig;\n", coeff);
        break;
    }

    case TONE_MAPPING_BT_2390:
        // We first need to encode both sig and sig_peak into PQ space
        GLSLF("vec4 sig_pq = vec4(sig.rgb, sig_peak);                           \n"
              "sig_pq *= vec4(1.0/%f);                                          \n"
              "sig_pq = pow(sig_pq, vec4(%f));                                  \n"
              "sig_pq = (vec4(%f) + vec4(%f) * sig_pq)                          \n"
              "          / (vec4(1.0) + vec4(%f) * sig_pq);                     \n"
              "sig_pq = pow(sig_pq, vec4(%f));                                  \n",
              10000.0 / MP_REF_WHITE, PQ_M1, PQ_C1, PQ_C2, PQ_C3, PQ_M2);
        // Encode both the signal and the target brightness to be relative to
        // the source peak brightness, and figure out the target peak in this space
        GLSLF("float scale = 1.0 / sig_pq.a;                                    \n"
              "sig_pq.rgb *= vec3(scale);                                       \n"
              "float maxLum = %f * scale;                                       \n",
              pq_delinearize(dst_peak));
        // Apply piece-wise hermite spline
        GLSLF("float ks = 1.5 * maxLum - 0.5;                                   \n"
              "vec3 tb = (sig_pq.rgb - vec3(ks)) / vec3(1.0 - ks);              \n"
              "vec3 tb2 = tb * tb;                                              \n"
              "vec3 tb3 = tb2 * tb;                                             \n"
              "vec3 pb = (2.0 * tb3 - 3.0 * tb2 + vec3(1.0)) * vec3(ks) +       \n"
              "          (tb3 - 2.0 * tb2 + tb) * vec3(1.0 - ks) +              \n"
              "          (-2.0 * tb3 + 3.0 * tb2) * vec3(maxLum);               \n"
              "sig = mix(pb, sig_pq.rgb, %s(lessThan(sig_pq.rgb, vec3(ks))));   \n",
              gl_sc_bvec(sc, 3));
        // Convert back from PQ space to linear light
        GLSLF("sig *= vec3(sig_pq.a);                                           \n"
              "sig = pow(sig, vec3(1.0/%f));                                    \n"
              "sig = max(sig - vec3(%f), 0.0) /                                 \n"
              "          (vec3(%f) - vec3(%f) * sig);                           \n"
              "sig = pow(sig, vec3(1.0/%f));                                    \n"
              "sig *= vec3(%f);                                                 \n",
              PQ_M2, PQ_C1, PQ_C2, PQ_C3, PQ_M1, 10000.0 / MP_REF_WHITE);
        break;

    default:
        abort();
    }

    GLSL(vec3 sig_lin = color.rgb * (sig[sig_idx] / sig_orig);)

    // Mix between the per-channel tone mapped and the linear tone mapped
    // signal based on the desaturation strength
    if (opts->desat > 0) {
        float base = 0.18 * dst_scale;
        GLSLF("float coeff = max(sig[sig_idx] - %f, 1e-6) / "
              "              max(sig[sig_idx], 1.0);\n", base);
        GLSLF("coeff = %f * pow(coeff, %f);\n", opts->desat, opts->desat_exp);
        GLSLF("color.rgb = mix(sig_lin, %f * sig, coeff);\n", dst_scale);
    } else {
        GLSL(color.rgb = sig_lin;)
    }
}

// Map colors from one source space to another. These source spaces must be
// known (i.e. not MP_CSP_*_AUTO), as this function won't perform any
// auto-guessing. If is_linear is true, we assume the input has already been
// linearized (e.g. for linear-scaling). If `opts->compute_peak` is true, we
// will detect the peak instead of relying on metadata. Note that this requires
// the caller to have already bound the appropriate SSBO and set up the compute
// shader metadata
void pass_color_map(struct gl_shader_cache *sc, bool is_linear,
                    struct mp_colorspace src, struct mp_colorspace dst,
                    const struct gl_tone_map_opts *opts)
{
    GLSLF("// color mapping\n");

    // Some operations need access to the video's luma coefficients, so make
    // them available
    float rgb2xyz[3][3];
    mp_get_rgb2xyz_matrix(mp_get_csp_primaries(src.primaries), rgb2xyz);
    gl_sc_uniform_vec3(sc, "src_luma", rgb2xyz[1]);
    mp_get_rgb2xyz_matrix(mp_get_csp_primaries(dst.primaries), rgb2xyz);
    gl_sc_uniform_vec3(sc, "dst_luma", rgb2xyz[1]);

    bool need_ootf = src.light != dst.light;
    if (src.light == MP_CSP_LIGHT_SCENE_HLG && src.sig_peak != dst.sig_peak)
        need_ootf = true;

    // All operations from here on require linear light as a starting point,
    // so we linearize even if src.gamma == dst.gamma when one of the other
    // operations needs it
    bool need_linear = src.gamma != dst.gamma ||
                       src.primaries != dst.primaries ||
                       src.sig_peak != dst.sig_peak ||
                       need_ootf;

    if (need_linear && !is_linear) {
        // We also pull it up so that 1.0 is the reference white
        pass_linearize(sc, src.gamma);
        is_linear = true;
    }

    // Pre-scale the incoming values into an absolute scale
    GLSLF("color.rgb *= vec3(%f);\n", mp_trc_nom_peak(src.gamma));

    if (need_ootf)
        pass_ootf(sc, src.light, src.sig_peak);

    // Tone map to prevent clipping due to excessive brightness
    if (src.sig_peak > dst.sig_peak)
        pass_tone_map(sc, src.sig_peak, dst.sig_peak, opts);

    // Adapt to the right colorspace if necessary
    if (src.primaries != dst.primaries) {
        struct mp_csp_primaries csp_src = mp_get_csp_primaries(src.primaries),
                                csp_dst = mp_get_csp_primaries(dst.primaries);
        float m[3][3] = {{0}};
        mp_get_cms_matrix(csp_src, csp_dst, MP_INTENT_RELATIVE_COLORIMETRIC, m);
        gl_sc_uniform_mat3(sc, "cms_matrix", true, &m[0][0]);
        GLSL(color.rgb = cms_matrix * color.rgb;)

        if (opts->gamut_clipping) {
            GLSL(float cmin = min(min(color.r, color.g), color.b);)
            GLSL(if (cmin < 0.0) {
                     float luma = dot(dst_luma, color.rgb);
                     float coeff = cmin / (cmin - luma);
                     color.rgb = mix(color.rgb, vec3(luma), coeff);
                 })
            GLSLF("float cmax = 1.0/%f * max(max(color.r, color.g), color.b);\n",
                  dst.sig_peak);
            GLSL(if (cmax > 1.0) color.rgb /= cmax;)
        }
    }

    if (need_ootf)
        pass_inverse_ootf(sc, dst.light, dst.sig_peak);

    // Post-scale the outgoing values from absolute scale to normalized.
    // For SDR, we normalize to the chosen signal peak. For HDR, we normalize
    // to the encoding range of the transfer function.
    float dst_range = dst.sig_peak;
    if (mp_trc_is_hdr(dst.gamma))
        dst_range = mp_trc_nom_peak(dst.gamma);

    GLSLF("color.rgb *= vec3(%f);\n", 1.0 / dst_range);

    // Warn for remaining out-of-gamut colors is enabled
    if (opts->gamut_warning) {
        GLSL(if (any(greaterThan(color.rgb, vec3(1.005))) ||
                 any(lessThan(color.rgb, vec3(-0.005)))))
            GLSL(color.rgb = vec3(1.0) - color.rgb;) // invert
    }

    if (is_linear)
        pass_delinearize(sc, dst.gamma);
}

// Wide usage friendly PRNG, shamelessly stolen from a GLSL tricks forum post.
// Obtain random numbers by calling rand(h), followed by h = permute(h) to
// update the state. Assumes the texture was hooked.
// permute() was modified from the original to avoid "large" numbers in
// calculations, since low-end mobile GPUs choke on them (overflow).
static void prng_init(struct gl_shader_cache *sc, AVLFG *lfg)
{
    GLSLH(float mod289(float x)  { return x - floor(x * 1.0/289.0) * 289.0; })
    GLSLHF("float permute(float x) {\n");
        GLSLH(return mod289( mod289(34.0*x + 1.0) * (fract(x) + 1.0) );)
    GLSLHF("}\n");
    GLSLH(float rand(float x)    { return fract(x * 1.0/41.0); })

    // Initialize the PRNG by hashing the position + a random uniform
    GLSL(vec3 _m = vec3(HOOKED_pos, random) + vec3(1.0);)
    GLSL(float h = permute(permute(permute(_m.x)+_m.y)+_m.z);)
    gl_sc_uniform_dynamic(sc);
    gl_sc_uniform_f(sc, "random", (double)av_lfg_get(lfg) / UINT32_MAX);
}

struct deband_opts {
    int enabled;
    int iterations;
    float threshold;
    float range;
    float grain;
};

const struct deband_opts deband_opts_def = {
    .iterations = 1,
    .threshold = 32.0,
    .range = 16.0,
    .grain = 48.0,
};

#define OPT_BASE_STRUCT struct deband_opts
const struct m_sub_options deband_conf = {
    .opts = (const m_option_t[]) {
        {"iterations", OPT_INT(iterations), M_RANGE(1, 16)},
        {"threshold", OPT_FLOAT(threshold), M_RANGE(0.0, 4096.0)},
        {"range", OPT_FLOAT(range), M_RANGE(1.0, 64.0)},
        {"grain", OPT_FLOAT(grain), M_RANGE(0.0, 4096.0)},
        {0}
    },
    .size = sizeof(struct deband_opts),
    .defaults = &deband_opts_def,
};

// Stochastically sample a debanded result from a hooked texture.
void pass_sample_deband(struct gl_shader_cache *sc, struct deband_opts *opts,
                        AVLFG *lfg, enum mp_csp_trc trc)
{
    // Initialize the PRNG
    GLSLF("{\n");
    prng_init(sc, lfg);

    // Helper: Compute a stochastic approximation of the avg color around a
    // pixel
    GLSLHF("vec4 average(float range, inout float h) {\n");
        // Compute a random rangle and distance
        GLSLH(float dist = rand(h) * range;     h = permute(h);)
        GLSLH(float dir  = rand(h) * 6.2831853; h = permute(h);)
        GLSLH(vec2 o = dist * vec2(cos(dir), sin(dir));)

        // Sample at quarter-turn intervals around the source pixel
        GLSLH(vec4 ref[4];)
        GLSLH(ref[0] = HOOKED_texOff(vec2( o.x,  o.y));)
        GLSLH(ref[1] = HOOKED_texOff(vec2(-o.y,  o.x));)
        GLSLH(ref[2] = HOOKED_texOff(vec2(-o.x, -o.y));)
        GLSLH(ref[3] = HOOKED_texOff(vec2( o.y, -o.x));)

        // Return the (normalized) average
        GLSLH(return (ref[0] + ref[1] + ref[2] + ref[3])*0.25;)
    GLSLHF("}\n");

    // Sample the source pixel
    GLSL(color = HOOKED_tex(HOOKED_pos);)
    GLSLF("vec4 avg, diff;\n");
    for (int i = 1; i <= opts->iterations; i++) {
        // Sample the average pixel and use it instead of the original if
        // the difference is below the given threshold
        GLSLF("avg = average(%f, h);\n", i * opts->range);
        GLSL(diff = abs(color - avg);)
        GLSLF("color = mix(avg, color, %s(greaterThan(diff, vec4(%f))));\n",
              gl_sc_bvec(sc, 4), opts->threshold / (i * 16384.0));
    }

    // Add some random noise to smooth out residual differences
    GLSL(vec3 noise;)
    GLSL(noise.x = rand(h); h = permute(h);)
    GLSL(noise.y = rand(h); h = permute(h);)
    GLSL(noise.z = rand(h); h = permute(h);)

    // Noise is scaled to the signal level to prevent extreme noise for HDR
    float gain = opts->grain/8192.0 / mp_trc_nom_peak(trc);
    GLSLF("color.xyz += %f * (noise - vec3(0.5));\n", gain);
    GLSLF("}\n");
}

// Assumes the texture was hooked
void pass_sample_unsharp(struct gl_shader_cache *sc, float param) {
    GLSLF("{\n");
    GLSL(float st1 = 1.2;)
    GLSL(vec4 p = HOOKED_tex(HOOKED_pos);)
    GLSL(vec4 sum1 = HOOKED_texOff(st1 * vec2(+1, +1))
                   + HOOKED_texOff(st1 * vec2(+1, -1))
                   + HOOKED_texOff(st1 * vec2(-1, +1))
                   + HOOKED_texOff(st1 * vec2(-1, -1));)
    GLSL(float st2 = 1.5;)
    GLSL(vec4 sum2 = HOOKED_texOff(st2 * vec2(+1,  0))
                   + HOOKED_texOff(st2 * vec2( 0, +1))
                   + HOOKED_texOff(st2 * vec2(-1,  0))
                   + HOOKED_texOff(st2 * vec2( 0, -1));)
    GLSL(vec4 t = p * 0.859375 + sum2 * -0.1171875 + sum1 * -0.09765625;)
    GLSLF("color = p + t * %f;\n", param);
    GLSLF("}\n");
}
