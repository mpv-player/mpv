/*
    BMP file parser for the MPlayer program
    by Mike Melanson
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

typedef struct {
  int image_size;
  int image_offset;
} bmp_image_t;

// Check if a file is a BMP file depending on whether starts with 'BM'
int bmp_check_file(demuxer_t *demuxer)
{
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, 0);

  if (stream_read_word(demuxer->stream) == (('B' << 8) | 'M'))
    return 1;
  else
    return 0;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_bmp_fill_buffer(demuxer_t *demuxer)
{
  bmp_image_t *bmp_image = (bmp_image_t *)demuxer->priv;

  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, bmp_image->image_offset);
  ds_read_packet(demuxer->video, demuxer->stream, bmp_image->image_size,
    0, bmp_image->image_offset, 1);

  return 1;
}

demuxer_t* demux_open_bmp(demuxer_t* demuxer)
{
  sh_video_t *sh_video = NULL;
  unsigned int filesize;
  unsigned int data_offset;
  bmp_image_t *bmp_image;

  // go back to the beginning
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, 2);
  filesize = stream_read_dword_le(demuxer->stream);
  stream_skip(demuxer->stream, 4);
  data_offset = stream_read_word_le(demuxer->stream);
  stream_skip(demuxer->stream, 2);

  // create a new video stream header
  sh_video = new_sh_video(demuxer, 0);

  // make sure the demuxer knows about the new video stream header
  demuxer->video->sh = sh_video;

  // make sure that the video demuxer stream header knows about its
  // parent video demuxer stream
  sh_video->ds = demuxer->video;

  // load the BITMAPINFOHEADER
  // allocate size and take the palette table into account
  sh_video->bih = (BITMAPINFOHEADER *)malloc(data_offset - 12);
  sh_video->bih->biSize = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biWidth = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biHeight = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biPlanes = stream_read_word_le(demuxer->stream);
  sh_video->bih->biBitCount = stream_read_word_le(demuxer->stream);
  sh_video->bih->biCompression = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biSizeImage = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biXPelsPerMeter = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biYPelsPerMeter = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biClrUsed = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biClrImportant = stream_read_dword_le(demuxer->stream);
  // fetch the palette
  stream_read(demuxer->stream, (unsigned char *)(sh_video->bih) + 40,
    sh_video->bih->biClrUsed * 4);

  // load the data
  bmp_image = (bmp_image_t *)malloc(sizeof(bmp_image_t));
  bmp_image->image_size = filesize - data_offset;
  bmp_image->image_offset = data_offset;

  // custom fourcc for internal MPlayer use
  sh_video->format = sh_video->bih->biCompression;

  sh_video->disp_w = sh_video->bih->biWidth;
  sh_video->disp_h = sh_video->bih->biHeight;

  // get the speed
  sh_video->fps = 2;
  sh_video->frametime = 1 / sh_video->fps;

  demuxer->priv = bmp_image;

  return demuxer;
}

void demux_close_bmp(demuxer_t* demuxer) {
  bmp_image_t *bmp_image = demuxer->priv;

  if(!bmp_image)
    return;
  free(bmp_image);
}
