/*
    FLI Decoder for MPlayer
    
    (C) 2001 Mike Melanson
    
    32bpp support (c) alex
*/

#include "config.h"
#include "bswap.h"

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
static unsigned char palette[256 * 4];

void Decode_Fli(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel)
{
  int stream_ptr = 0;
  int pixel_ptr;
  int palette_ptr1;
  int palette_ptr2;

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
  int line_inc;
  signed char byte_run;

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
      // it seems that a color packet has to be an even number of bytes
      // so account for a pad byte
      if (stream_ptr & 0x01)
        stream_ptr++;
      break;

    case FLI_DELTA:
      line_inc = width * bytes_per_pixel;
      y_ptr = 0;
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
        }
        else
        {
          pixel_ptr = y_ptr;
          for (i = 0; i < line_packets; i++)
          {
            // account for the skip bytes
            pixel_ptr += encoded[stream_ptr++] * bytes_per_pixel;
            byte_run = encoded[stream_ptr++];
            if (byte_run < 0)
            {
              byte_run = -byte_run;
              palette_ptr1 = encoded[stream_ptr++] * 4;
              palette_ptr2 = encoded[stream_ptr++] * 4;
              for (j = 0; j < byte_run; j++)
              {
                decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;

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
                palette_ptr1 = encoded[stream_ptr++] * 4;
                decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
                decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
		if (bytes_per_pixel == 4) /* 32bpp */
		    pixel_ptr++;
              }
            }
          }
          y_ptr += line_inc;
          compressed_lines--;
        }
      }
      break;

    case FLI_LC:
      // line compressed
      line_inc = width * bytes_per_pixel;
      starting_line = LE_16(&encoded[stream_ptr]);
      stream_ptr += 2;
      y_ptr = starting_line * line_inc;

      compressed_lines = LE_16(&encoded[stream_ptr]);
      stream_ptr += 2;
      while (compressed_lines > 0)
      {
        pixel_ptr = y_ptr;
        line_packets = encoded[stream_ptr++];
        if (line_packets > 0)
        {
          for (i = 0; i < line_packets; i++)
          {
            // account for the skip bytes
            pixel_ptr += encoded[stream_ptr++] * bytes_per_pixel;
            byte_run = encoded[stream_ptr++];
            if (byte_run > 0)
            {
              for (j = 0; j < byte_run; j++)
              {
                palette_ptr1 = encoded[stream_ptr++] * 4;
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
              palette_ptr1 = encoded[stream_ptr++] * 4;
              for (j = 0; j < byte_run; j++)
              {
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
        compressed_lines--;
      }
      break;

    case FLI_BLACK:
      // set the whole frame to color 0 (which is usually black)
      for (pixel_ptr = 0; pixel_ptr < (width * height * bytes_per_pixel); pixel_ptr++)
      {
        decoded[pixel_ptr++] = palette[0];
        decoded[pixel_ptr++] = palette[1];
        decoded[pixel_ptr++] = palette[2];
	if (bytes_per_pixel == 4) /* 32bpp */
	    pixel_ptr++;
      }
      break;

    case FLI_BRUN:
      // byte run compression
      line_inc = width * bytes_per_pixel;
      y_ptr = 0;
      for (lines = 0; lines < height; lines++)
      {
        pixel_ptr = y_ptr;
        line_packets = encoded[stream_ptr++];
        for (i = 0; i < line_packets; i++)
        {
          byte_run = encoded[stream_ptr++];
          if (byte_run > 0)
          {
            palette_ptr1 = encoded[stream_ptr++] * 4;
            for (j = 0; j < byte_run; j++)
            {
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
              palette_ptr1 = encoded[stream_ptr++] * 4;
              decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
              decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
              decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
	      if (bytes_per_pixel == 4) /* 32bpp */
		pixel_ptr++;
            }
          }
        }

        y_ptr += line_inc;
      }
      break;

    case FLI_COPY:
      // copy the chunk (uncompressed frame)
      for (pixel_ptr = 0; pixel_ptr < chunk_size - 6; pixel_ptr++)
      {
        palette_ptr1 = encoded[stream_ptr++] * 4;
        decoded[pixel_ptr++] = palette[palette_ptr1 + 0];
        decoded[pixel_ptr++] = palette[palette_ptr1 + 1];
        decoded[pixel_ptr++] = palette[palette_ptr1 + 2];
        if (bytes_per_pixel == 4) /* 32bpp */
          pixel_ptr++;
      }
      break;

    case FLI_MINI:
      // sort of a thumbnail? disregard this chunk...
      stream_ptr += chunk_size - 6;
      break;

    default:
      printf ("FLI: Unrecognized chunk type: %d\n", chunk_type);
      break;
    }

    frame_size -= chunk_size;
    num_chunks--;
  }
}
