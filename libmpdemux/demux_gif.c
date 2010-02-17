/*
 * GIF file parser
 * Copyright (C) 2003 Joey Parrish
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#include <gif_lib.h>
#include "libvo/fastmemcpy.h"
typedef struct {
  int current_pts;
  unsigned char *palette;
  GifFileType *gif;
  int w, h;
  int useref;
  uint8_t *refimg;
} gif_priv_t;

#define GIF_SIGNATURE (('G' << 16) | ('I' << 8) | 'F')

#ifndef CONFIG_GIF_TVT_HACK
// not supported by certain versions of the library
static int my_read_gif(GifFileType *gif, uint8_t *buf, int len)
{
  return stream_read(gif->UserData, buf, len);
}
#endif

static int gif_check_file(demuxer_t *demuxer)
{
  if (stream_read_int24(demuxer->stream) == GIF_SIGNATURE)
    return DEMUXER_TYPE_GIF;
  return 0;
}

static void memcpy_transp_pic(uint8_t *dst, uint8_t *src, int w, int h,
                int dstride, int sstride, int transp, uint8_t trans_col) {
  if (transp) {
    dstride -= w;
    sstride -= w;
    while (h-- > 0) {
      int wleft = w;
      while (wleft-- > 0) {
        if (*src != trans_col)
          *dst = *src;
        dst++; src++;
      }
      dst += dstride;
      src += sstride;
    }
  } else
    memcpy_pic(dst, src, w, h, dstride, sstride);
}

static int demux_gif_fill_buffer(demuxer_t *demuxer, demux_stream_t *ds)
{
  gif_priv_t *priv = demuxer->priv;
  GifFileType *gif = priv->gif;
  GifRecordType type = UNDEFINED_RECORD_TYPE;
  int len = 0;
  demux_packet_t *dp = NULL;
  ColorMapObject *effective_map = NULL;
  uint8_t *buf = NULL;
  int refmode = 0;
  int transparency = 0;
  uint8_t transparent_col;

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
        {
          transparency = p[1] & 1;
          refmode = (p[1] >> 2) & 3;
          // HACK: specification says
          // > 0 - No disposal specified. The decoder is not required to take any action.
          // but browsers treat it the same way as
          // > 1 - Do not dispose. The graphic is to be left in place.
          // Some broken files rely on this, e.g.
          // http://samples.mplayerhq.hu/GIF/broken-gif/CLAIRE.GIF
          if (refmode == 0) refmode = 1;
          frametime = (p[3] << 8) | p[2]; // set the time, centiseconds
          transparent_col = p[4];
        }
        priv->current_pts += frametime;
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
  dp = new_demux_packet(priv->w * priv->h);
  buf = calloc(gif->Image.Width, gif->Image.Height);
  if (priv->useref)
    fast_memcpy(dp->buffer, priv->refimg, priv->w * priv->h);
  else
    memset(dp->buffer, gif->SBackGroundColor, priv->w * priv->h);

  if (DGifGetLine(gif, buf, len) == GIF_ERROR) {
    PrintGifError();
    return 0; // oops
  }

  effective_map = gif->Image.ColorMap;
  if (effective_map == NULL) effective_map = gif->SColorMap;

  {
    int y;
    int cnt = FFMIN(effective_map->ColorCount, 256);
    int l = av_clip(gif->Image.Left, 0, priv->w);
    int t = av_clip(gif->Image.Top, 0, priv->h);
    int w = av_clip(gif->Image.Width, 0, priv->w - l);
    int h = av_clip(gif->Image.Height, 0, priv->h - t);
    unsigned char *dest = dp->buffer + priv->w * t + l;

    // copy the palette
    for (y = 0; y < cnt; y++) {
      priv->palette[(y * 4) + 0] = effective_map->Colors[y].Blue;
      priv->palette[(y * 4) + 1] = effective_map->Colors[y].Green;
      priv->palette[(y * 4) + 2] = effective_map->Colors[y].Red;
      priv->palette[(y * 4) + 3] = 0;
    }

    if (gif->Image.Interlace) {
      uint8_t *s = buf;
      int ih = (h - 0 + 7) >> 3;
      memcpy_transp_pic(dest, s, w, ih,
                        priv->w << 3, gif->Image.Width,
                        transparency, transparent_col);
      s += ih * w;
      ih = (h - 4 + 7) >> 3;
      memcpy_transp_pic(dest + (priv->w << 2), s, w, ih,
                        priv->w << 3, gif->Image.Width,
                        transparency, transparent_col);
      s += ih * w;
      ih = (h - 2 + 3) >> 2;
      memcpy_transp_pic(dest + (priv->w << 1), s, w, ih,
                        priv->w << 2, gif->Image.Width,
                        transparency, transparent_col);
      s += ih * w;
      ih = (h - 1 + 1) >> 1;
      memcpy_transp_pic(dest + priv->w, s, w, ih,
                        priv->w << 1, gif->Image.Width,
                        transparency, transparent_col);
    } else
      memcpy_transp_pic(dest, buf, w, h, priv->w, gif->Image.Width,
                        transparency, transparent_col);

    if (refmode == 1) fast_memcpy(priv->refimg, dp->buffer, priv->w * priv->h);
    if (refmode == 2 && priv->useref) {
      dest = priv->refimg + priv->w * t + l;
      memset(buf, gif->SBackGroundColor, len);
      memcpy_pic(dest, buf, w, h, priv->w, gif->Image.Width);
    }
    if (!(refmode & 2)) priv->useref = refmode & 1;
  }

  free(buf);

  demuxer->video->dpos++;
  dp->pts = ((float)priv->current_pts) / 100;
  dp->pos = stream_tell(demuxer->stream);
  ds_add_packet(demuxer->video, dp);

  return 1;
}

static demuxer_t* demux_open_gif(demuxer_t* demuxer)
{
  gif_priv_t *priv = calloc(1, sizeof(gif_priv_t));
  sh_video_t *sh_video = NULL;
  GifFileType *gif = NULL;

  priv->current_pts = 0;
  demuxer->seekable = 0; // FIXME

  // go back to the beginning
  stream_seek(demuxer->stream,demuxer->stream->start_pos);

#ifdef CONFIG_GIF_TVT_HACK
  // without the TVT functionality of libungif, a hard seek must be
  // done to the beginning of the file.  this is because libgif is
  // unable to use mplayer's cache, and without this lseek libgif will
  // not read from the beginning of the file and the command will fail.
  // with this hack enabled, you will lose the ability to stream a GIF.
  lseek(demuxer->stream->fd, 0, SEEK_SET);
  gif = DGifOpenFileHandle(demuxer->stream->fd);
#else
  gif = DGifOpen(demuxer->stream, my_read_gif);
#endif
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

  sh_video->format = mmioFOURCC(8, 'R', 'G', 'B');

  sh_video->fps = 5.0f;
  sh_video->frametime = 1.0f / sh_video->fps;

  sh_video->bih = malloc(sizeof(BITMAPINFOHEADER) + (256 * 4));
  sh_video->bih->biCompression = sh_video->format;
  sh_video->bih->biWidth = priv->w = (uint16_t)gif->SWidth;
  sh_video->bih->biHeight = priv->h = (uint16_t)gif->SHeight;
  sh_video->bih->biBitCount = 8;
  sh_video->bih->biPlanes = 2;
  priv->palette = (unsigned char *)(sh_video->bih + 1);
  priv->refimg = malloc(priv->w * priv->h);

  priv->gif = gif;
  demuxer->priv = priv;

  return demuxer;
}

static void demux_close_gif(demuxer_t* demuxer)
{
  gif_priv_t *priv = demuxer->priv;
  if (!priv) return;
  if (priv->gif && DGifCloseFile(priv->gif) == GIF_ERROR)
    PrintGifError();
  free(priv->refimg);
  free(priv);
}


const demuxer_desc_t demuxer_desc_gif = {
  "GIF demuxer",
  "gif",
  "GIF",
  "Joey Parrish",
  "",
  DEMUXER_TYPE_GIF,
  0, // unsafe autodetect
  gif_check_file,
  demux_gif_fill_buffer,
  demux_open_gif,
  demux_close_gif,
  NULL,
  NULL
};
