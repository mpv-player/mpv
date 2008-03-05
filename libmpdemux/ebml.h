#ifndef MPLAYER_EBML_H
#define MPLAYER_EBML_H

#include <inttypes.h>
#include "stream/stream.h"


/* EBML version supported */
#define EBML_VERSION 1

/*
 * EBML element IDs. max. 32-bit.
 */

/* top-level master-IDs */
#define EBML_ID_HEADER                   0x1A45DFA3

/* IDs in the HEADER master */
#define EBML_ID_EBMLVERSION              0x4286
#define EBML_ID_EBMLREADVERSION          0x42F7
#define EBML_ID_EBMLMAXIDLENGTH          0x42F2
#define EBML_ID_EBMLMAXSIZELENGTH        0x42F3
#define EBML_ID_DOCTYPE                  0x4282
#define EBML_ID_DOCTYPEVERSION           0x4287
#define EBML_ID_DOCTYPEREADVERSION       0x4285

/* general EBML types */
#define EBML_ID_VOID                     0xEC

/* ID returned in error cases */
#define EBML_ID_INVALID                  0xFFFFFFFF


/*
 * Matroska element IDs. max. 32-bit.
 */

/* toplevel segment */
#define MATROSKA_ID_SEGMENT              0x18538067

/* matroska top-level master IDs */
#define MATROSKA_ID_INFO                 0x1549A966
#define MATROSKA_ID_TRACKS               0x1654AE6B
#define MATROSKA_ID_CUES                 0x1C53BB6B
#define MATROSKA_ID_TAGS                 0x1254C367
#define MATROSKA_ID_SEEKHEAD             0x114D9B74
#define MATROSKA_ID_ATTACHMENTS          0x1941A469
#define MATROSKA_ID_CHAPTERS             0x1043A770
#define MATROSKA_ID_CLUSTER              0x1F43B675

/* IDs in the info master */
#define MATROSKA_ID_TIMECODESCALE        0x2AD7B1
#define MATROSKA_ID_DURATION             0x4489
#define MATROSKA_ID_WRITINGAPP           0x5741
#define MATROSKA_ID_MUXINGAPP            0x4D80
#define MATROSKA_ID_DATEUTC              0x4461

/* ID in the tracks master */
#define MATROSKA_ID_TRACKENTRY           0xAE

/* IDs in the trackentry master */
#define MATROSKA_ID_TRACKNUMBER          0xD7
#define MATROSKA_ID_TRACKUID             0x73C5
#define MATROSKA_ID_TRACKTYPE            0x83
#define MATROSKA_ID_TRACKAUDIO           0xE1
#define MATROSKA_ID_TRACKVIDEO           0xE0
#define MATROSKA_ID_CODECID              0x86
#define MATROSKA_ID_CODECPRIVATE         0x63A2
#define MATROSKA_ID_CODECNAME            0x258688
#define MATROSKA_ID_CODECINFOURL         0x3B4040
#define MATROSKA_ID_CODECDOWNLOADURL     0x26B240
#define MATROSKA_ID_TRACKNAME            0x536E
#define MATROSKA_ID_TRACKLANGUAGE        0x22B59C
#define MATROSKA_ID_TRACKFLAGENABLED     0xB9
#define MATROSKA_ID_TRACKFLAGDEFAULT     0x88
#define MATROSKA_ID_TRACKFLAGLACING      0x9C
#define MATROSKA_ID_TRACKMINCACHE        0x6DE7
#define MATROSKA_ID_TRACKMAXCACHE        0x6DF8
#define MATROSKA_ID_TRACKDEFAULTDURATION 0x23E383
#define MATROSKA_ID_TRACKENCODINGS       0x6D80

/* IDs in the trackaudio master */
#define MATROSKA_ID_AUDIOSAMPLINGFREQ    0xB5
#define MATROSKA_ID_AUDIOBITDEPTH        0x6264
#define MATROSKA_ID_AUDIOCHANNELS        0x9F

/* IDs in the trackvideo master */
#define MATROSKA_ID_VIDEOFRAMERATE       0x2383E3
#define MATROSKA_ID_VIDEODISPLAYWIDTH    0x54B0
#define MATROSKA_ID_VIDEODISPLAYHEIGHT   0x54BA
#define MATROSKA_ID_VIDEOPIXELWIDTH      0xB0
#define MATROSKA_ID_VIDEOPIXELHEIGHT     0xBA
#define MATROSKA_ID_VIDEOFLAGINTERLACED  0x9A
#define MATROSKA_ID_VIDEOSTEREOMODE      0x53B9
#define MATROSKA_ID_VIDEODISPLAYUNIT     0x54B2
#define MATROSKA_ID_VIDEOASPECTRATIO     0x54B3
#define MATROSKA_ID_VIDEOCOLOURSPACE     0x2EB524
#define MATROSKA_ID_VIDEOGAMMA           0x2FB523

/* IDs in the trackencodings master */
#define MATROSKA_ID_CONTENTENCODING      0x6240
#define MATROSKA_ID_CONTENTENCODINGORDER 0x5031
#define MATROSKA_ID_CONTENTENCODINGSCOPE 0x5032
#define MATROSKA_ID_CONTENTENCODINGTYPE  0x5033
#define MATROSKA_ID_CONTENTCOMPRESSION   0x5034
#define MATROSKA_ID_CONTENTCOMPALGO      0x4254
#define MATROSKA_ID_CONTENTCOMPSETTINGS  0x4255

/* ID in the cues master */
#define MATROSKA_ID_POINTENTRY           0xBB

/* IDs in the pointentry master */
#define MATROSKA_ID_CUETIME              0xB3
#define MATROSKA_ID_CUETRACKPOSITION     0xB7

/* IDs in the cuetrackposition master */
#define MATROSKA_ID_CUETRACK             0xF7
#define MATROSKA_ID_CUECLUSTERPOSITION   0xF1

/* IDs in the seekhead master */
#define MATROSKA_ID_SEEKENTRY            0x4DBB

/* IDs in the seekpoint master */
#define MATROSKA_ID_SEEKID               0x53AB
#define MATROSKA_ID_SEEKPOSITION         0x53AC

/* IDs in the chapters master */
#define MATROSKA_ID_EDITIONENTRY         0x45B9
#define MATROSKA_ID_CHAPTERATOM          0xB6
#define MATROSKA_ID_CHAPTERTIMESTART     0x91
#define MATROSKA_ID_CHAPTERTIMEEND       0x92
#define MATROSKA_ID_CHAPTERDISPLAY       0x80
#define MATROSKA_ID_CHAPSTRING           0x85

/* IDs in the cluster master */
#define MATROSKA_ID_CLUSTERTIMECODE      0xE7
#define MATROSKA_ID_BLOCKGROUP           0xA0

/* IDs in the blockgroup master */
#define MATROSKA_ID_BLOCKDURATION        0x9B
#define MATROSKA_ID_BLOCK                0xA1
#define MATROSKA_ID_SIMPLEBLOCK          0xA3
#define MATROSKA_ID_REFERENCEBLOCK       0xFB

/* IDs in the attachments master */
#define MATROSKA_ID_ATTACHEDFILE	 0x61A7
#define MATROSKA_ID_FILENAME		 0x466E
#define MATROSKA_ID_FILEMIMETYPE	 0x4660
#define MATROSKA_ID_FILEDATA		 0x465C
#define MATROSKA_ID_FILEUID		 0x46AE

/* matroska track types */
#define MATROSKA_TRACK_VIDEO    0x01 /* rectangle-shaped pictures aka video */
#define MATROSKA_TRACK_AUDIO    0x02 /* anything you can hear */
#define MATROSKA_TRACK_COMPLEX  0x03 /* audio+video in same track used by DV */
#define MATROSKA_TRACK_LOGO     0x10 /* overlay-pictures displayed over video*/
#define MATROSKA_TRACK_SUBTITLE 0x11 /* text-subtitles */
#define MATROSKA_TRACK_CONTROL  0x20 /* control-codes for menu or other stuff*/

/* matroska subtitle types */
#define MATROSKA_SUBTYPE_UNKNOWN   0
#define MATROSKA_SUBTYPE_TEXT      1
#define MATROSKA_SUBTYPE_SSA       2
#define MATROSKA_SUBTYPE_VOBSUB    3

#ifndef UINT64_MAX
#define UINT64_MAX 18446744073709551615ULL
#endif

#ifndef INT64_MAX
#define INT64_MAX 9223372036854775807LL
#endif

#define EBML_UINT_INVALID   UINT64_MAX
#define EBML_INT_INVALID    INT64_MAX
#define EBML_FLOAT_INVALID  -1000000000.0


uint32_t ebml_read_id (stream_t *s, int *length);
uint64_t ebml_read_vlen_uint (uint8_t *buffer, int *length);
int64_t ebml_read_vlen_int (uint8_t *buffer, int *length);
uint64_t ebml_read_length (stream_t *s, int *length);
uint64_t ebml_read_uint (stream_t *s, uint64_t *length);
int64_t ebml_read_int (stream_t *s, uint64_t *length);
long double ebml_read_float (stream_t *s, uint64_t *length);
char *ebml_read_ascii (stream_t *s, uint64_t *length);
char *ebml_read_utf8 (stream_t *s, uint64_t *length);
int ebml_read_skip (stream_t *s, uint64_t *length);
uint32_t ebml_read_master (stream_t *s, uint64_t *length);
char *ebml_read_header (stream_t *s, int *version);

#endif /* MPLAYER_EBML_H */
