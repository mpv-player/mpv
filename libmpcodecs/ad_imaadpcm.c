/*
    IMA ADPCM Decoder for MPlayer
      by Mike Melanson

    This file is in charge of decoding all of the various IMA ADPCM data
    formats that various entities have created. Details about the data
    formats can be found here:
      http://www.pcisys.net/~melanson/codecs/

    So far, this file handles these formats:
      'ima4': IMA ADPCM found in QT files
        0x11: IMA ADPCM found in MS AVI/ASF/WAV files
        0x61: DK4 ADPCM found in certain AVI files on Sega Saturn CD-ROMs;
              note that this is a 'rogue' format number in that it was
              never officially registered with Microsoft
    0x1100736d: IMA ADPCM coded like in MS AVI/ASF/WAV found in QT files
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "ad_internal.h"

#define MS_IMA_ADPCM_PREAMBLE_SIZE 4

#define QT_IMA_ADPCM_PREAMBLE_SIZE 2
#define QT_IMA_ADPCM_BLOCK_SIZE 0x22
#define QT_IMA_ADPCM_SAMPLES_PER_BLOCK 64

#define BE_16(x) (be2me_16(*(unsigned short *)(x)))
#define BE_32(x) (be2me_32(*(unsigned int *)(x)))
#define LE_16(x) (le2me_16(*(unsigned short *)(x)))
#define LE_32(x) (le2me_32(*(unsigned int *)(x)))

// pertinent tables for IMA ADPCM
static int adpcm_step[89] =
{
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
  19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
  130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
  337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
  876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
  2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
  5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static int adpcm_index[16] =
{
  -1, -1, -1, -1, 2, 4, 6, 8,
  -1, -1, -1, -1, 2, 4, 6, 8
};

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

static ad_info_t info = 
{
	"IMA ADPCM audio decoder",
	"imaadpcm",
	"Nick Kurshev",
	"Mike Melanson",
	""
};

LIBAD_EXTERN(imaadpcm)

static int preinit(sh_audio_t *sh_audio)
{
  // not exactly sure what this field is for
  sh_audio->audio_out_minsize = 8192;

  // if format is "ima4", assume the audio is coming from a QT file which
  // indicates constant block size, whereas an AVI/ASF/WAV file will fill
  // in this field with 0x11
  if ((sh_audio->format == 0x11) || (sh_audio->format == 0x61) ||
      (sh_audio->format == 0x1100736d))
  {
    sh_audio->ds->ss_div = (sh_audio->wf->nBlockAlign - 
      (MS_IMA_ADPCM_PREAMBLE_SIZE * sh_audio->wf->nChannels)) * 2;
    sh_audio->ds->ss_mul = sh_audio->wf->nBlockAlign;
  }
  else
  {
    sh_audio->ds->ss_div = QT_IMA_ADPCM_SAMPLES_PER_BLOCK;
    sh_audio->ds->ss_mul = QT_IMA_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels;
  }
  sh_audio->audio_in_minsize=sh_audio->ds->ss_mul;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
  /* IMA-ADPCM 4:1 audio codec:*/
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  /* decodes 34 byte -> 64 short*/
  sh_audio->i_bps = 
    (sh_audio->ds->ss_mul * sh_audio->samplerate) / sh_audio->ds->ss_div;
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

static void decode_nibbles(unsigned short *output,
  int output_size, int channels,
  int predictor_l, int index_l,
  int predictor_r, int index_r)
{
  int step[2];
  int predictor[2];
  int index[2];
  int diff;
  int i;
  int sign;
  int delta;
  int channel_number = 0;

  step[0] = adpcm_step[index_l];
  step[1] = adpcm_step[index_r];
  predictor[0] = predictor_l;
  predictor[1] = predictor_r;
  index[0] = index_l;
  index[1] = index_r;

  for (i = 0; i < output_size; i++)
  {
    delta = output[i];

    index[channel_number] += adpcm_index[delta];
    CLAMP_0_TO_88(index[channel_number]);

    sign = delta & 8;
    delta = delta & 7;

    diff = step[channel_number] >> 3;
    if (delta & 4) diff += step[channel_number];
    if (delta & 2) diff += step[channel_number] >> 1;
    if (delta & 1) diff += step[channel_number] >> 2;

    if (sign)
      predictor[channel_number] -= diff;
    else
      predictor[channel_number] += diff;

    CLAMP_S16(predictor[channel_number]);
    output[i] = predictor[channel_number];
    step[channel_number] = adpcm_step[index[channel_number]];

    // toggle channel
    channel_number ^= channels - 1;

  }
}

static int qt_ima_adpcm_decode_block(unsigned short *output,
  unsigned char *input, int channels)
{
  int initial_predictor_l = 0;
  int initial_predictor_r = 0;
  int initial_index_l = 0;
  int initial_index_r = 0;
  int i;

  initial_predictor_l = BE_16(&input[0]);
  initial_index_l = initial_predictor_l;

  // mask, sign-extend, and clamp the predictor portion
  initial_predictor_l &= 0xFF80;
  SE_16BIT(initial_predictor_l);
  CLAMP_S16(initial_predictor_l);

  // mask and clamp the index portion
  initial_index_l &= 0x7F;
  CLAMP_0_TO_88(initial_index_l);

  // handle stereo
  if (channels > 1)
  {
    initial_predictor_r = BE_16(&input[QT_IMA_ADPCM_BLOCK_SIZE]);
    initial_index_r = initial_predictor_r;

    // mask, sign-extend, and clamp the predictor portion
    initial_predictor_r &= 0xFF80;
    SE_16BIT(initial_predictor_r);
    CLAMP_S16(initial_predictor_r);

    // mask and clamp the index portion
    initial_index_r &= 0x7F;
    CLAMP_0_TO_88(initial_index_r);
  }

  // break apart all of the nibbles in the block
  if (channels == 1)
    for (i = 0; i < QT_IMA_ADPCM_SAMPLES_PER_BLOCK / 2; i++)
    {
      output[i * 2 + 0] = input[2 + i] & 0x0F;
      output[i * 2 + 1] = input[2 + i] >> 4;
    }
  else
    for (i = 0; i < QT_IMA_ADPCM_SAMPLES_PER_BLOCK / 2; i++)
    {
      output[i * 4 + 0] = input[2 + i] & 0x0F;
      output[i * 4 + 1] = input[2 + QT_IMA_ADPCM_BLOCK_SIZE + i] & 0x0F;
      output[i * 4 + 2] = input[2 + i] >> 4;
      output[i * 4 + 3] = input[2 + QT_IMA_ADPCM_BLOCK_SIZE + i] >> 4;
    }

  decode_nibbles(output,
    QT_IMA_ADPCM_SAMPLES_PER_BLOCK * channels, channels,
    initial_predictor_l, initial_index_l,
    initial_predictor_r, initial_index_r);

  return QT_IMA_ADPCM_SAMPLES_PER_BLOCK * channels;
}

static int ms_ima_adpcm_decode_block(unsigned short *output,
  unsigned char *input, int channels, int block_size)
{
  int predictor_l = 0;
  int predictor_r = 0;
  int index_l = 0;
  int index_r = 0;
  int i;
  int channel_counter;
  int channel_index;
  int channel_index_l;
  int channel_index_r;

  predictor_l = LE_16(&input[0]);
  SE_16BIT(predictor_l);
  index_l = input[2];
  if (channels == 2)
  {
    predictor_r = LE_16(&input[4]);
    SE_16BIT(predictor_r);
    index_r = input[6];
  }

  if (channels == 1)
    for (i = 0;
      i < (block_size - MS_IMA_ADPCM_PREAMBLE_SIZE * channels); i++)
    {
      output[i * 2 + 0] = input[MS_IMA_ADPCM_PREAMBLE_SIZE + i] & 0x0F;
      output[i * 2 + 1] = input[MS_IMA_ADPCM_PREAMBLE_SIZE + i] >> 4;
    }
  else
  {
    // encoded as 8 nibbles (4 bytes) per channel; switch channel every
    // 4th byte
    channel_counter = 0;
    channel_index_l = 0;
    channel_index_r = 1;
    channel_index = channel_index_l;
    for (i = 0;
      i < (block_size - MS_IMA_ADPCM_PREAMBLE_SIZE * channels); i++)
    {
      output[channel_index + 0] =
        input[MS_IMA_ADPCM_PREAMBLE_SIZE * 2 + i] & 0x0F;
      output[channel_index + 2] =
        input[MS_IMA_ADPCM_PREAMBLE_SIZE * 2 + i] >> 4;
      channel_index += 4;
      channel_counter++;
      if (channel_counter == 4)
      {
        channel_index_l = channel_index;
        channel_index = channel_index_r;
      }
      else if (channel_counter == 8)
      {
        channel_index_r = channel_index;
        channel_index = channel_index_l;
        channel_counter = 0;
      }
    }
  }
  
  decode_nibbles(output,
    (block_size - MS_IMA_ADPCM_PREAMBLE_SIZE * channels) * 2,
    channels,
    predictor_l, index_l,
    predictor_r, index_r);

  return (block_size - MS_IMA_ADPCM_PREAMBLE_SIZE * channels) * 2;
}

static int dk4_ima_adpcm_decode_block(unsigned short *output,
  unsigned char *input, int channels, int block_size)
{
  int i;
  int output_ptr;
  int predictor_l = 0;
  int predictor_r = 0;
  int index_l = 0;
  int index_r = 0;

  // the first predictor value goes straight to the output
  predictor_l = output[0] = LE_16(&input[0]);
  SE_16BIT(predictor_l);
  index_l = input[2];
  if (channels == 2)
  {
    predictor_r = output[1] = LE_16(&input[4]);
    SE_16BIT(predictor_r);
    index_r = input[6];
  }

  output_ptr = channels;
  for (i = MS_IMA_ADPCM_PREAMBLE_SIZE * channels; i < block_size; i++)
  {
    output[output_ptr++] = input[i] >> 4;
    output[output_ptr++] = input[i] & 0x0F;
  }

  decode_nibbles(&output[channels],
    (block_size - MS_IMA_ADPCM_PREAMBLE_SIZE * channels) * 2 - channels,
    channels,
    predictor_l, index_l,
    predictor_r, index_r);

  return (block_size - MS_IMA_ADPCM_PREAMBLE_SIZE * channels) * 2 - channels;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  if (demux_read_data(sh_audio->ds, sh_audio->a_in_buffer,
    sh_audio->ds->ss_mul) != 
    sh_audio->ds->ss_mul) 
    return -1;

  if ((sh_audio->format == 0x11) || (sh_audio->format == 0x1100736d))
  {
    return 2 * ms_ima_adpcm_decode_block(
      (unsigned short*)buf, sh_audio->a_in_buffer, sh_audio->wf->nChannels,
      sh_audio->ds->ss_mul);
  }
  else if (sh_audio->format == 0x61)
  {
    return 2 * dk4_ima_adpcm_decode_block(
      (unsigned short*)buf, sh_audio->a_in_buffer, sh_audio->wf->nChannels,
      sh_audio->ds->ss_mul);
  }
  else
  {
    return 2 * qt_ima_adpcm_decode_block(
      (unsigned short*)buf, sh_audio->a_in_buffer, sh_audio->wf->nChannels);
  }
}
