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
#include <libavutil/hdr_dynamic_metadata.h>
#include <libavutil/intreadwrite.h>

#include "common/av_common.h"
#include "common/common.h"
#include "demux.h"
#include "demux/ebml.h"

#include "packet.h"

// Free any refcounted data dp holds (but don't free dp itself). This does not
// care about pointers that are _not_ refcounted (like demux_packet.codec).
// Normally, a user should use talloc_free(dp). This function is only for
// annoyingly specific obscure use cases.
void demux_packet_unref_contents(struct demux_packet *dp)
{
    if (dp->avpacket) {
        assert(!dp->is_cached);
        av_packet_free(&dp->avpacket);
        dp->buffer = NULL;
        dp->len = 0;
    }
}

static void packet_destroy(void *ptr)
{
    struct demux_packet *dp = ptr;
    demux_packet_unref_contents(dp);
}

static struct demux_packet *packet_create(void)
{
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
        .avpacket = av_packet_alloc(),
        .animated = -1,
    };
    MP_HANDLE_OOM(dp->avpacket);
    return dp;
}

// This actually preserves only data and side data, not PTS/DTS/pos/etc.
// It also allows avpkt->data==NULL with avpkt->size!=0 - the libavcodec API
// does not allow it, but we do it to simplify new_demux_packet().
struct demux_packet *new_demux_packet_from_avpacket(struct AVPacket *avpkt)
{
    if (avpkt->size > 1000000000)
        return NULL;
    struct demux_packet *dp = packet_create();
    int r = -1;
    if (avpkt->data) {
        // We hope that this function won't need/access AVPacket input padding,
        // because otherwise new_demux_packet_from() wouldn't work.
        r = av_packet_ref(dp->avpacket, avpkt);
    } else {
        r = av_new_packet(dp->avpacket, avpkt->size);
    }
    if (r < 0) {
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
    if (buf->size > 1000000000)
        return NULL;

    struct demux_packet *dp = packet_create();
    dp->avpacket->buf = av_buffer_ref(buf);
    if (!dp->avpacket->buf) {
        talloc_free(dp);
        return NULL;
    }
    dp->avpacket->data = dp->buffer = buf->data;
    dp->avpacket->size = dp->len = buf->size;
    return dp;
}

// Input data doesn't need to be padded.
struct demux_packet *new_demux_packet_from(void *data, size_t len)
{
    struct demux_packet *dp = new_demux_packet(len);
    if (!dp)
        return NULL;
    memcpy(dp->avpacket->data, data, len);
    return dp;
}

struct demux_packet *new_demux_packet(size_t len)
{
    if (len > INT_MAX)
        return NULL;

    struct demux_packet *dp = packet_create();
    int r = av_new_packet(dp->avpacket, len);
    if (r < 0) {
        talloc_free(dp);
        return NULL;
    }
    dp->buffer = dp->avpacket->data;
    dp->len = len;
    return dp;
}

void demux_packet_shorten(struct demux_packet *dp, size_t len)
{
    assert(len <= dp->len);
    if (dp->len) {
        dp->len = len;
        memset(dp->buffer + dp->len, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    }
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

#define ROUND_ALLOC(s) MP_ALIGN_UP((s), 16)

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
    size += 8 * sizeof(void *); // ta  overhead
    size += 10 * sizeof(void *); // additional estimate for ta_ext_header
    if (dp->avpacket) {
        assert(!dp->is_cached);
        size += ROUND_ALLOC(dp->len);
        size += ROUND_ALLOC(sizeof(AVPacket));
        size += 8 * sizeof(void *); // ta  overhead
        size += ROUND_ALLOC(sizeof(AVBufferRef));
        size += ROUND_ALLOC(64); // upper bound estimate on sizeof(AVBuffer)
        size += ROUND_ALLOC(dp->avpacket->side_data_elems *
                            sizeof(dp->avpacket->side_data[0]));
        for (int n = 0; n < dp->avpacket->side_data_elems; n++)
            size += ROUND_ALLOC(dp->avpacket->side_data[n].size);
    }
    return size;
}

int demux_packet_set_padding(struct demux_packet *dp, int start, int end)
{
    if (!start && !end)
        return 0;
    if (!dp->avpacket)
        return -1;
    uint8_t *p = av_packet_new_side_data(dp->avpacket, AV_PKT_DATA_SKIP_SAMPLES, 10);
    if (!p)
        return -1;

    AV_WL32(p + 0, start);
    AV_WL32(p + 4, end);
    return 0;
}

int demux_packet_add_blockadditional(struct demux_packet *dp, uint64_t id,
                                     void *data, size_t size)
{
    if (!dp->avpacket)
        return -1;

    switch (id) {
    case MATROSKA_BLOCK_ADD_ID_TYPE_ITU_T_T35: {
        static const uint8_t ITU_T_T35_COUNTRY_CODE_US = 0xB5;
        static const uint16_t ITU_T_T35_PROVIDER_CODE_SMTPE = 0x3C;

        if (size < 6)
            break;

        uint8_t *p = data;

        uint8_t country_code = AV_RB8(p);
        p += sizeof(country_code);
        uint16_t provider_code = AV_RB16(p);
        p += sizeof(provider_code);

        if (country_code != ITU_T_T35_COUNTRY_CODE_US ||
            provider_code != ITU_T_T35_PROVIDER_CODE_SMTPE)
            break;

        uint16_t provider_oriented_code = AV_RB16(p);
        p += sizeof(provider_oriented_code);
        uint8_t application_identifier = AV_RB8(p);
        p += sizeof(application_identifier);

        if (provider_oriented_code != 1 || application_identifier != 4)
            break;

        size_t hdrplus_size;
        AVDynamicHDRPlus *hdrplus = av_dynamic_hdr_plus_alloc(&hdrplus_size);
        MP_HANDLE_OOM(hdrplus);

        if (av_dynamic_hdr_plus_from_t35(hdrplus, p, size - (p - (uint8_t *)data)) < 0 ||
            av_packet_add_side_data(dp->avpacket, AV_PKT_DATA_DYNAMIC_HDR10_PLUS,
                                    (uint8_t *)hdrplus, hdrplus_size) < 0)
        {
            av_free(hdrplus);
            return -1;
        }

        return 0;
    }
    default:
        break;
    }

    uint8_t *sd = av_packet_new_side_data(dp->avpacket,
                                          AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL,
                                          8 + size);
    if (!sd)
        return -1;
    AV_WB64(sd, id);
    if (size > 0)
        memcpy(sd + 8, data, size);
    return 0;
}
