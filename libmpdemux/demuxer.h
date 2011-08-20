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

#ifndef MPLAYER_DEMUXER_H
#define MPLAYER_DEMUXER_H

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "bstr.h"
#include "mpcommon.h"
#include "demux_packet.h"

struct MPOpts;

#ifdef HAVE_BUILTIN_EXPECT
#define likely(x) __builtin_expect((x) != 0, 1)
#define unlikely(x) __builtin_expect((x) != 0, 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#define MAX_PACKS 4096
#define MAX_PACK_BYTES 0x8000000  // 128 MiB

enum demuxer_type {
    DEMUXER_TYPE_UNKNOWN = 0,
    DEMUXER_TYPE_MPEG_ES,
    DEMUXER_TYPE_MPEG_PS,
    DEMUXER_TYPE_AVI,
    DEMUXER_TYPE_AVI_NI,
    DEMUXER_TYPE_AVI_NINI,
    DEMUXER_TYPE_ASF,
    DEMUXER_TYPE_MOV,
    DEMUXER_TYPE_VIVO,
    DEMUXER_TYPE_TV,
    DEMUXER_TYPE_FLI,
    DEMUXER_TYPE_REAL,
    DEMUXER_TYPE_Y4M,
    DEMUXER_TYPE_FILM,
    DEMUXER_TYPE_ROQ,
    DEMUXER_TYPE_MF,
    DEMUXER_TYPE_AUDIO,
    DEMUXER_TYPE_OGG,
    DEMUXER_TYPE_RAWAUDIO,
    DEMUXER_TYPE_RTP,
    DEMUXER_TYPE_RAWDV,
    DEMUXER_TYPE_PVA,
    DEMUXER_TYPE_SMJPEG,
    DEMUXER_TYPE_XMMS,
    DEMUXER_TYPE_RAWVIDEO,
    DEMUXER_TYPE_MPEG4_ES,
    DEMUXER_TYPE_GIF,
    DEMUXER_TYPE_MPEG_TS,
    DEMUXER_TYPE_H264_ES,
    DEMUXER_TYPE_MATROSKA,
    DEMUXER_TYPE_REALAUDIO,
    DEMUXER_TYPE_MPEG_TY,
    DEMUXER_TYPE_LMLM4,
    DEMUXER_TYPE_LAVF,
    DEMUXER_TYPE_NSV,
    DEMUXER_TYPE_VQF,
    DEMUXER_TYPE_AVS,
    DEMUXER_TYPE_AAC,
    DEMUXER_TYPE_MPC,
    DEMUXER_TYPE_MPEG_PES,
    DEMUXER_TYPE_MPEG_GXF,
    DEMUXER_TYPE_NUT,
    DEMUXER_TYPE_LAVF_PREFERRED,
    DEMUXER_TYPE_RTP_NEMESI,
    DEMUXER_TYPE_MNG,
    DEMUXER_TYPE_EDL,

    /* Values after this are for internal use and can not be selected
     * as demuxer type by the user (-demuxer option). */
    DEMUXER_TYPE_END,

    DEMUXER_TYPE_DEMUXERS,
    DEMUXER_TYPE_PLAYLIST,
};

enum timestamp_type {
    TIMESTAMP_TYPE_PTS,
    TIMESTAMP_TYPE_SORT,
};


// DEMUXER control commands/answers
#define DEMUXER_CTRL_NOTIMPL -1
#define DEMUXER_CTRL_DONTKNOW 0
#define DEMUXER_CTRL_OK 1
#define DEMUXER_CTRL_GUESS 2
#define DEMUXER_CTRL_GET_TIME_LENGTH 10
#define DEMUXER_CTRL_GET_PERCENT_POS 11
#define DEMUXER_CTRL_SWITCH_AUDIO 12
#define DEMUXER_CTRL_RESYNC 13
#define DEMUXER_CTRL_SWITCH_VIDEO 14
#define DEMUXER_CTRL_IDENTIFY_PROGRAM 15
#define DEMUXER_CTRL_CORRECT_PTS 16

#define SEEK_ABSOLUTE (1 << 0)
#define SEEK_FACTOR   (1 << 1)
#define SEEK_FORWARD  (1 << 2)
#define SEEK_BACKWARD (1 << 3)

// demux_lavf can pass lavf buffers using FF_INPUT_BUFFER_PADDING_SIZE instead
#define MP_INPUT_BUFFER_PADDING_SIZE 8

typedef struct demux_stream {
    int buffer_pos;        // current buffer position
    int buffer_size;       // current buffer size
    unsigned char *buffer; // current buffer, never free() it, always use free_demux_packet(buffer_ref);
    double pts;            // current buffer's pts
    int pts_bytes;         // number of bytes read after last pts stamp
    int eof;               // end of demuxed stream? (true if all buffer empty)
    off_t pos;               // position in the input stream (file)
    off_t dpos;              // position in the demuxed stream
    int pack_no;           // serial number of packet
    int flags;             // flags of current packet (keyframe etc)
    int non_interleaved;   // 1 if this stream is not properly interleaved,
                           // so e.g. subtitle handling must do explicit reads.
//---------------
    int packs;            // number of packets in buffer
    int bytes;            // total bytes of packets in buffer
    demux_packet_t *first; // read to current buffer from here
    demux_packet_t *last; // append new packets from input stream to here
    demux_packet_t *current; // needed for refcounting of the buffer
    int id;               // stream ID  (for multiple audio/video streams)
    struct demuxer *demuxer; // parent demuxer structure (stream handler)
// ---- asf -----
    struct demux_packet *asf_packet; // read asf fragments here
    int asf_seq;
// ---- mov -----
    unsigned int ss_mul, ss_div;
// ---- stream header ----
    void *sh;
} demux_stream_t;

typedef struct demuxer_info {
    char *name;
    char *author;
    char *encoder;
    char *comments;
    char *copyright;
} demuxer_info_t;

#define MAX_A_STREAMS 256
#define MAX_V_STREAMS 256
#define MAX_S_STREAMS 256

struct demuxer;

/**
 * Demuxer description structure
 */
typedef struct demuxer_desc {
    const char *info;      // What is it (long name and/or description)
    const char *name;      // Demuxer name, used with -demuxer switch
    const char *shortdesc; // Description printed at demuxer detection
    const char *author;    // Demuxer author(s)
    const char *comment;   // Comment, printed with -demuxer help

    enum demuxer_type type;
    // If 1 detection is safe and fast, do it before file extension check
    int safe_check;

    // Check if can demux the file, return DEMUXER_TYPE_xxx on success
    // Mandatory if safe_check == 1, else optional
    int (*check_file)(struct demuxer *demuxer);
    /// Get packets from file, return 0 on eof. Mandatory
    int (*fill_buffer)(struct demuxer *demuxer, struct demux_stream *ds);
    /// Open the demuxer, return demuxer on success, NULL on failure
    struct demuxer *(*open)(struct demuxer *demuxer); // Optional
    /// Close the demuxer
    void (*close)(struct demuxer *demuxer); // Optional
    // Seek. Optional
    void (*seek)(struct demuxer *demuxer, float rel_seek_secs,
                 float audio_delay, int flags);
    // Various control functions. Optional
    int (*control)(struct demuxer *demuxer, int cmd, void *arg);
} demuxer_desc_t;

typedef struct demux_chapter
{
    uint64_t start, end;
    char *name;
} demux_chapter_t;

struct matroska_data {
    unsigned char segment_uid[16];
    // Ordered chapter information if any
    struct matroska_chapter {
        uint64_t start;
        uint64_t end;
        bool has_segment_uid;
        unsigned char segment_uid[16];
        char *name;
    } *ordered_chapters;
    int num_ordered_chapters;
};

typedef struct demux_attachment
{
    char *name;
    char *type;
    void *data;
    unsigned int data_size;
} demux_attachment_t;

struct demuxer_params {
    unsigned char (*matroska_wanted_uids)[16];
};

typedef struct demuxer {
    const demuxer_desc_t *desc; ///< Demuxer description structure
    const char *filetype; // format name when not identified by demuxer (libavformat)
    off_t filepos;  // input stream current pos.
    off_t movi_start;
    off_t movi_end;
    struct stream *stream;
    double stream_pts;     // current stream pts, if applicable (e.g. dvd)
    double reference_clock;
    char *filename;  // Needed by avs_check_file
    int synced;      // stream synced (used by mpeg)
    enum demuxer_type type;
    /* Normally the file_format field is just a copy of the type field above.
     * There are 2 exceptions I noticed. Internal demux_avi may force
     * ->type to DEMUXER_TYPE_AVI_[NI|NINI] while leaving ->file_format at
     * DEMUXER_TYPE_AVI. Internal demux_mov may set ->type to
     * DEMUXER_TYPE_PLAYLIST and also return that from the check function
     * or not (looks potentially buggy). */
    enum demuxer_type file_format;
    int seekable; // flag
    /* Set if using absolute seeks for small movements is OK (no pts resets
     * that would make pts ambigious, preferably supports back/forward flags */
    bool accurate_seek;
    enum timestamp_type timestamp_type;

    struct demux_stream *audio; // audio buffer/demuxer
    struct demux_stream *video; // video buffer/demuxer
    struct demux_stream *sub;   // dvd subtitle buffer/demuxer

    // stream headers:
    struct sh_audio *a_streams[MAX_A_STREAMS];
    struct sh_video *v_streams[MAX_V_STREAMS];
    struct sh_sub   *s_streams[MAX_S_STREAMS];

    // teletext decoder private data, if demuxer stream contains teletext
    void *teletext;

    struct demux_chapter *chapters;
    int num_chapters;

    struct demux_attachment *attachments;
    int num_attachments;

    struct matroska_data matroska_data;
    // for trivial demuxers which just read the whole file for codec to use
    struct bstr file_contents;

    void *priv;   // demuxer-specific internal data
    char **info;  // metadata
    struct MPOpts *opts;
    struct demuxer_params *params;
} demuxer_t;

typedef struct {
    int progid;      //program id
    int aid, vid, sid; //audio, video and subtitle id
} demux_program_t;

struct demux_packet *new_demux_packet(size_t len);
// data must already have suitable padding
struct demux_packet *new_demux_packet_fromdata(void *data, size_t len);
void resize_demux_packet(struct demux_packet *dp, size_t len);
struct demux_packet *clone_demux_packet(struct demux_packet *pack);
void free_demux_packet(struct demux_packet *dp);

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

static inline void *realloc_struct(void *ptr, size_t nmemb, size_t size)
{
    if (nmemb > SIZE_MAX / size) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, nmemb * size);
}

struct demuxer *new_demuxer(struct MPOpts *opts, struct stream *stream,
                            int type, int a_id, int v_id, int s_id,
                            char *filename);
void free_demuxer(struct demuxer *demuxer);

void ds_add_packet(struct demux_stream *ds, struct demux_packet *dp);
void ds_read_packet(struct demux_stream *ds, struct stream *stream, int len,
                    double pts, off_t pos, int flags);

int demux_fill_buffer(struct demuxer *demux, struct demux_stream *ds);
int ds_fill_buffer(struct demux_stream *ds);

static inline off_t ds_tell(struct demux_stream *ds)
{
    return (ds->dpos - ds->buffer_size) + ds->buffer_pos;
}

static inline int ds_tell_pts(struct demux_stream *ds)
{
    return (ds->pts_bytes - ds->buffer_size) + ds->buffer_pos;
}

int demux_read_data(struct demux_stream *ds, unsigned char *mem, int len);
int demux_pattern_3(struct demux_stream *ds, unsigned char *mem, int maxlen,
                    int *read, uint32_t pattern);

#define demux_peekc(ds) ( \
        (likely(ds->buffer_pos<ds->buffer_size)) ? ds->buffer[ds->buffer_pos] \
        : ((unlikely(!ds_fill_buffer(ds))) ? (-1) : ds->buffer[ds->buffer_pos]))
#define demux_getc(ds) ( \
        (likely(ds->buffer_pos<ds->buffer_size)) ? ds->buffer[ds->buffer_pos++] \
        : ((unlikely(!ds_fill_buffer(ds))) ? (-1) : ds->buffer[ds->buffer_pos++]))

void ds_free_packs(struct demux_stream *ds);
int ds_get_packet(struct demux_stream *ds, unsigned char **start);
int ds_get_packet_pts(struct demux_stream *ds, unsigned char **start,
                      double *pts);
int ds_get_packet_sub(struct demux_stream *ds, unsigned char **start);
struct demux_packet *ds_get_packet2(struct demux_stream *ds);
double ds_get_next_pts(struct demux_stream *ds);
int ds_parse(struct demux_stream *sh, uint8_t **buffer, int *len, double pts,
             off_t pos);
void ds_clear_parser(struct demux_stream *sh);

static inline int avi_stream_id(unsigned int id)
{
    unsigned char a, b;
    a = id - '0';
    b = (id >> 8) - '0';
    if (a>9 || b>9)
        return 100;          // invalid ID
    return a * 10 + b;
}

struct demuxer *demux_open(struct MPOpts *opts, struct stream *stream,
                           int file_format, int aid, int vid, int sid,
                           char *filename);

struct demuxer *demux_open_withparams(struct MPOpts *opts,
        struct stream *stream, int file_format, int aid, int vid, int sid,
        char *filename, struct demuxer_params *params);

void demux_flush(struct demuxer *demuxer);
int demux_seek(struct demuxer *demuxer, float rel_seek_secs, float audio_delay,
               int flags);
struct demuxer *new_demuxers_demuxer(struct demuxer *vd, struct demuxer *ad,
                                     struct demuxer *sd);

// AVI demuxer params:
extern int index_mode;  // -1=untouched  0=don't use index  1=use (generate) index
extern char *index_file_save, *index_file_load;
extern int force_ni;
extern int pts_from_bps;

int demux_info_add(struct demuxer *demuxer, const char *opt, const char *param);
int demux_info_add_bstr(struct demuxer *demuxer, struct bstr opt,
                        struct bstr param);
char *demux_info_get(struct demuxer *demuxer, const char *opt);
int demux_info_print(struct demuxer *demuxer);
int demux_control(struct demuxer *demuxer, int cmd, void *arg);

int demuxer_switch_audio(struct demuxer *demuxer, int index);
int demuxer_switch_video(struct demuxer *demuxer, int index);

int demuxer_type_by_filename(char *filename);

void demuxer_help(void);

int demuxer_add_attachment(struct demuxer *demuxer, struct bstr name,
                           struct bstr type, struct bstr data);
int demuxer_add_chapter(struct demuxer *demuxer, struct bstr name,
                        uint64_t start, uint64_t end);
int demuxer_seek_chapter(struct demuxer *demuxer, int chapter, double *seek_pts,
                         char **chapter_name);

/// Get current chapter index if available.
int demuxer_get_current_chapter(struct demuxer *demuxer, double time_now);
/// Get chapter name by index if available.
char *demuxer_chapter_name(struct demuxer *demuxer, int chapter);
/// Get chapter display name by index.
char *demuxer_chapter_display_name(struct demuxer *demuxer, int chapter);
/// Get chapter start time and end time by index if available.
float demuxer_chapter_time(struct demuxer *demuxer, int chapter, float *end);
/// Get total chapter number.
int demuxer_chapter_count(struct demuxer *demuxer);
/// Get current angle index.
int demuxer_get_current_angle(struct demuxer *demuxer);
/// Set angle.
int demuxer_set_angle(struct demuxer *demuxer, int angle);
/// Get number of angles.
int demuxer_angles_count(struct demuxer *demuxer);

/* Get the index of a track.
 * lang is a string list, NULL is same as empty list
 * Sort tracks based on the following criteria:
 * 1) earlier match in lang list, or last no match
 * 2) track is marked default (default wins)
 * 3) track number (lower wins)
 * For audio, select best track according to these criteria; only return -1
 * if there are no tracks at all.
 * For subs, select best track according to the same criteria, but return -1
 * if all tracks are no-lang-match, not-default.
 */
int demuxer_audio_track_by_lang_and_default(struct demuxer *d, char **langt);
int demuxer_sub_track_by_lang_and_default(struct demuxer *d, char **langt);

#endif /* MPLAYER_DEMUXER_H */
