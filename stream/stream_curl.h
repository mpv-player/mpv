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

#include <stdbool.h>

#include "config.h"

#include <libavformat/avio.h>

struct mpv_global;
struct demuxer;

// Initialize libcurl state, must be called before stream_curl is used.
void mp_curl_global_init(struct mpv_global *global);

// Open `url` via mpv's libcurl backend and wrap it as a fresh AVIOContext.
// On success returns 0, fills *pb_out with the new context, and sets *data to
// an opaque handle that must later be passed to mp_curl_avio_close() to
// release all associated resources.
// Returns AVERROR(ENOSYS) when this backend doesn't handle the URL.
// Returns AVERROR(EINVAL) when the URL's protocol is rejected by
// `whitelist`/`blacklist`.
// `flags` is the AVIO_FLAG_* mask passed to AVFormatContext.io_open.
// If `options` has `protocol_whitelist` or `protocol_blacklist` entries they
// override the explicit `whitelist`/`blacklist` arguments (same as FFmpeg).
int mp_curl_avio_open(struct demuxer *demuxer, AVIOContext **pb_out,
                      void **data, const char *url, int flags,
                      AVDictionary **options,
                      const char *whitelist, const char *blacklist);

// Tear down an AVIOContext previously produced by mp_curl_avio_open().
void mp_curl_avio_close(AVIOContext *pb, void *data);
