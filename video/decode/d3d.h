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

#ifndef MPV_DECODE_D3D_H
#define MPV_DECODE_D3D_H

#include <windows.h>
#include <d3d11.h>

#include <stdbool.h>
#include <inttypes.h>

// Must call d3d_load_dlls() before accessing. Once this is done, the DLLs
// remain loaded forever.
extern HMODULE d3d11_dll, d3d9_dll, dxva2_dll;
extern PFN_D3D11_CREATE_DEVICE d3d11_D3D11CreateDevice;

void d3d_load_dlls(void);

bool d3d11_check_decoding(ID3D11Device *dev);

struct AVBufferRef;
struct IDirect3DDevice9;

struct AVBufferRef *d3d11_wrap_device_ref(ID3D11Device *device);
struct AVBufferRef *d3d9_wrap_device_ref(struct IDirect3DDevice9 *device);

#endif
