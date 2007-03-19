/*
 TV Interface for MPlayer
 
 (C) Alex Beregszaszi
 
 API idea based on libvo2

 Feb 19, 2002: Significant rewrites by Charles R. Henrich (henrich@msu.edu)
				to add support for audio, and bktr *BSD support.

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include "config.h"

int tv_param_on = 0;

#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "libaf/af_format.h"
#include "libmpcodecs/img_format.h"
#include "libvo/fastmemcpy.h"

#include "tv.h"

#include "frequencies.h"

/* some default values */
int tv_param_audiorate = 44100;
int tv_param_noaudio = 0;
int tv_param_immediate = 0;
char *tv_param_freq = NULL;
char *tv_param_channel = NULL;
char *tv_param_norm = "pal";
#ifdef HAVE_TV_V4L2
int tv_param_normid = -1;
#endif
char *tv_param_chanlist = "europe-east";
char *tv_param_device = NULL;
char *tv_param_driver = "dummy";
int tv_param_width = -1;
int tv_param_height = -1;
int tv_param_input = 0; /* used in v4l and bttv */
int tv_param_outfmt = -1;
float tv_param_fps = -1.0;
char **tv_param_channels = NULL;
int tv_param_audio_id = 0;
#if defined(HAVE_TV_V4L)
int tv_param_amode = -1;
int tv_param_volume = -1;
int tv_param_bass = -1;
int tv_param_treble = -1;
int tv_param_balance = -1;
int tv_param_forcechan = -1;
int tv_param_force_audio = 0;
int tv_param_buffer_size = -1;
int tv_param_mjpeg = 0;
int tv_param_decimation = 2;
int tv_param_quality = 90;
#if defined(HAVE_ALSA9) || defined(HAVE_ALSA1X)
int tv_param_alsa = 0;
#endif
#endif
char* tv_param_adevice = NULL;
int tv_param_brightness = 0;
int tv_param_contrast = 0;
int tv_param_hue = 0;
int tv_param_saturation = 0;
tv_channels_t *tv_channel_list;
tv_channels_t *tv_channel_current, *tv_channel_last;
char *tv_channel_last_real;

/* enumerating drivers (like in stream.c) */
extern tvi_info_t tvi_info_dummy;
#ifdef HAVE_TV_V4L1
extern tvi_info_t tvi_info_v4l;
#endif
#ifdef HAVE_TV_V4L2
extern tvi_info_t tvi_info_v4l2;
#endif
#ifdef HAVE_TV_BSDBT848
extern tvi_info_t tvi_info_bsdbt848;
#endif

static const tvi_info_t* tvi_driver_list[]={
    &tvi_info_dummy,
#ifdef HAVE_TV_V4L1
    &tvi_info_v4l,
#endif
#ifdef HAVE_TV_V4L2
    &tvi_info_v4l2,
#endif
#ifdef HAVE_TV_BSDBT848
    &tvi_info_bsdbt848,
#endif
    NULL
};


/* ================== DEMUX_TV ===================== */
/*
  Return value:
    0 = EOF(?) or no stream
    1 = successfully read a packet
*/
/* fill demux->video and demux->audio */

static int demux_tv_fill_buffer(demuxer_t *demux, demux_stream_t *ds)
{
    tvi_handle_t *tvh=(tvi_handle_t*)(demux->priv);
    demux_packet_t* dp;
    unsigned int len=0;

    /* ================== ADD AUDIO PACKET =================== */

    if (ds==demux->audio && tv_param_noaudio == 0 && 
        tvh->functions->control(tvh->priv, 
                                TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
        {
        len = tvh->functions->get_audio_framesize(tvh->priv);

        dp=new_demux_packet(len);
        dp->flags|=1; /* Keyframe */
        dp->pts=tvh->functions->grab_audio_frame(tvh->priv, dp->buffer,len);
        ds_add_packet(demux->audio,dp);
        }

    /* ================== ADD VIDEO PACKET =================== */

    if (ds==demux->video && tvh->functions->control(tvh->priv, 
                            TVI_CONTROL_IS_VIDEO, 0) == TVI_CONTROL_TRUE)
        {
		len = tvh->functions->get_video_framesize(tvh->priv);
       	dp=new_demux_packet(len);
		dp->flags|=1; /* Keyframe */
  		dp->pts=tvh->functions->grab_video_frame(tvh->priv, dp->buffer, len);
   		ds_add_packet(demux->video,dp);
	 }

    return 1;
}

static int norm_from_string(tvi_handle_t *tvh, char* norm)
{
#ifdef HAVE_TV_V4L2
    if (strcmp(tv_param_driver, "v4l2") != 0) {
#endif
    if (!strcasecmp(norm, "pal"))
	return TV_NORM_PAL;
    else if (!strcasecmp(norm, "ntsc"))
	return TV_NORM_NTSC;
    else if (!strcasecmp(norm, "secam"))
	return TV_NORM_SECAM;
    else if (!strcasecmp(norm, "palnc"))
	return TV_NORM_PALNC;
    else if (!strcasecmp(norm, "palm"))
	return TV_NORM_PALM;
    else if (!strcasecmp(norm, "paln"))
	return TV_NORM_PALN;
    else if (!strcasecmp(norm, "ntscjp"))
	return TV_NORM_NTSCJP;
    else {
	mp_msg(MSGT_TV, MSGL_WARN, "tv.c: norm_from_string(%s): Bogus norm parameter, setting PAL.\n", norm);
	return TV_NORM_PAL;
    }
#ifdef HAVE_TV_V4L2
    } else {
	tvi_functions_t *funcs = tvh->functions;
	char str[8];
	strncpy(str, norm, sizeof(str)-1);
	str[sizeof(str)-1] = '\0';
        if (funcs->control(tvh->priv, TVI_CONTROL_SPC_GET_NORMID, str) != TVI_CONTROL_TRUE)
        {
	    mp_msg(MSGT_TV, MSGL_WARN, "tv.c: norm_from_string(%s): Bogus norm parameter, setting default.\n", norm);
	    return 0;
        }
	return *(int *)str;
    }
#endif
}

static int open_tv(tvi_handle_t *tvh)
{
    int i;
    tvi_functions_t *funcs = tvh->functions;
    int tv_fmt_list[] = {
      IMGFMT_YV12,
      IMGFMT_I420,
      IMGFMT_UYVY,
      IMGFMT_YUY2,
      IMGFMT_RGB32,
      IMGFMT_RGB24,
      IMGFMT_RGB16,
      IMGFMT_RGB15
    };

    if (funcs->control(tvh->priv, TVI_CONTROL_IS_VIDEO, 0) != TVI_CONTROL_TRUE)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "Error: No video input present!\n");
	return 0;
    }

    if (tv_param_outfmt == -1)
      for (i = 0; i < sizeof (tv_fmt_list) / sizeof (*tv_fmt_list); i++)
        {
          tv_param_outfmt = tv_fmt_list[i];
          if (funcs->control (tvh->priv, TVI_CONTROL_VID_SET_FORMAT,
                              &tv_param_outfmt) == TVI_CONTROL_TRUE)
            break;
        }
    else
    {
    switch(tv_param_outfmt)
    {
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_UYVY:
	case IMGFMT_YUY2:
	case IMGFMT_RGB32:
	case IMGFMT_RGB24:
	case IMGFMT_BGR32:
	case IMGFMT_BGR24:
	case IMGFMT_BGR16:
	case IMGFMT_BGR15:
	    break;
	default:
	    mp_msg(MSGT_TV, MSGL_ERR, "==================================================================\n");
	    mp_msg(MSGT_TV, MSGL_ERR, " WARNING: UNTESTED OR UNKNOWN OUTPUT IMAGE FORMAT REQUESTED (0x%x)\n", tv_param_outfmt);
	    mp_msg(MSGT_TV, MSGL_ERR, " This may cause buggy playback or program crash! Bug reports will\n");
	    mp_msg(MSGT_TV, MSGL_ERR, " be ignored! You should try again with YV12 (which is the default\n");
	    mp_msg(MSGT_TV, MSGL_ERR, " colorspace) and read the documentation!\n");
	    mp_msg(MSGT_TV, MSGL_ERR, "==================================================================\n");
    }
    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_FORMAT, &tv_param_outfmt);
    }

    /* set some params got from cmdline */
    funcs->control(tvh->priv, TVI_CONTROL_SPC_SET_INPUT, &tv_param_input);

#ifdef HAVE_TV_V4L2
    if (!strcmp(tv_param_driver, "v4l2") && tv_param_normid >= 0) {
	mp_msg(MSGT_TV, MSGL_V, "Selected norm id: %d\n", tv_param_normid);
	if (funcs->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM, &tv_param_normid) != TVI_CONTROL_TRUE) {
	    mp_msg(MSGT_TV, MSGL_ERR, "Error: Cannot set norm!\n");
	}
    } else {
#endif
    /* select video norm */
    tvh->norm = norm_from_string(tvh, tv_param_norm);

    mp_msg(MSGT_TV, MSGL_V, "Selected norm: %s\n", tv_param_norm);
    if (funcs->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM, &tvh->norm) != TVI_CONTROL_TRUE) {
	mp_msg(MSGT_TV, MSGL_ERR, "Error: Cannot set norm!\n");
    }
#ifdef HAVE_TV_V4L2
    }
#endif

#ifdef HAVE_TV_V4L1
    if ( tv_param_mjpeg )
    {
      /* set width to expected value */
      if (tv_param_width == -1)
        {
          tv_param_width = 704/tv_param_decimation;
        }
      if (tv_param_height == -1)
        {
	  if ( tvh->norm != TV_NORM_NTSC )
            tv_param_height = 576/tv_param_decimation; 
	  else
            tv_param_height = 480/tv_param_decimation; 
        }
      mp_msg(MSGT_TV, MSGL_INFO, 
	       "  MJP: width %d height %d\n", tv_param_width, tv_param_height);
    }
#endif

    /* limits on w&h are norm-dependent -- JM */
    /* set width */
    if (tv_param_width != -1)
    {
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_CHK_WIDTH, &tv_param_width) == TVI_CONTROL_TRUE)
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_WIDTH, &tv_param_width);
	else
	{
	    mp_msg(MSGT_TV, MSGL_ERR, "Unable to set requested width: %d\n", tv_param_width);
	    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_WIDTH, &tv_param_width);
	}    
    }

    /* set height */
    if (tv_param_height != -1)
    {
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_CHK_HEIGHT, &tv_param_height) == TVI_CONTROL_TRUE)
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_HEIGHT, &tv_param_height);
	else
	{
	    mp_msg(MSGT_TV, MSGL_ERR, "Unable to set requested height: %d\n", tv_param_height);
	    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &tv_param_height);
	}    
    }

    if (funcs->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) != TVI_CONTROL_TRUE)
    {
	mp_msg(MSGT_TV, MSGL_WARN, "Selected input hasn't got a tuner!\n");	
	goto done;
    }

    /* select channel list */
    for (i = 0; chanlists[i].name != NULL; i++)
    {
	if (!strcasecmp(chanlists[i].name, tv_param_chanlist))
	{
	    tvh->chanlist = i;
	    tvh->chanlist_s = chanlists[i].list;
	    break;
	}
    }

    if (tvh->chanlist == -1)
	mp_msg(MSGT_TV, MSGL_WARN, "Unable to find selected channel list! (%s)\n",
	    tv_param_chanlist);
    else
	mp_msg(MSGT_TV, MSGL_V, "Selected channel list: %s (including %d channels)\n",
	    chanlists[tvh->chanlist].name, chanlists[tvh->chanlist].count);

    if (tv_param_freq && tv_param_channel)
    {
	mp_msg(MSGT_TV, MSGL_WARN, "You can't set frequency and channel simultaneously!\n");
	goto done;
    }

    /* Handle channel names */
    if (tv_param_channels) {
	char** channels = tv_param_channels;
	mp_msg(MSGT_TV, MSGL_INFO, "TV channel names detected.\n");
	tv_channel_list = malloc(sizeof(tv_channels_t));
	tv_channel_list->index=1;
	tv_channel_list->next=NULL;
	tv_channel_list->prev=NULL;
	tv_channel_current = tv_channel_list;

	while (*channels) {
		char* tmp = *(channels++);
		char* sep = strchr(tmp,'-');
		int i;
		struct CHANLIST cl;

		if (!sep) continue; // Wrong syntax, but mplayer should not crash

		strlcpy(tv_channel_current->name, sep + 1,
		        sizeof(tv_channel_current->name));
		sep[0] = '\0';
		strncpy(tv_channel_current->number, tmp, 5);
		tv_channel_current->number[4]='\0';

		while ((sep=strchr(tv_channel_current->name, '_')))
		    sep[0] = ' ';

		// if channel number is a number and larger than 1000 threat it as frequency
                // tmp still contain pointer to null-terminated string with channel number here
		if (atoi(tmp)>1000){ 
		    tv_channel_current->freq=atoi(tmp);
		}else{
		tv_channel_current->freq = 0;
		for (i = 0; i < chanlists[tvh->chanlist].count; i++) {
		    cl = tvh->chanlist_s[i];
		    if (!strcasecmp(cl.name, tv_channel_current->number)) {
			tv_channel_current->freq=cl.freq;
			break;
		    }
		}
		}
	        if (tv_channel_current->freq == 0)
		    mp_msg(MSGT_TV, MSGL_ERR, "Couldn't find frequency for channel %s (%s)\n",
				    tv_channel_current->number, tv_channel_current->name);
		else {
		  sep = strchr(tv_channel_current->name, '-');
		  if ( !sep ) sep = strchr(tv_channel_current->name, '+');

		  if ( sep ) {
		    i = atoi (sep+1);
		    if ( sep[0] == '+' ) tv_channel_current->freq += i * 100;
		    if ( sep[0] == '-' ) tv_channel_current->freq -= i * 100;
		    sep[0] = '\0';
		  }
		}

		/*mp_msg(MSGT_TV, MSGL_INFO, "-- Detected channel %s - %s (%5.3f)\n",
				tv_channel_current->number, tv_channel_current->name,
				(float)tv_channel_current->freq/1000);*/

		tv_channel_current->next = malloc(sizeof(tv_channels_t));
		tv_channel_current->next->index = tv_channel_current->index + 1;
		tv_channel_current->next->prev = tv_channel_current;
		tv_channel_current->next->next = NULL;
		tv_channel_current = tv_channel_current->next;
	}
	if (tv_channel_current->prev)
  	  tv_channel_current->prev->next = NULL;
	free(tv_channel_current);
    } else 
	    tv_channel_last_real = malloc(5);

    if (tv_channel_list) {
	int i;
	int channel = 0;
	if (tv_param_channel)
	 {
	   if (isdigit(*tv_param_channel))
		/* if tv_param_channel begins with a digit interpret it as a number */
		channel = atoi(tv_param_channel);
	   else
	      {
		/* if tv_param_channel does not begin with a digit 
		   set the first channel that contains tv_param_channel in its name */

		tv_channel_current = tv_channel_list;
		while ( tv_channel_current ) {
			if ( strstr(tv_channel_current->name, tv_param_channel) )
			  break;
			tv_channel_current = tv_channel_current->next;
			}
		if ( !tv_channel_current ) tv_channel_current = tv_channel_list;
	      }
	 }
	else
		channel = 1;

	if ( channel ) {
	tv_channel_current = tv_channel_list;
	for (i = 1; i < channel; i++)
		if (tv_channel_current->next)
			tv_channel_current = tv_channel_current->next;
	}

	mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s - %s (freq: %.3f)\n", tv_channel_current->number,
			tv_channel_current->name, (float)tv_channel_current->freq/1000);
	tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
	tv_channel_last = tv_channel_current;
    } else {
    /* we need to set frequency */
    if (tv_param_freq)
    {
	unsigned long freq = atof(tv_param_freq)*16;

        /* set freq in MHz */
	funcs->control(tvh->priv, TVI_CONTROL_TUN_SET_FREQ, &freq);

	funcs->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, &freq);
	mp_msg(MSGT_TV, MSGL_V, "Selected frequency: %lu (%.3f)\n",
	    freq, (float)freq/16);
    }

	    if (tv_param_channel) {
	struct CHANLIST cl;

	mp_msg(MSGT_TV, MSGL_V, "Requested channel: %s\n", tv_param_channel);
	for (i = 0; i < chanlists[tvh->chanlist].count; i++)
	{
	    cl = tvh->chanlist_s[i];
		    //  printf("count%d: name: %s, freq: %d\n",
		    //	i, cl.name, cl.freq);
	    if (!strcasecmp(cl.name, tv_param_channel))
	    {
			strcpy(tv_channel_last_real, cl.name);
		tvh->channel = i;
		mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s (freq: %.3f)\n",
		    cl.name, (float)cl.freq/1000);
		tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
		break;
	    }
	}
    }
    }
    
    /* grep frequency in chanlist */
    {
	unsigned long i2;
	int freq;
	
	tv_get_freq(tvh, &i2);
	
	freq = (int) (((float)(i2/16))*1000)+250;
	
	for (i = 0; i < chanlists[tvh->chanlist].count; i++)
	{
	    if (tvh->chanlist_s[i].freq == freq)
	    {
		tvh->channel = i+1;
		break;
	    }
	}
    }

done:    
    /* also start device! */
	return 1;
}

static demuxer_t* demux_open_tv(demuxer_t *demuxer)
{
    tvi_handle_t *tvh;
    sh_video_t *sh_video;
    sh_audio_t *sh_audio = NULL;
    tvi_functions_t *funcs;
    
    demuxer->priv=NULL;
    if(!(tvh=tv_begin())) return NULL;
    if (!tvh->functions->init(tvh->priv)) return NULL;
    if (!open_tv(tvh)){
	tv_uninit(tvh);
	return NULL;
    }
    funcs = tvh->functions;
    demuxer->priv=tvh;
    
    sh_video = new_sh_video(demuxer, 0);

    /* get IMAGE FORMAT */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FORMAT, &sh_video->format);
//    if (IMGFMT_IS_RGB(sh_video->format) || IMGFMT_IS_BGR(sh_video->format))
//	sh_video->format = 0x0;

    /* set FPS and FRAMETIME */

    if(!sh_video->fps)
    {
        float tmp;
        if (funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FPS, &tmp) != TVI_CONTROL_TRUE)
             sh_video->fps = 25.0f; /* on PAL */
        else sh_video->fps = tmp;
    }

    if (tv_param_fps != -1.0f)
        sh_video->fps = tv_param_fps;

    sh_video->frametime = 1.0f/sh_video->fps;

    /* If playback only mode, go to immediate mode, fail silently */
    if(tv_param_immediate == 1)
        {
        funcs->control(tvh->priv, TVI_CONTROL_IMMEDIATE, 0);
        tv_param_noaudio = 1; 
        }

    /* disable TV audio if -nosound is present */
    if (!demuxer->audio || demuxer->audio->id == -2) {
        tv_param_noaudio = 1; 
    }

    /* set width */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_WIDTH, &sh_video->disp_w);

    /* set height */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &sh_video->disp_h);

    demuxer->video->sh = sh_video;
    sh_video->ds = demuxer->video;
    demuxer->video->id = 0;
    demuxer->seekable = 0;

    /* here comes audio init */
    if (tv_param_noaudio == 0 && funcs->control(tvh->priv, TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
    {
	int audio_format;
	int sh_audio_format;
	char buf[128];

	/* yeah, audio is present */

	funcs->control(tvh->priv, TVI_CONTROL_AUD_SET_SAMPLERATE, 
				  &tv_param_audiorate);

	if (funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_FORMAT, &audio_format) != TVI_CONTROL_TRUE)
	    goto no_audio;

	switch(audio_format)
	{
	    case AF_FORMAT_U8:
	    case AF_FORMAT_S8:
	    case AF_FORMAT_U16_LE:
	    case AF_FORMAT_U16_BE:
	    case AF_FORMAT_S16_LE:
	    case AF_FORMAT_S16_BE:
	    case AF_FORMAT_S32_LE:
	    case AF_FORMAT_S32_BE:
		sh_audio_format = 0x1; /* PCM */
		break;
	    case AF_FORMAT_IMA_ADPCM:
	    case AF_FORMAT_MU_LAW:
	    case AF_FORMAT_A_LAW:
	    case AF_FORMAT_MPEG2:
	    case AF_FORMAT_AC3:
	    default:
		mp_msg(MSGT_TV, MSGL_ERR, "Audio type '%s (%x)' unsupported!\n",
		    af_fmt2str(audio_format, buf, 128), audio_format);
		goto no_audio;
	}
	
	sh_audio = new_sh_audio(demuxer, 0);

	funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_SAMPLERATE, 
                   &sh_audio->samplerate);
	funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_SAMPLESIZE, 
                   &sh_audio->samplesize);
	funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_CHANNELS, 
                   &sh_audio->channels);

	sh_audio->format = sh_audio_format;
	sh_audio->sample_format = audio_format;

	sh_audio->i_bps = sh_audio->o_bps =
	    sh_audio->samplerate * sh_audio->samplesize * 
	    sh_audio->channels;

	// emulate WF for win32 codecs:
	sh_audio->wf = malloc(sizeof(WAVEFORMATEX));
	sh_audio->wf->wFormatTag = sh_audio->format;
	sh_audio->wf->nChannels = sh_audio->channels;
	sh_audio->wf->wBitsPerSample = sh_audio->samplesize * 8;
	sh_audio->wf->nSamplesPerSec = sh_audio->samplerate;
	sh_audio->wf->nBlockAlign = sh_audio->samplesize * sh_audio->channels;
	sh_audio->wf->nAvgBytesPerSec = sh_audio->i_bps;

	mp_msg(MSGT_DECVIDEO, MSGL_V, "  TV audio: %d channels, %d bits, %d Hz\n",
          sh_audio->wf->nChannels, sh_audio->wf->wBitsPerSample,
          sh_audio->wf->nSamplesPerSec);

	demuxer->audio->sh = sh_audio;
	sh_audio->ds = demuxer->audio;
	demuxer->audio->id = 0;
    }
no_audio:

    if(!(funcs->start(tvh->priv))){
	// start failed :(
	tv_uninit(tvh);
	return NULL;
    }

    /* set color eq */
    tv_set_color_options(tvh, TV_COLOR_BRIGHTNESS, tv_param_brightness);
    tv_set_color_options(tvh, TV_COLOR_HUE, tv_param_hue);
    tv_set_color_options(tvh, TV_COLOR_SATURATION, tv_param_saturation);
    tv_set_color_options(tvh, TV_COLOR_CONTRAST, tv_param_contrast);

    return demuxer;
}

static void demux_close_tv(demuxer_t *demuxer)
{
    tvi_handle_t *tvh=(tvi_handle_t*)(demuxer->priv);
    if (!tvh) return;
    tvh->functions->uninit(tvh->priv);
    demuxer->priv=NULL;
}

/* ================== STREAM_TV ===================== */

tvi_handle_t *tv_begin(void)
{
    int i;
    tvi_info_t* info;
    tvi_handle_t* h;
    if(!strcmp(tv_param_driver,"help")){
        mp_msg(MSGT_TV,MSGL_INFO,"Available drivers:\n");
        for(i=0;tvi_driver_list[i];i++){
	    mp_msg(MSGT_TV,MSGL_INFO," %s\t%s",tvi_driver_list[i]->short_name,tvi_driver_list[i]->name);
	    if(tvi_driver_list[i]->comment)
	        mp_msg(MSGT_TV,MSGL_INFO," (%s)",tvi_driver_list[i]->comment);
	    mp_msg(MSGT_TV,MSGL_INFO,"\n");
	}
	return NULL;
    }

    for(i=0;tvi_driver_list[i];i++){
        if (!strcmp(tvi_driver_list[i]->short_name, tv_param_driver)){
            h=tvi_driver_list[i]->tvi_init(tv_param_device,tv_param_adevice);
            if(!h) return NULL;

            mp_msg(MSGT_TV, MSGL_INFO, "Selected driver: %s\n", tvi_driver_list[i]->short_name);
            mp_msg(MSGT_TV, MSGL_INFO, " name: %s\n", tvi_driver_list[i]->name);
            mp_msg(MSGT_TV, MSGL_INFO, " author: %s\n", tvi_driver_list[i]->author);
            if (tvi_driver_list[i]->comment)
                mp_msg(MSGT_TV, MSGL_INFO, " comment: %s\n", tvi_driver_list[i]->comment);
            return h;
        }
    }
    
    mp_msg(MSGT_TV, MSGL_ERR, "No such driver: %s\n", tv_param_driver); 
    return(NULL);
}

int tv_uninit(tvi_handle_t *tvh)
{
    int res;
    if(!tvh) return 1;
    if (!tvh->priv) return 1;
    res=tvh->functions->uninit(tvh->priv);
    if(res) tvh->priv=NULL;
    return res;
}

/* utilities for mplayer (not mencoder!!) */
int tv_set_color_options(tvi_handle_t *tvh, int opt, int value)
{
    tvi_functions_t *funcs = tvh->functions;

    switch(opt)
    {
	case TV_COLOR_BRIGHTNESS:
	    return funcs->control(tvh->priv, TVI_CONTROL_VID_SET_BRIGHTNESS, &value);
	case TV_COLOR_HUE:
	    return funcs->control(tvh->priv, TVI_CONTROL_VID_SET_HUE, &value);
	case TV_COLOR_SATURATION:
	    return funcs->control(tvh->priv, TVI_CONTROL_VID_SET_SATURATION, &value);
	case TV_COLOR_CONTRAST:
	    return funcs->control(tvh->priv, TVI_CONTROL_VID_SET_CONTRAST, &value);
	default:
	    mp_msg(MSGT_TV, MSGL_WARN, "Unknown color option (%d) specified!\n", opt);
    }
    
    return(TVI_CONTROL_UNKNOWN);
}

int tv_get_color_options(tvi_handle_t *tvh, int opt, int* value)
{
    tvi_functions_t *funcs = tvh->functions;

    switch(opt)
    {
	case TV_COLOR_BRIGHTNESS:
	    return funcs->control(tvh->priv, TVI_CONTROL_VID_GET_BRIGHTNESS, value);
	case TV_COLOR_HUE:
	    return funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HUE, value);
	case TV_COLOR_SATURATION:
	    return funcs->control(tvh->priv, TVI_CONTROL_VID_GET_SATURATION, value);
	case TV_COLOR_CONTRAST:
	    return funcs->control(tvh->priv, TVI_CONTROL_VID_GET_CONTRAST, value);
	default:
	    mp_msg(MSGT_TV, MSGL_WARN, "Unknown color option (%d) specified!\n", opt);
    }
    
    return(TVI_CONTROL_UNKNOWN);
}

int tv_get_freq(tvi_handle_t *tvh, unsigned long *freq)
{
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) == TVI_CONTROL_TRUE)
    {
	tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, freq);
	mp_msg(MSGT_TV, MSGL_V, "Current frequency: %lu (%.3f)\n",
	    *freq, (float)*freq/16);
    }
    return(1);
}

int tv_set_freq(tvi_handle_t *tvh, unsigned long freq)
{
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) == TVI_CONTROL_TRUE)
    {
//	unsigned long freq = atof(tv_param_freq)*16;

        /* set freq in MHz */
	tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_FREQ, &freq);

	tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, &freq);
	mp_msg(MSGT_TV, MSGL_V, "Current frequency: %lu (%.3f)\n",
	    freq, (float)freq/16);
    }
    return(1);
}

/*****************************************************************
 * \brief tune current frequency by step_interval value
 * \parameter step_interval increment value in 1/16 MHz
 * \note frequency is rounded to 1/16 MHz value
 * \return 1
 *
 */
int tv_step_freq(tvi_handle_t* tvh, float step_interval){
    unsigned long frequency;

    tv_get_freq(tvh,&frequency);
    frequency+=step_interval;
    return tv_set_freq(tvh,frequency);
}

int tv_step_channel_real(tvi_handle_t *tvh, int direction)
{
    struct CHANLIST cl;

    if (direction == TV_CHANNEL_LOWER)
    {
	if (tvh->channel-1 >= 0)
	{
	    strcpy(tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
	    cl = tvh->chanlist_s[--tvh->channel];
	    mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s (freq: %.3f)\n",
		cl.name, (float)cl.freq/1000);
	    tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
	}	
    }

    if (direction == TV_CHANNEL_HIGHER)
    {
	if (tvh->channel+1 < chanlists[tvh->chanlist].count)
	{
	    strcpy(tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
	    cl = tvh->chanlist_s[++tvh->channel];
	    mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s (freq: %.3f)\n",
		cl.name, (float)cl.freq/1000);
	    tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
	}	
    }
    return(1);
}

int tv_step_channel(tvi_handle_t *tvh, int direction) {
	if (tv_channel_list) {
		if (direction == TV_CHANNEL_HIGHER) {
			tv_channel_last = tv_channel_current;
			if (tv_channel_current->next)
				tv_channel_current = tv_channel_current->next;
			else
				tv_channel_current = tv_channel_list;
				tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
				mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s - %s (freq: %.3f)\n",
			tv_channel_current->number, tv_channel_current->name, (float)tv_channel_current->freq/1000);
		}
		if (direction == TV_CHANNEL_LOWER) {
			tv_channel_last = tv_channel_current;
			if (tv_channel_current->prev)
				tv_channel_current = tv_channel_current->prev;
			else
				while (tv_channel_current->next)
					tv_channel_current = tv_channel_current->next;
				tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
				mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s - %s (freq: %.3f)\n",
			tv_channel_current->number, tv_channel_current->name, (float)tv_channel_current->freq/1000);
		}
	} else tv_step_channel_real(tvh, direction);
	return(1);
}

int tv_set_channel_real(tvi_handle_t *tvh, char *channel) {
	int i;
	struct CHANLIST cl;

        strcpy(tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
	for (i = 0; i < chanlists[tvh->chanlist].count; i++)
	{
	    cl = tvh->chanlist_s[i];
//	    printf("count%d: name: %s, freq: %d\n",
//		i, cl.name, cl.freq);
	    if (!strcasecmp(cl.name, channel))
	    {
		tvh->channel = i;
		mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s (freq: %.3f)\n",
		    cl.name, (float)cl.freq/1000);
		tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
		break;
	    }
	}
	return(1);
}

int tv_set_channel(tvi_handle_t *tvh, char *channel) {
	int i, channel_int;

	if (tv_channel_list) {
		tv_channel_last = tv_channel_current;
		channel_int = atoi(channel);
		tv_channel_current = tv_channel_list;
		for (i = 1; i < channel_int; i++)
			if (tv_channel_current->next)
				tv_channel_current = tv_channel_current->next;
		mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s - %s (freq: %.3f)\n", tv_channel_current->number,
				tv_channel_current->name, (float)tv_channel_current->freq/1000);
		tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
	} else tv_set_channel_real(tvh, channel);
	return(1);
}

int tv_last_channel(tvi_handle_t *tvh) {

	if (tv_channel_list) {
		tv_channels_t *tmp;

		tmp = tv_channel_last;
		tv_channel_last = tv_channel_current;
		tv_channel_current = tmp;

		mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s - %s (freq: %.3f)\n", tv_channel_current->number,
				tv_channel_current->name, (float)tv_channel_current->freq/1000);
		tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
	} else {
		int i;
		struct CHANLIST cl;

		for (i = 0; i < chanlists[tvh->chanlist].count; i++)
		{
		    cl = tvh->chanlist_s[i];
		    if (!strcasecmp(cl.name, tv_channel_last_real))
		    {
			strcpy(tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
			tvh->channel = i;
			mp_msg(MSGT_TV, MSGL_INFO, "Selected channel: %s (freq: %.3f)\n",
			    cl.name, (float)cl.freq/1000);
			tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
			break;
		    }
		}
	}
	return(1);
}

int tv_step_norm(tvi_handle_t *tvh)
{
  tvh->norm++;
  if (tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM,
                              &tvh->norm) != TVI_CONTROL_TRUE) {
    tvh->norm = 0;
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM,
                                &tvh->norm) != TVI_CONTROL_TRUE) {
      mp_msg(MSGT_TV, MSGL_ERR, "Error: Cannot set norm!\n");
      return 0;
    }
  }
    return(1);
}

int tv_step_chanlist(tvi_handle_t *tvh)
{
    return(1);
}

int tv_set_norm(tvi_handle_t *tvh, char* norm)
{
    tvh->norm = norm_from_string(tvh, norm);

    mp_msg(MSGT_TV, MSGL_V, "Selected norm: %s\n", tv_param_norm);
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM, &tvh->norm) != TVI_CONTROL_TRUE) {
	mp_msg(MSGT_TV, MSGL_ERR, "Error: Cannot set norm!\n");
	return 0;
    }
    return(1);
}

demuxer_desc_t demuxer_desc_tv = {
  "Tv card demuxer",
  "tv",
  "TV",
  "Alex Beregszaszi, Charles R. Henrich",
  "?",
  DEMUXER_TYPE_TV,
  0, // no autodetect
  NULL,
  demux_tv_fill_buffer,
  demux_open_tv,
  demux_close_tv,
  NULL,
  NULL
};
