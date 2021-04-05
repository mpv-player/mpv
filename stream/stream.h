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

#ifndef MPLAYER_STREAM_H
#define MPLAYER_STREAM_H

#include "common/msg.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <fcntl.h>

#include "misc/bstr.h"

// Minimum guaranteed buffer and seek-back size. For any reads <= of this size,
// it's guaranteed that you can seek back by <= of this size again.
#define STREAM_BUFFER_SIZE 2048

// flags for stream_open_ext (this includes STREAM_READ and STREAM_WRITE)

// stream->mode
#define STREAM_READ             0
#define STREAM_WRITE            (1 << 0)

#define STREAM_SILENT           (1 << 1)

// Origin value for "security". This is an integer within the flags bit-field.
#define STREAM_ORIGIN_DIRECT    (1 << 2) // passed from cmdline or loadfile
#define STREAM_ORIGIN_FS        (2 << 2) // referenced from playlist on unix FS
#define STREAM_ORIGIN_NET       (3 << 2) // referenced from playlist on network
#define STREAM_ORIGIN_UNSAFE    (4 << 2) // from a grotesque source

#define STREAM_ORIGIN_MASK      (7 << 2) // for extracting origin value from flags

#define STREAM_LOCAL_FS_ONLY    (1 << 5) // stream_file only, no URLs
#define STREAM_LESS_NOISE       (1 << 6) // try to log errors only

// end flags for stream_open_ext (the naming convention sucks)

#define STREAM_UNSAFE -3
#define STREAM_NO_MATCH -2
#define STREAM_UNSUPPORTED -1
#define STREAM_ERROR 0
#define STREAM_OK    1

enum stream_ctrl {
    // Certain network protocols
    STREAM_CTRL_AVSEEK,
    STREAM_CTRL_HAS_AVSEEK,
    STREAM_CTRL_GET_METADATA,

    // Optical discs (internal interface between streams and demux_disc)
    STREAM_CTRL_GET_TIME_LENGTH,
    STREAM_CTRL_GET_DVD_INFO,
    STREAM_CTRL_GET_DISC_NAME,
    STREAM_CTRL_GET_NUM_CHAPTERS,
    STREAM_CTRL_GET_CURRENT_TIME,
    STREAM_CTRL_GET_CHAPTER_TIME,
    STREAM_CTRL_SEEK_TO_TIME,
    STREAM_CTRL_GET_ASPECT_RATIO,
    STREAM_CTRL_GET_NUM_ANGLES,
    STREAM_CTRL_GET_ANGLE,
    STREAM_CTRL_SET_ANGLE,
    STREAM_CTRL_GET_NUM_TITLES,
    STREAM_CTRL_GET_TITLE_LENGTH,       // double* (in: title number, out: len)
    STREAM_CTRL_GET_LANG,
    STREAM_CTRL_GET_CURRENT_TITLE,
    STREAM_CTRL_SET_CURRENT_TITLE,
};

struct stream_lang_req {
    int type;     // STREAM_AUDIO, STREAM_SUB
    int id;
    char name[50];
};

struct stream_dvd_info_req {
    unsigned int palette[16];
    int num_subs;
};

// for STREAM_CTRL_AVSEEK
struct stream_avseek {
    int stream_index;
    int64_t timestamp;
    int flags;
};

struct stream;
struct stream_open_args;
typedef struct stream_info_st {
    const char *name;
    // opts is set from ->opts
    int (*open)(struct stream *st);
    // Alternative to open(). Only either open() or open2() can be set.
    int (*open2)(struct stream *st, const struct stream_open_args *args);
    const char *const *protocols;
    bool can_write;     // correctly checks for READ/WRITE modes
    bool local_fs;      // supports STREAM_LOCAL_FS_ONLY
    int stream_origin;  // 0 or set of STREAM_ORIGIN_*; if 0, the same origin
                        // is set, or the stream's open() function handles it
} stream_info_t;

typedef struct stream {
    const struct stream_info_st *info;

    // Read
    int (*fill_buffer)(struct stream *s, void *buffer, int max_len);
    // Write
    int (*write_buffer)(struct stream *s, void *buffer, int len);
    // Seek
    int (*seek)(struct stream *s, int64_t pos);
    // Total stream size in bytes (negative if unavailable)
    int64_t (*get_size)(struct stream *s);
    // Control
    int (*control)(struct stream *s, int cmd, void *arg);
    // Close
    void (*close)(struct stream *s);

    int64_t pos;
    int eof; // valid only after read calls that returned a short result
    int mode; //STREAM_READ or STREAM_WRITE
    int stream_origin; // any STREAM_ORIGIN_*
    void *priv; // used for DVD, TV, RTSP etc
    char *url;  // filename/url (possibly including protocol prefix)
    char *path; // filename (url without protocol prefix)
    char *mime_type; // when HTTP streaming is used
    char *demuxer; // request demuxer to be used
    char *lavf_type; // name of expected demuxer type for lavf
    bool streaming : 1; // known to be a network stream if true
    bool seekable : 1; // presence of general byte seeking support
    bool fast_skip : 1; // consider stream fast enough to fw-seek by skipping
    bool is_network : 1; // I really don't know what this is for
    bool is_local_file : 1; // from the filesystem
    bool is_directory : 1; // directory on the filesystem
    bool access_references : 1; // open other streams
    struct mp_log *log;
    struct mpv_global *global;

    struct mp_cancel *cancel;   // cancellation notification

    // Read statistic for fill_buffer calls. All bytes read by fill_buffer() are
    // added to this. The user can reset this as needed.
    uint64_t total_unbuffered_read_bytes;
    // Seek statistics. The user can reset this as needed.
    uint64_t total_stream_seeks;

    // Buffer size requested by user; s->buffer may have a different size
    int requested_buffer_size;

    // This is a ring buffer. It is reset only on seeks (or when buffers are
    // dropped). Otherwise old contents always stay valid.
    // The valid buffer is from buf_start to buf_end; buf_end can be larger
    // than the buffer size (requires wrap around). buf_cur is a value in the
    // range [buf_start, buf_end].
    // When reading more data from the stream, buf_start is advanced as old
    // data is overwritten with new data.
    // Example:
    //    0  1  2  3    4  5  6  7    8  9  10 11   12 13 14 15
    //  +===========================+---------------------------+
    //  + 05 06 07 08 | 01 02 03 04 + 05 06 07 08 | 01 02 03 04 +
    //  +===========================+---------------------------+
    //                  ^ buf_start (4)  |          |
    //                                   |          ^ buf_end (12 % 8 => 4)
    //                                   ^ buf_cur (9 % 8 => 1)
    // Here, the entire 8 byte buffer is filled, i.e. buf_end - buf_start = 8.
    // buffer_mask == 7, so (x & buffer_mask) == (x % buffer_size)
    unsigned int buf_start; // index of oldest byte in buffer (is <= buffer_mask)
    unsigned int buf_cur;   // current read pos (can be > buffer_mask)
    unsigned int buf_end;   // end position (can be > buffer_mask)

    unsigned int buffer_mask; // buffer_size-1, where buffer_size == 2**n
    uint8_t *buffer;
} stream_t;

// Non-inline version of stream_read_char().
int stream_read_char_fallback(stream_t *s);

int stream_write_buffer(stream_t *s, void *buf, int len);

inline static int stream_read_char(stream_t *s)
{
    return s->buf_cur < s->buf_end
        ? s->buffer[(s->buf_cur++) & s->buffer_mask]
        : stream_read_char_fallback(s);
}

int stream_skip_bom(struct stream *s);

inline static int64_t stream_tell(stream_t *s)
{
    return s->pos + s->buf_cur - s->buf_end;
}

bool stream_seek_skip(stream_t *s, int64_t pos);
bool stream_seek(stream_t *s, int64_t pos);
int stream_read(stream_t *s, void *mem, int total);
int stream_read_partial(stream_t *s, void *buf, int buf_size);
int stream_peek(stream_t *s, int forward_size);
int stream_read_peek(stream_t *s, void *buf, int buf_size);
void stream_drop_buffers(stream_t *s);
int64_t stream_get_size(stream_t *s);

struct mpv_global;

struct bstr stream_read_complete(struct stream *s, void *talloc_ctx,
                                 int max_size);
struct bstr stream_read_file(const char *filename, void *talloc_ctx,
                             struct mpv_global *global, int max_size);

int stream_control(stream_t *s, int cmd, void *arg);
void free_stream(stream_t *s);

struct stream_open_args {
    struct mpv_global *global;
    struct mp_cancel *cancel;   // aborting stream access (used directly)
    const char *url;
    int flags;                  // STREAM_READ etc.
    const stream_info_t *sinfo; // NULL = autoprobe, otherwise force stream impl.
    void *special_arg;          // specific to impl., use only with sinfo
};

int stream_create_with_args(struct stream_open_args *args, struct stream **ret);
struct stream *stream_create(const char *url, int flags,
                             struct mp_cancel *c, struct mpv_global *global);
stream_t *open_output_stream(const char *filename, struct mpv_global *global);

void mp_url_unescape_inplace(char *buf);
char *mp_url_escape(void *talloc_ctx, const char *s, const char *ok);

// stream_memory.c
struct stream *stream_memory_open(struct mpv_global *global, void *data, int len);

// stream_concat.c
struct stream *stream_concat_open(struct mpv_global *global, struct mp_cancel *c,
                                  struct stream **streams, int num_streams);

// stream_file.c
char *mp_file_url_to_filename(void *talloc_ctx, bstr url);
char *mp_file_get_path(void *talloc_ctx, bstr url);

// stream_lavf.c
struct AVDictionary;
void mp_setup_av_network_options(struct AVDictionary **dict,
                                 const char *target_fmt,
                                 struct mpv_global *global,
                                 struct mp_log *log);

void stream_print_proto_list(struct mp_log *log);
char **stream_get_proto_list(void);
bool stream_has_proto(const char *proto);

#endif /* MPLAYER_STREAM_H */
