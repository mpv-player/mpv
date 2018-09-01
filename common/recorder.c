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

#include <math.h>

#include <libavformat/avformat.h>

#include "common/av_common.h"
#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "demux/packet.h"
#include "demux/stheader.h"

#include "recorder.h"

#define PTS_ADD(a, b) ((a) == MP_NOPTS_VALUE ? (a) : ((a) + (b)))

// Maximum number of packets we buffer at most to attempt to resync streams.
// Essentially, this should be higher than the highest supported keyframe
// interval.
#define QUEUE_MAX_PACKETS 256
// Number of packets we should buffer at least to determine timestamps (due to
// codec delay and frame reordering, and potentially lack of DTS).
// Keyframe flags can trigger this earlier.
#define QUEUE_MIN_PACKETS 16

struct mp_recorder {
    struct mpv_global *global;
    struct mp_log *log;

    struct mp_recorder_sink **streams;
    int num_streams;

    bool opened;            // mux context is valid
    bool muxing;            // we're currently recording (instead of preparing)
    bool muxing_from_start; // no discontinuity at start
    bool dts_warning;

    // The start timestamp of the currently recorded segment (the timestamp of
    // the first packet of the incoming packet stream).
    double base_ts;
    // The output packet timestamp corresponding to base_ts. It's the timestamp
    // of the first packet of the current segment written to the output.
    double rebase_ts;

    AVFormatContext *mux;
};

struct mp_recorder_sink {
    struct mp_recorder *owner;
    struct sh_stream *sh;
    AVStream *av_stream;
    double max_out_pts;
    bool discont;
    bool proper_eof;
    struct demux_packet **packets;
    int num_packets;
};

static int add_stream(struct mp_recorder *priv, struct sh_stream *sh)
{
    enum AVMediaType av_type = mp_to_av_stream_type(sh->type);
    if (av_type == AVMEDIA_TYPE_UNKNOWN)
        return -1;

    struct mp_recorder_sink *rst = talloc(priv, struct mp_recorder_sink);
    *rst = (struct mp_recorder_sink) {
        .owner = priv,
        .sh = sh,
        .av_stream = avformat_new_stream(priv->mux, NULL),
        .max_out_pts = MP_NOPTS_VALUE,
    };

    if (!rst->av_stream)
        return -1;

    AVCodecParameters *avp = mp_codec_params_to_av(sh->codec);
    if (!avp)
        return -1;

#if LIBAVCODEC_VERSION_MICRO >= 100
    // We don't know the delay, so make something up. If the format requires
    // DTS, the result will probably be broken. FFmpeg provides nothing better
    // yet (unless you demux with libavformat, which contains tons of hacks
    // that try to determine a PTS).
    if (!sh->codec->lav_codecpar)
        avp->video_delay = 16;
#endif

    if (avp->codec_id == AV_CODEC_ID_NONE)
        return -1;

    if (avcodec_parameters_copy(rst->av_stream->codecpar, avp) < 0)
        return -1;

    rst->av_stream->time_base = mp_get_codec_timebase(sh->codec);

    MP_TARRAY_APPEND(priv, priv->streams, priv->num_streams, rst);
    return 0;
}

struct mp_recorder *mp_recorder_create(struct mpv_global *global,
                                       const char *target_file,
                                       struct sh_stream **streams,
                                       int num_streams)
{
    struct mp_recorder *priv = talloc_zero(NULL, struct mp_recorder);

    priv->global = global;
    priv->log = mp_log_new(priv, global->log, "recorder");

    if (!num_streams) {
        MP_ERR(priv, "No streams.\n");
        goto error;
    }

    priv->mux = avformat_alloc_context();
    if (!priv->mux)
        goto error;

    priv->mux->oformat = av_guess_format(NULL, target_file, NULL);
    if (!priv->mux->oformat) {
        MP_ERR(priv, "Output format not found.\n");
        goto error;
    }

    if (avio_open2(&priv->mux->pb, target_file, AVIO_FLAG_WRITE, NULL, NULL) < 0) {
        MP_ERR(priv, "Failed opening output file.\n");
        goto error;
    }

    for (int n = 0; n < num_streams; n++) {
        if (add_stream(priv, streams[n]) < 0) {
            MP_ERR(priv, "Can't mux one of the input streams.\n");
            goto error;
        }
    }

    // Not sure how to write this in a "standard" way. It appears only mkv
    // and mp4 support this directly.
    char version[200];
    snprintf(version, sizeof(version), "%s experimental stream recording "
             "feature (can generate broken files - please report bugs)",
             mpv_version);
    av_dict_set(&priv->mux->metadata, "encoding_tool", version, 0);

    if (avformat_write_header(priv->mux, NULL) < 0) {
        MP_ERR(priv, "Writing header failed.\n");
        goto error;
    }

    priv->opened = true;
    priv->muxing_from_start = true;

    priv->base_ts = MP_NOPTS_VALUE;
    priv->rebase_ts = 0;

    MP_WARN(priv, "This is an experimental feature. Output files might be "
                  "broken or not play correctly with various players "
                  "(including mpv itself).\n");

    return priv;

error:
    mp_recorder_destroy(priv);
    return NULL;
}

static void flush_packets(struct mp_recorder *priv)
{
    for (int n = 0; n < priv->num_streams; n++) {
        struct mp_recorder_sink *rst = priv->streams[n];
        for (int i = 0; i < rst->num_packets; i++)
            talloc_free(rst->packets[i]);
        rst->num_packets = 0;
    }
}

static void mux_packet(struct mp_recorder_sink *rst,
                       struct demux_packet *pkt)
{
    struct mp_recorder *priv = rst->owner;
    struct demux_packet mpkt = *pkt;

    double diff = priv->rebase_ts - priv->base_ts;
    mpkt.pts = PTS_ADD(mpkt.pts, diff);
    mpkt.dts = PTS_ADD(mpkt.dts, diff);

    rst->max_out_pts = MPMAX(rst->max_out_pts, pkt->pts);

    AVPacket avpkt;
    mp_set_av_packet(&avpkt, &mpkt, &rst->av_stream->time_base);

    avpkt.stream_index = rst->av_stream->index;

    if (avpkt.duration < 0 && rst->sh->type != STREAM_SUB)
        avpkt.duration = 0;

    AVPacket *new_packet = av_packet_clone(&avpkt);
    if (!new_packet) {
        MP_ERR(priv, "Failed to allocate packet.\n");
        return;
    }

    if (av_interleaved_write_frame(priv->mux, new_packet) < 0)
        MP_ERR(priv, "Failed writing packet.\n");
}

// Write all packets that currently can be written.
static void mux_packets(struct mp_recorder_sink *rst, bool force)
{
    struct mp_recorder *priv = rst->owner;
    if (!priv->muxing || !rst->num_packets)
        return;

    int safe_count = 0;
    for (int n = 0; n < rst->num_packets; n++) {
        if (rst->packets[n]->keyframe)
            safe_count = n;
    }
    if (force)
        safe_count = rst->num_packets;

    for (int n = 0; n < safe_count; n++) {
        mux_packet(rst, rst->packets[n]);
        talloc_free(rst->packets[n]);
    }

    // Remove packets[0..safe_count]
    memmove(&rst->packets[0], &rst->packets[safe_count],
            (rst->num_packets - safe_count) * sizeof(rst->packets[0]));
    rst->num_packets -= safe_count;
}

// If there was a discontinuity, check whether we can resume muxing (and from
// where).
static void check_restart(struct mp_recorder *priv)
{
    if (priv->muxing)
        return;

    double min_ts = INFINITY;
    double rebase_ts = 0;
    for (int n = 0; n < priv->num_streams; n++) {
        struct mp_recorder_sink *rst = priv->streams[n];
        int min_packets = rst->sh->type == STREAM_VIDEO ? QUEUE_MIN_PACKETS : 1;

        rebase_ts = MPMAX(rebase_ts, rst->max_out_pts);

        if (rst->num_packets < min_packets) {
            if (!rst->proper_eof && rst->sh->type != STREAM_SUB)
                return;
            continue;
        }

        for (int i = 0; i < min_packets; i++)
            min_ts = MPMIN(min_ts, rst->packets[i]->pts);
    }

    // Subtitle only stream (wait longer) or stream without any PTS (fuck it).
    if (!isfinite(min_ts))
        return;

    priv->rebase_ts = rebase_ts;
    priv->base_ts = min_ts;

    for (int n = 0; n < priv->num_streams; n++) {
        struct mp_recorder_sink *rst = priv->streams[n];
        rst->max_out_pts = min_ts;
    }

    priv->muxing = true;

    if (!priv->muxing_from_start)
        MP_WARN(priv, "Discontinuity at timestamp %f.\n", priv->rebase_ts);
}

void mp_recorder_destroy(struct mp_recorder *priv)
{
    if (priv->opened) {
        for (int n = 0; n < priv->num_streams; n++) {
            struct mp_recorder_sink *rst = priv->streams[n];
            if (!rst->proper_eof)
                continue;
            mux_packets(rst, true);
        }

        if (av_write_trailer(priv->mux) < 0)
            MP_ERR(priv, "Writing trailer failed.\n");
    }

    if (priv->mux) {
        if (avio_closep(&priv->mux->pb) < 0)
            MP_ERR(priv, "Closing file failed\n");

        avformat_free_context(priv->mux);
    }

    flush_packets(priv);
    talloc_free(priv);
}

// This is called on a seek, or when recording was started mid-stream.
void mp_recorder_mark_discontinuity(struct mp_recorder *priv)
{
    flush_packets(priv);

    for (int n = 0; n < priv->num_streams; n++) {
        struct mp_recorder_sink *rst = priv->streams[n];
        rst->discont = true;
        rst->proper_eof = false;
    }

    priv->muxing = false;
    priv->muxing_from_start = false;
}

// Get a stream for writing. The pointer is valid until mp_recorder is
// destroyed. The stream is the index referencing the stream passed to
// mp_recorder_create().
struct mp_recorder_sink *mp_recorder_get_sink(struct mp_recorder *r, int stream)
{
    return stream >= 0 && stream < r->num_streams ? r->streams[stream] : NULL;
}

// Pass a packet to the given stream. The function does not own the packet, but
// can create a new reference to it if it needs to retain it. Can be NULL to
// signal proper end of stream.
void mp_recorder_feed_packet(struct mp_recorder_sink *rst,
                             struct demux_packet *pkt)
{
    struct mp_recorder *priv = rst->owner;

    if (!pkt) {
        rst->proper_eof = true;
        check_restart(priv);
        mux_packets(rst, false);
        return;
    }

    if (pkt->dts == MP_NOPTS_VALUE && !priv->dts_warning) {
        // No, FFmpeg has no actually usable helpers to generate correct DTS.
        // No, FFmpeg doesn't tell us which formats need DTS at all.
        // No, we can not shut up the FFmpeg warning, which will follow.
        MP_WARN(priv, "Source stream misses DTS on at least some packets!\n"
                      "If the target file format requires DTS, the written\n"
                      "file will be invalid.\n");
        priv->dts_warning = true;
    }

    if (rst->discont && !pkt->keyframe)
        return;
    rst->discont = false;

    if (rst->num_packets >= QUEUE_MAX_PACKETS) {
        MP_ERR(priv, "Stream %d has too many queued packets; dropping.\n",
               rst->av_stream->index);
        return;
    }

    pkt = demux_copy_packet(pkt);
    if (!pkt)
        return;
    MP_TARRAY_APPEND(rst, rst->packets, rst->num_packets, pkt);

    check_restart(priv);
    mux_packets(rst, false);
}
