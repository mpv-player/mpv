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

#ifndef MPLAYER_DEMUXER_H
#define MPLAYER_DEMUXER_H

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "misc/bstr.h"
#include "common/common.h"
#include "common/tags.h"
#include "packet.h"
#include "stheader.h"

// Maximum total size of packets queued - if larger, no new packets are read,
// and the demuxer pretends EOF was reached.
#define MAX_PACKS 16000
#define MAX_PACK_BYTES (400 * 1024 * 1024)

// DEMUXER control commands/answers
#define DEMUXER_CTRL_NOTIMPL -1
#define DEMUXER_CTRL_DONTKNOW 0
#define DEMUXER_CTRL_OK 1

enum demux_ctrl {
    DEMUXER_CTRL_SWITCHED_TRACKS = 1,
    DEMUXER_CTRL_GET_TIME_LENGTH,
    DEMUXER_CTRL_RESYNC,
    DEMUXER_CTRL_IDENTIFY_PROGRAM,
    DEMUXER_CTRL_STREAM_CTRL,
    DEMUXER_CTRL_GET_READER_STATE,
    DEMUXER_CTRL_GET_NAV_EVENT,
    DEMUXER_CTRL_GET_BITRATE_STATS, // double[STREAM_TYPE_COUNT]
};

struct demux_ctrl_reader_state {
    bool eof, underrun, idle;
    double ts_range[2]; // start, end
    double ts_duration;
};

struct demux_ctrl_stream_ctrl {
    int ctrl;
    void *arg;
    int res;
};

#define SEEK_ABSOLUTE (1 << 0)      // argument is a timestamp
#define SEEK_FACTOR   (1 << 1)      // argument is in range [0,1]
#define SEEK_FORWARD  (1 << 2)      // prefer later time if not exact
#define SEEK_BACKWARD (1 << 3)      // prefer earlier time if not exact
#define SEEK_HR       (1 << 5)      // hr-seek (this is a weak hint only)

// Strictness of the demuxer open format check.
// demux.c will try by default: NORMAL, UNSAFE (in this order)
// Using "-demuxer format" will try REQUEST
// Using "-demuxer +format" will try FORCE
// REQUEST can be used as special value for raw demuxers which have no file
// header check; then they should fail if check!=FORCE && check!=REQUEST.
//
// In general, the list is sorted from weakest check to normal check.
// You can use relation operators to compare the check level.
enum demux_check {
    DEMUX_CHECK_FORCE,  // force format if possible
    DEMUX_CHECK_UNSAFE, // risky/fuzzy detection
    DEMUX_CHECK_REQUEST,// requested by user or stream implementation
    DEMUX_CHECK_NORMAL, // normal, safe detection
};

enum demux_event {
    DEMUX_EVENT_INIT = 1 << 0,      // complete (re-)initialization
    DEMUX_EVENT_STREAMS = 1 << 1,   // a stream was added
    DEMUX_EVENT_METADATA = 1 << 2,  // metadata or stream_metadata changed
    DEMUX_EVENT_ALL = 0xFFFF,
};

#define MAX_SH_STREAMS 256

struct demuxer;
struct timeline;

/**
 * Demuxer description structure
 */
typedef struct demuxer_desc {
    const char *name;      // Demuxer name, used with -demuxer switch
    const char *desc;      // Displayed to user

    // Return 0 on success, otherwise -1
    int (*open)(struct demuxer *demuxer, enum demux_check check);
    // The following functions are all optional
    int (*fill_buffer)(struct demuxer *demuxer); // 0 on EOF, otherwise 1
    void (*close)(struct demuxer *demuxer);
    void (*seek)(struct demuxer *demuxer, double rel_seek_secs, int flags);
    int (*control)(struct demuxer *demuxer, int cmd, void *arg);
    // See timeline.c
    void (*load_timeline)(struct timeline *tl);
} demuxer_desc_t;

typedef struct demux_chapter
{
    int original_index;
    double pts;
    char *name;
    struct mp_tags *metadata;
    uint64_t demuxer_id; // for mapping to internal demuxer data structures
} demux_chapter_t;

struct demux_edition {
    uint64_t demuxer_id;
    bool default_edition;
    struct mp_tags *metadata;
};

struct matroska_segment_uid {
    unsigned char segment[16];
    uint64_t edition;
};

struct matroska_data {
    struct matroska_segment_uid uid;
    // Ordered chapter information if any
    struct matroska_chapter {
        uint64_t start;
        uint64_t end;
        bool has_segment_uid;
        struct matroska_segment_uid uid;
        char *name;
    } *ordered_chapters;
    int num_ordered_chapters;
};

struct replaygain_data {
    float track_gain;
    float track_peak;
    float album_gain;
    float album_peak;
};

typedef struct demux_attachment
{
    char *name;
    char *type;
    void *data;
    unsigned int data_size;
} demux_attachment_t;

struct demuxer_params {
    char *force_format;
    int matroska_num_wanted_uids;
    struct matroska_segment_uid *matroska_wanted_uids;
    int matroska_wanted_segment;
    bool *matroska_was_valid;
    bool expect_subtitle;
    bool disable_cache; // demux_open_url() only
};

typedef struct demuxer {
    const demuxer_desc_t *desc; ///< Demuxer description structure
    const char *filetype; // format name when not identified by demuxer (libavformat)
    int64_t filepos;  // input stream current pos.
    char *filename;  // same as stream->url
    bool seekable;
    bool partially_seekable; // implies seekable=true
    double start_time;
    // File format allows PTS resets (even if the current file is without)
    bool ts_resets_possible;
    // Send relative seek requests, instead of SEEK_ABSOLUTE or SEEK_FACTOR.
    // This is only done if the user explicitly uses a relative seek.
    bool rel_seeks;
    // Enable fast track switching hacks. This requires from the demuxer:
    // - seeking is somewhat reliable; packet contents must not change
    // - packet position (demux_packet.pos) is set, not negative, unique, and
    //   monotonically increasing
    // - seeking leaves packet positions invariant
    bool allow_refresh_seeks;
    // The file data was fully read, and there is no need to keep the stream
    // open, keep the cache active, or to run the demuxer thread. Generating
    // packets is not slow either (unlike e.g. libavdevice pseudo-demuxers).
    // Typical examples: text subtitles, playlists
    bool fully_read;

    // Bitmask of DEMUX_EVENT_*
    int events;

    struct sh_stream **streams;
    int num_streams;

    struct demux_edition *editions;
    int num_editions;
    int edition;

    struct demux_chapter *chapters;
    int num_chapters;

    struct demux_attachment *attachments;
    int num_attachments;

    struct matroska_data matroska_data;

    // If the file is a playlist file
    struct playlist *playlist;

    struct mp_tags *metadata;

    void *priv;   // demuxer-specific internal data
    struct MPOpts *opts;
    struct mpv_global *global;
    struct mp_log *log, *glog;
    struct demuxer_params *params;

    struct demux_internal *in; // internal to demux.c

    // Since the demuxer can run in its own thread, and the stream is not
    // thread-safe, only the demuxer is allowed to access the stream directly.
    // You can freely use demux_stream_control() to send STREAM_CTRLs, or use
    // demux_pause() to get exclusive access to the stream.
    struct stream *stream;
} demuxer_t;

typedef struct {
    int progid;      //program id
    int aid, vid, sid; //audio, video and subtitle id
} demux_program_t;

void free_demuxer(struct demuxer *demuxer);
void free_demuxer_and_stream(struct demuxer *demuxer);

int demux_add_packet(struct sh_stream *stream, demux_packet_t *dp);

struct demux_packet *demux_read_packet(struct sh_stream *sh);
int demux_read_packet_async(struct sh_stream *sh, struct demux_packet **out_pkt);
bool demux_stream_is_selected(struct sh_stream *stream);
double demux_get_next_pts(struct sh_stream *sh);
bool demux_has_packet(struct sh_stream *sh);
struct demux_packet *demux_read_any_packet(struct demuxer *demuxer);

struct sh_stream *new_sh_stream(struct demuxer *demuxer, enum stream_type type);

struct demuxer *demux_open(struct stream *stream, struct demuxer_params *params,
                           struct mpv_global *global);

struct mp_cancel;
struct demuxer *demux_open_url(const char *url,
                               struct demuxer_params *params,
                               struct mp_cancel *cancel,
                               struct mpv_global *global);

void demux_start_thread(struct demuxer *demuxer);
void demux_stop_thread(struct demuxer *demuxer);
void demux_set_wakeup_cb(struct demuxer *demuxer, void (*cb)(void *ctx), void *ctx);

bool demux_cancel_test(struct demuxer *demuxer);

void demux_flush(struct demuxer *demuxer);
int demux_seek(struct demuxer *demuxer, double rel_seek_secs, int flags);
void demux_set_enable_refresh_seeks(struct demuxer *demuxer, bool enabled);

int demux_control(struct demuxer *demuxer, int cmd, void *arg);

void demuxer_switch_track(struct demuxer *demuxer, enum stream_type type,
                          struct sh_stream *stream);
void demuxer_select_track(struct demuxer *demuxer, struct sh_stream *stream,
                          bool selected);
void demux_set_stream_autoselect(struct demuxer *demuxer, bool autoselect);

void demuxer_help(struct mp_log *log);

int demuxer_add_attachment(struct demuxer *demuxer, struct bstr name,
                           struct bstr type, struct bstr data);
int demuxer_add_chapter(demuxer_t *demuxer, struct bstr name,
                        double pts, uint64_t demuxer_id);

double demuxer_get_time_length(struct demuxer *demuxer);

int demux_stream_control(demuxer_t *demuxer, int ctrl, void *arg);

void demux_pause(demuxer_t *demuxer);
void demux_unpause(demuxer_t *demuxer);

void demux_changed(demuxer_t *demuxer, int events);
void demux_update(demuxer_t *demuxer);

struct sh_stream *demuxer_stream_by_demuxer_id(struct demuxer *d,
                                               enum stream_type t, int id);

struct demux_chapter *demux_copy_chapter_data(struct demux_chapter *c, int num);

bool demux_matroska_uid_cmp(struct matroska_segment_uid *a,
                            struct matroska_segment_uid *b);

const char *stream_type_name(enum stream_type type);

#endif /* MPLAYER_DEMUXER_H */
