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
 * fopencookie on linux. The stream is backed by user-defined callbacks
 * which can implement customized open, read, seek, size and close behaviors.
 *
 * Usage
 * -----
 *
 * Create the sub-api with mpv_get_sub_api(), then initialize it with
 * mpv_stream_cb_init(). Register your stream callbacks with the
 * mpv_stream_cb_set_*_fn(). Note that your custom callbacks must not
 * invoke libmpv APIs as that would cause a deadlock.
 *
 * Once registered, you can `loadfile cb://myfile`. Your open_fn will be invoked
 * with the URI and must return an opaque cookie. This cookie will be passed as
 * the first argument to all the remaining stream callbacks.
 */

/**
 * Opaque context, returned by mpv_get_sub_api(MPV_SUB_API_STREAM_CB).
 *
 * This context is global to libmpv, and the callbacks set within it are used
 * by all mpv_handles created in the same process.
 */
typedef struct mpv_stream_cb_context mpv_stream_cb_context;

/**
 * Open callback used to implement a custom stream.
 *
 * @param user_data opaque user data provided via mpv_stream_cb_init()
 * @param uri name of the stream to be opened
 * @return opaque cookie identifing the newly opened stream
 * @return NULL if the URI cannot be opened.
 */
typedef void *(*mpv_stream_cb_open_fn)(void *user_data, char *uri);

/**
 * Read callback used to implement a custom stream.
 * The semantics of the callback match read(2).
 *
 * @param cookie opaque cookie identifying the stream,
 *               returned from mpv_stream_cb_open_fn
 * @param buf buffer to read data into
 * @param size of the buffer
 * @return number of bytes read into the buffer
 * @return 0 on EOF
 * @return -1 on error
 */
typedef int64_t (*mpv_stream_cb_read_fn)(void *cookie, char *buf, uint64_t nbytes);

/**
 * Seek callback used to implement a custom stream.
 * The semantics of this callback match lseek(2).
 *
 * @param cookie opaque cookie identifying the stream,
 *               returned from mpv_stream_cb_open_fn
 * @param offset number of bytes to increment or decrement from
                 the current stream position
 * @param whence one of SEEK_SET, SEEK_CUR or SEEK_END
 * @return the resulting offset of the stream
 * @return -1 if the seek is not allowed or failed
 */
typedef int64_t (*mpv_stream_cb_seek_fn)(void *cookie, int64_t offset, int whence);

/**
 * Size callback used to implement a custom stream.
 *
 * @param cookie opaque cookie identifying the stream,
 *               returned from mpv_stream_cb_open_fn
 * @return the total size in bytes of the stream
 */
typedef int64_t (*mpv_stream_cb_size_fn)(void *cookie);

/**
 * Close callback used to implement a custom stream.
 *
 * @param cookie opaque cookie identifying the stream,
 *               returned from mpv_stream_cb_open_fn
 */
typedef void (*mpv_stream_cb_close_fn)(void *cookie);

/**
 * Initialize the stream callback module.
 *
 * @param user_data opaque pointer passed into the mpv_stream_cb_open_fn
 *        callback.
 */
void mpv_stream_cb_init(mpv_stream_cb_context *ctx, void *user_data);

/**
 * Set the open callback for the custom stream.
 */
void mpv_stream_cb_set_open_fn(mpv_stream_cb_context *ctx,
                               mpv_stream_cb_open_fn fn);

/**
 * Set the read callback for the custom stream.
 */
void mpv_stream_cb_set_read_fn(mpv_stream_cb_context *ctx,
                               mpv_stream_cb_read_fn fn);

/**
 * Set the seek callback for the custom stream.
 * This callback is optional, and can be omitted if the underlying
 * stream is not seekable.
 */
void mpv_stream_cb_set_seek_fn(mpv_stream_cb_context *ctx,
                               mpv_stream_cb_seek_fn fn);

/* Set the size callback for the custom stream.
 * This callback is optional, and can be omitted if the underlying
 * stream does not have a known size.
 */
void mpv_stream_cb_set_size_fn(mpv_stream_cb_context *ctx,
                               mpv_stream_cb_size_fn fn);

/* Set the close callback fro the custom stream.
 */
void mpv_stream_cb_set_close_fn(mpv_stream_cb_context *ctx,
                                mpv_stream_cb_close_fn fn);

#ifdef __cplusplus
}
#endif

#endif
