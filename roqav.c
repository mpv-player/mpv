/*
        RoQ A/V decoder for the MPlayer program
        by Mike Melanson
        based on Dr. Tim Ferguson's RoQ document and accompanying source
        code found at:
          http://www.csse.monash.edu.au/~timf/videocodec.html
*/

#include "config.h"
#include "bswap.h"
#include <stdio.h>
#include <stdlib.h>

#define LE_16(x) (le2me_16(*(unsigned short *)(x)))
#define LE_32(x) (le2me_32(*(unsigned int *)(x)))

#define CLAMP_S16(x)  if (x < -32768) x = -32768; \
  else if (x > 32767) x = 32767;
#define SE_16BIT(x)  if (x & 0x8000) x -= 0x10000;
// sign extend a 4-bit value

void *roq_decode_video_init(void)
{
}

void roq_decode_video(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  void *context)
{
}

// Initialize the RoQ audio decoder, which is to say, initialize the table
// of squares.
void *roq_decode_audio_init(void)
{
  short *square_array;
  short square;
  int i;

  square_array = (short *)malloc(256 * sizeof(short));
  if (!square_array)
    return NULL;

  for (i = 0; i < 128; i++)
  {
    square = i * i;
    square_array[i] = square;
    square_array[i + 128] = -square;
  }

  return square_array;
}

int roq_decode_audio(
  unsigned short *output,
  unsigned char *input,
  int encoded_size,
  int channels,
  void *context)
{
  short *square_array = (short *)context;
  int i;
  int predictor[2];
  int channel_number = 0;

  // prepare the initial predictors
  if (channels == 1)
    predictor[0] = LE_16(&input[0]);
  else
  {
    predictor[0] = input[1] << 8;
    predictor[1] = input[0] << 8;
  }
  SE_16BIT(predictor[0]);
  SE_16BIT(predictor[1]);

  // decode the samples
  for (i = 2; i < encoded_size; i++)
  {
    predictor[channel_number] += square_array[input[i]];
    CLAMP_S16(predictor[channel_number]);
    output[i - 2] = predictor[channel_number];

    // toggle channel
    channel_number ^= channels - 1;
  }

  // return the number of samples decoded
  return (encoded_size - 2);
}
