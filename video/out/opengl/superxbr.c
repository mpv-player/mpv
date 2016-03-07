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

#include "superxbr.h"

#include <assert.h>

#define GLSL(x) gl_sc_add(sc, #x "\n");
#define GLSLF(...) gl_sc_addf(sc, __VA_ARGS__)
#define GLSLH(x) gl_sc_hadd(sc, #x "\n");
#define GLSLHF(...) gl_sc_haddf(sc, __VA_ARGS__)

struct superxbr_opts {
    float sharpness;
    float edge_strength;
};

const struct superxbr_opts superxbr_opts_def = {
    .sharpness = 1.0f,
    .edge_strength = 0.6f,
};

#define OPT_BASE_STRUCT struct superxbr_opts
const struct m_sub_options superxbr_conf = {
    .opts = (const m_option_t[]) {
        OPT_FLOATRANGE("sharpness", sharpness, 0, 0.0, 2.0),
        OPT_FLOATRANGE("edge-strength", edge_strength, 0, 0.0, 1.0),
        {0}
    },
    .size = sizeof(struct superxbr_opts),
    .defaults = &superxbr_opts_def,
};

/*

    *******  Super XBR Shader  *******

    Copyright (c) 2015 Hyllian - sergiogdb@gmail.com

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

*/

struct step_params {
    const float dstr, ostr; // sharpness strength modifiers
    const int d1[3][3]; // 1-distance diagonal mask
    const int d2[2][2]; // 2-distance diagonal mask
    const int o1[3]; // 1-distance orthogonal mask
    const int o2[3]; // 2-distance orthogonal mask
};

const struct step_params params[3] = {
    {   .dstr = 0.129633,
        .ostr = 0.175068,
        .d1 = {{0, 1, 0},
               {1, 2, 1},
               {0, 1, 0}},
        .d2 = {{-1,  0},
               { 0, -1}},

        .o1 = {1, 2, 1},
        .o2 = { 0,  0},
    }, {
        .dstr = 0.175068,
        .ostr = 0.129633,
        .d1 = {{0, 1, 0},
               {1, 4, 1},
               {0, 1, 0}},
        .d2 = {{ 0,  0},
               { 0,  0}},

        .o1 = {1, 4, 1},
        .o2 = { 0,  0},
    }
};

// Compute a single step of the superxbr process, assuming the input can be
// sampled using i(x,y). Dumps its output into 'res'
static void superxbr_step_h(struct gl_shader_cache *sc,
                            const struct superxbr_opts *conf,
                            const struct step_params *mask)
{
    GLSLHF("{ // step\n");

    // Convolute along the diagonal and orthogonal lines
    GLSLH(vec4 d1 = vec4( i(0,0), i(1,1), i(2,2), i(3,3) );)
    GLSLH(vec4 d2 = vec4( i(0,3), i(1,2), i(2,1), i(3,0) );)
    GLSLH(vec4 h1 = vec4( i(0,1), i(1,1), i(2,1), i(3,1) );)
    GLSLH(vec4 h2 = vec4( i(0,2), i(1,2), i(2,2), i(3,2) );)
    GLSLH(vec4 v1 = vec4( i(1,0), i(1,1), i(1,2), i(1,3) );)
    GLSLH(vec4 v2 = vec4( i(2,0), i(2,1), i(2,2), i(2,3) );)

    GLSLHF("float dw = %f;\n", conf->sharpness * mask->dstr);
    GLSLHF("float ow = %f;\n", conf->sharpness * mask->ostr);
    GLSLH(vec4 dk = vec4(-dw, dw+0.5, dw+0.5, -dw);) // diagonal kernel
    GLSLH(vec4 ok = vec4(-ow, ow+0.5, ow+0.5, -ow);) // ortho kernel

    // Convoluted results
    GLSLH(float d1c = dot(d1, dk);)
    GLSLH(float d2c = dot(d2, dk);)
    GLSLH(float vc = dot(v1+v2, ok)/2.0;)
    GLSLH(float hc = dot(h1+h2, ok)/2.0;)

    // Compute diagonal edge strength using diagonal mask
    GLSLH(float d_edge = 0;)
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            if (mask->d1[x][y]) {
                // 1-distance diagonal neighbours
                GLSLHF("d_edge += %d * abs(i(%d,%d) - i(%d,%d));\n",
                       mask->d1[x][y], x+1, y, x, y+1);
                GLSLHF("d_edge -= %d * abs(i(%d,%d) - i(%d,%d));\n",
                       mask->d1[x][y], 3-y, x+1, 3-(y+1), x); // rotated
            }
            if (x < 2 && y < 2 && mask->d2[x][y]) {
                // 2-distance diagonal neighbours
                GLSLHF("d_edge += %d * abs(i(%d,%d) - i(%d,%d));\n",
                       mask->d2[x][y], x+2, y, x, y+2);
                GLSLHF("d_edge -= %d * abs(i(%d,%d) - i(%d,%d));\n",
                       mask->d2[x][y], 3-y, x+2, 3-(y+2), x); // rotated
            }
        }
    }

    // Compute orthogonal edge strength using orthogonal mask
    GLSLH(float o_edge = 0;)
    for (int x = 1; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            if (mask->o1[y]) {
                // 1-distance neighbours
                GLSLHF("o_edge += %d * abs(i(%d,%d) - i(%d,%d));\n",
                       mask->o1[y], x, y, x, y+1); // vertical
                GLSLHF("o_edge -= %d * abs(i(%d,%d) - i(%d,%d));\n",
                       mask->o1[y], y, x, y+1, x); // horizontal
            }
            if (y < 2 && mask->o2[y]) {
                // 2-distance neighbours
                GLSLHF("o_edge += %d * abs(i(%d,%d) - i(%d,%d));\n",
                       mask->o2[y], x, y, x, y+2); // vertical
                GLSLHF("o_edge -= %d * abs(i(%d,%d) - i(%d,%d));\n",
                       mask->o2[x], y, x, y+2, x); // horizontal
            }
        }
    }

    // Pick the two best directions and mix them together
    GLSLHF("float str = smoothstep(0.0, %f + 1e-6, abs(tex_mul*d_edge));\n",
           conf->edge_strength);
    GLSLH(res = mix(mix(d2c, d1c, step(0.0, d_edge)), \
                    mix(hc,   vc, step(0.0, o_edge)), 1.0 - str);)

    // Anti-ringing using center square
    GLSLH(float lo = min(min( i(1,1), i(2,1) ), min( i(1,2), i(2,2) ));)
    GLSLH(float hi = max(max( i(1,1), i(2,1) ), max( i(1,2), i(2,2) ));)
    GLSLH(res = clamp(res, lo, hi);)

    GLSLHF("} // step\n");
}

void pass_superxbr(struct gl_shader_cache *sc, int id, int step, float tex_mul,
                   const struct superxbr_opts *conf,
                   struct gl_transform *transform)
{
    if (!conf)
        conf = &superxbr_opts_def;

    assert(0 <= step && step < 2);
    GLSLF("// superxbr (step %d)\n", step);
    GLSLHF("#define tex texture%d\n", id);
    GLSLHF("#define tex_size texture_size%d\n", id);
    GLSLHF("#define tex_mul %f\n", tex_mul);
    GLSLHF("#define pt pixel_size%d\n", id);

    // We use a sub-function in the header so we can return early
    GLSLHF("float superxbr(vec2 pos) {\n");
    GLSLH(float i[4*4];)
    GLSLH(float res;)
    GLSLH(#define i(x,y) i[(x)*4+(y)])

    if (step == 0) {
        *transform = (struct gl_transform){{{2.0,0.0}, {0.0,2.0}}, {-0.5,-0.5}};
        GLSLH(vec2 dir = fract(pos * tex_size) - 0.5;)

        // Optimization: Discard (skip drawing) unused pixels, except those
        // at the edge.
        GLSLH(vec2 dist = tex_size * min(pos, vec2(1.0) - pos);)
        GLSLH(if (dir.x * dir.y < 0.0 && dist.x > 1.0 && dist.y > 1.0)
                  return 0.0;)

        GLSLH(if (dir.x < 0.0 || dir.y < 0.0 || dist.x < 1.0 || dist.y < 1.0)
                  return texture(tex, pos - pt * dir).x;)

        // Load the input samples
        GLSLH(for (int x = 0; x < 4; x++))
        GLSLH(for (int y = 0; y < 4; y++))
        GLSLH(i(x,y) = texture(tex, pos + pt * vec2(x-1.25, y-1.25)).x;)
    } else {
        *transform = (struct gl_transform){{{1.0,0.0}, {0.0,1.0}}, {0.0,0.0}};

        GLSLH(vec2 dir = fract(pos * tex_size / 2.0) - 0.5;)
        GLSLH(if (dir.x * dir.y > 0.0)
                  return texture(tex, pos).x;)

        GLSLH(for (int x = 0; x < 4; x++))
        GLSLH(for (int y = 0; y < 4; y++))
        GLSLH(i(x,y) = texture(tex, pos + pt * vec2(x+y-3, y-x)).x;)
    }

    superxbr_step_h(sc, conf, &params[step]);
    GLSLH(return res;)
    GLSLHF("}\n");

    GLSLF("color.x = tex_mul * superxbr(texcoord%d);\n", id);
}
