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

// Codec-specific initialization routines:
void rtpCodecInitialize_video(demuxer_t* demuxer,
			      MediaSubsession* subsession, unsigned& flags);
void rtpCodecInitialize_audio(demuxer_t* demuxer,
			      MediaSubsession* subsession, unsigned& flags);

// Flags that may be set by the above routines:
#define RTPSTATE_IS_MPEG 0x1 // is an MPEG audio, video or transport stream

// A routine to wait for the first packet of a RTP stream to arrive.
// (For some RTP payload formats, codecs cannot be fully initialized until
// we've started receiving data.)
Boolean awaitRTPPacket(demuxer_t* demuxer, unsigned streamType,
		       unsigned char*& packetData, unsigned& packetDataLen);
    // "streamType": 0 => video; 1 => audio
    // This routine returns False if the input stream has closed

#endif
