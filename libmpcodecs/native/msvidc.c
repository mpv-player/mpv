/*
    Microsoft Video 1 Decoder
    
    (C) 2001 Mike Melanson
    
    The description of the algorithm you can read here:
      http://www.pcisys.net/~melanson/codecs/

    32bpp support (c) alex
*/

#include "config.h"
#include "bswap.h"

#define LE_16(x) (le2me_16(*(unsigned short *)(x)))

#define DECODE_BGR555_TO_BGR888(x) \
        x.c1_b = (x.c1 >> 7) & 0xF8; \
        x.c1_g = (x.c1 >> 2) & 0xF8; \
        x.c1_r = (x.c1 << 3) & 0xF8; \
        x.c2_b = (x.c2 >> 7) & 0xF8; \
        x.c2_g = (x.c2 >> 2) & 0xF8; \
        x.c2_r = (x.c2 << 3) & 0xF8;

#define DECODE_PALETTE_TO_BGR888(x) \
        x.c1_b = palette_map[x.c1 * 4 + 2]; \
        x.c1_g = palette_map[x.c1 * 4 + 1]; \
        x.c1_r = palette_map[x.c1 * 4 + 0]; \
        x.c2_b = palette_map[x.c2 * 4 + 2]; \
        x.c2_g = palette_map[x.c2 * 4 + 1]; \
        x.c2_r = palette_map[x.c2 * 4 + 0];

struct
{
  unsigned short c1, c2;
  unsigned char c1_r, c1_g, c1_b;
  unsigned char c2_r, c2_g, c2_b;
} quad[2][2];

void AVI_Decode_Video1_16(
  char *encoded,
  int encoded_size,
  char *decoded,
  int width,
  int height,
  int bytes_per_pixel)
{
  int block_ptr, pixel_ptr;
  int total_blocks;
  int pixel_x, pixel_y;  // pixel width and height iterators
  int block_x, block_y;  // block width and height iterators
  int blocks_wide, blocks_high;  // width and height in 4x4 blocks
  int block_inc;
  int row_dec;

  // decoding parameters
  int stream_ptr;
  unsigned char byte_a, byte_b;
  unsigned short flags;
  int skip_blocks;

  stream_ptr = 0;
  skip_blocks = 0;
  blocks_wide = width / 4;
  blocks_high = height / 4;
  total_blocks = blocks_wide * blocks_high;
  block_inc = 4 * bytes_per_pixel;
  row_dec = (width + 4) * bytes_per_pixel;

  for (block_y = blocks_high; block_y > 0; block_y--)
  {
    block_ptr = ((block_y * 4) - 1) * (width * bytes_per_pixel);
    for (block_x = blocks_wide; block_x > 0; block_x--)
    {
      // check if this block should be skipped
      if (skip_blocks)
      {
        block_ptr += block_inc;
        skip_blocks--;
        total_blocks--;
        continue;
      }

      pixel_ptr = block_ptr;

      // get the next two bytes in the encoded data stream
      byte_a = encoded[stream_ptr++];
      byte_b = encoded[stream_ptr++];

      // check if the decode is finished
      if ((byte_a == 0) && (byte_b == 0) && (total_blocks == 0))
        return;

      // check if this is a skip code
      else if ((byte_b & 0xFC) == 0x84)
      {
        // but don't count the current block
        skip_blocks = ((byte_b - 0x84) << 8) + byte_a - 1;
      }

      // check if this is in the 2- or 8-color classes
      else if (byte_b < 0x80)
      {
        flags = (byte_b << 8) | byte_a;

        quad[0][0].c1 = LE_16(&encoded[stream_ptr]);
        stream_ptr += 2;
        quad[0][0].c2 = LE_16(&encoded[stream_ptr]);
        stream_ptr += 2;

        DECODE_BGR555_TO_BGR888(quad[0][0]);

        if (quad[0][0].c1 & 0x8000)
        {
          // 8-color encoding
          quad[1][0].c1 = LE_16(&encoded[stream_ptr]);
          stream_ptr += 2;
          quad[1][0].c2 = LE_16(&encoded[stream_ptr]);
          stream_ptr += 2;
          quad[0][1].c1 = LE_16(&encoded[stream_ptr]);
          stream_ptr += 2;
          quad[0][1].c2 = LE_16(&encoded[stream_ptr]);
          stream_ptr += 2;
          quad[1][1].c1 = LE_16(&encoded[stream_ptr]);
          stream_ptr += 2;
          quad[1][1].c2 = LE_16(&encoded[stream_ptr]);
          stream_ptr += 2;

          DECODE_BGR555_TO_BGR888(quad[0][1]);
          DECODE_BGR555_TO_BGR888(quad[1][0]);
          DECODE_BGR555_TO_BGR888(quad[1][1]);

          for (pixel_y = 0; pixel_y < 4; pixel_y++)
          {
            for (pixel_x = 0; pixel_x < 4; pixel_x++)
            {
              if (flags & 1)
              {
                decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c1_r;
                decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c1_g;
                decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c1_b;
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;
              }
              else
              {
                decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c2_r;
                decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c2_g;
                decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c2_b;
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;
              }

              // get the next flag ready to go
              flags >>= 1;
            }
            pixel_ptr -= row_dec;
          }
        }
        else
        {
          // 2-color encoding
          for (pixel_y = 0; pixel_y < 4; pixel_y++)
          {
            for (pixel_x = 0; pixel_x < 4; pixel_x++)
            {
              if (flags & 1)
              {
                decoded[pixel_ptr++] = quad[0][0].c1_r;
                decoded[pixel_ptr++] = quad[0][0].c1_g;
                decoded[pixel_ptr++] = quad[0][0].c1_b;
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;
              }
              else
              {
                decoded[pixel_ptr++] = quad[0][0].c2_r;
                decoded[pixel_ptr++] = quad[0][0].c2_g;
                decoded[pixel_ptr++] = quad[0][0].c2_b;
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;
              }

              // get the next flag ready to go
              flags >>= 1;
            }
            pixel_ptr -= row_dec;
          }
        }
      }

      // otherwise, it's a 1-color block
      else
      {
        quad[0][0].c1 = (byte_b << 8) | byte_a;
        DECODE_BGR555_TO_BGR888(quad[0][0]);

        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            decoded[pixel_ptr++] = quad[0][0].c1_r;
            decoded[pixel_ptr++] = quad[0][0].c1_g;
            decoded[pixel_ptr++] = quad[0][0].c1_b;
	    if (bytes_per_pixel == 4) /* 32bpp */
		pixel_ptr++;
          }
          pixel_ptr -= row_dec;
        }
      }

      block_ptr += block_inc;
      total_blocks--;
    }
  }
}

void AVI_Decode_Video1_8(
  char *encoded,
  int encoded_size,
  char *decoded,
  int width,
  int height,
  unsigned char *palette_map,
  int bytes_per_pixel)
{
  int block_ptr, pixel_ptr;
  int total_blocks;
  int pixel_x, pixel_y;  // pixel width and height iterators
  int block_x, block_y;  // block width and height iterators
  int blocks_wide, blocks_high;  // width and height in 4x4 blocks
  int block_inc;
  int row_dec;

  // decoding parameters
  int stream_ptr;
  unsigned char byte_a, byte_b;
  unsigned short flags;
  int skip_blocks;

  stream_ptr = 0;
  skip_blocks = 0;
  blocks_wide = width / 4;
  blocks_high = height / 4;
  total_blocks = blocks_wide * blocks_high;
  block_inc = 4 * bytes_per_pixel;
  row_dec = (width + 4) * bytes_per_pixel;

  for (block_y = blocks_high; block_y > 0; block_y--)
  {
    block_ptr = ((block_y * 4) - 1) * (width * bytes_per_pixel);
    for (block_x = blocks_wide; block_x > 0; block_x--)
    {
      // check if this block should be skipped
      if (skip_blocks)
      {
        block_ptr += block_inc;
        skip_blocks--;
        total_blocks--;
        continue;
      }

      pixel_ptr = block_ptr;

      // get the next two bytes in the encoded data stream
      byte_a = encoded[stream_ptr++];
      byte_b = encoded[stream_ptr++];

      // check if the decode is finished
      if ((byte_a == 0) && (byte_b == 0) && (total_blocks == 0))
        return;

      // check if this is a skip code
      else if ((byte_b & 0xFC) == 0x84)
      {
        // but don't count the current block
        skip_blocks = ((byte_b - 0x84) << 8) + byte_a - 1;
      }

      // check if this is a 2-color block
      else if (byte_b < 0x80)
      {
        flags = (byte_b << 8) | byte_a;

        quad[0][0].c1 = (unsigned char)encoded[stream_ptr++];
        quad[0][0].c2 = (unsigned char)encoded[stream_ptr++];

        DECODE_PALETTE_TO_BGR888(quad[0][0]);

        // 2-color encoding
        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            if (flags & 1)
            {
              decoded[pixel_ptr++] = quad[0][0].c1_r;
              decoded[pixel_ptr++] = quad[0][0].c1_g;
              decoded[pixel_ptr++] = quad[0][0].c1_b;
	      if (bytes_per_pixel == 4) /* 32bpp */
		pixel_ptr++;
            }
            else
            {
              decoded[pixel_ptr++] = quad[0][0].c2_r;
              decoded[pixel_ptr++] = quad[0][0].c2_g;
              decoded[pixel_ptr++] = quad[0][0].c2_b;
	      if (bytes_per_pixel == 4) /* 32bpp */
		pixel_ptr++;
            }

            // get the next flag ready to go
            flags >>= 1;
          }
          pixel_ptr -= row_dec;
        }
      }

      // check if it's an 8-color block
      else if (byte_b >= 0x90)
      {
        flags = (byte_b << 8) | byte_a;

        quad[0][0].c1 = (unsigned char)encoded[stream_ptr++];
        quad[0][0].c2 = (unsigned char)encoded[stream_ptr++];
        quad[1][0].c1 = (unsigned char)encoded[stream_ptr++];
        quad[1][0].c2 = (unsigned char)encoded[stream_ptr++];

        quad[0][1].c1 = (unsigned char)encoded[stream_ptr++];
        quad[0][1].c2 = (unsigned char)encoded[stream_ptr++];
        quad[1][1].c1 = (unsigned char)encoded[stream_ptr++];
        quad[1][1].c2 = (unsigned char)encoded[stream_ptr++];

        DECODE_PALETTE_TO_BGR888(quad[0][0]);
        DECODE_PALETTE_TO_BGR888(quad[0][1]);
        DECODE_PALETTE_TO_BGR888(quad[1][0]);
        DECODE_PALETTE_TO_BGR888(quad[1][1]);

        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            if (flags & 1)
            {
              decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c1_r;
              decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c1_g;
              decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c1_b;
	      if (bytes_per_pixel == 4) /* 32bpp */
		pixel_ptr++;
            }
            else
            {
              decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c2_r;
              decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c2_g;
              decoded[pixel_ptr++] = quad[pixel_x >> 1][pixel_y >> 1].c2_b;
	      if (bytes_per_pixel == 4) /* 32bpp */
		pixel_ptr++;
            }

            // get the next flag ready to go
            flags >>= 1;
          }
          pixel_ptr -= row_dec;
        }
      }

      // otherwise, it's a 1-color block
      else
      {
        // init c2 along with c1 just so c2 is a known value for macro
        quad[0][0].c1 = quad[0][0].c2 = byte_a;
        DECODE_PALETTE_TO_BGR888(quad[0][0]);

        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            decoded[pixel_ptr++] = quad[0][0].c1_r;
            decoded[pixel_ptr++] = quad[0][0].c1_g;
            decoded[pixel_ptr++] = quad[0][0].c1_b;
	    if (bytes_per_pixel == 4) /* 32bpp */
		pixel_ptr++;
          }
          pixel_ptr -= row_dec;
        }
      }

      block_ptr += block_inc;
      total_blocks--;
    }
  }
}

