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

#ifndef MPLAYER_STREAM_H
#define MPLAYER_STREAM_H

#include "config.h"
#include "core/mp_msg.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <fcntl.h>

#include "core/bstr.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define STREAMTYPE_DUMMY -1    // for placeholders, when the actual reading is handled in the demuxer
#define STREAMTYPE_FILE 0      // read from seekable file
#define STREAMTYPE_VCD  1      // raw mode-2 CDROM reading, 2324 bytes/sector
#define STREAMTYPE_DVD  3      // libdvdread
#define STREAMTYPE_MEMORY 4
#define STREAMTYPE_PLAYLIST 6  // FIXME!!! same as STREAMTYPE_FILE now
#define STREAMTYPE_CDDA 10     // raw audio CD reader
#define STREAMTYPE_SMB 11      // smb:// url, using libsmbclient (samba)
#define STREAMTYPE_VCDBINCUE 12      // vcd directly from bin/cue files
#define STREAMTYPE_DVB 13
#define STREAMTYPE_VSTREAM 14
#define STREAMTYPE_SDP 15
#define STREAMTYPE_PVR 16
#define STREAMTYPE_TV 17
#define STREAMTYPE_MF 18
#define STREAMTYPE_RADIO 19
#define STREAMTYPE_BLURAY 20
#define STREAMTYPE_AVDEVICE 21
#define STREAMTYPE_CACHE 22

#define STREAM_BUFFER_SIZE 2048
#define STREAM_MAX_SECTOR_SIZE (8 * 1024)

// Max buffer for initial probe.
#define STREAM_MAX_BUFFER_SIZE (2 * 1024 * 1024)


// stream->mode
#define STREAM_READ  0
#define STREAM_WRITE 1

// stream->flags
#define MP_STREAM_SEEK_BW  2
#define MP_STREAM_SEEK_FW  4
#define MP_STREAM_SEEK  (MP_STREAM_SEEK_BW | MP_STREAM_SEEK_FW)

#define STREAM_UNSUPPORTED -1
#define STREAM_ERROR 0
#define STREAM_OK    1

#define MAX_STREAM_PROTOCOLS 20

#define STREAM_CTRL_GET_TIME_LENGTH 1
#define STREAM_CTRL_SEEK_TO_CHAPTER 2
#define STREAM_CTRL_GET_CURRENT_CHAPTER 3
#define STREAM_CTRL_GET_NUM_CHAPTERS 4
#define STREAM_CTRL_GET_CURRENT_TIME 5
#define STREAM_CTRL_SEEK_TO_TIME 6
#define STREAM_CTRL_GET_SIZE 7
#define STREAM_CTRL_GET_ASPECT_RATIO 8
#define STREAM_CTRL_GET_NUM_ANGLES 9
#define STREAM_CTRL_GET_ANGLE 10
#define STREAM_CTRL_SET_ANGLE 11
#define STREAM_CTRL_GET_NUM_TITLES 12
#define STREAM_CTRL_GET_LANG 13
#define STREAM_CTRL_GET_CURRENT_TITLE 14
#define STREAM_CTRL_GET_CACHE_SIZE 15
#define STREAM_CTRL_GET_CACHE_FILL 16
#define STREAM_CTRL_GET_CACHE_IDLE 17
#define STREAM_CTRL_RECONNECT 18
// DVD/Bluray, signal general support for GET_CURRENT_TIME etc.
#define STREAM_CTRL_MANAGES_TIMELINE 19
#define STREAM_CTRL_GET_START_TIME 20
#define STREAM_CTRL_GET_CHAPTER_TIME 21
#define STREAM_CTRL_GET_DVD_INFO 22
#define STREAM_CTRL_SET_CONTENTS 23
#define STREAM_CTRL_GET_METADATA 24

struct stream_lang_req {
    int type;     // STREAM_AUDIO, STREAM_SUB
    int id;
    char name[50];
};

struct stream_dvd_info_req {
    unsigned int palette[16];
    int num_subs;
};

struct stream;
typedef struct stream_info_st {
    const char *info;
    const char *name;
    const char *author;
    const char *comment;
    // opts is set from ->opts
    int (*open)(struct stream *st, int mode, void *opts);
    const char *protocols[MAX_STREAM_PROTOCOLS];
    const void *opts;
    int opts_url; /* If this is 1 we will parse the url as an option string
                   * too. Otherwise options are only parsed from the
                   * options string given to open_stream_plugin */
} stream_info_t;

typedef struct stream {
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

    int fd; // file descriptor, see man open(2)
    int type; // see STREAMTYPE_*
    int uncached_type; // like (uncached_stream ? uncached_stream->type : type)
    int flags; // MP_STREAM_SEEK_* or'ed flags
    int sector_size; // sector size (seek will be aligned on this size if non 0)
    int read_chunk; // maximum amount of data to read at once to limit latency
    unsigned int buf_pos, buf_len;
    int64_t pos, start_pos, end_pos;
    int eof;
    int mode; //STREAM_READ or STREAM_WRITE
    bool streaming;     // known to be a network stream if true
    int cache_size;     // cache size in KB to use if enabled
    void *priv; // used for DVD, TV, RTSP etc
    char *url; // strdup() of filename/url
    char *mime_type; // when HTTP streaming is used
    char *demuxer; // request demuxer to be used
    char *lavf_type; // name of expected demuxer type for lavf
    struct MPOpts *opts;

    FILE *capture_file;
    char *capture_filename;

    struct stream *uncached_stream;

    // Includes additional padding in case sizes get rounded up by sector size.
    unsigned char buffer[];
} stream_t;

int stream_fill_buffer(stream_t *s);

void stream_set_capture_file(stream_t *s, const char *filename);

int stream_enable_cache_percent(stream_t **stream, int64_t stream_cache_size,
                                float stream_cache_min_percent,
                                float stream_cache_seek_min_percent);
int stream_enable_cache(stream_t **stream, int64_t size, int64_t min,
                        int64_t seek_limit);

// Internal
int stream_cache_init(stream_t *cache, stream_t *stream, int64_t size,
                      int64_t min, int64_t seek_limit);

int stream_write_buffer(stream_t *s, unsigned char *buf, int len);

inline static int stream_read_char(stream_t *s)
{
    return (s->buf_pos < s->buf_len) ? s->buffer[s->buf_pos++] :
           (stream_fill_buffer(s) ? s->buffer[s->buf_pos++] : -256);
}

inline static unsigned int stream_read_word(stream_t *s)
{
    int x, y;
    x = stream_read_char(s);
    y = stream_read_char(s);
    return (x << 8) | y;
}

inline static unsigned int stream_read_dword(stream_t *s)
{
    unsigned int y;
    y = stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    return y;
}

#define stream_read_fourcc stream_read_dword_le

inline static unsigned int stream_read_word_le(stream_t *s)
{
    int x, y;
    x = stream_read_char(s);
    y = stream_read_char(s);
    return (y << 8) | x;
}

inline static uint32_t stream_read_dword_le(stream_t *s)
{
    unsigned int y;
    y = stream_read_char(s);
    y |= stream_read_char(s) << 8;
    y |= stream_read_char(s) << 16;
    y |= stream_read_char(s) << 24;
    return y;
}

inline static uint64_t stream_read_qword(stream_t *s)
{
    uint64_t y;
    y = stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    y = (y << 8) | stream_read_char(s);
    return y;
}

inline static uint64_t stream_read_qword_le(stream_t *s)
{
    uint64_t y;
    y = stream_read_dword_le(s);
    y |= (uint64_t)stream_read_dword_le(s) << 32;
    return y;
}

unsigned char *stream_read_line(stream_t *s, unsigned char *mem, int max,
                                int utf16);

inline static int stream_eof(stream_t *s)
{
    return s->eof;
}

inline static int64_t stream_tell(stream_t *s)
{
    return s->pos + s->buf_pos - s->buf_len;
}

int stream_skip(stream_t *s, int64_t len);
int stream_seek(stream_t *s, int64_t pos);
int stream_read(stream_t *s, char *mem, int total);
int stream_read_partial(stream_t *s, char *buf, int buf_size);
struct bstr stream_peek(stream_t *s, int len);

struct MPOpts;

struct bstr stream_read_complete(struct stream *s, void *talloc_ctx,
                                 int max_size);
int stream_control(stream_t *s, int cmd, void *arg);
void stream_update_size(stream_t *s);
void free_stream(stream_t *s);
struct stream *stream_open(const char *filename, struct MPOpts *options);
stream_t *open_output_stream(const char *filename, struct MPOpts *options);
stream_t *open_memory_stream(void *data, int len);
struct demux_stream;

/// Set the callback to be used by libstream to check for user
/// interruption during long blocking operations (cache filling, etc).
struct input_ctx;
void stream_set_interrupt_callback(int (*cb)(struct input_ctx *, int),
                                   struct input_ctx *ctx);
/// Call the interrupt checking callback if there is one and
/// wait for time milliseconds
int stream_check_interrupt(int time);

bool stream_manages_timeline(stream_t *s);

extern int dvd_title;
extern int dvd_angle;

extern int bluray_angle;
extern char *bluray_device;

typedef struct {
    int id; // 0 - 31 mpeg; 128 - 159 ac3; 160 - 191 pcm
    int language;
    int type;
    int channels;
} stream_language_t;

#endif /* MPLAYER_STREAM_H */
