/*
 * video frame reading
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

#include "config.h"

#include <stdio.h>
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mp_msg.h"

#include "stream/stream.h"
#include "demuxer.h"
#ifdef DEMUX_TY_OSD
#include "demux_ty_osd.h"
#endif
#include "stheader.h"

/* sub_cc (closed captions)*/
#include "sub/sub_cc.h"

/* biCompression constant */
#define BI_RGB        0L

#ifdef CONFIG_LIVE555
#include "demux_rtp.h"
#endif

int video_read_properties(sh_video_t *sh_video){
demux_stream_t *d_video=sh_video->ds;

 if((d_video->demuxer->file_format == DEMUXER_TYPE_ASF) || (d_video->demuxer->file_format == DEMUXER_TYPE_AVI)) {
    // display info:
    // in case no strf chunk has been seen in avi, we have no bitmap header
    if(!sh_video->bih) return 0;
    sh_video->format=sh_video->bih->biCompression;
    sh_video->disp_w=sh_video->bih->biWidth;
    sh_video->disp_h=abs(sh_video->bih->biHeight);
  }

return 1;
}

int video_read_frame(sh_video_t* sh_video,float* frame_time_ptr,unsigned char** start,int force_fps){
    demux_stream_t *d_video=sh_video->ds;
    demuxer_t *demuxer=d_video->demuxer;
    float frame_time=1;
    float pts1=d_video->pts;
    int in_size=0;

    *start=NULL;

      // frame-based file formats: (AVI,ASF,MOV)
    in_size=ds_get_packet(d_video,start);
    if(in_size<0) return -1; // EOF


//------------------------ frame decoded. --------------------

    // Increase video timers:
    sh_video->num_frames+=frame_time;
    ++sh_video->num_frames_decoded;

    frame_time*=sh_video->frametime;

    // override frame_time for variable/unknown FPS formats:
    if(!force_fps) switch(demuxer->file_format){
      case DEMUXER_TYPE_GIF:
      case DEMUXER_TYPE_MATROSKA:
      case DEMUXER_TYPE_MNG:
       if(d_video->pts>0 && pts1>0 && d_video->pts>pts1)
         frame_time=d_video->pts-pts1;
        break;
      case DEMUXER_TYPE_TV:
      case DEMUXER_TYPE_VIVO:
      case DEMUXER_TYPE_ASF: {
        double next_pts = ds_get_next_pts(d_video);
        double d= (next_pts != MP_NOPTS_VALUE) ? next_pts - d_video->pts : d_video->pts-pts1;
        if(d>=0){
          if(d>0){
            if((int)sh_video->fps==1000)
              mp_msg(MSGT_CPLAYER,MSGL_V,"\navg. framerate: %d fps             \n",(int)(1.0f/d));
            sh_video->frametime=d; // 1ms
            sh_video->fps=1.0f/d;
          }
          frame_time = d;
        } else {
          mp_msg(MSGT_CPLAYER,MSGL_WARN,"\nInvalid frame duration value (%5.3f/%5.3f => %5.3f). Defaulting to %5.3f sec.\n",d_video->pts,next_pts,d,frame_time);
          // frame_time = 1/25.0;
        }
      }
      break;
      case DEMUXER_TYPE_LAVF:
      case DEMUXER_TYPE_LAVF_PREFERRED:
        if((int)sh_video->fps==1000 || (int)sh_video->fps<=1){
          double next_pts = ds_get_next_pts(d_video);
          double d= (next_pts != MP_NOPTS_VALUE) ? next_pts - d_video->pts : d_video->pts-pts1;
          if(d>=0){
            frame_time = d;
          }
        }
      break;
      case DEMUXER_TYPE_REAL:
        {
          double next_pts = ds_get_next_pts(d_video);
          double d = (next_pts != MP_NOPTS_VALUE) ? next_pts - d_video->pts : d_video->pts - pts1;

          frame_time = (d >= 0 && pts1 > 0) ? d : 0.001;
        }
      break;
    }


    sh_video->pts=d_video->pts;

    if(frame_time_ptr) *frame_time_ptr=frame_time;
    return in_size;
}
