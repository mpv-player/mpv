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

#pragma once

#include "filter.h"

struct sh_stream;

// Enhancement-layer pairing filter.
//
// Reads base-layer (BL) mp_image frames from its input pin, decodes the
// enhancement-layer (EL) stream `el_sh` via an internal mp_decoder_wrapper,
// and attaches each decoded EL frame as `mpi->enhancement_layer` of the BL
// frame with the matching PTS. Unmatched BL frames are forwarded unchanged
// (BL-only fallback).
//
// `el_sh` must be a dependent sibling of the BL stream via sh_stream_group,
// it is auto-selected by the demuxer when the BL is selected.
//
// 1 input pin (BL frames), 1 output pin (paired BL frames). Returns NULL
// on init failure. The caller should fall back to BL-only rendering.
struct mp_filter *mp_enhancement_pair_create(struct mp_filter *parent,
                                             struct sh_stream *el_sh);
