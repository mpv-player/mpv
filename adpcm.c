/*
    Unified ADPCM Decoder for MPlayer

    This file is in charge of decoding all of the various ADPCM data
    formats that various entities have created. Details about the data
    formats can be found here:
      http://www.pcisys.net/~melanson/codecs/

    (C) 2001 Mike Melanson
*/

#include "config.h"
#include "bswap.h"
#include "adpcm.h"

#define BE_16(x) (be2me_16(*(unsigned short *)(x)))
#define BE_32(x) (be2me_32(*(unsigned int *)(x)))
#define LE_16(x) (le2me_16(*(unsigned short *)(x)))
#define LE_32(x) (le2me_32(*(unsigned int *)(x)))

// pertinent tables
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

static int format_0x62_table[16] =
{
  1, 3, 5, 7, 9, 11, 13, 15,
  -1, -3, -5, -7, -9, -11, -13, -15
};

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

void ima_dvi_decode_nibbles(unsigned short *output, int channels,
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

  for (i = 0; i < IMA_ADPCM_SAMPLES_PER_BLOCK * channels; i++)
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

int ima_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels)
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
    initial_predictor_r = BE_16(&input[IMA_ADPCM_BLOCK_SIZE]);
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
    for (i = 0; i < IMA_ADPCM_SAMPLES_PER_BLOCK / 2; i++)
    {
      output[i * 2 + 0] = input[2 + i] & 0x0F;
      output[i * 2 + 1] = input[2 + i] >> 4;
    }  
  else
    for (i = 0; i < IMA_ADPCM_SAMPLES_PER_BLOCK / 2 * 2; i++)
    {
      output[i * 4 + 0] = input[2 + i] & 0x0F;
      output[i * 4 + 1] = input[2 + IMA_ADPCM_BLOCK_SIZE + i] & 0x0F;
      output[i * 4 + 2] = input[2 + i] >> 4;
      output[i * 4 + 3] = input[2 + IMA_ADPCM_BLOCK_SIZE + i] >> 4;
    }  

  ima_dvi_decode_nibbles(output, channels,
    initial_predictor_l, initial_index_l,
    initial_predictor_r, initial_index_r);

  return IMA_ADPCM_SAMPLES_PER_BLOCK * channels;
}

int ms_adpcm_decode_block(unsigned short *output, unsigned char *input,
  int channels)
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
  coeff1[0] = ms_adapt_coeff1[input[stream_ptr]];
  coeff2[0] = ms_adapt_coeff2[input[stream_ptr]];
  stream_ptr++;
  if (channels == 2)
  {
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

  while (stream_ptr < MS_ADPCM_BLOCK_SIZE * channels)
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

  return MS_ADPCM_SAMPLES_PER_BLOCK * channels;
}
