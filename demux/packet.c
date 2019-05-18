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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/intreadwrite.h>

#include "config.h"

#include "common/av_common.h"
#include "common/common.h"
#include "demux.h"

#include "packet.h"

static void packet_destroy(void *ptr)
{
    struct demux_packet *dp = ptr;
    av_packet_unref(dp->avpacket);
    mp_packet_tags_unref(dp->metadata);
}

// This actually preserves only data and side data, not PTS/DTS/pos/etc.
// It also allows avpkt->data==NULL with avpkt->size!=0 - the libavcodec API
// does not allow it, but we do it to simplify new_demux_packet().
struct demux_packet *new_demux_packet_from_avpacket(struct AVPacket *avpkt)
{
    if (avpkt->size > 1000000000)
        return NULL;
    struct demux_packet *dp = talloc(NULL, struct demux_packet);
    talloc_set_destructor(dp, packet_destroy);
    *dp = (struct demux_packet) {
        .pts = MP_NOPTS_VALUE,
        .dts = MP_NOPTS_VALUE,
        .duration = -1,
        .pos = -1,
        .start = MP_NOPTS_VALUE,
        .end = MP_NOPTS_VALUE,
        .stream = -1,
        .avpacket = talloc_zero(dp, AVPacket),
        .kf_seek_pts = MP_NOPTS_VALUE,
    };
    av_init_packet(dp->avpacket);
    int r = -1;
    if (avpkt->data) {
        // We hope that this function won't need/access AVPacket input padding,
        // because otherwise new_demux_packet_from() wouldn't work.
        r = av_packet_ref(dp->avpacket, avpkt);
    } else {
        r = av_new_packet(dp->avpacket, avpkt->size);
    }
    if (r < 0) {
        *dp->avpacket = (AVPacket){0};
        talloc_free(dp);
        return NULL;
    }
    dp->buffer = dp->avpacket->data;
    dp->len = dp->avpacket->size;
    return dp;
}

// (buf must include proper padding)
struct demux_packet *new_demux_packet_from_buf(struct AVBufferRef *buf)
{
    if (!buf)
        return NULL;
    AVPacket pkt = {
        .size = buf->size,
        .data = buf->data,
        .buf = buf,
    };
    return new_demux_packet_from_avpacket(&pkt);
}

// Input data doesn't need to be padded.
struct demux_packet *new_demux_packet_from(void *data, size_t len)
{
    if (len > INT_MAX)
        return NULL;
    AVPacket pkt = { .data = data, .size = len };
    return new_demux_packet_from_avpacket(&pkt);
}

struct demux_packet *new_demux_packet(size_t len)
{
    if (len > INT_MAX)
        return NULL;
    AVPacket pkt = { .data = NULL, .size = len };
    return new_demux_packet_from_avpacket(&pkt);
}

void demux_packet_shorten(struct demux_packet *dp, size_t len)
{
    assert(len <= dp->len);
    av_shrink_packet(dp->avpacket, len);
    dp->len = dp->avpacket->size;
}

void free_demux_packet(struct demux_packet *dp)
{
    talloc_free(dp);
}

void demux_packet_copy_attribs(struct demux_packet *dst, struct demux_packet *src)
{
    dst->pts = src->pts;
    dst->dts = src->dts;
    dst->duration = src->duration;
    dst->pos = src->pos;
    dst->segmented = src->segmented;
    dst->start = src->start;
    dst->end = src->end;
    dst->codec = src->codec;
    dst->back_restart = src->back_restart;
    dst->back_preroll = src->back_preroll;
    dst->keyframe = src->keyframe;
    dst->stream = src->stream;
    mp_packet_tags_setref(&dst->metadata, src->metadata);
}

struct demux_packet *demux_copy_packet(struct demux_packet *dp)
{
    struct demux_packet *new = NULL;
    if (dp->avpacket) {
        new = new_demux_packet_from_avpacket(dp->avpacket);
    } else {
        // Some packets might be not created by new_demux_packet*().
        new = new_demux_packet_from(dp->buffer, dp->len);
    }
    if (!new)
        return NULL;
    demux_packet_copy_attribs(new, dp);
    return new;
}

#define ROUND_ALLOC(s) MP_ALIGN_UP(s, 64)

// Attempt to estimate the total memory consumption of the given packet.
// This is important if we store thousands of packets and not to exceed
// user-provided limits. Of course we can't know how much memory internal
// fragmentation of the libc memory allocator will waste.
// Note that this should return a "stable" value - e.g. if a new packet ref
// is created, this should return the same value with the new ref. (This
// implies the value is not exact and does not return the actual size of
// memory wasted due to internal fragmentation.)
size_t demux_packet_estimate_total_size(struct demux_packet *dp)
{
    size_t size = ROUND_ALLOC(sizeof(struct demux_packet));
    size += ROUND_ALLOC(dp->len);
    if (dp->avpacket) {
        size += ROUND_ALLOC(sizeof(AVPacket));
        size += ROUND_ALLOC(sizeof(AVBufferRef));
        size += 64; // upper bound estimate on sizeof(AVBuffer)
        size += ROUND_ALLOC(dp->avpacket->side_data_elems *
                            sizeof(dp->avpacket->side_data[0]));
        for (int n = 0; n < dp->avpacket->side_data_elems; n++)
            size += ROUND_ALLOC(dp->avpacket->side_data[n].size);
    }
    return size;
}

int demux_packet_set_padding(struct demux_packet *dp, int start, int end)
{
#if LIBAVCODEC_VERSION_MICRO >= 100
    if (!start && !end)
        return 0;
    if (!dp->avpacket)
        return -1;
    uint8_t *p = av_packet_new_side_data(dp->avpacket, AV_PKT_DATA_SKIP_SAMPLES, 10);
    if (!p)
        return -1;

    AV_WL32(p + 0, start);
    AV_WL32(p + 4, end);
#endif
    return 0;
}

int demux_packet_add_blockadditional(struct demux_packet *dp, uint64_t id,
                                     void *data, size_t size)
{
#if LIBAVCODEC_VERSION_MICRO >= 100
    if (!dp->avpacket)
        return -1;
    uint8_t *sd =  av_packet_new_side_data(dp->avpacket,
                                           AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL,
                                           8 + size);
    if (!sd)
        return -1;
    AV_WB64(sd, id);
    if (size > 0)
        memcpy(sd + 8, data, size);
#endif
    return 0;
}
