#ifndef _DEMUX_RTP_H
#define _DEMUX_RTP_H

#include <stdlib.h>
#include <stdio.h>

#ifndef __STREAM_H
#include "stream.h"
#endif
#ifndef __DEMUXER_H
#include "demuxer.h"
#endif

// Open a SDP file:
stream_t* stream_open_sdp(int fd, off_t fileSize, int* file_format);

// Open a RTSP URL:
int rtsp_streaming_start(stream_t* stream);

// Open a RTP demuxer (which was initiated either from a SDP file,
// or from a RTSP URL):
void demux_open_rtp(demuxer_t* demuxer);

// Test whether a RTP demuxer is for a MPEG stream:
int demux_is_mpeg_rtp_stream(demuxer_t* demuxer);

// Read from a RTP demuxer:
int demux_rtp_fill_buffer(demuxer_t *demux, demux_stream_t* ds);

// Close a RTP demuxer
void demux_close_rtp(demuxer_t* demuxer);

#endif
