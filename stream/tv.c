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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <assert.h>
#include <libavutil/avstring.h>

#include "config.h"


#include "common/msg.h"
#include "misc/ctype.h"

#include "options/m_option.h"
#include "options/m_config.h"
#include "options/options.h"

#include "stream.h"

#include "audio/format.h"
#include "video/img_fourcc.h"
#include "osdep/timer.h"

#include "tv.h"

#include "frequencies.h"

/* enumerating drivers (like in stream.c) */
extern const tvi_info_t tvi_info_dummy;
extern const tvi_info_t tvi_info_v4l2;

/** List of drivers in autodetection order */
static const tvi_info_t *const tvi_driver_list[]={
#if HAVE_TV_V4L2
    &tvi_info_v4l2,
#endif
    &tvi_info_dummy,
    NULL
};

#define OPT_BASE_STRUCT tv_param_t
const struct m_sub_options tv_params_conf = {
    .opts = (const m_option_t[]) {
        OPT_FLAG("immediatemode", immediate, 0),
        OPT_FLAG("audio", audio, 0),
        OPT_INT("audiorate", audiorate, 0),
        OPT_STRING("driver", driver, 0),
        OPT_STRING("device", device, 0),
        OPT_STRING("freq", freq, 0),
        OPT_STRING("channel", channel, 0),
        OPT_STRING("chanlist", chanlist, 0),
        OPT_STRING("norm", norm, 0),
        OPT_INTRANGE("automute", automute, 0, 0, 255),
#if HAVE_TV_V4L2
        OPT_INT("normid", normid, 0),
#endif
        OPT_INTRANGE("width", width, 0, 0, 4096),
        OPT_INTRANGE("height", height, 0, 0, 4096),
        OPT_INT("input", input, 0),
        OPT_GENERAL(int, "outfmt", outfmt, 0, .type = &m_option_type_fourcc),
        OPT_FLOAT("fps", fps, 0),
        OPT_STRINGLIST("channels", channels, 0),
        OPT_INTRANGE("brightness", brightness, 0, -100, 100),
        OPT_INTRANGE("contrast", contrast, 0, -100, 100),
        OPT_INTRANGE("hue", hue, 0, -100, 100),
        OPT_INTRANGE("saturation", saturation, 0, -100, 100),
        OPT_INTRANGE("gain", gain, 0, -1, 100),
#if HAVE_TV_V4L2
        OPT_INTRANGE("amode", amode, 0, 0, 3),
        OPT_INTRANGE("volume", volume, 0, 0, 65535),
        OPT_INTRANGE("bass", bass, 0, 0, 65535),
        OPT_INTRANGE("treble", treble, 0, 0, 65535),
        OPT_INTRANGE("balance", balance, 0, 0, 65535),
        OPT_INTRANGE("forcechan", forcechan, 0, 1, 2),
        OPT_FLAG("forceaudio", force_audio, 0),
        OPT_INTRANGE("buffersize", buffer_size, 0, 16, 1024),
        OPT_FLAG("mjpeg", mjpeg, 0),
        OPT_INTRANGE("decimation", decimation, 0, 1, 4),
        OPT_INTRANGE("quality", quality, 0, 0, 100),
#if HAVE_ALSA
        OPT_FLAG("alsa", alsa, 0),
#endif /* HAVE_ALSA */
#endif /* HAVE_TV_V4L2 */
        OPT_STRING("adevice", adevice, 0),
        OPT_INTRANGE("audioid", audio_id, 0, 0, 9),
        OPT_FLAG("scan-autostart", scan, 0),
        OPT_INTRANGE("scan-threshold", scan_threshold, 0, 1, 100),
        OPT_FLOATRANGE("scan-period", scan_period, 0, 0.1, 2.0),
        {0}
    },
    .size = sizeof(tv_param_t),
    .defaults = &(const tv_param_t){
        .chanlist = "europe-east",
        .norm = "pal",
        .normid = -1,
        .width = -1,
        .height = -1,
        .outfmt = -1,
        .fps = -1.0,
        .audio = 1,
        .immediate = 1,
        .audiorate = 44100,
        .amode = -1,
        .volume = -1,
        .bass = -1,
        .treble = -1,
        .balance = -1,
        .forcechan = -1,
        .buffer_size = -1,
        .decimation = 2,
        .quality = 90,
        .gain =  -1,
        .scan_threshold = 50,
        .scan_period = 0.5,
    },
};

tvi_handle_t *tv_new_handle(int size, struct mp_log *log, const tvi_functions_t *functions)
{
    tvi_handle_t *h = malloc(sizeof(*h));

    if (!h)
        return NULL;

    h->priv = calloc(1, size);

    if (!h->priv) {
        free(h);
        return NULL;
    }

    h->log        = log;
    h->functions  = functions;
    h->seq        = 0;
    h->chanlist   = -1;
    h->chanlist_s = NULL;
    h->norm       = -1;
    h->channel    = -1;
    h->scan       = NULL;

    return h;
}

void tv_free_handle(tvi_handle_t *h)
{
    if (!h)
        return;
    free(h->priv);
    free(h->scan);
    free(h);
}

void tv_start_scan(tvi_handle_t *tvh, int start)
{
    MP_INFO(tvh, "start scan\n");
    tvh->tv_param->scan=start?1:0;
}

static int tv_set_freq_float(tvi_handle_t *tvh, float freq)
{
    return tv_set_freq(tvh, freq/1000.0*16);
}

void tv_scan(tvi_handle_t *tvh)
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
        MP_WARN(tvh, "Channel scanner is not available without tuner\n");
        tvh->tv_param->scan=0;
        return;
    }

    scan = tvh->scan;
    now=(unsigned int)mp_time_us();
    if (!scan) {
        scan=calloc(1,sizeof(tv_scan_t));
        tvh->scan=scan;
        cl = tvh->chanlist_s[scan->channel_num];
        tv_set_freq_float(tvh, cl.freq);
        scan->scan_timer=now+1e6*tvh->tv_param->scan_period;
    }
    if(scan->scan_timer>now)
        return;

    if (tv_get_signal(tvh)>tvh->tv_param->scan_threshold) {
        cl = tvh->chanlist_s[scan->channel_num];
        tv_channel_tmp=tvh->tv_channel_list;
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
            MP_INFO(tvh, "Found new channel: %s (#%d). \n",cl.name,index);
            scan->new_channels++;
            tv_channel_tmp = malloc(sizeof(tv_channels_t));
            tv_channel_tmp->index=index;
            tv_channel_tmp->next=NULL;
            tv_channel_tmp->prev=tv_channel_add;
            tv_channel_tmp->freq=cl.freq;
            snprintf(tv_channel_tmp->name,sizeof(tv_channel_tmp->name),"ch%d",index);
            strncpy(tv_channel_tmp->number, cl.name, 5);
            tv_channel_tmp->number[4]='\0';
            if (!tvh->tv_channel_list)
                tvh->tv_channel_list=tv_channel_tmp;
            else {
                tv_channel_add->next=tv_channel_tmp;
                tvh->tv_channel_list->prev=tv_channel_tmp;
            }
        }else
            MP_INFO(tvh, "Found existing channel: %s-%s.\n",
                tv_channel_tmp->number,tv_channel_tmp->name);
    }
    scan->channel_num++;
    scan->scan_timer=now+1e6*tvh->tv_param->scan_period;
    if (scan->channel_num>=chanlists[tvh->chanlist].count) {
        tvh->tv_param->scan=0;
        MP_INFO(tvh, "TV scan end. Found %d new channels.\n", scan->new_channels);
        tv_channel_tmp=tvh->tv_channel_list;
        if(tv_channel_tmp){
            MP_INFO(tvh, "channels=");
            while(tv_channel_tmp){
                MP_INFO(tvh, "%s-%s",tv_channel_tmp->number,tv_channel_tmp->name);
                if(tv_channel_tmp->next)
                    MP_INFO(tvh, ",");
                tv_channel_tmp=tv_channel_tmp->next;
            }
            MP_INFO(tvh, "\n");
        }
        if (!tvh->tv_channel_current) tvh->tv_channel_current=tvh->tv_channel_list;
        if (tvh->tv_channel_current)
            tv_set_freq_float(tvh, tvh->tv_channel_current->freq);
        free(tvh->scan);
        tvh->scan=NULL;
    }else{
        cl = tvh->chanlist_s[scan->channel_num];
        tv_set_freq_float(tvh, cl.freq);
        MP_INFO(tvh, "Trying: %s (%.2f). \n",cl.name,1e-3*cl.freq);
    }
}

static int norm_from_string(tvi_handle_t *tvh, char* norm)
{
    const tvi_functions_t *funcs = tvh->functions;
    char str[20];
    int ret;

    strncpy(str, norm, sizeof(str)-1);
    str[sizeof(str)-1] = '\0';
    ret=funcs->control(tvh->priv, TVI_CONTROL_SPC_GET_NORMID, str);

    if (ret == TVI_CONTROL_TRUE) {
        int *v = (int *)str;
        return *v;
    }

    if(ret!=TVI_CONTROL_UNKNOWN)
    {
        MP_WARN(tvh, "tv.c: norm_from_string(%s): Bogus norm parameter, setting %s.\n", norm,"default");
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
        MP_WARN(tvh, "tv.c: norm_from_string(%s): Bogus norm parameter, setting %s.\n", norm, "PAL");
        return TV_NORM_PAL;
    }
}

static void parse_channels(tvi_handle_t *tvh)
{
    char** channels = tvh->tv_param->channels;

    MP_INFO(tvh, "TV channel names detected.\n");
    tvh->tv_channel_list = malloc(sizeof(tv_channels_t));
    tvh->tv_channel_list->index=1;
    tvh->tv_channel_list->next=NULL;
    tvh->tv_channel_list->prev=NULL;
    tvh->tv_channel_current = tvh->tv_channel_list;
    tvh->tv_channel_current->norm = tvh->norm;

    while (*channels) {
        char* tmp = *(channels++);
        char* sep = strchr(tmp,'-');
        int i;
        struct CHANLIST cl;

        if (!sep) continue; // Wrong syntax, but mplayer should not crash

        av_strlcpy(tvh->tv_channel_current->name, sep + 1,
                        sizeof(tvh->tv_channel_current->name));
        sep[0] = '\0';
        strncpy(tvh->tv_channel_current->number, tmp, 5);
        tvh->tv_channel_current->number[4]='\0';

        while ((sep=strchr(tvh->tv_channel_current->name, '_')))
            sep[0] = ' ';

        // if channel number is a number and larger than 1000 threat it as frequency
        // tmp still contain pointer to null-terminated string with channel number here
        if (atoi(tmp)>1000){
            tvh->tv_channel_current->freq=atoi(tmp);
        }else{
            tvh->tv_channel_current->freq = 0;
            for (i = 0; i < chanlists[tvh->chanlist].count; i++) {
                cl = tvh->chanlist_s[i];
                if (!strcasecmp(cl.name, tvh->tv_channel_current->number)) {
                    tvh->tv_channel_current->freq=cl.freq;
                    break;
                }
            }
        }
        if (tvh->tv_channel_current->freq == 0)
            MP_ERR(tvh, "Couldn't find frequency for channel %s (%s)\n",
                            tvh->tv_channel_current->number, tvh->tv_channel_current->name);
        else {
          sep = strchr(tvh->tv_channel_current->name, '-');
          if ( !sep ) sep = strchr(tvh->tv_channel_current->name, '+');

          if ( sep ) {
            i = atoi (sep+1);
            if ( sep[0] == '+' ) tvh->tv_channel_current->freq += i * 100;
            if ( sep[0] == '-' ) tvh->tv_channel_current->freq -= i * 100;
            sep[0] = '\0';
          }

          sep = strchr(tvh->tv_channel_current->name, '=');
          if ( sep ) {
            tvh->tv_channel_current->norm = norm_from_string(tvh, sep+1);
            sep[0] = '\0';
          }
        }

        /*MP_INFO(tvh, "-- Detected channel %s - %s (%5.3f)\n",
                        tvh->tv_channel_current->number, tvh->tv_channel_current->name,
                        (float)tvh->tv_channel_current->freq/1000);*/

        tvh->tv_channel_current->next = malloc(sizeof(tv_channels_t));
        tvh->tv_channel_current->next->index = tvh->tv_channel_current->index + 1;
        tvh->tv_channel_current->next->prev = tvh->tv_channel_current;
        tvh->tv_channel_current->next->next = NULL;
        tvh->tv_channel_current = tvh->tv_channel_current->next;
        tvh->tv_channel_current->norm = tvh->norm;
    }
    if (tvh->tv_channel_current->prev)
        tvh->tv_channel_current->prev->next = NULL;
    free(tvh->tv_channel_current);
}

int tv_set_norm(tvi_handle_t *tvh, char* norm)
{
    tvh->norm = norm_from_string(tvh, norm);

    MP_VERBOSE(tvh, "Selected norm : %s\n", norm);
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM, &tvh->norm) != TVI_CONTROL_TRUE) {
        MP_ERR(tvh, "Error: Cannot set norm!\n");
        return 0;
    }
    return 1;
}

static int tv_set_norm_i(tvi_handle_t *tvh, int norm)
{
   tvh->norm = norm;

   MP_VERBOSE(tvh, "Selected norm id: %d\n", norm);
   if (tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM, &tvh->norm) != TVI_CONTROL_TRUE) {
      MP_ERR(tvh, "Error: Cannot set norm!\n");
      return 0;
   }

   return 1;
}

static void set_norm_and_freq(tvi_handle_t *tvh, tv_channels_t *chan)
{
    MP_INFO(tvh, "Selected channel: %s - %s (freq: %.3f)\n",
           chan->number, chan->name, chan->freq/1000.0);
    tv_set_norm_i(tvh, chan->norm);
    tv_set_freq_float(tvh, chan->freq);
}

int open_tv(tvi_handle_t *tvh)
{
    int i;
    const tvi_functions_t *funcs = tvh->functions;
    static const int tv_fmt_list[] = {
      MP_FOURCC_YV12,
      MP_FOURCC_I420,
      MP_FOURCC_UYVY,
      MP_FOURCC_YUY2,
      MP_FOURCC_RGB32,
      MP_FOURCC_RGB24,
      MP_FOURCC_RGB16,
      MP_FOURCC_RGB15
    };

    if (funcs->control(tvh->priv, TVI_CONTROL_IS_VIDEO, 0) != TVI_CONTROL_TRUE)
    {
        MP_ERR(tvh, "Error: No video input present!\n");
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
        case MP_FOURCC_YV12:
        case MP_FOURCC_I420:
        case MP_FOURCC_UYVY:
        case MP_FOURCC_YUY2:
        case MP_FOURCC_RGB32:
        case MP_FOURCC_RGB24:
        case MP_FOURCC_BGR32:
        case MP_FOURCC_BGR24:
        case MP_FOURCC_BGR16:
        case MP_FOURCC_BGR15:
            break;
        default:
            MP_ERR(tvh, "==================================================================\n"\
                        " WARNING: UNTESTED OR UNKNOWN OUTPUT IMAGE FORMAT REQUESTED (0x%x)\n"\
                        " This may cause buggy playback or program crash! Bug reports will\n"\
                        " be ignored! You should try again with YV12 (which is the default\n"\
                        " colorspace) and read the documentation!\n"\
                        "==================================================================\n"
                ,tvh->tv_param->outfmt);
    }
    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_FORMAT, &tvh->tv_param->outfmt);
    }

    /* set some params got from cmdline */
    funcs->control(tvh->priv, TVI_CONTROL_SPC_SET_INPUT, &tvh->tv_param->input);

    if ((!strcmp(tvh->tv_param->driver, "v4l2") && tvh->tv_param->normid >= 0))
        tv_set_norm_i(tvh, tvh->tv_param->normid);
    else
        tv_set_norm(tvh,tvh->tv_param->norm);

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
            MP_ERR(tvh, "Unable to set requested width: %d\n", tvh->tv_param->width);
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
            MP_ERR(tvh, "Unable to set requested height: %d\n", tvh->tv_param->height);
            funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &tvh->tv_param->height);
        }
    }

    if (funcs->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) != TVI_CONTROL_TRUE)
    {
        MP_WARN(tvh, "Selected input hasn't got a tuner!\n");
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

    if (tvh->chanlist == -1) {
        MP_WARN(tvh, "Unable to find selected channel list! (%s)\n",
            tvh->tv_param->chanlist);
        return 0;
    } else
        MP_VERBOSE(tvh, "Selected channel list: %s (including %d channels)\n",
            chanlists[tvh->chanlist].name, chanlists[tvh->chanlist].count);

    if (tvh->tv_param->freq && tvh->tv_param->channel)
    {
        MP_WARN(tvh, "You can't set frequency and channel simultaneously!\n");
        goto done;
    }

    /* Handle channel names */
    if (tvh->tv_param->channels) {
        parse_channels(tvh);
    } else
            tvh->tv_channel_last_real = malloc(5);

    if (tvh->tv_channel_list) {
        int channel = 0;
        if (tvh->tv_param->channel)
         {
           if (mp_isdigit(*tvh->tv_param->channel))
                /* if tvh->tv_param->channel begins with a digit interpret it as a number */
                channel = atoi(tvh->tv_param->channel);
           else
              {
                /* if tvh->tv_param->channel does not begin with a digit
                   set the first channel that contains tvh->tv_param->channel in its name */

                tvh->tv_channel_current = tvh->tv_channel_list;
                while ( tvh->tv_channel_current ) {
                        if ( strstr(tvh->tv_channel_current->name, tvh->tv_param->channel) )
                          break;
                        tvh->tv_channel_current = tvh->tv_channel_current->next;
                        }
                if ( !tvh->tv_channel_current ) tvh->tv_channel_current = tvh->tv_channel_list;
              }
         }
        else
                channel = 1;

        if ( channel ) {
        tvh->tv_channel_current = tvh->tv_channel_list;
        for (int n = 1; n < channel; n++)
                if (tvh->tv_channel_current->next)
                        tvh->tv_channel_current = tvh->tv_channel_current->next;
        }

        set_norm_and_freq(tvh, tvh->tv_channel_current);
        tvh->tv_channel_last = tvh->tv_channel_current;
    } else {
    /* we need to set frequency */
    if (tvh->tv_param->freq)
    {
        unsigned long freq = atof(tvh->tv_param->freq)*16;

        /* set freq in MHz */
        funcs->control(tvh->priv, TVI_CONTROL_TUN_SET_FREQ, &freq);

        funcs->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, &freq);
        MP_VERBOSE(tvh, "Selected frequency: %lu (%.3f)\n",
            freq, freq/16.0);
    }

            if (tvh->tv_param->channel) {
        struct CHANLIST cl;

        MP_VERBOSE(tvh, "Requested channel: %s\n", tvh->tv_param->channel);
        for (i = 0; i < chanlists[tvh->chanlist].count; i++)
        {
            cl = tvh->chanlist_s[i];
                    //  printf("count%d: name: %s, freq: %d\n",
                    //  i, cl.name, cl.freq);
            if (!strcasecmp(cl.name, tvh->tv_param->channel))
            {
                        strcpy(tvh->tv_channel_last_real, cl.name);
                tvh->channel = i;
                MP_INFO(tvh, "Selected channel: %s (freq: %.3f)\n",
                    cl.name, cl.freq/1000.0);
                tv_set_freq_float(tvh, cl.freq);
                break;
            }
        }
    }
    }

    /* grep frequency in chanlist */
    {
        unsigned long i2 = 0;
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

tvi_handle_t *tv_begin(tv_param_t* tv_param, struct mp_log *log)
{
    int i;
    tvi_handle_t* h;
    if(tv_param->driver && !strcmp(tv_param->driver,"help")){
        mp_info(log, "Available drivers:\n");
        for(i=0;tvi_driver_list[i];i++){
            mp_info(log, " %s\t%s\n",tvi_driver_list[i]->short_name,tvi_driver_list[i]->name);
        }
        return NULL;
    }

    for(i=0;tvi_driver_list[i];i++){
        if (!tv_param->driver || !strcmp(tvi_driver_list[i]->short_name, tv_param->driver)){
            h=tvi_driver_list[i]->tvi_init(log, tv_param);
            //Requested driver initialization failed
            if (!h && tv_param->driver)
                return NULL;
            //Driver initialization failed during autodetection process.
            if (!h)
                continue;

            h->tv_param=tv_param;
            MP_INFO(h, "Selected driver: %s\n name: %s\n", tvi_driver_list[i]->short_name,
            tvi_driver_list[i]->name);
            talloc_free(tv_param->driver);
            tv_param->driver=talloc_strdup(NULL, tvi_driver_list[i]->short_name);
            return h;
        }
    }

    if(tv_param->driver)
        mp_err(log, "No such driver: %s\n", tv_param->driver);
    else
        mp_err(log, "TV driver autodetection failed.\n");
    return NULL;
}

int tv_uninit(tvi_handle_t *tvh)
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
            MP_WARN(tvh, "Unknown color option (%d) specified!\n", opt);
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
            MP_WARN(tvh, "Unknown color option (%d) specified!\n", opt);
    }

    return TVI_CONTROL_UNKNOWN;
}

int tv_get_freq(tvi_handle_t *tvh, unsigned long *freq)
{
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) == TVI_CONTROL_TRUE)
    {
        tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, freq);
        MP_VERBOSE(tvh, "Current frequency: %lu (%.3f)\n",
            *freq, *freq/16.0);
    }
    return 1;
}

int tv_set_freq(tvi_handle_t *tvh, unsigned long freq)
{
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) == TVI_CONTROL_TRUE)
    {
//      unsigned long freq = atof(tvh->tv_param->freq)*16;

        /* set freq in MHz */
        tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_FREQ, &freq);

        tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, &freq);
        MP_VERBOSE(tvh, "Current frequency: %lu (%.3f)\n",
            freq, freq/16.0);
    }
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
    unsigned long frequency = 0;

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
            strcpy(tvh->tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
            cl = tvh->chanlist_s[--tvh->channel];
            MP_INFO(tvh, "Selected channel: %s (freq: %.3f)\n",
                cl.name, cl.freq/1000.0);
            tv_set_freq_float(tvh, cl.freq);
        }
    }

    if (direction == TV_CHANNEL_HIGHER)
    {
        if (tvh->channel+1 < chanlists[tvh->chanlist].count)
        {
            strcpy(tvh->tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
            cl = tvh->chanlist_s[++tvh->channel];
            MP_INFO(tvh, "Selected channel: %s (freq: %.3f)\n",
                cl.name, cl.freq/1000.0);
            tv_set_freq_float(tvh, cl.freq);
        }
    }
    return 1;
}

int tv_step_channel(tvi_handle_t *tvh, int direction) {
        tvh->tv_param->scan=0;
        if (tvh->tv_channel_list) {
                if (direction == TV_CHANNEL_HIGHER) {
                        tvh->tv_channel_last = tvh->tv_channel_current;
                        if (tvh->tv_channel_current->next)
                                tvh->tv_channel_current = tvh->tv_channel_current->next;
                        else
                                tvh->tv_channel_current = tvh->tv_channel_list;
                        set_norm_and_freq(tvh, tvh->tv_channel_current);
                }
                if (direction == TV_CHANNEL_LOWER) {
                        tvh->tv_channel_last = tvh->tv_channel_current;
                        if (tvh->tv_channel_current->prev)
                                tvh->tv_channel_current = tvh->tv_channel_current->prev;
                        else
                                while (tvh->tv_channel_current->next)
                                        tvh->tv_channel_current = tvh->tv_channel_current->next;
                        set_norm_and_freq(tvh, tvh->tv_channel_current);
                }
        } else tv_step_channel_real(tvh, direction);
        return 1;
}

int tv_set_channel_real(tvi_handle_t *tvh, char *channel) {
        int i;
        struct CHANLIST cl;

        tvh->tv_param->scan=0;
        strcpy(tvh->tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
        for (i = 0; i < chanlists[tvh->chanlist].count; i++)
        {
            cl = tvh->chanlist_s[i];
//          printf("count%d: name: %s, freq: %d\n",
//              i, cl.name, cl.freq);
            if (!strcasecmp(cl.name, channel))
            {
                tvh->channel = i;
                MP_INFO(tvh, "Selected channel: %s (freq: %.3f)\n",
                    cl.name, cl.freq/1000.0);
                tv_set_freq_float(tvh, cl.freq);
                break;
            }
        }
        return 1;
}

int tv_set_channel(tvi_handle_t *tvh, char *channel) {
        int i, channel_int;

        tvh->tv_param->scan=0;
        if (tvh->tv_channel_list) {
                tvh->tv_channel_last = tvh->tv_channel_current;
                channel_int = atoi(channel);
                tvh->tv_channel_current = tvh->tv_channel_list;
                for (i = 1; i < channel_int; i++)
                        if (tvh->tv_channel_current->next)
                                tvh->tv_channel_current = tvh->tv_channel_current->next;
                set_norm_and_freq(tvh, tvh->tv_channel_current);
        } else tv_set_channel_real(tvh, channel);
        return 1;
}

int tv_last_channel(tvi_handle_t *tvh) {

        tvh->tv_param->scan=0;
        if (tvh->tv_channel_list) {
                tv_channels_t *tmp;

                tmp = tvh->tv_channel_last;
                tvh->tv_channel_last = tvh->tv_channel_current;
                tvh->tv_channel_current = tmp;

                set_norm_and_freq(tvh, tvh->tv_channel_current);
        } else {
                int i;
                struct CHANLIST cl;

                for (i = 0; i < chanlists[tvh->chanlist].count; i++)
                {
                    cl = tvh->chanlist_s[i];
                    if (!strcasecmp(cl.name, tvh->tv_channel_last_real))
                    {
                        strcpy(tvh->tv_channel_last_real, tvh->chanlist_s[tvh->channel].name);
                        tvh->channel = i;
                        MP_INFO(tvh, "Selected channel: %s (freq: %.3f)\n",
                            cl.name, cl.freq/1000.0);
                        tv_set_freq_float(tvh, cl.freq);
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
      MP_ERR(tvh, "Error: Cannot set norm!\n");
      return 0;
    }
  }
    return 1;
}

int tv_stream_control(tvi_handle_t *tvh, int cmd, void *arg)
{
    switch (cmd) {
    case STREAM_CTRL_TV_SET_SCAN:
        tv_start_scan(tvh, *(int *)arg);
        return STREAM_OK;
    case STREAM_CTRL_SET_TV_FREQ:
        tv_set_freq(tvh, *(float *)arg * 16.0f);
        return STREAM_OK;
    case STREAM_CTRL_GET_TV_FREQ: {
        unsigned long tmp = 0;
        tv_get_freq(tvh, &tmp);
        *(float *)arg = tmp / 16.0f;
        return STREAM_OK;
    }
    case STREAM_CTRL_SET_TV_COLORS:
        tv_set_color_options(tvh, ((int *)arg)[0], ((int *)arg)[1]);
        return STREAM_OK;
    case STREAM_CTRL_GET_TV_COLORS:
        tv_get_color_options(tvh, ((int *)arg)[0], &((int *)arg)[1]);
        return STREAM_OK;
    case STREAM_CTRL_TV_SET_NORM:
        tv_set_norm(tvh, (char *)arg);
        return STREAM_OK;
    case STREAM_CTRL_TV_STEP_NORM:
        tv_step_norm(tvh);
        return STREAM_OK;
    case STREAM_CTRL_TV_SET_CHAN:
        tv_set_channel(tvh, (char *)arg);
        return STREAM_OK;
    case STREAM_CTRL_TV_STEP_CHAN:
        if (*(int *)arg >= 0) {
            tv_step_channel(tvh, TV_CHANNEL_HIGHER);
        } else {
            tv_step_channel(tvh, TV_CHANNEL_LOWER);
        }
        return STREAM_OK;
    case STREAM_CTRL_TV_LAST_CHAN:
        tv_last_channel(tvh);
        return STREAM_OK;
    }
    return STREAM_UNSUPPORTED;
}
