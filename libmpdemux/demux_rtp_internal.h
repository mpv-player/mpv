#ifndef _DEMUX_RTP_INTERNAL_H
#define _DEMUX_RTP_INTERNAL_H

#include <stdlib.h>

extern "C" {
#ifndef __STREAM_H
#include "stream.h"
#endif
#ifndef __DEMUXER_H
#include "demuxer.h"
#endif
}

#ifndef _LIVEMEDIA_HH
#include <liveMedia.hh>
#endif

#if (LIVEMEDIA_LIBRARY_VERSION_INT < 1046649600)
#error Please upgrade to version 2003.03.03 or later of the "LIVE.COM Streaming Media" libraries - available from <www.live.com/liveMedia/>
#endif

// Codec-specific initialization routines:
void rtpCodecInitialize_video(demuxer_t* demuxer,
			      MediaSubsession* subsession, unsigned& flags);
void rtpCodecInitialize_audio(demuxer_t* demuxer,
			      MediaSubsession* subsession, unsigned& flags);

// Flags that may be set by the above routines:
#define RTPSTATE_IS_MPEG12_VIDEO 0x1 // is a MPEG-1 or 2 video stream

// A routine to wait for the first packet of a RTP stream to arrive.
// (For some RTP payload formats, codecs cannot be fully initialized until
// we've started receiving data.)
Boolean awaitRTPPacket(demuxer_t* demuxer, demux_stream_t* ds,
		       unsigned char*& packetData, unsigned& packetDataLen,
		       float& pts);
    // "streamType": 0 => video; 1 => audio
    // This routine returns False if the input stream has closed

// A routine for adding our own data to an incoming RTP data stream:
Boolean insertRTPData(demuxer_t* demuxer, demux_stream_t* ds,
		      unsigned char* data, unsigned dataLen);

#endif
