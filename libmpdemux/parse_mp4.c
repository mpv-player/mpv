/* parse_mp4.c - MP4 file format parser code
 * This file is part of MPlayer, see http://mplayerhq.hu/ for info.  
 * (c)2002 by Felix Buenemann <atmosfear at users.sourceforge.net>
 * File licensed under the GPL, see http://www.fsf.org/ for more info.
 * Code inspired by libmp4 from http://mpeg4ip.sourceforge.net/.
 */
   
#include <stdio.h>
#include <inttypes.h>
#include <malloc.h>
#include "parse_mp4.h"
#include "mp_msg.h"
#include "stream.h"

#define MP4_DL MSGL_V

int mp4_read_descr_len(stream_t *s) {
  uint8_t b;
  uint8_t numBytes = 0;
  uint32_t length = 0;

  do {
    b = stream_read_char(s);
    numBytes++;
    length = (length << 7) | (b & 0x7F);
  } while ((b & 0x80) && numBytes < 4);

  return length;
}

int mp4_parse_esds(unsigned char *data, int datalen, esds_t *esds) {
  /* create memory stream from data */
  stream_t *s = new_memory_stream(data, datalen);
  uint8_t tag;
  uint8_t len;

  esds->version = stream_read_char(s);
  esds->flags = stream_read_int24(s);
  mp_msg(MSGT_DEMUX, MP4_DL,
      "ESDS MPEG4 version: %d  flags: 0x%06X\n",
      esds->version, esds->flags);

  /* get and verify ES_DescrTag */
  tag = stream_read_char(s);
  if (tag == MP4ESDescrTag) {
    /* read length */
    if ((len = mp4_read_descr_len(s)) < 5 + 15) {
      return 1;
    }
    esds->ESId = stream_read_word(s);
    esds->streamPriority = stream_read_char(s);
  } else {
#if 1 /* 1 == guessed */
    esds->ESId = stream_read_word(s);
#else    
    /* skip 2 bytes */
    stream_skip(s, 2);
#endif
  }
  mp_msg(MSGT_DEMUX, MP4_DL,
      "ESDS MPEG4 ES Descriptor (%dBytes):\n"
      " -> ESId: %d\n"
      " -> streamPriority: %d\n",
      len, esds->ESId, esds->streamPriority);

  /* get and verify DecoderConfigDescrTab */
  if (stream_read_char(s) != MP4DecConfigDescrTag) {
    return 1;
  }

  /* read length */
  if ((len = mp4_read_descr_len(s)) < 15) {
    return 1;
  }

  esds->objectTypeId = stream_read_char(s);
  esds->streamType = stream_read_char(s);
  esds->bufferSizeDB = stream_read_int24(s);
  esds->maxBitrate = stream_read_dword(s);
  esds->avgBitrate = stream_read_dword(s);
  mp_msg(MSGT_DEMUX, MP4_DL,
      "ESDS MPEG4 Decoder Config Descriptor (%dBytes):\n"
      " -> objectTypeId: %d\n"
      " -> streamType: 0x%02X\n"
      " -> bufferSizeDB: 0x%06X\n"
      " -> maxBitrate: %.3fkbit/s\n"
      " -> avgBitrate: %.3fkbit/s\n",
      len, esds->objectTypeId, esds->streamType,
      esds->bufferSizeDB, esds->maxBitrate/1000.0,
      esds->avgBitrate/1000.0);

  /* get and verify DecSpecificInfoTag */
  if (stream_read_char(s) != MP4DecSpecificDescrTag) {
    return 1;
  }

  /* read length */
  esds->decoderConfigLen = len = mp4_read_descr_len(s); 

  free(esds->decoderConfig);
  esds->decoderConfig = malloc(esds->decoderConfigLen);
  if (esds->decoderConfig) {
    stream_read(s, esds->decoderConfig, esds->decoderConfigLen);
  } else {
    esds->decoderConfigLen = 0;
  }
  mp_msg(MSGT_DEMUX, MP4_DL,
      "ESDS MPEG4 Decoder Specific Descriptor (%dBytes)\n", len);

  /* will skip the remainder of the atom */
  return 0;

}

