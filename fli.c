/*
    FLI Decoder for MPlayer
    
    (C) 2001 Mike Melanson
    
    32bpp support (c) alex

    Additional code and bug fixes by Roberto Togni

    For information on the FLI format, as well as various traps to
    avoid while programming one, visit:
      http://www.pcisys.net/~melanson/codecs/
*/

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "bswap.h"
#include "mp_msg.h"

#define LE_16(x) (le2me_16(*(unsigned short *)(x)))
#define LE_32(x) (le2me_32(*(unsigned int *)(x)))

#define FLI_256_COLOR 4
#define FLI_DELTA     7
#define FLI_COLOR     11
#define FLI_LC        12
#define FLI_BLACK     13
#define FLI_BRUN      15
#define FLI_COPY      16
#define FLI_MINI      18

// 256 RGB entries; 25% of these bytes will be unused, but it's faster
// to index 4-byte entries
#define PALETTE_SIZE 1024
static unsigned char palette[PALETTE_SIZE];

void *init_fli_decoder(int width, int height)
{
  memset(palette, 0, PALETTE_SIZE);

  return malloc(width * height * sizeof (unsigned char));
}

void decode_fli_frame(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel,
  void *context)
{
  int stream_ptr = 0;
  int stream_ptr_after_color_chunk;
  int pixel_ptr;
  int palette_ptr1;
  int palette_ptr2;
  unsigned char palette_idx1;
  unsigned char palette_idx2;

  unsigned int frame_size;
  int num_chunks;

  unsigned int chunk_size;
  int chunk_type;

  int i, j;

  int color_packets;
  int color_changes;
  int color_scale;

  int lines;
  int compressed_lines;
  int starting_line;
  signed short line_packets;
  int y_ptr;
  int line_inc = width * bytes_per_pixel;
  signed char byte_run;
  int pixel_skip;
  int update_whole_frame = 0; // Palette change flag
  unsigned char *fli_ghost_image = (unsigned char *)context;
  int ghost_pixel_ptr;
  int ghost_y_ptr;

  frame_size = LE_32(&encoded[stream_ptr]);
  stream_ptr += 6;  // skip the magic number
  num_chunks = LE_16(&encoded[stream_ptr]);
  stream_ptr += 10;  // skip padding

  // iterate through the chunks
  frame_size -= 16;
  while ((frame_size > 0) && (num_chunks > 0))
  {
    chunk_size = LE_32(&encoded[stream_ptr]);
    stream_ptr += 4;
    chunk_type = LE_16(&encoded[stream_ptr]);
    stream_ptr += 2;

    switch (chunk_type)
    {
    case FLI_256_COLOR:
    case FLI_COLOR:
      stream_ptr_after_color_chunk = stream_ptr + chunk_size - 6;
      if (chunk_type == FLI_COLOR)
        color_scale = 4;
      else
        color_scale = 1;
      // set up the palette
      color_packets = LE_16(&encoded[stream_ptr]);
      stream_ptr += 2;
      palette_ptr1 = 0;
      for (i = 0; i < color_packets; i++)
      {
        // first byte is how many colors to skip
        palette_ptr1 += (encoded[stream_ptr++] * 4);
        // wrap around, for good measure
        if (palette_ptr1 >= PALETTE_SIZE)
          palette_ptr1 = 0;
        // next byte indicates how many entries to change
        color_changes = encoded[stream_ptr++];
        // if there are 0 color changes, there are actually 256
        if (color_changes == 0)
          color_changes = 256;
        for (j = 0; j < color_changes; j++)
        {
          palette[palette_ptr1++] = encoded[stream_ptr + 2] * color_scale;
          palette[palette_ptr1++] = encoded[stream_ptr + 1] * color_scale;
          palette[palette_ptr1++] = encoded[stream_ptr + 0] * color_scale;
          palette_ptr1++;
          stream_ptr += 3;
        }
      }

      // color chunks sometimes have weird 16-bit alignment issues;
      // therefore, take the hardline approach and set the stream_ptr
      // to the value calculate w.r.t. the size specified by the color
      // chunk header
      stream_ptr = stream_ptr_after_color_chunk;

      /* Palette has changed, must update frame */
      update_whole_frame = 1;
      break;

    case FLI_DELTA:
      y_ptr = ghost_y_ptr = 0;
      compressed_lines = LE_16(&encoded[stream_ptr]);
      stream_ptr += 2;
      while (compressed_lines > 0)
      {
        line_packets = LE_16(&encoded[stream_ptr]);
        stream_ptr += 2;
        if (line_packets < 0)
        {
          line_packets = -line_packets;
          y_ptr += (line_packets * line_inc);
          ghost_y_ptr += (line_packets * width);
        }
        else
        {
          pixel_ptr = y_ptr;
          ghost_pixel_ptr = ghost_y_ptr;
          for (i = 0; i < line_packets; i++)
          {
            // account for the skip bytes
            pixel_skip = encoded[stream_ptr++];
            pixel_ptr += pixel_skip * bytes_per_pixel;
            ghost_pixel_ptr += pixel_skip;
            byte_run = encoded[stream_ptr++];
            if (byte_run < 0)
            {
              byte_run = -byte_run;
              palette_ptr1 = (palette_idx1 = encoded[stream_ptr++]) * 4;
              palette_ptr2 = (palette_idx2 = encoded[stream_ptr++]) * 4;
              for (j = 0; j < byte_run; j++)
              {
                fli_ghost_image[ghost_pixel_ptr++] = palette_idx1;
                decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;

                fli_ghost_image[ghost_pixel_ptr++] = palette_idx2;
                decoded[pixel_ptr++] = palette[palette_ptr2 + 0];
                decoded[pixel_ptr++] = palette[palette_ptr2 + 1];
                decoded[pixel_ptr++] = palette[palette_ptr2 + 2];
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;
              }
            }
            else
            {
              for (j = 0; j < byte_run * 2; j++)
              {
                palette_ptr1 = (palette_idx1 = encoded[stream_ptr++]) * 4;
                fli_ghost_image[ghost_pixel_ptr++] = palette_idx1;
                decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;
              }
            }
          }
          y_ptr += line_inc;
          ghost_y_ptr += width;
          compressed_lines--;
        }
      }
      break;

    case FLI_LC:
      // line compressed
      starting_line = LE_16(&encoded[stream_ptr]);
      stream_ptr += 2;
      y_ptr = starting_line * line_inc;
      ghost_y_ptr = starting_line * width;

      compressed_lines = LE_16(&encoded[stream_ptr]);
      stream_ptr += 2;
      while (compressed_lines > 0)
      {
        pixel_ptr = y_ptr;
        ghost_pixel_ptr = ghost_y_ptr;
        line_packets = encoded[stream_ptr++];
        if (line_packets > 0)
        {
          for (i = 0; i < line_packets; i++)
          {
            // account for the skip bytes
            pixel_skip = encoded[stream_ptr++];
            pixel_ptr += pixel_skip * bytes_per_pixel;
            ghost_pixel_ptr += pixel_skip;
            byte_run = encoded[stream_ptr++];
            if (byte_run > 0)
            {
              for (j = 0; j < byte_run; j++)
              {
                palette_ptr1 = (palette_idx1 = encoded[stream_ptr++]) * 4;
                fli_ghost_image[ghost_pixel_ptr++] = palette_idx1;
                decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;
              }
            }
            else
            {
              byte_run = -byte_run;
              palette_ptr1 = (palette_idx1 = encoded[stream_ptr++]) * 4;
              for (j = 0; j < byte_run; j++)
              {
                fli_ghost_image[ghost_pixel_ptr++] = palette_idx1;
                decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;
              }
            }
          }
        }

        y_ptr += line_inc;
        ghost_y_ptr += width;
        compressed_lines--;
      }
      break;

    case FLI_BLACK:
      // set the whole frame to color 0 (which is usually black) by
      // clearing the ghost image and trigger a full frame update
      memset(fli_ghost_image, 0, width * height * sizeof(unsigned char));
      update_whole_frame = 1;
      break;

    case FLI_BRUN:
      // byte run compression
      y_ptr = 0;
      ghost_y_ptr = 0;
      for (lines = 0; lines < height; lines++)
      {
        pixel_ptr = y_ptr;
        ghost_pixel_ptr = ghost_y_ptr;
        line_packets = encoded[stream_ptr++];
        for (i = 0; i < line_packets; i++)
        {
          byte_run = encoded[stream_ptr++];
          if (byte_run > 0)
          {
            palette_ptr1 = (palette_idx1 = encoded[stream_ptr++]) * 4;
            for (j = 0; j < byte_run; j++)
            {
              fli_ghost_image[ghost_pixel_ptr++] = palette_idx1;
              decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
              decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
              decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
	      if (bytes_per_pixel == 4) /* 32bpp */
		pixel_ptr++;
            }
          }
          else  // copy bytes if byte_run < 0
          {
            byte_run = -byte_run;
            for (j = 0; j < byte_run; j++)
            {
              palette_ptr1 = (palette_idx1 = encoded[stream_ptr++]) * 4;
              fli_ghost_image[ghost_pixel_ptr++] = palette_idx1;
              decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
              decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
              decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
	      if (bytes_per_pixel == 4) /* 32bpp */
		pixel_ptr++;
            }
          }
        }

        y_ptr += line_inc;
        ghost_y_ptr += width;
      }
      break;

    case FLI_COPY:
      // copy the chunk (uncompressed frame) to the ghost image and
      // schedule the whole frame to be updated
      if (chunk_size - 6 > width * height)
      {
        mp_msg(MSGT_DECVIDEO, MSGL_WARN,
         "FLI: in chunk FLI_COPY : source data (%d bytes) bigger than image," \
         " skipping chunk\n",
         chunk_size - 6);
         break;
      }
      else
        memcpy(fli_ghost_image, &encoded[stream_ptr], chunk_size - 6);
      stream_ptr += chunk_size - 6;
      update_whole_frame = 1;
      break;

    case FLI_MINI:
      // sort of a thumbnail? disregard this chunk...
      stream_ptr += chunk_size - 6;
      break;

    default:
      mp_msg (MSGT_DECVIDEO, MSGL_WARN,
       "FLI: Unrecognized chunk type: %d\n", chunk_type);
      break;
    }

    frame_size -= chunk_size;
    num_chunks--;
  }

  if (update_whole_frame)
  {
    pixel_ptr = ghost_pixel_ptr = 0;
    while (pixel_ptr < (width * height * bytes_per_pixel))
    {
      palette_ptr1 = fli_ghost_image[ghost_pixel_ptr++] * 4;
      decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
      decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
      decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
      if (bytes_per_pixel == 4) /* 32bpp */
        pixel_ptr++;
    }
  }

  // by the end of the chunk, the stream ptr should equal the frame 
  // size (minus 1, possibly); if it doesn't, issue a warning
  if ((stream_ptr != encoded_size) && (stream_ptr != encoded_size - 1))
    mp_msg(MSGT_DECVIDEO, MSGL_WARN,
      "  warning: processed FLI chunk where encoded size = %d\n" \
      "  and final chunk ptr = %d\n",
      encoded_size, stream_ptr);
}
