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

#ifndef MPV_DXVA2_H
#define MPV_DXVA2_H

#include <d3d9.h>
#include <dxva2api.h>

struct mp_image;
struct mp_image_pool;

LPDIRECT3DSURFACE9 d3d9_surface_in_mp_image(struct mp_image *mpi);

struct mp_image *dxva2_new_ref(IDirectXVideoDecoder *decoder,
                               LPDIRECT3DSURFACE9 d3d9_surface, int w, int h);

#endif
