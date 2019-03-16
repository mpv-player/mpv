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

#ifndef MP_GL_ERROR_DIFFUSION
#define MP_GL_ERROR_DIFFUSION

#include "shader_cache.h"

// defines the border of all error diffusion kernels
#define EF_MIN_DELTA_X (-2)
#define EF_MAX_DELTA_X  (2)
#define EF_MAX_DELTA_Y  (2)

struct error_diffusion_kernel {
    const char *name;

    // The minimum value such that a (y, x) -> (y, x + y * shift) mapping will
    // make all error pushing operations affect next column (and after it) only.
    int shift;

    // The diffusion factor for (y, x) is pattern[y][x - EF_MIN_DELTA_X] / divisor.
    int pattern[EF_MAX_DELTA_Y + 1][EF_MAX_DELTA_X - EF_MIN_DELTA_X + 1];
    int divisor;
};

extern const struct error_diffusion_kernel mp_error_diffusion_kernels[];

const struct error_diffusion_kernel *mp_find_error_diffusion_kernel(const char *name);
int mp_ef_compute_shared_memory_size(const struct error_diffusion_kernel *k, int height);
void pass_error_diffusion(struct gl_shader_cache *sc,
                          const struct error_diffusion_kernel *k,
                          int tex, int width, int height, int depth, int block_size);

#endif /* MP_GL_ERROR_DIFFUSION */
