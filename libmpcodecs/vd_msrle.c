#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"Microsoft RLE decoder",
	"msrle",
	"Mike Melanson",
	"Mike Melanson",
	"native codec"
};

LIBVD_EXTERN(msrle)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
  unsigned char *palette_map = NULL;
  unsigned char *orig_map = (unsigned char *)sh->bih+40;
  int i;
  unsigned short color;
  unsigned char r, g, b;
  int bits_per_pixel = sh->codec->outfmt[sh->outfmtidx] & 255;

  // convert the palette for the requested output format
  switch (bits_per_pixel)
  {
    case 15:
    case 16:
      if ((palette_map =
        (unsigned char *)malloc(sh->bih->biClrUsed * 2)) == NULL)
        return 0;

      for (i = 0; i < sh->bih->biClrUsed; i++)
      {
        r = orig_map[i * 4 + 2];
        g = orig_map[i * 4 + 1];
        b = orig_map[i * 4 + 0];
        if (bits_per_pixel == 15)
          color = ((r>>3)<<10) | ((g>>3)<<5) | ((b>>3));
        else
          color = ((r>>3)<<11) | ((g>>2)<<5) | ((b>>3));
        palette_map[i * 2 + 1] = color >> 8;
        palette_map[i * 2 + 0] = color & 0xFF;
      }
      break;

    case 24:
    case 32:
      if ((palette_map =
        (unsigned char *)malloc(sh->bih->biClrUsed * 4)) == NULL)
        return 0;
      memcpy(palette_map, orig_map, sh->bih->biClrUsed * 4);
      break;
  }

  sh->context = palette_map;

  return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR24);
}

// uninit driver
static void uninit(sh_video_t *sh){
  unsigned char *palette_map = (unsigned char *)sh->context;

  free(palette_map);
}

#define FETCH_NEXT_STREAM_BYTE() \
    if (stream_ptr >= encoded_size) \
    { \
      mp_msg(MSGT_DECVIDEO, MSGL_WARN, \
        "MS RLE: stream ptr just went out of bounds (1)\n"); \
      return; \
    } \
    stream_byte = encoded[stream_ptr++];

void decode_msrle4(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  unsigned char *palette_map,
  int bits_per_pixel)
{
  int bytes_per_pixel = (bits_per_pixel + 1) / 8;
  unsigned char r1, g1, b1;  // for 24/32 bpp
  unsigned char r2, g2, b2;
  unsigned char color_hi1, color_lo1;  // for 15/16 bpp
  unsigned char color_hi2, color_lo2;
  int stream_ptr = 0;
  unsigned char rle_code;
  unsigned char extra_byte;
  unsigned char stream_byte;
  int frame_size = width * height * bytes_per_pixel;
  int pixel_ptr = 0;
  int row_dec = width * bytes_per_pixel;
  int row_ptr = (height - 1) * row_dec;
  int i;

  r1 = r2 = g1 = g2 = b1 = b2 = 
    color_hi1 = color_hi2 = color_lo1 = color_lo2 = 0;
  while (row_ptr >= 0)
  {
    FETCH_NEXT_STREAM_BYTE();
    rle_code = stream_byte;
    if (rle_code == 0)
    {
      // fetch the next byte to see how to handle escape code
      FETCH_NEXT_STREAM_BYTE();
      if (stream_byte == 0)
      {
        // line is done, goto the next one
        row_ptr -= row_dec;
        pixel_ptr = 0;
      }
      else if (stream_byte == 1)
        // decode is done
        return;
      else if (stream_byte == 2)
      {
        // reposition frame decode coordinates
        FETCH_NEXT_STREAM_BYTE();
        pixel_ptr += stream_byte * bytes_per_pixel;
        FETCH_NEXT_STREAM_BYTE();
        row_ptr -= stream_byte * row_dec;
      }
      else
      {
        // copy pixels from encoded stream
        rle_code = stream_byte;
        rle_code /= 2;
        extra_byte = rle_code & 0x01;
        if ((row_ptr + pixel_ptr + rle_code * bytes_per_pixel > frame_size) ||
            (row_ptr < 0))
        {
          mp_msg(MSGT_DECVIDEO, MSGL_WARN,
            "MS RLE: frame ptr just went out of bounds (1)\n");
          return;
        }

        for (i = 0; i < rle_code; i++)
        {
          if (pixel_ptr >= row_dec)
            break;
          if (bytes_per_pixel == 2)
          {
            color_hi1 = palette_map[(encoded[stream_ptr + i] >> 4) * 2 + 0];
            color_lo1 = palette_map[(encoded[stream_ptr + i] >> 4) * 2 + 1];
            decoded[row_ptr + pixel_ptr + 0] = color_hi1;
            decoded[row_ptr + pixel_ptr + 1] = color_lo1;
          }
          else
          {
            r1 = palette_map[(encoded[stream_ptr + i] >> 4) * 4 + 2];
            g1 = palette_map[(encoded[stream_ptr + i] >> 4) * 4 + 1];
            b1 = palette_map[(encoded[stream_ptr + i] >> 4) * 4 + 0];
            decoded[row_ptr + pixel_ptr + 0] = b1;
            decoded[row_ptr + pixel_ptr + 1] = g1;
            decoded[row_ptr + pixel_ptr + 2] = r1;
          }
          pixel_ptr += bytes_per_pixel;

          if (pixel_ptr >= row_dec)
            break;
          if (bytes_per_pixel == 2)
          {
            color_hi1 = palette_map[(encoded[stream_ptr + i] & 0x0F) * 2 + 0];
            color_lo1 = palette_map[(encoded[stream_ptr + i] & 0x0F) * 2 + 1];
            decoded[row_ptr + pixel_ptr + 0] = color_hi1;
            decoded[row_ptr + pixel_ptr + 1] = color_lo1;
          }
          else
          {
            r1 = palette_map[(encoded[stream_ptr + i] & 0x0F) * 4 + 2];
            g1 = palette_map[(encoded[stream_ptr + i] & 0x0F) * 4 + 1];
            b1 = palette_map[(encoded[stream_ptr + i] & 0x0F) * 4 + 0];
            decoded[row_ptr + pixel_ptr + 0] = b1;
            decoded[row_ptr + pixel_ptr + 1] = g1;
            decoded[row_ptr + pixel_ptr + 2] = r1;
          }
          pixel_ptr += bytes_per_pixel;
        }
        stream_ptr += rle_code;

        // if the RLE code is odd, skip a byte in the stream
        if (extra_byte)
          stream_ptr++;
      }
    }
    else
    {
      // decode a run of data
      FETCH_NEXT_STREAM_BYTE();
      if (bytes_per_pixel == 2)
      {
        color_hi1 = palette_map[(stream_byte >> 4) * 2 + 0];
        color_lo1 = palette_map[(stream_byte >> 4) * 2 + 1];
        color_hi2 = palette_map[(stream_byte & 0x0F) * 2 + 0];
        color_lo2 = palette_map[(stream_byte & 0x0F) * 2 + 1];
      }
      else
      {
        r1 = palette_map[(stream_byte >> 4) * 4 + 2];
        g1 = palette_map[(stream_byte >> 4) * 4 + 1];
        b1 = palette_map[(stream_byte >> 4) * 4 + 0];
        r2 = palette_map[(stream_byte & 0x0F) * 4 + 2];
        g2 = palette_map[(stream_byte & 0x0F) * 4 + 1];
        b2 = palette_map[(stream_byte & 0x0F) * 4 + 0];
      }
      for (i = 0; i < rle_code; i++)
      {
        if (pixel_ptr >= row_dec)
          break;

        if ((i & 1) == 0)
        {
          if (bytes_per_pixel == 2)
          {
            decoded[row_ptr + pixel_ptr + 0] = color_hi1;
            decoded[row_ptr + pixel_ptr + 1] = color_lo1;
          }
          else
          {
            decoded[row_ptr + pixel_ptr + 0] = b1;
            decoded[row_ptr + pixel_ptr + 1] = g1;
            decoded[row_ptr + pixel_ptr + 2] = r1;
          }
        }
        else
        {
          if (bytes_per_pixel == 2)
          {
            decoded[row_ptr + pixel_ptr + 0] = color_hi2;
            decoded[row_ptr + pixel_ptr + 1] = color_lo2;
          }
          else
          {
            decoded[row_ptr + pixel_ptr + 0] = b2;
            decoded[row_ptr + pixel_ptr + 1] = g2;
            decoded[row_ptr + pixel_ptr + 2] = r2;
          }
        }
        pixel_ptr += bytes_per_pixel;
      }
    }
  }

  // one last sanity check on the way out
  if (stream_ptr < encoded_size)
    mp_msg(MSGT_DECVIDEO, MSGL_WARN,
      "MS RLE: ended frame decode with bytes left over (%d < %d)\n",
      stream_ptr, encoded_size);
}

void decode_msrle8(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  unsigned char *palette_map,
  int bits_per_pixel)
{
  int bytes_per_pixel = (bits_per_pixel + 1) / 8;
  unsigned char r, g, b;  // for 24/32 bpp
  unsigned char color_hi, color_lo;  // for 15/16 bpp
  int stream_ptr = 0;
  unsigned char rle_code;
  unsigned char extra_byte;
  unsigned char stream_byte;
  int frame_size = width * height * bytes_per_pixel;
  int pixel_ptr = 0;
  int row_dec = width * bytes_per_pixel;
  int row_ptr = (height - 1) * row_dec;

  r = g = b = color_hi = color_lo = 0;
  while (row_ptr >= 0)
  {
    FETCH_NEXT_STREAM_BYTE();
    rle_code = stream_byte;
    if (rle_code == 0)
    {
      // fetch the next byte to see how to handle escape code
      FETCH_NEXT_STREAM_BYTE();
      if (stream_byte == 0)
      {
        // line is done, goto the next one
        row_ptr -= row_dec;
        pixel_ptr = 0;
      }
      else if (stream_byte == 1)
        // decode is done
        return;
      else if (stream_byte == 2)
      {
        // reposition frame decode coordinates
        FETCH_NEXT_STREAM_BYTE();
        pixel_ptr += stream_byte * bytes_per_pixel;
        FETCH_NEXT_STREAM_BYTE();
        row_ptr -= stream_byte * row_dec;
      }
      else
      {
        // copy pixels from encoded stream
        if ((row_ptr + pixel_ptr + stream_byte * bytes_per_pixel > frame_size) ||
            (row_ptr < 0))
        {
          mp_msg(MSGT_DECVIDEO, MSGL_WARN,
            "MS RLE: frame ptr just went out of bounds (1)\n");
          return;
        }

        rle_code = stream_byte;
        extra_byte = stream_byte & 0x01;
        if (stream_ptr + rle_code + extra_byte > encoded_size)
        {
          mp_msg(MSGT_DECVIDEO, MSGL_WARN,
            "MS RLE: stream ptr just went out of bounds (2)\n");
          return;
        }

        while (rle_code--)
        {
          FETCH_NEXT_STREAM_BYTE();
          if (bytes_per_pixel == 2)
          {
            color_hi = palette_map[stream_byte * 2 + 0];
            color_lo = palette_map[stream_byte * 2 + 1];
            decoded[row_ptr + pixel_ptr + 0] = color_hi;
            decoded[row_ptr + pixel_ptr + 1] = color_lo;
          }
          else
          {
            r = palette_map[stream_byte * 4 + 2];
            g = palette_map[stream_byte * 4 + 1];
            b = palette_map[stream_byte * 4 + 0];
            decoded[row_ptr + pixel_ptr + 0] = b;
            decoded[row_ptr + pixel_ptr + 1] = g;
            decoded[row_ptr + pixel_ptr + 2] = r;
          }
          pixel_ptr += bytes_per_pixel;
        }

        // if the RLE code is odd, skip a byte in the stream
        if (extra_byte)
          stream_ptr++;
      }
    }
    else
    {
      // decode a run of data
      if ((row_ptr + pixel_ptr + stream_byte * bytes_per_pixel > frame_size) ||
          (row_ptr < 0))
      {
        mp_msg(MSGT_DECVIDEO, MSGL_WARN,
          "MS RLE: frame ptr just went out of bounds (2)\n");
        return;
      }

      FETCH_NEXT_STREAM_BYTE();

      if (bytes_per_pixel == 2)
      {
        color_hi = palette_map[stream_byte * 2 + 0];
        color_lo = palette_map[stream_byte * 2 + 1];
      }
      else
      {
        r = palette_map[stream_byte * 4 + 2];
        g = palette_map[stream_byte * 4 + 1];
        b = palette_map[stream_byte * 4 + 0];
      }
      while(rle_code--)
      {
        if (bytes_per_pixel == 2)
        {
          decoded[row_ptr + pixel_ptr + 0] = color_hi;
          decoded[row_ptr + pixel_ptr + 1] = color_lo;
        }
        else
        {
          decoded[row_ptr + pixel_ptr + 0] = b;
          decoded[row_ptr + pixel_ptr + 1] = g;
          decoded[row_ptr + pixel_ptr + 2] = r;
        }
        pixel_ptr += bytes_per_pixel;
      }
    }
  }

  // one last sanity check on the way out
  if (stream_ptr < encoded_size)
    mp_msg(MSGT_DECVIDEO, MSGL_WARN,
      "MS RLE: ended frame decode with bytes left over (%d < %d)\n",
      stream_ptr, encoded_size);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    if (sh->format == 1)
      decode_msrle8(
        data,len, mpi->planes[0],
        sh->disp_w, sh->disp_h,
        (unsigned char *)sh->context,
        mpi->imgfmt & 255);
    else if (sh->format == 2)
      decode_msrle4(
        data,len, mpi->planes[0],
        sh->disp_w, sh->disp_h,
        (unsigned char *)sh->context,
        mpi->imgfmt & 255);
    else
      mp_msg(MSGT_DECVIDEO, MSGL_WARN,
        "MS RLE: Don't know how to decode format %08X", sh->format);
    
    return mpi;
}
