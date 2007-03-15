/* parse_mp4.c - MP4 file format parser code
 * This file is part of MPlayer, see http://mplayerhq.hu/ for info.  
 * (c)2002 by Felix Buenemann <atmosfear at users.sourceforge.net>
 * File licensed under the GPL, see http://www.fsf.org/ for more info.
 * Code inspired by libmp4 from http://mpeg4ip.sourceforge.net/.
 */
   
#include <stdio.h>
#include <inttypes.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include "parse_mp4.h"
#include "mp_msg.h"
#include "stream/stream.h"

//#define MP4_DUMPATOM

#define MP4_DL MSGL_V
#define freereturn(a,b) free(a); return b

int mp4_read_descr_len(stream_t *s) {
  uint8_t b;
  uint8_t numBytes = 0;
  uint32_t length = 0;

  do {
    b = stream_read_char(s);
    numBytes++;
    length = (length << 7) | (b & 0x7F);
  } while ((b & 0x80) && numBytes < 4);

  //printf("MP4 read desc len: %d\n", length);
  return length;
}

/* parse the data part of MP4 esds atoms */
int mp4_parse_esds(unsigned char *data, int datalen, esds_t *esds) {
  /* create memory stream from data */
  stream_t *s = new_memory_stream(data, datalen);
  uint16_t len;
#ifdef MP4_DUMPATOM
  {int i;
  printf("ESDS Dump (%dbyte):\n", datalen);  
  for(i = 0; i < datalen; i++)
    printf("%02X ", data[i]);
  printf("\nESDS Dumped\n");}
#endif  
  memset(esds, 0, sizeof(esds_t));

  esds->version = stream_read_char(s);
  esds->flags = stream_read_int24(s);
  mp_msg(MSGT_DEMUX, MP4_DL,
      "ESDS MPEG4 version: %d  flags: 0x%06X\n",
      esds->version, esds->flags);

  /* get and verify ES_DescrTag */
  if (stream_read_char(s) == MP4ESDescrTag) {
    /* read length */
    len = mp4_read_descr_len(s);

    esds->ESId = stream_read_word(s);
    esds->streamPriority = stream_read_char(s);
    mp_msg(MSGT_DEMUX, MP4_DL,
      	"ESDS MPEG4 ES Descriptor (%dBytes):\n"
	" -> ESId: %d\n"
	" -> streamPriority: %d\n",
	len, esds->ESId, esds->streamPriority);

    if (len < (5 + 15)) {
      freereturn(s,1);
    }
  } else {
    esds->ESId = stream_read_word(s);
    mp_msg(MSGT_DEMUX, MP4_DL,
      	"ESDS MPEG4 ES Descriptor (%dBytes):\n"
	" -> ESId: %d\n", 2, esds->ESId);
  }

  /* get and verify DecoderConfigDescrTab */
  if (stream_read_char(s) != MP4DecConfigDescrTag) {
    freereturn(s,1);
  }

  /* read length */
  len = mp4_read_descr_len(s);

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

  esds->decoderConfigLen=0;

  if (len < 15) {
    freereturn(s,0);
  }

  /* get and verify DecSpecificInfoTag */
  if (stream_read_char(s) != MP4DecSpecificDescrTag) {
    freereturn(s,0);
  }

  /* read length */
  esds->decoderConfigLen = len = mp4_read_descr_len(s); 

  esds->decoderConfig = malloc(esds->decoderConfigLen);
  if (esds->decoderConfig) {
    stream_read(s, esds->decoderConfig, esds->decoderConfigLen);
  } else {
    esds->decoderConfigLen = 0;
  }
  mp_msg(MSGT_DEMUX, MP4_DL,
      "ESDS MPEG4 Decoder Specific Descriptor (%dBytes)\n", len);

  /* get and verify SLConfigDescrTag */
  if(stream_read_char(s) != MP4SLConfigDescrTag) {
    freereturn(s,0);
  }

  /* Note: SLConfig is usually constant value 2, size 1Byte */
  esds->SLConfigLen = len = mp4_read_descr_len(s);
  esds->SLConfig = malloc(esds->SLConfigLen);
  if (esds->SLConfig) {
    stream_read(s, esds->SLConfig, esds->SLConfigLen);
  } else {
    esds->SLConfigLen = 0;
  }
  mp_msg(MSGT_DEMUX, MP4_DL,
      "ESDS MPEG4 Sync Layer Config Descriptor (%dBytes)\n"
      " -> predefined: %d\n", len, esds->SLConfig[0]);

  /* will skip the remainder of the atom */
  freereturn(s,0);

}

/* cleanup all mem occupied by mp4_parse_esds */
void mp4_free_esds(esds_t *esds) {
  if(esds->decoderConfigLen)
    free(esds->decoderConfig);
  if(esds->SLConfigLen)
    free(esds->SLConfig);
}

#undef freereturn
#undef MP4_DL

