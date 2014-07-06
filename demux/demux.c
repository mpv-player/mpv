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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "options/options.h"
#include "talloc.h"
#include "common/msg.h"
#include "common/global.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "mf.h"

#include "audio/format.h"

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
extern const demuxer_desc_t demuxer_desc_disc;

/* Please do not add any new demuxers here. If you want to implement a new
 * demuxer, add it to libavformat, except for wrappers around external
 * libraries and demuxers requiring binary support. */

const demuxer_desc_t *const demuxer_list[] = {
    &demuxer_desc_disc,
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
    NULL
};

struct demux_stream {
    struct demuxer *demuxer;
    int selected;          // user wants packets from this stream
    int eof;               // end of demuxed stream? (true if all buffer empty)
    int packs;            // number of packets in buffer
    int bytes;            // total bytes of packets in buffer
    struct demux_packet *head;
    struct demux_packet *tail;
};

void demuxer_sort_chapters(demuxer_t *demuxer);

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

struct sh_stream *new_sh_stream(demuxer_t *demuxer, enum stream_type type)
{
    if (demuxer->num_streams > MAX_SH_STREAMS) {
        MP_WARN(demuxer, "Too many streams.\n");
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
    sh->ds->demuxer = demuxer;
    sh->ds->selected = demuxer->stream_select_default;
    MP_TARRAY_APPEND(demuxer, demuxer->streams, demuxer->num_streams, sh);
    switch (sh->type) {
    case STREAM_VIDEO: sh->video = talloc_zero(demuxer, struct sh_video); break;
    case STREAM_AUDIO: sh->audio = talloc_zero(demuxer, struct sh_audio); break;
    case STREAM_SUB:   sh->sub = talloc_zero(demuxer, struct sh_sub); break;
    }

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

const char *stream_type_name(enum stream_type type)
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
int demux_add_packet(struct sh_stream *stream, demux_packet_t *dp)
{
    struct demux_stream *ds = stream ? stream->ds : NULL;
    if (!dp || !ds || !ds->selected) {
        talloc_free(dp);
        return 0;
    }
    struct demuxer *demuxer = ds->demuxer;

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
    // obviously not true anymore
    ds->eof = 0;

    // For video, PTS determination is not trivial, but for other media types
    // distinguishing PTS and DTS is not useful.
    if (stream->type != STREAM_VIDEO && dp->pts == MP_NOPTS_VALUE)
        dp->pts = dp->dts;

    if (mp_msg_test(demuxer->log, MSGL_DEBUG)) {
        MP_DBG(demuxer, "DEMUX: Append packet to %s, len=%d  pts=%5.3f  pos="
               "%"PRIi64" [A=%d V=%d S=%d]\n", stream_type_name(stream->type),
               dp->len, dp->pts, dp->pos, count_packs(demuxer, STREAM_AUDIO),
               count_packs(demuxer, STREAM_VIDEO), count_packs(demuxer, STREAM_SUB));
    }
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
        MP_ERR(demux, "Too many packets in the demuxer "
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
            if (pkt && pkt->pos >= 0)
                sh->demuxer->filepos = pkt->pos;

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

// Return whether EOF was returned with an earlier packet read.
bool demux_stream_eof(struct sh_stream *sh)
{
    return !sh || sh->ds->eof;
}

// Read and return any packet we find.
struct demux_packet *demux_read_any_packet(struct demuxer *demuxer)
{
    for (int retry = 0; retry < 2; retry++) {
        for (int n = 0; n < demuxer->num_streams; n++) {
            struct sh_stream *sh = demuxer->streams[n];
            if (sh->ds->head)
                return demux_read_packet(sh);
        }
        // retry after calling this
        demux_fill_buffer(demuxer);
    }
    return NULL;
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

static int decode_gain(demuxer_t *demuxer, const char *tag, float *out)
{
    char *tag_val = NULL;
    float dec_val;

    tag_val = mp_tags_get_str(demuxer->metadata, tag);
    if (!tag_val) {
        mp_msg(demuxer->log, MSGL_V, "Replaygain tags not found\n");
        return -1;
    }

    if (decode_float(tag_val, &dec_val)) {
        mp_msg(demuxer->log, MSGL_ERR, "Invalid replaygain value\n");
        return -1;
    }

    *out = dec_val;
    return 0;
}

static int decode_peak(demuxer_t *demuxer, const char *tag, float *out)
{
    char *tag_val = NULL;
    float dec_val;

    *out = 1.0;

    tag_val = mp_tags_get_str(demuxer->metadata, tag);
    if (!tag_val)
        return 0;

    if (decode_float(tag_val, &dec_val))
        return 0;

    if (dec_val == 0.0)
        return 0;

    *out = dec_val;
    return 0;
}

static void demux_export_replaygain(demuxer_t *demuxer)
{
    float tg, tp, ag, ap;

    if (!decode_gain(demuxer, "REPLAYGAIN_TRACK_GAIN", &tg) &&
        !decode_peak(demuxer, "REPLAYGAIN_TRACK_PEAK", &tp) &&
        !decode_gain(demuxer, "REPLAYGAIN_ALBUM_GAIN", &ag) &&
        !decode_peak(demuxer, "REPLAYGAIN_ALBUM_PEAK", &ap))
    {
        struct replaygain_data *rgain = talloc_ptrtype(demuxer, rgain);

        rgain->track_gain = tg;
        rgain->track_peak = tp;
        rgain->album_gain = ag;
        rgain->album_peak = ap;

        for (int n = 0; n < demuxer->num_streams; n++) {
            struct sh_stream *sh = demuxer->streams[n];
            if (sh->audio && !sh->audio->replaygain_data)
                sh->audio->replaygain_data = rgain;
        }
    }
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
        .seekable = stream->seekable,
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
    int64_t start_pos = stream_tell(stream);

    mp_verbose(log, "Trying demuxer: %s (force-level: %s)\n",
               desc->name, d_level(check));

    int ret = demuxer->desc->open(demuxer, check);
    if (ret >= 0) {
        demuxer->params = NULL;
        if (demuxer->filetype)
            mp_verbose(log, "Detected file format: %s (%s)\n",
                       demuxer->filetype, desc->desc);
        else
            mp_verbose(log, "Detected file format: %s\n", desc->desc);
        demuxer_sort_chapters(demuxer);
        demux_info_update(demuxer);
        demux_export_replaygain(demuxer);
        // Pretend we can seek if we can't seek, but there's a cache.
        if (!demuxer->seekable && stream->uncached_stream) {
            mp_warn(log,
                    "File is not seekable, but there's a cache: enabling seeking.\n");
            demuxer->seekable = true;
        }
        return demuxer;
    }

    free_demuxer(demuxer);
    stream_seek(stream, start_pos);
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

    // clear the packet queues
    demux_flush(demuxer);

    if (demuxer->desc->seek)
        demuxer->desc->seek(demuxer, rel_seek_secs, flags);

    return 1;
}

static int demux_info_print(demuxer_t *demuxer)
{
    struct mp_tags *info = demuxer->metadata;
    int n;

    if (!info || !info->num_keys)
        return 0;

    mp_info(demuxer->glog, "File tags:\n");
    for (n = 0; n < info->num_keys; n++) {
        mp_info(demuxer->glog, " %s: %s\n", info->keys[n], info->values[n]);
    }

    return 0;
}

char *demux_info_get(demuxer_t *demuxer, const char *opt)
{
    return mp_tags_get_str(demuxer->metadata, opt);
}

bool demux_info_update(struct demuxer *demuxer)
{
    bool r = false;
    // Take care of stream metadata as well
    struct mp_tags *s_meta = NULL;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_METADATA, &s_meta) > 0) {
        talloc_free(demuxer->stream_metadata);
        demuxer->stream_metadata = talloc_steal(demuxer, s_meta);
        demuxer->events |= DEMUX_EVENT_METADATA;
    }
    if (demuxer->events & DEMUX_EVENT_METADATA) {
        demuxer->events &= ~DEMUX_EVENT_METADATA;
        if (demuxer->stream_metadata)
            mp_tags_merge(demuxer->metadata, demuxer->stream_metadata);
        demux_info_print(demuxer);
        r = true;
    }
    return r;
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

bool demux_stream_is_selected(struct sh_stream *stream)
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
    return demuxer->num_chapters - 1;
}

double demuxer_get_time_length(struct demuxer *demuxer)
{
    double len;
    if (demux_control(demuxer, DEMUXER_CTRL_GET_TIME_LENGTH, &len) > 0)
        return len;
    return -1;
}
