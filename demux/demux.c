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

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>

#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "cache.h"
#include "config.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "mpv_talloc.h"
#include "common/av_common.h"
#include "common/msg.h"
#include "common/global.h"
#include "common/recorder.h"
#include "common/stats.h"
#include "misc/charset_conv.h"
#include "misc/thread_tools.h"
#include "osdep/atomic.h"
#include "osdep/timer.h"
#include "osdep/threads.h"

#include "stream/stream.h"
#include "demux.h"
#include "timeline.h"
#include "stheader.h"
#include "cue.h"

// Demuxer list
extern const struct demuxer_desc demuxer_desc_edl;
extern const struct demuxer_desc demuxer_desc_cue;
extern const demuxer_desc_t demuxer_desc_rawaudio;
extern const demuxer_desc_t demuxer_desc_rawvideo;
extern const demuxer_desc_t demuxer_desc_mf;
extern const demuxer_desc_t demuxer_desc_matroska;
extern const demuxer_desc_t demuxer_desc_lavf;
extern const demuxer_desc_t demuxer_desc_playlist;
extern const demuxer_desc_t demuxer_desc_disc;
extern const demuxer_desc_t demuxer_desc_rar;
extern const demuxer_desc_t demuxer_desc_libarchive;
extern const demuxer_desc_t demuxer_desc_null;
extern const demuxer_desc_t demuxer_desc_timeline;

static const demuxer_desc_t *const demuxer_list[] = {
    &demuxer_desc_disc,
    &demuxer_desc_edl,
    &demuxer_desc_cue,
    &demuxer_desc_rawaudio,
    &demuxer_desc_rawvideo,
    &demuxer_desc_matroska,
#if HAVE_LIBARCHIVE
    &demuxer_desc_libarchive,
#endif
    &demuxer_desc_lavf,
    &demuxer_desc_mf,
    &demuxer_desc_playlist,
    &demuxer_desc_null,
    NULL
};

struct demux_opts {
    int enable_cache;
    int disk_cache;
    int64_t max_bytes;
    int64_t max_bytes_bw;
    int donate_fw;
    double min_secs;
    int force_seekable;
    double min_secs_cache;
    int access_references;
    int seekable_cache;
    int create_ccs;
    char *record_file;
    int video_back_preroll;
    int audio_back_preroll;
    int back_batch[STREAM_TYPE_COUNT];
    double back_seek_size;
    char *meta_cp;
    int force_retry_eof;
};

#define OPT_BASE_STRUCT struct demux_opts

static bool get_demux_sub_opts(int index, const struct m_sub_options **sub);

const struct m_sub_options demux_conf = {
    .opts = (const struct m_option[]){
        {"cache", OPT_CHOICE(enable_cache,
            {"no", 0}, {"auto", -1}, {"yes", 1})},
        {"cache-on-disk", OPT_FLAG(disk_cache)},
        {"demuxer-readahead-secs", OPT_DOUBLE(min_secs), M_RANGE(0, DBL_MAX)},
        {"demuxer-max-bytes", OPT_BYTE_SIZE(max_bytes),
            M_RANGE(0, M_MAX_MEM_BYTES)},
        {"demuxer-max-back-bytes", OPT_BYTE_SIZE(max_bytes_bw),
            M_RANGE(0, M_MAX_MEM_BYTES)},
        {"demuxer-donate-buffer", OPT_FLAG(donate_fw)},
        {"force-seekable", OPT_FLAG(force_seekable)},
        {"cache-secs", OPT_DOUBLE(min_secs_cache), M_RANGE(0, DBL_MAX)},
        {"access-references", OPT_FLAG(access_references)},
        {"demuxer-seekable-cache", OPT_CHOICE(seekable_cache,
            {"auto", -1}, {"no", 0}, {"yes", 1})},
        {"sub-create-cc-track", OPT_FLAG(create_ccs)},
        {"stream-record", OPT_STRING(record_file)},
        {"video-backward-overlap", OPT_CHOICE(video_back_preroll, {"auto", -1}),
            M_RANGE(0, 1024)},
        {"audio-backward-overlap", OPT_CHOICE(audio_back_preroll, {"auto", -1}),
            M_RANGE(0, 1024)},
        {"video-backward-batch", OPT_INT(back_batch[STREAM_VIDEO]),
            M_RANGE(0, 1024)},
        {"audio-backward-batch", OPT_INT(back_batch[STREAM_AUDIO]),
            M_RANGE(0, 1024)},
        {"demuxer-backward-playback-step", OPT_DOUBLE(back_seek_size),
            M_RANGE(0, DBL_MAX)},
        {"metadata-codepage", OPT_STRING(meta_cp)},
        {"demuxer-force-retry-on-eof", OPT_FLAG(force_retry_eof),
         .deprecation_message = "temporary debug option, no replacement"},
        {0}
    },
    .size = sizeof(struct demux_opts),
    .defaults = &(const struct demux_opts){
        .enable_cache = -1, // auto
        .max_bytes = 150 * 1024 * 1024,
        .max_bytes_bw = 50 * 1024 * 1024,
        .donate_fw = 1,
        .min_secs = 1.0,
        .min_secs_cache = 1000.0 * 60 * 60,
        .seekable_cache = -1,
        .access_references = 1,
        .video_back_preroll = -1,
        .audio_back_preroll = -1,
        .back_seek_size = 60,
        .back_batch = {
            [STREAM_VIDEO] = 1,
            [STREAM_AUDIO] = 10,
        },
        .meta_cp = "utf-8",
    },
    .get_sub_options = get_demux_sub_opts,
};

struct demux_internal {
    struct mp_log *log;
    struct mpv_global *global;
    struct stats_ctx *stats;

    bool can_cache;             // not a slave demuxer; caching makes sense
    bool can_record;            // stream recording is allowed

    // The demuxer runs potentially in another thread, so we keep two demuxer
    // structs; the real demuxer can access the shadow struct only.
    struct demuxer *d_thread;   // accessed by demuxer impl. (producer)
    struct demuxer *d_user;     // accessed by player (consumer)

    // The lock protects the packet queues (struct demux_stream),
    // and the fields below.
    pthread_mutex_t lock;
    pthread_cond_t wakeup;
    pthread_t thread;

    // -- All the following fields are protected by lock.

    struct demux_opts *opts;
    struct m_config_cache *opts_cache;

    bool thread_terminate;
    bool threading;
    bool shutdown_async;
    void (*wakeup_cb)(void *ctx);
    void *wakeup_cb_ctx;

    struct sh_stream **streams;
    int num_streams;

    char *meta_charset;

    // If non-NULL, a stream which is used for global (timed) metadata. It will
    // be an arbitrary stream, which hopefully will happen to work.
    struct sh_stream *metadata_stream;

    int events;

    struct demux_cache *cache;

    bool warned_queue_overflow;
    bool eof;                   // whether we're in EOF state
    double min_secs;
    size_t max_bytes;
    size_t max_bytes_bw;
    bool seekable_cache;
    bool using_network_cache_opts;
    char *record_filename;

    // Whether the demuxer thread should prefetch packets. This is set to false
    // if EOF was reached or the demuxer cache is full. This is also important
    // in the initial state: the decoder thread needs to select streams before
    // the first packet is read, so this is set to true by packet reading only.
    // Reset to false again on EOF or if prefetching is done.
    bool reading;

    // Set if we just performed a seek, without reading packets yet. Used to
    // avoid a redundant initial seek after enabling streams. We could just
    // allow it, but to avoid buggy seeking affecting normal playback, we don't.
    bool after_seek;
    // Set in addition to after_seek if we think we seeked to the start of the
    // file (or if the demuxer was just opened).
    bool after_seek_to_start;

    // Demuxing backwards. Since demuxer implementations don't support this
    // directly, it is emulated by seeking backwards for every packet run. Also,
    // packets between keyframes are demuxed forwards (you can't decode that
    // stuff otherwise), which adds complexity on top of it.
    bool back_demuxing;

    // For backward demuxing:
    bool need_back_seek;        // back-step seek needs to be triggered
    bool back_any_need_recheck; // at least 1 ds->back_need_recheck set

    bool tracks_switched;       // thread needs to inform demuxer of this

    bool seeking;               // there's a seek queued
    int seek_flags;             // flags for next seek (if seeking==true)
    double seek_pts;

    // (fields for debugging)
    double seeking_in_progress; // low level seek state
    int low_level_seeks;        // number of started low level seeks
    double demux_ts;            // last demuxed DTS or PTS

    double ts_offset;           // timestamp offset to apply to everything

    // (sorted by least recent use: index 0 is least recently used)
    struct demux_cached_range **ranges;
    int num_ranges;

    size_t total_bytes;         // total sum of packet data buffered
    // Range from which decoder is reading, and to which demuxer is appending.
    // This is normally never NULL. This is always ranges[num_ranges - 1].
    // This is can be NULL during initialization or deinitialization.
    struct demux_cached_range *current_range;

    double highest_av_pts;      // highest non-subtitle PTS seen - for duration

    bool blocked;

    // Transient state.
    double duration;
    // Cached state.
    int64_t stream_size;
    int64_t last_speed_query;
    double speed_query_prev_sample;
    uint64_t bytes_per_second;
    int64_t next_cache_update;

    // demux user state (user thread, somewhat similar to reader/decoder state)
    double last_playback_pts;   // last playback_pts from demux_update()
    bool force_metadata_update;
    int cached_metadata_index;  // speed up repeated lookups

    struct mp_recorder *dumper;
    int dumper_status;

    bool owns_stream;

    // -- Access from demuxer thread only
    bool enable_recording;
    struct mp_recorder *recorder;
    int64_t slave_unbuffered_read_bytes; // value repoted from demuxer impl.
    int64_t hack_unbuffered_read_bytes;  // for demux_get_bytes_read_hack()
    int64_t cache_unbuffered_read_bytes; // for demux_reader_state.bytes_per_second
    int64_t byte_level_seeks;            // for demux_reader_state.byte_level_seeks
};

struct timed_metadata {
    double pts;
    struct mp_tags *tags;
    bool from_stream;
};

// A continuous range of cached packets for all enabled streams.
// (One demux_queue for each known stream.)
struct demux_cached_range {
    // streams[] is indexed by demux_stream->index
    struct demux_queue **streams;
    int num_streams;

    // Computed from the stream queue's values. These fields (unlike as with
    // demux_queue) are always either NOPTS, or fully valid.
    double seek_start, seek_end;

    bool is_bof;            // set if the file begins with this range
    bool is_eof;            // set if the file ends with this range

    struct timed_metadata **metadata;
    int num_metadata;
};

#define QUEUE_INDEX_SIZE_MASK(queue) ((queue)->index_size - 1)

// Access the idx-th entry in the given demux_queue.
// Requirement: idx >= 0 && idx < queue->num_index
#define QUEUE_INDEX_ENTRY(queue, idx) \
    ((queue)->index[((queue)->index0 + (idx)) & QUEUE_INDEX_SIZE_MASK(queue)])

// Don't index packets whose timestamps that are within the last index entry by
// this amount of time (it's better to seek them manually).
#define INDEX_STEP_SIZE 1.0

struct index_entry {
    double pts;
    struct demux_packet *pkt;
};

// A continuous list of cached packets for a single stream/range. There is one
// for each stream and range. Also contains some state for use during demuxing
// (keeping it across seeks makes it easier to resume demuxing).
struct demux_queue {
    struct demux_stream *ds;
    struct demux_cached_range *range;

    struct demux_packet *head;
    struct demux_packet *tail;

    uint64_t tail_cum_pos;  // cumulative size including tail packet

    bool correct_dts;       // packet DTS is strictly monotonically increasing
    bool correct_pos;       // packet pos is strictly monotonically increasing
    int64_t last_pos;       // for determining correct_pos
    int64_t last_pos_fixup; // for filling in unset dp->pos values
    double last_dts;        // for determining correct_dts
    double last_ts;         // timestamp of the last packet added to queue

    // for incrementally determining seek PTS range
    struct demux_packet *keyframe_latest;
    struct demux_packet *keyframe_first; // cached value of first KF packet

    // incrementally maintained seek range, possibly invalid
    double seek_start, seek_end;
    double last_pruned;     // timestamp of last pruned keyframe

    bool is_bof;            // started demuxing at beginning of file
    bool is_eof;            // received true EOF here

    // Complete index, though it may skip some entries to reduce density.
    struct index_entry *index;  // ring buffer
    size_t index_size;          // size of index[] (0 or a power of 2)
    size_t index0;              // first index entry
    size_t num_index;           // number of index entries (wraps on index_size)
};

struct demux_stream {
    struct demux_internal *in;
    struct sh_stream *sh;   // ds->sh->ds == ds
    enum stream_type type;  // equals to sh->type
    int index;              // equals to sh->index
    // --- all fields are protected by in->lock

    void (*wakeup_cb)(void *ctx);
    void *wakeup_cb_ctx;

    // demuxer state
    bool selected;          // user wants packets from this stream
    bool eager;             // try to keep at least 1 packet queued
                            // if false, this stream is disabled, or passively
                            // read (like subtitles)
    bool still_image;       // stream has still video images
    bool refreshing;        // finding old position after track switches
    bool eof;               // end of demuxed stream? (true if no more packets)

    bool global_correct_dts;// all observed so far
    bool global_correct_pos;

    // current queue - used both for reading and demuxing (this is never NULL)
    struct demux_queue *queue;

    // reader (decoder) state (bitrate calculations are part of it because we
    // want to return the bitrate closest to the "current position")
    double base_ts;         // timestamp of the last packet returned to decoder
    double last_br_ts;      // timestamp of last packet bitrate was calculated
    size_t last_br_bytes;   // summed packet sizes since last bitrate calculation
    double bitrate;
    struct demux_packet *reader_head;   // points at current decoder position
    bool skip_to_keyframe;
    bool attached_picture_added;
    bool need_wakeup;       // call wakeup_cb on next reader_head state change
    double force_read_until;// eager=false streams (subs): force read-ahead

    // For demux_internal.dumper. Currently, this is used only temporarily
    // during blocking dumping.
    struct demux_packet *dump_pos;

    // for refresh seeks: pos/dts of last packet returned to reader
    int64_t last_ret_pos;
    double last_ret_dts;

    // Backwards demuxing.
    bool back_need_recheck; // flag for incremental find_backward_restart_pos work
    // pos/dts of the previous keyframe packet returned; always valid if back-
    // demuxing is enabled, and back_restart_eof/back_restart_next are false.
    int64_t back_restart_pos;
    double back_restart_dts;
    bool back_restart_eof; // restart position is at EOF; overrides pos/dts
    bool back_restart_next; // restart before next keyframe; overrides above
    bool back_restarting;   // searching keyframe before restart pos
    // Current PTS lower bound for back demuxing.
    double back_seek_pos;
    // pos/dts of the packet to resume demuxing from when another stream caused
    // a seek backward to get more packets. reader_head will be reset to this
    // packet as soon as it's encountered again.
    int64_t back_resume_pos;
    double back_resume_dts;
    bool back_resuming;     // resuming mode (above fields are valid/used)
    // Set to true if the first packet (keyframe) of a range was returned.
    bool back_range_started;
    // Number of KF packets at start of range yet to return. -1 is used for BOF.
    int back_range_count;
    // Number of KF packets yet to return that are marked as preroll.
    int back_range_preroll;
    // Static packet preroll count.
    int back_preroll;

    // for closed captions (demuxer_feed_caption)
    struct sh_stream *cc;
    bool ignore_eof;        // ignore stream in underrun detection
};

static void switch_to_fresh_cache_range(struct demux_internal *in);
static void demuxer_sort_chapters(demuxer_t *demuxer);
static void *demux_thread(void *pctx);
static void update_cache(struct demux_internal *in);
static void add_packet_locked(struct sh_stream *stream, demux_packet_t *dp);
static struct demux_packet *advance_reader_head(struct demux_stream *ds);
static bool queue_seek(struct demux_internal *in, double seek_pts, int flags,
                       bool clear_back_state);
static struct demux_packet *compute_keyframe_times(struct demux_packet *pkt,
                                                   double *out_kf_min,
                                                   double *out_kf_max);
static void find_backward_restart_pos(struct demux_stream *ds);
static struct demux_packet *find_seek_target(struct demux_queue *queue,
                                             double pts, int flags);
static void prune_old_packets(struct demux_internal *in);
static void dumper_close(struct demux_internal *in);
static void demux_convert_tags_charset(struct demuxer *demuxer);

static uint64_t get_foward_buffered_bytes(struct demux_stream *ds)
{
    if (!ds->reader_head)
        return 0;
    return ds->queue->tail_cum_pos - ds->reader_head->cum_pos;
}

#if 0
// very expensive check for redundant cached queue state
static void check_queue_consistency(struct demux_internal *in)
{
    uint64_t total_bytes = 0;

    assert(in->current_range && in->num_ranges > 0);
    assert(in->current_range == in->ranges[in->num_ranges - 1]);

    for (int n = 0; n < in->num_ranges; n++) {
        struct demux_cached_range *range = in->ranges[n];
        int range_num_packets = 0;

        assert(range->num_streams == in->num_streams);

        for (int i = 0; i < range->num_streams; i++) {
            struct demux_queue *queue = range->streams[i];

            assert(queue->range == range);

            size_t fw_bytes = 0;
            bool is_forward = false;
            bool kf_found = false;
            bool kf1_found = false;
            size_t next_index = 0;
            uint64_t queue_total_bytes = 0;
            for (struct demux_packet *dp = queue->head; dp; dp = dp->next) {
                is_forward |= dp == queue->ds->reader_head;
                kf_found |= dp == queue->keyframe_latest;
                kf1_found |= dp == queue->keyframe_first;

                size_t bytes = demux_packet_estimate_total_size(dp);
                total_bytes += bytes;
                queue_total_bytes += bytes;
                if (is_forward) {
                    fw_bytes += bytes;
                    assert(range == in->current_range);
                    assert(queue->ds->queue == queue);
                }
                range_num_packets += 1;

                if (!dp->next)
                    assert(queue->tail == dp);

                if (next_index < queue->num_index &&
                    QUEUE_INDEX_ENTRY(queue, next_index).pkt == dp)
                    next_index += 1;
            }
            if (!queue->head)
                assert(!queue->tail);
            assert(next_index == queue->num_index);

            uint64_t queue_total_bytes2 = 0;
            if (queue->head)
                queue_total_bytes2 = queue->tail_cum_pos - queue->head->cum_pos;

            assert(queue_total_bytes == queue_total_bytes2);

            // If the queue is currently used...
            if (queue->ds->queue == queue) {
                // ...reader_head and others must be in the queue.
                assert(is_forward == !!queue->ds->reader_head);
                assert(kf_found == !!queue->keyframe_latest);
                uint64_t fw_bytes2 = get_foward_buffered_bytes(queue->ds);
                assert(fw_bytes == fw_bytes2);
            }

            assert(kf1_found == !!queue->keyframe_first);

            if (range != in->current_range) {
                assert(fw_bytes == 0);
            }

            if (queue->keyframe_latest)
                assert(queue->keyframe_latest->keyframe);

            total_bytes += queue->index_size * sizeof(struct index_entry);
        }

        // Invariant needed by pruning; violation has worse effects than just
        // e.g. broken seeking due to incorrect seek ranges.
        if (range->seek_start != MP_NOPTS_VALUE)
            assert(range_num_packets > 0);
    }

    assert(in->total_bytes == total_bytes);
}
#endif

// (this doesn't do most required things for a switch, like updating ds->queue)
static void set_current_range(struct demux_internal *in,
                              struct demux_cached_range *range)
{
    in->current_range = range;

    // Move to in->ranges[in->num_ranges-1] (for LRU sorting/invariant)
    for (int n = 0; n < in->num_ranges; n++) {
        if (in->ranges[n] == range) {
            MP_TARRAY_REMOVE_AT(in->ranges, in->num_ranges, n);
            break;
        }
    }
    MP_TARRAY_APPEND(in, in->ranges, in->num_ranges, range);
}

static void prune_metadata(struct demux_cached_range *range)
{
    int first_needed = 0;

    if (range->seek_start == MP_NOPTS_VALUE) {
        first_needed = range->num_metadata;
    } else {
        for (int n = 0; n < range->num_metadata ; n++) {
            if (range->metadata[n]->pts > range->seek_start)
                break;
            first_needed = n;
        }
    }

    // Always preserve the last entry.
    first_needed = MPMIN(first_needed, range->num_metadata - 1);

    // (Could make this significantly more efficient for large first_needed,
    // however that might be very rare and even then it might not matter.)
    for (int n = 0; n < first_needed; n++) {
        talloc_free(range->metadata[0]);
        MP_TARRAY_REMOVE_AT(range->metadata, range->num_metadata, 0);
    }
}

// Refresh range->seek_start/end. Idempotent.
static void update_seek_ranges(struct demux_cached_range *range)
{
    range->seek_start = range->seek_end = MP_NOPTS_VALUE;
    range->is_bof = true;
    range->is_eof = true;

    double min_start_pts = MP_NOPTS_VALUE;
    double max_end_pts = MP_NOPTS_VALUE;

    for (int n = 0; n < range->num_streams; n++) {
        struct demux_queue *queue = range->streams[n];

        if (queue->ds->selected && queue->ds->eager) {
            if (queue->is_bof) {
                min_start_pts = MP_PTS_MIN(min_start_pts, queue->seek_start);
            } else {
                range->seek_start =
                    MP_PTS_MAX(range->seek_start, queue->seek_start);
            }

            if (queue->is_eof) {
                max_end_pts = MP_PTS_MAX(max_end_pts, queue->seek_end);
            } else {
                range->seek_end = MP_PTS_MIN(range->seek_end, queue->seek_end);
            }

            range->is_eof &= queue->is_eof;
            range->is_bof &= queue->is_bof;

            bool empty = queue->is_eof && !queue->head;
            if (queue->seek_start >= queue->seek_end && !empty &&
                !(queue->seek_start == queue->seek_end &&
                  queue->seek_start != MP_NOPTS_VALUE))
                goto broken;
        }
    }

    if (range->is_eof)
        range->seek_end = max_end_pts;
    if (range->is_bof)
        range->seek_start = min_start_pts;

    // Sparse (subtitle) stream behavior is not very clearly defined, but
    // usually we don't want it to restrict the range of other streams. For
    // example, if there are subtitle packets at position 5 and 10 seconds, and
    // the demuxer demuxed the other streams until position 7 seconds, the seek
    // range end position is 7.
    // Assume that reading a non-sparse (audio/video) packet gets all sparse
    // packets that are needed before that non-sparse packet.
    // This is incorrect in any of these cases:
    //  - sparse streams only (it's unknown how to determine an accurate range)
    //  - if sparse streams have non-keyframe packets (we set queue->last_pruned
    //    to the start of the pruned keyframe range - we'd need the end or so)
    // We also assume that ds->eager equals to a stream not being sparse
    // (usually true, except if only sparse streams are selected).
    // We also rely on the fact that the demuxer position will always be ahead
    // of the seek_end for audio/video, because they need to prefetch at least
    // 1 packet to detect the end of a keyframe range. This means that there's
    // a relatively high guarantee to have all sparse (subtitle) packets within
    // the seekable range.
    // As a consequence, the code _never_ checks queue->seek_end for a sparse
    // queue, as the end of it is implied by the highest PTS of a non-sparse
    // stream (i.e. the latest demuxer position).
    // On the other hand, if a sparse packet was pruned, and that packet has
    // a higher PTS than seek_start for non-sparse queues, that packet is
    // missing. So the range's seek_start needs to be adjusted accordingly.
    for (int n = 0; n < range->num_streams; n++) {
        struct demux_queue *queue = range->streams[n];
        if (queue->ds->selected && !queue->ds->eager &&
            queue->last_pruned != MP_NOPTS_VALUE &&
            range->seek_start != MP_NOPTS_VALUE)
        {
            // (last_pruned is _exclusive_ to the seekable range, so add a small
            // value to exclude it from the valid range.)
            range->seek_start =
                MP_PTS_MAX(range->seek_start, queue->last_pruned + 0.1);
        }
    }

    if (range->seek_start >= range->seek_end)
        goto broken;

    prune_metadata(range);
    return;

broken:
    range->seek_start = range->seek_end = MP_NOPTS_VALUE;
    prune_metadata(range);
}

// Remove queue->head from the queue.
static void remove_head_packet(struct demux_queue *queue)
{
    struct demux_packet *dp = queue->head;

    assert(queue->ds->reader_head != dp);
    if (queue->keyframe_first == dp)
        queue->keyframe_first = NULL;
    if (queue->keyframe_latest == dp)
        queue->keyframe_latest = NULL;
    queue->is_bof = false;

    uint64_t end_pos = dp->next ? dp->next->cum_pos : queue->tail_cum_pos;
    queue->ds->in->total_bytes -= end_pos - dp->cum_pos;

    if (queue->num_index && queue->index[queue->index0].pkt == dp) {
        queue->index0 = (queue->index0 + 1) & QUEUE_INDEX_SIZE_MASK(queue);
        queue->num_index -= 1;
    }

    queue->head = dp->next;
    if (!queue->head)
        queue->tail = NULL;

    talloc_free(dp);
}

static void free_index(struct demux_queue *queue)
{
    struct demux_stream *ds = queue->ds;
    struct demux_internal *in = ds->in;

    in->total_bytes -= queue->index_size * sizeof(queue->index[0]);
    queue->index_size = 0;
    queue->index0 = 0;
    queue->num_index = 0;
    TA_FREEP(&queue->index);
}

static void clear_queue(struct demux_queue *queue)
{
    struct demux_stream *ds = queue->ds;
    struct demux_internal *in = ds->in;

    if (queue->head)
        in->total_bytes -= queue->tail_cum_pos - queue->head->cum_pos;

    free_index(queue);

    struct demux_packet *dp = queue->head;
    while (dp) {
        struct demux_packet *dn = dp->next;
        assert(ds->reader_head != dp);
        talloc_free(dp);
        dp = dn;
    }
    queue->head = queue->tail = NULL;
    queue->keyframe_first = NULL;
    queue->keyframe_latest = NULL;
    queue->seek_start = queue->seek_end = queue->last_pruned = MP_NOPTS_VALUE;

    queue->correct_dts = queue->correct_pos = true;
    queue->last_pos = -1;
    queue->last_ts = queue->last_dts = MP_NOPTS_VALUE;
    queue->last_pos_fixup = -1;

    queue->is_eof = false;
    queue->is_bof = false;
}

static void clear_cached_range(struct demux_internal *in,
                               struct demux_cached_range *range)
{
    for (int n = 0; n < range->num_streams; n++)
        clear_queue(range->streams[n]);

    for (int n = 0; n < range->num_metadata; n++)
        talloc_free(range->metadata[n]);
    range->num_metadata = 0;

    update_seek_ranges(range);
}

// Remove ranges with no data (except in->current_range). Also remove excessive
// ranges.
static void free_empty_cached_ranges(struct demux_internal *in)
{
    while (1) {
        struct demux_cached_range *worst = NULL;

        int end = in->num_ranges - 1;

        // (Not set during early init or late destruction.)
        if (in->current_range) {
            assert(in->current_range && in->num_ranges > 0);
            assert(in->current_range == in->ranges[in->num_ranges - 1]);
            end -= 1;
        }

        for (int n = end; n >= 0; n--) {
            struct demux_cached_range *range = in->ranges[n];
            if (range->seek_start == MP_NOPTS_VALUE || !in->seekable_cache) {
                clear_cached_range(in, range);
                MP_TARRAY_REMOVE_AT(in->ranges, in->num_ranges, n);
                for (int i = 0; i < range->num_streams; i++)
                    talloc_free(range->streams[i]);
                talloc_free(range);
            } else {
                if (!worst || (range->seek_end - range->seek_start <
                               worst->seek_end - worst->seek_start))
                    worst = range;
            }
        }

        if (in->num_ranges <= MAX_SEEK_RANGES || !worst)
            break;

        clear_cached_range(in, worst);
    }
}

static void ds_clear_reader_queue_state(struct demux_stream *ds)
{
    ds->reader_head = NULL;
    ds->eof = false;
    ds->need_wakeup = true;
}

static void ds_clear_reader_state(struct demux_stream *ds,
                                  bool clear_back_state)
{
    ds_clear_reader_queue_state(ds);

    ds->base_ts = ds->last_br_ts = MP_NOPTS_VALUE;
    ds->last_br_bytes = 0;
    ds->bitrate = -1;
    ds->skip_to_keyframe = false;
    ds->attached_picture_added = false;
    ds->last_ret_pos = -1;
    ds->last_ret_dts = MP_NOPTS_VALUE;
    ds->force_read_until = MP_NOPTS_VALUE;

    if (clear_back_state) {
        ds->back_restart_pos = -1;
        ds->back_restart_dts = MP_NOPTS_VALUE;
        ds->back_restart_eof = false;
        ds->back_restart_next = ds->in->back_demuxing;
        ds->back_restarting = ds->in->back_demuxing && ds->eager;
        ds->back_seek_pos = MP_NOPTS_VALUE;
        ds->back_resume_pos = -1;
        ds->back_resume_dts = MP_NOPTS_VALUE;
        ds->back_resuming = false;
        ds->back_range_started = false;
        ds->back_range_count = 0;
        ds->back_range_preroll = 0;
    }
}

// called locked, from user thread only
static void clear_reader_state(struct demux_internal *in,
                               bool clear_back_state)
{
    for (int n = 0; n < in->num_streams; n++)
        ds_clear_reader_state(in->streams[n]->ds, clear_back_state);
    in->warned_queue_overflow = false;
    in->d_user->filepos = -1; // implicitly synchronized
    in->blocked = false;
    in->need_back_seek = false;
}

// Call if the observed reader state on this stream somehow changes. The wakeup
// is skipped if the reader successfully read a packet, because that means we
// expect it to come back and ask for more.
static void wakeup_ds(struct demux_stream *ds)
{
    if (ds->need_wakeup) {
        if (ds->wakeup_cb) {
            ds->wakeup_cb(ds->wakeup_cb_ctx);
        } else if (ds->in->wakeup_cb) {
            ds->in->wakeup_cb(ds->in->wakeup_cb_ctx);
        }
        ds->need_wakeup = false;
        pthread_cond_signal(&ds->in->wakeup);
    }
}

static void update_stream_selection_state(struct demux_internal *in,
                                          struct demux_stream *ds)
{
    ds->eof = false;
    ds->refreshing = false;

    // We still have to go over the whole stream list to update ds->eager for
    // other streams too, because they depend on other stream's selections.

    bool any_av_streams = false;
    bool any_streams = false;

    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *s = in->streams[n]->ds;

        s->still_image = s->sh->still_image;
        s->eager = s->selected && !s->sh->attached_picture;
        if (s->eager && !s->still_image)
            any_av_streams |= s->type != STREAM_SUB;
        any_streams |= s->selected;
    }

    // Subtitles are only eagerly read if there are no other eagerly read
    // streams.
    if (any_av_streams) {
        for (int n = 0; n < in->num_streams; n++) {
            struct demux_stream *s = in->streams[n]->ds;

            if (s->type == STREAM_SUB)
                s->eager = false;
        }
    }

    if (!any_streams)
        in->blocked = false;

    ds_clear_reader_state(ds, true);

    // Make sure any stream reselection or addition is reflected in the seek
    // ranges, and also get rid of data that is not needed anymore (or
    // rather, which can't be kept consistent). This has to happen after we've
    // updated all the subtle state (like s->eager).
    for (int n = 0; n < in->num_ranges; n++) {
        struct demux_cached_range *range = in->ranges[n];

        if (!ds->selected)
            clear_queue(range->streams[ds->index]);

        update_seek_ranges(range);
    }

    free_empty_cached_ranges(in);

    wakeup_ds(ds);
}

void demux_set_ts_offset(struct demuxer *demuxer, double offset)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    in->ts_offset = offset;
    pthread_mutex_unlock(&in->lock);
}

static void add_missing_streams(struct demux_internal *in,
                                struct demux_cached_range *range)
{
    for (int n = range->num_streams; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;

        struct demux_queue *queue = talloc_ptrtype(NULL, queue);
        *queue = (struct demux_queue){
            .ds = ds,
            .range = range,
        };
        clear_queue(queue);
        MP_TARRAY_APPEND(range, range->streams, range->num_streams, queue);
        assert(range->streams[ds->index] == queue);
    }
}

// Allocate a new sh_stream of the given type. It either has to be released
// with talloc_free(), or added to a demuxer with demux_add_sh_stream(). You
// cannot add or read packets from the stream before it has been added.
// type may be changed later, but only before demux_add_sh_stream().
struct sh_stream *demux_alloc_sh_stream(enum stream_type type)
{
    struct sh_stream *sh = talloc_ptrtype(NULL, sh);
    *sh = (struct sh_stream) {
        .type = type,
        .index = -1,
        .ff_index = -1,     // may be overwritten by demuxer
        .demuxer_id = -1,   // ... same
        .codec = talloc_zero(sh, struct mp_codec_params),
        .tags = talloc_zero(sh, struct mp_tags),
    };
    sh->codec->type = type;
    return sh;
}

// Add a new sh_stream to the demuxer. Note that as soon as the stream has been
// added, it must be immutable, and must not be released (this will happen when
// the demuxer is destroyed).
static void demux_add_sh_stream_locked(struct demux_internal *in,
                                       struct sh_stream *sh)
{
    assert(!sh->ds); // must not be added yet

    sh->index = in->num_streams;

    sh->ds = talloc(sh, struct demux_stream);
    *sh->ds = (struct demux_stream) {
        .in = in,
        .sh = sh,
        .type = sh->type,
        .index = sh->index,
        .global_correct_dts = true,
        .global_correct_pos = true,
    };

    struct demux_stream *ds = sh->ds;

    if (!sh->codec->codec)
        sh->codec->codec = "";

    if (sh->ff_index < 0)
        sh->ff_index = sh->index;

    MP_TARRAY_APPEND(in, in->streams, in->num_streams, sh);
    assert(in->streams[sh->index] == sh);

    if (in->current_range) {
        for (int n = 0; n < in->num_ranges; n++)
            add_missing_streams(in, in->ranges[n]);

        sh->ds->queue = in->current_range->streams[sh->ds->index];
    }

    update_stream_selection_state(in, sh->ds);

    switch (ds->type) {
    case STREAM_AUDIO:
        ds->back_preroll = in->opts->audio_back_preroll;
        if (ds->back_preroll < 0) { // auto
            ds->back_preroll = mp_codec_is_lossless(sh->codec->codec) ? 0 : 1;
            if (sh->codec->codec && (strcmp(sh->codec->codec, "opus") == 0 ||
                                     strcmp(sh->codec->codec, "vorbis") == 0 ||
                                     strcmp(sh->codec->codec, "mp3") == 0))
                ds->back_preroll = 2;
        }
        break;
    case STREAM_VIDEO:
        ds->back_preroll = in->opts->video_back_preroll;
        if (ds->back_preroll < 0)
            ds->back_preroll = 0; // auto
        break;
    }

    if (!ds->sh->attached_picture) {
        // Typically this is used for webradio, so any stream will do.
        if (!in->metadata_stream)
            in->metadata_stream = sh;
    }

    in->events |= DEMUX_EVENT_STREAMS;
    if (in->wakeup_cb)
        in->wakeup_cb(in->wakeup_cb_ctx);
}

// For demuxer implementations only.
void demux_add_sh_stream(struct demuxer *demuxer, struct sh_stream *sh)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_thread);
    pthread_mutex_lock(&in->lock);
    demux_add_sh_stream_locked(in, sh);
    pthread_mutex_unlock(&in->lock);
}

// Return a stream with the given index. Since streams can only be added during
// the lifetime of the demuxer, it is guaranteed that an index within the valid
// range [0, demux_get_num_stream()) always returns a valid sh_stream pointer,
// which will be valid until the demuxer is destroyed.
struct sh_stream *demux_get_stream(struct demuxer *demuxer, int index)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    assert(index >= 0 && index < in->num_streams);
    struct sh_stream *r = in->streams[index];
    pthread_mutex_unlock(&in->lock);
    return r;
}

// See demux_get_stream().
int demux_get_num_stream(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    int r = in->num_streams;
    pthread_mutex_unlock(&in->lock);
    return r;
}

// It's UB to call anything but demux_dealloc() on the demuxer after this.
static void demux_shutdown(struct demux_internal *in)
{
    struct demuxer *demuxer = in->d_user;

    if (in->recorder) {
        mp_recorder_destroy(in->recorder);
        in->recorder = NULL;
    }

    dumper_close(in);

    if (demuxer->desc->close)
        demuxer->desc->close(in->d_thread);
    demuxer->priv = NULL;
    in->d_thread->priv = NULL;

    demux_flush(demuxer);
    assert(in->total_bytes == 0);

    in->current_range = NULL;
    free_empty_cached_ranges(in);

    talloc_free(in->cache);
    in->cache = NULL;

    if (in->owns_stream)
        free_stream(demuxer->stream);
    demuxer->stream = NULL;
}

static void demux_dealloc(struct demux_internal *in)
{
    for (int n = 0; n < in->num_streams; n++)
        talloc_free(in->streams[n]);
    pthread_mutex_destroy(&in->lock);
    pthread_cond_destroy(&in->wakeup);
    talloc_free(in->d_user);
}

void demux_free(struct demuxer *demuxer)
{
    if (!demuxer)
        return;
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    demux_stop_thread(demuxer);
    demux_shutdown(in);
    demux_dealloc(in);
}

// Start closing the demuxer and eventually freeing the demuxer asynchronously.
// You must not access the demuxer once this has been started. Once the demuxer
// is shutdown, the wakeup callback is invoked. Then you need to call
// demux_free_async_finish() to end the operation (it must not be called from
// the wakeup callback).
// This can return NULL. Then the demuxer cannot be free'd asynchronously, and
// you need to call demux_free() instead.
struct demux_free_async_state *demux_free_async(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    if (!in->threading)
        return NULL;

    pthread_mutex_lock(&in->lock);
    in->thread_terminate = true;
    in->shutdown_async = true;
    pthread_cond_signal(&in->wakeup);
    pthread_mutex_unlock(&in->lock);

    return (struct demux_free_async_state *)demuxer->in; // lies
}

// As long as state is valid, you can call this to request immediate abort.
// Roughly behaves as demux_cancel_and_free(), except you still need to wait
// for the result.
void demux_free_async_force(struct demux_free_async_state *state)
{
    struct demux_internal *in = (struct demux_internal *)state; // reverse lies

    mp_cancel_trigger(in->d_user->cancel);
}

// Check whether the demuxer is shutdown yet. If not, return false, and you
// need to call this again in the future (preferably after you were notified by
// the wakeup callback). If yes, deallocate all state, and return true (in
// particular, the state ptr becomes invalid, and the wakeup callback will never
// be called again).
bool demux_free_async_finish(struct demux_free_async_state *state)
{
    struct demux_internal *in = (struct demux_internal *)state; // reverse lies

    pthread_mutex_lock(&in->lock);
    bool busy = in->shutdown_async;
    pthread_mutex_unlock(&in->lock);

    if (busy)
        return false;

    demux_stop_thread(in->d_user);
    demux_dealloc(in);
    return true;
}

// Like demux_free(), but trigger an abort, which will force the demuxer to
// terminate immediately. If this wasn't opened with demux_open_url(), there is
// some chance this will accidentally abort other things via demuxer->cancel.
void demux_cancel_and_free(struct demuxer *demuxer)
{
    if (!demuxer)
        return;
    mp_cancel_trigger(demuxer->cancel);
    demux_free(demuxer);
}

// Start the demuxer thread, which reads ahead packets on its own.
void demux_start_thread(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    if (!in->threading) {
        in->threading = true;
        if (pthread_create(&in->thread, NULL, demux_thread, in))
            in->threading = false;
    }
}

void demux_stop_thread(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    if (in->threading) {
        pthread_mutex_lock(&in->lock);
        in->thread_terminate = true;
        pthread_cond_signal(&in->wakeup);
        pthread_mutex_unlock(&in->lock);
        pthread_join(in->thread, NULL);
        in->threading = false;
        in->thread_terminate = false;
    }
}

// The demuxer thread will call cb(ctx) if there's a new packet, or EOF is reached.
void demux_set_wakeup_cb(struct demuxer *demuxer, void (*cb)(void *ctx), void *ctx)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    in->wakeup_cb = cb;
    in->wakeup_cb_ctx = ctx;
    pthread_mutex_unlock(&in->lock);
}

void demux_start_prefetch(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    pthread_mutex_lock(&in->lock);
    in->reading = true;
    pthread_cond_signal(&in->wakeup);
    pthread_mutex_unlock(&in->lock);
}

const char *stream_type_name(enum stream_type type)
{
    switch (type) {
    case STREAM_VIDEO:  return "video";
    case STREAM_AUDIO:  return "audio";
    case STREAM_SUB:    return "sub";
    default:            return "unknown";
    }
}

static struct sh_stream *demuxer_get_cc_track_locked(struct sh_stream *stream)
{
    struct sh_stream *sh = stream->ds->cc;

    if (!sh) {
        sh = demux_alloc_sh_stream(STREAM_SUB);
        if (!sh)
            return NULL;
        sh->codec->codec = "eia_608";
        sh->default_track = true;
        stream->ds->cc = sh;
        demux_add_sh_stream_locked(stream->ds->in, sh);
        sh->ds->ignore_eof = true;
    }

    return sh;
}

void demuxer_feed_caption(struct sh_stream *stream, demux_packet_t *dp)
{
    struct demux_internal *in = stream->ds->in;

    pthread_mutex_lock(&in->lock);
    struct sh_stream *sh = demuxer_get_cc_track_locked(stream);
    if (!sh) {
        pthread_mutex_unlock(&in->lock);
        talloc_free(dp);
        return;
    }

    dp->keyframe = true;
    dp->pts = MP_ADD_PTS(dp->pts, -in->ts_offset);
    dp->dts = MP_ADD_PTS(dp->dts, -in->ts_offset);
    dp->stream = sh->index;
    add_packet_locked(sh, dp);
    pthread_mutex_unlock(&in->lock);
}

static void error_on_backward_demuxing(struct demux_internal *in)
{
    if (!in->back_demuxing)
        return;
    MP_ERR(in, "Disabling backward demuxing.\n");
    in->back_demuxing = false;
    clear_reader_state(in, true);
}

static void perform_backward_seek(struct demux_internal *in)
{
    double target = MP_NOPTS_VALUE;

    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;

        if (ds->reader_head && !ds->back_restarting && !ds->back_resuming &&
            ds->eager)
        {
            ds->back_resuming = true;
            ds->back_resume_pos = ds->reader_head->pos;
            ds->back_resume_dts = ds->reader_head->dts;
        }

        target = MP_PTS_MIN(target, ds->back_seek_pos);
    }

    target = MP_PTS_OR_DEF(target, in->d_thread->start_time);

    MP_VERBOSE(in, "triggering backward seek to get more packets\n");
    queue_seek(in, target, SEEK_SATAN | SEEK_HR, false);
    in->reading = true;

    // Don't starve other threads.
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_lock(&in->lock);
}

// For incremental backward demuxing search work.
static void check_backward_seek(struct demux_internal *in)
{
    in->back_any_need_recheck = false;

    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;

        if (ds->back_need_recheck)
            find_backward_restart_pos(ds);
    }
}

// Search for a packet to resume demuxing from.
// The implementation of this function is quite awkward, because the packet
// queue is a singly linked list without back links, while it needs to search
// backwards.
// This is the core of backward demuxing.
static void find_backward_restart_pos(struct demux_stream *ds)
{
    struct demux_internal *in = ds->in;

    ds->back_need_recheck = false;
    if (!ds->back_restarting)
        return;

    struct demux_packet *first = ds->reader_head;
    struct demux_packet *last = ds->queue->tail;

    if (first && !first->keyframe)
        MP_WARN(in, "Queue not starting on keyframe.\n");

    // Packet at back_restart_pos. (Note: we don't actually need it, only the
    // packet immediately before it. But same effort.)
    // If this is NULL, look for EOF (resume from very last keyframe).
    struct demux_packet *back_restart = NULL;

    if (ds->back_restart_next) {
        // Initial state. Switch to one of the other modi.

        for (struct demux_packet *cur = first; cur; cur = cur->next) {
            // Restart for next keyframe after reader_head.
            if (cur != first && cur->keyframe) {
                ds->back_restart_dts = cur->dts;
                ds->back_restart_pos = cur->pos;
                ds->back_restart_eof = false;
                ds->back_restart_next = false;
                break;
            }
        }

        if (ds->back_restart_next && ds->eof) {
            // Restart from end if nothing was found.
            ds->back_restart_eof = true;
            ds->back_restart_next = false;
        }

        if (ds->back_restart_next)
            return;
    }

    if (ds->back_restart_eof) {
        // We're trying to find EOF (without discarding packets). Only continue
        // if we really reach EOF.
        if (!ds->eof)
            return;
    } else if (!first && ds->eof) {
        // Reached EOF during normal backward demuxing. We probably returned the
        // last keyframe range to user. Need to resume at an earlier position.
        // Fall through, hit the no-keyframe case (and possibly the BOF check
        // if there are no packets at all), and then resume_earlier.
    } else if (!first) {
        return; // no packets yet
    } else {
        assert(last);

        if ((ds->global_correct_dts && last->dts < ds->back_restart_dts) ||
            (ds->global_correct_pos && last->pos < ds->back_restart_pos))
            return; // restart pos not reached yet

        // The target we're searching for is apparently before the start of the
        // queue.
        if ((ds->global_correct_dts && first->dts > ds->back_restart_dts) ||
            (ds->global_correct_pos && first->pos > ds->back_restart_pos))
            goto resume_earlier; // current position is too late; seek back


        for (struct demux_packet *cur = first; cur; cur = cur->next) {
            if ((ds->global_correct_dts && cur->dts == ds->back_restart_dts) ||
                (ds->global_correct_pos && cur->pos == ds->back_restart_pos))
            {
                back_restart = cur;
                break;
            }
        }

        if (!back_restart) {
            // The packet should have been in the searched range; maybe dts/pos
            // determinism assumptions were broken.
            MP_ERR(in, "Demuxer not cooperating.\n");
            error_on_backward_demuxing(in);
            return;
        }
    }

    // Find where to restart demuxing. It's usually the last keyframe packet
    // before restart_pos, but might be up to back_preroll + batch keyframe
    // packets earlier.

    // (Normally, we'd just iterate backwards, but no back links.)
    int num_kf = 0;
    struct demux_packet *pre_1 = NULL; // idiotic "optimization" for total=1
    for (struct demux_packet *dp = first; dp != back_restart; dp = dp->next) {
        if (dp->keyframe) {
            num_kf++;
            pre_1 = dp;
        }
    }

    // Number of renderable keyframes to return to user.
    // (Excludes preroll, which is decoded by user, but then discarded.)
    int batch = MPMAX(in->opts->back_batch[ds->type], 1);
    // Number of keyframes to return to the user in total.
    int total = batch + ds->back_preroll;

    assert(total >= 1);

    bool is_bof = ds->queue->is_bof &&
        (first == ds->queue->head || ds->back_seek_pos < ds->queue->seek_start);

    struct demux_packet *target = NULL; // resume pos
    // nr. of keyframes, incl. target, excl. restart_pos
    int got_total = num_kf < total && is_bof ? num_kf : total;
    int got_preroll = MPMAX(got_total - batch, 0);

    if (got_total == 1) {
        target = pre_1;
    } else if (got_total <= num_kf) {
        int cur_kf = 0;
        for (struct demux_packet *dp = first; dp != back_restart; dp = dp->next) {
            if (dp->keyframe) {
                if (num_kf - cur_kf == got_total) {
                    target = dp;
                    break;
                }
                cur_kf++;
            }
        }
    }

    if (!target) {
        if (is_bof) {
            MP_VERBOSE(in, "BOF for stream %d\n", ds->index);
            ds->back_restarting = false;
            ds->back_range_started = false;
            ds->back_range_count = -1;
            ds->back_range_preroll = 0;
            ds->need_wakeup = true;
            wakeup_ds(ds);
            return;
        }
        goto resume_earlier;
    }

    // Skip reader_head from previous keyframe to current one.
    // Or if preroll is involved, the first preroll packet.
    while (ds->reader_head != target) {
        if (!advance_reader_head(ds))
            assert(0); // target must be in list
    }

    double seek_pts;
    compute_keyframe_times(target, &seek_pts, NULL);
    if (seek_pts != MP_NOPTS_VALUE)
        ds->back_seek_pos = seek_pts;

    // For next backward adjust action.
    struct demux_packet *restart_pkt = NULL;
    int kf_pos = 0;
    for (struct demux_packet *dp = target; dp; dp = dp->next) {
        if (dp->keyframe) {
            if (kf_pos == got_preroll) {
                restart_pkt = dp;
                break;
            }
            kf_pos++;
        }
    }
    assert(restart_pkt);
    ds->back_restart_dts = restart_pkt->dts;
    ds->back_restart_pos = restart_pkt->pos;

    ds->back_restarting = false;
    ds->back_range_started = false;
    ds->back_range_count = got_total;
    ds->back_range_preroll = got_preroll;
    ds->need_wakeup = true;
    wakeup_ds(ds);
    return;

resume_earlier:
    // We want to seek back to get earlier packets. But before we do this, we
    // must be sure that other streams have initialized their state. The only
    // time when this state is not initialized is right after the seek that
    // started backward demuxing (not any subsequent backstep seek). If this
    // initialization is omitted, the stream would try to start demuxing from
    // the "current" position. If another stream backstepped before that, the
    // other stream will miss the original seek target, and start playback from
    // a position that is too early.
    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds2 = in->streams[n]->ds;
        if (ds2 == ds || !ds2->eager)
            continue;

        if (ds2->back_restarting && ds2->back_restart_next) {
            MP_VERBOSE(in, "delaying stream %d for %d\n", ds->index, ds2->index);
            return;
        }
    }

    if (ds->back_seek_pos != MP_NOPTS_VALUE) {
        struct demux_packet *t =
            find_seek_target(ds->queue, ds->back_seek_pos - 0.001, 0);
        if (t && t != ds->reader_head) {
            double pts;
            compute_keyframe_times(t, &pts, NULL);
            ds->back_seek_pos = MP_PTS_MIN(ds->back_seek_pos, pts);
            ds_clear_reader_state(ds, false);
            ds->reader_head = t;
            ds->back_need_recheck = true;
            in->back_any_need_recheck = true;
            pthread_cond_signal(&in->wakeup);
        } else {
            ds->back_seek_pos -= in->opts->back_seek_size;
            in->need_back_seek = true;
        }
    }
}

// Process that one or multiple packets were added.
static void back_demux_see_packets(struct demux_stream *ds)
{
    struct demux_internal *in = ds->in;

    if (!ds->selected || !in->back_demuxing || !ds->eager)
        return;

    assert(!(ds->back_resuming && ds->back_restarting));

    if (!ds->global_correct_dts && !ds->global_correct_pos) {
        MP_ERR(in, "Can't demux backward due to demuxer problems.\n");
        error_on_backward_demuxing(in);
        return;
    }

    while (ds->back_resuming && ds->reader_head) {
        struct demux_packet *head = ds->reader_head;
        if ((ds->global_correct_dts && head->dts == ds->back_resume_dts) ||
            (ds->global_correct_pos && head->pos == ds->back_resume_pos))
        {
            ds->back_resuming = false;
            ds->need_wakeup = true;
            wakeup_ds(ds); // probably
            break;
        }
        advance_reader_head(ds);
    }

    if (ds->back_restarting)
        find_backward_restart_pos(ds);
}

// Add the keyframe to the end of the index. Not all packets are actually added.
static void add_index_entry(struct demux_queue *queue, struct demux_packet *dp,
                            double pts)
{
    struct demux_internal *in = queue->ds->in;

    assert(dp->keyframe && pts != MP_NOPTS_VALUE);

    if (queue->num_index > 0) {
        struct index_entry *last = &QUEUE_INDEX_ENTRY(queue, queue->num_index - 1);
        if (pts - last->pts < INDEX_STEP_SIZE)
            return;
    }

    if (queue->num_index == queue->index_size) {
        // Needs to honor power-of-2 requirement.
        size_t new_size = MPMAX(128, queue->index_size * 2);
        assert(!(new_size & (new_size - 1)));
        MP_DBG(in, "stream %d: resize index to %zu\n", queue->ds->index,
               new_size);
        // Note: we could tolerate allocation failure, and just discard the
        // entire index (and prevent the index from being recreated).
        MP_RESIZE_ARRAY(NULL, queue->index, new_size);
        size_t highest_index = queue->index0 + queue->num_index;
        for (size_t n = queue->index_size; n < highest_index; n++)
            queue->index[n] = queue->index[n - queue->index_size];
        in->total_bytes +=
            (new_size - queue->index_size) * sizeof(queue->index[0]);
        queue->index_size = new_size;
    }

    assert(queue->num_index < queue->index_size);

    queue->num_index += 1;

    QUEUE_INDEX_ENTRY(queue, queue->num_index - 1) = (struct index_entry){
        .pts = pts,
        .pkt = dp,
    };
}

// Check whether the next range in the list is, and if it appears to overlap,
// try joining it into a single range.
static void attempt_range_joining(struct demux_internal *in)
{
    struct demux_cached_range *current = in->current_range;
    struct demux_cached_range *next = NULL;
    double next_dist = INFINITY;

    assert(current && in->num_ranges > 0);
    assert(current == in->ranges[in->num_ranges - 1]);

    for (int n = 0; n < in->num_ranges - 1; n++) {
        struct demux_cached_range *range = in->ranges[n];

        if (current->seek_start <= range->seek_start) {
            // This uses ">" to get some non-0 overlap.
            double dist = current->seek_end - range->seek_start;
            if (dist > 0 && dist < next_dist) {
                next = range;
                next_dist = dist;
            }
        }
    }

    if (!next)
        return;

    MP_VERBOSE(in, "going to join ranges %f-%f + %f-%f\n",
               current->seek_start, current->seek_end,
               next->seek_start, next->seek_end);

    // Try to find a join point, where packets obviously overlap. (It would be
    // better and faster to do this incrementally, but probably too complex.)
    // The current range can overlap arbitrarily with the next one, not only by
    // by the seek overlap, but for arbitrary packet readahead as well.
    // We also drop the overlapping packets (if joining fails, we discard the
    // entire next range anyway, so this does no harm).
    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;

        struct demux_queue *q1 = current->streams[n];
        struct demux_queue *q2 = next->streams[n];

        if (!ds->global_correct_pos && !ds->global_correct_dts) {
            MP_WARN(in, "stream %d: ranges unjoinable\n", n);
            goto failed;
        }

        struct demux_packet *end = q1->tail;
        bool join_point_found = !end; // no packets yet -> joining will work
        if (end) {
            while (q2->head) {
                struct demux_packet *dp = q2->head;

                // Some weird corner-case. We'd have to search the equivalent
                // packet in q1 to update it correctly. Better just give up.
                if (dp == q2->keyframe_latest) {
                    MP_VERBOSE(in, "stream %d: not enough keyframes for join\n", n);
                    goto failed;
                }

                if ((ds->global_correct_dts && dp->dts == end->dts) ||
                    (ds->global_correct_pos && dp->pos == end->pos))
                {
                    // Do some additional checks as a (imperfect) sanity check
                    // in case pos/dts are not "correct" across the ranges (we
                    // never actually check that).
                    if (dp->dts != end->dts || dp->pos != end->pos ||
                        dp->pts != end->pts)
                    {
                        MP_WARN(in,
                            "stream %d: non-repeatable demuxer behavior\n", n);
                        goto failed;
                    }

                    remove_head_packet(q2);
                    join_point_found = true;
                    break;
                }

                // This happens if the next range misses the end packet. For
                // normal streams (ds->eager==true), this is a failure to find
                // an overlap. For subtitles, this can mean the current_range
                // has a subtitle somewhere before the end of its range, and
                // next has another subtitle somewhere after the start of its
                // range.
                if ((ds->global_correct_dts && dp->dts > end->dts) ||
                    (ds->global_correct_pos && dp->pos > end->pos))
                    break;

                remove_head_packet(q2);
            }
        }

        // For enabled non-sparse streams, always require an overlap packet.
        if (ds->eager && !join_point_found) {
            MP_WARN(in, "stream %d: no join point found\n", n);
            goto failed;
        }
    }

    // Actually join the ranges. Now that we think it will work, mutate the
    // data associated with the current range.

    for (int n = 0; n < in->num_streams; n++) {
        struct demux_queue *q1 = current->streams[n];
        struct demux_queue *q2 = next->streams[n];

        struct demux_stream *ds = in->streams[n]->ds;
        assert(ds->queue == q1);

        // First new packet that is appended to the current range.
        struct demux_packet *join_point = q2->head;

        if (q2->head) {
            if (q1->head) {
                q1->tail->next = q2->head;
            } else {
                q1->head = q2->head;
            }
            q1->tail = q2->tail;
        }

        q1->seek_end = q2->seek_end;
        q1->correct_dts &= q2->correct_dts;
        q1->correct_pos &= q2->correct_pos;
        q1->last_pos = q2->last_pos;
        q1->last_dts = q2->last_dts;
        q1->last_ts = q2->last_ts;
        q1->keyframe_latest = q2->keyframe_latest;
        q1->is_eof = q2->is_eof;

        q1->last_pos_fixup = -1;

        q2->head = q2->tail = NULL;
        q2->keyframe_first = NULL;
        q2->keyframe_latest = NULL;

        if (ds->selected && !ds->reader_head)
            ds->reader_head = join_point;
        ds->skip_to_keyframe = false;

        // Make the cum_pos values in all q2 packets continuous.
        for (struct demux_packet *dp = join_point; dp; dp = dp->next) {
            uint64_t next_pos = dp->next ? dp->next->cum_pos : q2->tail_cum_pos;
            uint64_t size = next_pos - dp->cum_pos;
            dp->cum_pos = q1->tail_cum_pos;
            q1->tail_cum_pos += size;
        }

        // And update the index with packets from q2.
        for (size_t i = 0; i < q2->num_index; i++) {
            struct index_entry *e = &QUEUE_INDEX_ENTRY(q2, i);
            add_index_entry(q1, e->pkt, e->pts);
        }
        free_index(q2);

        // For moving demuxer position.
        ds->refreshing = ds->selected;
    }

    for (int n = 0; n < next->num_metadata; n++) {
        MP_TARRAY_APPEND(current, current->metadata, current->num_metadata,
                         next->metadata[n]);
    }
    next->num_metadata = 0;

    update_seek_ranges(current);

    // Move demuxing position to after the current range.
    in->seeking = true;
    in->seek_flags = SEEK_HR;
    in->seek_pts = next->seek_end - 1.0;

    MP_VERBOSE(in, "ranges joined!\n");

    for (int n = 0; n < in->num_streams; n++)
        back_demux_see_packets(in->streams[n]->ds);

failed:
    clear_cached_range(in, next);
    free_empty_cached_ranges(in);
}

// Compute the assumed first and last frame timestamp for keyframe range
// starting at pkt. To get valid results, pkt->keyframe must be true, otherwise
// nonsense will be returned.
// Always sets *out_kf_min and *out_kf_max without reading them. Both are set
// to NOPTS if there are no timestamps at all in the stream. *kf_max will not
// be set to the actual end time of the decoded output, just the last frame
// (audio will typically end up with kf_min==kf_max).
// Either of out_kf_min and out_kf_max can be NULL, which discards the result.
// Return the next keyframe packet after pkt, or NULL if there's none.
static struct demux_packet *compute_keyframe_times(struct demux_packet *pkt,
                                                   double *out_kf_min,
                                                   double *out_kf_max)
{
    struct demux_packet *start = pkt;
    double min = MP_NOPTS_VALUE;
    double max = MP_NOPTS_VALUE;

    while (pkt) {
        if (pkt->keyframe && pkt != start)
            break;

        double ts = MP_PTS_OR_DEF(pkt->pts, pkt->dts);
        if (pkt->segmented && ((pkt->start != MP_NOPTS_VALUE && ts < pkt->start) ||
                               (pkt->end != MP_NOPTS_VALUE && ts > pkt->end)))
            ts = MP_NOPTS_VALUE;

        min = MP_PTS_MIN(min, ts);
        max = MP_PTS_MAX(max, ts);

        pkt = pkt->next;
    }

    if (out_kf_min)
        *out_kf_min = min;
    if (out_kf_max)
        *out_kf_max = max;
    return pkt;
}

// Determine seekable range when a packet is added. If dp==NULL, treat it as
// EOF (i.e. closes the current block).
// This has to deal with a number of corner cases, such as demuxers potentially
// starting output at non-keyframes.
// Can join seek ranges, which messes with in->current_range and all.
static void adjust_seek_range_on_packet(struct demux_stream *ds,
                                        struct demux_packet *dp)
{
    struct demux_queue *queue = ds->queue;

    if (!ds->in->seekable_cache)
        return;

    bool new_eof = !dp;
    bool update_ranges = queue->is_eof != new_eof;
    queue->is_eof = new_eof;

    if (!dp || dp->keyframe) {
        if (queue->keyframe_latest) {
            double kf_min, kf_max;
            compute_keyframe_times(queue->keyframe_latest, &kf_min, &kf_max);

            if (kf_min != MP_NOPTS_VALUE) {
                add_index_entry(queue, queue->keyframe_latest, kf_min);

                // Initialize the queue's start if it's unset.
                if (queue->seek_start == MP_NOPTS_VALUE) {
                    update_ranges = true;
                    queue->seek_start = kf_min + ds->sh->seek_preroll;
                }
            }

            if (kf_max != MP_NOPTS_VALUE &&
                (queue->seek_end == MP_NOPTS_VALUE || kf_max > queue->seek_end))
            {
                // If the queue was past the current range's end even before
                // this update, it means _other_ streams are not there yet,
                // and the seek range doesn't need to be updated. This means
                // if the _old_ queue->seek_end was already after the range end,
                // then the new seek_end won't extend the range either.
                if (queue->range->seek_end == MP_NOPTS_VALUE ||
                    queue->seek_end <= queue->range->seek_end)
                {
                    update_ranges = true;
                }

                queue->seek_end = kf_max;
            }
        }

        queue->keyframe_latest = dp;
    }

    // Adding a sparse packet never changes the seek range.
    if (update_ranges && ds->eager) {
        update_seek_ranges(queue->range);
        attempt_range_joining(ds->in);
    }
}

static struct mp_recorder *recorder_create(struct demux_internal *in,
                                           const char *dst)
{
    struct sh_stream **streams = NULL;
    int num_streams = 0;
    for (int n = 0; n < in->num_streams; n++) {
        struct sh_stream *stream = in->streams[n];
        if (stream->ds->selected)
            MP_TARRAY_APPEND(NULL, streams, num_streams, stream);
    }

    struct demuxer *demuxer = in->d_thread;
    struct demux_attachment **attachments = talloc_array(NULL, struct demux_attachment*, demuxer->num_attachments);
    for (int n = 0; n < demuxer->num_attachments; n++) {
        attachments[n] = &demuxer->attachments[n];
    }

    struct mp_recorder *res = mp_recorder_create(in->d_thread->global, dst,
                                                 streams, num_streams,
                                                 attachments, demuxer->num_attachments);
    talloc_free(streams);
    talloc_free(attachments);
    return res;
}

static void write_dump_packet(struct demux_internal *in, struct demux_packet *dp)
{
    assert(in->dumper);
    assert(in->dumper_status == CONTROL_TRUE);

    struct mp_recorder_sink *sink =
        mp_recorder_get_sink(in->dumper, in->streams[dp->stream]);
    if (sink) {
        mp_recorder_feed_packet(sink, dp);
    } else {
        MP_ERR(in, "New stream appeared; stopping recording.\n");
        in->dumper_status = CONTROL_ERROR;
    }
}

static void record_packet(struct demux_internal *in, struct demux_packet *dp)
{
    // (should preferably be outside of the lock)
    if (in->enable_recording && !in->recorder &&
        in->opts->record_file && in->opts->record_file[0])
    {
        // Later failures shouldn't make it retry and overwrite the previously
        // recorded file.
        in->enable_recording = false;

        in->recorder = recorder_create(in, in->opts->record_file);
        if (!in->recorder)
            MP_ERR(in, "Disabling recording.\n");
    }

    if (in->recorder) {
        struct mp_recorder_sink *sink =
            mp_recorder_get_sink(in->recorder, in->streams[dp->stream]);
        if (sink) {
            mp_recorder_feed_packet(sink, dp);
        } else {
            MP_ERR(in, "New stream appeared; stopping recording.\n");
            mp_recorder_destroy(in->recorder);
            in->recorder = NULL;
        }
    }

    if (in->dumper_status == CONTROL_OK)
        write_dump_packet(in, dp);
}

static void add_packet_locked(struct sh_stream *stream, demux_packet_t *dp)
{
    struct demux_stream *ds = stream ? stream->ds : NULL;
    if (!dp->len || demux_cancel_test(ds->in->d_thread)) {
        talloc_free(dp);
        return;
    }

    assert(dp->stream == stream->index);
    assert(!dp->next);

    struct demux_internal *in = ds->in;

    in->after_seek = false;
    in->after_seek_to_start = false;

    double ts = dp->dts == MP_NOPTS_VALUE ? dp->pts : dp->dts;
    if (dp->segmented)
        ts = MP_PTS_MIN(ts, dp->end);

    if (ts != MP_NOPTS_VALUE)
        in->demux_ts = ts;

    struct demux_queue *queue = ds->queue;

    bool drop = !ds->selected || in->seeking || ds->sh->attached_picture;

    if (!drop) {
        // If libavformat splits packets, some packets will have pos unset, so
        // make up one based on the first packet => makes refresh seeks work.
        if ((dp->pos < 0 || dp->pos == queue->last_pos_fixup) &&
            !dp->keyframe && queue->last_pos_fixup >= 0)
            dp->pos = queue->last_pos_fixup + 1;
        queue->last_pos_fixup = dp->pos;
    }

    if (!drop && ds->refreshing) {
        // Resume reading once the old position was reached (i.e. we start
        // returning packets where we left off before the refresh).
        // If it's the same position, drop, but continue normally next time.
        if (queue->correct_dts) {
            ds->refreshing = dp->dts < queue->last_dts;
        } else if (queue->correct_pos) {
            ds->refreshing = dp->pos < queue->last_pos;
        } else {
            ds->refreshing = false; // should not happen
            MP_WARN(in, "stream %d: demux refreshing failed\n", ds->index);
        }
        drop = true;
    }

    if (drop) {
        talloc_free(dp);
        return;
    }

    record_packet(in, dp);

    if (in->cache && in->opts->disk_cache) {
        int64_t pos = demux_cache_write(in->cache, dp);
        if (pos >= 0) {
            demux_packet_unref_contents(dp);
            dp->is_cached = true;
            dp->cached_data.pos = pos;
        }
    }

    queue->correct_pos &= dp->pos >= 0 && dp->pos > queue->last_pos;
    queue->correct_dts &= dp->dts != MP_NOPTS_VALUE && dp->dts > queue->last_dts;
    queue->last_pos = dp->pos;
    queue->last_dts = dp->dts;
    ds->global_correct_pos &= queue->correct_pos;
    ds->global_correct_dts &= queue->correct_dts;

    // (keep in mind that even if the reader went out of data, the queue is not
    // necessarily empty due to the backbuffer)
    if (!ds->reader_head && (!ds->skip_to_keyframe || dp->keyframe)) {
        ds->reader_head = dp;
        ds->skip_to_keyframe = false;
    }

    size_t bytes = demux_packet_estimate_total_size(dp);
    in->total_bytes += bytes;
    dp->cum_pos = queue->tail_cum_pos;
    queue->tail_cum_pos += bytes;

    if (queue->tail) {
        // next packet in stream
        queue->tail->next = dp;
        queue->tail = dp;
    } else {
        // first packet in stream
        queue->head = queue->tail = dp;
    }

    if (!ds->ignore_eof) {
        // obviously not true anymore
        ds->eof = false;
        in->eof = false;
    }

    // For video, PTS determination is not trivial, but for other media types
    // distinguishing PTS and DTS is not useful.
    if (stream->type != STREAM_VIDEO && dp->pts == MP_NOPTS_VALUE)
        dp->pts = dp->dts;

    if (ts != MP_NOPTS_VALUE && (ts > queue->last_ts || ts + 10 < queue->last_ts))
        queue->last_ts = ts;
    if (ds->base_ts == MP_NOPTS_VALUE)
        ds->base_ts = queue->last_ts;

    const char *num_pkts = queue->head == queue->tail ? "1" : ">1";
    uint64_t fw_bytes = get_foward_buffered_bytes(ds);
    MP_TRACE(in, "append packet to %s: size=%zu pts=%f dts=%f pos=%"PRIi64" "
             "[num=%s size=%zd]\n", stream_type_name(stream->type),
             dp->len, dp->pts, dp->dts, dp->pos, num_pkts, (size_t)fw_bytes);

    adjust_seek_range_on_packet(ds, dp);

    // May need to reduce backward cache.
    prune_old_packets(in);

    // Possibly update duration based on highest TS demuxed (but ignore subs).
    if (stream->type != STREAM_SUB) {
        if (dp->segmented)
            ts = MP_PTS_MIN(ts, dp->end);
        if (ts > in->highest_av_pts) {
            in->highest_av_pts = ts;
            double duration = in->highest_av_pts - in->d_thread->start_time;
            if (duration > in->d_thread->duration) {
                in->d_thread->duration = duration;
                // (Don't wakeup user thread, would be too noisy.)
                in->events |= DEMUX_EVENT_DURATION;
                in->duration = duration;
            }
        }
    }

    // Don't process the packet further if it's skipped by the previous seek
    // (see reader_head check/assignment above).
    if (!ds->reader_head)
        return;

    back_demux_see_packets(ds);

    wakeup_ds(ds);
}

static void mark_stream_eof(struct demux_stream *ds)
{
    if (!ds->eof) {
        ds->eof = true;
        adjust_seek_range_on_packet(ds, NULL);
        back_demux_see_packets(ds);
        wakeup_ds(ds);
    }
}

static bool lazy_stream_needs_wait(struct demux_stream *ds)
{
    struct demux_internal *in = ds->in;
    // Attempt to read until force_read_until was reached, or reading has
    // stopped for some reason (true EOF, queue overflow).
    return !ds->eager && !ds->reader_head && !in->back_demuxing &&
           !in->eof && ds->force_read_until != MP_NOPTS_VALUE &&
           (in->demux_ts == MP_NOPTS_VALUE ||
            in->demux_ts <= ds->force_read_until);
}

// Returns true if there was "progress" (lock was released temporarily).
static bool read_packet(struct demux_internal *in)
{
    bool was_reading = in->reading;
    in->reading = false;

    if (!was_reading || in->blocked || demux_cancel_test(in->d_thread))
        return false;

    // Check if we need to read a new packet. We do this if all queues are below
    // the minimum, or if a stream explicitly needs new packets. Also includes
    // safe-guards against packet queue overflow.
    bool read_more = false, prefetch_more = false, refresh_more = false;
    uint64_t total_fw_bytes = 0;
    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;
        if (ds->eager) {
            read_more |= !ds->reader_head;
            if (in->back_demuxing)
                read_more |= ds->back_restarting || ds->back_resuming;
        } else {
            if (lazy_stream_needs_wait(ds)) {
                read_more = true;
            } else {
                mark_stream_eof(ds); // let playback continue
            }
        }
        refresh_more |= ds->refreshing;
        if (ds->eager && ds->queue->last_ts != MP_NOPTS_VALUE &&
            in->min_secs > 0 && ds->base_ts != MP_NOPTS_VALUE &&
            ds->queue->last_ts >= ds->base_ts &&
            !in->back_demuxing)
            prefetch_more |= ds->queue->last_ts - ds->base_ts < in->min_secs;
        total_fw_bytes += get_foward_buffered_bytes(ds);
    }

    MP_TRACE(in, "bytes=%zd, read_more=%d prefetch_more=%d, refresh_more=%d\n",
             (size_t)total_fw_bytes, read_more, prefetch_more, refresh_more);
    if (total_fw_bytes >= in->max_bytes) {
        // if we hit the limit just by prefetching, simply stop prefetching
        if (!read_more)
            return false;
        if (!in->warned_queue_overflow) {
            in->warned_queue_overflow = true;
            MP_WARN(in, "Too many packets in the demuxer packet queues:\n");
            for (int n = 0; n < in->num_streams; n++) {
                struct demux_stream *ds = in->streams[n]->ds;
                if (ds->selected) {
                    size_t num_pkts = 0;
                    for (struct demux_packet *dp = ds->reader_head;
                         dp; dp = dp->next)
                        num_pkts++;
                    uint64_t fw_bytes = get_foward_buffered_bytes(ds);
                    MP_WARN(in, "  %s/%d: %zd packets, %zd bytes%s%s\n",
                            stream_type_name(ds->type), n,
                            num_pkts, (size_t)fw_bytes,
                            ds->eager ? "" : " (lazy)",
                            ds->refreshing ? " (refreshing)" : "");
                }
            }
            if (in->back_demuxing)
                MP_ERR(in, "Backward playback is likely stuck/broken now.\n");
        }
        for (int n = 0; n < in->num_streams; n++) {
            struct demux_stream *ds = in->streams[n]->ds;
            if (!ds->reader_head)
                mark_stream_eof(ds);
        }
        return false;
    }

    if (!read_more && !prefetch_more && !refresh_more)
        return false;

    if (in->after_seek_to_start) {
        for (int n = 0; n < in->num_streams; n++) {
            struct demux_stream *ds = in->streams[n]->ds;
            in->current_range->streams[n]->is_bof =
                ds->selected && !ds->refreshing;
        }
    }

    // Actually read a packet. Drop the lock while doing so, because waiting
    // for disk or network I/O can take time.
    in->reading = true;
    in->after_seek = false;
    in->after_seek_to_start = false;
    pthread_mutex_unlock(&in->lock);

    struct demuxer *demux = in->d_thread;
    struct demux_packet *pkt = NULL;

    bool eof = true;
    if (demux->desc->read_packet && !demux_cancel_test(demux))
        eof = !demux->desc->read_packet(demux, &pkt);

    pthread_mutex_lock(&in->lock);
    update_cache(in);

    if (pkt) {
        assert(pkt->stream >= 0 && pkt->stream < in->num_streams);
        add_packet_locked(in->streams[pkt->stream], pkt);
    }

    if (!in->seeking) {
        if (eof) {
            for (int n = 0; n < in->num_streams; n++)
                mark_stream_eof(in->streams[n]->ds);
            // If we had EOF previously, then don't wakeup (avoids wakeup loop)
            if (!in->eof) {
                if (in->wakeup_cb)
                    in->wakeup_cb(in->wakeup_cb_ctx);
                pthread_cond_signal(&in->wakeup);
                MP_VERBOSE(in, "EOF reached.\n");
            }
        }
        in->eof = eof;
        in->reading = !eof;
    }
    return true;
}

static void prune_old_packets(struct demux_internal *in)
{
    assert(in->current_range == in->ranges[in->num_ranges - 1]);

    // It's not clear what the ideal way to prune old packets is. For now, we
    // prune the oldest packet runs, as long as the total cache amount is too
    // big.
    while (1) {
        uint64_t fw_bytes = 0;
        for (int n = 0; n < in->num_streams; n++) {
            struct demux_stream *ds = in->streams[n]->ds;
            fw_bytes += get_foward_buffered_bytes(ds);
        }
        uint64_t max_avail = in->max_bytes_bw;
        // Backward cache (if enabled at all) can use unused forward cache.
        // Still leave 1 byte free, so the read_packet logic doesn't get stuck.
        if (max_avail && in->max_bytes > (fw_bytes + 1) && in->opts->donate_fw)
            max_avail += in->max_bytes - (fw_bytes + 1);
        if (in->total_bytes - fw_bytes <= max_avail)
            break;

        // (Start from least recently used range.)
        struct demux_cached_range *range = in->ranges[0];
        double earliest_ts = MP_NOPTS_VALUE;
        struct demux_stream *earliest_stream = NULL;

        for (int n = 0; n < range->num_streams; n++) {
            struct demux_queue *queue = range->streams[n];
            struct demux_stream *ds = queue->ds;

            if (queue->head && queue->head != ds->reader_head) {
                struct demux_packet *dp = queue->head;
                double ts = queue->seek_start;
                // If the ts is NOPTS, the queue has no retainable packets, so
                // delete them all. This code is not run when there's enough
                // free space, so normally the queue gets the chance to build up.
                bool prune_always =
                    !in->seekable_cache || ts == MP_NOPTS_VALUE || !dp->keyframe;
                if (prune_always || !earliest_stream || ts < earliest_ts) {
                    earliest_ts = ts;
                    earliest_stream = ds;
                    if (prune_always)
                        break;
                }
            }
        }

        // In some cases (like when the seek index became huge), there aren't
        // any backwards packets, even if the total cache size is exceeded.
        if (!earliest_stream)
            break;

        struct demux_stream *ds = earliest_stream;
        struct demux_queue *queue = range->streams[ds->index];

        bool non_kf_prune = queue->head && !queue->head->keyframe;
        bool kf_was_pruned = false;

        while (queue->head && queue->head != ds->reader_head) {
            if (queue->head->keyframe) {
                // If the cache is seekable, only delete until up the next
                // keyframe. This is not always efficient, but ensures we
                // prune all streams fairly.
                // Also, if the first packet was _not_ a keyframe, we want it
                // to remove all preceding non-keyframe packets first, before
                // re-evaluating what to prune next.
                if ((kf_was_pruned || non_kf_prune) && in->seekable_cache)
                    break;
                kf_was_pruned = true;
            }

            remove_head_packet(queue);
        }

        // Need to update the seekable time range.
        if (kf_was_pruned) {
            assert(!queue->keyframe_first); // it was just deleted, supposedly

            queue->keyframe_first = queue->head;
            // (May happen if reader_head stopped pruning the range, and there's
            // no next range.)
            while (queue->keyframe_first && !queue->keyframe_first->keyframe)
                queue->keyframe_first = queue->keyframe_first->next;

            if (queue->seek_start != MP_NOPTS_VALUE)
                queue->last_pruned = queue->seek_start;

            double kf_min;
            compute_keyframe_times(queue->keyframe_first, &kf_min, NULL);

            bool update_range = true;

            queue->seek_start = kf_min;

            if (queue->seek_start != MP_NOPTS_VALUE) {
                queue->seek_start += ds->sh->seek_preroll;

                // Don't need to update if the new start is still before the
                // range's start (or if the range was undefined anyway).
                if (range->seek_start == MP_NOPTS_VALUE ||
                    queue->seek_start <= range->seek_start)
                {
                    update_range = false;
                }
            }

            if (update_range)
                update_seek_ranges(range);
        }

        if (range != in->current_range && range->seek_start == MP_NOPTS_VALUE)
            free_empty_cached_ranges(in);
    }
}

static void execute_trackswitch(struct demux_internal *in)
{
    in->tracks_switched = false;

    bool any_selected = false;
    for (int n = 0; n < in->num_streams; n++)
        any_selected |= in->streams[n]->ds->selected;

    pthread_mutex_unlock(&in->lock);

    if (in->d_thread->desc->switched_tracks)
        in->d_thread->desc->switched_tracks(in->d_thread);

    pthread_mutex_lock(&in->lock);
}

static void execute_seek(struct demux_internal *in)
{
    int flags = in->seek_flags;
    double pts = in->seek_pts;
    in->eof = false;
    in->seeking = false;
    in->seeking_in_progress = pts;
    in->demux_ts = MP_NOPTS_VALUE;
    in->low_level_seeks += 1;
    in->after_seek = true;
    in->after_seek_to_start =
        !(flags & (SEEK_FORWARD | SEEK_FACTOR)) &&
        pts <= in->d_thread->start_time;

    for (int n = 0; n < in->num_streams; n++)
        in->streams[n]->ds->queue->last_pos_fixup = -1;

    if (in->recorder)
        mp_recorder_mark_discontinuity(in->recorder);

    pthread_mutex_unlock(&in->lock);

    MP_VERBOSE(in, "execute seek (to %f flags %d)\n", pts, flags);

    if (in->d_thread->desc->seek)
        in->d_thread->desc->seek(in->d_thread, pts, flags);

    MP_VERBOSE(in, "seek done\n");

    pthread_mutex_lock(&in->lock);

    in->seeking_in_progress = MP_NOPTS_VALUE;
}

static void update_opts(struct demux_internal *in)
{
    struct demux_opts *opts = in->opts;

    in->min_secs = opts->min_secs;
    in->max_bytes = opts->max_bytes;
    in->max_bytes_bw = opts->max_bytes_bw;

    int seekable = opts->seekable_cache;
    bool is_streaming = in->d_thread->is_streaming;
    bool use_cache = is_streaming;
    if (opts->enable_cache >= 0)
        use_cache = opts->enable_cache == 1;

    if (use_cache) {
        in->min_secs = MPMAX(in->min_secs, opts->min_secs_cache);
        if (seekable < 0)
            seekable = 1;
    }
    in->seekable_cache = seekable == 1;
    in->using_network_cache_opts = is_streaming && use_cache;

    if (!in->seekable_cache)
        in->max_bytes_bw = 0;

    if (!in->can_cache) {
        in->seekable_cache = false;
        in->min_secs = 0;
        in->max_bytes = 1;
        in->max_bytes_bw = 0;
        in->using_network_cache_opts = false;
    }

    if (in->seekable_cache && opts->disk_cache && !in->cache) {
        in->cache = demux_cache_create(in->global, in->log);
        if (!in->cache)
            MP_ERR(in, "Failed to create file cache.\n");
    }

    // The filename option really decides whether recording should be active.
    // So if the filename changes, act upon it.
    char *old = in->record_filename ? in->record_filename : "";
    char *new = opts->record_file ? opts->record_file : "";
    if (strcmp(old, new) != 0) {
        if (in->recorder) {
            MP_WARN(in, "Stopping recording.\n");
            mp_recorder_destroy(in->recorder);
            in->recorder = NULL;
        }
        talloc_free(in->record_filename);
        in->record_filename = talloc_strdup(in, opts->record_file);
        // Note: actual recording only starts once packets are read. It may be
        // important to delay creating in->recorder to that point, because the
        // demuxer might detect more streams until finding the first packet.
        in->enable_recording = in->can_record;
    }

    // In case the cache was reduced in size.
    prune_old_packets(in);

    // In case the seekable cache was disabled.
    free_empty_cached_ranges(in);
}

// Make demuxing progress. Return whether progress was made.
static bool thread_work(struct demux_internal *in)
{
    if (m_config_cache_update(in->opts_cache))
        update_opts(in);
    if (in->tracks_switched) {
        execute_trackswitch(in);
        return true;
    }
    if (in->need_back_seek) {
        perform_backward_seek(in);
        return true;
    }
    if (in->back_any_need_recheck) {
        check_backward_seek(in);
        return true;
    }
    if (in->seeking) {
        execute_seek(in);
        return true;
    }
    if (read_packet(in))
        return true; // read_packet unlocked, so recheck conditions
    if (mp_time_us() >= in->next_cache_update) {
        update_cache(in);
        return true;
    }
    return false;
}

static void *demux_thread(void *pctx)
{
    struct demux_internal *in = pctx;
    mpthread_set_name("demux");
    pthread_mutex_lock(&in->lock);

    stats_register_thread_cputime(in->stats, "thread");

    while (!in->thread_terminate) {
        if (thread_work(in))
            continue;
        pthread_cond_signal(&in->wakeup);
        struct timespec until = mp_time_us_to_timespec(in->next_cache_update);
        pthread_cond_timedwait(&in->wakeup, &in->lock, &until);
    }

    if (in->shutdown_async) {
        pthread_mutex_unlock(&in->lock);
        demux_shutdown(in);
        pthread_mutex_lock(&in->lock);
        in->shutdown_async = false;
        if (in->wakeup_cb)
            in->wakeup_cb(in->wakeup_cb_ctx);
    }

    stats_unregister_thread(in->stats, "thread");

    pthread_mutex_unlock(&in->lock);
    return NULL;
}

// Low-level part of dequeueing a packet.
static struct demux_packet *advance_reader_head(struct demux_stream *ds)
{
    struct demux_packet *pkt = ds->reader_head;
    if (!pkt)
        return NULL;

    ds->reader_head = pkt->next;

    ds->last_ret_pos = pkt->pos;
    ds->last_ret_dts = pkt->dts;

    return pkt;
}

// Return a newly allocated new packet. The pkt parameter may be either a
// in-memory packet (then a new reference is made), or a reference to
// packet in the disk cache (then the packet is read from disk).
static struct demux_packet *read_packet_from_cache(struct demux_internal *in,
                                                   struct demux_packet *pkt)
{
    if (!pkt)
        return NULL;

    if (pkt->is_cached) {
        assert(in->cache);
        struct demux_packet *meta = pkt;
        pkt = demux_cache_read(in->cache, pkt->cached_data.pos);
        if (pkt) {
            demux_packet_copy_attribs(pkt, meta);
        } else {
            MP_ERR(in, "Failed to retrieve packet from cache.\n");
        }
    } else {
        // The returned packet is mutated etc. and will be owned by the user.
        pkt = demux_copy_packet(pkt);
    }

    return pkt;
}

// Returns:
//   < 0: EOF was reached, *res is not set
//  == 0: no new packet yet, wait, *res is not set
//   > 0: new packet is moved to *res
static int dequeue_packet(struct demux_stream *ds, double min_pts,
                          struct demux_packet **res)
{
    struct demux_internal *in = ds->in;

    if (!ds->selected)
        return -1;
    if (in->blocked)
        return 0;

    if (ds->sh->attached_picture) {
        ds->eof = true;
        if (ds->attached_picture_added)
            return -1;
        ds->attached_picture_added = true;
        struct demux_packet *pkt = demux_copy_packet(ds->sh->attached_picture);
        if (!pkt)
            abort();
        pkt->stream = ds->sh->index;
        *res = pkt;
        return 1;
    }

    if (!in->reading && (!in->eof || in->opts->force_retry_eof)) {
        in->reading = true; // enable demuxer thread prefetching
        pthread_cond_signal(&in->wakeup);
    }

    ds->force_read_until = min_pts;

    if (ds->back_resuming || ds->back_restarting) {
        assert(in->back_demuxing);
        return 0;
    }

    bool eof = !ds->reader_head && ds->eof;

    if (in->back_demuxing) {
        // Subtitles not supported => EOF.
        if (!ds->eager)
            return -1;

        // Next keyframe (or EOF) was reached => step back.
        if (ds->back_range_started && !ds->back_range_count &&
            ((ds->reader_head && ds->reader_head->keyframe) || eof))
        {
            ds->back_restarting = true;
            ds->back_restart_eof = false;
            ds->back_restart_next = false;

            find_backward_restart_pos(ds);

            if (ds->back_restarting)
                return 0;
        }

        eof = ds->back_range_count < 0;
    }

    ds->need_wakeup = !ds->reader_head;
    if (!ds->reader_head || eof) {
        if (!ds->eager) {
            // Non-eager streams temporarily return EOF. If they returned 0,
            // the reader would have to wait for new packets, which does not
            // make sense due to the sparseness and passiveness of non-eager
            // streams.
            // Unless the min_pts feature is used: then EOF is only signaled
            // if read-ahead went above min_pts.
            if (!lazy_stream_needs_wait(ds))
                ds->eof = eof = true;
        }
        return eof ? -1 : 0;
    }

    struct demux_packet *pkt = advance_reader_head(ds);
    assert(pkt);
    pkt = read_packet_from_cache(in, pkt);
    if (!pkt)
        return 0;

    if (in->back_demuxing) {
        if (pkt->keyframe) {
            assert(ds->back_range_count > 0);
            ds->back_range_count -= 1;
            if (ds->back_range_preroll >= 0)
                ds->back_range_preroll -= 1;
        }

        if (ds->back_range_preroll >= 0)
            pkt->back_preroll = true;

        if (!ds->back_range_started) {
            pkt->back_restart = true;
            ds->back_range_started = true;
        }
    }

    double ts = MP_PTS_OR_DEF(pkt->dts, pkt->pts);
    if (ts != MP_NOPTS_VALUE)
        ds->base_ts = ts;

    if (pkt->keyframe && ts != MP_NOPTS_VALUE) {
        // Update bitrate - only at keyframe points, because we use the
        // (possibly) reordered packet timestamps instead of realtime.
        double d = ts - ds->last_br_ts;
        if (ds->last_br_ts == MP_NOPTS_VALUE || d < 0) {
            ds->bitrate = -1;
            ds->last_br_ts = ts;
            ds->last_br_bytes = 0;
        } else if (d >= 0.5) { // a window of least 500ms for UI purposes
            ds->bitrate = ds->last_br_bytes / d;
            ds->last_br_ts = ts;
            ds->last_br_bytes = 0;
        }
    }
    ds->last_br_bytes += pkt->len;

    // This implies this function is actually called from "the" user thread.
    if (pkt->pos >= in->d_user->filepos)
        in->d_user->filepos = pkt->pos;
    in->d_user->filesize = in->stream_size;

    pkt->pts = MP_ADD_PTS(pkt->pts, in->ts_offset);
    pkt->dts = MP_ADD_PTS(pkt->dts, in->ts_offset);

    if (pkt->segmented) {
        pkt->start = MP_ADD_PTS(pkt->start, in->ts_offset);
        pkt->end = MP_ADD_PTS(pkt->end, in->ts_offset);
    }

    prune_old_packets(in);
    *res = pkt;
    return 1;
}

// Poll the demuxer queue, and if there's a packet, return it. Otherwise, just
// make the demuxer thread read packets for this stream, and if there's at
// least one packet, call the wakeup callback.
// This enables readahead if it wasn't yet (except for interleaved subtitles).
// Returns:
//   < 0: EOF was reached, *out_pkt=NULL
//  == 0: no new packet yet, but maybe later, *out_pkt=NULL
//   > 0: new packet read, *out_pkt is set
// Note: when reading interleaved subtitles, the demuxer won't try to forcibly
// read ahead to get the next subtitle packet (as the next packet could be
// minutes away). In this situation, this function will just return -1.
int demux_read_packet_async(struct sh_stream *sh, struct demux_packet **out_pkt)
{
    return demux_read_packet_async_until(sh, MP_NOPTS_VALUE, out_pkt);
}

// Like demux_read_packet_async(). They are the same for min_pts==MP_NOPTS_VALUE.
// If min_pts is set, and the stream is lazily read (eager=false, interleaved
// subtitles), then return 0 until demuxing has reached min_pts, or the queue
// overflowed, or EOF was reached, or a packet was read for this stream.
int demux_read_packet_async_until(struct sh_stream *sh, double min_pts,
                                  struct demux_packet **out_pkt)
{
    struct demux_stream *ds = sh ? sh->ds : NULL;
    *out_pkt = NULL;
    if (!ds)
        return -1;
    struct demux_internal *in = ds->in;

    pthread_mutex_lock(&in->lock);
    int r = -1;
    while (1) {
        r = dequeue_packet(ds, min_pts, out_pkt);
        if (in->threading || in->blocked || r != 0)
            break;
        // Needs to actually read packets until we got a packet or EOF.
        thread_work(in);
    }
    pthread_mutex_unlock(&in->lock);
    return r;
}

// Read and return any packet we find. NULL means EOF.
// Does not work with threading (don't call demux_start_thread()).
struct demux_packet *demux_read_any_packet(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    assert(!in->threading); // doesn't work with threading
    struct demux_packet *out_pkt = NULL;
    bool read_more = true;
    while (read_more && !in->blocked) {
        bool all_eof = true;
        for (int n = 0; n < in->num_streams; n++) {
            int r = dequeue_packet(in->streams[n]->ds, MP_NOPTS_VALUE, &out_pkt);
            if (r > 0)
                goto done;
            if (r == 0)
                all_eof = false;
        }
        // retry after calling this
        read_more = thread_work(in);
        read_more &= !all_eof;
    }
done:
    pthread_mutex_unlock(&in->lock);
    return out_pkt;
}

int demuxer_help(struct mp_log *log, const m_option_t *opt, struct bstr name)
{
    int i;

    mp_info(log, "Available demuxers:\n");
    mp_info(log, " demuxer:   info:\n");
    for (i = 0; demuxer_list[i]; i++) {
        mp_info(log, "%10s  %s\n",
                demuxer_list[i]->name, demuxer_list[i]->desc);
    }
    mp_info(log, "\n");

    return M_OPT_EXIT;
}

static const char *d_level(enum demux_check level)
{
    switch (level) {
    case DEMUX_CHECK_FORCE:  return "force";
    case DEMUX_CHECK_UNSAFE: return "unsafe";
    case DEMUX_CHECK_REQUEST:return "request";
    case DEMUX_CHECK_NORMAL: return "normal";
    }
    abort();
}

static int decode_float(char *str, float *out)
{
    char *rest;
    float dec_val;

    dec_val = strtod(str, &rest);
    if (!rest || (rest == str) || !isfinite(dec_val))
        return -1;

    *out = dec_val;
    return 0;
}

static int decode_gain(struct mp_log *log, struct mp_tags *tags,
                       const char *tag, float *out)
{
    char *tag_val = NULL;
    float dec_val;

    tag_val = mp_tags_get_str(tags, tag);
    if (!tag_val)
        return -1;

    if (decode_float(tag_val, &dec_val) < 0) {
        mp_msg(log, MSGL_ERR, "Invalid replaygain value\n");
        return -1;
    }

    *out = dec_val;
    return 0;
}

static int decode_peak(struct mp_log *log, struct mp_tags *tags,
                       const char *tag, float *out)
{
    char *tag_val = NULL;
    float dec_val;

    *out = 1.0;

    tag_val = mp_tags_get_str(tags, tag);
    if (!tag_val)
        return 0;

    if (decode_float(tag_val, &dec_val) < 0 || dec_val <= 0.0)
        return -1;

    *out = dec_val;
    return 0;
}

static struct replaygain_data *decode_rgain(struct mp_log *log,
                                            struct mp_tags *tags)
{
    struct replaygain_data rg = {0};

    // Set values in *rg, using track gain as a fallback for album gain if the
    // latter is not present. This behavior matches that in demux/demux_lavf.c's
    // export_replaygain; if you change this, please make equivalent changes
    // there too.
    if (decode_gain(log, tags, "REPLAYGAIN_TRACK_GAIN", &rg.track_gain) >= 0 &&
        decode_peak(log, tags, "REPLAYGAIN_TRACK_PEAK", &rg.track_peak) >= 0)
    {
        if (decode_gain(log, tags, "REPLAYGAIN_ALBUM_GAIN", &rg.album_gain) < 0 ||
            decode_peak(log, tags, "REPLAYGAIN_ALBUM_PEAK", &rg.album_peak) < 0)
        {
            // Album gain is undefined; fall back to track gain.
            rg.album_gain = rg.track_gain;
            rg.album_peak = rg.track_peak;
        }
        return talloc_dup(NULL, &rg);
    }

    if (decode_gain(log, tags, "REPLAYGAIN_GAIN", &rg.track_gain) >= 0 &&
        decode_peak(log, tags, "REPLAYGAIN_PEAK", &rg.track_peak) >= 0)
    {
        rg.album_gain = rg.track_gain;
        rg.album_peak = rg.track_peak;
        return talloc_dup(NULL, &rg);
    }

    return NULL;
}

static void demux_update_replaygain(demuxer_t *demuxer)
{
    struct demux_internal *in = demuxer->in;
    for (int n = 0; n < in->num_streams; n++) {
        struct sh_stream *sh = in->streams[n];
        if (sh->type == STREAM_AUDIO && !sh->codec->replaygain_data) {
            struct replaygain_data *rg = decode_rgain(demuxer->log, sh->tags);
            if (!rg)
                rg = decode_rgain(demuxer->log, demuxer->metadata);
            if (rg)
                sh->codec->replaygain_data = talloc_steal(in, rg);
        }
    }
}

// Copy some fields from src to dst (for initialization).
static void demux_copy(struct demuxer *dst, struct demuxer *src)
{
    // Note that we do as shallow copies as possible. We expect the data
    // that is not-copied (only referenced) to be immutable.
    // This implies e.g. that no chapters are added after initialization.
    dst->chapters = src->chapters;
    dst->num_chapters = src->num_chapters;
    dst->editions = src->editions;
    dst->num_editions = src->num_editions;
    dst->edition = src->edition;
    dst->attachments = src->attachments;
    dst->num_attachments = src->num_attachments;
    dst->matroska_data = src->matroska_data;
    dst->playlist = src->playlist;
    dst->seekable = src->seekable;
    dst->partially_seekable = src->partially_seekable;
    dst->filetype = src->filetype;
    dst->ts_resets_possible = src->ts_resets_possible;
    dst->fully_read = src->fully_read;
    dst->start_time = src->start_time;
    dst->duration = src->duration;
    dst->is_network = src->is_network;
    dst->is_streaming = src->is_streaming;
    dst->stream_origin = src->stream_origin;
    dst->priv = src->priv;
    dst->metadata = mp_tags_dup(dst, src->metadata);
}

// Update metadata after initialization. If sh==NULL, it's global metadata,
// otherwise it's bound to the stream. If pts==NOPTS, use the highest known pts
// in the stream. Caller retains ownership of tags ptr. Called locked.
static void add_timed_metadata(struct demux_internal *in, struct mp_tags *tags,
                               struct sh_stream *sh, double pts)
{
    struct demux_cached_range *r = in->current_range;
    if (!r)
        return;

    // We don't expect this, nor do we find it useful.
    if (sh && sh != in->metadata_stream)
        return;

    if (pts == MP_NOPTS_VALUE) {
        for (int n = 0; n < r->num_streams; n++)
            pts = MP_PTS_MAX(pts, r->streams[n]->last_ts);

        // Tends to happen when doing the initial icy update.
        if (pts == MP_NOPTS_VALUE)
            pts = in->d_thread->start_time;
    }

    struct timed_metadata *tm = talloc_zero(NULL, struct timed_metadata);
    *tm = (struct timed_metadata){
        .pts = pts,
        .tags = mp_tags_dup(tm, tags),
        .from_stream = !!sh,
    };
    MP_TARRAY_APPEND(r, r->metadata, r->num_metadata, tm);
}

// This is called by demuxer implementations if sh->tags changed. Note that
// sh->tags itself is never actually changed (it's immutable, because sh->tags
// can be accessed by the playback thread, and there is no synchronization).
// pts is the time at/after which the metadata becomes effective. You're
// supposed to call this ordered by time, and only while a packet is being
// read.
// Ownership of tags goes to the function.
void demux_stream_tags_changed(struct demuxer *demuxer, struct sh_stream *sh,
                               struct mp_tags *tags, double pts)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_thread);
    struct demux_stream *ds = sh ? sh->ds : NULL;
    assert(!sh || ds); // stream must have been added

    pthread_mutex_lock(&in->lock);

    if (pts == MP_NOPTS_VALUE) {
        MP_WARN(in, "Discarding timed metadata without timestamp.\n");
    } else {
        add_timed_metadata(in, tags, sh, pts);
    }
    talloc_free(tags);

    pthread_mutex_unlock(&in->lock);
}

// This is called by demuxer implementations if demuxer->metadata changed.
// (It will be propagated to the user as timed metadata.)
void demux_metadata_changed(demuxer_t *demuxer)
{
    assert(demuxer == demuxer->in->d_thread); // call from demuxer impl. only
    struct demux_internal *in = demuxer->in;

    pthread_mutex_lock(&in->lock);
    add_timed_metadata(in, demuxer->metadata, NULL, MP_NOPTS_VALUE);
    pthread_mutex_unlock(&in->lock);
}

// Called locked, with user demuxer.
static void update_final_metadata(demuxer_t *demuxer, struct timed_metadata *tm)
{
    assert(demuxer == demuxer->in->d_user);
    struct demux_internal *in = demuxer->in;

    struct mp_tags *dyn_tags = NULL;

    // Often useful for audio-only files, which have metadata in the audio track
    // metadata instead of the main metadata, but can also have cover art
    // metadata (which libavformat likes to treat as video streams).
    int astreams = 0;
    int astream_id = -1;
    int vstreams = 0;
    for (int n = 0; n < in->num_streams; n++) {
        struct sh_stream *sh = in->streams[n];
        if (sh->type == STREAM_VIDEO && !sh->attached_picture)
            vstreams += 1;
        if (sh->type == STREAM_AUDIO) {
            astreams += 1;
            astream_id = n;
        }
    }

    // Use the metadata_stream tags only if this really seems to be an audio-
    // only stream. Otherwise it will happen too often that "uninteresting"
    // stream metadata will trash the actual file tags.
    if (vstreams == 0 && astreams == 1 &&
        in->streams[astream_id] == in->metadata_stream)
    {
        dyn_tags = in->metadata_stream->tags;
        if (tm && tm->from_stream)
            dyn_tags = tm->tags;
    }

    // Global metadata updates.
    if (tm && !tm->from_stream)
        dyn_tags = tm->tags;

    if (dyn_tags)
        mp_tags_merge(demuxer->metadata, dyn_tags);
}

static struct timed_metadata *lookup_timed_metadata(struct demux_internal *in,
                                                    double pts)
{
    struct demux_cached_range *r = in->current_range;

    if (!r || !r->num_metadata || pts == MP_NOPTS_VALUE)
        return NULL;

    int start = 1;
    int i = in->cached_metadata_index;
    if (i >= 0 &&  i < r->num_metadata && r->metadata[i]->pts <= pts)
        start = i + 1;

    in->cached_metadata_index = r->num_metadata - 1;
    for (int n = start; n < r->num_metadata; n++) {
        if (r->metadata[n]->pts >= pts) {
            in->cached_metadata_index = n - 1;
            break;
        }
    }

    return r->metadata[in->cached_metadata_index];
}

// Called by the user thread (i.e. player) to update metadata and other things
// from the demuxer thread.
// The pts parameter is the current playback position.
void demux_update(demuxer_t *demuxer, double pts)
{
    assert(demuxer == demuxer->in->d_user);
    struct demux_internal *in = demuxer->in;

    pthread_mutex_lock(&in->lock);

    if (!in->threading)
        update_cache(in);

    // This implies this function is actually called from "the" user thread.
    in->d_user->filesize = in->stream_size;

    pts = MP_ADD_PTS(pts, -in->ts_offset);

    struct timed_metadata *prev = lookup_timed_metadata(in, in->last_playback_pts);
    struct timed_metadata *cur = lookup_timed_metadata(in, pts);
    if (prev != cur || in->force_metadata_update) {
        in->force_metadata_update = false;
        update_final_metadata(demuxer, cur);
        demuxer->events |= DEMUX_EVENT_METADATA;
    }

    in->last_playback_pts = pts;

    demuxer->events |= in->events;
    in->events = 0;
    if (demuxer->events & (DEMUX_EVENT_METADATA | DEMUX_EVENT_STREAMS))
        demux_update_replaygain(demuxer);
    if (demuxer->events & DEMUX_EVENT_DURATION)
        demuxer->duration = in->duration;

    pthread_mutex_unlock(&in->lock);
}

static void demux_init_cuesheet(struct demuxer *demuxer)
{
    char *cue = mp_tags_get_str(demuxer->metadata, "cuesheet");
    if (cue && !demuxer->num_chapters) {
        struct cue_file *f = mp_parse_cue(bstr0(cue));
        if (f) {
            if (mp_check_embedded_cue(f) < 0) {
                MP_WARN(demuxer, "Embedded cue sheet references more than one file. "
                        "Ignoring it.\n");
            } else {
                for (int n = 0; n < f->num_tracks; n++) {
                    struct cue_track *t = &f->tracks[n];
                    int idx = demuxer_add_chapter(demuxer, "", t->start, -1);
                    mp_tags_merge(demuxer->chapters[idx].metadata, t->tags);
                }
            }
        }
        talloc_free(f);
    }
}

// A demuxer can use this during opening if all data was read from the stream.
// Calling this after opening was completed is not allowed. Also, if opening
// failed, this must not be called (or trying another demuxer would fail).
// Useful so that e.g. subtitles don't keep the file or socket open.
// If there's ever the situation where we can't allow the demuxer to close
// the stream, this function could ignore the request.
void demux_close_stream(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(!in->threading && demuxer == in->d_thread);

    if (!demuxer->stream || !in->owns_stream)
        return;

    MP_VERBOSE(demuxer, "demuxer read all data; closing stream\n");
    free_stream(demuxer->stream);
    demuxer->stream = NULL;
    in->d_user->stream = NULL;
}

static void demux_init_ccs(struct demuxer *demuxer, struct demux_opts *opts)
{
    struct demux_internal *in = demuxer->in;
    if (!opts->create_ccs)
        return;
    pthread_mutex_lock(&in->lock);
    for (int n = 0; n < in->num_streams; n++) {
        struct sh_stream *sh = in->streams[n];
        if (sh->type == STREAM_VIDEO && !sh->attached_picture)
            demuxer_get_cc_track_locked(sh);
    }
    pthread_mutex_unlock(&in->lock);
}

// Return whether "heavy" caching on this stream is enabled. By default, this
// corresponds to whether the source stream is considered in the network. The
// only effect should be adjusting display behavior (of cache stats etc.), and
// possibly switching between which set of options influence cache settings.
bool demux_is_network_cached(demuxer_t *demuxer)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    bool r = in->using_network_cache_opts;
    pthread_mutex_unlock(&in->lock);
    return r;
}

struct parent_stream_info {
    bool seekable;
    bool is_network;
    bool is_streaming;
    int stream_origin;
    struct mp_cancel *cancel;
    char *filename;
};

static struct demuxer *open_given_type(struct mpv_global *global,
                                       struct mp_log *log,
                                       const struct demuxer_desc *desc,
                                       struct stream *stream,
                                       struct parent_stream_info *sinfo,
                                       struct demuxer_params *params,
                                       enum demux_check check)
{
    if (mp_cancel_test(sinfo->cancel))
        return NULL;

    struct demuxer *demuxer = talloc_ptrtype(NULL, demuxer);
    struct m_config_cache *opts_cache =
        m_config_cache_alloc(demuxer, global, &demux_conf);
    struct demux_opts *opts = opts_cache->opts;
    *demuxer = (struct demuxer) {
        .desc = desc,
        .stream = stream,
        .cancel = sinfo->cancel,
        .seekable = sinfo->seekable,
        .filepos = -1,
        .global = global,
        .log = mp_log_new(demuxer, log, desc->name),
        .glog = log,
        .filename = talloc_strdup(demuxer, sinfo->filename),
        .is_network = sinfo->is_network,
        .is_streaming = sinfo->is_streaming,
        .stream_origin = sinfo->stream_origin,
        .access_references = opts->access_references,
        .events = DEMUX_EVENT_ALL,
        .duration = -1,
    };

    struct demux_internal *in = demuxer->in = talloc_ptrtype(demuxer, in);
    *in = (struct demux_internal){
        .global = global,
        .log = demuxer->log,
        .stats = stats_ctx_create(in, global, "demuxer"),
        .can_cache = params && params->is_top_level,
        .can_record = params && params->stream_record,
        .opts = opts,
        .opts_cache = opts_cache,
        .d_thread = talloc(demuxer, struct demuxer),
        .d_user = demuxer,
        .after_seek = true, // (assumed identical to initial demuxer state)
        .after_seek_to_start = true,
        .highest_av_pts = MP_NOPTS_VALUE,
        .seeking_in_progress = MP_NOPTS_VALUE,
        .demux_ts = MP_NOPTS_VALUE,
        .owns_stream = !params->external_stream,
    };
    pthread_mutex_init(&in->lock, NULL);
    pthread_cond_init(&in->wakeup, NULL);

    *in->d_thread = *demuxer;

    in->d_thread->metadata = talloc_zero(in->d_thread, struct mp_tags);

    mp_dbg(log, "Trying demuxer: %s (force-level: %s)\n",
           desc->name, d_level(check));

    if (stream)
        stream_seek(stream, 0);

    in->d_thread->params = params; // temporary during open()
    int ret = demuxer->desc->open(in->d_thread, check);
    if (ret >= 0) {
        in->d_thread->params = NULL;
        if (in->d_thread->filetype)
            mp_verbose(log, "Detected file format: %s (%s)\n",
                       in->d_thread->filetype, desc->desc);
        else
            mp_verbose(log, "Detected file format: %s\n", desc->desc);
        if (!in->d_thread->seekable)
            mp_verbose(log, "Stream is not seekable.\n");
        if (!in->d_thread->seekable && opts->force_seekable) {
            mp_warn(log, "Not seekable, but enabling seeking on user request.\n");
            in->d_thread->seekable = true;
            in->d_thread->partially_seekable = true;
        }
        demux_init_cuesheet(in->d_thread);
        demux_init_ccs(demuxer, opts);
        demux_convert_tags_charset(in->d_thread);
        demux_copy(in->d_user, in->d_thread);
        in->duration = in->d_thread->duration;
        demuxer_sort_chapters(demuxer);
        in->events = DEMUX_EVENT_ALL;

        struct demuxer *sub = NULL;
        if (!(params && params->disable_timeline)) {
            struct timeline *tl = timeline_load(global, log, demuxer);
            if (tl) {
                struct demuxer_params params2 = {0};
                params2.timeline = tl;
                params2.is_top_level = params && params->is_top_level;
                params2.stream_record = params && params->stream_record;
                sub =
                    open_given_type(global, log, &demuxer_desc_timeline,
                                    NULL, sinfo, &params2, DEMUX_CHECK_FORCE);
                if (sub) {
                    in->can_cache = false;
                    in->can_record = false;
                } else {
                    timeline_destroy(tl);
                }
            }
        }

        switch_to_fresh_cache_range(in);

        update_opts(in);

        demux_update(demuxer, MP_NOPTS_VALUE);

        demuxer = sub ? sub : demuxer;
        return demuxer;
    }

    demuxer->stream = NULL;
    demux_free(demuxer);
    return NULL;
}

static const int d_normal[]  = {DEMUX_CHECK_NORMAL, DEMUX_CHECK_UNSAFE, -1};
static const int d_request[] = {DEMUX_CHECK_REQUEST, -1};
static const int d_force[]   = {DEMUX_CHECK_FORCE, -1};

// params can be NULL
// This may free the stream parameter on success.
static struct demuxer *demux_open(struct stream *stream,
                                  struct mp_cancel *cancel,
                                  struct demuxer_params *params,
                                  struct mpv_global *global)
{
    const int *check_levels = d_normal;
    const struct demuxer_desc *check_desc = NULL;
    struct mp_log *log = mp_log_new(NULL, global->log, "!demux");
    struct demuxer *demuxer = NULL;
    char *force_format = params ? params->force_format : NULL;

    if (!force_format)
        force_format = stream->demuxer;

    if (force_format && force_format[0] && !stream->is_directory) {
        check_levels = d_request;
        if (force_format[0] == '+') {
            force_format += 1;
            check_levels = d_force;
        }
        for (int n = 0; demuxer_list[n]; n++) {
            if (strcmp(demuxer_list[n]->name, force_format) == 0)
                check_desc = demuxer_list[n];
        }
        if (!check_desc) {
            mp_err(log, "Demuxer %s does not exist.\n", force_format);
            goto done;
        }
    }

    struct parent_stream_info sinfo = {
        .seekable = stream->seekable,
        .is_network = stream->is_network,
        .is_streaming = stream->streaming,
        .stream_origin = stream->stream_origin,
        .cancel = cancel,
        .filename = talloc_strdup(NULL, stream->url),
    };

    // Test demuxers from first to last, one pass for each check_levels[] entry
    for (int pass = 0; check_levels[pass] != -1; pass++) {
        enum demux_check level = check_levels[pass];
        mp_verbose(log, "Trying demuxers for level=%s.\n", d_level(level));
        for (int n = 0; demuxer_list[n]; n++) {
            const struct demuxer_desc *desc = demuxer_list[n];
            if (!check_desc || desc == check_desc) {
                demuxer = open_given_type(global, log, desc, stream, &sinfo,
                                          params, level);
                if (demuxer) {
                    talloc_steal(demuxer, log);
                    log = NULL;
                    goto done;
                }
            }
        }
    }

done:
    talloc_free(sinfo.filename);
    talloc_free(log);
    return demuxer;
}

static struct stream *create_webshit_concat_stream(struct mpv_global *global,
                                                   struct mp_cancel *c,
                                                   bstr init, struct stream *real)
{
    struct stream *mem = stream_memory_open(global, init.start, init.len);
    assert(mem);

    struct stream *streams[2] = {mem, real};
    struct stream *concat = stream_concat_open(global, c, streams, 2);
    if (!concat) {
        free_stream(mem);
        free_stream(real);
    }
    return concat;
}

// Convenience function: open the stream, enable the cache (according to params
// and global opts.), open the demuxer.
// Also for some reason may close the opened stream if it's not needed.
// demuxer->cancel is not the cancel parameter, but is its own object that will
// be a slave (mp_cancel_set_parent()) to provided cancel object.
// demuxer->cancel is automatically freed.
struct demuxer *demux_open_url(const char *url,
                               struct demuxer_params *params,
                               struct mp_cancel *cancel,
                               struct mpv_global *global)
{
    if (!params)
        return NULL;
    struct mp_cancel *priv_cancel = mp_cancel_new(NULL);
    if (cancel)
        mp_cancel_set_parent(priv_cancel, cancel);
    struct stream *s = params->external_stream;
    if (!s) {
        s = stream_create(url, STREAM_READ | params->stream_flags,
                          priv_cancel, global);
        if (s && params->init_fragment.len) {
            s = create_webshit_concat_stream(global, priv_cancel,
                                             params->init_fragment, s);
        }
    }
    if (!s) {
        talloc_free(priv_cancel);
        return NULL;
    }
    struct demuxer *d = demux_open(s, priv_cancel, params, global);
    if (d) {
        talloc_steal(d->in, priv_cancel);
        assert(d->cancel);
    } else {
        params->demuxer_failed = true;
        if (!params->external_stream)
            free_stream(s);
        talloc_free(priv_cancel);
    }
    return d;
}

// clear the packet queues
void demux_flush(demuxer_t *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    pthread_mutex_lock(&in->lock);
    clear_reader_state(in, true);
    for (int n = 0; n < in->num_ranges; n++)
        clear_cached_range(in, in->ranges[n]);
    free_empty_cached_ranges(in);
    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;
        ds->refreshing = false;
        ds->eof = false;
    }
    in->eof = false;
    in->seeking = false;
    pthread_mutex_unlock(&in->lock);
}

// Does some (but not all) things for switching to another range.
static void switch_current_range(struct demux_internal *in,
                                 struct demux_cached_range *range)
{
    struct demux_cached_range *old = in->current_range;
    assert(old != range);

    set_current_range(in, range);

    if (old) {
        // Remove packets which can't be used when seeking back to the range.
        for (int n = 0; n < in->num_streams; n++) {
            struct demux_queue *queue = old->streams[n];

            // Remove all packets which cannot be involved in seeking.
            while (queue->head && !queue->head->keyframe)
                remove_head_packet(queue);
        }

        // Exclude weird corner cases that break resuming.
        for (int n = 0; n < in->num_streams; n++) {
            struct demux_stream *ds = in->streams[n]->ds;
            // This is needed to resume or join the range at all.
            if (ds->selected && !(ds->global_correct_dts ||
                                  ds->global_correct_pos))
            {
                MP_VERBOSE(in, "discarding unseekable range due to stream %d\n", n);
                clear_cached_range(in, old);
                break;
            }
        }
    }

    // Set up reading from new range (as well as writing to it).
    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;

        ds->queue = range->streams[n];
        ds->refreshing = false;
        ds->eof = false;
    }

    // No point in keeping any junk (especially if old current_range is empty).
    free_empty_cached_ranges(in);

    // The change detection doesn't work across ranges.
    in->force_metadata_update = true;
}

// Search for the entry with the highest index with entry.pts <= pts true.
static struct demux_packet *search_index(struct demux_queue *queue, double pts)
{
    size_t a = 0;
    size_t b = queue->num_index;

    while (a < b) {
        size_t m = a + (b - a) / 2;
        struct index_entry *e = &QUEUE_INDEX_ENTRY(queue, m);

        bool m_ok = e->pts <= pts;

        if (a + 1 == b)
            return m_ok ? e->pkt : NULL;

        if (m_ok) {
            a = m;
        } else {
            b = m;
        }
    }

    return NULL;
}

static struct demux_packet *find_seek_target(struct demux_queue *queue,
                                             double pts, int flags)
{
    pts -= queue->ds->sh->seek_preroll;

    struct demux_packet *start = search_index(queue, pts);
    if (!start)
        start = queue->head;

    struct demux_packet *target = NULL;
    struct demux_packet *next = NULL;
    for (struct demux_packet *dp = start; dp; dp = next) {
        next = dp->next;
        if (!dp->keyframe)
            continue;

        double range_pts;
        next = compute_keyframe_times(dp, &range_pts, NULL);

        if (range_pts == MP_NOPTS_VALUE)
            continue;

        if (flags & SEEK_FORWARD) {
            // Stop on the first packet that is >= pts.
            if (target)
                break;
            if (range_pts < pts)
                continue;
        } else {
            // Stop before the first packet that is > pts.
            // This still returns a packet with > pts if there's no better one.
            if (target && range_pts > pts)
                break;
        }

        target = dp;
    }

    return target;
}

// Return a cache range for the given pts/flags, or NULL if none available.
// must be called locked
static struct demux_cached_range *find_cache_seek_range(struct demux_internal *in,
                                                        double pts, int flags)
{
    // Note about queued low level seeks: in->seeking can be true here, and it
    // might come from a previous resume seek to the current range. If we end
    // up seeking into the current range (i.e. just changing time offset), the
    // seek needs to continue. Otherwise, we override the queued seek anyway.
    if ((flags & SEEK_FACTOR) || !in->seekable_cache)
        return NULL;

    struct demux_cached_range *res = NULL;

    for (int n = 0; n < in->num_ranges; n++) {
        struct demux_cached_range *r = in->ranges[n];
        if (r->seek_start != MP_NOPTS_VALUE) {
            MP_VERBOSE(in, "cached range %d: %f <-> %f (bof=%d, eof=%d)\n",
                       n, r->seek_start, r->seek_end, r->is_bof, r->is_eof);

            if ((pts >= r->seek_start || r->is_bof) &&
                (pts <= r->seek_end || r->is_eof))
            {
                MP_VERBOSE(in, "...using this range for in-cache seek.\n");
                res = r;
                break;
            }
        }
    }

    return res;
}

// Adjust the seek target to the found video key frames. Otherwise the
// video will undershoot the seek target, while audio will be closer to it.
// The player frontend will play the additional video without audio, so
// you get silent audio for the amount of "undershoot". Adjusting the seek
// target will make the audio seek to the video target or before.
// (If hr-seeks are used, it's better to skip this, as it would only mean
// that more audio data than necessary would have to be decoded.)
static void adjust_cache_seek_target(struct demux_internal *in,
                                     struct demux_cached_range *range,
                                     double *pts, int *flags)
{
    if (*flags & SEEK_HR)
        return;

    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;
        struct demux_queue *queue = range->streams[n];
        if (ds->selected && ds->type == STREAM_VIDEO) {
            struct demux_packet *target = find_seek_target(queue, *pts, *flags);
            if (target) {
                double target_pts;
                compute_keyframe_times(target, &target_pts, NULL);
                if (target_pts != MP_NOPTS_VALUE) {
                    MP_VERBOSE(in, "adjust seek target %f -> %f\n",
                               *pts, target_pts);
                    // (We assume the find_seek_target() call will return
                    // the same target for the video stream.)
                    *pts = target_pts;
                    *flags &= ~SEEK_FORWARD;
                }
            }
            break;
        }
    }
}

// must be called locked
// range must be non-NULL and from find_cache_seek_range() using the same pts
// and flags, before any other changes to the cached state
static void execute_cache_seek(struct demux_internal *in,
                               struct demux_cached_range *range,
                               double pts, int flags)
{
    adjust_cache_seek_target(in, range, &pts, &flags);

    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;
        struct demux_queue *queue = range->streams[n];

        struct demux_packet *target = find_seek_target(queue, pts, flags);
        ds->reader_head = target;
        ds->skip_to_keyframe = !target;
        if (ds->reader_head)
            ds->base_ts = MP_PTS_OR_DEF(ds->reader_head->pts, ds->reader_head->dts);

        MP_VERBOSE(in, "seeking stream %d (%s) to ",
                   n, stream_type_name(ds->type));

        if (target) {
            MP_VERBOSE(in, "packet %f/%f\n", target->pts, target->dts);
        } else {
            MP_VERBOSE(in, "nothing\n");
        }
    }

    // If we seek to another range, we want to seek the low level demuxer to
    // there as well, because reader and demuxer queue must be the same.
    if (in->current_range != range) {
        switch_current_range(in, range);

        in->seeking = true;
        in->seek_flags = SEEK_HR;
        in->seek_pts = range->seek_end - 1.0;

        // When new packets are being appended, they could overlap with the old
        // range due to demuxer seek imprecisions, or because the queue contains
        // packets past the seek target but before the next seek target. Don't
        // append them twice, instead skip them until new packets are found.
        for (int n = 0; n < in->num_streams; n++) {
            struct demux_stream *ds = in->streams[n]->ds;

            ds->refreshing = ds->selected;
        }

        MP_VERBOSE(in, "resuming demuxer to end of cached range\n");
    }
}

// Create a new blank cache range, and backup the old one. If the seekable
// demuxer cache is disabled, merely reset the current range to a blank state.
static void switch_to_fresh_cache_range(struct demux_internal *in)
{
    if (!in->seekable_cache && in->current_range) {
        clear_cached_range(in, in->current_range);
        return;
    }

    struct demux_cached_range *range = talloc_ptrtype(NULL, range);
    *range = (struct demux_cached_range){
        .seek_start = MP_NOPTS_VALUE,
        .seek_end = MP_NOPTS_VALUE,
    };
    MP_TARRAY_APPEND(in, in->ranges, in->num_ranges, range);
    add_missing_streams(in, range);

    switch_current_range(in, range);
}

int demux_seek(demuxer_t *demuxer, double seek_pts, int flags)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    pthread_mutex_lock(&in->lock);

    if (!(flags & SEEK_FACTOR))
        seek_pts = MP_ADD_PTS(seek_pts, -in->ts_offset);

    int res = queue_seek(in, seek_pts, flags, true);

    pthread_cond_signal(&in->wakeup);
    pthread_mutex_unlock(&in->lock);

    return res;
}

static bool queue_seek(struct demux_internal *in, double seek_pts, int flags,
                       bool clear_back_state)
{
    if (seek_pts == MP_NOPTS_VALUE)
        return false;

    MP_VERBOSE(in, "queuing seek to %f%s\n", seek_pts,
               in->seeking ? " (cascade)" : "");

    bool require_cache = flags & SEEK_CACHED;
    flags &= ~(unsigned)SEEK_CACHED;

    bool set_backwards = flags & SEEK_SATAN;
    flags &= ~(unsigned)SEEK_SATAN;

    bool force_seek = flags & SEEK_FORCE;
    flags &= ~(unsigned)SEEK_FORCE;

    bool block = flags & SEEK_BLOCK;
    flags &= ~(unsigned)SEEK_BLOCK;

    struct demux_cached_range *cache_target =
        find_cache_seek_range(in, seek_pts, flags);

    if (!cache_target) {
        if (require_cache) {
            MP_VERBOSE(in, "Cached seek not possible.\n");
            return false;
        }
        if (!in->d_thread->seekable && !force_seek) {
            MP_WARN(in, "Cannot seek in this file.\n");
            return false;
        }
    }

    in->eof = false;
    in->reading = false;
    in->back_demuxing = set_backwards;

    clear_reader_state(in, clear_back_state);

    in->blocked = block;

    if (cache_target) {
        execute_cache_seek(in, cache_target, seek_pts, flags);
    } else {
        switch_to_fresh_cache_range(in);

        in->seeking = true;
        in->seek_flags = flags;
        in->seek_pts = seek_pts;
    }

    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;

        if (in->back_demuxing) {
            if (ds->back_seek_pos == MP_NOPTS_VALUE)
                ds->back_seek_pos = seek_pts;
            // Process possibly cached packets.
            back_demux_see_packets(in->streams[n]->ds);
        }

        wakeup_ds(ds);
    }

    if (!in->threading && in->seeking)
        execute_seek(in);

    return true;
}

struct sh_stream *demuxer_stream_by_demuxer_id(struct demuxer *d,
                                               enum stream_type t, int id)
{
    if (id < 0)
        return NULL;
    int num = demux_get_num_stream(d);
    for (int n = 0; n < num; n++) {
        struct sh_stream *s = demux_get_stream(d, n);
        if (s->type == t && s->demuxer_id == id)
            return s;
    }
    return NULL;
}

// An obscure mechanism to get stream switching to be executed "faster" (as
// perceived by the user), by making the stream return packets from the
// current position
// On a switch, it seeks back, and then grabs all packets that were
// "missing" from the packet queue of the newly selected stream.
static void initiate_refresh_seek(struct demux_internal *in,
                                  struct demux_stream *stream,
                                  double start_ts)
{
    struct demuxer *demux = in->d_thread;
    bool seekable = demux->desc->seek && demux->seekable &&
                    !demux->partially_seekable;

    bool normal_seek = true;
    bool refresh_possible = true;
    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;

        if (!ds->selected)
            continue;

        if (ds->type == STREAM_VIDEO || ds->type == STREAM_AUDIO)
            start_ts = MP_PTS_MIN(start_ts, ds->base_ts);

        // If there were no other streams selected, we can use a normal seek.
        normal_seek &= stream == ds;

        refresh_possible &= ds->queue->correct_dts || ds->queue->correct_pos;
    }

    if (start_ts == MP_NOPTS_VALUE || !seekable)
        return;

    if (!normal_seek) {
        if (!refresh_possible) {
            MP_VERBOSE(in, "can't issue refresh seek\n");
            return;
        }

        for (int n = 0; n < in->num_streams; n++) {
            struct demux_stream *ds = in->streams[n]->ds;

            bool correct_pos = ds->queue->correct_pos;
            bool correct_dts = ds->queue->correct_dts;

            // We need to re-read all packets anyway, so discard the buffered
            // data. (In theory, we could keep the packets, and be able to use
            // it for seeking if partially read streams are deselected again,
            // but this causes other problems like queue overflows when
            // selecting a new stream.)
            ds_clear_reader_queue_state(ds);
            clear_queue(ds->queue);

            // Streams which didn't have any packets yet will return all packets,
            // other streams return packets only starting from the last position.
            if (ds->selected && (ds->last_ret_pos != -1 ||
                                 ds->last_ret_dts != MP_NOPTS_VALUE))
            {
                ds->refreshing = true;
                ds->queue->correct_dts = correct_dts;
                ds->queue->correct_pos = correct_pos;
                ds->queue->last_pos = ds->last_ret_pos;
                ds->queue->last_dts = ds->last_ret_dts;
            }

            update_seek_ranges(in->current_range);
        }

        start_ts -= 1.0; // small offset to get correct overlap
    }

    MP_VERBOSE(in, "refresh seek to %f\n", start_ts);
    in->seeking = true;
    in->seek_flags = SEEK_HR;
    in->seek_pts = start_ts;
}

// Set whether the given stream should return packets.
// ref_pts is used only if the stream is enabled. Then it serves as approximate
// start pts for this stream (in the worst case it is ignored).
void demuxer_select_track(struct demuxer *demuxer, struct sh_stream *stream,
                          double ref_pts, bool selected)
{
    struct demux_internal *in = demuxer->in;
    struct demux_stream *ds = stream->ds;
    pthread_mutex_lock(&in->lock);
    ref_pts = MP_ADD_PTS(ref_pts, -in->ts_offset);
    // don't flush buffers if stream is already selected / unselected
    if (ds->selected != selected) {
        MP_VERBOSE(in, "%sselect track %d\n", selected ? "" : "de", stream->index);
        ds->selected = selected;
        update_stream_selection_state(in, ds);
        in->tracks_switched = true;
        if (ds->selected) {
            if (in->back_demuxing)
                ds->back_seek_pos = ref_pts;
            if (!in->after_seek)
                initiate_refresh_seek(in, ds, ref_pts);
        }
        if (in->threading) {
            pthread_cond_signal(&in->wakeup);
        } else {
            execute_trackswitch(in);
        }
    }
    pthread_mutex_unlock(&in->lock);
}

// Execute a refresh seek on the given stream.
// ref_pts has the same meaning as with demuxer_select_track()
void demuxer_refresh_track(struct demuxer *demuxer, struct sh_stream *stream,
                           double ref_pts)
{
    struct demux_internal *in = demuxer->in;
    struct demux_stream *ds = stream->ds;
    pthread_mutex_lock(&in->lock);
    ref_pts = MP_ADD_PTS(ref_pts, -in->ts_offset);
    if (ds->selected) {
        MP_VERBOSE(in, "refresh track %d\n", stream->index);
        update_stream_selection_state(in, ds);
        if (in->back_demuxing)
            ds->back_seek_pos = ref_pts;
        if (!in->after_seek)
            initiate_refresh_seek(in, ds, ref_pts);
    }
    pthread_mutex_unlock(&in->lock);
}

// This is for demuxer implementations only. demuxer_select_track() sets the
// logical state, while this function returns the actual state (in case the
// demuxer attempts to cache even unselected packets for track switching - this
// will potentially be done in the future).
bool demux_stream_is_selected(struct sh_stream *stream)
{
    if (!stream)
        return false;
    bool r = false;
    pthread_mutex_lock(&stream->ds->in->lock);
    r = stream->ds->selected;
    pthread_mutex_unlock(&stream->ds->in->lock);
    return r;
}

void demux_set_stream_wakeup_cb(struct sh_stream *sh,
                                void (*cb)(void *ctx), void *ctx)
{
    pthread_mutex_lock(&sh->ds->in->lock);
    sh->ds->wakeup_cb = cb;
    sh->ds->wakeup_cb_ctx = ctx;
    sh->ds->need_wakeup = true;
    pthread_mutex_unlock(&sh->ds->in->lock);
}

int demuxer_add_attachment(demuxer_t *demuxer, char *name, char *type,
                           void *data, size_t data_size)
{
    if (!(demuxer->num_attachments % 32))
        demuxer->attachments = talloc_realloc(demuxer, demuxer->attachments,
                                              struct demux_attachment,
                                              demuxer->num_attachments + 32);

    struct demux_attachment *att = &demuxer->attachments[demuxer->num_attachments];
    att->name = talloc_strdup(demuxer->attachments, name);
    att->type = talloc_strdup(demuxer->attachments, type);
    att->data = talloc_memdup(demuxer->attachments, data, data_size);
    att->data_size = data_size;

    return demuxer->num_attachments++;
}

static int chapter_compare(const void *p1, const void *p2)
{
    struct demux_chapter *c1 = (void *)p1;
    struct demux_chapter *c2 = (void *)p2;

    if (c1->pts > c2->pts)
        return 1;
    else if (c1->pts < c2->pts)
        return -1;
    return c1->original_index > c2->original_index ? 1 :-1; // never equal
}

static void demuxer_sort_chapters(demuxer_t *demuxer)
{
    if (demuxer->num_chapters) {
        qsort(demuxer->chapters, demuxer->num_chapters,
            sizeof(struct demux_chapter), chapter_compare);
    }
}

int demuxer_add_chapter(demuxer_t *demuxer, char *name,
                        double pts, uint64_t demuxer_id)
{
    struct demux_chapter new = {
        .original_index = demuxer->num_chapters,
        .pts = pts,
        .metadata = talloc_zero(demuxer, struct mp_tags),
        .demuxer_id = demuxer_id,
    };
    mp_tags_set_str(new.metadata, "TITLE", name);
    MP_TARRAY_APPEND(demuxer, demuxer->chapters, demuxer->num_chapters, new);
    return demuxer->num_chapters - 1;
}

// Disallow reading any packets and make readers think there is no new data
// yet, until a seek is issued.
void demux_block_reading(struct demuxer *demuxer, bool block)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    pthread_mutex_lock(&in->lock);
    in->blocked = block;
    for (int n = 0; n < in->num_streams; n++) {
        in->streams[n]->ds->need_wakeup = true;
        wakeup_ds(in->streams[n]->ds);
    }
    pthread_cond_signal(&in->wakeup);
    pthread_mutex_unlock(&in->lock);
}

static void update_bytes_read(struct demux_internal *in)
{
    struct demuxer *demuxer = in->d_thread;

    int64_t new = in->slave_unbuffered_read_bytes;
    in->slave_unbuffered_read_bytes = 0;

    int64_t new_seeks = 0;

    struct stream *stream = demuxer->stream;
    if (stream) {
        new += stream->total_unbuffered_read_bytes;
        stream->total_unbuffered_read_bytes = 0;
        new_seeks += stream->total_stream_seeks;
        stream->total_stream_seeks = 0;
    }

    in->cache_unbuffered_read_bytes += new;
    in->hack_unbuffered_read_bytes += new;
    in->byte_level_seeks += new_seeks;
}

// must be called locked, temporarily unlocks
static void update_cache(struct demux_internal *in)
{
    struct demuxer *demuxer = in->d_thread;
    struct stream *stream = demuxer->stream;

    int64_t now = mp_time_us();
    int64_t diff = now - in->last_speed_query;
    bool do_update = diff >= MP_SECOND_US;

    // Don't lock while querying the stream.
    pthread_mutex_unlock(&in->lock);

    int64_t stream_size = -1;
    struct mp_tags *stream_metadata = NULL;
    if (stream) {
        if (do_update)
            stream_size = stream_get_size(stream);
        stream_control(stream, STREAM_CTRL_GET_METADATA, &stream_metadata);
    }

    pthread_mutex_lock(&in->lock);

    update_bytes_read(in);

    if (do_update)
        in->stream_size = stream_size;
    if (stream_metadata) {
        add_timed_metadata(in, stream_metadata, NULL, MP_NOPTS_VALUE);
        talloc_free(stream_metadata);
    }

    in->next_cache_update = INT64_MAX;

    if (do_update) {
        uint64_t bytes = in->cache_unbuffered_read_bytes;
        in->cache_unbuffered_read_bytes = 0;
        in->last_speed_query = now;
        double speed = bytes / (diff / (double)MP_SECOND_US);
        in->bytes_per_second = 0.5 * in->speed_query_prev_sample +
                               0.5 * speed;
        in->speed_query_prev_sample = speed;
    }
    // The idea is to update as long as there is "activity".
    if (in->bytes_per_second)
        in->next_cache_update = now + MP_SECOND_US + 1;
}

static void dumper_close(struct demux_internal *in)
{
    if (in->dumper)
        mp_recorder_destroy(in->dumper);
    in->dumper = NULL;
    if (in->dumper_status == CONTROL_TRUE)
        in->dumper_status = CONTROL_FALSE; // make abort equal to success
}

static int range_time_compare(const void *p1, const void *p2)
{
    struct demux_cached_range *r1 = *((struct demux_cached_range **)p1);
    struct demux_cached_range *r2 = *((struct demux_cached_range **)p2);

    if (r1->seek_start == r2->seek_start)
        return 0;
    return r1->seek_start < r2->seek_start ? -1 : 1;
}

static void dump_cache(struct demux_internal *in, double start, double end)
{
    in->dumper_status = in->dumper ? CONTROL_TRUE : CONTROL_ERROR;
    if (!in->dumper)
        return;

    // (only in pathological cases there might be more ranges than allowed)
    struct demux_cached_range *ranges[MAX_SEEK_RANGES];
    int num_ranges = 0;
    for (int n = 0; n < MPMIN(MP_ARRAY_SIZE(ranges), in->num_ranges); n++)
        ranges[num_ranges++] = in->ranges[n];
    qsort(ranges, num_ranges, sizeof(ranges[0]), range_time_compare);

    for (int n = 0; n < num_ranges; n++) {
        struct demux_cached_range *r = ranges[n];
        if (r->seek_start == MP_NOPTS_VALUE)
            continue;
        if (r->seek_end <= start)
            continue;
        if (end != MP_NOPTS_VALUE && r->seek_start >= end)
            continue;

        mp_recorder_mark_discontinuity(in->dumper);

        double pts = start;
        int flags = 0;
        adjust_cache_seek_target(in, r, &pts, &flags);

        for (int i = 0; i < r->num_streams; i++) {
            struct demux_queue *q = r->streams[i];
            struct demux_stream *ds = q->ds;

            ds->dump_pos = find_seek_target(q, pts, flags);
        }

        // We need to reinterleave the separate streams somehow, which makes
        // everything more complex.
        while (1) {
            struct demux_packet *next = NULL;
            double next_dts = MP_NOPTS_VALUE;

            for (int i = 0; i < r->num_streams; i++) {
                struct demux_stream *ds = r->streams[i]->ds;
                struct demux_packet *dp = ds->dump_pos;

                if (!dp)
                    continue;
                assert(dp->stream == ds->index);

                double pdts = MP_PTS_OR_DEF(dp->dts, dp->pts);

                // Check for stream EOF. Note that we don't try to EOF
                // streams at the same point (e.g. video can take longer
                // to finish than audio, so the output file will have no
                // audio for the last part of the video). Too much effort.
                if (pdts != MP_NOPTS_VALUE && end != MP_NOPTS_VALUE &&
                    pdts >= end && dp->keyframe)
                {
                    ds->dump_pos = NULL;
                    continue;
                }

                if (pdts == MP_NOPTS_VALUE || next_dts == MP_NOPTS_VALUE ||
                    pdts < next_dts)
                {
                    next_dts = pdts;
                    next = dp;
                }
            }

            if (!next)
                break;

            struct demux_stream *ds = in->streams[next->stream]->ds;
            ds->dump_pos = next->next;

            struct demux_packet *dp = read_packet_from_cache(in, next);
            if (!dp) {
                in->dumper_status = CONTROL_ERROR;
                break;
            }

            write_dump_packet(in, dp);

            talloc_free(dp);
        }

        if (in->dumper_status != CONTROL_OK)
            break;
    }

    // (strictly speaking unnecessary; for clarity)
    for (int n = 0; n < in->num_streams; n++)
        in->streams[n]->ds->dump_pos = NULL;

    // If dumping (in end==NOPTS mode) doesn't continue at the range that
    // was written last, we have a discontinuity.
    if (num_ranges && ranges[num_ranges - 1] != in->current_range)
        mp_recorder_mark_discontinuity(in->dumper);

    // end=NOPTS means the demuxer output continues to be written to the
    // dump file.
    if (end != MP_NOPTS_VALUE || in->dumper_status != CONTROL_OK)
        dumper_close(in);
}

// Set the current cache dumping mode. There is only at most 1 dump process
// active, so calling this aborts the previous dumping. Passing file==NULL
// stops dumping.
// This is synchronous with demux_cache_dump_get_status() (i.e. starting or
// aborting is not asynchronous). On status change, the demuxer wakeup callback
// is invoked (except for this call).
// Returns whether dumping was logically started.
bool demux_cache_dump_set(struct demuxer *demuxer, double start, double end,
                          char *file)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    bool res = false;

    pthread_mutex_lock(&in->lock);

    start = MP_ADD_PTS(start, -in->ts_offset);
    end = MP_ADD_PTS(end, -in->ts_offset);

    dumper_close(in);

    if (file && file[0] && start != MP_NOPTS_VALUE) {
        res = true;

        in->dumper = recorder_create(in, file);

        // This is not asynchronous and will freeze the shit for a while if the
        // user is unlucky. It could be moved to a thread with some effort.
        // General idea: iterate over all cache ranges, dump what intersects.
        // After that, and if the user requested it, make it dump all newly
        // received packets, even if it's awkward (consider the case if the
        // current range is not the last range).
        dump_cache(in, start, end);
    }

    pthread_mutex_unlock(&in->lock);

    return res;
}

// Returns one of CONTROL_*. CONTROL_TRUE means dumping is in progress.
int demux_cache_dump_get_status(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    int status = in->dumper_status;
    pthread_mutex_unlock(&in->lock);
    return status;
}

// Return what range demux_cache_dump_set() would (probably) yield. This is a
// conservative amount (in addition to internal consistency of this code, it
// depends on what a player will do with the resulting file).
// Use for_end==true to get the end of dumping, other the start.
// Returns NOPTS if nothing was found.
double demux_probe_cache_dump_target(struct demuxer *demuxer, double pts,
                                     bool for_end)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    double res = MP_NOPTS_VALUE;
    if (pts == MP_NOPTS_VALUE)
        return pts;

    pthread_mutex_lock(&in->lock);

    pts = MP_ADD_PTS(pts, -in->ts_offset);

    // (When determining the end, look before the keyframe at pts, so subtract
    // an arbitrary amount to round down.)
    double seek_pts = for_end ? pts - 0.001 : pts;
    int flags = 0;
    struct demux_cached_range *r = find_cache_seek_range(in, seek_pts, flags);
    if (r) {
        if (!for_end)
            adjust_cache_seek_target(in, r, &pts, &flags);

        double t[STREAM_TYPE_COUNT];
        for (int n = 0; n < STREAM_TYPE_COUNT; n++)
            t[n] = MP_NOPTS_VALUE;

        for (int n = 0; n < in->num_streams; n++) {
            struct demux_stream *ds = in->streams[n]->ds;
            struct demux_queue *q = r->streams[n];

            struct demux_packet *dp = find_seek_target(q, pts, flags);
            if (dp) {
                if (for_end) {
                    while (dp) {
                        double pdts = MP_PTS_OR_DEF(dp->dts, dp->pts);

                        if (pdts != MP_NOPTS_VALUE && pdts >= pts && dp->keyframe)
                            break;

                        t[ds->type] = MP_PTS_MAX(t[ds->type], pdts);

                        dp = dp->next;
                    }
                } else {
                    double start;
                    compute_keyframe_times(dp, &start, NULL);
                    start = MP_PTS_MAX(start, r->seek_start);
                    t[ds->type] = MP_PTS_MAX(t[ds->type], start);
                }
            }
        }

        res = t[STREAM_VIDEO];
        if (res == MP_NOPTS_VALUE)
            res = t[STREAM_AUDIO];
        if (res == MP_NOPTS_VALUE) {
            for (int n = 0; n < STREAM_TYPE_COUNT; n++) {
                res = t[n];
                if (res != MP_NOPTS_VALUE)
                    break;
            }
        }
    }

    res = MP_ADD_PTS(res, in->ts_offset);

    pthread_mutex_unlock(&in->lock);

    return res;
}

// Used by demuxers to report the amount of transferred bytes. This is for
// streams which circumvent demuxer->stream (stream statistics are handled by
// demux.c itself).
void demux_report_unbuffered_read_bytes(struct demuxer *demuxer, int64_t new)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_thread);

    in->slave_unbuffered_read_bytes += new;
}

// Return bytes read since last query. It's a hack because it works only if
// the demuxer thread is disabled.
int64_t demux_get_bytes_read_hack(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;

    // Required because demuxer==in->d_user, and we access in->d_thread.
    // Locking won't solve this, because we also need to access struct stream.
    assert(!in->threading);

    update_bytes_read(in);

    int64_t res = in->hack_unbuffered_read_bytes;
    in->hack_unbuffered_read_bytes = 0;
    return res;
}

void demux_get_bitrate_stats(struct demuxer *demuxer, double *rates)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    pthread_mutex_lock(&in->lock);

    for (int n = 0; n < STREAM_TYPE_COUNT; n++)
        rates[n] = -1;
    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;
        if (ds->selected && ds->bitrate >= 0)
            rates[ds->type] = MPMAX(0, rates[ds->type]) + ds->bitrate;
    }

    pthread_mutex_unlock(&in->lock);
}

void demux_get_reader_state(struct demuxer *demuxer, struct demux_reader_state *r)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    pthread_mutex_lock(&in->lock);

    *r = (struct demux_reader_state){
        .eof = in->eof,
        .ts_reader = MP_NOPTS_VALUE,
        .ts_end = MP_NOPTS_VALUE,
        .ts_duration = -1,
        .total_bytes = in->total_bytes,
        .seeking = in->seeking_in_progress,
        .low_level_seeks = in->low_level_seeks,
        .ts_last = in->demux_ts,
        .bytes_per_second = in->bytes_per_second,
        .byte_level_seeks = in->byte_level_seeks,
        .file_cache_bytes = in->cache ? demux_cache_get_size(in->cache) : -1,
    };
    bool any_packets = false;
    for (int n = 0; n < in->num_streams; n++) {
        struct demux_stream *ds = in->streams[n]->ds;
        if (ds->eager && !(!ds->queue->head && ds->eof) && !ds->ignore_eof) {
            r->underrun |= !ds->reader_head && !ds->eof && !ds->still_image;
            r->ts_reader = MP_PTS_MAX(r->ts_reader, ds->base_ts);
            r->ts_end = MP_PTS_MAX(r->ts_end, ds->queue->last_ts);
            any_packets |= !!ds->reader_head;
        }
        r->fw_bytes += get_foward_buffered_bytes(ds);
    }
    r->idle = (!in->reading && !r->underrun) || r->eof;
    r->underrun &= !r->idle && in->threading;
    r->ts_reader = MP_ADD_PTS(r->ts_reader, in->ts_offset);
    r->ts_end = MP_ADD_PTS(r->ts_end, in->ts_offset);
    if (r->ts_reader != MP_NOPTS_VALUE && r->ts_reader <= r->ts_end)
        r->ts_duration = r->ts_end - r->ts_reader;
    if (in->seeking || !any_packets)
        r->ts_duration = 0;
    for (int n = 0; n < MPMIN(in->num_ranges, MAX_SEEK_RANGES); n++) {
        struct demux_cached_range *range = in->ranges[n];
        if (range->seek_start != MP_NOPTS_VALUE) {
            r->seek_ranges[r->num_seek_ranges++] =
                (struct demux_seek_range){
                    .start = MP_ADD_PTS(range->seek_start, in->ts_offset),
                    .end = MP_ADD_PTS(range->seek_end, in->ts_offset),
                };
            r->bof_cached |= range->is_bof;
            r->eof_cached |= range->is_eof;
        }
    }

    pthread_mutex_unlock(&in->lock);
}

bool demux_cancel_test(struct demuxer *demuxer)
{
    return mp_cancel_test(demuxer->cancel);
}

struct demux_chapter *demux_copy_chapter_data(struct demux_chapter *c, int num)
{
    struct demux_chapter *new = talloc_array(NULL, struct demux_chapter, num);
    for (int n = 0; n < num; n++) {
        new[n] = c[n];
        new[n].metadata = mp_tags_dup(new, new[n].metadata);
    }
    return new;
}

static void visit_tags(void *ctx, void (*visit)(void *ctx, void *ta, char **s),
                       struct mp_tags *tags)
{
    for (int n = 0; n < (tags ? tags->num_keys : 0); n++)
        visit(ctx, tags, &tags->values[n]);
}

static void visit_meta(struct demuxer *demuxer, void *ctx,
                       void (*visit)(void *ctx, void *ta, char **s))
{
    struct demux_internal *in = demuxer->in;

    for (int n = 0; n < in->num_streams; n++) {
        struct sh_stream *sh = in->streams[n];

        visit(ctx, sh, &sh->title);
        visit_tags(ctx, visit, sh->tags);
    }

    for (int n = 0; n < demuxer->num_chapters; n++)
        visit_tags(ctx, visit, demuxer->chapters[n].metadata);

    visit_tags(ctx, visit, demuxer->metadata);
}


static void visit_detect(void *ctx, void *ta, char **s)
{
    char **all = ctx;

    if (*s)
        *all = talloc_asprintf_append_buffer(*all, "%s\n", *s);
}

static void visit_convert(void *ctx, void *ta, char **s)
{
    struct demuxer *demuxer = ctx;
    struct demux_internal *in = demuxer->in;

    if (!*s)
        return;

    bstr data = bstr0(*s);
    bstr conv = mp_iconv_to_utf8(in->log, data, in->meta_charset,
                                 MP_ICONV_VERBOSE);
    if (conv.start && conv.start != data.start) {
        char *ns = conv.start; // 0-termination is guaranteed
        // (The old string might not be an alloc, but if it is, it's a talloc
        // child, and will not leak, even if it stays allocated uselessly.)
        *s = ns;
        talloc_steal(ta, *s);
    }
}

static void demux_convert_tags_charset(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;

    char *cp = in->opts->meta_cp;
    if (!cp || mp_charset_is_utf8(cp))
        return;

    char *data = talloc_strdup(NULL, "");
    visit_meta(demuxer, &data, visit_detect);

    in->meta_charset = (char *)mp_charset_guess(in, in->log, bstr0(data), cp, 0);
    if (in->meta_charset && !mp_charset_is_utf8(in->meta_charset)) {
        MP_INFO(demuxer, "Using tag charset: %s\n", in->meta_charset);
        visit_meta(demuxer, demuxer, visit_convert);
    }

    talloc_free(data);
}

static bool get_demux_sub_opts(int index, const struct m_sub_options **sub)
{
    if (!demuxer_list[index])
        return false;
    *sub = demuxer_list[index]->options;
    return true;
}
