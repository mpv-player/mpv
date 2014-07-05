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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <libavcodec/avcodec.h>

#include "common/av_common.h"
#include "common/common.h"

#include "packet.h"

static void packet_destroy(void *ptr)
{
    struct demux_packet *dp = ptr;
    talloc_free(dp->avpacket);
    av_free(dp->allocation);
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
    dp->buffer = av_malloc(len + FF_INPUT_BUFFER_PADDING_SIZE);
    if (!dp->buffer) {
        fprintf(stderr, "Memory allocation failure!\n");
        abort();
    }
    memset(dp->buffer + len, 0, FF_INPUT_BUFFER_PADDING_SIZE);
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

void demux_packet_shorten(struct demux_packet *dp, size_t len)
{
    assert(len <= dp->len);
    dp->len = len;
    memset(dp->buffer + dp->len, 0, FF_INPUT_BUFFER_PADDING_SIZE);
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
    if (dp->avpacket) {
        assert(dp->buffer == dp->avpacket->data);
        assert(dp->len == dp->avpacket->size);
        AVPacket *newavp = talloc_zero(NULL, AVPacket);
        talloc_set_destructor(newavp, destroy_avpacket);
        av_init_packet(newavp);
        if (av_packet_ref(newavp, dp->avpacket) < 0)
            abort();
        new = new_demux_packet_fromdata(newavp->data, newavp->size);
        new->avpacket = newavp;
    }
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
