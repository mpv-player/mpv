/*
    Apple Graphics (SMC) Decoder for MPlayer
    by Mike Melanson
*/

#include "config.h"
#include "bswap.h"

#define BE_16(x) (be2me_16(*(unsigned short *)(x)))
#define BE_32(x) (be2me_32(*(unsigned int *)(x)))

void qt_decode_smc(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel)
{

}
