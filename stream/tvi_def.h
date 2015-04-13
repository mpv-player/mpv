/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_TVI_DEF_H
#define MPLAYER_TVI_DEF_H

#include <stdlib.h> /* malloc */
#include <string.h> /* memset */
#include "video/img_fourcc.h"
#include "tv.h"

static int init(priv_t *priv);
static int uninit(priv_t *priv);
static int do_control(priv_t *priv, int cmd, void *arg);
static int start(priv_t *priv);
static double grab_video_frame(priv_t *priv, char *buffer, int len);
static int get_video_framesize(priv_t *priv);
static double grab_audio_frame(priv_t *priv, char *buffer, int len);
static int get_audio_framesize(priv_t *priv);

static const tvi_functions_t functions =
{
    init,
    uninit,
    do_control,
    start,
    grab_video_frame,
    get_video_framesize,
    grab_audio_frame,
    get_audio_framesize
};

/**
 Fills video frame in given buffer with blue color for yv12,i420,uyvy,yuy2.
 Other formats will be filled with 0xC0
*/
static inline void fill_blank_frame(char* buffer,int len,int fmt){
    int i;
    // RGB(0,0,255) <-> YVU(41,110,240)

    switch(fmt){
    case MP_FOURCC_YV12:
        memset(buffer, 41,4*len/6);       //Y
        memset(buffer+4*len/6, 110,len/6);//V
        memset(buffer+5*len/6, 240,len/6);//U
        break;
    case MP_FOURCC_I420:
        memset(buffer, 41,4*len/6);       //Y
        memset(buffer+4*len/6, 240,len/6);//U
        memset(buffer+5*len/6, 110,len/6);//V
        break;
    case MP_FOURCC_UYVY:
        for(i=0;i<len;i+=4){
            buffer[i]=0xFF;
            buffer[i+1]=0;
            buffer[i+2]=0;
            buffer[i+3]=0;
        }
        break;
    case MP_FOURCC_YUY2:
        for(i=0;i<len;i+=4){
            buffer[i]=0;
            buffer[i+1]=0xFF;
            buffer[i+2]=0;
            buffer[i+3]=0;
        }
        break;
    case MP_FOURCC_MJPEG:
        /*
        This is compressed format. I don't know yet how to fill such frame with blue color.
        Keeping frame unchanged.
        */
        break;
    default:
        memset(buffer,0xC0,len);
    }
}

#endif /* MPLAYER_TVI_DEF_H */
