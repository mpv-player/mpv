/*
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "core/m_option.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"

#include "video/img_format.h"
#include "video/img_fourcc.h"

static int format = MP_FOURCC_I420;
static int mp_format;
static char *codec;
static int width = 0;
static int height = 0;
static float fps = 25;
static int imgsize=0;

const m_option_t demux_rawvideo_opts[] = {
  // size:
  { "w", &width, CONF_TYPE_INT,CONF_RANGE,1,8192, NULL },
  { "h", &height, CONF_TYPE_INT,CONF_RANGE,1,8192, NULL },
  // format:
  { "format", &format, CONF_TYPE_FOURCC, 0, 0 , 0, NULL },
  { "mp-format", &mp_format, CONF_TYPE_IMGFMT, 0, 0 , 0, NULL },
  { "codec", &codec, CONF_TYPE_STRING, 0, 0 , 0, NULL },
  // misc:
  { "fps", &fps, CONF_TYPE_FLOAT,CONF_RANGE,0.001,1000, NULL },
  { "size", &imgsize, CONF_TYPE_INT, CONF_RANGE, 1 , 8192*8192*4, NULL },

  {NULL, NULL, 0, 0, 0, 0, NULL}
};


static demuxer_t* demux_rawvideo_open(demuxer_t* demuxer) {
  sh_video_t* sh_video;

  if(!width || !height){
      mp_msg(MSGT_DEMUX,MSGL_ERR,"rawvideo: width or height not specified!\n");
      return 0;
  }

  const char *decoder = "rawvideo";
  int imgfmt = format;
  if (mp_format) {
    decoder = "mp-rawvideo";
    imgfmt = mp_format;
    if (!imgsize) {
      struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(mp_format);
      for (int p = 0; p < desc.num_planes; p++) {
        imgsize += ((width >> desc.xs[p]) * (height >> desc.ys[p]) *
                    desc.bpp[p] + 7) / 8;
      }
    }
  } else if (codec && codec[0]) {
    decoder = talloc_strdup(demuxer, codec);
  }

  if (!imgsize) {
    int bpp = 0;
    switch(format){
    case MP_FOURCC_I420: case MP_FOURCC_IYUV:
    case MP_FOURCC_NV12: case MP_FOURCC_NV21:
    case MP_FOURCC_HM12:
    case MP_FOURCC_YV12:
      bpp = 12;
      break;
    case MP_FOURCC_RGB12: case MP_FOURCC_BGR12:
    case MP_FOURCC_RGB15: case MP_FOURCC_BGR15:
    case MP_FOURCC_RGB16: case MP_FOURCC_BGR16:
    case MP_FOURCC_YUY2:  case MP_FOURCC_UYVY:
      bpp = 16;
      break;
    case MP_FOURCC_RGB8: case MP_FOURCC_BGR8:
    case MP_FOURCC_Y800: case MP_FOURCC_Y8:
      bpp = 8;
      break;
    case MP_FOURCC_RGB24: case MP_FOURCC_BGR24:
      bpp = 24;
      break;
    case MP_FOURCC_RGB32: case MP_FOURCC_BGR32:
      bpp = 32;
      break;
    }
    if (!bpp) {
      mp_msg(MSGT_DEMUX,MSGL_ERR,"rawvideo: img size not specified and unknown format!\n");
      return 0;
    }
    imgsize = width * height * bpp / 8;
  }

  sh_video = new_sh_video(demuxer,0);
  sh_video->gsh->codec=decoder;
  sh_video->format=imgfmt;
  sh_video->fps=fps;
  sh_video->frametime=1.0/fps;
  sh_video->disp_w=width;
  sh_video->disp_h=height;
  sh_video->i_bps=fps*imgsize;

  demuxer->movi_start = demuxer->stream->start_pos;
  demuxer->movi_end = demuxer->stream->end_pos;

  demuxer->video->sh = sh_video;
  sh_video->ds = demuxer->video;

  return demuxer;
}

static int demux_rawvideo_fill_buffer(demuxer_t* demuxer, demux_stream_t *ds) {
  sh_video_t* sh = demuxer->video->sh;
  int64_t pos;
  if(demuxer->stream->eof) return 0;
  if(ds!=demuxer->video) return 0;
  pos = stream_tell(demuxer->stream);
  ds_read_packet(ds,demuxer->stream,imgsize,(pos/imgsize)*sh->frametime,pos,0x10);
  return 1;
}

static void demux_rawvideo_seek(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags){
  stream_t* s = demuxer->stream;
  sh_video_t* sh_video = demuxer->video->sh;
  int64_t pos;

  pos = (flags & SEEK_ABSOLUTE) ? demuxer->movi_start : stream_tell(s);
  if(flags & SEEK_FACTOR)
    pos += ((demuxer->movi_end - demuxer->movi_start)*rel_seek_secs);
  else
    pos += (rel_seek_secs*sh_video->i_bps);
  if(pos < 0) pos = 0;
  if(demuxer->movi_end && pos > demuxer->movi_end) pos = (demuxer->movi_end-imgsize);
  pos/=imgsize;
  stream_seek(s,pos*imgsize);
  //sh_video->timer=pos * sh_video->frametime;
  demuxer->video->pts = pos * sh_video->frametime;
//  printf("demux_rawvideo: streamtell=%d\n",(int)stream_tell(demuxer->stream));
}


const demuxer_desc_t demuxer_desc_rawvideo = {
  "Raw video demuxer",
  "rawvideo",
  "rawvideo",
  "?",
  "",
  DEMUXER_TYPE_RAWVIDEO,
  0, // no autodetect
  NULL,
  demux_rawvideo_fill_buffer,
  demux_rawvideo_open,
  NULL,
  demux_rawvideo_seek,
  NULL
};
