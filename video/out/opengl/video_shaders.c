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
    GLSLF("#define tex texture%d\n", tex_num);
    GLSLF("vec2 pos = texcoord%d;\n", tex_num);
    GLSLF("vec2 size = texture_size%d;\n", tex_num);
    GLSLF("vec2 pt = pixel_size%d;\n", tex_num);
}

static void pass_sample_separated_get_weights(struct gl_shader_cache *sc,
                                              struct scaler *scaler)
{
    gl_sc_uniform_tex(sc, "lut", scaler->gl_target, scaler->gl_lut);
    // Define a new variable to cache the corrected fcoord.
    GLSLF("float fcoord_lut = LUT_POS(fcoord, %d.0);\n", scaler->lut_size);

    int N = scaler->kernel->size;
    if (N == 2) {
        GLSL(vec2 c1 = texture(lut, vec2(0.5, fcoord_lut)).rg;)
        GLSL(float weights[2] = float[](c1.r, c1.g);)
    } else if (N == 6) {
        GLSL(vec4 c1 = texture(lut, vec2(0.25, fcoord_lut));)
        GLSL(vec4 c2 = texture(lut, vec2(0.75, fcoord_lut));)
        GLSL(float weights[6] = float[](c1.r, c1.g, c1.b, c2.r, c2.g, c2.b);)
    } else {
        GLSLF("float weights[%d];\n", N);
        for (int n = 0; n < N / 4; n++) {
            GLSLF("c = texture(lut, vec2(1.0 / %d.0 + %d.0 / %d.0, fcoord_lut));\n",
                    N / 2, n, N / 4);
            GLSLF("weights[%d] = c.r;\n", n * 4 + 0);
            GLSLF("weights[%d] = c.g;\n", n * 4 + 1);
            GLSLF("weights[%d] = c.b;\n", n * 4 + 2);
            GLSLF("weights[%d] = c.a;\n", n * 4 + 3);
        }
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
// If subtexel < 0, samples directly. Otherwise, takes the texel from cN[comp]
static void polar_sample(struct gl_shader_cache *sc, struct scaler *scaler,
                         int x, int y, int subtexel, int components)
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
    GLSLF("d = length(vec2(%d.0, %d.0) - fcoord)/%f;\n", x, y, radius);
    // Check for samples that might be skippable
    bool maybe_skippable = dmax >= radius_cutoff - M_SQRT2;
    if (maybe_skippable)
        GLSLF("if (d < %f) {\n", radius_cutoff / radius);

    // get the weight for this pixel
    if (scaler->gl_target == GL_TEXTURE_1D) {
        GLSLF("w = texture1D(lut, LUT_POS(d, %d.0)).r;\n",
              scaler->lut_size);
    } else {
        GLSLF("w = texture(lut, vec2(0.5, LUT_POS(d, %d.0))).r;\n",
              scaler->lut_size);
    }
    GLSL(wsum += w;)

    if (subtexel < 0) {
        GLSLF("c0 = texture(tex, base + pt * vec2(%d.0, %d.0));\n", x, y);
        GLSL(color += vec4(w) * c0;)
    } else {
        for (int n = 0; n < components; n++)
            GLSLF("color[%d] += w * c%d[%d];\n", n, n, subtexel);
    }

    if (maybe_skippable)
        GLSLF("}\n");
}

void pass_sample_polar(struct gl_shader_cache *sc, struct scaler *scaler,
                       int components, int glsl_version)
{
    GLSL(color = vec4(0.0);)
    GLSLF("{\n");
    GLSL(vec2 fcoord = fract(pos * size - vec2(0.5));)
    GLSL(vec2 base = pos - fcoord * pt;)
    GLSLF("float w, d, wsum = 0.0;\n");
    for (int n = 0; n < components; n++)
        GLSLF("vec4 c%d;\n", n);

    gl_sc_uniform_tex(sc, "lut", scaler->gl_target, scaler->gl_lut);

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

            // textureGather is only supported in GLSL 400+
            if (glsl_version < 400)
                use_gather = false;

            if (use_gather) {
                // Gather the four surrounding texels simultaneously
                for (int n = 0; n < components; n++) {
                    GLSLF("c%d = textureGatherOffset(tex, base, ivec2(%d, %d), %d);\n",
                          n, x, y, n);
                }

                // Mix in all of the points with their weights
                for (int p = 0; p < 4; p++) {
                    // The four texels are gathered counterclockwise starting
                    // from the bottom left
                    static const int xo[4] = {0, 1, 1, 0};
                    static const int yo[4] = {1, 1, 0, 0};
                    if (x+xo[p] > bound || y+yo[p] > bound)
                        continue;
                    polar_sample(sc, scaler, x+xo[p], y+yo[p], p, components);
                }
            } else {
                // switch to direct sampling instead, for efficiency/compatibility
                for (int yy = y; yy <= bound && yy <= y+1; yy++) {
                    for (int xx = x; xx <= bound && xx <= x+1; xx++)
                        polar_sample(sc, scaler, xx, yy, -1, components);
                }
            }
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
    GLSLF("coeff = (coeff - %f) / %f;\n", threshold, 1.0 - 2 * threshold);
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
        GLSL(color.rgb = mix(color.rgb / vec3(12.92),
                             pow((color.rgb + vec3(0.055))/vec3(1.055), vec3(2.4)),
                             lessThan(vec3(0.04045), color.rgb));)
        break;
    case MP_CSP_TRC_BT_1886:
        GLSL(color.rgb = pow(color.rgb, vec3(2.4));)
        break;
    case MP_CSP_TRC_GAMMA18:
        GLSL(color.rgb = pow(color.rgb, vec3(1.8));)
        break;
    case MP_CSP_TRC_GAMMA22:
        GLSL(color.rgb = pow(color.rgb, vec3(2.2));)
        break;
    case MP_CSP_TRC_GAMMA28:
        GLSL(color.rgb = pow(color.rgb, vec3(2.8));)
        break;
    case MP_CSP_TRC_PRO_PHOTO:
        GLSL(color.rgb = mix(color.rgb / vec3(16.0),
                             pow(color.rgb, vec3(1.8)),
                             lessThan(vec3(0.03125), color.rgb));)
        break;
    case MP_CSP_TRC_PQ:
        GLSLF("color.rgb = pow(color.rgb, vec3(1.0/%f));\n", PQ_M2);
        GLSLF("color.rgb = max(color.rgb - vec3(%f), vec3(0.0)) \n"
              "             / (vec3(%f) - vec3(%f) * color.rgb);\n",
              PQ_C1, PQ_C2, PQ_C3);
        GLSLF("color.rgb = pow(color.rgb, vec3(1.0/%f));\n", PQ_M1);
        // PQ's output range is 0-10000, but we need it to be relative to to
        // MP_REF_WHITE instead, so rescale
        GLSLF("color.rgb *= vec3(%f);\n", 10000 / MP_REF_WHITE);
        break;
    case MP_CSP_TRC_HLG:
        GLSLF("color.rgb = mix(vec3(4.0) * color.rgb * color.rgb,\n"
              "                exp((color.rgb - vec3(%f)) / vec3(%f)) + vec3(%f),\n"
              "                lessThan(vec3(0.5), color.rgb));\n",
              HLG_C, HLG_A, HLG_B);
        break;
    case MP_CSP_TRC_V_LOG:
        GLSLF("color.rgb = mix((color.rgb - vec3(0.125)) / vec3(5.6), \n"
              "    pow(vec3(10.0), (color.rgb - vec3(%f)) / vec3(%f)) \n"
              "              - vec3(%f),                              \n"
              "    lessThanEqual(vec3(0.181), color.rgb));            \n",
              VLOG_D, VLOG_C, VLOG_B);
        break;
    case MP_CSP_TRC_S_LOG1:
        GLSLF("color.rgb = pow(vec3(10.0), (color.rgb - vec3(%f)) / vec3(%f))\n"
              "            - vec3(%f);\n",
              SLOG_C, SLOG_A, SLOG_B);
        break;
    case MP_CSP_TRC_S_LOG2:
        GLSLF("color.rgb = mix((color.rgb - vec3(%f)) / vec3(%f),      \n"
              "    (pow(vec3(10.0), (color.rgb - vec3(%f)) / vec3(%f)) \n"
              "              - vec3(%f)) / vec3(%f),                   \n"
              "    lessThanEqual(vec3(%f), color.rgb));                \n",
              SLOG_Q, SLOG_P, SLOG_C, SLOG_A, SLOG_B, SLOG_K2, SLOG_Q);
        break;
    default:
        abort();
    }

    // Rescale to prevent clipping on non-float textures
    GLSLF("color.rgb /= vec3(%f);\n", mp_trc_nom_peak(trc));
}

// Delinearize (compress), given a TRC as output. This corresponds to the
// inverse EOTF (not the OETF) in ITU-R terminology, again assuming a
// reference monitor.
void pass_delinearize(struct gl_shader_cache *sc, enum mp_csp_trc trc)
{
    if (trc == MP_CSP_TRC_LINEAR)
        return;

    GLSLF("// delinearize\n");
    GLSL(color.rgb = clamp(color.rgb, 0.0, 1.0);)
    GLSLF("color.rgb *= vec3(%f);\n", mp_trc_nom_peak(trc));

    switch (trc) {
    case MP_CSP_TRC_SRGB:
        GLSL(color.rgb = mix(color.rgb * vec3(12.92),
                             vec3(1.055) * pow(color.rgb, vec3(1.0/2.4))
                                 - vec3(0.055),
                             lessThanEqual(vec3(0.0031308), color.rgb));)
        break;
    case MP_CSP_TRC_BT_1886:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.4));)
        break;
    case MP_CSP_TRC_GAMMA18:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/1.8));)
        break;
    case MP_CSP_TRC_GAMMA22:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.2));)
        break;
    case MP_CSP_TRC_GAMMA28:
        GLSL(color.rgb = pow(color.rgb, vec3(1.0/2.8));)
        break;
    case MP_CSP_TRC_PRO_PHOTO:
        GLSL(color.rgb = mix(color.rgb * vec3(16.0),
                             pow(color.rgb, vec3(1.0/1.8)),
                             lessThanEqual(vec3(0.001953), color.rgb));)
        break;
    case MP_CSP_TRC_PQ:
        GLSLF("color.rgb /= vec3(%f);\n", 10000 / MP_REF_WHITE);
        GLSLF("color.rgb = pow(color.rgb, vec3(%f));\n", PQ_M1);
        GLSLF("color.rgb = (vec3(%f) + vec3(%f) * color.rgb) \n"
              "             / (vec3(1.0) + vec3(%f) * color.rgb);\n",
              PQ_C1, PQ_C2, PQ_C3);
        GLSLF("color.rgb = pow(color.rgb, vec3(%f));\n", PQ_M2);
        break;
    case MP_CSP_TRC_HLG:
        GLSLF("color.rgb = mix(vec3(0.5) * sqrt(color.rgb),\n"
              "                vec3(%f) * log(color.rgb - vec3(%f)) + vec3(%f),\n"
              "                lessThan(vec3(1.0), color.rgb));\n",
              HLG_A, HLG_B, HLG_C);
        break;
    case MP_CSP_TRC_V_LOG:
        GLSLF("color.rgb = mix(vec3(5.6) * color.rgb + vec3(0.125),   \n"
              "                vec3(%f) * log(color.rgb + vec3(%f))   \n"
              "                    + vec3(%f),                        \n"
              "                lessThanEqual(vec3(0.01), color.rgb)); \n",
              VLOG_C / M_LN10, VLOG_B, VLOG_D);
        break;
    case MP_CSP_TRC_S_LOG1:
        GLSLF("color.rgb = vec3(%f) * log(color.rgb + vec3(%f)) + vec3(%f);\n",
              SLOG_A / M_LN10, SLOG_B, SLOG_C);
        break;
    case MP_CSP_TRC_S_LOG2:
        GLSLF("color.rgb = mix(vec3(%f) * color.rgb + vec3(%f),                \n"
              "                vec3(%f) * log(vec3(%f) * color.rgb + vec3(%f)) \n"
              "                    + vec3(%f),                                 \n"
              "                lessThanEqual(vec3(0.0), color.rgb));           \n",
              SLOG_P, SLOG_Q, SLOG_A / M_LN10, SLOG_K2, SLOG_B, SLOG_C);
        break;
    default:
        abort();
    }
}

// Apply the OOTF mapping from a given light type to display-referred light.
// The extra peak parameter is used to scale the values before and after
// the OOTF, and can be inferred using mp_trc_nom_peak
void pass_ootf(struct gl_shader_cache *sc, enum mp_csp_light light, float peak)
{
    if (light == MP_CSP_LIGHT_DISPLAY)
        return;

    GLSLF("// apply ootf\n", sc);
    GLSLF("color.rgb *= vec3(%f);\n", peak);

    switch (light)
    {
    case MP_CSP_LIGHT_SCENE_HLG:
        // HLG OOTF from BT.2100, assuming a reference display with a
        // peak of 1000 cd/m² -> gamma = 1.2
        GLSLF("color.rgb *= vec3(%f * pow(dot(src_luma, color.rgb), 0.2));\n",
              (1000 / MP_REF_WHITE) / pow(12, 1.2));
        break;
    case MP_CSP_LIGHT_SCENE_709_1886:
        // This OOTF is defined by encoding the result as 709 and then decoding
        // it as 1886; although this is called 709_1886 we actually use the
        // more precise (by one decimal) values from BT.2020 instead
        GLSL(color.rgb = mix(color.rgb * vec3(4.5),
                             vec3(1.0993) * pow(color.rgb, vec3(0.45)) - vec3(0.0993),
                             lessThan(vec3(0.0181), color.rgb));)
        GLSL(color.rgb = pow(color.rgb, vec3(2.4));)
        break;
    case MP_CSP_LIGHT_SCENE_1_2:
        GLSL(color.rgb = pow(color.rgb, vec3(1.2));)
        break;
    default:
        abort();
    }

    GLSLF("color.rgb /= vec3(%f);\n", peak);
}

// Inverse of the function pass_ootf, for completeness' sake. Note that the
// inverse OOTF for MP_CSP_LIGHT_SCENE_HLG has no analytical solution and is
// therefore unimplemented. Care must be used to never call this function
// in that way.(In principle, a iterative algorithm can approach
// the solution numerically, but this is tricky and we don't really need it
// since mpv currently only supports outputting display-referred light)
void pass_inverse_ootf(struct gl_shader_cache *sc, enum mp_csp_light light, float peak)
{
    if (light == MP_CSP_LIGHT_DISPLAY)
        return;

    GLSLF("// apply inverse ootf\n");
    GLSLF("color.rgb *= vec3(%f);\n", peak);

    switch (light)
    {
    case MP_CSP_LIGHT_SCENE_HLG:
        // Has no analytical solution
        abort();
        break;
    case MP_CSP_LIGHT_SCENE_709_1886:
        GLSL(color.rgb = pow(color.rgb, vec3(1/2.4));)
        GLSL(color.rgb = mix(color.rgb / vec3(4.5),
                             pow((color.rgb + vec3(0.0993)) / vec3(1.0993), vec3(1/0.45)),
                             lessThan(vec3(0.08145), color.rgb));)
        break;
    case MP_CSP_LIGHT_SCENE_1_2:
        GLSL(color.rgb = pow(color.rgb, vec3(1/1.2));)
        break;
    default:
        abort();
    }

    GLSLF("color.rgb /= vec3(%f);\n", peak);
}

// Tone map from a known peak brightness to the range [0,1]
static void pass_tone_map(struct gl_shader_cache *sc, float ref_peak,
                          enum tone_mapping algo, float param, float desat)
{
    GLSLF("// HDR tone mapping\n");

    // To prevent discoloration, we tone map on the luminance only
    GLSL(float luma = dot(src_luma, color.rgb);)
    GLSL(float luma_orig = luma;)

    // Desaturate the color using a coefficient dependent on the brightness
    if (desat > 0 && ref_peak > desat) {
        GLSLF("float overbright = max(luma - %f, 1e-6) / max(luma, 1e-6);\n", desat);
        GLSL(color.rgb = mix(color.rgb, vec3(luma), overbright);)
    }

    switch (algo) {
    case TONE_MAPPING_CLIP:
        GLSLF("luma = clamp(%f * luma, 0.0, 1.0);\n", isnan(param) ? 1.0 : param);
        break;

    case TONE_MAPPING_MOBIUS: {
        float j = isnan(param) ? 0.3 : param;
        // solve for M(j) = j; M(ref_peak) = 1.0; M'(j) = 1.0
        // where M(x) = scale * (x+a)/(x+b)
        float a = -j*j * (ref_peak - 1) / (j*j - 2*j + ref_peak),
              b = (j*j - 2*j*ref_peak + ref_peak) / (ref_peak - 1);

        GLSLF("luma = mix(%f * (luma + %f) / (luma + %f), luma, luma <= %f);\n",
              (b*b + 2*b*j + j*j) / (b - a), a, b, j);
        break;
    }

    case TONE_MAPPING_REINHARD: {
        float contrast = isnan(param) ? 0.5 : param,
              offset = (1.0 - contrast) / contrast;
        GLSLF("luma = luma / (luma + %f);\n", offset);
        GLSLF("luma *= %f;\n", (ref_peak + offset) / ref_peak);
        break;
    }

    case TONE_MAPPING_HABLE: {
        float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
        GLSLHF("float hable(float x) {\n");
        GLSLHF("return ((x * (%f*x + %f)+%f)/(x * (%f*x + %f) + %f)) - %f;\n",
               A, C*B, D*E, A, B, D*F, E/F);
        GLSLHF("}\n");

        GLSLF("luma = hable(luma) / hable(%f);\n", ref_peak);
        break;
    }

    case TONE_MAPPING_GAMMA: {
        float gamma = isnan(param) ? 1.8 : param;
        GLSLF("luma = pow(luma / %f, %f);\n", ref_peak, 1.0/gamma);
        break;
    }

    case TONE_MAPPING_LINEAR: {
        float coeff = isnan(param) ? 1.0 : param;
        GLSLF("luma = %f * luma;\n", coeff / ref_peak);
        break;
    }

    default:
        abort();
    }

    // Apply the computed brightness difference back to the original color
    GLSL(color.rgb *= luma / luma_orig;)
}

// Map colors from one source space to another. These source spaces must be
// known (i.e. not MP_CSP_*_AUTO), as this function won't perform any
// auto-guessing. If is_linear is true, we assume the input has already been
// linearized (e.g. for linear-scaling)
void pass_color_map(struct gl_shader_cache *sc,
                    struct mp_colorspace src, struct mp_colorspace dst,
                    enum tone_mapping algo, float tone_mapping_param,
                    float tone_mapping_desat, bool is_linear)
{
    GLSLF("// color mapping\n");

    // Compute the highest encodable level
    float src_range = mp_trc_nom_peak(src.gamma),
          dst_range = mp_trc_nom_peak(dst.gamma);

    // Some operations need access to the video's luma coefficients (src
    // colorspace), so make it available
    struct mp_csp_primaries prim = mp_get_csp_primaries(src.primaries);
    float rgb2xyz[3][3];
    mp_get_rgb2xyz_matrix(prim, rgb2xyz);
    gl_sc_uniform_vec3(sc, "src_luma", rgb2xyz[1]);

    // All operations from here on require linear light as a starting point,
    // so we linearize even if src.gamma == dst.gamma when one of the other
    // operations needs it
    bool need_gamma = src.gamma != dst.gamma ||
                      src.primaries != dst.primaries ||
                      src_range != dst_range ||
                      src.sig_peak > dst_range ||
                      src.light != dst.light;

    if (need_gamma && !is_linear) {
        pass_linearize(sc, src.gamma);
        is_linear= true;
    }

    if (src.light != dst.light)
        pass_ootf(sc, src.light, mp_trc_nom_peak(src.gamma));

    // Rescale the signal to compensate for differences in the encoding range
    // and reference white level. This is necessary because of how mpv encodes
    // brightness in textures.
    if (src_range != dst_range) {
        GLSLF("// rescale value range;\n");
        GLSLF("color.rgb *= vec3(%f);\n", src_range / dst_range);
    }

    // Tone map to prevent clipping when the source signal peak exceeds the
    // encodable range
    if (src.sig_peak > dst_range) {
        pass_tone_map(sc, src.sig_peak / dst_range, algo, tone_mapping_param,
                      tone_mapping_desat);
    }

    // Adapt to the right colorspace if necessary
    if (src.primaries != dst.primaries) {
        struct mp_csp_primaries csp_src = mp_get_csp_primaries(src.primaries),
                                csp_dst = mp_get_csp_primaries(dst.primaries);
        float m[3][3] = {{0}};
        mp_get_cms_matrix(csp_src, csp_dst, MP_INTENT_RELATIVE_COLORIMETRIC, m);
        gl_sc_uniform_mat3(sc, "cms_matrix", true, &m[0][0]);
        GLSL(color.rgb = cms_matrix * color.rgb;)
    }

    if (src.light != dst.light)
        pass_inverse_ootf(sc, dst.light, mp_trc_nom_peak(dst.gamma));

    if (is_linear)
        pass_delinearize(sc, dst.gamma);
}

// Wide usage friendly PRNG, shamelessly stolen from a GLSL tricks forum post.
// Obtain random numbers by calling rand(h), followed by h = permute(h) to
// update the state. Assumes the texture was hooked.
static void prng_init(struct gl_shader_cache *sc, AVLFG *lfg)
{
    GLSLH(float mod289(float x)  { return x - floor(x / 289.0) * 289.0; })
    GLSLH(float permute(float x) { return mod289((34.0*x + 1.0) * x); })
    GLSLH(float rand(float x)    { return fract(x / 41.0); })

    // Initialize the PRNG by hashing the position + a random uniform
    GLSL(vec3 _m = vec3(HOOKED_pos, random) + vec3(1.0);)
    GLSL(float h = permute(permute(permute(_m.x)+_m.y)+_m.z);)
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
    .threshold = 64.0,
    .range = 16.0,
    .grain = 48.0,
};

#define OPT_BASE_STRUCT struct deband_opts
const struct m_sub_options deband_conf = {
    .opts = (const m_option_t[]) {
        OPT_INTRANGE("iterations", iterations, 0, 1, 16),
        OPT_FLOATRANGE("threshold", threshold, 0, 0.0, 4096.0),
        OPT_FLOATRANGE("range", range, 0, 1.0, 64.0),
        OPT_FLOATRANGE("grain", grain, 0, 0.0, 4096.0),
        {0}
    },
    .size = sizeof(struct deband_opts),
    .defaults = &deband_opts_def,
};

// Stochastically sample a debanded result from a hooked texture.
void pass_sample_deband(struct gl_shader_cache *sc, struct deband_opts *opts,
                        AVLFG *lfg)
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
        GLSLH(return (ref[0] + ref[1] + ref[2] + ref[3])/4.0;)
    GLSLHF("}\n");

    // Sample the source pixel
    GLSL(color = HOOKED_tex(HOOKED_pos);)
    GLSLF("vec4 avg, diff;\n");
    for (int i = 1; i <= opts->iterations; i++) {
        // Sample the average pixel and use it instead of the original if
        // the difference is below the given threshold
        GLSLF("avg = average(%f, h);\n", i * opts->range);
        GLSL(diff = abs(color - avg);)
        GLSLF("color = mix(avg, color, greaterThan(diff, vec4(%f)));\n",
              opts->threshold / (i * 16384.0));
    }

    // Add some random noise to smooth out residual differences
    GLSL(vec3 noise;)
    GLSL(noise.x = rand(h); h = permute(h);)
    GLSL(noise.y = rand(h); h = permute(h);)
    GLSL(noise.z = rand(h); h = permute(h);)
    GLSLF("color.xyz += %f * (noise - vec3(0.5));\n", opts->grain/8192.0);
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
