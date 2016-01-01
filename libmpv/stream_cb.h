/* Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Note: the client API is licensed under ISC (see above) to ease
 * interoperability with other licenses. But keep in mind that the
 * mpv core is still mostly GPLv2+. It's up to lawyers to decide
 * whether applications using this API are affected by the GPL.
 * One argument against this is that proprietary applications
 * using mplayer in slave mode is apparently tolerated, and this
 * API is basically equivalent to slave mode.
 */

#ifndef MPV_CLIENT_API_STREAM_CB_H_
#define MPV_CLIENT_API_STREAM_CB_H_

#include "client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Warning: this API is not stable yet.
 *
 * Overview
 * --------
 *
 * This API can be used to make mpv read from a stream with a custom
 * implementation. This interface is inspired by funopen on BSD and
 * fopencookie on linux. The stream is backed by five callbacks: read,
 * write, seek, size and close.
 *
 * Usage
 * -----
 *
 * Initialize the sub-api with mpv_get_sub_api(), then register your stream
 * callbacks using mpv_stream_cb_set_callbacks()
 *
 * Once registered, you can `loadfile cb://myfile`. Your open_fn will be invoked
 * with the URI and must return an opaque pointer. This pointer will be passed as
 * the first argument to all the remaining stream callbacks.
 */

/**
 * Opaque context, returned by mpv_get_sub_api(MPV_SUB_API_STREAM_CB).
 *
 * This context is global to libmpv, and the callbacks set within it are used
 * by all mpv_handles created in the same process.
 */
typedef struct mpv_stream_cb_context mpv_stream_cb_context;

typedef void *(*mpv_stream_cb_open_fn)(char *);
typedef int64_t (*mpv_stream_cb_read_fn)(void *, char *, uint64_t);
typedef int64_t (*mpv_stream_cb_write_fn)(void *, char *, uint64_t);
typedef int64_t (*mpv_stream_cb_seek_fn)(void *, int64_t, int);
typedef int64_t (*mpv_stream_cb_size_fn)(void *);
typedef void (*mpv_stream_cb_close_fn)(void *);

/**
 * Set the callbacks that are invoked to manipulate the underlying stream.
 * The callbacks specified here should not call into libmpv APIs as that
 * would cause a deadlock.
 *
 * @param open_fn callback invoked to open a stream given a URI
 * @param read_fn callback invoked to read from the stream
 * @param write_fn callback invoked to write from the stream
 * @param seek_fn callback invoked to seek the stream
 * @param size_fn callback invoked to get the size of a stream
 * @param close_fn callback invoked to close the stream
 */
void mpv_stream_cb_set_callbacks(mpv_stream_cb_context *ctx,
                                 mpv_stream_cb_open_fn open_fn,
                                 mpv_stream_cb_read_fn read_fn,
                                 mpv_stream_cb_write_fn write_fn,
                                 mpv_stream_cb_seek_fn seek_fn,
                                 mpv_stream_cb_size_fn size_fn,
                                 mpv_stream_cb_close_fn close_fn);

#ifdef __cplusplus
}
#endif

#endif
