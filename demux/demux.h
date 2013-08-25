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

#include "mpvcore/bstr.h"
#include "mpvcore/mp_common.h"
#include "demux_packet.h"
#include "stheader.h"

struct MPOpts;

#define MAX_PACKS 4096
#define MAX_PACK_BYTES 0x8000000  // 128 MiB

enum demuxer_type {
    DEMUXER_TYPE_GENERIC = 0,
    DEMUXER_TYPE_TV,
    DEMUXER_TYPE_MATROSKA,
    DEMUXER_TYPE_EDL,
    DEMUXER_TYPE_CUE,
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

#define DEMUXER_CTRL_UPDATE_INFO 8
#define DEMUXER_CTRL_SWITCHED_TRACKS 9
#define DEMUXER_CTRL_GET_TIME_LENGTH 10
#define DEMUXER_CTRL_GET_START_TIME 11
#define DEMUXER_CTRL_SWITCH_AUDIO 12
#define DEMUXER_CTRL_RESYNC 13
#define DEMUXER_CTRL_SWITCH_VIDEO 14
#define DEMUXER_CTRL_IDENTIFY_PROGRAM 15

#define SEEK_ABSOLUTE (1 << 0)
#define SEEK_FACTOR   (1 << 1)
#define SEEK_FORWARD  (1 << 2)
#define SEEK_BACKWARD (1 << 3)
#define SEEK_SUBPREROLL (1 << 4)

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

// demux_lavf can pass lavf buffers using FF_INPUT_BUFFER_PADDING_SIZE instead
#define MP_INPUT_BUFFER_PADDING_SIZE 16

#define MAX_SH_STREAMS 256

struct demuxer;

/**
 * Demuxer description structure
 */
typedef struct demuxer_desc {
    const char *name;      // Demuxer name, used with -demuxer switch
    const char *desc;      // Displayed to user

    enum demuxer_type type; // optional

    // Return 0 on success, otherwise -1
    int (*open)(struct demuxer *demuxer, enum demux_check check);
    // The following functions are all optional
    int (*fill_buffer)(struct demuxer *demuxer); // 0 on EOF, otherwise 1
    void (*close)(struct demuxer *demuxer);
    void (*seek)(struct demuxer *demuxer, float rel_seek_secs, int flags);
    int (*control)(struct demuxer *demuxer, int cmd, void *arg);
} demuxer_desc_t;

typedef struct demux_chapter
{
    int original_index;
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
    int matroska_wanted_segment;
    bool *matroska_was_valid;
    struct ass_library *ass_library;
};

typedef struct demuxer {
    const demuxer_desc_t *desc; ///< Demuxer description structure
    const char *filetype; // format name when not identified by demuxer (libavformat)
    int64_t filepos;  // input stream current pos.
    int64_t movi_start;
    int64_t movi_end;
    struct stream *stream;
    double stream_pts;     // current stream pts, if applicable (e.g. dvd)
    char *filename;  // same as stream->url
    enum demuxer_type type;
    int seekable; // flag
    /* Set if using absolute seeks for small movements is OK (no pts resets
     * that would make pts ambigious, preferably supports back/forward flags */
    bool accurate_seek;
    // File format allows PTS resets (even if the current file is without)
    bool ts_resets_possible;
    enum timestamp_type timestamp_type;
    bool warned_queue_overflow;

    struct sh_stream **streams;
    int num_streams;
    bool stream_autoselect;

    int num_editions;
    int edition;

    struct demux_chapter *chapters;
    int num_chapters;

    struct demux_attachment *attachments;
    int num_attachments;

    struct matroska_data matroska_data;
    // for trivial demuxers which just read the whole file for codec to use
    struct bstr file_contents;

    // If the file is a playlist file
    struct playlist *playlist;

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
struct demux_packet *new_demux_packet_from(void *data, size_t len);
void resize_demux_packet(struct demux_packet *dp, size_t len);
void free_demux_packet(struct demux_packet *dp);
struct demux_packet *demux_copy_packet(struct demux_packet *dp);

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

void free_demuxer(struct demuxer *demuxer);

int demuxer_add_packet(demuxer_t *demuxer, struct sh_stream *stream,
                       demux_packet_t *dp);

struct demux_packet *demux_read_packet(struct sh_stream *sh);
double demux_get_next_pts(struct sh_stream *sh);
bool demux_has_packet(struct sh_stream *sh);
bool demux_stream_eof(struct sh_stream *sh);

struct sh_stream *new_sh_stream(struct demuxer *demuxer, enum stream_type type);

struct demuxer *demux_open(struct stream *stream, char *force_format,
                           struct demuxer_params *params, struct MPOpts *opts);

void demux_flush(struct demuxer *demuxer);
int demux_seek(struct demuxer *demuxer, float rel_seek_secs, int flags);

int demux_info_add(struct demuxer *demuxer, const char *opt, const char *param);
int demux_info_add_bstr(struct demuxer *demuxer, struct bstr opt,
                        struct bstr param);
char *demux_info_get(struct demuxer *demuxer, const char *opt);
int demux_info_print(struct demuxer *demuxer);
void demux_info_update(struct demuxer *demuxer);

int demux_control(struct demuxer *demuxer, int cmd, void *arg);

void demuxer_switch_track(struct demuxer *demuxer, enum stream_type type,
                          struct sh_stream *stream);
void demuxer_select_track(struct demuxer *demuxer, struct sh_stream *stream,
                          bool selected);
void demuxer_enable_autoselect(struct demuxer *demuxer);

void demuxer_help(void);

int demuxer_add_attachment(struct demuxer *demuxer, struct bstr name,
                           struct bstr type, struct bstr data);
int demuxer_add_chapter(struct demuxer *demuxer, struct bstr name,
                        uint64_t start, uint64_t end);
int demuxer_seek_chapter(struct demuxer *demuxer, int chapter,
                         double *seek_pts);
void demuxer_sort_chapters(demuxer_t *demuxer);

double demuxer_get_time_length(struct demuxer *demuxer);
double demuxer_get_start_time(struct demuxer *demuxer);

/// Get current chapter index if available.
int demuxer_get_current_chapter(struct demuxer *demuxer, double time_now);
/// Get chapter name by index if available.
char *demuxer_chapter_name(struct demuxer *demuxer, int chapter);
/// Get chapter start time by index if available.
double demuxer_chapter_time(struct demuxer *demuxer, int chapter);
/// Get total chapter number.
int demuxer_chapter_count(struct demuxer *demuxer);
/// Get current angle index.
int demuxer_get_current_angle(struct demuxer *demuxer);
/// Set angle.
int demuxer_set_angle(struct demuxer *demuxer, int angle);
/// Get number of angles.
int demuxer_angles_count(struct demuxer *demuxer);

struct sh_stream *demuxer_stream_by_demuxer_id(struct demuxer *d,
                                               enum stream_type t, int id);

bool demuxer_stream_is_selected(struct demuxer *d, struct sh_stream *stream);
bool demuxer_stream_has_packets_queued(struct demuxer *d, struct sh_stream *stream);

void demux_packet_list_sort(struct demux_packet **pkts, int num_pkts);
void demux_packet_list_seek(struct demux_packet **pkts, int num_pkts,
                            int *current, float rel_seek_secs, int flags);
double demux_packet_list_duration(struct demux_packet **pkts, int num_pkts);
struct demux_packet *demux_packet_list_fill(struct demux_packet **pkts,
                                            int num_pkts, int *current);

#endif /* MPLAYER_DEMUXER_H */
