/*
 * DTS code based on "ac3/decode_dts.c" and "ac3/conversion.c" from "ogle 0.9"
 * (see http://www.dtek.chalmers.se/~dvd/)
 * Reference: DOCS/tech/hwac3.txt !!!!!
 *
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

#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "mpbswap.h"
#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"

#include "ad_internal.h"


static int isdts = -1;

static const ad_info_t info =
{
  "AC3/DTS pass-through S/PDIF",
  "hwac3",
  "Nick Kurshev/Peter Schüller",
  "???",
  ""
};

LIBAD_EXTERN(hwac3)


static int dts_syncinfo(uint8_t *indata_ptr, int *flags, int *sample_rate, int *bit_rate);
static int decode_audio_dts(unsigned char *indata_ptr, int len, unsigned char *buf);


static int a52_syncinfo (uint8_t *buf, int *sample_rate, int *bit_rate)
{
    static const uint16_t rate[] = { 32,  40,  48,  56,  64,  80,  96, 112,
                                    128, 160, 192, 224, 256, 320, 384, 448,
                                    512, 576, 640};
    int frmsizecod;
    int bitrate;
    int half;

    if (buf[0] != 0x0b || buf[1] != 0x77)    /* syncword */
        return 0;

    if (buf[5] >= 0x60)                      /* bsid >= 12 */
        return 0;
    half = buf[5] >> 3;
    half = FFMAX(half - 8, 0);

    frmsizecod = buf[4] & 63;
    if (frmsizecod >= 38)
        return 0;
    bitrate = rate[frmsizecod >> 1];
    *bit_rate = (bitrate * 1000) >> half;

    switch (buf[4] & 0xc0) {
    case 0:
        *sample_rate = 48000 >> half;
        return 4 * bitrate;
    case 0x40:
        *sample_rate = 44100 >> half;
        return 2 * (320 * bitrate / 147 + (frmsizecod & 1));
    case 0x80:
        *sample_rate = 32000 >> half;
        return 6 * bitrate;
    default:
        return 0;
    }
}

static int ac3dts_fillbuff(sh_audio_t *sh_audio)
{
  int length = 0;
  int flags = 0;
  int sample_rate = 0;
  int bit_rate = 0;

  sh_audio->a_in_buffer_len = 0;
  /* sync frame:*/
  while(1)
  {
    // Original code DTS has a 10 bytes header.
    // Now max 12 bytes for 14 bits DTS header.
    while(sh_audio->a_in_buffer_len < 12)
    {
      int c = demux_getc(sh_audio->ds);
      if(c<0)
        return -1; /* EOF*/
      sh_audio->a_in_buffer[sh_audio->a_in_buffer_len++] = c;
    }

    if (sh_audio->format == 0x2001)
    {
      length = dts_syncinfo(sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
      if(length >= 12)
      {
        if(isdts != 1)
        {
          mp_msg(MSGT_DECAUDIO, MSGL_STATUS, "hwac3: switched to DTS, %d bps, %d Hz\n", bit_rate, sample_rate);
          isdts = 1;
        }
        break;
      }
    }
    else
    {
      length = a52_syncinfo(sh_audio->a_in_buffer, &sample_rate, &bit_rate);
      if(length >= 7 && length <= 3840)
      {
        if(isdts != 0)
        {
          mp_msg(MSGT_DECAUDIO, MSGL_STATUS, "hwac3: switched to AC3, %d bps, %d Hz\n", bit_rate, sample_rate);
          isdts = 0;
        }
        break; /* we're done.*/
      }
    }
    /* bad file => resync*/
    memcpy(sh_audio->a_in_buffer, sh_audio->a_in_buffer + 1, 11);
    --sh_audio->a_in_buffer_len;
  }
  mp_msg(MSGT_DECAUDIO, MSGL_DBG2, "ac3dts: %s len=%d  flags=0x%X  %d Hz %d bit/s\n", isdts == 1 ? "DTS" : isdts == 0 ? "AC3" : "unknown", length, flags, sample_rate, bit_rate);

  sh_audio->samplerate = sample_rate;
  sh_audio->i_bps = bit_rate / 8;
  demux_read_data(sh_audio->ds, sh_audio->a_in_buffer + 12, length - 12);
  sh_audio->a_in_buffer_len = length;

  return length;
}


static int preinit(sh_audio_t *sh)
{
  /* Dolby AC3 audio: */
  sh->audio_out_minsize = 128 * 32 * 2 * 2; // DTS seems to need more than AC3
  sh->audio_in_minsize = 8192;
  sh->channels = 2;
  sh->samplesize = 2;
  sh->sample_format = AF_FORMAT_AC3_BE;
  // HACK for DTS where useless swapping can't easily be removed
  if (sh->format == 0x2001)
    sh->sample_format = AF_FORMAT_AC3_NE;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
  /* Dolby AC3 passthrough:*/
  if(ac3dts_fillbuff(sh_audio) < 0)
  {
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "AC3/DTS sync failed\n");
    return 0;
  }
  return 1;
}

static void uninit(sh_audio_t *sh)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
  switch(cmd)
  {
  case ADCTRL_RESYNC_STREAM:
  case ADCTRL_SKIP_FRAME:
      ac3dts_fillbuff(sh);
      return CONTROL_TRUE;
  }
  return CONTROL_UNKNOWN;
}


static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  int len = sh_audio->a_in_buffer_len;

  if(len <= 0)
    if((len = ac3dts_fillbuff(sh_audio)) <= 0)
      return len; /*EOF*/
  sh_audio->a_in_buffer_len = 0;

  if(isdts == 1)
  {
    return decode_audio_dts(sh_audio->a_in_buffer, len, buf);
  }
  else if(isdts == 0)
  {
    AV_WB16(buf,     0xF872);   // iec 61937 syncword 1
    AV_WB16(buf + 2, 0x4E1F);   // iec 61937 syncword 2
    buf[4] = sh_audio->a_in_buffer[5] & 0x7; // bsmod
    buf[5] = 0x01;              // data-type ac3
    AV_WB16(buf + 6, len << 3); // number of bits in payload
    memcpy(buf + 8, sh_audio->a_in_buffer, len);
    memset(buf + 8 + len, 0, 6144 - 8 - len);

    return 6144;
  }
  else
    return -1;
}


static const int DTS_SAMPLEFREQS[16] =
{
  0,
  8000,
  16000,
  32000,
  64000,
  128000,
  11025,
  22050,
  44100,
  88200,
  176400,
  12000,
  24000,
  48000,
  96000,
  192000
};

static const int DTS_BITRATES[30] =
{
  32000,
  56000,
  64000,
  96000,
  112000,
  128000,
  192000,
  224000,
  256000,
  320000,
  384000,
  448000,
  512000,
  576000,
  640000,
  768000,
  896000,
  1024000,
  1152000,
  1280000,
  1344000,
  1408000,
  1411200,
  1472000,
  1536000,
  1920000,
  2048000,
  3072000,
  3840000,
  4096000
};

static int dts_decode_header(uint8_t *indata_ptr, int *rate, int *nblks, int *sfreq)
{
  int ftype;
  int surp;
  int unknown_bit;
  int fsize;
  int amode;

  int word_mode;
  int le_mode;

  unsigned int first4bytes = indata_ptr[0] << 24 | indata_ptr[1] << 16
                             | indata_ptr[2] << 8 | indata_ptr[3];

  switch(first4bytes)
  {
    /* 14 bits LE */
    case 0xff1f00e8:
      /* Also make sure frame type is 1. */
      if ((indata_ptr[4]&0xf0) != 0xf0 || indata_ptr[5] != 0x07)
        return -1;
      word_mode = 0;
      le_mode = 1;
      break;
    /* 14 bits BE */
    case 0x1fffe800:
      /* Also make sure frame type is 1. */
      if (indata_ptr[4] != 0x07 || (indata_ptr[5]&0xf0) != 0xf0)
        return -1;
      word_mode = 0;
      le_mode = 0;
      break;
    /* 16 bits LE */
    case 0xfe7f0180:
      word_mode = 1;
      le_mode = 1;
      break;
    /* 16 bits BE */
    case 0x7ffe8001:
      word_mode = 1;
      le_mode = 0;
      break;
    default:
      return -1;
  }

  if(word_mode)
  {
    /* First bit after first 32 bits:
       Frame type ( 1: Normal frame; 0: Termination frame ) */
    ftype = indata_ptr[4+le_mode] >> 7;

  if(ftype != 1)
  {
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: Termination frames not handled, REPORT BUG\n");
    return -1;
  }
    /* Next 5 bits: Surplus Sample Count V SURP 5 bits */
    surp = indata_ptr[4+le_mode] >> 2 & 0x1f;
    /* Number of surplus samples */
    surp = (surp + 1) % 32;

    /* One unknown bit, crc? */
    unknown_bit = indata_ptr[4+le_mode] >> 1 & 0x01;

    /* NBLKS 7 bits: Valid Range=5-127, Invalid Range=0-4 */
    *nblks = (indata_ptr[4+le_mode] & 0x01) << 6 | indata_ptr[5-le_mode] >> 2;
    /* NBLKS+1 indicates the number of 32 sample PCM audio blocks per channel
       encoded in the current frame per channel. */
    ++(*nblks);

    /* Frame Byte Size V FSIZE 14 bits: 0-94=Invalid, 95-8191=Valid range-1
       (ie. 96 bytes to 8192 bytes), 8192-16383=Invalid
       FSIZE defines the byte size of the current audio frame. */
    fsize = (indata_ptr[5-le_mode] & 0x03) << 12 | indata_ptr[6+le_mode] << 4
            | indata_ptr[7-le_mode] >> 4;
    ++fsize;

    /* Audio Channel Arrangement ACC AMODE 6 bits */
    amode = (indata_ptr[7-le_mode] & 0x0f) << 2 | indata_ptr[8+le_mode] >> 6;

    /* Source Sampling rate ACC SFREQ 4 bits */
    *sfreq = indata_ptr[8+le_mode] >> 2 & 0x0f;
    /* Transmission Bit Rate ACC RATE 5 bits */
    *rate = (indata_ptr[8+le_mode] & 0x03) << 3
            | (indata_ptr[9-le_mode] >> 5 & 0x07);
  }
  else
  {
    /* in the case judgement, we assure this */
    ftype = 1;
    surp = 0;
    /* 14 bits support, every 2 bytes, & 0x3fff, got used 14 bits */
    /* Bits usage:
       32 bits: Sync code (28 + 4)      1th and 2th word, 4 bits in 3th word
       1  bits: Frame type              1 bits in 3th word
       5  bits: SURP                    5 bits in 3th word
       1  bits: crc?                    1 bits in 3th word
       7  bits: NBLKS                   3 bits in 3th word, 4 bits in 4th word
       14 bits: FSIZE                   10 bits in 4th word, 4 bits in 5th word
                                        in 14 bits mode, FSIZE = FSIZE*8/14*2
       6  bits: AMODE                   6 bits in 5th word
       4  bits: SFREQ                   4 bits in 5th word
       5  bits: RATE                    5 bits in 6th word
       total bits: 75 bits    */

    /* NBLKS 7 bits: Valid Range=5-127, Invalid Range=0-4 */
    *nblks = (indata_ptr[5-le_mode] & 0x07) << 4
             | (indata_ptr[6+le_mode] & 0x3f) >> 2;
    /* NBLKS+1 indicates the number of 32 sample PCM audio blocks per channel
       encoded in the current frame per channel. */
    ++(*nblks);

    /* Frame Byte Size V FSIZE 14 bits: 0-94=Invalid, 95-8191=Valid range-1
       (ie. 96 bytes to 8192 bytes), 8192-16383=Invalid
       FSIZE defines the byte size of the current audio frame. */
    fsize = (indata_ptr[6+le_mode] & 0x03) << 12 | indata_ptr[7-le_mode] << 4
            | (indata_ptr[8+le_mode] & 0x3f) >> 2;
    ++fsize;
    fsize = fsize * 8 / 14 * 2;

    /* Audio Channel Arrangement ACC AMODE 6 bits */
    amode = (indata_ptr[8+le_mode] & 0x03) << 4
            | (indata_ptr[9-le_mode] & 0xf0) >> 4;

    /* Source Sampling rate ACC SFREQ 4 bits */
    *sfreq = indata_ptr[9-le_mode] & 0x0f;
    /* Transmission Bit Rate ACC RATE 5 bits */
    *rate = (indata_ptr[10+le_mode] & 0x3f) >> 1;
  }
#if 0
  if(*sfreq != 13)
  {
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: Only 48kHz supported, REPORT BUG\n");
    return -1;
  }
#endif
  if((fsize > 8192) || (fsize < 96))
  {
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: fsize: %d invalid, REPORT BUG\n", fsize);
    return -1;
  }

  if(*nblks != 8 &&
    *nblks != 16 &&
    *nblks != 32 &&
    *nblks != 64 &&
    *nblks != 128 &&
    ftype == 1)
  {
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: nblks %d not valid for normal frame, REPORT BUG\n", *nblks);
    return -1;
  }

  return fsize;
}

static int dts_syncinfo(uint8_t *indata_ptr, int *flags, int *sample_rate, int *bit_rate)
{
  int nblks;
  int fsize;
  int rate;
  int sfreq;

  fsize = dts_decode_header(indata_ptr, &rate, &nblks, &sfreq);
  if(fsize >= 0)
  {
    if(rate >= 0 && rate <= 29)
      *bit_rate = DTS_BITRATES[rate];
    else
      *bit_rate = 0;
    if(sfreq >= 1 && sfreq <= 15)
      *sample_rate = DTS_SAMPLEFREQS[sfreq];
    else
      *sample_rate = 0;
  }
  return fsize;
}

static int convert_14bits_to_16bits(const unsigned char *src,
                                    unsigned char *dest,
                                    int len,
                                    int is_le)
{
  uint16_t *p = (uint16_t *)dest;
  uint16_t buf = 0;
  int spacebits = 16;
  if (len <= 0) return 0;
  while (len > 0) {
    uint16_t v;
    if (len == 1)
      v = is_le ? src[0] : src[0] << 8;
    else
      v = is_le ? src[1] << 8 | src[0] : src[0] << 8 | src[1];
    v <<= 2;
    src += 2;
    len -= 2;
    buf |= v >> (16 - spacebits);
    spacebits -= 14;
    if (spacebits < 0) {
      *p++ = buf;
      spacebits += 16;
      buf = v << (spacebits - 2);
    }
  }
  *p++ = buf;
  return (unsigned char *)p - dest;
}

static int decode_audio_dts(unsigned char *indata_ptr, int len, unsigned char *buf)
{
  int nblks;
  int fsize;
  int rate;
  int sfreq;
  int nr_samples;
  int convert_16bits = 0;
  uint16_t *buf16 = (uint16_t *)buf;

  fsize = dts_decode_header(indata_ptr, &rate, &nblks, &sfreq);
  if(fsize < 0)
    return -1;
  nr_samples = nblks * 32;

  buf16[0] = 0xf872; /* iec 61937     */
  buf16[1] = 0x4e1f; /*  syncword     */
  switch(nr_samples)
  {
  case 512:
    buf16[2] = 0x000b;      /* DTS-1 (512-sample bursts) */
    break;
  case 1024:
    buf16[2] = 0x000c;      /* DTS-2 (1024-sample bursts) */
    break;
  case 2048:
    buf16[2] = 0x000d;      /* DTS-3 (2048-sample bursts) */
    break;
  default:
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: %d-sample bursts not supported\n", nr_samples);
    buf16[2] = 0x0000;
    break;
  }

  if(fsize + 8 > nr_samples * 2 * 2)
  {
    // dts wav (14bits LE) match this condition, one way to passthrough
    // is not add iec 61937 header, decoders will notice the dts header
    // and identify the dts stream. Another way here is convert
    // the stream from 14 bits to 16 bits.
    if ((indata_ptr[0] == 0xff || indata_ptr[0] == 0x1f)
        && fsize * 14 / 16 + 8 <= nr_samples * 2 * 2) {
      // The input stream is 14 bits, we can shrink it to 16 bits
      // to save space for add the 61937 header
      fsize = convert_14bits_to_16bits(indata_ptr,
                                       &buf[8],
                                       fsize,
                                       indata_ptr[0] == 0xff /* is LE */
                                       );
      mp_msg(MSGT_DECAUDIO, MSGL_DBG3, "DTS: shrink 14 bits stream to "
             "16 bits %02x%02x%02x%02x => %02x%02x%02x%02x, new size %d.\n",
             indata_ptr[0], indata_ptr[1], indata_ptr[2], indata_ptr[3],
             buf[8], buf[9], buf[10], buf[11], fsize);
      convert_16bits = 1;
    }
    else
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: more data than fits\n");
  }

  buf16[3] = fsize << 3;

  if (!convert_16bits) {
#if HAVE_BIGENDIAN
  /* BE stream */
  if (indata_ptr[0] == 0x1f || indata_ptr[0] == 0x7f)
#else
  /* LE stream */
  if (indata_ptr[0] == 0xff || indata_ptr[0] == 0xfe)
#endif
  memcpy(&buf[8], indata_ptr, fsize);
  else
  {
  swab(indata_ptr, &buf[8], fsize);
  if (fsize & 1) {
    buf[8+fsize-1] = 0;
    buf[8+fsize] = indata_ptr[fsize-1];
    fsize++;
  }
  }
  }
  memset(&buf[fsize + 8], 0, nr_samples * 2 * 2 - (fsize + 8));

  return nr_samples * 2 * 2;
}
