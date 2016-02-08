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
 */

#ifndef MPV_DXVA2_H
#define MPV_DXVA2_H

#include <d3d9.h>
#include <dxva2api.h>

struct mp_image;
struct mp_image_pool;

LPDIRECT3DSURFACE9 d3d9_surface_in_mp_image(struct mp_image *mpi);
void dxva2_img_ref_decoder(struct mp_image *mpi, IDirectXVideoDecoder *decoder);

void dxva2_pool_set_allocator(struct mp_image_pool *pool,
                              IDirectXVideoDecoderService *decoder_service,
                              D3DFORMAT target_format, int surface_alignment);

#endif
