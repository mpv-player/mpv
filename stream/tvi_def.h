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

#ifndef MPLAYER_TVI_DEF_H
#define MPLAYER_TVI_DEF_H

#include <stdlib.h> /* malloc */
#include <string.h> /* memset */
#include "libmpcodecs/img_format.h"
#include "tv.h"

static int init(priv_t *priv);
static int uninit(priv_t *priv);
static int control(priv_t *priv, int cmd, void *arg);
static int start(priv_t *priv);
static double grab_video_frame(priv_t *priv, char *buffer, int len);
static int get_video_framesize(priv_t *priv);
static double grab_audio_frame(priv_t *priv, char *buffer, int len);
static int get_audio_framesize(priv_t *priv);

static const tvi_functions_t functions =
{
    init,
    uninit,
    control,
    start,
    grab_video_frame,
    get_video_framesize,
    grab_audio_frame,
    get_audio_framesize
};

static tvi_handle_t *new_handle(void)
{
    tvi_handle_t *h = malloc(sizeof(tvi_handle_t));

    if (!h)
	return NULL;
    h->priv = malloc(sizeof(priv_t));
    if (!h->priv)
    {
	free(h);
	return NULL;
    }
    memset(h->priv, 0, sizeof(priv_t));
    h->functions = &functions;
    h->seq = 0;
    h->chanlist = -1;
    h->chanlist_s = NULL;
    h->norm = -1;
    h->channel = -1;
    h->scan = NULL;
    return h;
}

static void free_handle(tvi_handle_t *h)
{
    if (h) {
	if (h->priv)
	    free(h->priv);
	if (h->scan)
	    free(h->scan);
	free(h);
    }
}

/**
 Fills video frame in given buffer with blue color for yv12,i420,uyvy,yuy2.
 Other formats will be filled with 0xC0
*/
static inline void fill_blank_frame(char* buffer,int len,int fmt){
    int i;
    // RGB(0,0,255) <-> YVU(41,110,240)

    switch(fmt){
    case IMGFMT_YV12:
        memset(buffer, 41,4*len/6);       //Y
        memset(buffer+4*len/6, 110,len/6);//V
        memset(buffer+5*len/6, 240,len/6);//U
        break;
    case IMGFMT_I420:
        memset(buffer, 41,4*len/6);       //Y
        memset(buffer+4*len/6, 240,len/6);//U
        memset(buffer+5*len/6, 110,len/6);//V
        break;
    case IMGFMT_UYVY:
        for(i=0;i<len;i+=4){
            buffer[i]=0xFF;
            buffer[i+1]=0;
            buffer[i+2]=0;
            buffer[i+3]=0;
	}
        break;
    case IMGFMT_YUY2:
        for(i=0;i<len;i+=4){
            buffer[i]=0;
            buffer[i+1]=0xFF;
            buffer[i+2]=0;
            buffer[i+3]=0;
	}
        break;
    case IMGFMT_MJPEG:
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
