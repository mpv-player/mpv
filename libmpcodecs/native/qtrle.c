/*
    Quicktime Animation (RLE) Decoder for MPlayer

    (C) 2001 Mike Melanson
*/

#include "config.h"
#include "bswap.h"

#define BE_16(x) (be2me_16(*(unsigned short *)(x)))
#define BE_32(x) (be2me_32(*(unsigned int *)(x)))

// 256 RGB entries; 25% of these bytes will be unused, but it's faster
// to index 4-byte entries
static unsigned char palette[256 * 4];

void qt_decode_rle24(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel)
{
  int stream_ptr;
  int header;
  int start_line;
  int lines_to_change;
  signed char rle_code;
  int row_ptr, pixel_ptr;
  int row_inc = bytes_per_pixel * width;
  unsigned char r, g, b;

  // check if this frame is even supposed to change
  if (encoded_size < 8)
    return;

  // start after the chunk size
  stream_ptr = 4;

  // fetch the header
  header = BE_16(&encoded[stream_ptr]);
  stream_ptr += 2;

  // if a header is present, fetch additional decoding parameters
  if (header & 0x0008)
  {
    start_line = BE_16(&encoded[stream_ptr]);
    stream_ptr += 4;
    lines_to_change = BE_16(&encoded[stream_ptr]);
    stream_ptr += 4;
  }
  else
  {
    start_line = 0;
    lines_to_change = height;
  }

  row_ptr = row_inc * start_line;
  while (lines_to_change--)
  {
    pixel_ptr = row_ptr + ((encoded[stream_ptr++] - 1) * bytes_per_pixel);

    while (stream_ptr < encoded_size &&
           (rle_code = (signed char)encoded[stream_ptr++]) != -1)
    {
      if (rle_code == 0)
        // there's another skip code in the stream
        pixel_ptr += ((encoded[stream_ptr++] - 1) * bytes_per_pixel);
      else if (rle_code < 0)
      {
        // decode the run length code
        rle_code = -rle_code;
        r = encoded[stream_ptr++];
        g = encoded[stream_ptr++];
        b = encoded[stream_ptr++];
        while (rle_code--)
        {
          decoded[pixel_ptr++] = b;
          decoded[pixel_ptr++] = g;
          decoded[pixel_ptr++] = r;
          if (bytes_per_pixel == 4)
            pixel_ptr++;
        }
      }
      else
      {
        // copy pixels directly to output
        while (rle_code--)
        {
          decoded[pixel_ptr++] = encoded[stream_ptr + 2];
          decoded[pixel_ptr++] = encoded[stream_ptr + 1];
          decoded[pixel_ptr++] = encoded[stream_ptr + 0];
          stream_ptr += 3;
          if (bytes_per_pixel == 4)
            pixel_ptr++;
        }
      }
    }

    row_ptr += row_inc;
  }
}

void qt_decode_rle(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int encoded_bpp,
  int bytes_per_pixel)
{
  switch (encoded_bpp)
  {
    case 24:
      qt_decode_rle24(
        encoded,
        encoded_size,
        decoded,
        width,
        height,
        bytes_per_pixel);
      break;
  }
}
