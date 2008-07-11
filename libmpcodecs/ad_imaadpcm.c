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
#include <inttypes.h>

#include "config.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "ad_internal.h"

#define MS_IMA_ADPCM_PREAMBLE_SIZE 4

#define QT_IMA_ADPCM_PREAMBLE_SIZE 2
#define QT_IMA_ADPCM_BLOCK_SIZE 0x22
#define QT_IMA_ADPCM_SAMPLES_PER_BLOCK 64

#define BE_16(x) (be2me_16(*(unsigned short *)(x)))
#define LE_16(x) (le2me_16(*(unsigned short *)(x)))

// pertinent tables for IMA ADPCM
static const int16_t adpcm_step[89] =
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

static const int8_t adpcm_index[8] =
{
  -1, -1, -1, -1, 2, 4, 6, 8,
};

// useful macros
// clamp a number between 0 and 88
#define CLAMP_0_TO_88(x) x = av_clip(x, 0, 88);
// clamp a number within a signed 16-bit range
#define CLAMP_S16(x) x = av_clip_int16(x);
// clamp a number above 16
#define CLAMP_ABOVE_16(x)  if (x < 16) x = 16;

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
  int predictor[2], int index[2])
{
  int step[2];
  int i;
  int sign;
  int delta;
  int channel_number = 0;

  step[0] = adpcm_step[index[0]];
  step[1] = adpcm_step[index[1]];

  for (i = 0; i < output_size; i++)
  {
    delta = output[i];
    sign = delta & 8;
    delta = delta & 7;

    index[channel_number] += adpcm_index[delta];
    CLAMP_0_TO_88(index[channel_number]);

    delta = 2 * delta + 1;
    if (sign) delta = -delta;

    predictor[channel_number] += (delta * step[channel_number]) >> 3;

    CLAMP_S16(predictor[channel_number]);
    output[i] = predictor[channel_number];
    step[channel_number] = adpcm_step[index[channel_number]];

    // toggle channel
    channel_number ^= channels - 1;

  }
}

static int qt_ima_adpcm_decode_block(unsigned short *output,
  unsigned char *input, int channels, int block_size)
{
  int initial_predictor[2];
  int initial_index[2];
  int i;

  if (channels != 1) channels = 2;
  if (block_size < channels * QT_IMA_ADPCM_BLOCK_SIZE)
    return -1;

  for (i = 0; i < channels; i++) {
    initial_index[i] = initial_predictor[i] = (int16_t)BE_16(&input[i * QT_IMA_ADPCM_BLOCK_SIZE]);

    // mask, sign-extend, and clamp the predictor portion
    initial_predictor[i] &= ~0x7F;
    CLAMP_S16(initial_predictor[i]);

    // mask and clamp the index portion
    initial_index[i] &= 0x7F;
    CLAMP_0_TO_88(initial_index[i]);
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
    initial_predictor, initial_index);

  return QT_IMA_ADPCM_SAMPLES_PER_BLOCK * channels;
}

static int ms_ima_adpcm_decode_block(unsigned short *output,
  unsigned char *input, int channels, int block_size)
{
  int predictor[2];
  int index[2];
  int i;
  int channel_counter;
  int channel_index;
  int channel_index_l;
  int channel_index_r;

  if (channels != 1) channels = 2;
  if (block_size < MS_IMA_ADPCM_PREAMBLE_SIZE * channels)
    return -1;

  for (i = 0; i < channels; i++) {
    predictor[i] = (int16_t)LE_16(&input[i * 4]);
    index[i] = input[i * 4 + 2];
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
    predictor, index);

  return (block_size - MS_IMA_ADPCM_PREAMBLE_SIZE * channels) * 2;
}

static int dk4_ima_adpcm_decode_block(unsigned short *output,
  unsigned char *input, int channels, int block_size)
{
  int i;
  int output_ptr;
  int predictor[2];
  int index[2];

  if (channels != 1) channels = 2;
  if (block_size < MS_IMA_ADPCM_PREAMBLE_SIZE * channels)
    return -1;

  for (i = 0; i < channels; i++) {
    // the first predictor value goes straight to the output
    predictor[i] = output[i] = (int16_t)LE_16(&input[i * 4]);
    index[i] = input[i * 4 + 2];
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
    predictor, index);

  return (block_size - MS_IMA_ADPCM_PREAMBLE_SIZE * channels) * 2 - channels;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  int res = -1;
  int (*decode_func)(unsigned short *output, unsigned char *input, int channels, int block_size) = qt_ima_adpcm_decode_block;
  if (demux_read_data(sh_audio->ds, sh_audio->a_in_buffer,
    sh_audio->ds->ss_mul) != 
    sh_audio->ds->ss_mul) 
    return -1;

  if ((sh_audio->format == 0x11) || (sh_audio->format == 0x1100736d))
    decode_func = ms_ima_adpcm_decode_block;
  else if (sh_audio->format == 0x61)
    decode_func = dk4_ima_adpcm_decode_block;

  res = decode_func((unsigned short*)buf, sh_audio->a_in_buffer,
                    sh_audio->wf->nChannels, sh_audio->ds->ss_mul);
  return res < 0 ? res : 2 * res;
}
