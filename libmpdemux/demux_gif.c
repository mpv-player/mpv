/*
	GIF file parser for MPlayer
	by Joey Parrish
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#ifdef HAVE_GIF

#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include <gif_lib.h>
static int current_pts = 0;
static unsigned char *pallete = NULL;

#define GIF_SIGNATURE (('G' << 16) | ('I' << 8) | 'F')

int my_read_gif(GifFileType *gif, uint8_t *buf, int len) {
  return stream_read(gif->UserData, buf, len);
}
  
int gif_check_file(demuxer_t *demuxer)
{
  if (stream_read_int24(demuxer->stream) == GIF_SIGNATURE)
    return 1;
  return 0;
}

int demux_gif_fill_buffer(demuxer_t *demuxer)
{
  GifFileType *gif = (GifFileType *)demuxer->priv;
  sh_video_t *sh_video = (sh_video_t *)demuxer->video->sh;
  GifRecordType type = UNDEFINED_RECORD_TYPE;
  int len = 0;
  demux_packet_t *dp = NULL;
  ColorMapObject *effective_map = NULL;
  char *buf = NULL;

  while (type != IMAGE_DESC_RECORD_TYPE) {
    if (DGifGetRecordType(gif, &type) == GIF_ERROR) {
      PrintGifError();
      return 0; // oops
    }
    if (type == TERMINATE_RECORD_TYPE)
      return 0; // eof
    if (type == SCREEN_DESC_RECORD_TYPE) {
      if (DGifGetScreenDesc(gif) == GIF_ERROR) {
        PrintGifError();
        return 0; // oops
      }
    }
    if (type == EXTENSION_RECORD_TYPE) {
      int code;
      unsigned char *p = NULL;
      if (DGifGetExtension(gif, &code, &p) == GIF_ERROR) {
        PrintGifError();
        return 0; // oops
      }
      if (code == 0xF9) {
        int frametime = 0;
        if (p[0] == 4) // is the length correct?
          frametime = (p[1] << 8) | p[2]; // set the time, centiseconds
        current_pts += frametime;
      } else if ((code == 0xFE) && (verbose)) { // comment extension
	// print iff verbose
	printf("GIF comment: ");
        while (p != NULL) {
          int length = p[0];
	  char *comments = p + 1;
	  comments[length] = 0;
	  printf("%s", comments);
          if (DGifGetExtensionNext(gif, &p) == GIF_ERROR) {
            PrintGifError();
            return 0; // oops
          }
	}
	printf("\n");
      }
      while (p != NULL) {
        if (DGifGetExtensionNext(gif, &p) == GIF_ERROR) {
          PrintGifError();
          return 0; // oops
        }
      }
    }
  }
  
  if (DGifGetImageDesc(gif) == GIF_ERROR) {
    PrintGifError();
    return 0; // oops
  }

  len = gif->Image.Width * gif->Image.Height;
  dp = new_demux_packet(len);
  buf = malloc(len);
  memset(buf, 0, len);
  memset(dp->buffer, 0, len);
  
  if (DGifGetLine(gif, buf, len) == GIF_ERROR) {
    PrintGifError();
    return 0; // oops
  }

  effective_map = gif->Image.ColorMap;
  if (effective_map == NULL) effective_map = gif->SColorMap;

  {
    int y;

    // copy the pallete
    for (y = 0; y < 256; y++) {
	pallete[(y * 4) + 0] = effective_map->Colors[y].Blue;
	pallete[(y * 4) + 1] = effective_map->Colors[y].Green;
	pallete[(y * 4) + 2] = effective_map->Colors[y].Red;
	pallete[(y * 4) + 3] = 0;
    }

    for (y = 0; y < gif->Image.Height; y++) {
      unsigned char *drow = dp->buffer;
      unsigned char *gbuf = buf + (y * gif->Image.Width);

      drow += gif->Image.Width * (y + gif->Image.Top);
      drow += gif->Image.Left;

      memcpy(drow, gbuf, gif->Image.Width);
    }
  }

  free(buf);

  demuxer->video->dpos++;
  dp->pts = ((float)current_pts) / 100;
  dp->pos = stream_tell(demuxer->stream);
  ds_add_packet(demuxer->video, dp);

  return 1;
}

demuxer_t* demux_open_gif(demuxer_t* demuxer)
{
  sh_video_t *sh_video = NULL;
  GifFileType *gif = NULL;

  current_pts = 0;
  demuxer->seekable = 0; // FIXME

  // go back to the beginning
  stream_seek(demuxer->stream,demuxer->stream->start_pos);

  gif = DGifOpen(demuxer->stream, my_read_gif);
  if (!gif) {
    PrintGifError();
    return NULL;
  }

  // create a new video stream header
  sh_video = new_sh_video(demuxer, 0);

  // make sure the demuxer knows about the new video stream header
  // (even though new_sh_video() ought to take care of it)
  demuxer->video->sh = sh_video;

  // make sure that the video demuxer stream header knows about its
  // parent video demuxer stream (this is getting wacky), or else
  // video_read_properties() will choke
  sh_video->ds = demuxer->video;

  sh_video->disp_w = gif->SWidth;
  sh_video->disp_h = gif->SHeight;

  sh_video->format = mmioFOURCC(8, 'R', 'G', 'B');
  
  sh_video->fps = 5.0f;
  sh_video->frametime = 1.0f / sh_video->fps;
  
  sh_video->bih = malloc(sizeof(BITMAPINFOHEADER) + (256 * 4));
  sh_video->bih->biCompression = sh_video->format;
  sh_video->bih->biBitCount = 8;
  sh_video->bih->biPlanes = 2;
  pallete = (unsigned char *)(sh_video->bih + 1);
  
  demuxer->priv = gif;

  return demuxer;
}

void demux_close_gif(demuxer_t* demuxer)
{
  GifFileType *gif = (GifFileType *)demuxer->priv;

  if(!gif)
    return;

  if (DGifCloseFile(gif) == GIF_ERROR)
    PrintGifError();
  
  demuxer->stream->fd = 0;
  demuxer->priv = NULL;
}
#endif /* HAVE_GIF */
