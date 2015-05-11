/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

// Time in seconds the main thread waits for the cache thread. On wakeups, the
// code checks for user requested aborts and also prints warnings that the
// cache is being slow.
#define CACHE_WAIT_TIME 1.0

// The time the cache sleeps in idle mode. This controls how often the cache
// retries reading from the stream after EOF has reached (in case the stream is
// actually readable again, for example if data has been appended to a file).
// Note that if this timeout is too low, the player will waste too much CPU
// when player is paused.
#define CACHE_IDLE_SLEEP_TIME 1.0

// Time in seconds the cache updates "cached" controls. Note that idle mode
// will block the cache from doing this, and this timeout is honored only if
// the cache is active.
#define CACHE_UPDATE_CONTROLS_TIME 2.0


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include <libavutil/common.h>

#include "config.h"

#include "osdep/timer.h"
#include "osdep/threads.h"

#include "common/msg.h"
#include "common/tags.h"
#include "options/options.h"

#include "stream.h"
#include "common/common.h"


// Note: (struct priv*)(cache->priv)->cache == cache
struct priv {
    pthread_t cache_thread;
    bool cache_thread_running;
    pthread_mutex_t mutex;
    pthread_cond_t wakeup;

    // Constants (as long as cache thread is running)
    // Some of these might actually be changed by a synced cache resize.
    unsigned char *buffer;  // base pointer of the allocated buffer memory
    int64_t buffer_size;    // size of the allocated buffer memory
    int64_t back_size;      // keep back_size amount of old bytes for backward seek
    int64_t seek_limit;     // keep filling cache if distance is less that seek limit
    bool seekable;          // underlying stream is seekable

    struct mp_log *log;

    // Owned by the main thread
    stream_t *cache;        // wrapper stream, used by demuxer etc.

    // Owned by the cache thread
    stream_t *stream;       // "real" stream, used to read from the source media

    // All the following members are shared between the threads.
    // You must lock the mutex to access them.

    // Ringbuffer
    int64_t min_filepos;    // range of file that is cached in the buffer
    int64_t max_filepos;    // ... max_filepos being the last read position
    bool eof;               // true if max_filepos = EOF
    int64_t offset;         // buffer[WRAP(s->max_filepos - offset)] corresponds
                            // to the byte at max_filepos (must be wrapped by
                            // buffer_size)

    bool idle;              // cache thread has stopped reading
    int64_t reads;          // number of actual read attempts performed

    int64_t read_filepos;   // client read position (mirrors cache->pos)

    int64_t eof_pos;

    int control;            // requested STREAM_CTRL_... or CACHE_CTRL_...
    void *control_arg;      // temporary for executing STREAM_CTRLs
    int control_res;
    bool control_flush;

    // Cached STREAM_CTRLs
    double stream_time_length;
    int64_t stream_size;
    struct mp_tags *stream_metadata;
    double start_pts;
    bool has_avseek;
};

enum {
    CACHE_CTRL_NONE = 0,
    CACHE_CTRL_QUIT = -1,
    CACHE_CTRL_PING = -2,

    // we should fill buffer only if space>=FILL_LIMIT
    FILL_LIMIT = 16 * 1024,
};

// Used by the main thread to wakeup the cache thread, and to wait for the
// cache thread. The cache mutex has to be locked when calling this function.
// *retry_time should be set to 0 on the first call.
static void cache_wakeup_and_wait(struct priv *s, double *retry_time)
{
    double start = mp_time_sec();
    if (*retry_time >= CACHE_WAIT_TIME) {
        MP_WARN(s, "Cache is not responding - slow/stuck network connection?\n");
        *retry_time = -1; // do not warn again for this call
    }

    pthread_cond_signal(&s->wakeup);
    struct timespec ts = mp_rel_time_to_timespec(CACHE_WAIT_TIME);
    pthread_cond_timedwait(&s->wakeup, &s->mutex, &ts);

    if (*retry_time >= 0)
        *retry_time += mp_time_sec() - start;
}

// Runs in the cache thread
static void cache_drop_contents(struct priv *s)
{
    s->offset = s->min_filepos = s->max_filepos = s->read_filepos;
    s->eof = false;
    s->start_pts = MP_NOPTS_VALUE;
}

// Copy at most dst_size from the cache at the given absolute file position pos.
// Return number of bytes that could actually be read.
// Does not advance the file position, or change anything else.
// Can be called from anywhere, as long as the mutex is held.
static size_t read_buffer(struct priv *s, unsigned char *dst,
                          size_t dst_size, int64_t pos)
{
    size_t read = 0;
    while (read < dst_size) {
        if (pos >= s->max_filepos || pos < s->min_filepos)
            break;
        int64_t newb = s->max_filepos - pos; // new bytes in the buffer

        int64_t bpos = pos - s->offset; // file pos to buffer memory pos
        if (bpos < 0) {
            bpos += s->buffer_size;
        } else if (bpos >= s->buffer_size) {
            bpos -= s->buffer_size;
        }

        if (newb > s->buffer_size - bpos)
            newb = s->buffer_size - bpos; // handle wrap...

        newb = MPMIN(newb, dst_size - read);

        assert(newb >= 0 && read + newb <= dst_size);
        assert(bpos >= 0 && bpos + newb <= s->buffer_size);
        memcpy(&dst[read], &s->buffer[bpos], newb);
        read += newb;
        pos += newb;
    }
    return read;
}

// Runs in the cache thread.
// Returns true if reading was attempted, and the mutex was shortly unlocked.
static bool cache_fill(struct priv *s)
{
    int64_t read = s->read_filepos;
    int len = 0;

    // drop cache contents only if seeking backward or too much fwd.
    // This is also done for on-disk files, since it loses the backseek cache.
    // That in turn can cause major bandwidth increase and performance
    // issues with e.g. mov or badly interleaved files
    if (read < s->min_filepos || read > s->max_filepos + s->seek_limit) {
        MP_VERBOSE(s, "Dropping cache at pos %"PRId64", "
                   "cached range: %"PRId64"-%"PRId64".\n", read,
                   s->min_filepos, s->max_filepos);
        cache_drop_contents(s);
    }

    if (stream_tell(s->stream) != s->max_filepos && s->seekable) {
        MP_VERBOSE(s, "Seeking underlying stream: %"PRId64" -> %"PRId64"\n",
                   stream_tell(s->stream), s->max_filepos);
        stream_seek(s->stream, s->max_filepos);
        if (stream_tell(s->stream) != s->max_filepos)
            goto done;
    }

    if (mp_cancel_test(s->cache->cancel))
        goto done;

    // number of buffer bytes which should be preserved in backwards direction
    int64_t back = MPCLAMP(read - s->min_filepos, 0, s->back_size);

    // number of buffer bytes that are valid and can be read
    int64_t newb = FFMAX(s->max_filepos - read, 0);

    // max. number of bytes that can be written (starting from max_filepos)
    int64_t space = s->buffer_size - (newb + back);

    // offset into the buffer that maps to max_filepos
    int64_t pos = s->max_filepos - s->offset;
    if (pos >= s->buffer_size)
        pos -= s->buffer_size; // wrap-around

    if (space < FILL_LIMIT) {
        s->idle = true;
        s->reads++; // don't stuck main thread
        return false;
    }

    // limit to end of buffer (without wrapping)
    if (pos + space >= s->buffer_size)
        space = s->buffer_size - pos;

    // limit read size (or else would block and read the entire buffer in 1 call)
    space = FFMIN(space, s->stream->read_chunk);

    // back+newb+space <= buffer_size
    int64_t back2 = s->buffer_size - (space + newb); // max back size
    if (s->min_filepos < (read - back2))
        s->min_filepos = read - back2;

    // The read call might take a long time and block, so drop the lock.
    pthread_mutex_unlock(&s->mutex);
    len = stream_read_partial(s->stream, &s->buffer[pos], space);
    pthread_mutex_lock(&s->mutex);

    // Do this after reading a block, because at least libdvdnav updates the
    // stream position only after actually reading something after a seek.
    if (s->start_pts == MP_NOPTS_VALUE) {
        double pts;
        if (stream_control(s->stream, STREAM_CTRL_GET_CURRENT_TIME, &pts) > 0)
            s->start_pts = pts;
    }

    s->max_filepos += len;
    if (pos + len == s->buffer_size)
        s->offset += s->buffer_size; // wrap...

done:
    s->eof = len <= 0;
    s->idle = s->eof;
    s->reads++;
    if (s->eof) {
        s->eof_pos = stream_tell(s->stream);
        MP_TRACE(s, "EOF reached.\n");
    }

    pthread_cond_signal(&s->wakeup);

    return true;
}

// This is called both during init and at runtime.
static int resize_cache(struct priv *s, int64_t size)
{
    int64_t min_size = FILL_LIMIT * 4;
    int64_t max_size = ((size_t)-1) / 4;
    int64_t buffer_size = MPCLAMP(size, min_size, max_size);

    unsigned char *buffer = malloc(buffer_size);
    if (!buffer) {
        free(buffer);
        return STREAM_ERROR;
    }

    if (s->buffer) {
        // Copy & free the old ringbuffer data.
        // If the buffer is too small, prefer to copy these regions:
        // 1. Data starting from read_filepos, until cache end
        size_t read_1 = read_buffer(s, buffer, buffer_size, s->read_filepos);
        // 2. then data from before read_filepos until cache start
        //    (this one needs to be copied to the end of the ringbuffer)
        size_t read_2 = 0;
        if (s->min_filepos < s->read_filepos) {
            size_t copy_len = buffer_size - read_1;
            copy_len = MPMIN(copy_len, s->read_filepos - s->min_filepos);
            assert(copy_len + read_1 <= buffer_size);
            read_2 = read_buffer(s, buffer + buffer_size - copy_len, copy_len,
                                 s->read_filepos - copy_len);
            // This shouldn't happen, unless copy_len was computed incorrectly.
            assert(read_2 == copy_len);
        }
        // Set it up such that read_1 is at buffer pos 0, and read_2 wraps
        // around below it, so that it is located at the end of the buffer.
        s->min_filepos = s->read_filepos - read_2;
        s->max_filepos = s->read_filepos + read_1;
        s->offset = s->max_filepos - read_1;
    } else {
        cache_drop_contents(s);
    }

    free(s->buffer);

    s->buffer_size = buffer_size;
    s->back_size = buffer_size / 2;
    s->buffer = buffer;
    s->idle = false;
    s->eof = false;

    //make sure that we won't wait from cache_fill
    //more data than it is allowed to fill
    if (s->seek_limit > s->buffer_size - FILL_LIMIT)
        s->seek_limit = s->buffer_size - FILL_LIMIT;

    return STREAM_OK;
}

static void update_cached_controls(struct priv *s)
{
    int64_t i64;
    double d;
    struct mp_tags *tags;
    s->stream_time_length = 0;
    if (stream_control(s->stream, STREAM_CTRL_GET_TIME_LENGTH, &d) == STREAM_OK)
        s->stream_time_length = d;
    if (stream_control(s->stream, STREAM_CTRL_GET_METADATA, &tags) == STREAM_OK) {
        talloc_free(s->stream_metadata);
        s->stream_metadata = talloc_steal(s, tags);
    }
    s->stream_size = s->eof_pos;
    if (stream_control(s->stream, STREAM_CTRL_GET_SIZE, &i64) == STREAM_OK)
        s->stream_size = i64;
    s->has_avseek = stream_control(s->stream, STREAM_CTRL_HAS_AVSEEK, NULL) > 0;
}

// the core might call these every frame, so cache them...
static int cache_get_cached_control(stream_t *cache, int cmd, void *arg)
{
    struct priv *s = cache->priv;
    switch (cmd) {
    case STREAM_CTRL_GET_CACHE_SIZE:
        *(int64_t *)arg = s->buffer_size;
        return STREAM_OK;
    case STREAM_CTRL_GET_CACHE_FILL:
        *(int64_t *)arg = s->max_filepos - s->read_filepos;
        return STREAM_OK;
    case STREAM_CTRL_GET_CACHE_IDLE:
        *(int *)arg = s->idle;
        return STREAM_OK;
    case STREAM_CTRL_GET_TIME_LENGTH:
        *(double *)arg = s->stream_time_length;
        return s->stream_time_length ? STREAM_OK : STREAM_UNSUPPORTED;
    case STREAM_CTRL_GET_SIZE:
        if (s->stream_size < 0)
            return STREAM_UNSUPPORTED;
        *(int64_t *)arg = s->stream_size;
        return STREAM_OK;
    case STREAM_CTRL_GET_CURRENT_TIME: {
        if (s->start_pts == MP_NOPTS_VALUE)
            return STREAM_UNSUPPORTED;
        *(double *)arg = s->start_pts;
        return STREAM_OK;
    }
    case STREAM_CTRL_HAS_AVSEEK:
        return s->has_avseek ? STREAM_OK : STREAM_UNSUPPORTED;
    case STREAM_CTRL_GET_METADATA: {
        if (s->stream_metadata) {
            ta_set_parent(s->stream_metadata, NULL);
            *(struct mp_tags **)arg = s->stream_metadata;
            s->stream_metadata = NULL;
            return STREAM_OK;
        }
        return STREAM_UNSUPPORTED;
    }
    case STREAM_CTRL_RESUME_CACHE:
        s->idle = s->eof = false;
        pthread_cond_signal(&s->wakeup);
        return STREAM_OK;
    case STREAM_CTRL_AVSEEK:
        if (!s->has_avseek)
            return STREAM_UNSUPPORTED;
        break;
    }
    return STREAM_ERROR;
}

static bool control_needs_flush(int stream_ctrl)
{
    switch (stream_ctrl) {
    case STREAM_CTRL_SEEK_TO_TIME:
    case STREAM_CTRL_AVSEEK:
    case STREAM_CTRL_SET_ANGLE:
    case STREAM_CTRL_SET_CURRENT_TITLE:
    case STREAM_CTRL_RECONNECT:
    case STREAM_CTRL_DVB_SET_CHANNEL:
    case STREAM_CTRL_DVB_STEP_CHANNEL:
        return true;
    }
    return false;
}

// Runs in the cache thread
static void cache_execute_control(struct priv *s)
{
    uint64_t old_pos = stream_tell(s->stream);
    s->control_flush = false;

    switch (s->control) {
    case STREAM_CTRL_SET_CACHE_SIZE:
        s->control_res = resize_cache(s, *(int64_t *)s->control_arg);
        break;
    default:
        s->control_res = stream_control(s->stream, s->control, s->control_arg);
    }

    bool pos_changed = old_pos != stream_tell(s->stream);
    bool ok = s->control_res == STREAM_OK;
    if (pos_changed && !ok) {
        MP_ERR(s, "STREAM_CTRL changed stream pos but "
               "returned error, this is not allowed!\n");
    } else if (pos_changed || (ok && control_needs_flush(s->control))) {
        MP_VERBOSE(s, "Dropping cache due to control()\n");
        s->read_filepos = stream_tell(s->stream);
        s->control_flush = true;
        cache_drop_contents(s);
    }

    update_cached_controls(s);
    s->control = CACHE_CTRL_NONE;
    pthread_cond_signal(&s->wakeup);
}

static void *cache_thread(void *arg)
{
    struct priv *s = arg;
    mpthread_set_name("cache");
    pthread_mutex_lock(&s->mutex);
    update_cached_controls(s);
    double last = mp_time_sec();
    while (s->control != CACHE_CTRL_QUIT) {
        if (mp_time_sec() - last > CACHE_UPDATE_CONTROLS_TIME) {
            update_cached_controls(s);
            last = mp_time_sec();
        }
        if (s->control > 0) {
            cache_execute_control(s);
        } else {
            cache_fill(s);
        }
        if (s->control == CACHE_CTRL_PING) {
            pthread_cond_signal(&s->wakeup);
            s->control = CACHE_CTRL_NONE;
        }
        if (s->idle && s->control == CACHE_CTRL_NONE) {
            struct timespec ts = mp_rel_time_to_timespec(CACHE_IDLE_SLEEP_TIME);
            pthread_cond_timedwait(&s->wakeup, &s->mutex, &ts);
        }
    }
    pthread_cond_signal(&s->wakeup);
    pthread_mutex_unlock(&s->mutex);
    MP_VERBOSE(s, "Cache exiting...\n");
    return NULL;
}

static int cache_fill_buffer(struct stream *cache, char *buffer, int max_len)
{
    struct priv *s = cache->priv;
    assert(s->cache_thread_running);

    pthread_mutex_lock(&s->mutex);

    if (cache->pos != s->read_filepos)
        MP_ERR(s, "!!! read_filepos differs !!! report this bug...\n");

    int readb = 0;
    if (max_len > 0) {
        double retry_time = 0;
        int64_t retry = s->reads - 1; // try at least 1 read on EOF
        while (1) {
            readb = read_buffer(s, buffer, max_len, s->read_filepos);
            s->read_filepos += readb;
            if (readb > 0)
                break;
            if (s->eof && s->read_filepos >= s->max_filepos && s->reads >= retry)
                break;
            s->idle = false;
            if (mp_cancel_test(s->cache->cancel))
                break;
            cache_wakeup_and_wait(s, &retry_time);
        }
    }

    // wakeup the cache thread, possibly make it read more data ahead
    pthread_cond_signal(&s->wakeup);
    pthread_mutex_unlock(&s->mutex);
    return readb;
}

static int cache_seek(stream_t *cache, int64_t pos)
{
    struct priv *s = cache->priv;
    assert(s->cache_thread_running);
    int r = 1;

    pthread_mutex_lock(&s->mutex);

    MP_DBG(s, "request seek: %" PRId64 " <= to=%" PRId64
           " (cur=%" PRId64 ") <= %" PRId64 "  \n",
           s->min_filepos, pos, s->read_filepos, s->max_filepos);

    if (!s->seekable && pos > s->max_filepos) {
        MP_ERR(s, "Attempting to seek past cached data in unseekable stream.\n");
        r = 0;
    } else if (!s->seekable && pos < s->min_filepos) {
        MP_ERR(s, "Attempting to seek before cached data in unseekable stream.\n");
        r = 0;
    } else {
        cache->pos = s->read_filepos = pos;
        s->eof = false; // so that cache_read() will actually wait for new data
        pthread_cond_signal(&s->wakeup);
    }

    pthread_mutex_unlock(&s->mutex);

    return r;
}

static int cache_control(stream_t *cache, int cmd, void *arg)
{
    struct priv *s = cache->priv;
    int r = STREAM_ERROR;

    assert(cmd > 0);

    pthread_mutex_lock(&s->mutex);

    r = cache_get_cached_control(cache, cmd, arg);
    if (r != STREAM_ERROR)
        goto done;

    MP_VERBOSE(s, "blocking for STREAM_CTRL %d\n", cmd);

    s->control = cmd;
    s->control_arg = arg;
    double retry = 0;
    while (s->control != CACHE_CTRL_NONE) {
        if (mp_cancel_test(s->cache->cancel)) {
            s->eof = 1;
            r = STREAM_UNSUPPORTED;
            goto done;
        }
        cache_wakeup_and_wait(s, &retry);
    }
    r = s->control_res;
    if (s->control_flush) {
        stream_drop_buffers(cache);
        cache->pos = s->read_filepos;
    }

done:
    pthread_mutex_unlock(&s->mutex);
    return r;
}

static void cache_uninit(stream_t *cache)
{
    struct priv *s = cache->priv;
    if (s->cache_thread_running) {
        MP_VERBOSE(s, "Terminating cache...\n");
        pthread_mutex_lock(&s->mutex);
        s->control = CACHE_CTRL_QUIT;
        pthread_cond_signal(&s->wakeup);
        pthread_mutex_unlock(&s->mutex);
        pthread_join(s->cache_thread, NULL);
    }
    pthread_mutex_destroy(&s->mutex);
    pthread_cond_destroy(&s->wakeup);
    free(s->buffer);
    talloc_free(s);
}

// return 1 on success, 0 if the cache is disabled/not needed, and -1 on error
// or if the cache is disabled
int stream_cache_init(stream_t *cache, stream_t *stream,
                      struct mp_cache_opts *opts)
{
    if (opts->size < 1)
        return 0;

    struct priv *s = talloc_zero(NULL, struct priv);
    s->log = cache->log;
    s->eof_pos = -1;

    cache_drop_contents(s);

    s->seek_limit = opts->seek_min * 1024ULL;

    int64_t cache_size = opts->size * 1024ULL;

    int64_t file_size = -1;
    stream_control(stream, STREAM_CTRL_GET_SIZE, &file_size);
    if (file_size >= 0)
        cache_size = MPMIN(cache_size, file_size);

    if (resize_cache(s, cache_size) != STREAM_OK) {
        MP_ERR(s, "Failed to allocate cache buffer.\n");
        talloc_free(s);
        return -1;
    }

    MP_VERBOSE(cache, "Cache size set to %" PRId64 " KiB\n",
               s->buffer_size / 1024);

    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->wakeup, NULL);

    cache->priv = s;
    s->cache = cache;
    s->stream = stream;

    cache->seek = cache_seek;
    cache->fill_buffer = cache_fill_buffer;
    cache->control = cache_control;
    cache->close = cache_uninit;

    int64_t min = opts->initial * 1024ULL;
    if (min > s->buffer_size - FILL_LIMIT)
        min = s->buffer_size - FILL_LIMIT;

    s->seekable = stream->seekable;

    if (pthread_create(&s->cache_thread, NULL, cache_thread, s) != 0) {
        MP_ERR(s, "Starting cache thread failed.\n");
        return -1;
    }
    s->cache_thread_running = true;

    // wait until cache is filled with at least min bytes
    if (min < 1)
        return 1;
    for (;;) {
        if (mp_cancel_test(cache->cancel))
            return -1;
        int64_t fill;
        int idle;
        if (stream_control(s->cache, STREAM_CTRL_GET_CACHE_FILL, &fill) < 0)
            break;
        if (stream_control(s->cache, STREAM_CTRL_GET_CACHE_IDLE, &idle) < 0)
            break;
        MP_INFO(s, "\rCache fill: %5.2f%% "
                "(%" PRId64 " bytes)   ", 100.0 * fill / s->buffer_size, fill);
        if (fill >= min)
            break;
        if (idle)
            break;    // file is smaller than prefill size
        // Wake up if the cache is done reading some data (or on timeout/abort)
        pthread_mutex_lock(&s->mutex);
        s->control = CACHE_CTRL_PING;
        pthread_cond_signal(&s->wakeup);
        cache_wakeup_and_wait(s, &(double){0});
        pthread_mutex_unlock(&s->mutex);
    }
    MP_INFO(s, "\n");
    return 1;
}
