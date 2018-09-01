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

#define STREAM_BUFFER_SIZE 2048
#define STREAM_MAX_SECTOR_SIZE (8 * 1024)

// Max buffer for initial probe.
#define STREAM_MAX_BUFFER_SIZE (2 * 1024 * 1024)


// stream->mode
#define STREAM_READ  0
#define STREAM_WRITE 1

// flags for stream_open_ext (this includes STREAM_READ and STREAM_WRITE)
#define STREAM_SAFE_ONLY 4
#define STREAM_NETWORK_ONLY 8

#define STREAM_UNSAFE -3
#define STREAM_NO_MATCH -2
#define STREAM_UNSUPPORTED -1
#define STREAM_ERROR 0
#define STREAM_OK    1

enum stream_ctrl {
    STREAM_CTRL_GET_SIZE = 1,

    // stream_memory.c
    STREAM_CTRL_SET_CONTENTS,

    // stream_rar.c
    STREAM_CTRL_GET_BASE_FILENAME,

    // Certain network protocols
    STREAM_CTRL_AVSEEK,
    STREAM_CTRL_HAS_AVSEEK,
    STREAM_CTRL_GET_METADATA,

    // TV
    STREAM_CTRL_TV_SET_SCAN,
    STREAM_CTRL_SET_TV_FREQ,
    STREAM_CTRL_GET_TV_FREQ,
    STREAM_CTRL_SET_TV_COLORS,
    STREAM_CTRL_GET_TV_COLORS,
    STREAM_CTRL_TV_SET_NORM,
    STREAM_CTRL_TV_STEP_NORM,
    STREAM_CTRL_TV_SET_CHAN,
    STREAM_CTRL_TV_GET_CHAN,
    STREAM_CTRL_TV_STEP_CHAN,
    STREAM_CTRL_TV_LAST_CHAN,
    STREAM_CTRL_DVB_SET_CHANNEL,
    STREAM_CTRL_DVB_SET_CHANNEL_NAME,
    STREAM_CTRL_DVB_GET_CHANNEL_NAME,
    STREAM_CTRL_DVB_STEP_CHANNEL,

    // Optical discs
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

// for STREAM_CTRL_SET_TV_COLORS
#define TV_COLOR_BRIGHTNESS     1
#define TV_COLOR_HUE            2
#define TV_COLOR_SATURATION     3
#define TV_COLOR_CONTRAST       4

// for STREAM_CTRL_AVSEEK
struct stream_avseek {
    int stream_index;
    int64_t timestamp;
    int flags;
};

struct stream;
typedef struct stream_info_st {
    const char *name;
    // opts is set from ->opts
    int (*open)(struct stream *st);
    const char *const *protocols;
    bool can_write;     // correctly checks for READ/WRITE modes
    bool is_safe;       // opening is no security issue, even with remote provided URLs
    bool is_network;    // used to restrict remote playlist entries to remote URLs
} stream_info_t;

typedef struct stream {
    const struct stream_info_st *info;

    // Read
    int (*fill_buffer)(struct stream *s, char *buffer, int max_len);
    // Write
    int (*write_buffer)(struct stream *s, char *buffer, int len);
    // Seek
    int (*seek)(struct stream *s, int64_t pos);
    // Control
    // Will be later used to let streams like dvd and cdda report
    // their structure (ie tracks, chapters, etc)
    int (*control)(struct stream *s, int cmd, void *arg);
    // Close
    void (*close)(struct stream *s);

    int sector_size; // sector size (seek will be aligned on this size if non 0)
    int read_chunk; // maximum amount of data to read at once to limit latency
    unsigned int buf_pos, buf_len;
    int64_t pos;
    int eof;
    int mode; //STREAM_READ or STREAM_WRITE
    void *priv; // used for DVD, TV, RTSP etc
    char *url;  // filename/url (possibly including protocol prefix)
    char *path; // filename (url without protocol prefix)
    char *mime_type; // when HTTP streaming is used
    char *demuxer; // request demuxer to be used
    char *lavf_type; // name of expected demuxer type for lavf
    bool streaming : 1; // known to be a network stream if true
    bool seekable : 1; // presence of general byte seeking support
    bool fast_skip : 1; // consider stream fast enough to fw-seek by skipping
    bool is_network : 1; // original stream_info_t.is_network flag
    bool is_local_file : 1; // from the filesystem
    bool is_directory : 1; // directory on the filesystem
    bool access_references : 1; // open other streams
    bool extended_ctrls : 1; // supports some of BD/DVD/DVB/TV controls
    struct mp_log *log;
    struct mpv_global *global;

    struct mp_cancel *cancel;   // cancellation notification

    // Read statistic for fill_buffer calls. All bytes read by fill_buffer() are
    // added to this. The user can reset this as needed.
    uint64_t total_unbuffered_read_bytes;

    // Includes additional padding in case sizes get rounded up by sector size.
    unsigned char buffer[];
} stream_t;

int stream_fill_buffer(stream_t *s);

int stream_write_buffer(stream_t *s, unsigned char *buf, int len);

inline static int stream_read_char(stream_t *s)
{
    return (s->buf_pos < s->buf_len) ? s->buffer[s->buf_pos++] :
           (stream_fill_buffer(s) ? s->buffer[s->buf_pos++] : -256);
}

unsigned char *stream_read_line(stream_t *s, unsigned char *mem, int max,
                                int utf16);
int stream_skip_bom(struct stream *s);

inline static int stream_eof(stream_t *s)
{
    return s->eof;
}

inline static int64_t stream_tell(stream_t *s)
{
    return s->pos + s->buf_pos - s->buf_len;
}

bool stream_skip(stream_t *s, int64_t len);
bool stream_seek(stream_t *s, int64_t pos);
int stream_read(stream_t *s, char *mem, int total);
int stream_read_partial(stream_t *s, char *buf, int buf_size);
struct bstr stream_peek(stream_t *s, int len);
void stream_drop_buffers(stream_t *s);
int64_t stream_get_size(stream_t *s);

struct mpv_global;

struct bstr stream_read_complete(struct stream *s, void *talloc_ctx,
                                 int max_size);
struct bstr stream_read_file(const char *filename, void *talloc_ctx,
                             struct mpv_global *global, int max_size);
int stream_control(stream_t *s, int cmd, void *arg);
void free_stream(stream_t *s);
struct stream *stream_create(const char *url, int flags,
                             struct mp_cancel *c, struct mpv_global *global);
struct stream *stream_open(const char *filename, struct mpv_global *global);
stream_t *open_output_stream(const char *filename, struct mpv_global *global);
stream_t *open_memory_stream(void *data, int len);

void mp_url_unescape_inplace(char *buf);
char *mp_url_escape(void *talloc_ctx, const char *s, const char *ok);

// stream_file.c
char *mp_file_url_to_filename(void *talloc_ctx, bstr url);
char *mp_file_get_path(void *talloc_ctx, bstr url);

// stream_lavf.c
struct AVDictionary;
void mp_setup_av_network_options(struct AVDictionary **dict,
                                 struct mpv_global *global,
                                 struct mp_log *log);

void stream_print_proto_list(struct mp_log *log);
char **stream_get_proto_list(void);
bool stream_has_proto(const char *proto);

#endif /* MPLAYER_STREAM_H */
