#ifndef _DEMUX_RTP_INTERNAL_H
#define _DEMUX_RTP_INTERNAL_H

#include <stdlib.h>

extern "C" {
#ifndef __STREAM_H
#include "stream/stream.h"
#endif
#ifndef __DEMUXER_H
#include "demuxer.h"
#endif
#ifdef USE_LIBAVCODEC_SO
#include <ffmpeg/avcodec.h>
#elif defined(USE_LIBAVCODEC)
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

#endif
