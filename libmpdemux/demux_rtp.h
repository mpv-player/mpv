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

#ifndef MPLAYER_DEMUX_RTP_H
#define MPLAYER_DEMUX_RTP_H

#include <stdlib.h>
#include <stdio.h>
#include "demuxer.h"

// Open a RTP demuxer (which was initiated either from a SDP file,
// or from a RTSP URL):
demuxer_t* demux_open_rtp(demuxer_t* demuxer);

// Test whether a RTP demuxer is for a MPEG stream:
int demux_is_mpeg_rtp_stream(demuxer_t* demuxer);

// Test whether a RTP demuxer contains combined (multiplexed)
// audio+video (and so needs to be demuxed by higher-level code):
int demux_is_multiplexed_rtp_stream(demuxer_t* demuxer);

// Read from a RTP demuxer:
int demux_rtp_fill_buffer(demuxer_t *demux, demux_stream_t* ds);

// Close a RTP demuxer
void demux_close_rtp(demuxer_t* demuxer);

#endif /* MPLAYER_DEMUX_RTP_H */
