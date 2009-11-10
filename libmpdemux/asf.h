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

#ifndef MPLAYER_ASF_H
#define MPLAYER_ASF_H

//#include "config.h"	/* for HAVE_BIGENDIAN */
#include <inttypes.h>
#include "libavutil/common.h"
#include "mpbswap.h"

///////////////////////
// ASF Object Header
///////////////////////
typedef struct __attribute__((packed)) {
  uint8_t guid[16];
  uint64_t size;
} ASF_obj_header_t;

////////////////
// ASF Header
////////////////
typedef struct __attribute__((packed)) {
  ASF_obj_header_t objh;
  uint32_t cno; // number of subchunks
  uint8_t v1; // unknown (0x01)
  uint8_t v2; // unknown (0x02)
} ASF_header_t;

/////////////////////
// ASF File Header
/////////////////////
typedef struct __attribute__((packed)) {
  uint8_t stream_id[16]; // stream GUID
  uint64_t file_size;
  uint64_t creation_time; //File creation time FILETIME 8
  uint64_t num_packets;    //Number of packets UINT64 8
  uint64_t play_duration; //Timestamp of the end position UINT64 8
  uint64_t send_duration;  //Duration of the playback UINT64 8
  uint64_t preroll; //Time to bufferize before playing UINT32 4
  uint32_t flags; //Unknown, maybe flags ( usually contains 2 ) UINT32 4
  uint32_t min_packet_size; //Min size of the packet, in bytes UINT32 4
  uint32_t max_packet_size; //Max size of the packet  UINT32 4
  uint32_t max_bitrate; //Maximum bitrate of the media (sum of all the stream)
} ASF_file_header_t;

///////////////////////
// ASF Stream Header
///////////////////////
typedef struct __attribute__((packed)) {
  uint8_t type[16]; // Stream type (audio/video) GUID 16
  uint8_t concealment[16]; // Audio error concealment type GUID 16
  uint64_t unk1; // Unknown, maybe reserved ( usually contains 0 ) UINT64 8
  uint32_t type_size; //Total size of type-specific data UINT32 4
  uint32_t stream_size; //Size of stream-specific data UINT32 4
  uint16_t stream_no; //Stream number UINT16 2
  uint32_t unk2; //Unknown UINT32 4
} ASF_stream_header_t;

///////////////////////////
// ASF Content Description
///////////////////////////
typedef struct  __attribute__((packed)) {
  uint16_t title_size;
  uint16_t author_size;
  uint16_t copyright_size;
  uint16_t comment_size;
  uint16_t rating_size;
} ASF_content_description_t;

////////////////////////
// ASF Segment Header
////////////////////////
typedef struct __attribute__((packed)) {
  uint8_t streamno;
  uint8_t seq;
  uint32_t x;
  uint8_t flag;
} ASF_segmhdr_t;

//////////////////////
// ASF Stream Chunck
//////////////////////
typedef struct __attribute__((packed)) {
	uint16_t	type;
	uint16_t	size;
	uint32_t	sequence_number;
	uint16_t	unknown;
	uint16_t	size_confirm;
} ASF_stream_chunck_t;

// Definition of the stream type
#if HAVE_BIGENDIAN
	#define ASF_STREAMING_CLEAR	0x2443		// $C
	#define ASF_STREAMING_DATA	0x2444		// $D
	#define ASF_STREAMING_END_TRANS	0x2445		// $E
	#define	ASF_STREAMING_HEADER	0x2448		// $H
#else
	#define ASF_STREAMING_CLEAR	0x4324		// $C
	#define ASF_STREAMING_DATA	0x4424		// $D
	#define ASF_STREAMING_END_TRANS	0x4524		// $E
	#define	ASF_STREAMING_HEADER	0x4824		// $H
#endif

// Definition of the differents type of ASF streaming
typedef enum {
	ASF_Unknown_e,
	ASF_Live_e,
	ASF_Prerecorded_e,
	ASF_Redirector_e,
	ASF_PlainText_e,
	ASF_Authenticate_e
} ASF_StreamType_e;

typedef struct {
	ASF_StreamType_e streaming_type;
	int request;
	int packet_size;
	int *audio_streams,n_audio,*video_streams,n_video;
	int audio_id, video_id;
} asf_http_streaming_ctrl_t;


/*
 * Some macros to swap little endian structures read from an ASF file
 * into machine endian format
 */
#if HAVE_BIGENDIAN
#define	le2me_ASF_obj_header_t(h) {					\
    (h)->size = le2me_64((h)->size);					\
}
#define	le2me_ASF_header_t(h) {						\
    le2me_ASF_obj_header_t(&(h)->objh);					\
    (h)->cno = le2me_32((h)->cno);					\
}
#define le2me_ASF_stream_header_t(h) {					\
    (h)->unk1 = le2me_64((h)->unk1);					\
    (h)->type_size = le2me_32((h)->type_size);				\
    (h)->stream_size = le2me_32((h)->stream_size);			\
    (h)->stream_no = le2me_16((h)->stream_no);				\
    (h)->unk2 = le2me_32((h)->unk2);					\
}
#define le2me_ASF_file_header_t(h) {					\
    (h)->file_size = le2me_64((h)->file_size);				\
    (h)->creation_time = le2me_64((h)->creation_time);			\
    (h)->num_packets = le2me_64((h)->num_packets);			\
    (h)->play_duration = le2me_64((h)->play_duration);			\
    (h)->send_duration = le2me_64((h)->send_duration);			\
    (h)->preroll = le2me_64((h)->preroll);				\
    (h)->flags = le2me_32((h)->flags);					\
    (h)->min_packet_size = le2me_32((h)->min_packet_size);		\
    (h)->max_packet_size = le2me_32((h)->max_packet_size);		\
    (h)->max_bitrate = le2me_32((h)->max_bitrate);			\
}
#define le2me_ASF_content_description_t(h) {				\
    (h)->title_size = le2me_16((h)->title_size);			\
    (h)->author_size = le2me_16((h)->author_size);			\
    (h)->copyright_size = le2me_16((h)->copyright_size);		\
    (h)->comment_size = le2me_16((h)->comment_size);			\
    (h)->rating_size = le2me_16((h)->rating_size);			\
}
#define le2me_BITMAPINFOHEADER(h) {					\
    (h)->biSize = le2me_32((h)->biSize);				\
    (h)->biWidth = le2me_32((h)->biWidth);				\
    (h)->biHeight = le2me_32((h)->biHeight);				\
    (h)->biPlanes = le2me_16((h)->biPlanes);				\
    (h)->biBitCount = le2me_16((h)->biBitCount);			\
    (h)->biCompression = le2me_32((h)->biCompression);			\
    (h)->biSizeImage = le2me_32((h)->biSizeImage);			\
    (h)->biXPelsPerMeter = le2me_32((h)->biXPelsPerMeter);		\
    (h)->biYPelsPerMeter = le2me_32((h)->biYPelsPerMeter);		\
    (h)->biClrUsed = le2me_32((h)->biClrUsed);				\
    (h)->biClrImportant = le2me_32((h)->biClrImportant);		\
}
#define le2me_WAVEFORMATEX(h) {						\
    (h)->wFormatTag = le2me_16((h)->wFormatTag);			\
    (h)->nChannels = le2me_16((h)->nChannels);				\
    (h)->nSamplesPerSec = le2me_32((h)->nSamplesPerSec);		\
    (h)->nAvgBytesPerSec = le2me_32((h)->nAvgBytesPerSec);		\
    (h)->nBlockAlign = le2me_16((h)->nBlockAlign);			\
    (h)->wBitsPerSample = le2me_16((h)->wBitsPerSample);		\
    (h)->cbSize = le2me_16((h)->cbSize);				\
}
#define le2me_ASF_stream_chunck_t(h) {					\
    (h)->size = le2me_16((h)->size);					\
    (h)->sequence_number = le2me_32((h)->sequence_number);		\
    (h)->unknown = le2me_16((h)->unknown);				\
    (h)->size_confirm = le2me_16((h)->size_confirm);			\
}
#else
#define	le2me_ASF_obj_header_t(h)	/**/
#define	le2me_ASF_header_t(h)		/**/
#define le2me_ASF_stream_header_t(h)	/**/
#define le2me_ASF_file_header_t(h)	/**/
#define le2me_ASF_content_description_t(h) /**/
#define le2me_BITMAPINFOHEADER(h)   /**/
#define le2me_WAVEFORMATEX(h)	    /**/
#define le2me_ASF_stream_chunck_t(h) /**/
#endif

// priv struct for the demuxer
struct asf_priv {
    ASF_header_t header;
    unsigned char* packet;
    int scrambling_h;
    int scrambling_w;
    int scrambling_b;
    unsigned packetsize;
    double   packetrate;
    double movielength;
    int asf_is_dvr_ms;
    uint32_t asf_frame_state;
    int asf_frame_start_found;
    double dvr_last_vid_pts;
    uint64_t vid_frame_ct;
    uint64_t play_duration;
    uint64_t num_packets;
    int new_vid_frame_seg;
    int *vid_repdata_sizes;
    int *aud_repdata_sizes;
    int vid_repdata_count;
    int aud_repdata_count;
    uint64_t avg_vid_frame_time;
    uint64_t last_key_payload_time;
    uint64_t last_aud_pts;
    uint64_t last_aud_diff;
    int found_first_key_frame;
    uint32_t last_vid_seq;
    int vid_ext_timing_index;
    int aud_ext_timing_index;
    int vid_ext_frame_index;
    int know_frame_time;
    unsigned bps;
};

#endif /* MPLAYER_ASF_H */
