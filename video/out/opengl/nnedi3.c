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
 *
 * The shader portions may have been derived from existing LGPLv3 shaders
 * (see below), possibly making this file effectively LGPLv3.
 */

#include "nnedi3.h"

#if HAVE_NNEDI

#include <assert.h>
#include <stdint.h>
#include <float.h>

#include <libavutil/bswap.h>

#include "video.h"

/*
 * NNEDI3, an intra-field deinterlacer
 *
 * The original filter was authored by Kevin Stone (aka. tritical) and is
 * licensed under GPL2 terms:
 *     http://bengal.missouri.edu/~kes25c/
 *
 * A LGPLv3 licensed OpenCL kernel was created by SEt:
 *     http://forum.doom9.org/showthread.php?t=169766
 *
 * A HLSL port further modified by madshi, Shiandow and Zach Saw could be
 * found at (also LGPLv3 licensed):
 *     https://github.com/zachsaw/MPDN_Extensions
 *
 */

#define GLSL(x) gl_sc_add(sc, #x "\n");
#define GLSLF(...) gl_sc_addf(sc, __VA_ARGS__)
#define GLSLH(x) gl_sc_hadd(sc, #x "\n");
#define GLSLHF(...) gl_sc_haddf(sc, __VA_ARGS__)

const struct nnedi3_opts nnedi3_opts_def = {
    .neurons = 1,
    .window = 0,
    .upload = NNEDI3_UPLOAD_UBO,
};

#define OPT_BASE_STRUCT struct nnedi3_opts
const struct m_sub_options nnedi3_conf = {
    .opts = (const m_option_t[]) {
        OPT_CHOICE("neurons", neurons, 0,
                   ({"16", 0},
                    {"32", 1},
                    {"64", 2},
                    {"128", 3})),
        OPT_CHOICE("window", window, 0,
                   ({"8x4", 0},
                    {"8x6", 1})),
        OPT_CHOICE("upload", upload, 0,
                   ({"ubo", NNEDI3_UPLOAD_UBO},
                    {"shader", NNEDI3_UPLOAD_SHADER})),
        {0}
    },
    .size = sizeof(struct nnedi3_opts),
    .defaults = &nnedi3_opts_def,
};

const static char nnedi3_weights[40320 * 4 + 1] =
#include "video/out/opengl/nnedi3_weights.inc"
;

const int nnedi3_weight_offsets[9] =
    {0, 1088, 3264, 7616, 16320, 17920, 21120, 27520, 40320};

const int nnedi3_neurons[4] = {16, 32, 64, 128};
const int nnedi3_window_width[2] = {8, 8};
const int nnedi3_window_height[2] = {4, 6};

const float* get_nnedi3_weights(const struct nnedi3_opts *conf, int *size)
{
    int idx = conf->window * 4 + conf->neurons;
    const int offset = nnedi3_weight_offsets[idx];
    *size = (nnedi3_weight_offsets[idx + 1] - offset) * 4;
    return (const float*)(nnedi3_weights + offset * 4);
}

void pass_nnedi3(GL *gl, struct gl_shader_cache *sc, int planes, int tex_num,
                 int step, float tex_mul, const struct nnedi3_opts *conf,
                 struct gl_transform *transform)
{
    assert(0 <= step && step < 2);

    if (!conf)
        conf = &nnedi3_opts_def;

    const int neurons = nnedi3_neurons[conf->neurons];
    const int width = nnedi3_window_width[conf->window];
    const int height = nnedi3_window_height[conf->window];

    const int offset = nnedi3_weight_offsets[conf->window * 4 + conf->neurons];
    const uint32_t *weights = (const int*)(nnedi3_weights + offset * 4);

    GLSLF("// nnedi3 (tex %d, step %d, neurons %d, window %dx%d, mode %d)\n",
          tex_num, step + 1, neurons, width, height, conf->upload);

    // This is required since each row will be encoded into vec4s
    assert(width % 4 == 0);
    const int sample_count = width * height / 4;

    if (conf->upload == NNEDI3_UPLOAD_UBO) {
        char buf[32];
        snprintf(buf, sizeof(buf), "vec4 weights[%d];",
                 neurons * (sample_count * 2 + 1));
        gl_sc_uniform_buffer(sc, "NNEDI3_WEIGHTS", buf, 0);
        if (!gl->es && gl->glsl_version < 140)
            gl_sc_enable_extension(sc, "GL_ARB_uniform_buffer_object");
    } else if (conf->upload == NNEDI3_UPLOAD_SHADER) {
        // Somehow necessary for hard coding approach.
        GLSLH(#pragma optionNV(fastprecision on))
    }

    GLSLHF("float nnedi3(sampler2D tex, vec2 pos, vec2 tex_size, int plane, float tex_mul) {\n");

    if (step == 0) {
        *transform = (struct gl_transform){{{1.0,0.0}, {0.0,2.0}}, {0.0,-0.5}};

        GLSLH(if (fract(pos.y * tex_size.y) < 0.5)
                  return texture(tex, pos + vec2(0, 0.25) / tex_size)[plane] * tex_mul;)
        GLSLHF("#define GET(i, j) "
               "(texture(tex, pos+vec2((i)-(%f),(j)-(%f)+0.25)/tex_size)[plane]*tex_mul)\n",
               width / 2.0 - 1, (height - 1) / 2.0);
    } else {
        *transform = (struct gl_transform){{{2.0,0.0}, {0.0,1.0}}, {-0.5,0.0}};

        GLSLH(if (fract(pos.x * tex_size.x) < 0.5)
                  return texture(tex, pos + vec2(0.25, 0) / tex_size)[plane] * tex_mul;)
        GLSLHF("#define GET(i, j) "
               "(texture(tex, pos+vec2((j)-(%f)+0.25,(i)-(%f))/tex_size)[plane]*tex_mul)\n",
               (height - 1) / 2.0, width / 2.0 - 1);
    }

    GLSLHF("vec4 samples[%d];\n", sample_count);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x += 4) {
            GLSLHF("samples[%d] = vec4(GET(%d.0, %d.0), GET(%d.0, %d.0),"
                                      "GET(%d.0, %d.0), GET(%d.0, %d.0));\n",
                   (y * width + x) / 4, x, y, x+1, y, x+2, y, x+3, y);
        }

    GLSLHF("float sum = 0.0, sumsq = 0.0;"
           "for (int i = 0; i < %d; i++) {"
               "sum += dot(samples[i], vec4(1.0));"
               "sumsq += dot(samples[i], samples[i]);"
           "}\n", sample_count);

    GLSLHF("float mstd0 = sum / %d.0;\n"
           "float mstd1 = sumsq / %d.0 - mstd0 * mstd0;\n"
           "float mstd2 = mix(0.0, inversesqrt(mstd1), mstd1 >= %.12e);\n"
           "mstd1 *= mstd2;\n",
           width * height, width * height, FLT_EPSILON);

    GLSLHF("float vsum = 0.0, wsum = 0.0, sum1, sum2;\n");

    if (conf->upload == NNEDI3_UPLOAD_SHADER) {
        GLSLH(#define T(x) intBitsToFloat(x))
        GLSLH(#define W(i,w0,w1,w2,w3) dot(samples[i],vec4(T(w0),T(w1),T(w2),T(w3))))

        GLSLHF("#define WS(w0,w1) "
               "sum1 = exp(sum1 * mstd2 + T(w0));"
               "sum2 = sum2 * mstd2 + T(w1);"
               "wsum += sum1;"
               "vsum += sum1*(sum2/(1.0+abs(sum2)));\n");

        for (int n = 0; n < neurons; n++) {
            const uint32_t *weights_ptr = weights + (sample_count * 2 + 1) * 4 * n;
            for (int s = 0; s < 2; s++) {
                GLSLHF("sum%d", s + 1);
                for (int i = 0; i < sample_count; i++) {
                    GLSLHF("%cW(%d,%d,%d,%d,%d)", i == 0 ? '=' : '+', i,
                           (int)av_le2ne32(weights_ptr[0]),
                           (int)av_le2ne32(weights_ptr[1]),
                           (int)av_le2ne32(weights_ptr[2]),
                           (int)av_le2ne32(weights_ptr[3]));
                    weights_ptr += 4;
                }
                GLSLHF(";");
            }
            GLSLHF("WS(%d,%d);\n", (int)av_le2ne32(weights_ptr[0]),
                                   (int)av_le2ne32(weights_ptr[1]));
        }
    } else if (conf->upload == NNEDI3_UPLOAD_UBO) {
        GLSLH(int idx = 0;)

        GLSLHF("for (int n = 0; n < %d; n++) {\n", neurons);

        for (int s = 0; s < 2; s++) {
            GLSLHF("sum%d = 0.0;\n"
                   "for (int i = 0; i < %d; i++) {"
                       "sum%d += dot(samples[i], weights[idx++]);"
                   "}\n",
                   s + 1, sample_count, s + 1);
        }

        GLSLH(sum1 = exp(sum1 * mstd2 + weights[idx][0]);
              sum2 = sum2 * mstd2 + weights[idx++][1];
              wsum += sum1;
              vsum += sum1*(sum2/(1.0+abs(sum2)));)

        GLSLHF("}\n");
    }

    GLSLH(return clamp(mstd0 + 5.0 * vsum / wsum * mstd1, 0.0, 1.0);)

    GLSLHF("}\n"); // nnedi3

    GLSL(vec4 color = vec4(1.0);)

    for (int i = 0; i < planes; i++) {
        GLSLF("color[%d] = nnedi3(texture%d, texcoord%d, texture_size%d, %d, %f);\n",
              i, tex_num, tex_num, tex_num, i, tex_mul);
    }
}

#else

const struct m_sub_options nnedi3_conf = {0};


const float* get_nnedi3_weights(const struct nnedi3_opts *conf, int *size)
{
    return NULL;
}

void pass_nnedi3(GL *gl, struct gl_shader_cache *sc, int planes, int tex_num,
                 int step, float tex_mul, const struct nnedi3_opts *conf,
                 struct gl_transform *transform)
{
}

#endif
