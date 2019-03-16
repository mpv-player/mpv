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

#include <stdlib.h>

#include "error_diffusion.h"

#include "common/common.h"

#define GLSL(...) gl_sc_addf(sc, __VA_ARGS__)
#define GLSLH(...) gl_sc_haddf(sc, __VA_ARGS__)

// After a (y, x) -> (y, x + y * shift) mapping, find the right most column that
// will be affected by the current column.
static int compute_rightmost_shifted_column(const struct error_diffusion_kernel *k)
{
    int ret = 0;
    for (int y = 0; y <= EF_MAX_DELTA_Y; y++) {
        for (int x = EF_MIN_DELTA_X; x <= EF_MAX_DELTA_X; x++) {
            if (k->pattern[y][x - EF_MIN_DELTA_X] != 0) {
                int shifted_x = x + y * k->shift;

                // The shift mapping guarantees current column (or left of it)
                // won't be affected by error diffusion.
                assert(shifted_x > 0);

                ret = MPMAX(ret, shifted_x);
            }
        }
    }
    return ret;
}

const struct error_diffusion_kernel *mp_find_error_diffusion_kernel(const char *name)
{
    if (!name)
        return NULL;
    for (const struct error_diffusion_kernel *k = mp_error_diffusion_kernels;
         k->name;
         k++) {
        if (strcmp(k->name, name) == 0)
            return k;
    }
    return NULL;
}

int mp_ef_compute_shared_memory_size(const struct error_diffusion_kernel *k,
                                     int height)
{
    // We add EF_MAX_DELTA_Y empty lines on the bottom to handle errors
    // propagated out from bottom side.
    int rows = height + EF_MAX_DELTA_Y;
    int shifted_columns = compute_rightmost_shifted_column(k) + 1;

    // The shared memory is an array of size rows*shifted_columns. Each element
    // is three int, for each RGB component.
    return rows * shifted_columns * 3 * 4;
}

void pass_error_diffusion(struct gl_shader_cache *sc,
                          const struct error_diffusion_kernel *k,
                          int tex, int width, int height, int depth, int block_size)
{
    assert(block_size <= height);

    // The parallel error diffusion works by applying the shift mapping first.
    // Taking the Floyd and Steinberg algorithm for example. After applying
    // the (y, x) -> (y, x + y * shift) mapping (with shift=2), all errors are
    // propagated into the next few columns, which makes parallel processing on
    // the same column possible.
    //
    //           X    7/16                X    7/16
    //    3/16  5/16  1/16   ==>    0     0    3/16  5/16  1/16

    // Figuring out the size of rectangle containing all shifted pixels.
    // The rectangle height is not changed.
    int shifted_width = width + (height - 1) * k->shift;

    // We process all pixels from the shifted rectangles column by column, with
    // a single global work group of size |block_size|.
    // Figuring out how many block are required to process all pixels. We need
    // this explicitly to make the number of barrier() calls match.
    int blocks = (height * shifted_width + block_size - 1) / block_size;

    // If we figure out how many of the next columns will be affected while the
    // current columns is being processed. We can store errors of only a few
    // columns in the shared memory. Using a ring buffer will further save the
    // cost while iterating to next column.
    int ring_buffer_rows = height + EF_MAX_DELTA_Y;
    int ring_buffer_columns = compute_rightmost_shifted_column(k) + 1;
    int ring_buffer_size = ring_buffer_rows * ring_buffer_columns;

    const char *rgb = "rgb";

    // Defines the ring buffer in shared memory.
    for (int comp = 0; comp < 3; comp++)
        GLSLH("shared int err_%c[%d];\n", rgb[comp], ring_buffer_size);

    // Initialize the ring buffer.
    GLSL("for (int i = int(gl_LocalInvocationIndex); i < %d; i += %d) {\n",
         ring_buffer_size, block_size);
    for (int comp = 0; comp < 3; comp++)
        GLSL("err_%c[i] = 0;\n", rgb[comp]);
    GLSL("}\n");

    GLSL("for (int block_id = 0; block_id < %d; ++block_id) {\n", blocks);

    // Add barrier here to have previous block all processed before starting
    // the processing of the next.
    GLSL("groupMemoryBarrier();\n");
    GLSL("barrier();\n");

    // Compute the coordinate of the pixel we are currently processing, both
    // before and after the shift mapping.
    GLSL("int id = int(gl_LocalInvocationIndex) + block_id * %d;\n", block_size);
    GLSL("int y = id %% %d, x_shifted = id / %d;\n", height, height);
    GLSL("int x = x_shifted - y * %d;\n", k->shift);

    // Proceed only if we are processing a valid pixel.
    GLSL("if (0 <= x && x < %d) {\n", width);

    // The index that the current pixel have on the ring buffer.
    GLSL("int idx = (x_shifted * %d + y) %% %d;\n", ring_buffer_rows, ring_buffer_size);

    // Fetch the current pixel.
    GLSL("vec3 pix = texelFetch(texture%d, ivec2(x, y), 0).rgb;\n", tex);

    // The dithering will quantize pixel value into multiples of 1/dither_quant.
    int dither_quant = (1 << depth) - 1;
    // The absolute value of the errors to propagate is less than 1/dither_quant,
    // multiply by dither_quant24 to have them processed with int in 24 bit
    // precision.
    double dither_quant24 = (double)(1 << 24) * dither_quant;

    // Adding the error previously propagated into current pixel, and clear it
    // in the buffer.
    GLSL("pix += vec3(err_r[idx], err_g[idx], err_b[idx]) / %f;\n", dither_quant24);
    for (int comp = 0; comp < 3; comp++)
        GLSL("err_%c[idx] = 0;\n", rgb[comp]);

    // Dithering to depth.
    GLSL("vec3 dithered = floor(pix * %d.0 + 0.5) / %d.0;\n", dither_quant, dither_quant);
    GLSL("ivec3 err = ivec3((pix - dithered) * %f + 0.5);\n", dither_quant24);

    // Write the dithered pixel.
    GLSL("imageStore(out_image, ivec2(x, y), vec4(dithered, 0.0));\n");

    GLSL("int nidx;\n");
    for (int y = 0; y <= EF_MAX_DELTA_Y; y++) {
        for (int x = EF_MIN_DELTA_X; x <= EF_MAX_DELTA_X; x++) {
            if (k->pattern[y][x - EF_MIN_DELTA_X] != 0) {
                int shifted_x = x + y * k->shift;

                // Unlike the right border, errors propagated out from left
                // border will remain in the ring buffer. This will produce
                // visible artifacts near the left border, especially for
                // shift=3 kernels.
                bool left_border_check = x < 0;

                if (left_border_check)
                    GLSL("if (x >= %d) {\n", -x);

                // Calculate the new position in the ring buffer to propagate
                // the error into.
                int ring_buffer_delta = shifted_x * ring_buffer_rows + y;
                GLSL("nidx = (idx + %d) %% %d;\n", ring_buffer_delta, ring_buffer_size);

                // Propagate the error with atomic operation.
                for (int comp = 0; comp < 3; comp++) {
                    GLSL("atomicAdd(err_%c[nidx], err.%c * %d / %d);\n",
                         rgb[comp], rgb[comp],
                         k->pattern[y][x - EF_MIN_DELTA_X],
                         k->divisor);
                }

                if (left_border_check)
                    GLSL("}\n");
            }
        }
    }

    GLSL("}\n"); // if (0 <= x && x < width)

    GLSL("}\n"); // block_id
}

// Different kernels for error diffusion.
// Patterns are from http://www.efg2.com/Lab/Library/ImageProcessing/DHALF.TXT
const struct error_diffusion_kernel mp_error_diffusion_kernels[] = {
    {
        .name = "simple",
        .shift = 1,
        .pattern = {{0, 0, 0, 1, 0},
                    {0, 0, 1, 0, 0},
                    {0, 0, 0, 0, 0}},
        .divisor = 2
    },
    {
        // The "false" Floyd-Steinberg kernel
        .name = "false-fs",
        .shift = 1,
        .pattern = {{0, 0, 0, 3, 0},
                    {0, 0, 3, 2, 0},
                    {0, 0, 0, 0, 0}},
        .divisor = 8
    },
    {
        .name = "sierra-lite",
        .shift = 2,
        .pattern = {{0, 0, 0, 2, 0},
                    {0, 1, 1, 0, 0},
                    {0, 0, 0, 0, 0}},
        .divisor = 4
    },
    {
        .name = "floyd-steinberg",
        .shift = 2,
        .pattern = {{0, 0, 0, 7, 0},
                    {0, 3, 5, 1, 0},
                    {0, 0, 0, 0, 0}},
        .divisor = 16
    },
    {
        .name = "atkinson",
        .shift = 2,
        .pattern = {{0, 0, 0, 1, 1},
                    {0, 1, 1, 1, 0},
                    {0, 0, 1, 0, 0}},
        .divisor = 8
    },
    // All kernels below have shift value of 3, and probably are too heavy for
    // low end GPU.
    {
        .name = "jarvis-judice-ninke",
        .shift = 3,
        .pattern = {{0, 0, 0, 7, 5},
                    {3, 5, 7, 5, 3},
                    {1, 3, 5, 3, 1}},
        .divisor = 48
    },
    {
        .name = "stucki",
        .shift = 3,
        .pattern = {{0, 0, 0, 8, 4},
                    {2, 4, 8, 4, 2},
                    {1, 2, 4, 2, 1}},
        .divisor = 42
    },
    {
        .name = "burkes",
        .shift = 3,
        .pattern = {{0, 0, 0, 8, 4},
                    {2, 4, 8, 4, 2},
                    {0, 0, 0, 0, 0}},
        .divisor = 32
    },
    {
        .name = "sierra-3",
        .shift = 3,
        .pattern = {{0, 0, 0, 5, 3},
                    {2, 4, 5, 4, 2},
                    {0, 2, 3, 2, 0}},
        .divisor = 32
    },
    {
        .name = "sierra-2",
        .shift = 3,
        .pattern = {{0, 0, 0, 4, 3},
                    {1, 2, 3, 2, 1},
                    {0, 0, 0, 0, 0}},
        .divisor = 16
    },
    {0}
};
