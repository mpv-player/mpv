/*
    Apple Graphics (SMC) Decoder for MPlayer
    by Mike Melanson
    Special thanks for Roberto Togni <rtogni@bresciaonline.it> for
    tracking down the final, nagging bugs.

    The description of the decoding algorithm can be found here:
      http://www.pcisys.net/~melanson/codecs/
*/

#include <stdlib.h>
#include "config.h"
#include "bswap.h"
#include "mp_msg.h"

#define BE_16(x) (be2me_16(*(unsigned short *)(x)))
#define BE_32(x) (be2me_32(*(unsigned int *)(x)))

#define COLORS_PER_TABLE 256
#define BYTES_PER_COLOR 4

#define CPAIR 2
#define CQUAD 4
#define COCTET 8

static unsigned char *color_pairs;
static unsigned char *color_quads;
static unsigned char *color_octets;

static int color_pair_index;
static int color_quad_index;
static int color_octet_index;

static int smc_initialized;

// returns 0 if successfully initialized (enough memory was available),
//  non-zero on failure
int qt_init_decode_smc(void)
{
  // be pessimistic to start
  smc_initialized = 0;

  // allocate memory for the 3 palette tables
  if ((color_pairs = (unsigned char *)malloc(
    COLORS_PER_TABLE * BYTES_PER_COLOR * 2)) == 0)
    return 1;  
  if ((color_quads = (unsigned char *)malloc(
    COLORS_PER_TABLE * BYTES_PER_COLOR * 4)) == 0)
    return 1;  
  if ((color_octets = (unsigned char *)malloc(
    COLORS_PER_TABLE * BYTES_PER_COLOR * 8)) == 0)
    return 1;

  // if execution got this far, initialization succeeded
  smc_initialized = 1;
  return 0;
}

#define GET_BLOCK_COUNT \
  (opcode & 0x10) ? (1 + encoded[stream_ptr++]) : 1 + (opcode & 0x0F);
#define ADVANCE_BLOCK() \
{ \
  pixel_ptr += block_x_inc; \
  if (pixel_ptr >= byte_width) \
  { \
    pixel_ptr = 0; \
    row_ptr += block_y_inc * 4; \
  } \
  total_blocks--; \
  if (total_blocks < 0) \
  { \
    mp_msg(MSGT_DECVIDEO, MSGL_WARN, "block counter just went negative (this should not happen)\n"); \
    return; \
  } \
}

void qt_decode_smc(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int pixel_width,
  int pixel_height,
  unsigned char *palette_map,
  int bytes_per_pixel)
{
  int i;
  int stream_ptr = 0;
  int chunk_size;
  unsigned char opcode;
  int n_blocks;
  unsigned int color_flags;
  unsigned int color_flags_a;
  unsigned int color_flags_b;
  unsigned int flag_mask;

  int byte_width = pixel_width * bytes_per_pixel;  // width of a row in bytes
  int byte_height = pixel_height * byte_width;  // max image size, basically
  int row_ptr = 0;
  int pixel_ptr = 0;
  int pixel_x, pixel_y;
  int row_inc = bytes_per_pixel * (pixel_width - 4);
  int block_x_inc = bytes_per_pixel * 4;
  int block_y_inc = bytes_per_pixel * pixel_width;
  int block_ptr;
  int prev_block_ptr;
  int prev_block_ptr1, prev_block_ptr2;
  int prev_block_flag;
  int total_blocks;
  int color_table_index;  // indexes to color pair, quad, or octet tables
  int color_index;  // indexes into palette map

  if (!smc_initialized)
    return;

  // reset color tables
  color_pair_index = 0;
  color_quad_index = 0;
  color_octet_index = 0;

  chunk_size = BE_32(&encoded[stream_ptr]) & 0x00FFFFFF;
  stream_ptr += 4;
  if (chunk_size != encoded_size)
    mp_msg(MSGT_DECVIDEO, MSGL_WARN, "MOV chunk size != encoded chunk size; using MOV chunk size\n");

  chunk_size = encoded_size;
  total_blocks = (pixel_width * pixel_height) / (4 * 4);

  // traverse through the blocks
  while (total_blocks)
  {
    // sanity checks
    // make sure stream ptr hasn't gone out of bounds
    if (stream_ptr > chunk_size)
    {
      mp_msg(MSGT_DECVIDEO, MSGL_ERR, 
        "SMC decoder just went out of bounds (stream ptr = %d, chunk size = %d)\n",
        stream_ptr, chunk_size);
      return;
    }
    // make sure the row pointer hasn't gone wild
    if (row_ptr >= byte_height)
    {
      mp_msg(MSGT_DECVIDEO, MSGL_ERR, 
        "SMC decoder just went out of bounds (row ptr = %d, height = %d)\n",
        row_ptr, byte_height);
      return;
    }

    opcode = encoded[stream_ptr++];
    switch (opcode & 0xF0)
    {
    // skip n blocks
    case 0x00:
    case 0x10:
      n_blocks = GET_BLOCK_COUNT;
      while (n_blocks--)
        ADVANCE_BLOCK();
      break;

    // repeat last block n times
    case 0x20:
    case 0x30:
      n_blocks = GET_BLOCK_COUNT;

      // sanity check
      if ((row_ptr == 0) && (pixel_ptr == 0))
      {
        mp_msg(MSGT_DECVIDEO, MSGL_WARN,
          "encountered repeat block opcode (%02X) but no blocks rendered yet\n",
          opcode & 0xF0);
        break;
      }

      // figure out where the previous block started
      if (pixel_ptr == 0)
        prev_block_ptr1 = (row_ptr - block_y_inc * 4) + 
          byte_width - block_x_inc;
      else
        prev_block_ptr1 = row_ptr + pixel_ptr - block_x_inc;

      while (n_blocks--)
      {
        block_ptr = row_ptr + pixel_ptr;
        prev_block_ptr = prev_block_ptr1;
        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            decoded[block_ptr++] = decoded[prev_block_ptr++];
            decoded[block_ptr++] = decoded[prev_block_ptr++];
            decoded[block_ptr++] = decoded[prev_block_ptr++];
            if (bytes_per_pixel == 4) /* 32bpp */
            {
              block_ptr++;
              prev_block_ptr++;
            }
          }
          block_ptr += row_inc;
          prev_block_ptr += row_inc;
        }
        ADVANCE_BLOCK();
      }
      break;

    // repeat previous pair of blocks n times
    case 0x40:
    case 0x50:
      n_blocks = GET_BLOCK_COUNT;
      n_blocks *= 2;

      // sanity check
      if ((row_ptr == 0) && (pixel_ptr < 2 * block_x_inc))
      {
        mp_msg(MSGT_DECVIDEO, MSGL_WARN,
          "encountered repeat block opcode (%02X) but not enough blocks rendered yet\n",
          opcode & 0xF0);
        break;
      }

      // figure out where the previous 2 blocks started
      if (pixel_ptr == 0)
        prev_block_ptr1 = (row_ptr - block_y_inc * 4) + 
          byte_width - block_x_inc * 2;
      else if (pixel_ptr == block_x_inc)
        prev_block_ptr1 = (row_ptr - block_y_inc * 4) + 
          byte_width - block_x_inc;
      else
        prev_block_ptr1 = row_ptr + pixel_ptr - block_x_inc * 2;

      if (pixel_ptr == 0)
        prev_block_ptr2 = (row_ptr - block_y_inc * 4) + 
          (byte_width - block_x_inc);
      else
        prev_block_ptr2 = row_ptr + pixel_ptr - block_x_inc;

      prev_block_flag = 0;
      while (n_blocks--)
      {
        block_ptr = row_ptr + pixel_ptr;
        if (prev_block_flag)
          prev_block_ptr = prev_block_ptr2;
        else
          prev_block_ptr = prev_block_ptr1;
        prev_block_flag = !prev_block_flag;

        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            decoded[block_ptr++] = decoded[prev_block_ptr++];
            decoded[block_ptr++] = decoded[prev_block_ptr++];
            decoded[block_ptr++] = decoded[prev_block_ptr++];
            if (bytes_per_pixel == 4) /* 32bpp */
            {
              block_ptr++;
              prev_block_ptr++;
            }
          }
          block_ptr += row_inc;
          prev_block_ptr += row_inc;
        }
        ADVANCE_BLOCK();
      }
      break;

    // 1-color block encoding
    case 0x60:
    case 0x70:
      n_blocks = GET_BLOCK_COUNT;
      color_index = encoded[stream_ptr++] * 4;

      while (n_blocks--)
      {
        block_ptr = row_ptr + pixel_ptr;
        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            decoded[block_ptr++] = palette_map[color_index + 0];
            decoded[block_ptr++] = palette_map[color_index + 1];
            decoded[block_ptr++] = palette_map[color_index + 2];
            if (bytes_per_pixel == 4) /* 32bpp */
              block_ptr++;
          }
          block_ptr += row_inc;
        }
        ADVANCE_BLOCK();
      }
      break;

    // 2-color block encoding
    case 0x80:
    case 0x90:
      n_blocks = (opcode & 0x0F) + 1;

      // figure out which color pair to use to paint the 2-color block
      if ((opcode & 0xF0) == 0x80)
      {
        // fetch the next 2 colors from bytestream and store in next
        // available entry in the color pair table
        for (i = 0; i < CPAIR; i++)
        {
          color_index = encoded[stream_ptr++] * BYTES_PER_COLOR;
          color_table_index = CPAIR * BYTES_PER_COLOR * color_pair_index + 
            (i * BYTES_PER_COLOR);
          color_pairs[color_table_index + 0] = palette_map[color_index + 0];
          color_pairs[color_table_index + 1] = palette_map[color_index + 1];
          color_pairs[color_table_index + 2] = palette_map[color_index + 2];
        }
        // this is the base index to use for this block
        color_table_index = CPAIR * BYTES_PER_COLOR * color_pair_index;
        color_pair_index++;
        if (color_pair_index == COLORS_PER_TABLE)
          color_pair_index = 0;
      }
      else
        color_table_index = CPAIR * BYTES_PER_COLOR * encoded[stream_ptr++];

      while (n_blocks--)
      {
        color_flags = BE_16(&encoded[stream_ptr]);
        stream_ptr += 2;
        flag_mask = 0x8000;
        block_ptr = row_ptr + pixel_ptr;
        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            if (color_flags & flag_mask)
              color_index = color_table_index + BYTES_PER_COLOR;
            else
              color_index = color_table_index;
            flag_mask >>= 1;

            decoded[block_ptr++] = color_pairs[color_index + 0];
            decoded[block_ptr++] = color_pairs[color_index + 1];
            decoded[block_ptr++] = color_pairs[color_index + 2];
            if (bytes_per_pixel == 4) /* 32bpp */
              block_ptr++;
          }
          block_ptr += row_inc;
        }
        ADVANCE_BLOCK();
      }
      break;

    // 4-color block encoding
    case 0xA0:
    case 0xB0:
      n_blocks = (opcode & 0x0F) + 1;

      // figure out which color quad to use to paint the 4-color block
      if ((opcode & 0xF0) == 0xA0)
      {
        // fetch the next 4 colors from bytestream and store in next
        // available entry in the color quad table
        for (i = 0; i < CQUAD; i++)
        {
          color_index = encoded[stream_ptr++] * BYTES_PER_COLOR;
          color_table_index = CQUAD * BYTES_PER_COLOR * color_quad_index + 
            (i * BYTES_PER_COLOR);
          color_quads[color_table_index + 0] = palette_map[color_index + 0];
          color_quads[color_table_index + 1] = palette_map[color_index + 1];
          color_quads[color_table_index + 2] = palette_map[color_index + 2];
        }
        // this is the base index to use for this block
        color_table_index = CQUAD * BYTES_PER_COLOR * color_quad_index;
        color_quad_index++;
        if (color_quad_index == COLORS_PER_TABLE)
          color_quad_index = 0;
      }
      else
        color_table_index = CQUAD * BYTES_PER_COLOR * encoded[stream_ptr++];

      while (n_blocks--)
      {
        color_flags = BE_32(&encoded[stream_ptr]);
        stream_ptr += 4;
        // flag mask actually acts as a bit shift count here
        flag_mask = 30;
        block_ptr = row_ptr + pixel_ptr;
        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            color_index = color_table_index + (BYTES_PER_COLOR * 
              ((color_flags >> flag_mask) & 0x03));
            flag_mask -= 2;

            decoded[block_ptr++] = color_quads[color_index + 0];
            decoded[block_ptr++] = color_quads[color_index + 1];
            decoded[block_ptr++] = color_quads[color_index + 2];
            if (bytes_per_pixel == 4) /* 32bpp */
              block_ptr++;
          }
          block_ptr += row_inc;
        }
        ADVANCE_BLOCK();
      }
      break;

    // 8-color block encoding
    case 0xC0:
    case 0xD0:
      n_blocks = (opcode & 0x0F) + 1;

      // figure out which color octet to use to paint the 8-color block
      if ((opcode & 0xF0) == 0xC0)
      {
        // fetch the next 8 colors from bytestream and store in next
        // available entry in the color octet table
        for (i = 0; i < COCTET; i++)
        {
          color_index = encoded[stream_ptr++] * BYTES_PER_COLOR;
          color_table_index = COCTET * BYTES_PER_COLOR * color_octet_index + 
            (i * BYTES_PER_COLOR);
          color_octets[color_table_index + 0] = palette_map[color_index + 0];
          color_octets[color_table_index + 1] = palette_map[color_index + 1];
          color_octets[color_table_index + 2] = palette_map[color_index + 2];
        }
        // this is the base index to use for this block
        color_table_index = COCTET * BYTES_PER_COLOR * color_octet_index;
        color_octet_index++;
        if (color_octet_index == COLORS_PER_TABLE)
          color_octet_index = 0;
      }
      else
        color_table_index = COCTET * BYTES_PER_COLOR * encoded[stream_ptr++];

      while (n_blocks--)
      {
        /*
          For this input:
            01 23 45 67 89 AB
          This is the output:
            flags_a = xx012456, flags_b = xx89A37B
        */
        // build the color flags
        color_flags_a = color_flags_b = 0;
        color_flags_a =
          (encoded[stream_ptr + 0] << 16) |
          ((encoded[stream_ptr + 1] & 0xF0) << 8) |
          ((encoded[stream_ptr + 2] & 0xF0) << 4) |
          ((encoded[stream_ptr + 2] & 0x0F) << 4) |
          ((encoded[stream_ptr + 3] & 0xF0) >> 4);
        color_flags_b =
          (encoded[stream_ptr + 4] << 16) |
          ((encoded[stream_ptr + 5] & 0xF0) << 8) |
          ((encoded[stream_ptr + 1] & 0x0F) << 8) |
          ((encoded[stream_ptr + 3] & 0x0F) << 4) |
          (encoded[stream_ptr + 5] & 0x0F);
        stream_ptr += 6;

        color_flags = color_flags_a;
        // flag mask actually acts as a bit shift count here
        flag_mask = 21;
        block_ptr = row_ptr + pixel_ptr;
        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          // reload flags at third row (iteration pixel_y == 2)
          if (pixel_y == 2)
          {
            color_flags = color_flags_b;
            flag_mask = 21;
          }
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            color_index = color_table_index + (BYTES_PER_COLOR * 
              ((color_flags >> flag_mask) & 0x07));
            flag_mask -= 3;

            decoded[block_ptr++] = color_octets[color_index + 0];
            decoded[block_ptr++] = color_octets[color_index + 1];
            decoded[block_ptr++] = color_octets[color_index + 2];
            if (bytes_per_pixel == 4) /* 32bpp */
              block_ptr++;
          }
          block_ptr += row_inc;
        }
        ADVANCE_BLOCK();
      }
      break;

    // 16-color block encoding (every pixel is a different color)
    case 0xE0:
      n_blocks = (opcode & 0x0F) + 1;

      while (n_blocks--)
      {
        block_ptr = row_ptr + pixel_ptr;
        for (pixel_y = 0; pixel_y < 4; pixel_y++)
        {
          for (pixel_x = 0; pixel_x < 4; pixel_x++)
          {
            color_index = encoded[stream_ptr++] * BYTES_PER_COLOR;
            decoded[block_ptr++] = palette_map[color_index + 0];
            decoded[block_ptr++] = palette_map[color_index + 1];
            decoded[block_ptr++] = palette_map[color_index + 2];
            if (bytes_per_pixel == 4) /* 32bpp */
              block_ptr++;
          }
          block_ptr += row_inc;
        }
        ADVANCE_BLOCK();
      }
      break;

    case 0xF0:
      mp_msg(MSGT_DECVIDEO, MSGL_HINT, "0xF0 opcode seen in SMC chunk (MPlayer developers would like to know)\n");
      break;
    }
  }
}
