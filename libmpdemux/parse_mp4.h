/* parse_mp4.h - Headerfile for MP4 file format parser code
 * This file is part of MPlayer, see http://mplayerhq.hu/ for info.  
 * (c)2002 by Felix Buenemann <atmosfear at users.sourceforge.net>
 * File licensed under the GPL, see http://www.fsf.org/ for more info.
 */

#ifndef __PARSE_MP4_H
#define __PARSE_MP4_H 1

#include <inttypes.h>

/* one byte tag identifiers */
#define MP4ODescrTag			0x01 
#define MP4IODescrTag			0x02 
#define MP4ESDescrTag			0x03 
#define MP4DecConfigDescrTag		0x04 
#define MP4DecSpecificDescrTag		0x05 
#define MP4SLConfigDescrTag		0x06 
#define MP4ContentIdDescrTag		0x07 
#define MP4SupplContentIdDescrTag	0x08 
#define MP4IPIPtrDescrTag		0x09 
#define MP4IPMPPtrDescrTag		0x0A 
#define MP4IPMPDescrTag			0x0B 
#define MP4RegistrationDescrTag		0x0D 
#define MP4ESIDIncDescrTag		0x0E 
#define MP4ESIDRefDescrTag		0x0F 
#define MP4FileIODescrTag		0x10 
#define MP4FileODescrTag		0x11 
#define MP4ExtProfileLevelDescrTag	0x13 
#define MP4ExtDescrTagsStart		0x80 
#define MP4ExtDescrTagsEnd		0xFE 

/* I define uint24 here for better understanding */
#ifndef uint24_t
#define uint24_t uint32_t
#endif

/* esds_t */
typedef struct {
  uint8_t  version;
  uint24_t flags;
  
  /* 0x03 ESDescrTag */
  uint16_t ESId;
  uint8_t  streamPriority;
  
  /* 0x04 DecConfigDescrTag */
  uint8_t  objectTypeId;
  uint8_t  streamType;
  /* XXX: really streamType is
   * only 6bit, followed by:
   * 1bit  upStream
   * 1bit  reserved
   */  
  uint24_t bufferSizeDB;
  uint32_t maxBitrate;
  uint32_t avgBitrate;

  /* 0x05 DecSpecificDescrTag */
  uint8_t  decoderConfigLen;
  uint8_t *decoderConfig;

  /* 0x06 SLConfigDescrTag */
  uint8_t  SLConfigLen;
  uint8_t *SLConfig;

  /* TODO: add the missing tags,
   * I currently have no specs
   * for them and doubt they
   * are currently needed ::atmos
   */
  
} esds_t;

int mp4_parse_esds(unsigned char *data, int datalen, esds_t *esds);
void mp4_free_esds(esds_t *esds); 

#endif /* !__PARSE_MP4_H */

