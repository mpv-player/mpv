
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "m_option.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "libmpcodecs/img_format.h"

extern int demuxer_type;
static int format = IMGFMT_I420;
static int size_id = 0;
static int width = 0;
static int height = 0;
static float fps = 25;
static int imgsize=0;

m_option_t demux_rawvideo_opts[] = {
  // size:
  { "w", &width, CONF_TYPE_INT,CONF_RANGE,1,8192, NULL },
  { "h", &height, CONF_TYPE_INT,CONF_RANGE,1,8192, NULL },
  { "sqcif", &size_id, CONF_TYPE_FLAG,0,0,1, NULL },
  { "qcif", &size_id, CONF_TYPE_FLAG,0,0,2, NULL },
  { "cif", &size_id, CONF_TYPE_FLAG,0,0,3, NULL },
  { "4cif", &size_id, CONF_TYPE_FLAG,0,0,4, NULL },
  { "pal", &size_id, CONF_TYPE_FLAG,0,0,5, NULL },
  { "ntsc", &size_id, CONF_TYPE_FLAG,0,0,6, NULL },
  { "16cif", &size_id, CONF_TYPE_FLAG,0,0,7, NULL },
  { "sif", &size_id, CONF_TYPE_FLAG,0,0,8, NULL },
  // format:
  { "format", &format, CONF_TYPE_IMGFMT, 0, 0 , 0, NULL },
  // below options are obsolete
  { "i420", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_I420, NULL },
  { "yv12", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_YV12, NULL },
  { "nv12", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_NV12, NULL },
  { "hm12", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_HM12, NULL },
  { "yuy2", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_YUY2, NULL },
  { "uyvy", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_UYVY, NULL },
  { "y8", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_Y8, NULL },
  // misc:
  { "fps", &fps, CONF_TYPE_FLOAT,CONF_RANGE,0.001,1000, NULL },
  { "size", &imgsize, CONF_TYPE_INT, CONF_RANGE, 1 , 8192*8192*4, NULL },

  {NULL, NULL, 0, 0, 0, 0, NULL}
};


static demuxer_t* demux_rawvideo_open(demuxer_t* demuxer) {
  sh_video_t* sh_video;

  switch(size_id){
  case 1: width=128; height=96; break;
  case 2: width=176; height=144; break;
  case 3: width=352; height=288; break;
  case 4: width=704; height=576; break;
  case 5: width=720; height=576; break;
  case 6: width=720; height=480; break;
  case 7: width=1408;height=1152;break;
  case 8: width=352; height=240; break;
  }
  if(!width || !height){
      mp_msg(MSGT_DEMUX,MSGL_ERR,"rawvideo: width or height not specified!\n");
      return 0;
  }

  if(!imgsize)
  switch(format){
  case IMGFMT_I420:
  case IMGFMT_IYUV:
  case IMGFMT_NV12:
  case IMGFMT_HM12:
  case IMGFMT_YV12: imgsize=width*height+2*(width>>1)*(height>>1);break;
  case IMGFMT_YUY2: imgsize=width*height*2;break;
  case IMGFMT_UYVY: imgsize=width*height*2;break;
  case IMGFMT_Y8: imgsize=width*height;break;
  default:
      if (IMGFMT_IS_RGB(format))
        imgsize = width * height * ((IMGFMT_RGB_DEPTH(format) + 7) >> 3);
      else if (IMGFMT_IS_BGR(format))
        imgsize = width * height * ((IMGFMT_BGR_DEPTH(format) + 7) >> 3);
      else {
      mp_msg(MSGT_DEMUX,MSGL_ERR,"rawvideo: img size not specified and unknown format!\n");
      return 0;
      }
  }

  sh_video = new_sh_video(demuxer,0);
  sh_video->format=format;
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
  off_t pos;
  if(demuxer->stream->eof) return 0;
  if(ds!=demuxer->video) return 0;
  pos = stream_tell(demuxer->stream);
  ds_read_packet(ds,demuxer->stream,imgsize,(pos/imgsize)*sh->frametime,pos,0x10);
  return 1;
}

static void demux_rawvideo_seek(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags){
  stream_t* s = demuxer->stream;
  sh_video_t* sh_video = demuxer->video->sh;
  off_t pos;

  pos = (flags & 1) ? demuxer->movi_start : stream_tell(s);
  if(flags & 2)
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


demuxer_desc_t demuxer_desc_rawvideo = {
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
