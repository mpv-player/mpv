
// Reference: DOCS/tech/hwac3.txt !!!!!

/* DTS code based on "ac3/decode_dts.c" and "ac3/conversion.c" from "ogle 0.9"
   (see http://www.dtek.chalmers.se/~dvd/)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#ifdef USE_LIBA52

#include "mp_msg.h"
#include "help_mp.h"

#include "ad_internal.h"

#include "liba52/a52.h"


static int isdts = -1;

static ad_info_t info = 
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
    // DTS has a 10 byte header
    while(sh_audio->a_in_buffer_len < 10)
    {
      int c = demux_getc(sh_audio->ds);
      if(c<0)
        return -1; /* EOF*/
      sh_audio->a_in_buffer[sh_audio->a_in_buffer_len++] = c;
    }

    length = dts_syncinfo(sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
    if(length >= 10)
    {
      if(isdts != 1)
      {
        mp_msg(MSGT_DECAUDIO, MSGL_STATUS, "hwac3: switched to DTS, %d bps, %d Hz\n", bit_rate, sample_rate);
        isdts = 1;
      }
      break;
    }
    length = a52_syncinfo(sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
    if(length >= 7 && length <= 3840) 
    {
      if(isdts != 0)
      {
        mp_msg(MSGT_DECAUDIO, MSGL_STATUS, "hwac3: switched to AC3, %d bps, %d Hz\n", bit_rate, sample_rate);
        isdts = 0;
      }
      break; /* we're done.*/
    }
    /* bad file => resync*/
    memcpy(sh_audio->a_in_buffer, sh_audio->a_in_buffer + 1, 9);
    --sh_audio->a_in_buffer_len;
  }
  mp_msg(MSGT_DECAUDIO, MSGL_DBG2, "ac3dts: %s len=%d  flags=0x%X  %d Hz %d bit/s\n", isdts == 1 ? "DTS" : isdts == 0 ? "AC3" : "unknown", length, flags, sample_rate, bit_rate);

  sh_audio->samplerate = sample_rate;
  sh_audio->i_bps = bit_rate / 8;
  demux_read_data(sh_audio->ds, sh_audio->a_in_buffer + 10, length - 10);
  sh_audio->a_in_buffer_len = length;
    
  // TODO: is DTS also checksummed?
  if(isdts == 0 && crc16_block(sh_audio->a_in_buffer + 2, length - 2) != 0)
    mp_msg(MSGT_DECAUDIO, MSGL_STATUS, "a52: CRC check failed!  \n");
    
  return length;
}


static int preinit(sh_audio_t *sh)
{
  /* Dolby AC3 audio: */
  sh->audio_out_minsize = 128 * 32 * 2 * 2; // DTS seems to need more than AC3
  sh->audio_in_minsize = 8192;
  sh->channels = 2;
  sh->samplesize = 2;
  sh->sample_format = AF_FORMAT_AC3;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
  /* Dolby AC3 passthrough:*/
  a52_state_t *a52_state = a52_init(0);
  if(a52_state == NULL)
  {
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "A52 init failed\n");
    return 0;
  }
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
    buf[0] = 0x72;
    buf[1] = 0xF8;
    buf[2] = 0x1F;
    buf[3] = 0x4E;
    buf[4] = 0x01; //(length) ? data_type : 0; /* & 0x1F; */
    buf[5] = 0x00;
    buf[6] = (len << 3) & 0xFF;
    buf[7] = (len >> 5) & 0xFF;
#ifdef WORDS_BIGENDIAN
    memcpy(buf + 8, sh_audio->a_in_buffer, len);  // untested
#else
    swab(sh_audio->a_in_buffer, buf + 8, len);
#endif
    memset(buf + 8 + len, 0, 6144 - 8 - len);

    return 6144;
  }
  else
    return -1;
}


static int DTS_SAMPLEFREQS[16] =
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

static int DTS_BITRATES[30] =
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

  if(((indata_ptr[0] << 24) | (indata_ptr[1] << 16) | (indata_ptr[2] << 8)
    | (indata_ptr[3])) != 0x7ffe8001)
    return -1;

  ftype = indata_ptr[4] >> 7;

  surp = (indata_ptr[4] >> 2) & 0x1f;
  surp = (surp + 1) % 32;

  unknown_bit = (indata_ptr[4] >> 1) & 0x01;

  *nblks = (indata_ptr[4] & 0x01) << 6 | (indata_ptr[5] >> 2);
  *nblks = *nblks + 1;

  fsize = (indata_ptr[5] & 0x03) << 12 | (indata_ptr[6] << 4) | (indata_ptr[7] >> 4);
  fsize = fsize + 1;
    
  amode = (indata_ptr[7] & 0x0f) << 2 | (indata_ptr[8] >> 6);
  *sfreq = (indata_ptr[8] >> 2) & 0x0f;
  *rate = (indata_ptr[8] & 0x03) << 3 | ((indata_ptr[9] >> 5) & 0x07);
    
  if(ftype != 1) 
  {
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: Termination frames not handled, REPORT BUG\n");
    return -1;
  }
    
  if(*sfreq != 13) 
  {
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: Only 48kHz supported, REPORT BUG\n");
    return -1;
  }
    
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


static int decode_audio_dts(unsigned char *indata_ptr, int len, unsigned char *buf)
{
  int nblks;
  int fsize;
  int rate;
  int sfreq;
  int burst_len;
  int nr_samples;

  fsize = dts_decode_header(indata_ptr, &rate, &nblks, &sfreq);
  if(fsize < 0)
    return -1;
 
  burst_len = fsize * 8;
  nr_samples = nblks * 32;

  buf[0] = 0x72; buf[1] = 0xf8; /* iec 61937     */
  buf[2] = 0x1f; buf[3] = 0x4e; /*  syncword     */
  switch(nr_samples) 
  {
  case 512:
    buf[4] = 0x0b;      /* DTS-1 (512-sample bursts) */
    break;
  case 1024:
    buf[4] = 0x0c;      /* DTS-2 (1024-sample bursts) */
    break;
  case 2048:
    buf[4] = 0x0d;      /* DTS-3 (2048-sample bursts) */
    break;
  default:
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: %d-sample bursts not supported\n", nr_samples);
    buf[4] = 0x00;
    break;
  }
  buf[5] = 0;                      /* ?? */    
  buf[6] = (burst_len) & 0xff;   
  buf[7] = (burst_len >> 8) & 0xff;
 
  if(fsize + 8 > nr_samples * 2 * 2)
  {
    mp_msg(MSGT_DECAUDIO, MSGL_ERR, "DTS: more data than fits\n");
  }
#ifdef WORDS_BIGENDIAN
  memcpy(&buf[8], indata_ptr, fsize);  // untested
#else
  //TODO if fzise is odd, swab doesn't copy the last byte
  swab(indata_ptr, &buf[8], fsize);
  if (fsize & 1)
    buf[8+fsize] = indata_ptr[fsize];
#endif
  memset(&buf[fsize + 8], 0, nr_samples * 2 * 2 - (fsize + 8));

  return nr_samples * 2 * 2;
}
#endif
