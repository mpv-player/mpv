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

#include <libavformat/avio.h>

struct bstr;

// AES-128-CBC + PKCS#7 decryption layer over `inner`. `key` and `iv` are 16
// bytes each; other lengths return AVERROR(EINVAL). Neither buffer is retained
// past this call.
//
// The returned wrapper is read-only and non-seekable, and does not own
// `inner`.
int mp_avio_crypto_open(AVIOContext **out_pb, AVIOContext *inner,
                        struct bstr key, struct bstr iv);
void mp_avio_crypto_close(AVIOContext **pb);
