/*
    Unified ADPCM Decoder for MPlayer

    (C) 2001 Mike Melanson
*/

#include "config.h"
#include "bswap.h"
#include "adpcm.h"

#define BE_16(x) (be2me_16(*(unsigned short *)(x)))
#define BE_32(x) (be2me_32(*(unsigned int *)(x)))
#define LE_16(x) (le2me_16(*(unsigned short *)(x)))
#define LE_32(x) (le2me_32(*(unsigned int *)(x)))

// clamp a number between 0 and 88
#define CLAMP_0_TO_88(x)  if (x < 0) x = 0; else if (x > 88) x = 88;
// clamp a number within a signed 16-bit range
#define CLAMP_S16(x)  if (x < -32768) x = -32768; \
  else if (x > 32767) x = 32767;
// sign extend a 16-bit value
#define SE_16BIT(x)  if (x & 0x8000) x -= 0x10000;

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
  int stream_ptr = 0;
  int i;

  initial_predictor_l = BE_16(&input[stream_ptr]);
  stream_ptr += 2;
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
    initial_predictor_r = BE_16(&input[stream_ptr]);
    stream_ptr += 2;
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
      output[i * 2 + 0] = input[stream_ptr] & 0x0F;
      output[i * 2 + 1] = input[stream_ptr] >> 4;
      stream_ptr++;
    }  
  else
    for (i = 0; i < IMA_ADPCM_SAMPLES_PER_BLOCK / 2 * 2; i++)
    {
      output[i * 4 + 0] = input[stream_ptr] & 0x0F;
      output[i * 4 + 1] = input[stream_ptr + 1] & 0x0F;
      output[i * 4 + 2] = input[stream_ptr] >> 4;
      output[i * 4 + 3] = input[stream_ptr + 1] >> 4;
      stream_ptr++;
    }  

  ima_dvi_decode_nibbles(output, channels,
    initial_predictor_l, initial_index_l,
    initial_predictor_r, initial_index_r);

  return IMA_ADPCM_SAMPLES_PER_BLOCK * channels;
}
