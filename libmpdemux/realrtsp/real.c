/*
 * This file was ported to MPlayer from xine CVS real.c,v 1.8 2003/03/30 17:11:50
 */

/*
 * Copyright (C) 2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * special functions for real streams.
 * adopted from joschkas real tools.
 *
 */

#include <stdio.h>
#include <string.h>

#include "real.h"
#include "asmrp.h"
#include "sdpplin.h"

/*
#define LOG
*/

const unsigned char xor_table[] = {
    0x05, 0x18, 0x74, 0xd0, 0x0d, 0x09, 0x02, 0x53,
    0xc0, 0x01, 0x05, 0x05, 0x67, 0x03, 0x19, 0x70,
    0x08, 0x27, 0x66, 0x10, 0x10, 0x72, 0x08, 0x09,
    0x63, 0x11, 0x03, 0x71, 0x08, 0x08, 0x70, 0x02,
    0x10, 0x57, 0x05, 0x18, 0x54, 0x00, 0x00, 0x00 };


#define BE_32C(x,y) x[3]=(char)(y & 0xff);\
    x[2]=(char)((y >> 8) & 0xff);\
    x[1]=(char)((y >> 16) & 0xff);\
    x[0]=(char)((y >> 24) & 0xff);

#define BE_16(x)  ((((uint8_t*)(x))[0] << 8) | ((uint8_t*)(x))[1])

#define BE_32(x)  ((((uint8_t*)(x))[0] << 24) | \
                   (((uint8_t*)(x))[1] << 16) | \
                   (((uint8_t*)(x))[2] << 8) | \
                    ((uint8_t*)(x))[3])

#define MAX(x,y) ((x>y) ? x : y)

#ifdef LOG
static void hexdump (const char *buf, int length) {

  int i;

  printf (" hexdump> ");
  for (i = 0; i < length; i++) {
    unsigned char c = buf[i];

    printf ("%02x", c);

    if ((i % 16) == 15)
      printf ("\n         ");

    if ((i % 2) == 1)
      printf (" ");

  }
  printf ("\n");
}
#endif


static void hash(char *field, char *param) {

  uint32_t a, b, c, d;
 

  /* fill variables */
  memcpy(&a, field, sizeof(uint32_t));
  memcpy(&b, &field[4], sizeof(uint32_t));
  memcpy(&c, &field[8], sizeof(uint32_t));
  memcpy(&d, &field[12], sizeof(uint32_t));

#ifdef LOG
  printf("real: hash input: %x %x %x %x\n", a, b, c, d);
  printf("real: hash parameter:\n");
  hexdump(param, 64);
#endif
  
  a = ((b & c) | (~b & d)) + *((uint32_t*)(param+0x00)) + a - 0x28955B88;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + *((uint32_t*)(param+0x04)) + d - 0x173848AA;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + *((uint32_t*)(param+0x08)) + c + 0x242070DB;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + *((uint32_t*)(param+0x0c)) + b - 0x3E423112;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  a = ((b & c) | (~b & d)) + *((uint32_t*)(param+0x10)) + a - 0x0A83F051;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + *((uint32_t*)(param+0x14)) + d + 0x4787C62A;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + *((uint32_t*)(param+0x18)) + c - 0x57CFB9ED;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + *((uint32_t*)(param+0x1c)) + b - 0x02B96AFF;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  a = ((b & c) | (~b & d)) + *((uint32_t*)(param+0x20)) + a + 0x698098D8;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + *((uint32_t*)(param+0x24)) + d - 0x74BB0851;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + *((uint32_t*)(param+0x28)) + c - 0x0000A44F;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + *((uint32_t*)(param+0x2C)) + b - 0x76A32842;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  a = ((b & c) | (~b & d)) + *((uint32_t*)(param+0x30)) + a + 0x6B901122;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + *((uint32_t*)(param+0x34)) + d - 0x02678E6D;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + *((uint32_t*)(param+0x38)) + c - 0x5986BC72;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + *((uint32_t*)(param+0x3c)) + b + 0x49B40821;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  
  a = ((b & d) | (~d & c)) + *((uint32_t*)(param+0x04)) + a - 0x09E1DA9E;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + *((uint32_t*)(param+0x18)) + d - 0x3FBF4CC0;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + *((uint32_t*)(param+0x2c)) + c + 0x265E5A51;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + *((uint32_t*)(param+0x00)) + b - 0x16493856;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  a = ((b & d) | (~d & c)) + *((uint32_t*)(param+0x14)) + a - 0x29D0EFA3;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + *((uint32_t*)(param+0x28)) + d + 0x02441453;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + *((uint32_t*)(param+0x3c)) + c - 0x275E197F;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + *((uint32_t*)(param+0x10)) + b - 0x182C0438;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  a = ((b & d) | (~d & c)) + *((uint32_t*)(param+0x24)) + a + 0x21E1CDE6;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + *((uint32_t*)(param+0x38)) + d - 0x3CC8F82A;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + *((uint32_t*)(param+0x0c)) + c - 0x0B2AF279;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + *((uint32_t*)(param+0x20)) + b + 0x455A14ED;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  a = ((b & d) | (~d & c)) + *((uint32_t*)(param+0x34)) + a - 0x561C16FB;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + *((uint32_t*)(param+0x08)) + d - 0x03105C08;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + *((uint32_t*)(param+0x1c)) + c + 0x676F02D9;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + *((uint32_t*)(param+0x30)) + b - 0x72D5B376;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  
  a = (b ^ c ^ d) + *((uint32_t*)(param+0x14)) + a - 0x0005C6BE;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + *((uint32_t*)(param+0x20)) + d - 0x788E097F;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + *((uint32_t*)(param+0x2c)) + c + 0x6D9D6122;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + *((uint32_t*)(param+0x38)) + b - 0x021AC7F4;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  a = (b ^ c ^ d) + *((uint32_t*)(param+0x04)) + a - 0x5B4115BC;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + *((uint32_t*)(param+0x10)) + d + 0x4BDECFA9;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + *((uint32_t*)(param+0x1c)) + c - 0x0944B4A0;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + *((uint32_t*)(param+0x28)) + b - 0x41404390;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  a = (b ^ c ^ d) + *((uint32_t*)(param+0x34)) + a + 0x289B7EC6;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + *((uint32_t*)(param+0x00)) + d - 0x155ED806;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + *((uint32_t*)(param+0x0c)) + c - 0x2B10CF7B;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + *((uint32_t*)(param+0x18)) + b + 0x04881D05;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  a = (b ^ c ^ d) + *((uint32_t*)(param+0x24)) + a - 0x262B2FC7;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + *((uint32_t*)(param+0x30)) + d - 0x1924661B;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + *((uint32_t*)(param+0x3c)) + c + 0x1fa27cf8;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + *((uint32_t*)(param+0x08)) + b - 0x3B53A99B;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  
  a = ((~d | b) ^ c)  + *((uint32_t*)(param+0x00)) + a - 0x0BD6DDBC;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + *((uint32_t*)(param+0x1c)) + d + 0x432AFF97;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + *((uint32_t*)(param+0x38)) + c - 0x546BDC59;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + *((uint32_t*)(param+0x14)) + b - 0x036C5FC7;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 
  a = ((~d | b) ^ c)  + *((uint32_t*)(param+0x30)) + a + 0x655B59C3;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + *((uint32_t*)(param+0x0C)) + d - 0x70F3336E;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + *((uint32_t*)(param+0x28)) + c - 0x00100B83;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + *((uint32_t*)(param+0x04)) + b - 0x7A7BA22F;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 
  a = ((~d | b) ^ c)  + *((uint32_t*)(param+0x20)) + a + 0x6FA87E4F;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + *((uint32_t*)(param+0x3c)) + d - 0x01D31920;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + *((uint32_t*)(param+0x18)) + c - 0x5CFEBCEC;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + *((uint32_t*)(param+0x34)) + b + 0x4E0811A1;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 
  a = ((~d | b) ^ c)  + *((uint32_t*)(param+0x10)) + a - 0x08AC817E;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + *((uint32_t*)(param+0x2c)) + d - 0x42C50DCB;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + *((uint32_t*)(param+0x08)) + c + 0x2AD7D2BB;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + *((uint32_t*)(param+0x24)) + b - 0x14792C6F;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 

#ifdef LOG
  printf("real: hash output: %x %x %x %x\n", a, b, c, d);
#endif
  
  *((uint32_t *)(field+0)) += a;
  *((uint32_t *)(field+4)) += b;
  *((uint32_t *)(field+8)) += c;
  *((uint32_t *)(field+12)) += d;
}

static void call_hash (char *key, char *challenge, int len) {

  uint32_t *ptr1, *ptr2;
  uint32_t a, b, c, d;
  
  ptr1=(uint32_t*)(key+16);
  ptr2=(uint32_t*)(key+20);
  
  a = *ptr1;
  b = (a >> 3) & 0x3f;
  a += len * 8;
  *ptr1 = a;
  
  if (a < (len << 3))
  {
#ifdef LOG
    printf("not verified: (len << 3) > a true\n");
#endif
    ptr2 += 4;
  }

  *ptr2 += (len >> 0x1d);
  a = 64 - b;
  c = 0;  
  if (a <= len)
  {

    memcpy(key+b+24, challenge, a);
    hash(key, key+24);
    c = a;
    d = c + 0x3f;
    
    while ( d < len ) {

#ifdef LOG
      printf("not verified:  while ( d < len )\n");
#endif
      hash(key, challenge+d-0x3f);
      d += 64;
      c += 64;
    }
    b = 0;
  }
  
  memcpy(key+b+24, challenge+c, len-c);
}

static void calc_response (char *result, char *field) {

  char buf1[128];
  char buf2[128];
  int i;

  memset (buf1, 0, 64);
  *buf1 = 128;
  
  memcpy (buf2, field+16, 8);
  
  i = ( *((uint32_t*)(buf2)) >> 3 ) & 0x3f;
 
  if (i < 56) {
    i = 56 - i;
  } else {
#ifdef LOG
    printf("not verified: ! (i < 56)\n");
#endif
    i = 120 - i;
  }

  call_hash (field, buf1, i);
  call_hash (field, buf2, 8);

  memcpy (result, field, 16);

}


static void calc_response_string (char *result, char *challenge) {
 
  char field[128];
  char zres[20];
  int  i;
      
  /* initialize our field */
  BE_32C (field,      0x01234567);
  BE_32C ((field+4),  0x89ABCDEF);
  BE_32C ((field+8),  0xFEDCBA98);
  BE_32C ((field+12), 0x76543210);
  BE_32C ((field+16), 0x00000000);
  BE_32C ((field+20), 0x00000000);

  /* calculate response */
  call_hash(field, challenge, 64);
  calc_response(zres,field);
 
  /* convert zres to ascii string */
  for (i=0; i<16; i++ ) {
    char a, b;
    
    a = (zres[i] >> 4) & 15;
    b = zres[i] & 15;

    result[i*2]   = ((a<10) ? (a+48) : (a+87)) & 255;
    result[i*2+1] = ((b<10) ? (b+48) : (b+87)) & 255;
  }
}

void real_calc_response_and_checksum (char *response, char *chksum, char *challenge) {

  int   ch_len, table_len, resp_len;
  int   i;
  char *ptr;
  char  buf[128];

  /* initialize return values */
  memset(response, 0, 64);
  memset(chksum, 0, 34);

  /* initialize buffer */
  memset(buf, 0, 128);
  ptr=buf;
  BE_32C(ptr, 0xa1e9149d);
  ptr+=4;
  BE_32C(ptr, 0x0e6b3b59);
  ptr+=4;

  /* some (length) checks */
  if (challenge != NULL)
  {
    ch_len = strlen (challenge);

    if (ch_len == 40) /* what a hack... */
    {
      challenge[32]=0;
      ch_len=32;
    }
    if ( ch_len > 56 ) ch_len=56;
    
    /* copy challenge to buf */
    memcpy(ptr, challenge, ch_len);
  }
  
  if (xor_table != NULL)
  {
    table_len = strlen(xor_table);

    if (table_len > 56) table_len=56;

    /* xor challenge bytewise with xor_table */
    for (i=0; i<table_len; i++)
      ptr[i] = ptr[i] ^ xor_table[i];
  }

  calc_response_string (response, buf);

  /* add tail */
  resp_len = strlen (response);
  strcpy (&response[resp_len], "01d0a8e3");

  /* calculate checksum */
  for (i=0; i<resp_len/4; i++)
    chksum[i] = response[i*4];
}


/*
 * takes a MLTI-Chunk and a rule number got from match_asm_rule,
 * returns a pointer to selected data and number of bytes in that.
 */

static int select_mlti_data(const char *mlti_chunk, int mlti_size, int selection, char *out) {

  int numrules, codec, size;
  int i;
  
  /* MLTI chunk should begin with MLTI */

  if ((mlti_chunk[0] != 'M')
      ||(mlti_chunk[1] != 'L')
      ||(mlti_chunk[2] != 'T')
      ||(mlti_chunk[3] != 'I'))
  {
#ifdef LOG
    printf("libreal: MLTI tag not detected, copying data\n");
#endif
    memcpy(out, mlti_chunk, mlti_size);
    return mlti_size;
  }

  mlti_chunk+=4;

  /* next 16 bits are the number of rules */
  numrules=BE_16(mlti_chunk);
  if (selection >= numrules) return 0;

  /* now <numrules> indices of codecs follows */
  /* we skip to selection                     */
  mlti_chunk+=(selection+1)*2;

  /* get our index */
  codec=BE_16(mlti_chunk);

  /* skip to number of codecs */
  mlti_chunk+=(numrules-selection)*2;

  /* get number of codecs */
  numrules=BE_16(mlti_chunk);

  if (codec >= numrules) {
    printf("codec index >= number of codecs. %i %i\n", codec, numrules);
    return 0;
  }

  mlti_chunk+=2;
 
  /* now seek to selected codec */
  for (i=0; i<codec; i++) {
    size=BE_32(mlti_chunk);
    mlti_chunk+=size+4;
  }
  
  size=BE_32(mlti_chunk);

#ifdef LOG
  hexdump(mlti_chunk+4, size);
#endif
  memcpy(out,mlti_chunk+4, size);
  return size;
}

/*
 * looking at stream description.
 */

rmff_header_t *real_parse_sdp(char *data, char *stream_rules, uint32_t bandwidth) {

  sdpplin_t *desc;
  rmff_header_t *header;
  char buf[2048];
  int len, i;
  int max_bit_rate=0;
  int avg_bit_rate=0;
  int max_packet_size=0;
  int avg_packet_size=0;
  int duration=0;
  

  if (!data) return NULL;

  desc=sdpplin_parse(data);

  if (!desc) return NULL;
  
  header=calloc(1,sizeof(rmff_header_t));

  header->fileheader=rmff_new_fileheader(4+desc->stream_count);
  header->cont=rmff_new_cont(
      desc->title,
      desc->author,
      desc->copyright,
      desc->abstract);
  header->data=rmff_new_dataheader(0,0);
  header->streams=calloc(1,sizeof(rmff_mdpr_t*)*(desc->stream_count+1));
#ifdef LOG
    printf("number of streams: %u\n", desc->stream_count);
#endif

  for (i=0; i<desc->stream_count; i++) {

    int j=0;
    int n;
    char b[64];
    int rulematches[16];

#ifdef LOG
    printf("calling asmrp_match with:\n%s\n%u\n", desc->stream[i]->asm_rule_book, bandwidth);
#endif
    n=asmrp_match(desc->stream[i]->asm_rule_book, bandwidth, rulematches);
    for (j=0; j<n; j++) {
#ifdef LOG
      printf("asmrp rule match: %u for stream %u\n", rulematches[j], desc->stream[i]->stream_id);
#endif
      sprintf(b,"stream=%u;rule=%u,", desc->stream[i]->stream_id, rulematches[j]);
      strcat(stream_rules, b);
    }

    if (!desc->stream[i]->mlti_data) return NULL;

    len=select_mlti_data(desc->stream[i]->mlti_data, desc->stream[i]->mlti_data_size, rulematches[0], buf);
    
    header->streams[i]=rmff_new_mdpr(
	desc->stream[i]->stream_id,
        desc->stream[i]->max_bit_rate,
        desc->stream[i]->avg_bit_rate,
        desc->stream[i]->max_packet_size,
        desc->stream[i]->avg_packet_size,
        desc->stream[i]->start_time,
        desc->stream[i]->preroll,
        desc->stream[i]->duration,
        desc->stream[i]->stream_name,
        desc->stream[i]->mime_type,
	len,
	buf);

    duration=MAX(duration,desc->stream[i]->duration);
    max_bit_rate+=desc->stream[i]->max_bit_rate;
    avg_bit_rate+=desc->stream[i]->avg_bit_rate;
    max_packet_size=MAX(max_packet_size, desc->stream[i]->max_packet_size);
    if (avg_packet_size)
      avg_packet_size=(avg_packet_size + desc->stream[i]->avg_packet_size) / 2;
    else
      avg_packet_size=desc->stream[i]->avg_packet_size;
  }
  
  if (stream_rules)
    stream_rules[strlen(stream_rules)-1]=0; /* delete last ',' in stream_rules */

  header->prop=rmff_new_prop(
      max_bit_rate,
      avg_bit_rate,
      max_packet_size,
      avg_packet_size,
      0,
      duration,
      0,
      0,
      0,
      desc->stream_count,
      desc->flags);

  rmff_fix_header(header);

  return header;
}

int real_get_rdt_chunk(rtsp_t *rtsp_session, char *buffer) {

  int n=1;
  uint8_t header[8];
  rmff_pheader_t ph;
  int size;
  int flags1;
  int unknown1;
  uint32_t ts;

  n=rtsp_read_data(rtsp_session, header, 8);
  if (n<8) return 0;
  if (header[0] != 0x24)
  {
    printf("rdt chunk not recognized: got 0x%02x\n", header[0]);
    return 0;
  }
  size=(header[1]<<12)+(header[2]<<8)+(header[3]);
  flags1=header[4];
  if ((flags1!=0x40)&&(flags1!=0x42))
  {
#ifdef LOG
    printf("got flags1: 0x%02x\n",flags1);
#endif
    header[0]=header[5];
    header[1]=header[6];
    header[2]=header[7];
    n=rtsp_read_data(rtsp_session, header+3, 5);
    if (n<5) return 0;
#ifdef LOG
    printf("ignoring bytes:\n");
    hexdump(header, 8);
#endif
    n=rtsp_read_data(rtsp_session, header+4, 4);
    if (n<4) return 0;
    flags1=header[4];
    size-=9;
  }
  unknown1=(header[5]<<12)+(header[6]<<8)+(header[7]);
  n=rtsp_read_data(rtsp_session, header, 6);
  if (n<6) return 0;
  ts=BE_32(header);
  
#ifdef LOG
  printf("ts: %u size: %u, flags: 0x%02x, unknown values: %u 0x%02x 0x%02x\n", 
          ts, size, flags1, unknown1, header[4], header[5]);
#endif
  size+=2;
  
  ph.object_version=0;
  ph.length=size;
  ph.stream_number=(flags1>>1)&1;
  ph.timestamp=ts;
  ph.reserved=0;
  ph.flags=0;      /* TODO: determine keyframe flag and insert here? */
  rmff_dump_pheader(&ph, buffer);
  size-=12;
  n=rtsp_read_data(rtsp_session, buffer+12, size);
  
  return n+12;
}

rmff_header_t  *real_setup_and_get_header(rtsp_t *rtsp_session, uint32_t bandwidth) {

  char *description=NULL;
  char *session_id=NULL;
  rmff_header_t *h;
  char *challenge1;
  char challenge2[64];
  char checksum[34];
  char subscribe[256];
  char buf[256];
  char *mrl=rtsp_get_mrl(rtsp_session);
  unsigned int size;
  int status;
  
  /* get challenge */
  challenge1=strdup(rtsp_search_answers(rtsp_session,"RealChallenge1"));
#ifdef LOG
  printf("real: Challenge1: %s\n", challenge1);
#endif
  
  /* request stream description */
  rtsp_schedule_field(rtsp_session, "Accept: application/sdp");
  sprintf(buf, "Bandwidth: %u", bandwidth);
  rtsp_schedule_field(rtsp_session, buf);
  rtsp_schedule_field(rtsp_session, "GUID: 00000000-0000-0000-0000-000000000000");
  rtsp_schedule_field(rtsp_session, "RegionData: 0");
  rtsp_schedule_field(rtsp_session, "ClientID: Linux_2.4_6.0.9.1235_play32_RN01_EN_586");
  rtsp_schedule_field(rtsp_session, "SupportsMaximumASMBandwidth: 1");
  rtsp_schedule_field(rtsp_session, "Language: en-US");
  rtsp_schedule_field(rtsp_session, "Require: com.real.retain-entity-for-setup");
  status=rtsp_request_describe(rtsp_session,NULL);

  if ( status<200 || status>299 )
  {
    char *alert=rtsp_search_answers(rtsp_session,"Alert");
    if (alert) {
      printf("real: got message from server:\n%s\n", alert);
    }
    rtsp_send_ok(rtsp_session);
    return NULL;
  }

  /* receive description */
  size=0;
  if (!rtsp_search_answers(rtsp_session,"Content-length"))
    printf("real: got no Content-length!\n");
  else
    size=atoi(rtsp_search_answers(rtsp_session,"Content-length"));

  if (!rtsp_search_answers(rtsp_session,"ETag"))
    printf("real: got no ETag!\n");
  else
    session_id=strdup(rtsp_search_answers(rtsp_session,"ETag"));
    
#ifdef LOG
  printf("real: Stream description size: %i\n", size);
#endif

  description=malloc(sizeof(char)*(size+1));

  rtsp_read_data(rtsp_session, description, size);
  description[size]=0;

  /* parse sdp (sdpplin) and create a header and a subscribe string */
  strcpy(subscribe, "Subscribe: ");
  h=real_parse_sdp(description, subscribe+11, bandwidth);
  if (!h) return NULL;
  rmff_fix_header(h);

#ifdef LOG
  printf("Title: %s\nCopyright: %s\nAuthor: %s\nStreams: %i\n",
    h->cont->title, h->cont->copyright, h->cont->author, h->prop->num_streams);
#endif
  
  /* setup our streams */
  real_calc_response_and_checksum (challenge2, checksum, challenge1);
  sprintf(buf, "RealChallenge2: %s, sd=%s", challenge2, checksum);
  rtsp_schedule_field(rtsp_session, buf);
  sprintf(buf, "If-Match: %s", session_id);
  rtsp_schedule_field(rtsp_session, buf);
  rtsp_schedule_field(rtsp_session, "Transport: x-pn-tng/tcp;mode=play,rtp/avp/tcp;unicast;mode=play");
  sprintf(buf, "%s/streamid=0", mrl);
  rtsp_request_setup(rtsp_session,buf);

  if (h->prop->num_streams > 1) {
    rtsp_schedule_field(rtsp_session, "Transport: x-pn-tng/tcp;mode=play,rtp/avp/tcp;unicast;mode=play");
    sprintf(buf, "If-Match: %s", session_id);
    rtsp_schedule_field(rtsp_session, buf);

    sprintf(buf, "%s/streamid=1", mrl);
    rtsp_request_setup(rtsp_session,buf);
  }
  /* set stream parameter (bandwidth) with our subscribe string */
  rtsp_schedule_field(rtsp_session, subscribe);
  rtsp_request_setparameter(rtsp_session,NULL);

  /* and finally send a play request */
  rtsp_schedule_field(rtsp_session, "Range: npt=0-");
  rtsp_request_play(rtsp_session,NULL);

  return h;
}

