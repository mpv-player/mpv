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
    .edge_strength = 1.0f,
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

void pass_superxbr(struct gl_shader_cache *sc, int planes, int tex_num,
                   int step, float tex_mul, const struct superxbr_opts *conf,
                   struct gl_transform *transform)
{
    assert(0 <= step && step < 2);
    GLSLF("// superxbr (tex %d, step %d)\n", tex_num, step + 1);

    if (!conf)
        conf = &superxbr_opts_def;

    if (step == 0) {
        *transform = (struct gl_transform){{{2.0,0.0}, {0.0,2.0}}, {-0.5,-0.5}};

        GLSLH(#define wp1  2.0)
        GLSLH(#define wp2  1.0)
        GLSLH(#define wp3 -1.0)
        GLSLH(#define wp4  4.0)
        GLSLH(#define wp5 -1.0)
        GLSLH(#define wp6  1.0)

        GLSLHF("#define weight1 (%f*1.29633/10.0)\n", conf->sharpness);
        GLSLHF("#define weight2 (%f*1.75068/10.0/2.0)\n", conf->sharpness);

        GLSLH(#define Get(x, y) (texture(tex, pos + (vec2(x, y) - vec2(0.25, 0.25)) / tex_size)[plane] * tex_mul))
    } else {
        *transform = (struct gl_transform){{{1.0,0.0}, {0.0,1.0}}, {0.0,0.0}};

        GLSLH(#define wp1  2.0)
        GLSLH(#define wp2  0.0)
        GLSLH(#define wp3  0.0)
        GLSLH(#define wp4  0.0)
        GLSLH(#define wp5  0.0)
        GLSLH(#define wp6  0.0)

        GLSLHF("#define weight1 (%f*1.75068/10.0)\n", conf->sharpness);
        GLSLHF("#define weight2 (%f*1.29633/10.0/2.0)\n", conf->sharpness);

        GLSLH(#define Get(x, y) (texture(tex, pos + (vec2((x) + (y) - 1, (y) - (x))) / tex_size)[plane] * tex_mul))
    }
    GLSLH(float df(float A, float B)
          {
              return abs(A-B);
          })

    GLSLH(float d_wd(float b0, float b1, float c0, float c1, float c2,
                     float d0, float d1, float d2, float d3, float e1,
                     float e2, float e3, float f2, float f3)
          {
              return (wp1*(df(c1,c2) + df(c1,c0) + df(e2,e1) + df(e2,e3)) +
                      wp2*(df(d2,d3) + df(d0,d1)) +
                      wp3*(df(d1,d3) + df(d0,d2)) +
                      wp4*df(d1,d2) +
                      wp5*(df(c0,c2) + df(e1,e3)) +
                      wp6*(df(b0,b1) + df(f2,f3)));
          })

    GLSLH(float hv_wd(float i1, float i2, float i3, float i4,
                      float e1, float e2, float e3, float e4)
          {
              return (wp4*(df(i1,i2)+df(i3,i4)) +
                      wp1*(df(i1,e1)+df(i2,e2)+df(i3,e3)+df(i4,e4)) +
                      wp3*(df(i1,e2)+df(i3,e4)+df(e1,i2)+df(e3,i4)));
          })

    GLSLHF("float superxbr(sampler2D tex, vec2 pos, vec2 tex_size, int plane, float tex_mul) {\n");

    if (step == 0) {
        GLSLH(vec2 dir = fract(pos * tex_size) - 0.5;)

        // Optimization: Discard (skip drawing) unused pixels, except those
        // at the edge.
        GLSLH(vec2 dist = tex_size * min(pos, vec2(1.0) - pos);)
        GLSLH(if (dir.x * dir.y < 0.0 && dist.x > 1.0 && dist.y > 1.0)
                  return 0.0;)

        GLSLH(if (dir.x < 0.0 || dir.y < 0.0 || dist.x < 1.0 || dist.y < 1.0)
                  return texture(tex, pos - dir / tex_size)[plane] * tex_mul;)
    } else {
        GLSLH(vec2 dir = fract(pos * tex_size / 2.0) - 0.5;)
        GLSLH(if (dir.x * dir.y > 0.0)
                  return texture(tex, pos)[plane] * tex_mul;)
    }

    GLSLH(float P0 = Get(-1,-1);
          float P1 = Get( 2,-1);
          float P2 = Get(-1, 2);
          float P3 = Get( 2, 2);

          float  B = Get( 0,-1);
          float  C = Get( 1,-1);
          float  D = Get(-1, 0);
          float  E = Get( 0, 0);
          float  F = Get( 1, 0);
          float  G = Get(-1, 1);
          float  H = Get( 0, 1);
          float  I = Get( 1, 1);

          float F4 = Get(2, 0);
          float I4 = Get(2, 1);
          float H5 = Get(0, 2);
          float I5 = Get(1, 2);)

/*
                                  P1
         |P0|B |C |P1|         C     F4          |a0|b1|c2|d3|
         |D |E |F |F4|      B     F     I4       |b0|c1|d2|e3|   |e1|i1|i2|e2|
         |G |H |I |I4|   P0    E  A  I     P3    |c0|d1|e2|f3|   |e3|i3|i4|e4|
         |P2|H5|I5|P3|      D     H     I5       |d0|e1|f2|g3|
                               G     H5
                                  P2
*/

    /* Calc edgeness in diagonal directions. */
    GLSLH(float d_edge = (d_wd( D, B, G, E, C, P2, H, F, P1, H5, I, F4, I5, I4 ) -
                          d_wd( C, F4, B, F, I4, P0, E, I, P3, D, H, I5, G, H5 ));)

    /* Calc edgeness in horizontal/vertical directions. */
    GLSLH(float hv_edge = (hv_wd(F, I, E, H, C, I5, B, H5) -
                           hv_wd(E, F, H, I, D, F4, G, I4));)

    /* Filter weights. Two taps only. */
    GLSLH(vec4 w1 = vec4(-weight1, weight1+0.5, weight1+0.5, -weight1);
          vec4 w2 = vec4(-weight2, weight2+0.25, weight2+0.25, -weight2);)

    /* Filtering and normalization in four direction generating four colors. */
    GLSLH(float c1 = dot(vec4(P2, H, F, P1), w1);
          float c2 = dot(vec4(P0, E, I, P3), w1);
          float c3 = dot(vec4( D+G, E+H, F+I, F4+I4), w2);
          float c4 = dot(vec4( C+B, F+E, I+H, I5+H5), w2);)

    GLSLHF("float limits = %f + 0.000001;\n", conf->edge_strength);
    GLSLH(float edge_strength = smoothstep(0.0, limits, abs(d_edge));)

    /* Smoothly blends the two strongest directions(one in diagonal and the
     * other in vert/horiz direction). */
    GLSLHF("float color =  mix(mix(c1, c2, step(0.0, d_edge)),"
                              "mix(c3, c4, step(0.0, hv_edge)), 1.0 - %f);\n",
           conf->edge_strength);
    /* Anti-ringing code. */
    GLSLH(float min_sample = min(min(E, F), min(H, I));
          float max_sample = max(max(E, F), max(H, I));
          float aux = color;
          color = clamp(color, min_sample, max_sample);)
    GLSLHF("color = mix(aux, color, 1.0-2.0*abs(%f-0.5));\n", conf->edge_strength);

    GLSLH(return color;)

    GLSLHF("}");  // superxbr()

    GLSL(vec4 color = vec4(1.0);)

    for (int i = 0; i < planes; i++) {
        GLSLF("color[%d] = superxbr(texture%d, texcoord%d, texture_size%d, %d, %f);\n",
              i, tex_num, tex_num, tex_num, i, tex_mul);
    }
}
