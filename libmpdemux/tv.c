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
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "tv.h"

/* some default values */
unsigned long tv_param_freq = 0;
char *tv_param_channel = "0";
char *tv_param_norm = "pal";
int tv_param_on = 0;
char *tv_param_device = NULL;
char *tv_param_driver = "dummy";
int tv_param_width = -1;
int tv_param_height = -1;
int tv_param_input = 0; /* used in v4l and bttv */


/* ================== DEMUX_TV ===================== */
/*
  Return value:
    0 = EOF(?) or no stream
    1 = successfully read a packet
*/
/* fill demux->video and demux->audio */
int demux_tv_fill_buffer(demuxer_t *demux, tvi_handle_t *tvh)
{
    int seq = tvh->seq;
    demux_stream_t *ds_video = NULL;
    demux_packet_t *dp_video = NULL;
    demux_stream_t *ds_audio = NULL;
    demux_packet_t *dp_audio = NULL;
    int len_video, len_audio;

    printf("demux_tv_fill_buffer(sequence:%d) called!\n", seq);

    demux->filepos = -1;

    seq++;
    tvh->seq++;

    /* ================== ADD VIDEO PACKET =================== */
    len_video = tvh->functions->get_video_framesize(tvh->priv);
    ds_video = demux->video;

    if (!ds_video->asf_packet)
    {
	/* create new packet */
	dp_video = new_demux_packet(len_video);
//	printf("new dp_video->buffer: %p (%d bytes)\n", dp_video, len_video);
	tvh->functions->grab_video_frame(tvh->priv, dp_video->buffer, len_video);
	dp_video->pos = demux->filepos;
	ds_video->asf_packet = dp_video;
	ds_video->asf_seq = seq;
    }
    else if (ds_video->asf_packet)
    {
	if (ds_video->asf_seq != seq)
	{
	    /* close segment, finalize packet */
	    ds_add_packet(ds_video, ds_video->asf_packet);
	    ds_video->asf_packet = NULL;
	}
	else
	{
	    /* append data to segment */
	    dp_video = ds_video->asf_packet;
	    dp_video->buffer = realloc(dp_video->buffer, dp_video->len+len_video);
//	    printf("dp_video->buffer: %p (%d bytes)\n", dp_video, dp_video->len+len_video);
	    tvh->functions->grab_video_frame(tvh->priv, dp_video->buffer+dp_video->len, len_video);
	    mp_dbg(MSGT_DEMUX,MSGL_DBG4, "video data appended %d+%d\n", dp_video->len, len_video);
	    dp_video->len += len_video;
	}
    }
    

    /* ================== ADD AUDIO PACKET =================== */
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_AUDIO, 0) != TVI_CONTROL_TRUE)
	return 1; /* no audio, only video */

    len_audio = tvh->functions->get_audio_framesize(tvh->priv);
    ds_audio = demux->audio;
    
    if (!ds_audio->asf_packet)
    {
	dp_audio = new_demux_packet(len_audio);
	tvh->functions->grab_audio_frame(tvh->priv, dp_audio->buffer, len_audio);
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
	    tvh->functions->grab_audio_frame(tvh->priv, dp_audio->buffer+dp_audio->len, len_audio);
	    mp_dbg(MSGT_DEMUX,MSGL_DBG4, "audio data appended %d+%d\n", dp_audio->len, len_audio);
	    dp_audio->len += len_audio;
	}
    }

    return 1;
}

int demux_open_tv(demuxer_t *demuxer, tvi_handle_t *tvh)
{
    sh_video_t *sh_video = NULL;
    sh_audio_t *sh_audio = NULL;
    tvi_functions_t *funcs = tvh->functions;
    
    if (funcs->control(tvh->priv, TVI_CONTROL_IS_VIDEO, 0) != TVI_CONTROL_TRUE)
    {
	printf("Error: no video input present!\n");
	return;
    }
    
    sh_video = new_sh_video(demuxer, 0);

    /* hack to use YUV 4:2:0 format ;) */
    sh_video->format = IMGFMT_YV12;
    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_FORMAT, &sh_video->format);

    /* get IMGFMT_ */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FORMAT, &sh_video->format);
    if (IMGFMT_IS_RGB(sh_video->format) || IMGFMT_IS_BGR(sh_video->format))
	sh_video->format = 0x0;

    /* set FPS and FRAMETIME */
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

    printf("Output size: %dx%d\n", sh_video->disp_w, sh_video->disp_h);
    
    demuxer->video->sh = sh_video;
    sh_video->ds = demuxer->video;
    demuxer->video->id = 0;

    /* here comes audio init */
    if (funcs->control(tvh->priv, TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
    {
	int audio_format;

	sh_audio = new_sh_audio(demuxer, 0);
	
	sh_audio->wf = malloc(sizeof(WAVEFORMATEX));
	memset(sh_audio->wf, 0, sizeof(WAVEFORMATEX));
	
	/* yeah, audio is present */
	if (funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_FORMAT, &audio_format) != TVI_CONTROL_TRUE)
	    goto no_audio;
	switch(audio_format)
	{
	    case AFMT_U8:
	    case AFMT_S8:
	    case AFMT_U16_LE:
	    case AFMT_U16_BE:
	    case AFMT_S16_LE:
	    case AFMT_S16_BE:
	    case AFMT_S32_LE:
	    case AFMT_S32_BE:
		sh_audio->format = 0x1; /* PCM */
		break;
	    case AFMT_IMA_ADPCM:
	    case AFMT_MU_LAW:
	    case AFMT_A_LAW:
	    case AFMT_MPEG:
	    case AFMT_AC3:
	    default:
		printf("%s unsupported!\n", audio_out_format_name(audio_format));
		goto no_audio;
	}
	
	funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_CHANNELS, &sh_audio->wf->nChannels);
	funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_SAMPLERATE, &sh_audio->wf->nSamplesPerSec);
	funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_SAMPLESIZE, &sh_audio->wf->nAvgBytesPerSec);

	demuxer->audio->sh = sh_audio;
	sh_audio->ds = demuxer->audio;
	demuxer->audio->id = 0;
    }
no_audio:

    /* set some params got from cmdline */
    funcs->control(tvh->priv, TVI_CONTROL_SPC_SET_INPUT, &tv_param_input);

    /* set freq in MHz - change this to float ! */
    funcs->control(tvh->priv, TVI_CONTROL_TUN_SET_FREQ, &tv_param_freq);

    funcs->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, &tv_param_freq);
    printf("freq: %lu\n", tv_param_freq);
    
    /* also start device! */
    funcs->start(tvh->priv);
}

/* ================== STREAM_TV ===================== */
tvi_handle_t *tv_begin(void)
{
    if (!strcmp(tv_param_driver, "dummy"))
	return (tvi_handle_t *)tvi_init_dummy(tv_param_device);
    if (!strcmp(tv_param_driver, "v4l"))
	return (tvi_handle_t *)tvi_init_v4l(tv_param_device);

    mp_msg(MSGT_TV, MSGL_ERR, "No such driver: %s\n", tv_param_driver); 
    return(NULL);
}

int tv_init(tvi_handle_t *tvh)
{
    tvi_param_t *params;

    printf("Selected driver: %s\n", tvh->info->short_name);
    printf(" name: %s\n", tvh->info->name);
    printf(" author: %s\n", tvh->info->author);
    if (tvh->info->comment)
	printf(" comment: %s\n", tvh->info->comment);

    params = malloc(sizeof(tvi_param_t)*2);
    params[0].opt = malloc(strlen("input"));
    sprintf((char *)params[0].opt, "input");
    params[0].value = malloc(sizeof(int));
    (int)*(void **)params[0].value = tv_param_input;
    params[1].opt = params[1].value = NULL;

    return tvh->functions->init(tvh->priv, params);
}
#endif /* USE_TV */
