/*
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_EOSD_PACKER_H
#define MPLAYER_EOSD_PACKER_H

#include <inttypes.h>
#include <stdbool.h>

#include "sub/ass_mp.h"
#include "sub/dec_sub.h"

// Pool of surfaces
struct eosd_surface {
    //void *native_surface;
    int w;
    int h;
};

struct eosd_rect {
    int x0, y0, x1, y1;
};

// List of surfaces to be rendered
struct eosd_target {
    struct eosd_rect source;    // position in EOSD surface
    struct eosd_rect dest;      // position on screen
    uint32_t color;             // libass-style color of the image
    // NOTE: This must not be accessed after you return from your VO's
    //       VOCTRL_DRAW_EOSD call - libass will free or reuse the associated
    //       memory. Feel free to set this to NULL to make erroneous accesses to
    //       this member fail early.
    ASS_Image *ass_img;
};

struct eosd_packer {
    struct eosd_surface surface;
    struct eosd_target *targets;
    int targets_count;  // number of valid elements in targets
    int targets_size;   // number of allocated elements in targets

    uint32_t max_surface_width;
    uint32_t max_surface_height;

    int *scratch;
};

struct eosd_packer *eosd_packer_create(void *talloc_ctx);
void eosd_packer_reinit(struct eosd_packer *state, uint32_t max_width,
                        uint32_t max_height);
void eosd_packer_generate(struct eosd_packer *state, mp_eosd_images_t *imgs,
                          bool *out_need_reposition, bool *out_need_upload,
                          bool *out_need_reallocate);
bool eosd_packer_calculate_source_bb(struct eosd_packer *state,
                                     struct eosd_rect *out_bb);

#endif /* MPLAYER_EOSD_PACKER_H */
