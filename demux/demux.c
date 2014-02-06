/*
 * DEMUXER v2.5
 *
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
#define DEMUX_PRIV(x) x

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "options/options.h"
#include "common/av_common.h"
#include "talloc.h"
#include "common/msg.h"
#include "common/global.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "mf.h"

#include "audio/format.h"

#include <libavcodec/avcodec.h>

#if MP_INPUT_BUFFER_PADDING_SIZE < FF_INPUT_BUFFER_PADDING_SIZE
#error MP_INPUT_BUFFER_PADDING_SIZE is too small!
#endif

// Demuxer list
extern const struct demuxer_desc demuxer_desc_edl;
extern const struct demuxer_desc demuxer_desc_cue;
extern const demuxer_desc_t demuxer_desc_rawaudio;
extern const demuxer_desc_t demuxer_desc_rawvideo;
extern const demuxer_desc_t demuxer_desc_tv;
extern const demuxer_desc_t demuxer_desc_mf;
extern const demuxer_desc_t demuxer_desc_matroska;
extern const demuxer_desc_t demuxer_desc_lavf;
extern const demuxer_desc_t demuxer_desc_libass;
extern const demuxer_desc_t demuxer_desc_subreader;
extern const demuxer_desc_t demuxer_desc_playlist;

/* Please do not add any new demuxers here. If you want to implement a new
 * demuxer, add it to libavformat, except for wrappers around external
 * libraries and demuxers requiring binary support. */

const demuxer_desc_t *const demuxer_list[] = {
    &demuxer_desc_edl,
    &demuxer_desc_cue,
    &demuxer_desc_rawaudio,
    &demuxer_desc_rawvideo,
#if HAVE_TV
    &demuxer_desc_tv,
#endif
#if HAVE_LIBASS
    &demuxer_desc_libass,
#endif
    &demuxer_desc_matroska,
    &demuxer_desc_lavf,
    &demuxer_desc_mf,
    &demuxer_desc_playlist,
    // Pretty aggressive, so should be last.
    &demuxer_desc_subreader,
    /* Please do not add any new demuxers here. If you want to implement a new
     * demuxer, add it to libavformat, except for wrappers around external
     * libraries and demuxers requiring binary support. */
    NULL
};

struct demux_stream {
    int selected;          // user wants packets from this stream
    int eof;               // end of demuxed stream? (true if all buffer empty)
    int packs;            // number of packets in buffer
    int bytes;            // total bytes of packets in buffer
    struct demux_packet *head;
    struct demux_packet *tail;
};

static void add_stream_chapters(struct demuxer *demuxer);

static void ds_free_packs(struct demux_stream *ds)
{
    demux_packet_t *dp = ds->head;
    while (dp) {
        demux_packet_t *dn = dp->next;
        free_demux_packet(dp);
        dp = dn;
    }
    ds->head = ds->tail = NULL;
    ds->packs = 0; // !!!!!
    ds->bytes = 0;
    ds->eof = 0;
}

static void packet_destroy(void *ptr)
{
    struct demux_packet *dp = ptr;
    talloc_free(dp->avpacket);
    free(dp->allocation);
}

static struct demux_packet *create_packet(size_t len)
{
    if (len > 1000000000) {
        fprintf(stderr, "Attempt to allocate demux packet over 1 GB!\n");
        abort();
    }
    struct demux_packet *dp = talloc(NULL, struct demux_packet);
    talloc_set_destructor(dp, packet_destroy);
    *dp = (struct demux_packet) {
        .len = len,
        .pts = MP_NOPTS_VALUE,
        .dts = MP_NOPTS_VALUE,
        .duration = -1,
        .stream_pts = MP_NOPTS_VALUE,
        .pos = -1,
        .stream = -1,
    };
    return dp;
}

struct demux_packet *new_demux_packet(size_t len)
{
    struct demux_packet *dp = create_packet(len);
    dp->buffer = malloc(len + MP_INPUT_BUFFER_PADDING_SIZE);
    if (!dp->buffer) {
        fprintf(stderr, "Memory allocation failure!\n");
        abort();
    }
    memset(dp->buffer + len, 0, MP_INPUT_BUFFER_PADDING_SIZE);
    dp->allocation = dp->buffer;
    return dp;
}

// data must already have suitable padding, and does not copy the data
struct demux_packet *new_demux_packet_fromdata(void *data, size_t len)
{
    struct demux_packet *dp = create_packet(len);
    dp->buffer = data;
    return dp;
}

struct demux_packet *new_demux_packet_from(void *data, size_t len)
{
    struct demux_packet *dp = new_demux_packet(len);
    memcpy(dp->buffer, data, len);
    return dp;
}

void resize_demux_packet(struct demux_packet *dp, size_t len)
{
    if (len > 1000000000) {
        fprintf(stderr, "Attempt to realloc demux packet over 1 GB!\n");
        abort();
    }
    assert(dp->allocation);
    dp->buffer = realloc(dp->buffer, len + MP_INPUT_BUFFER_PADDING_SIZE);
    if (!dp->buffer) {
        fprintf(stderr, "Memory allocation failure!\n");
        abort();
    }
    memset(dp->buffer + len, 0, MP_INPUT_BUFFER_PADDING_SIZE);
    dp->len = len;
    dp->allocation = dp->buffer;
}

void free_demux_packet(struct demux_packet *dp)
{
    talloc_free(dp);
}

static void destroy_avpacket(void *pkt)
{
    av_free_packet(pkt);
}

struct demux_packet *demux_copy_packet(struct demux_packet *dp)
{
    struct demux_packet *new = NULL;
    // No av_copy_packet() in Libav
#if LIBAVCODEC_VERSION_MICRO >= 100
    if (dp->avpacket) {
        assert(dp->buffer == dp->avpacket->data);
        assert(dp->len == dp->avpacket->size);
        AVPacket *newavp = talloc_zero(NULL, AVPacket);
        talloc_set_destructor(newavp, destroy_avpacket);
        av_init_packet(newavp);
        if (av_copy_packet(newavp, dp->avpacket) < 0)
            abort();
        new = new_demux_packet_fromdata(newavp->data, newavp->size);
        new->avpacket = newavp;
    }
#endif
    if (!new) {
        new = new_demux_packet(dp->len);
        memcpy(new->buffer, dp->buffer, new->len);
    }
    new->pts = dp->pts;
    new->dts = dp->dts;
    new->duration = dp->duration;
    new->stream_pts = dp->stream_pts;
    return new;
}

struct sh_stream *new_sh_stream(demuxer_t *demuxer, enum stream_type type)
{
    if (demuxer->num_streams > MAX_SH_STREAMS) {
        MP_WARN(demuxer, "Too many streams.");
        return NULL;
    }

    int demuxer_id = 0;
    for (int n = 0; n < demuxer->num_streams; n++) {
        if (demuxer->streams[n]->type == type)
            demuxer_id++;
    }

    struct sh_stream *sh = talloc_ptrtype(demuxer, sh);
    *sh = (struct sh_stream) {
        .type = type,
        .demuxer = demuxer,
        .index = demuxer->num_streams,
        .demuxer_id = demuxer_id, // may be overwritten by demuxer
        .ds = talloc_zero(sh, struct demux_stream),
    };
    MP_TARRAY_APPEND(demuxer, demuxer->streams, demuxer->num_streams, sh);
    switch (sh->type) {
        case STREAM_VIDEO: {
            struct sh_video *sht = talloc_zero(demuxer, struct sh_video);
            sh->video = sht;
            break;
        }
        case STREAM_AUDIO: {
            struct sh_audio *sht = talloc_zero(demuxer, struct sh_audio);
            sh->audio = sht;
            break;
        }
        case STREAM_SUB: {
            struct sh_sub *sht = talloc_zero(demuxer, struct sh_sub);
            sh->sub = sht;
            break;
        }
        default: assert(false);
    }

    sh->ds->selected = demuxer->stream_autoselect;

    return sh;
}

void free_demuxer(demuxer_t *demuxer)
{
    if (!demuxer)
        return;
    if (demuxer->desc->close)
        demuxer->desc->close(demuxer);
    // free streams:
    for (int n = 0; n < demuxer->num_streams; n++)
        ds_free_packs(demuxer->streams[n]->ds);
    talloc_free(demuxer);
}

static const char *stream_type_name(enum stream_type type)
{
    switch (type) {
    case STREAM_VIDEO:  return "video";
    case STREAM_AUDIO:  return "audio";
    case STREAM_SUB:    return "sub";
    default:            return "unknown";
    }
}

static int count_packs(struct demuxer *demux, enum stream_type type)
{
    int c = 0;
    for (int n = 0; n < demux->num_streams; n++)
        c += demux->streams[n]->type == type ? demux->streams[n]->ds->packs : 0;
    return c;
}

static int count_bytes(struct demuxer *demux, enum stream_type type)
{
    int c = 0;
    for (int n = 0; n < demux->num_streams; n++)
        c += demux->streams[n]->type == type ? demux->streams[n]->ds->bytes : 0;
    return c;
}

// Returns the same value as demuxer->fill_buffer: 1 ok, 0 EOF/not selected.
int demuxer_add_packet(demuxer_t *demuxer, struct sh_stream *stream,
                       demux_packet_t *dp)
{
    struct demux_stream *ds = stream ? stream->ds : NULL;
    if (!dp || !ds || !ds->selected) {
        talloc_free(dp);
        return 0;
    }

    if (stream->type == STREAM_VIDEO && !dp->len) {
        /* Video packets with size 0 are assumed to not correspond to frames,
         * but to indicate the absence of a frame in formats like AVI
         * that must have packets at fixed timestamp intervals. */
        talloc_free(dp);
        return 1;
    }

    dp->stream = stream->index;
    dp->next = NULL;

    ds->packs++;
    ds->bytes += dp->len;
    if (ds->tail) {
        // next packet in stream
        ds->tail->next = dp;
        ds->tail = dp;
    } else {
        // first packet in stream
        ds->head = ds->tail = dp;
    }
    /* ds_get_packets() can set ds->eof to 1 when another stream runs out of
     * buffer space. That makes sense because in that situation the calling
     * code should not count on being able to demux more packets from this
     * stream. (Can happen with e.g. badly interleaved files.)
     * In this case, we didn't necessarily reach EOF, and new packet can
     * appear. */
    ds->eof = 0;

    if (dp->pos >= 0)
        demuxer->filepos = dp->pos;

    // For video, PTS determination is not trivial, but for other media types
    // distinguishing PTS and DTS is not useful.
    if (stream->type != STREAM_VIDEO && dp->pts == MP_NOPTS_VALUE)
        dp->pts = dp->dts;

    MP_DBG(demuxer, "DEMUX: Append packet to %s, len=%d  pts=%5.3f  pos=%"PRIu64" "
           "[packs: A=%d V=%d S=%d]\n", stream_type_name(stream->type),
           dp->len, dp->pts, dp->pos, count_packs(demuxer, STREAM_AUDIO),
           count_packs(demuxer, STREAM_VIDEO), count_packs(demuxer, STREAM_SUB));
    return 1;
}

static bool demux_check_queue_full(demuxer_t *demux)
{
    for (int n = 0; n < demux->num_streams; n++) {
        struct sh_stream *sh = demux->streams[n];
        if (sh->ds->packs > MAX_PACKS || sh->ds->bytes > MAX_PACK_BYTES)
            goto overflow;
    }
    return false;

overflow:

    if (!demux->warned_queue_overflow) {
        MP_ERR(demux, "\nToo many packets in the demuxer "
               "packet queue (video: %d packets in %d bytes, audio: %d "
               "packets in %d bytes, sub: %d packets in %d bytes).\n",
               count_packs(demux, STREAM_VIDEO), count_bytes(demux, STREAM_VIDEO),
               count_packs(demux, STREAM_AUDIO), count_bytes(demux, STREAM_AUDIO),
               count_packs(demux, STREAM_SUB), count_bytes(demux, STREAM_SUB));
        MP_INFO(demux, "Maybe you are playing a non-"
                "interleaved stream/file or the codec failed?\n");
    }
    demux->warned_queue_overflow = true;
    return true;
}

// return value:
//     0 = EOF or no stream found or invalid type
//     1 = successfully read a packet

static int demux_fill_buffer(demuxer_t *demux)
{
    return demux->desc->fill_buffer ? demux->desc->fill_buffer(demux) : 0;
}

static void ds_get_packets(struct sh_stream *sh)
{
    struct demux_stream *ds = sh->ds;
    demuxer_t *demux = sh->demuxer;
    MP_TRACE(demux, "ds_get_packets (%s) called\n",
             stream_type_name(sh->type));
    while (1) {
        if (ds->head)
            return;

        if (demux_check_queue_full(demux))
            break;

        if (!demux_fill_buffer(demux))
            break; // EOF
    }
    MP_VERBOSE(demux, "ds_get_packets: EOF reached (stream: %s)\n",
               stream_type_name(sh->type));
    ds->eof = 1;
}

// Read a packet from the given stream. The returned packet belongs to the
// caller, who has to free it with talloc_free(). Might block. Returns NULL
// on EOF.
struct demux_packet *demux_read_packet(struct sh_stream *sh)
{
    struct demux_stream *ds = sh ? sh->ds : NULL;
    if (ds) {
        ds_get_packets(sh);
        struct demux_packet *pkt = ds->head;
        if (pkt) {
            ds->head = pkt->next;
            pkt->next = NULL;
            if (!ds->head)
                ds->tail = NULL;
            ds->bytes -= pkt->len;
            ds->packs--;

            if (pkt->stream_pts != MP_NOPTS_VALUE)
                sh->demuxer->stream_pts = pkt->stream_pts;

            return pkt;
        }
    }
    return NULL;
}

// Return the pts of the next packet that demux_read_packet() would return.
// Might block. Sometimes used to force a packet read, without removing any
// packets from the queue.
double demux_get_next_pts(struct sh_stream *sh)
{
    if (sh && sh->ds->selected) {
        ds_get_packets(sh);
        if (sh->ds->head)
            return sh->ds->head->pts;
    }
    return MP_NOPTS_VALUE;
}

// Return whether a packet is queued. Never blocks, never forces any reads.
bool demux_has_packet(struct sh_stream *sh)
{
    return sh && sh->ds->head;
}

// Same as demux_has_packet, but to be called internally by demuxers, as
// opposed to the user of the demuxer.
bool demuxer_stream_has_packets_queued(struct demuxer *d, struct sh_stream *stream)
{
    return demux_has_packet(stream);
}

// Return whether EOF was returned with an earlier packet read.
bool demux_stream_eof(struct sh_stream *sh)
{
    return !sh || sh->ds->eof;
}

// ====================================================================

void demuxer_help(struct mp_log *log)
{
    int i;

    mp_info(log, "Available demuxers:\n");
    mp_info(log, " demuxer:   info:\n");
    for (i = 0; demuxer_list[i]; i++) {
        mp_info(log, "%10s  %s\n",
                demuxer_list[i]->name, demuxer_list[i]->desc);
    }
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

static struct demuxer *open_given_type(struct mpv_global *global,
                                       struct mp_log *log,
                                       const struct demuxer_desc *desc,
                                       struct stream *stream,
                                       struct demuxer_params *params,
                                       enum demux_check check)
{
    struct demuxer *demuxer = talloc_ptrtype(NULL, demuxer);
    *demuxer = (struct demuxer) {
        .desc = desc,
        .type = desc->type,
        .stream = stream,
        .stream_pts = MP_NOPTS_VALUE,
        .seekable = (stream->flags & MP_STREAM_SEEK) == MP_STREAM_SEEK &&
                    stream->end_pos > 0,
        .accurate_seek = true,
        .filepos = -1,
        .opts = global->opts,
        .global = global,
        .log = mp_log_new(demuxer, log, desc->name),
        .glog = log,
        .filename = talloc_strdup(demuxer, stream->url),
        .metadata = talloc_zero(demuxer, struct mp_tags),
    };
    demuxer->params = params; // temporary during open()
    stream_seek(stream, stream->start_pos);

    mp_verbose(log, "Trying demuxer: %s (force-level: %s)\n",
               desc->name, d_level(check));

    int ret = demuxer->desc->open(demuxer, check);
    if (ret >= 0) {
        demuxer->params = NULL;
        if (demuxer->filetype)
            mp_info(log, "Detected file format: %s (%s)\n",
                    demuxer->filetype, desc->desc);
        else
            mp_info(log, "Detected file format: %s\n", desc->desc);
        if (stream_manages_timeline(demuxer->stream)) {
            // Incorrect, but fixes some behavior with DVD/BD
            demuxer->ts_resets_possible = false;
            // Doesn't work, because stream_pts is a "guess".
            demuxer->accurate_seek = false;
            // Can be seekable even if the stream isn't.
            demuxer->seekable = true;
        }
        add_stream_chapters(demuxer);
        demuxer_sort_chapters(demuxer);
        demux_info_update(demuxer);
        // Pretend we can seek if we can't seek, but there's a cache.
        if (!demuxer->seekable && stream->uncached_stream) {
            mp_warn(log,
                    "File is not seekable, but there's a cache: enabling seeking.\n");
            demuxer->seekable = true;
        }
        return demuxer;
    }

    free_demuxer(demuxer);
    return NULL;
}

static const int d_normal[]  = {DEMUX_CHECK_NORMAL, DEMUX_CHECK_UNSAFE, -1};
static const int d_request[] = {DEMUX_CHECK_REQUEST, -1};
static const int d_force[]   = {DEMUX_CHECK_FORCE, -1};

struct demuxer *demux_open(struct stream *stream, char *force_format,
                           struct demuxer_params *params,
                           struct mpv_global *global)
{
    const int *check_levels = d_normal;
    const struct demuxer_desc *check_desc = NULL;
    struct mp_log *log = mp_log_new(NULL, global->log, "!demux");
    struct demuxer *demuxer = NULL;

    if (!force_format)
        force_format = stream->demuxer;

    if (force_format && force_format[0]) {
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

    // Peek this much data to avoid that stream_read() run by some demuxers
    // or stream filters will flush previous peeked data.
    stream_peek(stream, STREAM_BUFFER_SIZE);

    // Test demuxers from first to last, one pass for each check_levels[] entry
    for (int pass = 0; check_levels[pass] != -1; pass++) {
        enum demux_check level = check_levels[pass];
        for (int n = 0; demuxer_list[n]; n++) {
            const struct demuxer_desc *desc = demuxer_list[n];
            if (!check_desc || desc == check_desc) {
                demuxer = open_given_type(global, log, desc, stream, params, level);
                if (demuxer) {
                    talloc_steal(demuxer, log);
                    log = NULL;
                    goto done;
                }
            }
        }
    }

done:
    talloc_free(log);
    return demuxer;
}

void demux_flush(demuxer_t *demuxer)
{
    for (int n = 0; n < demuxer->num_streams; n++)
        ds_free_packs(demuxer->streams[n]->ds);
    demuxer->warned_queue_overflow = false;
}

int demux_seek(demuxer_t *demuxer, float rel_seek_secs, int flags)
{
    if (!demuxer->seekable) {
        MP_WARN(demuxer, "Cannot seek in this file.\n");
        return 0;
    }

    if (rel_seek_secs == MP_NOPTS_VALUE && (flags & SEEK_ABSOLUTE))
        return 0;

    // clear demux buffers:
    demux_flush(demuxer);

    /* Note: this is for DVD and BD playback. The stream layer has to do these
     * seeks, and the demuxer has to react to DEMUXER_CTRL_RESYNC in order to
     * deal with the suddenly changing stream position.
     */
    struct stream *stream = demuxer->stream;
    if (stream_manages_timeline(stream)) {
        double pts;

        if (flags & SEEK_ABSOLUTE)
            pts = 0.0f;
        else {
            if (demuxer->stream_pts == MP_NOPTS_VALUE)
                goto dmx_seek;
            pts = demuxer->stream_pts;
        }

        if (flags & SEEK_FACTOR) {
            double tmp = 0;
            if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH,
                               &tmp) == STREAM_UNSUPPORTED)
                goto dmx_seek;
            pts += tmp * rel_seek_secs;
        } else
            pts += rel_seek_secs;

        if (stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_TIME, &pts)
            != STREAM_UNSUPPORTED) {
            demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);
            return 1;
        }
    }

  dmx_seek:
    if (demuxer->desc->seek)
        demuxer->desc->seek(demuxer, rel_seek_secs, flags);

    return 1;
}

void mp_tags_set_str(struct mp_tags *tags, const char *key, const char *value)
{
    mp_tags_set_bstr(tags, bstr0(key), bstr0(value));
}

void mp_tags_set_bstr(struct mp_tags *tags, bstr key, bstr value)
{
    for (int n = 0; n < tags->num_keys; n++) {
        if (bstrcasecmp0(key, tags->keys[n]) == 0) {
            talloc_free(tags->values[n]);
            tags->values[n] = talloc_strndup(tags, value.start, value.len);
            return;
        }
    }

    MP_RESIZE_ARRAY(tags, tags->keys,   tags->num_keys + 1);
    MP_RESIZE_ARRAY(tags, tags->values, tags->num_keys + 1);
    tags->keys[tags->num_keys]   = talloc_strndup(tags, key.start,   key.len);
    tags->values[tags->num_keys] = talloc_strndup(tags, value.start, value.len);
    tags->num_keys++;
}

char *mp_tags_get_str(struct mp_tags *tags, const char *key)
{
    return mp_tags_get_bstr(tags, bstr0(key));
}

char *mp_tags_get_bstr(struct mp_tags *tags, bstr key)
{
    for (int n = 0; n < tags->num_keys; n++) {
        if (bstrcasecmp0(key, tags->keys[n]) == 0)
            return tags->values[n];
    }
    return NULL;
}

int demux_info_add(demuxer_t *demuxer, const char *opt, const char *param)
{
    return demux_info_add_bstr(demuxer, bstr0(opt), bstr0(param));
}

int demux_info_add_bstr(demuxer_t *demuxer, struct bstr opt, struct bstr param)
{
    mp_tags_set_bstr(demuxer->metadata, opt, param);
    return 1;
}

static int demux_info_print(demuxer_t *demuxer)
{
    struct mp_tags *info = demuxer->metadata;
    int n;

    if (!info || !info->num_keys)
        return 0;

    mp_info(demuxer->glog, "Clip info:\n");
    for (n = 0; n < info->num_keys; n++) {
        mp_info(demuxer->glog, " %s: %s\n", info->keys[n], info->values[n]);
        mp_msg(demuxer->glog, MSGL_SMODE, "ID_CLIP_INFO_NAME%d=%s\n", n,
               info->keys[n]);
        mp_msg(demuxer->glog, MSGL_SMODE, "ID_CLIP_INFO_VALUE%d=%s\n", n,
               info->values[n]);
    }
    mp_msg(demuxer->glog, MSGL_SMODE, "ID_CLIP_INFO_N=%d\n", n);

    return 0;
}

char *demux_info_get(demuxer_t *demuxer, const char *opt)
{
    return mp_tags_get_str(demuxer->metadata, opt);
}

void demux_info_update(struct demuxer *demuxer)
{
    struct mp_tags *tags = demuxer->metadata;
    // Take care of stream metadata as well
    char **meta;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_METADATA, &meta) > 0) {
        for (int n = 0; meta[n + 0]; n += 2)
            mp_tags_set_str(tags, meta[n + 0], meta[n + 1]);
        talloc_free(meta);
    }
    // Check for metadata changes the hard way.
    char *data = talloc_strdup(demuxer, "");
    for (int n = 0; n < tags->num_keys; n++) {
        data = talloc_asprintf_append_buffer(data, "%s=%s\n", tags->keys[n],
                                             tags->values[n]);
    }
    if (!demuxer->previous_metadata ||
        strcmp(demuxer->previous_metadata, data) != 0)
    {
        talloc_free(demuxer->previous_metadata);
        demuxer->previous_metadata = data;
        demux_info_print(demuxer);
    } else {
        talloc_free(data);
    }
}

int demux_control(demuxer_t *demuxer, int cmd, void *arg)
{

    if (demuxer->desc->control)
        return demuxer->desc->control(demuxer, cmd, arg);

    return DEMUXER_CTRL_NOTIMPL;
}

struct sh_stream *demuxer_stream_by_demuxer_id(struct demuxer *d,
                                               enum stream_type t, int id)
{
    for (int n = 0; n < d->num_streams; n++) {
        struct sh_stream *s = d->streams[n];
        if (s->type == t && s->demuxer_id == id)
            return d->streams[n];
    }
    return NULL;
}

void demuxer_switch_track(struct demuxer *demuxer, enum stream_type type,
                          struct sh_stream *stream)
{
    assert(!stream || stream->type == type);

    for (int n = 0; n < demuxer->num_streams; n++) {
        struct sh_stream *cur = demuxer->streams[n];
        if (cur->type == type)
            demuxer_select_track(demuxer, cur, cur == stream);
    }
}

void demuxer_select_track(struct demuxer *demuxer, struct sh_stream *stream,
                          bool selected)
{
    // don't flush buffers if stream is already selected / unselected
    if (stream->ds->selected != selected) {
        stream->ds->selected = selected;
        ds_free_packs(stream->ds);
        demux_control(demuxer, DEMUXER_CTRL_SWITCHED_TRACKS, NULL);
    }
}

void demuxer_enable_autoselect(struct demuxer *demuxer)
{
    demuxer->stream_autoselect = true;
}

bool demuxer_stream_is_selected(struct demuxer *d, struct sh_stream *stream)
{
    return stream && stream->ds->selected;
}

int demuxer_add_attachment(demuxer_t *demuxer, struct bstr name,
                           struct bstr type, struct bstr data)
{
    if (!(demuxer->num_attachments % 32))
        demuxer->attachments = talloc_realloc(demuxer, demuxer->attachments,
                                              struct demux_attachment,
                                              demuxer->num_attachments + 32);

    struct demux_attachment *att =
        demuxer->attachments + demuxer->num_attachments;
    att->name = talloc_strndup(demuxer->attachments, name.start, name.len);
    att->type = talloc_strndup(demuxer->attachments, type.start, type.len);
    att->data = talloc_size(demuxer->attachments, data.len);
    memcpy(att->data, data.start, data.len);
    att->data_size = data.len;

    return demuxer->num_attachments++;
}

static int chapter_compare(const void *p1, const void *p2)
{
    struct demux_chapter *c1 = (void *)p1;
    struct demux_chapter *c2 = (void *)p2;

    if (c1->start > c2->start)
        return 1;
    else if (c1->start < c2->start)
        return -1;
    return c1->original_index > c2->original_index ? 1 :-1; // never equal
}

void demuxer_sort_chapters(demuxer_t *demuxer)
{
    qsort(demuxer->chapters, demuxer->num_chapters,
          sizeof(struct demux_chapter), chapter_compare);
}

int demuxer_add_chapter(demuxer_t *demuxer, struct bstr name,
                        uint64_t start, uint64_t end, uint64_t demuxer_id)
{
    struct demux_chapter new = {
        .original_index = demuxer->num_chapters,
        .start = start,
        .end = end,
        .name = name.len ? bstrdup0(demuxer, name) : NULL,
        .metadata = talloc_zero(demuxer, struct mp_tags),
        .demuxer_id = demuxer_id,
    };
    mp_tags_set_bstr(new.metadata, bstr0("TITLE"), name);
    MP_TARRAY_APPEND(demuxer, demuxer->chapters, demuxer->num_chapters, new);
    return 0;
}

void demuxer_add_chapter_info(struct demuxer *demuxer, uint64_t demuxer_id,
                              bstr key, bstr value)
{
    for (int n = 0; n < demuxer->num_chapters; n++) {
        struct demux_chapter *ch = &demuxer->chapters[n];
        if (ch->demuxer_id == demuxer_id) {
            mp_tags_set_bstr(ch->metadata, key, value);
            return;
        }
    }
}

static void add_stream_chapters(struct demuxer *demuxer)
{
    if (demuxer->num_chapters)
        return;
    int num_chapters = demuxer_chapter_count(demuxer);
    for (int n = 0; n < num_chapters; n++) {
        double p = n;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_CHAPTER_TIME, &p)
                != STREAM_OK)
            return;
        demuxer_add_chapter(demuxer, bstr0(""), p * 1e9, 0, 0);
    }
}

/**
 * \brief demuxer_seek_chapter() seeks to a chapter in two possible ways:
 *        either using the demuxer->chapters structure set by the demuxer
 *        or asking help to the stream layer (e.g. dvd)
 * \param chapter - chapter number wished - 0-based
 * \param seek_pts set by the function to the pts to seek to (if demuxer->chapters is set)
 * \return -1 on error, current chapter if successful
 */

int demuxer_seek_chapter(demuxer_t *demuxer, int chapter, double *seek_pts)
{
    int ris = STREAM_UNSUPPORTED;

    if (demuxer->num_chapters == 0)
        ris = stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_CHAPTER,
                             &chapter);

    if (ris != STREAM_UNSUPPORTED) {
        demux_flush(demuxer);
        demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);

        // exit status may be ok, but main() doesn't have to seek itself
        // (because e.g. dvds depend on sectors, not on pts)
        *seek_pts = -1.0;

        return chapter;
    } else {
        if (chapter >= demuxer->num_chapters)
            return -1;
        if (chapter < 0)
            chapter = 0;

        *seek_pts = demuxer->chapters[chapter].start / 1e9;

        return chapter;
    }
}

int demuxer_get_current_chapter(demuxer_t *demuxer, double time_now)
{
    int chapter = -2;
    if (!demuxer->num_chapters || !demuxer->chapters) {
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_CHAPTER,
                           &chapter) == STREAM_UNSUPPORTED)
            chapter = -2;
    } else {
        uint64_t now = time_now * 1e9 + 0.5;
        for (chapter = demuxer->num_chapters - 1; chapter >= 0; --chapter) {
            if (demuxer->chapters[chapter].start <= now)
                break;
        }
    }
    return chapter;
}

char *demuxer_chapter_name(demuxer_t *demuxer, int chapter)
{
    if (demuxer->num_chapters && demuxer->chapters) {
        if (chapter >= 0 && chapter < demuxer->num_chapters
            && demuxer->chapters[chapter].name)
            return talloc_strdup(NULL, demuxer->chapters[chapter].name);
    }
    return NULL;
}

double demuxer_chapter_time(demuxer_t *demuxer, int chapter)
{
    if (demuxer->num_chapters && demuxer->chapters && chapter >= 0
        && chapter < demuxer->num_chapters) {
        return demuxer->chapters[chapter].start / 1e9;
    }
    return -1.0;
}

int demuxer_chapter_count(demuxer_t *demuxer)
{
    if (!demuxer->num_chapters || !demuxer->chapters) {
        int num_chapters = 0;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_CHAPTERS,
                           &num_chapters) == STREAM_UNSUPPORTED)
            num_chapters = 0;
        return num_chapters;
    } else
        return demuxer->num_chapters;
}

double demuxer_get_time_length(struct demuxer *demuxer)
{
    double len;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &len) > 0)
        return len;
    // <= 0 means DEMUXER_CTRL_NOTIMPL or DEMUXER_CTRL_DONTKNOW
    if (demux_control(demuxer, DEMUXER_CTRL_GET_TIME_LENGTH, &len) > 0)
        return len;
    return -1;
}

double demuxer_get_start_time(struct demuxer *demuxer)
{
    double time;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_START_TIME, &time) > 0)
        return time;
    if (demux_control(demuxer, DEMUXER_CTRL_GET_START_TIME, &time) > 0)
        return time;
    return 0;
}

int demuxer_angles_count(demuxer_t *demuxer)
{
    int ris, angles = -1;

    ris = stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_ANGLES, &angles);
    if (ris == STREAM_UNSUPPORTED)
        return -1;
    return angles;
}

int demuxer_get_current_angle(demuxer_t *demuxer)
{
    int ris, curr_angle = -1;
    ris = stream_control(demuxer->stream, STREAM_CTRL_GET_ANGLE, &curr_angle);
    if (ris == STREAM_UNSUPPORTED)
        return -1;
    return curr_angle;
}


int demuxer_set_angle(demuxer_t *demuxer, int angle)
{
    int ris, angles = -1;

    angles = demuxer_angles_count(demuxer);
    if ((angles < 1) || (angle > angles))
        return -1;

    demux_flush(demuxer);

    ris = stream_control(demuxer->stream, STREAM_CTRL_SET_ANGLE, &angle);
    if (ris == STREAM_UNSUPPORTED)
        return -1;

    demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);

    return angle;
}

static int packet_sort_compare(const void *p1, const void *p2)
{
    struct demux_packet *c1 = *(struct demux_packet **)p1;
    struct demux_packet *c2 = *(struct demux_packet **)p2;

    if (c1->pts > c2->pts)
        return 1;
    else if (c1->pts < c2->pts)
        return -1;
    return 0;
}

void demux_packet_list_sort(struct demux_packet **pkts, int num_pkts)
{
    qsort(pkts, num_pkts, sizeof(struct demux_packet *), packet_sort_compare);
}

void demux_packet_list_seek(struct demux_packet **pkts, int num_pkts,
                            int *current, float rel_seek_secs, int flags)
{
    double ref_time = 0;
    if (*current >= 0 && *current < num_pkts) {
        ref_time = pkts[*current]->pts;
    } else if (*current == num_pkts && num_pkts > 0) {
        ref_time = pkts[num_pkts - 1]->pts + pkts[num_pkts - 1]->duration;
    }

    if (flags & SEEK_ABSOLUTE)
        ref_time = 0;

    if (flags & SEEK_FACTOR) {
        ref_time += demux_packet_list_duration(pkts, num_pkts) * rel_seek_secs;
    } else {
        ref_time += rel_seek_secs;
    }

    // Could do binary search, but it's probably not worth the complexity.
    int last_index = 0;
    for (int n = 0; n < num_pkts; n++) {
        if (pkts[n]->pts > ref_time)
            break;
        last_index = n;
    }
    *current = last_index;
}

double demux_packet_list_duration(struct demux_packet **pkts, int num_pkts)
{
    if (num_pkts > 0)
        return pkts[num_pkts - 1]->pts + pkts[num_pkts - 1]->duration;
    return 0;
}

struct demux_packet *demux_packet_list_fill(struct demux_packet **pkts,
                                            int num_pkts, int *current)
{
    if (*current < 0)
        *current = 0;
    if (*current >= num_pkts)
        return NULL;
    struct demux_packet *new = talloc(NULL, struct demux_packet);
    *new = *pkts[*current];
    *current += 1;
    return new;
}
