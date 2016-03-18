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
#include <inttypes.h>

struct mp_image;
struct lavc_ctx;

struct d3d_decoder_fmt {
    const GUID *guid;
    int   mpfmt_decoded;
    DWORD dxfmt_decoded; // D3DFORMAT or DXGI_FORMAT
};

int d3d_probe_codec(const char *decode);
struct d3d_decoder_fmt d3d_select_decoder_mode(
    struct lavc_ctx *s, const GUID *device_guids, UINT n_guids,
    DWORD (*get_dxfmt_cb)(struct lavc_ctx *s, const GUID *guid, int depth));

char *d3d_decoder_guid_to_desc_buf(char *buf, size_t buf_size,
                                   const GUID *mode_guid);
#define d3d_decoder_guid_to_desc(guid) d3d_decoder_guid_to_desc_buf((char[256]){0}, 256, (guid))

void d3d_surface_align(struct lavc_ctx *s, int *w, int *h);
unsigned d3d_decoder_config_score(struct lavc_ctx *s,
                                  GUID *guidConfigBitstreamEncryption,
                                  UINT ConfigBitstreamRaw);
BOOL is_clearvideo(const GUID *mode_guid);
void copy_nv12(struct mp_image *dest, uint8_t *src_bits,
               unsigned src_pitch, unsigned surf_height);

#endif
