#ifndef ASF_H
#define ASF_H

#include <inttypes.h>

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
  uint32_t unk1; //Unknown, maybe reserved ( usually contains 0 ) UINT32 4
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
	ASF_Redirector_e
} ASF_StreamType_e;

#endif
