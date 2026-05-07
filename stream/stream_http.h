#pragma once

#include <stdbool.h>

#include "config.h"

#include <libavformat/avio.h>

struct mpv_global;
struct demuxer;

#if HAVE_LIBCURL
// Initialize libcurl state, must be called before stream_http is used.
void mp_curl_global_init(struct mpv_global *global);

// Returns true if the libcurl backend is built in and a global curl context
// has been initialized.
bool mp_curl_is_available(struct mpv_global *global);

// Open `url` via mpv's libcurl HTTP backend and wrap it as a fresh
// AVIOContext. On success returns 0, fills *pb_out with the new context, and
// sets *data to an opaque handle that must later be passed to
// mp_curl_avio_close() to release all associated resources.
//
// Returns AVERROR(ENOSYS) when the URL is not eligible (non-http(s) scheme,
// write flag set, libcurl disabled at build time, or no curl global state
// available). Other negative AVERROR codes indicate a hard failure.
//
// `flags` is the AVIO_FLAG_* mask passed to AVFormatContext.io_open.
int mp_curl_avio_open(struct demuxer *demuxer, AVIOContext **pb_out,
                      void **data, const char *url, int flags);

// Tear down an AVIOContext previously produced by mp_curl_avio_open().
void mp_curl_avio_close(AVIOContext *pb, void *data);
#else
static inline void mp_curl_global_init(struct mpv_global *global) {}
static inline bool mp_curl_is_available(struct mpv_global *global) { return false; }
static inline int mp_curl_avio_open(struct demuxer *demuxer, AVIOContext **pb_out,
                                    void **data, const char *url, int flags)
{
    return AVERROR(ENOSYS);
}
static inline void mp_curl_avio_close(AVIOContext *pb, void *data) {}
#endif
