#ifndef __ASF_H
#define __ASF_H

//#include "config.h"	/* for WORDS_BIGENDIAN */
#include <inttypes.h>
#include "bswap.h"
#ifdef	STREAMING
#include "stream.h"
#include "network.h"
#endif

#ifndef MIN
#define MIN(a,b) ((a<b)?a:b)
#endif

///////////////////////
// MS GUID definition
///////////////////////
#ifndef GUID_DEFINED
#define GUID_DEFINED
// Size of GUID is 16 bytes!
typedef struct __attribute__((packed)) {
	uint32_t	Data1;		// 4 bytes
	uint16_t	Data2;		// 2 bytes
	uint16_t	Data3;		// 2 bytes
	uint8_t		Data4[8];	// 8 bytes
} GUID_t;
#endif

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
  uint8_t client[16]; // Client GUID
  uint64_t file_size;
  uint64_t creat_time; //File creation time FILETIME 8
  uint64_t packets;    //Number of packets UINT64 8
  uint64_t end_timestamp; //Timestamp of the end position UINT64 8
  uint64_t duration;  //Duration of the playback UINT64 8
  uint32_t start_timestamp; //Timestamp of the start position UINT32 4
  uint32_t preroll; //Time to bufferize before playing UINT32 4
  uint32_t flags; //Unknown, maybe flags ( usually contains 2 ) UINT32 4
  uint32_t packetsize; //Size of packet, in bytes UINT32 4
  uint32_t packetsize2; //Size of packet ( confirm ) UINT32 4
  uint32_t frame_size; //Size of uncompressed video frame UINT32 4
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


// Definition of the differents type of ASF streaming
typedef enum {
	ASF_Unknown_e,
	ASF_Live_e,
	ASF_Prerecorded_e,
	ASF_Redirector_e,
	ASF_PlainText_e
} ASF_StreamType_e;


/*
 * Some macros to swap little endian structures read from an ASF file
 * into machine endian format
 */
#ifdef WORDS_BIGENDIAN
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
    (h)->creat_time = le2me_64((h)->creat_time);			\
    (h)->packets = le2me_64((h)->packets);				\
    (h)->end_timestamp = le2me_64((h)->end_timestamp);			\
    (h)->duration = le2me_64((h)->duration);				\
    (h)->start_timestamp = le2me_32((h)->start_timestamp);		\
    (h)->unk1 = le2me_32((h)->unk1);					\
    (h)->flags = le2me_32((h)->flags);					\
    (h)->packetsize = le2me_32((h)->packetsize);			\
    (h)->packetsize2 = le2me_32((h)->packetsize2);			\
    (h)->frame_size = le2me_32((h)->frame_size);			\
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
#else
#define	le2me_ASF_obj_header_t(h)	/**/
#define	le2me_ASF_header_t(h)		/**/
#define le2me_ASF_stream_header_t(h)	/**/
#define le2me_ASF_file_header_t(h)	/**/
#define le2me_ASF_content_description_t(h) /**/
#define le2me_BITMAPINFOHEADER(h)   /**/
#define le2me_WAVEFORMATEX(h)	    /**/
#endif


#ifdef	STREAMING
//int asf_http_streaming_type(char *content_type, char *features);
int asf_http_streaming_start( stream_t *stream );
int asf_http_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *streaming_ctrl );

int asf_streaming(ASF_stream_chunck_t *stream_chunck, int *drop_packet );
#endif

#endif
