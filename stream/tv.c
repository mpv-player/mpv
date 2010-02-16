/*
 * TV Interface for MPlayer
 *
 * API idea based on libvo2
 *
 * Copyright (C) 2001 Alex Beregszaszi
 *
 * Feb 19, 2002: Significant rewrites by Charles R. Henrich (henrich@msu.edu)
 *               to add support for audio, and bktr *BSD support.
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
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include "config.h"


#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "libaf/af_format.h"
#include "libmpcodecs/img_format.h"
#include "libmpcodecs/dec_teletext.h"
#include "libavutil/avstring.h"
#include "osdep/timer.h"

#include "tv.h"

#include "frequencies.h"

tv_channels_t *tv_channel_list;
tv_channels_t *tv_channel_current, *tv_channel_last;
char *tv_channel_last_real;

/* enumerating drivers (like in stream.c) */
extern const tvi_info_t tvi_info_dummy;
extern const tvi_info_t tvi_info_dshow;
extern const tvi_info_t tvi_info_v4l;
extern const tvi_info_t tvi_info_v4l2;
extern const tvi_info_t tvi_info_bsdbt848;

/** List of drivers in autodetection order */
static const tvi_info_t* tvi_driver_list[]={
#ifdef CONFIG_TV_V4L2
    &tvi_info_v4l2,
#endif
#ifdef CONFIG_TV_V4L1
    &tvi_info_v4l,
#endif
#ifdef CONFIG_TV_BSDBT848
    &tvi_info_bsdbt848,
#endif
#ifdef CONFIG_TV_DSHOW
    &tvi_info_dshow,
#endif
    &tvi_info_dummy,
    NULL
};

void tv_start_scan(tvi_handle_t *tvh, int start)
{
    mp_msg(MSGT_TV,MSGL_INFO,"start scan\n");
    tvh->tv_param->scan=start?1:0;
}

static void tv_scan(tvi_handle_t *tvh)
{
    unsigned int now;
    struct CHANLIST cl;
    tv_channels_t *tv_channel_tmp=NULL;
    tv_channels_t *tv_channel_add=NULL;
    tv_scan_t* scan;
    int found=0, index=1;

    //Channel scanner without tuner is useless and causes crash due to uninitialized chanlist_s
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) != TVI_CONTROL_TRUE)
    {
        mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TV_ScannerNotAvailableWithoutTuner);
        tvh->tv_param->scan=0;
        return;
    }

    scan = tvh->scan;
    now=GetTimer();
    if (!scan) {
        scan=calloc(1,sizeof(tv_scan_t));
        tvh->scan=scan;
        cl = tvh->chanlist_s[scan->channel_num];
        tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
        scan->scan_timer=now+1e6*tvh->tv_param->scan_period;
    }
    if(scan->scan_timer>now)
        return;

    if (tv_get_signal(tvh)>tvh->tv_param->scan_threshold) {
        cl = tvh->chanlist_s[scan->channel_num];
        tv_channel_tmp=tv_channel_list;
        while (tv_channel_tmp) {
            index++;
            if (cl.freq==tv_channel_tmp->freq){
                found=1;
                break;
            }
            tv_channel_add=tv_channel_tmp;
            tv_channel_tmp=tv_channel_tmp->next;
        }
        if (!found) {
            mp_msg(MSGT_TV, MSGL_INFO, "Found new channel: %s (#%d). \n",cl.name,index);
            scan->new_channels++;
            tv_channel_tmp = malloc(sizeof(tv_channels_t));
            tv_channel_tmp->index=index;
            tv_channel_tmp->next=NULL;
            tv_channel_tmp->prev=tv_channel_add;
            tv_channel_tmp->freq=cl.freq;
            snprintf(tv_channel_tmp->name,sizeof(tv_channel_tmp->name),"ch%d",index);
            strncpy(tv_channel_tmp->number, cl.name, 5);
            tv_channel_tmp->number[4]='\0';
            if (!tv_channel_list)
                tv_channel_list=tv_channel_tmp;
            else {
                tv_channel_add->next=tv_channel_tmp;
                tv_channel_list->prev=tv_channel_tmp;
            }
        }else
            mp_msg(MSGT_TV, MSGL_INFO, "Found existing channel: %s-%s.\n",
                tv_channel_tmp->number,tv_channel_tmp->name);
    }
    scan->channel_num++;
    scan->scan_timer=now+1e6*tvh->tv_param->scan_period;
    if (scan->channel_num>=chanlists[tvh->chanlist].count) {
        tvh->tv_param->scan=0;
        mp_msg(MSGT_TV, MSGL_INFO, "TV scan end. Found %d new channels.\n", scan->new_channels);
        tv_channel_tmp=tv_channel_list;
        if(tv_channel_tmp){
            mp_msg(MSGT_TV,MSGL_INFO,"channels=");
            while(tv_channel_tmp){
                mp_msg(MSGT_TV,MSGL_INFO,"%s-%s",tv_channel_tmp->number,tv_channel_tmp->name);
                if(tv_channel_tmp->next)
                    mp_msg(MSGT_TV,MSGL_INFO,",");
                tv_channel_tmp=tv_channel_tmp->next;
            }
            mp_msg(MSGT_TV, MSGL_INFO, "\n");
        }
        if (!tv_channel_current) tv_channel_current=tv_channel_list;
        if (tv_channel_current)
            tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
        free(tvh->scan);
        tvh->scan=NULL;
    }else{
        cl = tvh->chanlist_s[scan->channel_num];
        tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
        mp_msg(MSGT_TV, MSGL_INFO, "Trying: %s (%.2f). \n",cl.name,1e-3*cl.freq);
    }
}

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

    if (ds==demux->audio && tvh->tv_param->noaudio == 0 &&
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

    if (tvh->tv_param->scan) tv_scan(tvh);
    return 1;
}

static int norm_from_string(tvi_handle_t *tvh, char* norm)
{
    const tvi_functions_t *funcs = tvh->functions;
    char str[20];
    int ret;

    strncpy(str, norm, sizeof(str)-1);
    str[sizeof(str)-1] = '\0';
    ret=funcs->control(tvh->priv, TVI_CONTROL_SPC_GET_NORMID, str);

    if(ret==TVI_CONTROL_TRUE)
        return *(int *)str;

    if(ret!=TVI_CONTROL_UNKNOWN)
    {
        mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TV_BogusNormParameter, norm,"default");
        return 0;
    }

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
        mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TV_BogusNormParameter, norm, "PAL");
        return TV_NORM_PAL;
    }
}

static void parse_channels(tvi_handle_t *tvh)
{
    char** channels = tvh->tv_param->channels;

    mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_ChannelNamesDetected);
    tv_channel_list = malloc(sizeof(tv_channels_t));
    tv_channel_list->index=1;
    tv_channel_list->next=NULL;
    tv_channel_list->prev=NULL;
    tv_channel_current = tv_channel_list;
    tv_channel_current->norm = tvh->norm;

    while (*channels) {
        char* tmp = *(channels++);
        char* sep = strchr(tmp,'-');
        int i;
        struct CHANLIST cl;

        if (!sep) continue; // Wrong syntax, but mplayer should not crash

        av_strlcpy(tv_channel_current->name, sep + 1,
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
            mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_NoFreqForChannel,
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

          sep = strchr(tv_channel_current->name, '=');
          if ( sep ) {
            tv_channel_current->norm = norm_from_string(tvh, sep+1);
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
        tv_channel_current->norm = tvh->norm;
    }
    if (tv_channel_current->prev)
        tv_channel_current->prev->next = NULL;
    free(tv_channel_current);
}

int tv_set_norm(tvi_handle_t *tvh, char* norm)
{
    tvh->norm = norm_from_string(tvh, norm);

    mp_msg(MSGT_TV, MSGL_V, MSGTR_TV_SelectedNorm, norm);
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM, &tvh->norm) != TVI_CONTROL_TRUE) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_CannotSetNorm);
	return 0;
    }
    teletext_control(tvh->demuxer->teletext,TV_VBI_CONTROL_RESET,
                     &tvh->tv_param->teletext);
    return 1;
}

static int tv_set_norm_i(tvi_handle_t *tvh, int norm)
{
   tvh->norm = norm;

   mp_msg(MSGT_TV, MSGL_V, MSGTR_TV_SelectedNormId, norm);
   if (tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM, &tvh->norm) != TVI_CONTROL_TRUE) {
      mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_CannotSetNorm);
      return 0;
   }

   teletext_control(tvh->demuxer->teletext,TV_VBI_CONTROL_RESET,
                    &tvh->tv_param->teletext);
   return 1;
}

static int open_tv(tvi_handle_t *tvh)
{
    int i;
    const tvi_functions_t *funcs = tvh->functions;
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
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_NoVideoInputPresent);
	return 0;
    }

    if (tvh->tv_param->outfmt == -1)
      for (i = 0; i < sizeof (tv_fmt_list) / sizeof (*tv_fmt_list); i++)
        {
          tvh->tv_param->outfmt = tv_fmt_list[i];
          if (funcs->control (tvh->priv, TVI_CONTROL_VID_SET_FORMAT,
                              &tvh->tv_param->outfmt) == TVI_CONTROL_TRUE)
            break;
        }
    else
    {
    switch(tvh->tv_param->outfmt)
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
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_UnknownImageFormat,tvh->tv_param->outfmt);
    }
    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_FORMAT, &tvh->tv_param->outfmt);
    }

    /* set some params got from cmdline */
    funcs->control(tvh->priv, TVI_CONTROL_SPC_SET_INPUT, &tvh->tv_param->input);

#if defined(CONFIG_TV_V4L2) || defined(CONFIG_TV_DSHOW)
    if (0
#ifdef CONFIG_TV_V4L2
    || (!strcmp(tvh->tv_param->driver, "v4l2") && tvh->tv_param->normid >= 0)
#endif
#ifdef CONFIG_TV_DSHOW
    || (!strcmp(tvh->tv_param->driver, "dshow") && tvh->tv_param->normid >= 0)
#endif
    )
	tv_set_norm_i(tvh, tvh->tv_param->normid);
    else
#endif
    tv_set_norm(tvh,tvh->tv_param->norm);

#ifdef CONFIG_TV_V4L1
    if ( tvh->tv_param->mjpeg )
    {
      /* set width to expected value */
      if (tvh->tv_param->width == -1)
        {
          tvh->tv_param->width = 704/tvh->tv_param->decimation;
        }
      if (tvh->tv_param->height == -1)
        {
	  if ( tvh->norm != TV_NORM_NTSC )
            tvh->tv_param->height = 576/tvh->tv_param->decimation;
	  else
            tvh->tv_param->height = 480/tvh->tv_param->decimation;
        }
      mp_msg(MSGT_TV, MSGL_INFO,
	       MSGTR_TV_MJP_WidthHeight, tvh->tv_param->width, tvh->tv_param->height);
    }
#endif

    /* limits on w&h are norm-dependent -- JM */
    if (tvh->tv_param->width != -1 && tvh->tv_param->height != -1) {
        // first tell the driver both width and height, some drivers do not support setting them independently.
        int dim[2];
        dim[0] = tvh->tv_param->width; dim[1] = tvh->tv_param->height;
        funcs->control(tvh->priv, TVI_CONTROL_VID_SET_WIDTH_HEIGHT, dim);
    }
    /* set width */
    if (tvh->tv_param->width != -1)
    {
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_CHK_WIDTH, &tvh->tv_param->width) == TVI_CONTROL_TRUE)
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_WIDTH, &tvh->tv_param->width);
	else
	{
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_UnableToSetWidth, tvh->tv_param->width);
	    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_WIDTH, &tvh->tv_param->width);
	}
    }

    /* set height */
    if (tvh->tv_param->height != -1)
    {
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_CHK_HEIGHT, &tvh->tv_param->height) == TVI_CONTROL_TRUE)
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_HEIGHT, &tvh->tv_param->height);
	else
	{
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_UnableToSetHeight, tvh->tv_param->height);
	    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &tvh->tv_param->height);
	}
    }

    if (funcs->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) != TVI_CONTROL_TRUE)
    {
	mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TV_NoTuner);
	goto done;
    }

    /* select channel list */
    for (i = 0; chanlists[i].name != NULL; i++)
    {
	if (!strcasecmp(chanlists[i].name, tvh->tv_param->chanlist))
	{
	    tvh->chanlist = i;
	    tvh->chanlist_s = chanlists[i].list;
	    break;
	}
    }

    if (tvh->chanlist == -1)
	mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TV_UnableFindChanlist,
	    tvh->tv_param->chanlist);
    else
	mp_msg(MSGT_TV, MSGL_V, MSGTR_TV_SelectedChanlist,
	    chanlists[tvh->chanlist].name, chanlists[tvh->chanlist].count);

    if (tvh->tv_param->freq && tvh->tv_param->channel)
    {
	mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TV_ChannelFreqParamConflict);
	goto done;
    }

    /* Handle channel names */
    if (tvh->tv_param->channels) {
        parse_channels(tvh);
    } else
	    tv_channel_last_real = malloc(5);

    if (tv_channel_list) {
	int i;
	int channel = 0;
	if (tvh->tv_param->channel)
	 {
	   if (isdigit(*tvh->tv_param->channel))
		/* if tvh->tv_param->channel begins with a digit interpret it as a number */
		channel = atoi(tvh->tv_param->channel);
	   else
	      {
		/* if tvh->tv_param->channel does not begin with a digit
		   set the first channel that contains tvh->tv_param->channel in its name */

		tv_channel_current = tv_channel_list;
		while ( tv_channel_current ) {
			if ( strstr(tv_channel_current->name, tvh->tv_param->channel) )
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

	mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel3, tv_channel_current->number,
			tv_channel_current->name, (float)tv_channel_current->freq/1000);
	tv_set_norm_i(tvh, tv_channel_current->norm);
	tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
	tv_channel_last = tv_channel_current;
    } else {
    /* we need to set frequency */
    if (tvh->tv_param->freq)
    {
	unsigned long freq = atof(tvh->tv_param->freq)*16;

        /* set freq in MHz */
	funcs->control(tvh->priv, TVI_CONTROL_TUN_SET_FREQ, &freq);

	funcs->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, &freq);
	mp_msg(MSGT_TV, MSGL_V, MSGTR_TV_SelectedFrequency,
	    freq, (float)freq/16);
    }

	    if (tvh->tv_param->channel) {
	struct CHANLIST cl;

	mp_msg(MSGT_TV, MSGL_V, MSGTR_TV_RequestedChannel, tvh->tv_param->channel);
	for (i = 0; i < chanlists[tvh->chanlist].count; i++)
	{
	    cl = tvh->chanlist_s[i];
		    //  printf("count%d: name: %s, freq: %d\n",
		    //	i, cl.name, cl.freq);
	    if (!strcasecmp(cl.name, tvh->tv_param->channel))
	    {
			strcpy(tv_channel_last_real, cl.name);
		tvh->channel = i;
		mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel2,
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

static tvi_handle_t *tv_begin(tv_param_t* tv_param)
{
    int i;
    tvi_handle_t* h;
    if(tv_param->driver && !strcmp(tv_param->driver,"help")){
        mp_msg(MSGT_TV,MSGL_INFO,MSGTR_TV_AvailableDrivers);
        for(i=0;tvi_driver_list[i];i++){
	    mp_msg(MSGT_TV,MSGL_INFO," %s\t%s",tvi_driver_list[i]->short_name,tvi_driver_list[i]->name);
	    if(tvi_driver_list[i]->comment)
	        mp_msg(MSGT_TV,MSGL_INFO," (%s)",tvi_driver_list[i]->comment);
	    mp_msg(MSGT_TV,MSGL_INFO,"\n");
	}
	return NULL;
    }

    for(i=0;tvi_driver_list[i];i++){
        if (!tv_param->driver || !strcmp(tvi_driver_list[i]->short_name, tv_param->driver)){
            h=tvi_driver_list[i]->tvi_init(tv_param);
            //Requested driver initialization failed
            if (!h && tv_param->driver)
                return NULL;
            //Driver initialization failed during autodetection process.
            if (!h)
                continue;

            h->tv_param=tv_param;
            mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_DriverInfo, tvi_driver_list[i]->short_name,
            tvi_driver_list[i]->name,
            tvi_driver_list[i]->author,
            tvi_driver_list[i]->comment?tvi_driver_list[i]->comment:"");
            tv_param->driver=strdup(tvi_driver_list[i]->short_name);
            return h;
        }
    }

    if(tv_param->driver)
        mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_NoSuchDriver, tv_param->driver);
    else
        mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_DriverAutoDetectionFailed);
    return NULL;
}

static int tv_uninit(tvi_handle_t *tvh)
{
    int res;
    if(!tvh) return 1;
    if (!tvh->priv) return 1;
    res=tvh->functions->uninit(tvh->priv);
    if(res) {
        free(tvh->priv);
        tvh->priv=NULL;
    }
    return res;
}

static demuxer_t* demux_open_tv(demuxer_t *demuxer)
{
    tvi_handle_t *tvh;
    sh_video_t *sh_video;
    sh_audio_t *sh_audio = NULL;
    const tvi_functions_t *funcs;

    demuxer->priv=NULL;
    if(!(tvh=tv_begin(demuxer->stream->priv))) return NULL;
    if (!tvh->functions->init(tvh->priv)) return NULL;

    tvh->demuxer = demuxer;
    tvh->functions->control(tvh->priv,TVI_CONTROL_VBI_INIT,
                            &(tvh->tv_param->teletext.device));
    tvh->functions->control(tvh->priv,TVI_CONTROL_GET_VBI_PTR,
                            &demuxer->teletext);

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

    if (tvh->tv_param->fps != -1.0f)
        sh_video->fps = tvh->tv_param->fps;

    sh_video->frametime = 1.0f/sh_video->fps;

    /* If playback only mode, go to immediate mode, fail silently */
    if(tvh->tv_param->immediate == 1)
        {
        funcs->control(tvh->priv, TVI_CONTROL_IMMEDIATE, 0);
        tvh->tv_param->noaudio = 1;
        }

    /* disable TV audio if -nosound is present */
    if (!demuxer->audio || demuxer->audio->id == -2) {
        tvh->tv_param->noaudio = 1;
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
    if (tvh->tv_param->noaudio == 0 && funcs->control(tvh->priv, TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
    {
	int audio_format;
	int sh_audio_format;
	char buf[128];

	/* yeah, audio is present */

	funcs->control(tvh->priv, TVI_CONTROL_AUD_SET_SAMPLERATE,
				  &tvh->tv_param->audiorate);

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
	    default:
		mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_UnsupportedAudioType,
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

	mp_msg(MSGT_DECVIDEO, MSGL_V, MSGTR_TV_AudioFormat,
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
    tv_set_color_options(tvh, TV_COLOR_BRIGHTNESS, tvh->tv_param->brightness);
    tv_set_color_options(tvh, TV_COLOR_HUE, tvh->tv_param->hue);
    tv_set_color_options(tvh, TV_COLOR_SATURATION, tvh->tv_param->saturation);
    tv_set_color_options(tvh, TV_COLOR_CONTRAST, tvh->tv_param->contrast);

    if(tvh->tv_param->gain!=-1)
        if(funcs->control(tvh->priv,TVI_CONTROL_VID_SET_GAIN,&tvh->tv_param->gain)!=TVI_CONTROL_TRUE)
            mp_msg(MSGT_TV,MSGL_WARN,"Unable to set gain control!\n");

    teletext_control(demuxer->teletext,TV_VBI_CONTROL_RESET,
                     &tvh->tv_param->teletext);

    return demuxer;
}

static void demux_close_tv(demuxer_t *demuxer)
{
    tvi_handle_t *tvh=(tvi_handle_t*)(demuxer->priv);
    if (!tvh) return;
    tv_uninit(tvh);
    free(tvh);
    demuxer->priv=NULL;
    demuxer->teletext=NULL;
}

/* utilities for mplayer (not mencoder!!) */
int tv_set_color_options(tvi_handle_t *tvh, int opt, int value)
{
    const tvi_functions_t *funcs = tvh->functions;

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
	    mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TV_UnknownColorOption, opt);
    }

    return TVI_CONTROL_UNKNOWN;
}

int tv_get_color_options(tvi_handle_t *tvh, int opt, int* value)
{
    const tvi_functions_t *funcs = tvh->functions;

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
	    mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TV_UnknownColorOption, opt);
    }

    return TVI_CONTROL_UNKNOWN;
}

int tv_get_freq(tvi_handle_t *tvh, unsigned long *freq)
{
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) == TVI_CONTROL_TRUE)
    {
	tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, freq);
	mp_msg(MSGT_TV, MSGL_V, MSGTR_TV_CurrentFrequency,
	    *freq, (float)*freq/16);
    }
    return 1;
}

int tv_set_freq(tvi_handle_t *tvh, unsigned long freq)
{
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) == TVI_CONTROL_TRUE)
    {
//	unsigned long freq = atof(tvh->tv_param->freq)*16;

        /* set freq in MHz */
	tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_FREQ, &freq);

	tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, &freq);
	mp_msg(MSGT_TV, MSGL_V, MSGTR_TV_CurrentFrequency,
	    freq, (float)freq/16);
    }
    teletext_control(tvh->demuxer->teletext,TV_VBI_CONTROL_RESET,
                     &tvh->tv_param->teletext);
    return 1;
}

int tv_get_signal(tvi_handle_t *tvh)
{
    int signal=0;
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) != TVI_CONTROL_TRUE ||
        tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_GET_SIGNAL, &signal)!=TVI_CONTROL_TRUE)
        return 0;

    return signal;
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

    tvh->tv_param->scan=0;
    tv_get_freq(tvh,&frequency);
    frequency+=step_interval;
    return tv_set_freq(tvh,frequency);
}

int tv_step_channel_real(tvi_handle_t *tvh, int direction)
{
    struct CHANLIST cl;

    tvh->tv_param->scan=0;
    if (direction == TV_CHANNEL_LOWER)
    {
	if (tvh->channel-1 >= 0)
	{
	    strcpy(tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
	    cl = tvh->chanlist_s[--tvh->channel];
	    mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel2,
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
	    mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel2,
		cl.name, (float)cl.freq/1000);
	    tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
	}
    }
    return 1;
}

int tv_step_channel(tvi_handle_t *tvh, int direction) {
	tvh->tv_param->scan=0;
	if (tv_channel_list) {
		if (direction == TV_CHANNEL_HIGHER) {
			tv_channel_last = tv_channel_current;
			if (tv_channel_current->next)
				tv_channel_current = tv_channel_current->next;
			else
				tv_channel_current = tv_channel_list;

				tv_set_norm_i(tvh, tv_channel_current->norm);
				tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
				mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel3,
			tv_channel_current->number, tv_channel_current->name, (float)tv_channel_current->freq/1000);
		}
		if (direction == TV_CHANNEL_LOWER) {
			tv_channel_last = tv_channel_current;
			if (tv_channel_current->prev)
				tv_channel_current = tv_channel_current->prev;
			else
				while (tv_channel_current->next)
					tv_channel_current = tv_channel_current->next;
				tv_set_norm_i(tvh, tv_channel_current->norm);
				tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
				mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel3,
			tv_channel_current->number, tv_channel_current->name, (float)tv_channel_current->freq/1000);
		}
	} else tv_step_channel_real(tvh, direction);
	return 1;
}

int tv_set_channel_real(tvi_handle_t *tvh, char *channel) {
	int i;
	struct CHANLIST cl;

        tvh->tv_param->scan=0;
        strcpy(tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
	for (i = 0; i < chanlists[tvh->chanlist].count; i++)
	{
	    cl = tvh->chanlist_s[i];
//	    printf("count%d: name: %s, freq: %d\n",
//		i, cl.name, cl.freq);
	    if (!strcasecmp(cl.name, channel))
	    {
		tvh->channel = i;
		mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel2,
		    cl.name, (float)cl.freq/1000);
		tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
		break;
	    }
	}
	return 1;
}

int tv_set_channel(tvi_handle_t *tvh, char *channel) {
	int i, channel_int;

	tvh->tv_param->scan=0;
	if (tv_channel_list) {
		tv_channel_last = tv_channel_current;
		channel_int = atoi(channel);
		tv_channel_current = tv_channel_list;
		for (i = 1; i < channel_int; i++)
			if (tv_channel_current->next)
				tv_channel_current = tv_channel_current->next;
		mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel3, tv_channel_current->number,
				tv_channel_current->name, (float)tv_channel_current->freq/1000);
		tv_set_norm_i(tvh, tv_channel_current->norm);
		tv_set_freq(tvh, (unsigned long)(((float)tv_channel_current->freq/1000)*16));
	} else tv_set_channel_real(tvh, channel);
	return 1;
}

int tv_last_channel(tvi_handle_t *tvh) {

	tvh->tv_param->scan=0;
	if (tv_channel_list) {
		tv_channels_t *tmp;

		tmp = tv_channel_last;
		tv_channel_last = tv_channel_current;
		tv_channel_current = tmp;

		mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel3, tv_channel_current->number,
				tv_channel_current->name, (float)tv_channel_current->freq/1000);
		tv_set_norm_i(tvh, tv_channel_current->norm);
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
			mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TV_SelectedChannel2,
			    cl.name, (float)cl.freq/1000);
			tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
			break;
		    }
		}
	}
	return 1;
}

int tv_step_norm(tvi_handle_t *tvh)
{
  tvh->norm++;
  if (tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM,
                              &tvh->norm) != TVI_CONTROL_TRUE) {
    tvh->norm = 0;
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM,
                                &tvh->norm) != TVI_CONTROL_TRUE) {
      mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TV_CannotSetNorm);
      return 0;
    }
  }
    teletext_control(tvh->demuxer->teletext,TV_VBI_CONTROL_RESET,
                     &tvh->tv_param->teletext);
    return 1;
}

int tv_step_chanlist(tvi_handle_t *tvh)
{
    return 1;
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
