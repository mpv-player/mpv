/*
        RoQ A/V decoder for the MPlayer program
        by Mike Melanson
        based on Dr. Tim Ferguson's RoQ document found at:
          http://www.csse.monash.edu.au/~timf/videocodec.html
*/

#include "config.h"
#include "bswap.h"
#include <stdio.h>

#define LE_16(x) (le2me_16(*(unsigned short *)(x)))
#define LE_32(x) (le2me_32(*(unsigned int *)(x)))

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

void *roq_decode_audio_init(void)
{
}

int roq_decode_audio(
  unsigned short *output,
  unsigned char *input,
  int channels,
  void *context)
{
}
