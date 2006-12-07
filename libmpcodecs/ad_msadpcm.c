/*
    MS ADPCM Decoder for MPlayer
      by Mike Melanson

    This file is responsible for decoding Microsoft ADPCM data.
    Details about the data format can be found here:
      http://www.pcisys.net/~melanson/codecs/
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"MS ADPCM audio decoder",
	"msadpcm",
	"Nick Kurshev",
	"Mike Melanson",
	""
};

LIBAD_EXTERN(msadpcm)

static int ms_adapt_table[] =
{
  230, 230, 230, 230, 307, 409, 512, 614,
  768, 614, 512, 409, 307, 230, 230, 230
};

static int ms_adapt_coeff1[] =
{
  256, 512, 0, 192, 240, 460, 392
};

static int ms_adapt_coeff2[] =
{
  0, -256, 0, 64, 0, -208, -232
};

#define MS_ADPCM_PREAMBLE_SIZE 6

#define LE_16(x) ((x)[0]+(256*((x)[1])))
//#define LE_16(x) (le2me_16((x)[1]+(256*((x)[0]))))
//#define LE_16(x) (le2me_16(*(unsigned short *)(x)))
//#define LE_32(x) (le2me_32(*(unsigned int *)(x)))

// useful macros
// clamp a number between 0 and 88
#define CLAMP_0_TO_88(x)  if (x < 0) x = 0; else if (x > 88) x = 88;
// clamp a number within a signed 16-bit range
#define CLAMP_S16(x)  if (x < -32768) x = -32768; \
  else if (x > 32767) x = 32767;
// clamp a number above 16
#define CLAMP_ABOVE_16(x)  if (x < 16) x = 16;
// sign extend a 16-bit value
#define SE_16BIT(x)  if (x & 0x8000) x -= 0x10000;
// sign extend a 4-bit value
#define SE_4BIT(x)  if (x & 0x8) x -= 0x10;

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize = sh_audio->wf->nBlockAlign * 4;
  sh_audio->ds->ss_div = 
    (sh_audio->wf->nBlockAlign - MS_ADPCM_PREAMBLE_SIZE) * 2;
  sh_audio->audio_in_minsize =
  sh_audio->ds->ss_mul = sh_audio->wf->nBlockAlign;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps = sh_audio->wf->nBlockAlign *
    (sh_audio->channels*sh_audio->samplerate) / sh_audio->ds->ss_div;
  sh_audio->samplesize=2;

  return 1;
}

static void uninit(sh_audio_t *sh_audio)
{
}

static int control(sh_audio_t *sh_audio,int cmd,void* arg, ...)
{
  if(cmd==ADCTRL_SKIP_FRAME){
    demux_read_data(sh_audio->ds, sh_audio->a_in_buffer,sh_audio->ds->ss_mul);
    return CONTROL_TRUE;
  }
  return CONTROL_UNKNOWN;
}

static int ms_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels, int block_size)
{
  int current_channel = 0;
  int idelta[2];
  int sample1[2];
  int sample2[2];
  int coeff1[2];
  int coeff2[2];
  int stream_ptr = 0;
  int out_ptr = 0;
  int upper_nibble = 1;
  int nibble;
  int snibble;  // signed nibble
  int predictor;

  // fetch the header information, in stereo if both channels are present
  if (input[stream_ptr] > 6)
    mp_msg(MSGT_DECAUDIO, MSGL_WARN,
      "MS ADPCM: coefficient (%d) out of range (should be [0..6])\n",
      input[stream_ptr]);
  coeff1[0] = ms_adapt_coeff1[input[stream_ptr]];
  coeff2[0] = ms_adapt_coeff2[input[stream_ptr]];
  stream_ptr++;
  if (channels == 2)
  {
    if (input[stream_ptr] > 6)
     mp_msg(MSGT_DECAUDIO, MSGL_WARN,
       "MS ADPCM: coefficient (%d) out of range (should be [0..6])\n",
       input[stream_ptr]);
    coeff1[1] = ms_adapt_coeff1[input[stream_ptr]];
    coeff2[1] = ms_adapt_coeff2[input[stream_ptr]];
    stream_ptr++;
  }

  idelta[0] = LE_16(&input[stream_ptr]);
  stream_ptr += 2;
  SE_16BIT(idelta[0]);
  if (channels == 2)
  {
    idelta[1] = LE_16(&input[stream_ptr]);
    stream_ptr += 2;
    SE_16BIT(idelta[1]);
  }

  sample1[0] = LE_16(&input[stream_ptr]);
  stream_ptr += 2;
  SE_16BIT(sample1[0]);
  if (channels == 2)
  {
    sample1[1] = LE_16(&input[stream_ptr]);
    stream_ptr += 2;
    SE_16BIT(sample1[1]);
  }

  sample2[0] = LE_16(&input[stream_ptr]);
  stream_ptr += 2;
  SE_16BIT(sample2[0]);
  if (channels == 2)
  {
    sample2[1] = LE_16(&input[stream_ptr]);
    stream_ptr += 2;
    SE_16BIT(sample2[1]);
  }

  if (channels == 1)
  {
    output[out_ptr++] = sample2[0];
    output[out_ptr++] = sample1[0];
  } else {
    output[out_ptr++] = sample2[0];
    output[out_ptr++] = sample2[1];
    output[out_ptr++] = sample1[0];
    output[out_ptr++] = sample1[1];
  }

  while (stream_ptr < block_size)
  {
    // get the next nibble
    if (upper_nibble)
      nibble = snibble = input[stream_ptr] >> 4;
    else
      nibble = snibble = input[stream_ptr++] & 0x0F;
    upper_nibble ^= 1;
    SE_4BIT(snibble);

    predictor = (
      ((sample1[current_channel] * coeff1[current_channel]) +
       (sample2[current_channel] * coeff2[current_channel])) / 256) +
      (snibble * idelta[current_channel]);
    CLAMP_S16(predictor);
    sample2[current_channel] = sample1[current_channel];
    sample1[current_channel] = predictor;
    output[out_ptr++] = predictor;

    // compute the next adaptive scale factor (a.k.a. the variable idelta)
    idelta[current_channel] =
      (ms_adapt_table[nibble] * idelta[current_channel]) / 256;
    CLAMP_ABOVE_16(idelta[current_channel]);

    // toggle the channel
    current_channel ^= channels - 1;
  }

  return (block_size - (MS_ADPCM_PREAMBLE_SIZE * channels)) * 2;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  if (demux_read_data(sh_audio->ds, sh_audio->a_in_buffer,
    sh_audio->ds->ss_mul) != 
    sh_audio->ds->ss_mul) 
      return -1; /* EOF */

  return 2 * ms_adpcm_decode_block(
    (unsigned short*)buf, sh_audio->a_in_buffer,
    sh_audio->wf->nChannels, sh_audio->wf->nBlockAlign);
}
