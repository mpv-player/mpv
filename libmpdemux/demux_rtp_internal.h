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

#ifndef MPLAYER_DEMUX_RTP_INTERNAL_H
#define MPLAYER_DEMUX_RTP_INTERNAL_H

#include <stdlib.h>

extern "C" {
#include "demuxer.h"
#ifdef CONFIG_LIBAVCODEC
#include "libavcodec/avcodec.h"
#endif
}

#ifndef _LIVEMEDIA_HH
#undef STREAM_SEEK
#include <liveMedia.hh>
#endif

// Codec-specific initialization routines:
void rtpCodecInitialize_video(demuxer_t* demuxer,
			      MediaSubsession* subsession, unsigned& flags);
void rtpCodecInitialize_audio(demuxer_t* demuxer,
			      MediaSubsession* subsession, unsigned& flags);

// Flags that may be set by the above routines:
#define RTPSTATE_IS_MPEG12_VIDEO 0x1 // is a MPEG-1 or 2 video stream
#define RTPSTATE_IS_MULTIPLEXED 0x2 // is a combined audio+video stream

// A routine to wait for the first packet of a RTP stream to arrive.
// (For some RTP payload formats, codecs cannot be fully initialized until
// we've started receiving data.)
Boolean awaitRTPPacket(demuxer_t* demuxer, demux_stream_t* ds,
		       unsigned char*& packetData, unsigned& packetDataLen,
		       float& pts);
    // "streamType": 0 => video; 1 => audio
    // This routine returns False if the input stream has closed

#endif /* MPLAYER_DEMUX_RTP_INTERNAL_H */
