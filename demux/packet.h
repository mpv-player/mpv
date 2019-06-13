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

#ifndef MPLAYER_DEMUX_PACKET_H
#define MPLAYER_DEMUX_PACKET_H

#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

// Holds one packet/frame/whatever
typedef struct demux_packet {
    double pts;
    double dts;
    double duration;
    int64_t pos;        // position in source file byte stream

    union {
        // Normally valid for packets.
        struct {
            unsigned char *buffer;
            size_t len;
        };

        // Used if is_cached==true, special uses only.
        struct {
            uint64_t pos;
        } cached_data;
    };

    int stream;         // source stream index (typically sh_stream.index)

    bool keyframe;

    // backward playback
    bool back_restart : 1;  // restart point (reverse and return previous frames)
    bool back_preroll : 1;  // initial discarded frame for smooth decoder reinit

    // If true, cached_data is valid, while buffer/len are not.
    bool is_cached : 1;

    // segmentation (ordered chapters, EDL)
    bool segmented;
    struct mp_codec_params *codec;  // set to non-NULL iff segmented is set
    double start, end;              // set to non-NOPTS iff segmented is set

    // private
    struct demux_packet *next;
    struct AVPacket *avpacket;   // keep the buffer allocation and sidedata
    uint64_t cum_pos; // demux.c internal: cumulative size until _start_ of pkt
} demux_packet_t;

struct AVBufferRef;

struct demux_packet *new_demux_packet(size_t len);
struct demux_packet *new_demux_packet_from_avpacket(struct AVPacket *avpkt);
struct demux_packet *new_demux_packet_from(void *data, size_t len);
struct demux_packet *new_demux_packet_from_buf(struct AVBufferRef *buf);
void demux_packet_shorten(struct demux_packet *dp, size_t len);
void free_demux_packet(struct demux_packet *dp);
struct demux_packet *demux_copy_packet(struct demux_packet *dp);
size_t demux_packet_estimate_total_size(struct demux_packet *dp);

void demux_packet_copy_attribs(struct demux_packet *dst, struct demux_packet *src);

int demux_packet_set_padding(struct demux_packet *dp, int start, int end);
int demux_packet_add_blockadditional(struct demux_packet *dp, uint64_t id,
                                     void *data, size_t size);

void demux_packet_unref_contents(struct demux_packet *dp);

#endif /* MPLAYER_DEMUX_PACKET_H */
