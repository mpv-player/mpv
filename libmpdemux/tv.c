/*
 TV subsystem for libMPDemux by Alex
 
 API idea based on libvo2's

 UNDER HEAVY DEVELOPEMENT, DO NOT USE! :)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#ifdef USE_TV
#include "tv.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

/* global! */
tvi_handle_t *tv_handler;

/* some default values */
float tv_param_freq = 0.0;
char *tv_param_channel = "0";
char *tv_param_norm = "pal";
int tv_param_on = 0;
char *tv_param_device = NULL;
char *tv_param_driver = "dummy";
int tv_param_width = -1;
int tv_param_height = -1;


/* ================== DEMUX_TV ===================== */
/*
  Return value:
    0 = EOF(?) or no stream
    1 = successfully read a packet
*/
/* fill demux->video and demux->audio */
int demux_tv_fill_buffer(demuxer_t *demux)
{
    int seq;
    demux_stream_t *ds_video = NULL;
    demux_packet_t *dp_video = NULL;
    demux_stream_t *ds_audio = NULL;
    demux_packet_t *dp_audio = NULL;
    int len_video, len_audio;

    demux->filepos = -1;

    /* ================== ADD VIDEO PACKET =================== */
    len_video = tv_handler->functions->get_video_framesize(tv_handler->priv);
    ds_video = demux->video;

    if (!ds_video)
    {    
	dp_video = new_demux_packet(len_video);
	tv_handler->functions->grab_video_frame(tv_handler->priv, dp_video->buffer, len_video);
	dp_video->pos = demux->filepos;
	ds_video->asf_packet = dp_video;
	ds_video->asf_seq = seq;
    }
    else if (ds_video->asf_packet)
    {
	if (ds_video->asf_seq != seq)
	{
	    ds_add_packet(ds_video, ds_video->asf_packet);
	    ds_video->asf_packet = NULL;
	}
	else
	{
	    dp_video = ds_video->asf_packet;
	    dp_video->buffer = realloc(dp_video->buffer, dp_video->len+len_video);
	    tv_handler->functions->grab_video_frame(tv_handler->priv, dp_video->buffer+dp_video->len, len_video);
	    mp_dbg(MSGT_DEMUX,MSGL_DBG4, "video data appended %d+%d\n", dp_video->len, len_video);
	    dp_video->len += len_video;
	}
    }
    

    /* ================== ADD AUDIO PACKET =================== */
    len_audio = tv_handler->functions->get_audio_framesize(tv_handler->priv);
    ds_audio = demux->audio;
    
    if (!ds_audio)
    {
	dp_audio = new_demux_packet(len_audio);
	tv_handler->functions->grab_audio_frame(tv_handler->priv, dp_audio->buffer, len_audio);
	dp_audio->pos = demux->filepos;
	ds_audio->asf_packet = dp_audio;
	ds_audio->asf_seq = seq;
    }
    else if (ds_audio->asf_packet)
    {
	if (ds_audio->asf_seq != seq)
	{
	    ds_add_packet(ds_audio, ds_audio->asf_packet);
	    ds_audio->asf_packet = NULL;
	}
	else
	{
	    dp_audio = ds_audio->asf_packet;
	    dp_audio->buffer = realloc(dp_audio->buffer, dp_audio->len+len_audio);
	    tv_handler->functions->grab_audio_frame(tv_handler->priv, dp_audio->buffer+dp_audio->len, len_audio);
	    mp_dbg(MSGT_DEMUX,MSGL_DBG4, "audio data appended %d+%d\n", dp_audio->len, len_audio);
	    dp_audio->len += len_audio;
	}
    }

    return 1;
}

int demux_open_tv(demuxer_t *demuxer)
{
    sh_video_t *sh_video;
    tvi_handle_t *tvh = tv_handler;
    tvi_functions_t *funcs = tvh->functions;
    
    sh_video = new_sh_video(demuxer,0);

//    sh->format=0x7476696e; /* "tvin" */
    if (funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FORMAT, &sh_video->format) != TVI_CONTROL_TRUE)
	sh_video->format = 0x00000000;

    if(!sh_video->fps)
    {
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FPS, &sh_video->fps) != TVI_CONTROL_TRUE)
	    sh_video->fps = 24.0f;
    }
    sh_video->frametime = 1.0f/sh_video->fps;

    /* set width */
    if (tv_param_width != -1)
    {
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_CHK_WIDTH, &tv_param_width) == TVI_CONTROL_TRUE)
	{
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_WIDTH, &tv_param_width);
	    sh_video->disp_w = tv_param_width;
	}
	else
	{
	    printf("Unable set requested width: %d\n", tv_param_width);
	    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_WIDTH, &sh_video->disp_w);
	    tv_param_width = sh_video->disp_w;
	}    
    }
    else
	funcs->control(tvh->priv, TVI_CONTROL_VID_GET_WIDTH, &sh_video->disp_w);

    /* set height */
    if (tv_param_height != -1)
    {
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_CHK_HEIGHT, &tv_param_height) == TVI_CONTROL_TRUE)
	{
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_HEIGHT, &tv_param_height);
	    sh_video->disp_h = tv_param_height;
	}
	else
	{
	    printf("Unable set requested height: %d\n", tv_param_height);
	    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &sh_video->disp_h);
	    tv_param_height = sh_video->disp_h;
	}    
    }
    else
	funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &sh_video->disp_h);
    
    /* emulate BITMAPINFOHEADER */
    sh_video->bih = malloc(sizeof(BITMAPINFOHEADER));
    memset(sh_video->bih, 0, sizeof(BITMAPINFOHEADER));
    sh_video->bih->biSize = 40;
    sh_video->bih->biWidth = sh_video->disp_w;
    sh_video->bih->biHeight = sh_video->disp_h;
    if (funcs->control(tvh->priv, TVI_CONTROL_VID_GET_PLANES, &sh_video->bih->biPlanes) != TVI_CONTROL_TRUE)
	sh_video->bih->biPlanes = 1;
    if (funcs->control(tvh->priv, TVI_CONTROL_VID_GET_BITS, &sh_video->bih->biBitCount) != TVI_CONTROL_TRUE)
	sh_video->bih->biBitCount = 12;
    sh_video->bih->biCompression = sh_video->format;
    sh_video->bih->biSizeImage = sh_video->bih->biWidth * sh_video->bih->biHeight * 3;
    
    demuxer->video->sh = sh_video;
    sh_video->ds = demuxer->video;
    demuxer->video->id = 0;

    /* here comes audio init */
}

/* ================== STREAM_TV ===================== */
tvi_handle_t *tv_begin()
{
    if (!strcmp(tv_param_driver, "dummy"))
	return tvi_init_dummy(tv_param_device);
    if (!strcmp(tv_param_driver, "v4l"))
	return tvi_init_v4l(tv_param_device);

    mp_msg(MSGT_TV, MSGL_ERR, "No such driver: %s\n", tv_param_driver); 
    return(NULL);
}

void tv_init(tvi_handle_t *tvi)
{
    printf("Using driver: %s\n", tvi->info->short_name);
    printf(" name: %s\n", tvi->info->name);
    printf(" author: %s\n", tvi->info->author);
    if (tvi->info->comment)
	printf(" comment: %s\n", tvi->info->comment);

    return tvi->functions->init(tvi->priv);
}
#endif /* USE_TV */
