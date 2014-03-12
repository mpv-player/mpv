/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// Time in seconds the main thread waits for the cache thread. On wakeups, the
// code checks for user requested aborts and also prints warnings that the
// cache is being slow.
#define CACHE_WAIT_TIME 0.5

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

// Time in seconds the cache prints a new message at all.
#define CACHE_NO_SPAM 5.0


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

#include "stream.h"
#include "common/common.h"


// Note: (struct priv*)(cache->priv)->cache == cache
struct priv {
    pthread_t cache_thread;
    bool cache_thread_running;
    pthread_mutex_t mutex;
    pthread_cond_t wakeup;

    // Constants (as long as cache thread is running)
    unsigned char *buffer;  // base pointer of the allocated buffer memory
    int64_t buffer_size;    // size of the allocated buffer memory
    int64_t back_size;      // keep back_size amount of old bytes for backward seek
    int64_t fill_limit;     // we should fill buffer only if space>=fill_limit
    int64_t seek_limit;     // keep filling cache if distance is less that seek limit
    struct byte_meta *bm;   // additional per-byte metadata
    bool seekable;          // underlying stream is seekable

    struct mp_log *log;

    // Owned by the main thread
    stream_t *cache;        // wrapper stream, used by demuxer etc.
    double last_warn_time;

    // Owned by the cache thread
    stream_t *stream;       // "real" stream, used to read from the source media

    // All the following members are shared between the threads.
    // You must lock the mutex to access them.

    // Ringbuffer
    int64_t min_filepos;    // range of file that is cached in the buffer
    int64_t max_filepos;    // ... max_filepos being the last read position
    bool eof;               // true if max_filepos = EOF
    int64_t offset;         // buffer[offset] correponds to max_filepos

    bool idle;              // cache thread has stopped reading
    int64_t reads;          // number of actual read attempts performed

    int64_t read_filepos;   // client read position (mirrors cache->pos)
    int control;            // requested STREAM_CTRL_... or CACHE_CTRL_...
    void *control_arg;      // temporary for executing STREAM_CTRLs
    int control_res;
    bool control_flush;

    // Cached STREAM_CTRLs
    double stream_time_length;
    double stream_start_time;
    int64_t stream_size;
    bool stream_manages_timeline;
    unsigned int stream_num_chapters;
    int stream_cache_idle;
    int stream_cache_fill;
    char **stream_metadata;
    char *disc_name;
};

// Store additional per-byte metadata. Since per-byte would be way too
// inefficient, store it only for every BYTE_META_CHUNK_SIZE byte.
struct byte_meta {
    float stream_pts;
};

enum {
    BYTE_META_CHUNK_SIZE = 8 * 1024,

    CACHE_INTERRUPTED = -1,

    CACHE_CTRL_NONE = 0,
    CACHE_CTRL_QUIT = -1,
    CACHE_CTRL_PING = -2,
};

static int64_t mp_clipi64(int64_t val, int64_t min, int64_t max)
{
    val = FFMIN(val, max);
    val = FFMAX(val, min);
    return val;
}

// Used by the main thread to wakeup the cache thread, and to wait for the
// cache thread. The cache mutex has to be locked when calling this function.
// *retry_time should be set to 0 on the first call.
// Returns CACHE_INTERRUPTED if the caller is supposed to abort.
static int cache_wakeup_and_wait(struct priv *s, double *retry_time)
{
    if (stream_check_interrupt(0))
        return CACHE_INTERRUPTED;

    double start = mp_time_sec();

    if (!s->last_warn_time || start - s->last_warn_time >= CACHE_NO_SPAM) {
        // Print a "more severe" warning after waiting 1 second and no new data
        if ((*retry_time) >= 1.0) {
            MP_ERR(s, "Cache keeps not responding.\n");
            s->last_warn_time = start;
        } else if (*retry_time > 0.1) {
            MP_WARN(s, "Cache is not responding - slow/stuck network connection?\n");
            s->last_warn_time = start;
        }
    }

    pthread_cond_signal(&s->wakeup);
    mpthread_cond_timed_wait(&s->wakeup, &s->mutex, CACHE_WAIT_TIME);

    *retry_time += mp_time_sec() - start;

    return 0;
}

// Runs in the cache thread
static void cache_drop_contents(struct priv *s)
{
    s->offset = s->min_filepos = s->max_filepos = s->read_filepos;
    s->eof = false;
}

// Runs in the main thread
// mutex must be held, but is sometimes temporarily dropped
static int cache_read(struct priv *s, unsigned char *buf, int size)
{
    if (size <= 0)
        return 0;

    double retry = 0;
    int64_t eof_retry = s->reads - 1; // try at least 1 read on EOF
    while (s->read_filepos >= s->max_filepos ||
           s->read_filepos < s->min_filepos)
    {
        if (s->eof && s->read_filepos >= s->max_filepos && s->reads >= eof_retry)
            return 0;
        if (cache_wakeup_and_wait(s, &retry) == CACHE_INTERRUPTED)
            return 0;
    }

    int64_t newb = s->max_filepos - s->read_filepos; // new bytes in the buffer

    int64_t pos = s->read_filepos - s->offset; // file pos to buffer memory pos
    if (pos < 0)
        pos += s->buffer_size;
    else if (pos >= s->buffer_size)
        pos -= s->buffer_size;

    if (newb > s->buffer_size - pos)
        newb = s->buffer_size - pos; // handle wrap...

    newb = FFMIN(newb, size);

    memcpy(buf, &s->buffer[pos], newb);

    s->read_filepos += newb;
    return newb;
}

// Runs in the cache thread.
// Returns true if reading was attempted, and the mutex was shortly unlocked.
static bool cache_fill(struct priv *s)
{
    int64_t read = s->read_filepos;
    int len;

    if (read < s->min_filepos || read > s->max_filepos) {
        // seek...
        MP_DBG(s, "Out of boundaries... seeking to %" PRId64 "  \n", read);
        // drop cache contents only if seeking backward or too much fwd.
        // This is also done for on-disk files, since it loses the backseek cache.
        // That in turn can cause major bandwidth increase and performance
        // issues with e.g. mov or badly interleaved files
        if (read < s->min_filepos || read >= s->max_filepos + s->seek_limit) {
            MP_VERBOSE(s, "Dropping cache at pos %"PRId64", "
                   "cached range: %"PRId64"-%"PRId64".\n", read,
                   s->min_filepos, s->max_filepos);
            cache_drop_contents(s);
            stream_seek(s->stream, read);
        }
    }

    // number of buffer bytes which should be preserved in backwards direction
    int64_t back = mp_clipi64(read - s->min_filepos, 0, s->back_size);

    // number of buffer bytes that are valid and can be read
    int64_t newb = FFMAX(s->max_filepos - read, 0);

    // max. number of bytes that can be written (starting from max_filepos)
    int64_t space = s->buffer_size - (newb + back);

    // offset into the buffer that maps to max_filepos
    int pos = s->max_filepos - s->offset;
    if (pos >= s->buffer_size)
        pos -= s->buffer_size; // wrap-around

    if (space < s->fill_limit) {
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

    double pts;
    if (stream_control(s->stream, STREAM_CTRL_GET_CURRENT_TIME, &pts) <= 0)
        pts = MP_NOPTS_VALUE;
    for (int64_t b_pos = pos; b_pos < pos + len + BYTE_META_CHUNK_SIZE;
         b_pos += BYTE_META_CHUNK_SIZE)
    {
        s->bm[b_pos / BYTE_META_CHUNK_SIZE] = (struct byte_meta){.stream_pts = pts};
    }

    s->max_filepos += len;
    if (pos + len == s->buffer_size)
        s->offset += s->buffer_size; // wrap...

    s->eof = len <= 0;
    s->idle = s->eof;
    s->reads++;
    if (s->eof)
        MP_VERBOSE(s, "EOF reached.\n");

    pthread_cond_signal(&s->wakeup);

    return true;
}

static void update_cached_controls(struct priv *s)
{
    unsigned int ui;
    double d;
    char **m;
    char *t;
    s->stream_time_length = 0;
    if (stream_control(s->stream, STREAM_CTRL_GET_TIME_LENGTH, &d) == STREAM_OK)
        s->stream_time_length = d;
    s->stream_start_time = MP_NOPTS_VALUE;
    if (stream_control(s->stream, STREAM_CTRL_GET_START_TIME, &d) == STREAM_OK)
        s->stream_start_time = d;
    s->stream_manages_timeline = false;
    if (stream_control(s->stream, STREAM_CTRL_MANAGES_TIMELINE, NULL) == STREAM_OK)
        s->stream_manages_timeline = true;
    s->stream_num_chapters = 0;
    if (stream_control(s->stream, STREAM_CTRL_GET_NUM_CHAPTERS, &ui) == STREAM_OK)
        s->stream_num_chapters = ui;
    if (stream_control(s->stream, STREAM_CTRL_GET_METADATA, &m) == STREAM_OK) {
        talloc_free(s->stream_metadata);
        s->stream_metadata = talloc_steal(s, m);
    }
    if (stream_control(s->stream, STREAM_CTRL_GET_DISC_NAME, &t) == STREAM_OK)
    {
        talloc_free(s->disc_name);
        s->disc_name = talloc_steal(s, t);
    }
    stream_update_size(s->stream);
    s->stream_size = s->stream->end_pos;
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
    case STREAM_CTRL_GET_START_TIME:
        *(double *)arg = s->stream_start_time;
        return s->stream_start_time !=
               MP_NOPTS_VALUE ? STREAM_OK : STREAM_UNSUPPORTED;
    case STREAM_CTRL_GET_SIZE:
        *(int64_t *)arg = s->stream_size;
        return STREAM_OK;
    case STREAM_CTRL_MANAGES_TIMELINE:
        return s->stream_manages_timeline ? STREAM_OK : STREAM_UNSUPPORTED;
    case STREAM_CTRL_GET_NUM_CHAPTERS:
        *(unsigned int *)arg = s->stream_num_chapters;
        return STREAM_OK;
    case STREAM_CTRL_GET_CURRENT_TIME: {
        if (s->read_filepos >= s->min_filepos &&
            s->read_filepos <= s->max_filepos &&
            s->min_filepos < s->max_filepos)
        {
            int64_t fpos = FFMIN(s->read_filepos, s->max_filepos - 1);
            int64_t pos = fpos - s->offset;
            if (pos < 0)
                pos += s->buffer_size;
            else if (pos >= s->buffer_size)
                pos -= s->buffer_size;
            double pts = s->bm[pos / BYTE_META_CHUNK_SIZE].stream_pts;
            *(double *)arg = pts;
            return pts == MP_NOPTS_VALUE ? STREAM_UNSUPPORTED : STREAM_OK;
        }
        return STREAM_UNSUPPORTED;
    }
    case STREAM_CTRL_GET_METADATA: {
        if (s->stream_metadata && s->stream_metadata[0]) {
            char **m = talloc_new(NULL);
            int num_m = 0;
            for (int n = 0; s->stream_metadata[n]; n++) {
                char *t = talloc_strdup(m, s->stream_metadata[n]);
                MP_TARRAY_APPEND(NULL, m, num_m, t);
            }
            MP_TARRAY_APPEND(NULL, m, num_m, NULL);
            MP_TARRAY_APPEND(NULL, m, num_m, NULL);
            *(char ***)arg = m;
            return STREAM_OK;
        }
        return STREAM_UNSUPPORTED;
    }
    case STREAM_CTRL_GET_DISC_NAME: {
        if (!s->disc_name)
            return STREAM_UNSUPPORTED;
        *(char **)arg = talloc_strdup(NULL, s->disc_name);
        return STREAM_OK;
    }
    case STREAM_CTRL_RESUME_CACHE:
        s->idle = s->eof = false;
        pthread_cond_signal(&s->wakeup);
        return STREAM_OK;
    }
    return STREAM_ERROR;
}

static bool control_needs_flush(int stream_ctrl)
{
    switch (stream_ctrl) {
    case STREAM_CTRL_SEEK_TO_TIME:
    case STREAM_CTRL_SEEK_TO_CHAPTER:
    case STREAM_CTRL_SET_ANGLE:
    case STREAM_CTRL_SET_CURRENT_TITLE:
        return true;
    }
    return false;
}

// Runs in the cache thread
static void cache_execute_control(struct priv *s)
{
    uint64_t old_pos = stream_tell(s->stream);

    s->control_res = stream_control(s->stream, s->control, s->control_arg);
    s->control_flush = false;

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

    s->control = CACHE_CTRL_NONE;
    pthread_cond_signal(&s->wakeup);
}

static void *cache_thread(void *arg)
{
    struct priv *s = arg;
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
        if (s->idle && s->control == CACHE_CTRL_NONE)
            mpthread_cond_timed_wait(&s->wakeup, &s->mutex, CACHE_IDLE_SLEEP_TIME);
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

    int t = cache_read(s, buffer, max_len);
    // wakeup the cache thread, possibly make it read more data ahead
    pthread_cond_signal(&s->wakeup);
    pthread_mutex_unlock(&s->mutex);
    return t;
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

    MP_VERBOSE(s, "[cache] blocking for STREAM_CTRL %d\n", cmd);

    s->control = cmd;
    s->control_arg = arg;
    double retry = 0;
    while (s->control != CACHE_CTRL_NONE) {
        if (cache_wakeup_and_wait(s, &retry) == CACHE_INTERRUPTED) {
            s->eof = 1;
            r = STREAM_UNSUPPORTED;
            goto done;
        }
    }
    r = s->control_res;
    if (s->control_flush) {
        cache->pos = s->read_filepos;
        cache->eof = 0;
        cache->buf_pos = cache->buf_len = 0;
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
    free(s->bm);
    talloc_free(s);
}

// return 1 on success, 0 if the function was interrupted and -1 on error, or
// if the cache is disabled
int stream_cache_init(stream_t *cache, stream_t *stream, int64_t size,
                      int64_t min, int64_t seek_limit)
{
    if (size < 1)
        return -1;

    MP_INFO(cache, "Cache size set to %" PRId64 " KiB\n",
            size / 1024);

    if (size > SIZE_MAX) {
        MP_FATAL(cache, "Cache size larger than max. allocation size\n");
        return -1;
    }

    struct priv *s = talloc_zero(NULL, struct priv);
    s->log = cache->log;

    //64kb min_size
    s->fill_limit = FFMAX(16 * 1024, BYTE_META_CHUNK_SIZE * 2);
    s->buffer_size = FFMAX(size, s->fill_limit * 4);
    s->back_size = s->buffer_size / 2;

    s->buffer = malloc(s->buffer_size);
    s->bm = malloc((s->buffer_size / BYTE_META_CHUNK_SIZE + 2) *
                   sizeof(struct byte_meta));
    if (!s->buffer || !s->bm) {
        MP_ERR(s, "Failed to allocate cache buffer.\n");
        free(s->buffer);
        free(s->bm);
        talloc_free(s);
        return -1;
    }

    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->wakeup, NULL);

    cache->priv = s;
    s->cache = cache;
    s->stream = stream;

    cache->seek = cache_seek;
    cache->fill_buffer = cache_fill_buffer;
    cache->control = cache_control;
    cache->close = cache_uninit;

    s->seek_limit = seek_limit;
    //make sure that we won't wait from cache_fill
    //more data than it is allowed to fill
    if (s->seek_limit > s->buffer_size - s->fill_limit)
        s->seek_limit = s->buffer_size - s->fill_limit;
    if (min > s->buffer_size - s->fill_limit)
        min = s->buffer_size - s->fill_limit;

    s->seekable = (stream->flags & MP_STREAM_SEEK) == MP_STREAM_SEEK &&
                  stream->end_pos > 0;

    if (pthread_create(&s->cache_thread, NULL, cache_thread, s) != 0) {
        MP_ERR(s, "Starting cache process/thread failed: %s.\n",
               strerror(errno));
        return -1;
    }
    s->cache_thread_running = true;

    // wait until cache is filled at least prefill_init %
    for (;;) {
        if (stream_check_interrupt(0))
            return 0;
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
