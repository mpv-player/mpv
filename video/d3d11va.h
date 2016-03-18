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

#ifndef MPV_D3D11_H
#define MPV_D3D11_H

#include <d3d11.h>

struct mp_image;

ID3D11VideoDecoderOutputView *d3d11_surface_in_mp_image(struct mp_image *mpi);
ID3D11Texture2D              *d3d11_texture_in_mp_image(struct mp_image *mpi);
struct mp_image *d3d11va_new_ref(ID3D11VideoDecoderOutputView *view,
                                 int w, int h);

#endif
