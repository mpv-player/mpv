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

#!section frag_video
uniform VIDEO_SAMPLER texture0;
uniform VIDEO_SAMPLER texture1;
uniform VIDEO_SAMPLER texture2;
uniform VIDEO_SAMPLER texture3;
uniform vec2 textures_size[4];

uniform float filter_param1;

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
