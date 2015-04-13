/*
 * stream layer for hardware MPEG 1/2/4 encoders a.k.a PVR
 *  (such as WinTV PVR-150/250/350/500 (a.k.a IVTV), pvrusb2 and cx88)
 * See http://ivtvdriver.org/index.php/Main_Page for more details on the
 *  cards supported by the ivtv driver.
 *
 * Copyright (C) 2006 Benjamin Zores
 * Copyright (C) 2007 Sven Gothel (channel navigation)
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <linux/videodev2.h>

#include <libavutil/common.h>
#include <libavutil/avstring.h>

#include "osdep/io.h"

#include "common/msg.h"
#include "common/common.h"
#include "options/options.h"

#include "stream.h"
#include "tv.h"
#include "frequencies.h"

#define PVR_DEFAULT_DEVICE "/dev/video0"
#define PVR_MAX_CONTROLS 10

/* audio codec mode */
#define PVR_AUDIO_MODE_ARG_STEREO                              "stereo"
#define PVR_AUDIO_MODE_ARG_JOINT_STEREO                        "joint_stereo"
#define PVR_AUDIO_MODE_ARG_DUAL                                "dual"
#define PVR_AUDIO_MODE_ARG_MONO                                "mono"

/* video codec bitrate mode */
#define PVR_VIDEO_BITRATE_MODE_ARG_VBR                         "vbr"
#define PVR_VIDEO_BITRATE_MODE_ARG_CBR                         "cbr"

/* video codec stream type */
#define PVR_VIDEO_STREAM_TYPE_PS                               "ps"
#define PVR_VIDEO_STREAM_TYPE_TS                               "ts"
#define PVR_VIDEO_STREAM_TYPE_MPEG1                            "mpeg1"
#define PVR_VIDEO_STREAM_TYPE_DVD                              "dvd"
#define PVR_VIDEO_STREAM_TYPE_VCD                              "vcd"
#define PVR_VIDEO_STREAM_TYPE_SVCD                             "svcd"

#define PVR_STATION_NAME_SIZE 256

/* command line arguments */
struct pvr_params {
    int aspect_ratio;
    int sample_rate;
    int audio_layer;
    int audio_bitrate;
    char *audio_mode;
    int bitrate;
    char *bitrate_mode;
    int bitrate_peak;
    char *stream_type;
};

#define OPT_BASE_STRUCT struct pvr_params
const struct m_sub_options stream_pvr_conf = {
    .opts = (const m_option_t[]) {
        OPT_INT("aspect", aspect_ratio, 0),
        OPT_INT("arate", sample_rate, 0),
        OPT_INT("alayer", audio_layer, 0),
        OPT_INT("abitrate", audio_bitrate, 0),
        OPT_STRING("amode", audio_mode, 0),
        OPT_INT("vbitrate", bitrate, 0),
        OPT_STRING("vmode", bitrate_mode, 0),
        OPT_INT("vpeak", bitrate_peak, 0),
        OPT_STRING("fmt", stream_type, 0),
        {0}
    },
    .size = sizeof(struct pvr_params),
};

#define BUFSTRCPY(d, s) av_strlcpy(d, s, sizeof(d))
#define BUFPRINTF(d, ...) snprintf(d, sizeof(d), __VA_ARGS__)

typedef struct station_elem_s {
    char name[PVR_STATION_NAME_SIZE];
    int freq;
    char station[PVR_STATION_NAME_SIZE];
    int enabled;
    int priority;
} station_elem_t;

typedef struct stationlist_s {
    char name[PVR_STATION_NAME_SIZE];
    station_elem_t *list;
    int total; /* total number */
    int used; /* used number */
    int enabled; /* enabled number */
} stationlist_t;

struct pvr_t {
    struct mp_log *log;
    tv_param_t *tv_params;
    struct pvr_params *params;
    int dev_fd;
    char *video_dev;
    int chantab;

    /* v4l2 params */
    int mute;
    int input;
    int normid;
    int brightness;
    int contrast;
    int hue;
    int saturation;
    int width;
    int height;
    int freq;
    int chan_idx;
    int chan_idx_last;
    stationlist_t stationlist;
    /* dups the tv_param_channel, or the url's channel param */
    char *param_channel;

    /* encoder params */
    int aspect;
    int samplerate;
    int layer;
    int audio_rate;
    int audio_mode;
    int bitrate;
    int bitrate_mode;
    int bitrate_peak;
    int stream_type;
};

static int pvr_stream_control(struct stream *s, int cmd, void *arg);

static struct pvr_t *pvr_init(void)
{
    struct pvr_t *pvr = NULL;

    pvr = calloc(1, sizeof (struct pvr_t));
    pvr->dev_fd = -1;
    pvr->video_dev = strdup(PVR_DEFAULT_DEVICE);
    pvr->chantab = 5;

    /* v4l2 params */
    pvr->mute = 0;
    pvr->input = 0;
    pvr->normid = -1;
    pvr->brightness = 0;
    pvr->contrast = 0;
    pvr->hue = 0;
    pvr->saturation = 0;
    pvr->width = -1;
    pvr->height = -1;
    pvr->freq = -1;
    pvr->chan_idx = -1;
    pvr->chan_idx_last = -1;

    /* set default encoding settings
     * may be overlapped by user parameters
     * Use VBR MPEG_PS encoding at 6 Mbps (peak at 9.6 Mbps)
     * with 48 KHz L2 384 kbps audio.
     */
    pvr->aspect = V4L2_MPEG_VIDEO_ASPECT_4x3;
    pvr->samplerate = V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000;
    pvr->layer = V4L2_MPEG_AUDIO_ENCODING_LAYER_2;
    pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_384K;
    pvr->audio_mode = V4L2_MPEG_AUDIO_MODE_STEREO;
    pvr->bitrate = 6000000;
    pvr->bitrate_mode = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
    pvr->bitrate_peak = 9600000;
    pvr->stream_type = V4L2_MPEG_STREAM_TYPE_MPEG2_PS;

    return pvr;
}

static void pvr_uninit(struct pvr_t *pvr)
{
    if (!pvr)
        return;

    /* close device */
    if (pvr->dev_fd != -1)
        close(pvr->dev_fd);

    free(pvr->video_dev);
    free(pvr->stationlist.list);
    free(pvr->param_channel);
    free(pvr);
}

/**
 * @brief Copy Constructor for stationlist
 *
 * @see parse_setup_stationlist
 */
static int copycreate_stationlist(struct pvr_t *pvr, stationlist_t *stationlist,
                                  int num)
{
    int i;

    if (pvr->chantab < 0 || !stationlist)
        return -1;

    num = FFMAX(num, chanlists[pvr->chantab].count);

    free(stationlist->list);
    stationlist->list = NULL;

    stationlist->total = 0;
    stationlist->enabled = 0;
    stationlist->used = 0;
    stationlist->list = calloc(num, sizeof (station_elem_t));

    if (!stationlist->list) {
        MP_ERR(pvr, "No memory allocated for station list, giving up\n");
        return -1;
    }

    /* transport the channel list data to our extented struct */
    stationlist->total = num;
    BUFSTRCPY(stationlist->name, chanlists[pvr->chantab].name);

    for (i = 0; i < chanlists[pvr->chantab].count; i++) {
        stationlist->list[i].station[0] = '\0'; /* no station name yet */
        BUFSTRCPY(stationlist->list[i].name,
                  chanlists[pvr->chantab].list[i].name);
        stationlist->list[i].freq = chanlists[pvr->chantab].list[i].freq;
        stationlist->list[i].enabled = 1; /* default enabled */
        stationlist->enabled++;
        stationlist->used++;
    }

    return 0;
}

static int print_all_stations(struct pvr_t *pvr)
{
    int i;

    if (!pvr || !pvr->stationlist.list)
        return -1;

    for (i = 0; i < pvr->stationlist.total; i++) {
        MP_VERBOSE(pvr, "%3d: [%c] channel: %8s - freq: %8d - station: %s\n",
                   i, (pvr->stationlist.list[i].enabled) ? 'X' : ' ',
                   pvr->stationlist.list[i].name, pvr->stationlist.list[i].freq,
                   pvr->stationlist.list[i].station);
    }

    return 0;
}

/**
 * Disables all stations
 *
 * @see parse_setup_stationlist
 */
static void disable_all_stations(struct pvr_t *pvr)
{
    int i;

    for (i = 0; i < pvr->stationlist.total; i++)
        pvr->stationlist.list[i].enabled = 0;
    pvr->stationlist.enabled = 0;
}

/**
 * Update or add a station
 *
 * @see parse_setup_stationlist
 */
static int set_station(struct pvr_t *pvr, const char *station,
                       const char *channel, int freq, int priority)
{
    int i;

    if (!pvr || !pvr->stationlist.list)
        return -1;

    if (0 >= pvr->stationlist.total || (!channel && !freq))
        return -1;

    /* select channel */
    for (i = 0; i < pvr->stationlist.used; i++) {
        if (channel && !strcasecmp(pvr->stationlist.list[i].name, channel))
            break;  /* found existing channel entry */

        if (freq > 0 && pvr->stationlist.list[i].freq == freq)
            break;  /* found existing frequency entry */
    }

    if (i < pvr->stationlist.used) {
        /**
         * found an existing entry,
         * which is about to change with the user data.
         * it is also enabled ..
         */
        if (!pvr->stationlist.list[i].enabled) {
            pvr->stationlist.list[i].enabled = 1;
            pvr->stationlist.enabled++;
        }

        if (station)
            BUFSTRCPY(pvr->stationlist.list[i].station, station);
        else if (channel)
            BUFSTRCPY(pvr->stationlist.list[i].station, channel);
        else
            BUFPRINTF(pvr->stationlist.list[i].station, "F %d", freq);

        pvr->stationlist.list[i].priority = priority;

        MP_DBG(pvr, "Set user station channel: %8s - freq: %8d - station: %s\n",
               pvr->stationlist.list[i].name,
               pvr->stationlist.list[i].freq,
               pvr->stationlist.list[i].station);
        return 0;
    }

    /* from here on, we have to create a new entry, frequency is mandatory */
    if (freq < 0) {
        MP_ERR(pvr, "Cannot add new station/channel without frequency\n");
        return -1;
    }

    if (pvr->stationlist.total < i) {
        /**
         * we have to extend the stationlist about
         * an arbitrary size, even though this path is not performance critical
         */
        pvr->stationlist.total += 10;
        pvr->stationlist.list =
            realloc(pvr->stationlist.list,
                    pvr->stationlist.total * sizeof (station_elem_t));

        if (!pvr->stationlist.list) {
            MP_ERR(pvr, "No memory allocated for station list, giving up\n");
            return -1;
        }

        /* clear the new space ..*/
        memset(&(pvr->stationlist.list[pvr->stationlist.used]), 0,
               (pvr->stationlist.total - pvr->stationlist.used)
               * sizeof (station_elem_t));
    }

    /* here we go, our actual new entry */
    pvr->stationlist.used++;
    pvr->stationlist.list[i].enabled = 1;
    pvr->stationlist.list[i].priority = priority;
    pvr->stationlist.enabled++;

    if (station)
        BUFSTRCPY(pvr->stationlist.list[i].station, station);
    if (channel)
        BUFSTRCPY(pvr->stationlist.list[i].name, channel);
    else
        BUFPRINTF(pvr->stationlist.list[i].name, "F %d", freq);

    pvr->stationlist.list[i].freq = freq;

    MP_DBG(pvr, "Add user station channel: %8s - freq: %8d - station: %s\n",
           pvr->stationlist.list[i].name,
           pvr->stationlist.list[i].freq,
           pvr->stationlist.list[i].station);

    return 0;
}

static int compare_priority(const void *pa, const void *pb)
{
    const station_elem_t *a = pa;
    const station_elem_t *b = pb;

    if (a->priority < b->priority)
        return -1;
    if (a->priority > b->priority)
        return 1;
    return 0;
}

/**
 * Here we set our stationlist, as follow
 *  - choose the frequency channel table, e.g. ntsc-cable
 *  - create our stationlist, same element size as the channellist
 *  - copy the channellist content to our stationlist
 *  - IF the user provides his channel-mapping, THEN:
 *    - disable all stations
 *    - update and/or create entries in the stationlist and enable them
 */
static int parse_setup_stationlist(struct pvr_t *pvr)
{
    int i;

    if (!pvr)
        return -1;

    /* Create our station/channel list */
    if (pvr->tv_params->chanlist) {
        /* select channel list */
        for (i = 0; chanlists[i].name != NULL; i++) {
            if (!strcasecmp(chanlists[i].name, pvr->tv_params->chanlist)) {
                pvr->chantab = i;
                break;
            }
        }
        if (!chanlists[i].name) {
            MP_ERR(pvr, "unable to find channel list %s, using default %s\n",
                   pvr->tv_params->chanlist, chanlists[pvr->chantab].name);
        } else {
            MP_INFO(pvr, "select channel list %s, entries %d\n",
                    chanlists[pvr->chantab].name, chanlists[pvr->chantab].count);
        }
    }

    if (0 > pvr->chantab) {
        MP_FATAL(pvr, "No channel list selected, giving up\n");
        return -1;
    }

    if (copycreate_stationlist(pvr, &(pvr->stationlist), -1) < 0) {
        MP_FATAL(pvr, "No memory allocated for station list, giving up\n");
        return -1;
    }

    /* Handle user channel mappings */
    if (pvr->tv_params->channels) {
        char channel[PVR_STATION_NAME_SIZE];
        char station[PVR_STATION_NAME_SIZE];
        char **channels = pvr->tv_params->channels;

        disable_all_stations(pvr);

        int prio = 0;
        for (i = 0; i < pvr->stationlist.total; i++)
            pvr->stationlist.list[i].priority = ++prio;

        while (*channels) {
            char *tmp = *(channels++);
            char *sep = strchr(tmp, '-');
            int freq = -1;

            if (!sep)
                continue;  /* Wrong syntax, but mplayer should not crash */

            BUFSTRCPY(station, sep + 1);

            sep[0] = '\0';
            BUFSTRCPY(channel, tmp);

            while ((sep = strchr(station, '_')))
                sep[0] = ' ';

            /* if channel number is a number and larger than 1000 treat it as
             * frequency tmp still contain pointer to null-terminated string with
             * channel number here
             */
            if ((freq = atoi(channel)) <= 1000)
                freq = -1;

            if (set_station(pvr, station, (freq <= 0) ? channel : NULL, freq,
                            ++prio) < 0)
            {
                MP_ERR(pvr, "Unable to set user station channel: %8s - "
                       "freq: %8d - station: %s\n", channel, freq, station);
            }
        }

        qsort(pvr->stationlist.list, pvr->stationlist.total,
              sizeof(station_elem_t), compare_priority);
    }

    return print_all_stations(pvr);
}

static int get_v4l2_freq(struct pvr_t *pvr)
{
    int freq;
    struct v4l2_frequency vf;
    struct v4l2_tuner vt;

    if (!pvr)
        return -1;

    if (pvr->dev_fd < 0)
        return -1;

    memset(&vt, 0, sizeof (vt));
    memset(&vf, 0, sizeof (vf));

    if (ioctl(pvr->dev_fd, VIDIOC_G_TUNER, &vt) < 0) {
        MP_ERR(pvr, "can't set tuner (%s).\n", mp_strerror(errno));
        return -1;
    }

    if (ioctl(pvr->dev_fd, VIDIOC_G_FREQUENCY, &vf) < 0) {
        MP_ERR(pvr, "can't get frequency (%s).\n", mp_strerror(errno));
        return -1;
    }
    freq = vf.frequency;
    if (!(vt.capability & V4L2_TUNER_CAP_LOW))
        freq *= 1000;
    freq /= 16;

    return freq;
}

static int set_v4l2_freq(struct pvr_t *pvr)
{
    struct v4l2_frequency vf;
    struct v4l2_tuner vt;

    if (!pvr)
        return -1;

    if (0 >= pvr->freq) {
        MP_ERR(pvr, "Frequency invalid %d !!!\n", pvr->freq);
        return -1;
    }

    /* don't set the frequency, if it's already set.
     * setting it here would interrupt the stream.
     */
    if (get_v4l2_freq(pvr) == pvr->freq) {
        MP_INFO(pvr, "Frequency %d already set.\n", pvr->freq);
        return 0;
    }

    if (pvr->dev_fd < 0)
        return -1;

    memset(&vf, 0, sizeof (vf));
    memset(&vt, 0, sizeof (vt));

    if (ioctl(pvr->dev_fd, VIDIOC_G_TUNER, &vt) < 0) {
        MP_ERR(pvr, "can't get tuner (%s).\n", mp_strerror(errno));
        return -1;
    }

    vf.type = vt.type;
    vf.frequency = pvr->freq * 16;

    if (!(vt.capability & V4L2_TUNER_CAP_LOW))
        vf.frequency /= 1000;

    if (ioctl(pvr->dev_fd, VIDIOC_S_FREQUENCY, &vf) < 0) {
        MP_ERR(pvr, "can't set frequency (%s).\n", mp_strerror(errno));
        return -1;
    }

    memset(&vt, 0, sizeof(vt));
    if (ioctl(pvr->dev_fd, VIDIOC_G_TUNER, &vt) < 0) {
        MP_ERR(pvr, "can't set tuner (%s).\n", mp_strerror(errno));
        return -1;
    }

    /* just a notification */
    if (!vt.signal) {
        MP_ERR(pvr, "NO SIGNAL at frequency %d (%d)\n", pvr->freq, vf.frequency);
    } else {
        MP_INFO(pvr, "Got signal at frequency %d (%d)\n", pvr->freq,
                vf.frequency);
    }

    return 0;
}

static int set_station_by_step(struct pvr_t *pvr, int step, int v4lAction)
{
    if (!pvr || !pvr->stationlist.list)
        return -1;

    if (pvr->stationlist.enabled >= abs(step)) {
        int gotcha = 0;
        int chidx = pvr->chan_idx + step;

        while (!gotcha) {
            chidx = (chidx + pvr->stationlist.used) % pvr->stationlist.used;

            MP_DBG(pvr, "Offset switch: current %d, enabled %d, step %d -> %d\n",
                   pvr->chan_idx, pvr->stationlist.enabled, step,
                   chidx);

            if (!pvr->stationlist.list[chidx].enabled) {
                MP_DBG(pvr, "Switch disabled to user station channel: %8s - "
                       "freq: %8d - station: %s\n",
                       pvr->stationlist.list[chidx].name,
                       pvr->stationlist.list[chidx].freq,
                       pvr->stationlist.list[chidx].station);
                chidx += FFSIGN(step);
            } else {
                gotcha = 1;
            }
        }

        pvr->freq = pvr->stationlist.list[chidx].freq;
        pvr->chan_idx_last = pvr->chan_idx;
        pvr->chan_idx = chidx;

        MP_INFO(pvr, "Switch to user station channel: %8s - freq: %8d - station: %s\n",
                pvr->stationlist.list[chidx].name,
                pvr->stationlist.list[chidx].freq,
                pvr->stationlist.list[chidx].station);

        if (v4lAction)
            return set_v4l2_freq(pvr);

        return (pvr->freq > 0) ? 0 : -1;
    }

    MP_ERR(pvr, "Ooops couldn't set freq by channel entry step %d to "
           "current %d, enabled %d\n",
           step, pvr->chan_idx, pvr->stationlist.enabled);

    return -1;
}

static int set_station_by_channelname_or_freq(struct pvr_t *pvr,
                                              const char *channel,
                                              int freq, int v4lAction)
{
    int i = 0;

    if (!pvr || !pvr->stationlist.list)
        return -1;

    if (0 >= pvr->stationlist.enabled) {
        MP_WARN(pvr, "No enabled station, cannot switch channel/frequency\n");
        return -1;
    }

    if (channel) {
        /* select by channel */
        for (i = 0; i < pvr->stationlist.used; i++) {
            if (!strcasecmp(pvr->stationlist.list[i].name, channel)) {
                if (!pvr->stationlist.list[i].enabled) {
                    MP_WARN(pvr, "Switch disabled to user station channel: %8s "
                            "- freq: %8d - station: %s\n",
                            pvr->stationlist.list[i].name,
                            pvr->stationlist.list[i].freq,
                            pvr->stationlist.list[i].station);

                    return -1;
                }

                pvr->freq = pvr->stationlist.list[i].freq;
                pvr->chan_idx_last = pvr->chan_idx;
                pvr->chan_idx = i;
                break;
            }
        }
    } else if (freq >= 0) {
        /* select by freq */
        for (i = 0; i < pvr->stationlist.used; i++) {
            if (pvr->stationlist.list[i].freq == freq) {
                if (!pvr->stationlist.list[i].enabled) {
                    MP_WARN(pvr, "Switch disabled to user station channel: "
                            "%8s - freq: %8d - station: %s\n",
                            pvr->stationlist.list[i].name,
                            pvr->stationlist.list[i].freq,
                            pvr->stationlist.list[i].station);

                    return -1;
                }

                pvr->freq = pvr->stationlist.list[i].freq;
                pvr->chan_idx_last = pvr->chan_idx;
                pvr->chan_idx = i;
                break;
            }
        }
    }

    if (i >= pvr->stationlist.used) {
        if (channel) {
            MP_WARN(pvr, "unable to find channel %s\n", channel);
        } else {
            MP_WARN(pvr, "unable to find frequency %d\n", freq);
        }
        return -1;
    }

    MP_INFO(pvr, "Switch to user station channel: %8s - freq: %8d - station: %s\n",
            pvr->stationlist.list[i].name,
            pvr->stationlist.list[i].freq,
            pvr->stationlist.list[i].station);

    if (v4lAction)
        return set_v4l2_freq(pvr);

    return (pvr->freq > 0) ? 0 : -1;
}

static void parse_encoder_options(struct pvr_t *pvr)
{
    if (!pvr)
        return;

    /* -pvr aspect=digit */
    if (pvr->params->aspect_ratio >= 0 && pvr->params->aspect_ratio <= 3)
        pvr->aspect = pvr->params->aspect_ratio;

    /* -pvr arate=x */
    if (pvr->params->sample_rate != 0) {
        switch (pvr->params->sample_rate) {
        case 32000:
            pvr->samplerate = V4L2_MPEG_AUDIO_SAMPLING_FREQ_32000;
            break;
        case 44100:
            pvr->samplerate = V4L2_MPEG_AUDIO_SAMPLING_FREQ_44100;
            break;
        case 48000:
            pvr->samplerate = V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000;
            break;
        default:
            break;
        }
    }

    /* -pvr alayer=x */
    if (pvr->params->audio_layer == 1) {
        pvr->layer = V4L2_MPEG_AUDIO_ENCODING_LAYER_1;
    } else if (pvr->params->audio_layer == 2) {
        pvr->layer = V4L2_MPEG_AUDIO_ENCODING_LAYER_2;
    } else if (pvr->params->audio_layer == 3) {
        pvr->layer = V4L2_MPEG_AUDIO_ENCODING_LAYER_3;
    }

    /* -pvr abitrate=x */
    if (pvr->params->audio_bitrate != 0) {
        if (pvr->layer == V4L2_MPEG_AUDIO_ENCODING_LAYER_1) {
            switch (pvr->params->audio_bitrate) {
            case 32:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_32K;
                break;
            case 64:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_64K;
                break;
            case 96:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_96K;
                break;
            case 128:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_128K;
                break;
            case 160:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_160K;
                break;
            case 192:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_192K;
                break;
            case 224:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_224K;
                break;
            case 256:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_256K;
                break;
            case 288:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_288K;
                break;
            case 320:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_320K;
                break;
            case 352:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_352K;
                break;
            case 384:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_384K;
                break;
            case 416:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_416K;
                break;
            case 448:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L1_BITRATE_448K;
                break;
            default:
                break;
            }
        } else if (pvr->layer == V4L2_MPEG_AUDIO_ENCODING_LAYER_2) {
            switch (pvr->params->audio_bitrate) {
            case 32:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_32K;
                break;
            case 48:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_48K;
                break;
            case 56:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_56K;
                break;
            case 64:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_64K;
                break;
            case 80:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_80K;
                break;
            case 96:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_96K;
                break;
            case 112:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_112K;
                break;
            case 128:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_128K;
                break;
            case 160:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_160K;
                break;
            case 192:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_192K;
                break;
            case 224:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_224K;
                break;
            case 256:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_256K;
                break;
            case 320:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_320K;
                break;
            case 384:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L2_BITRATE_384K;
                break;
            default:
                break;
            }
        } else if (pvr->layer == V4L2_MPEG_AUDIO_ENCODING_LAYER_3) {
            switch (pvr->params->audio_bitrate) {
            case 32:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_32K;
                break;
            case 40:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_40K;
                break;
            case 48:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_48K;
                break;
            case 56:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_56K;
                break;
            case 64:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_64K;
                break;
            case 80:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_80K;
                break;
            case 96:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_96K;
                break;
            case 112:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_112K;
                break;
            case 128:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_128K;
                break;
            case 160:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_160K;
                break;
            case 192:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_192K;
                break;
            case 224:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_224K;
                break;
            case 256:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_256K;
                break;
            case 320:
                pvr->audio_rate = V4L2_MPEG_AUDIO_L3_BITRATE_320K;
                break;
            default:
                break;
            }
        }
    }

    /* -pvr amode=x */
    if (pvr->params->audio_mode) {
        if (!strcmp(pvr->params->audio_mode, PVR_AUDIO_MODE_ARG_STEREO)) {
            pvr->audio_mode = V4L2_MPEG_AUDIO_MODE_STEREO;
        } else if (!strcmp(pvr->params->audio_mode,
                           PVR_AUDIO_MODE_ARG_JOINT_STEREO))
        {
            pvr->audio_mode = V4L2_MPEG_AUDIO_MODE_JOINT_STEREO;
        } else if (!strcmp(pvr->params->audio_mode, PVR_AUDIO_MODE_ARG_DUAL)) {
            pvr->audio_mode = V4L2_MPEG_AUDIO_MODE_DUAL;
        } else if (!strcmp(pvr->params->audio_mode, PVR_AUDIO_MODE_ARG_MONO)) {
            pvr->audio_mode = V4L2_MPEG_AUDIO_MODE_MONO;
        }
    }

    /* -pvr vbitrate=x */
    if (pvr->params->bitrate)
        pvr->bitrate = pvr->params->bitrate;

    /* -pvr vmode=x */
    if (pvr->params->bitrate_mode) {
        if (!strcmp(pvr->params->bitrate_mode, PVR_VIDEO_BITRATE_MODE_ARG_VBR)) {
            pvr->bitrate_mode = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
        } else if (!strcmp(pvr->params->bitrate_mode,
                           PVR_VIDEO_BITRATE_MODE_ARG_CBR))
        {
            pvr->bitrate_mode = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
        }
    }

    /* -pvr vpeak=x */
    if (pvr->params->bitrate_peak)
        pvr->bitrate_peak = pvr->params->bitrate_peak;

    /* -pvr fmt=x */
    if (pvr->params->stream_type) {
        if (!strcmp(pvr->params->stream_type, PVR_VIDEO_STREAM_TYPE_PS)) {
            pvr->stream_type = V4L2_MPEG_STREAM_TYPE_MPEG2_PS;
        } else if (!strcmp(pvr->params->stream_type, PVR_VIDEO_STREAM_TYPE_TS)) {
            pvr->stream_type = V4L2_MPEG_STREAM_TYPE_MPEG2_TS;
        } else if (!strcmp(pvr->params->stream_type, PVR_VIDEO_STREAM_TYPE_MPEG1)) {
            pvr->stream_type = V4L2_MPEG_STREAM_TYPE_MPEG1_SS;
        } else if (!strcmp(pvr->params->stream_type, PVR_VIDEO_STREAM_TYPE_DVD)) {
            pvr->stream_type = V4L2_MPEG_STREAM_TYPE_MPEG2_DVD;
        } else if (!strcmp(pvr->params->stream_type, PVR_VIDEO_STREAM_TYPE_VCD)) {
            pvr->stream_type = V4L2_MPEG_STREAM_TYPE_MPEG1_VCD;
        } else if (!strcmp(pvr->params->stream_type, PVR_VIDEO_STREAM_TYPE_SVCD)) {
            pvr->stream_type = V4L2_MPEG_STREAM_TYPE_MPEG2_SVCD;
        }
    }
}

static void add_v4l2_ext_control(struct v4l2_ext_control *ctrl,
                                 uint32_t id, int32_t value)
{
    ctrl->id = id;
    ctrl->value = value;
}

static int set_encoder_settings(struct pvr_t *pvr)
{
    struct v4l2_ext_control *ext_ctrl = NULL;
    struct v4l2_ext_controls ctrls;
    uint32_t count = 0;

    if (!pvr)
        return -1;

    if (pvr->dev_fd < 0)
        return -1;

    ext_ctrl = malloc(PVR_MAX_CONTROLS * sizeof (struct v4l2_ext_control));

    add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_VIDEO_ASPECT,
                         pvr->aspect);

    add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ,
                         pvr->samplerate);

    add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_AUDIO_ENCODING,
                         pvr->layer);

    switch (pvr->layer) {
    case V4L2_MPEG_AUDIO_ENCODING_LAYER_1:
        add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_AUDIO_L1_BITRATE,
                             pvr->audio_rate);
        break;
    case V4L2_MPEG_AUDIO_ENCODING_LAYER_2:
        add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_AUDIO_L2_BITRATE,
                             pvr->audio_rate);
        break;
    case V4L2_MPEG_AUDIO_ENCODING_LAYER_3:
        add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_AUDIO_L3_BITRATE,
                             pvr->audio_rate);
        break;
    default:
        break;
    }

    add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_AUDIO_MODE,
                         pvr->audio_mode);

    add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_VIDEO_BITRATE,
                         pvr->bitrate);

    add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
                         pvr->bitrate_peak);

    add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                         pvr->bitrate_mode);

    add_v4l2_ext_control(&ext_ctrl[count++], V4L2_CID_MPEG_STREAM_TYPE,
                         pvr->stream_type);

    /* set new encoding settings */
    ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ctrls.count = count;
    ctrls.controls = ext_ctrl;

    if (ioctl(pvr->dev_fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0) {
        MP_ERR(pvr, "Error setting MPEG controls (%s).\n", mp_strerror(errno));
        free(ext_ctrl);
        return -1;
    }

    free(ext_ctrl);

    return 0;
}

static void parse_v4l2_tv_options(struct pvr_t *pvr)
{
    if (!pvr)
        return;

    /* Create our station/channel list */
    parse_setup_stationlist(pvr);

    if (pvr->param_channel) {
        if (set_station_by_channelname_or_freq(pvr, pvr->param_channel,
                                               -1, 0) >= 0) {
            if (pvr->tv_params->freq) {
                MP_INFO(pvr, "tv param freq %s is overwritten by channel "
                        "setting freq %d\n", pvr->tv_params->freq, pvr->freq);
            }
        }
    }

    if (pvr->freq < 0 && pvr->tv_params->freq) {
        MP_INFO(pvr, "tv param freq %s is used directly\n",
                pvr->tv_params->freq);

        if (set_station_by_channelname_or_freq(pvr, NULL,
                                               atoi(pvr->tv_params->freq),
                                               0) < 0) {
            MP_WARN(pvr, "tv param freq %s invalid to set station\n",
                    pvr->tv_params->freq);
        }
    }

    if (pvr->tv_params->device) {
        free(pvr->video_dev);
        pvr->video_dev = strdup(pvr->tv_params->device);
    }

    if (!pvr->tv_params->audio)
        pvr->mute = !pvr->tv_params->audio;

    if (pvr->tv_params->input)
        pvr->input = pvr->tv_params->input;

    if (pvr->tv_params->normid)
        pvr->normid = pvr->tv_params->normid;

    if (pvr->tv_params->brightness)
        pvr->brightness = pvr->tv_params->brightness;

    if (pvr->tv_params->contrast)
        pvr->contrast = pvr->tv_params->contrast;

    if (pvr->tv_params->hue)
        pvr->hue = pvr->tv_params->hue;

    if (pvr->tv_params->saturation)
        pvr->saturation = pvr->tv_params->saturation;

    if (pvr->tv_params->width)
        pvr->width = pvr->tv_params->width;

    if (pvr->tv_params->height)
        pvr->height = pvr->tv_params->height;
}

static int set_v4l2_settings(struct pvr_t *pvr)
{
    if (!pvr)
        return -1;

    if (pvr->dev_fd < 0)
        return -1;

    /* -tv noaudio */
    if (pvr->mute) {
        struct v4l2_control ctrl;
        ctrl.id = V4L2_CID_AUDIO_MUTE;
        ctrl.value = 1;
        if (ioctl(pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
            MP_ERR(pvr, "can't mute (%s).\n", mp_strerror(errno));
            return -1;
        }
    }

    /* -tv input=x */
    if (pvr->input != 0) {
        if (ioctl(pvr->dev_fd, VIDIOC_S_INPUT, &pvr->input) < 0) {
            MP_ERR(pvr, "can't set input (%s)\n", mp_strerror(errno));
            return -1;
        }
    }

    /* -tv normid=x */
    if (pvr->normid != -1) {
        struct v4l2_standard std;
        std.index = pvr->normid;

        if (ioctl(pvr->dev_fd, VIDIOC_ENUMSTD, &std) < 0) {
            MP_ERR(pvr, "can't set norm (%s)\n", mp_strerror(errno));
            return -1;
        }

        MP_VERBOSE(pvr, "set norm to %s\n", std.name);

        if (ioctl(pvr->dev_fd, VIDIOC_S_STD, &std.id) < 0) {
            MP_ERR(pvr, "can't set norm (%s)\n", mp_strerror(errno));
            return -1;
        }
    }

    /* -tv brightness=x */
    if (pvr->brightness != 0) {
        struct v4l2_control ctrl;
        ctrl.id = V4L2_CID_BRIGHTNESS;
        ctrl.value = pvr->brightness;

        if (ctrl.value < 0)
            ctrl.value = 0;
        if (ctrl.value > 255)
            ctrl.value = 255;

        if (ioctl(pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
            MP_ERR(pvr, "can't set brightness to %d (%s).\n",
                   ctrl.value, mp_strerror(errno));
            return -1;
        }
    }

    /* -tv contrast=x */
    if (pvr->contrast != 0) {
        struct v4l2_control ctrl;
        ctrl.id = V4L2_CID_CONTRAST;
        ctrl.value = pvr->contrast;

        if (ctrl.value < 0)
            ctrl.value = 0;
        if (ctrl.value > 127)
            ctrl.value = 127;

        if (ioctl(pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
            MP_ERR(pvr, "can't set contrast to %d (%s).\n",
                   ctrl.value, mp_strerror(errno));
            return -1;
        }
    }

    /* -tv hue=x */
    if (pvr->hue != 0) {
        struct v4l2_control ctrl;
        ctrl.id = V4L2_CID_HUE;
        ctrl.value = pvr->hue;

        if (ctrl.value < -128)
            ctrl.value = -128;
        if (ctrl.value > 127)
            ctrl.value = 127;

        if (ioctl(pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
            MP_ERR(pvr, "can't set hue to %d (%s).\n",
                   ctrl.value, mp_strerror(errno));
            return -1;
        }
    }

    /* -tv saturation=x */
    if (pvr->saturation != 0) {
        struct v4l2_control ctrl;
        ctrl.id = V4L2_CID_SATURATION;
        ctrl.value = pvr->saturation;

        if (ctrl.value < 0)
            ctrl.value = 0;
        if (ctrl.value > 127)
            ctrl.value = 127;

        if (ioctl(pvr->dev_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
            MP_ERR(pvr, "can't set saturation to %d (%s).\n",
                   ctrl.value, mp_strerror(errno));
            return -1;
        }
    }

    /* -tv width=x:height=y */
    if (pvr->width && pvr->height) {
        struct v4l2_format vfmt;
        vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vfmt.fmt.pix.width = pvr->width;
        vfmt.fmt.pix.height = pvr->height;

        if (ioctl(pvr->dev_fd, VIDIOC_S_FMT, &vfmt) < 0) {
            MP_ERR(pvr, "can't set resolution to %dx%d (%s).\n",
                   pvr->width, pvr->height, mp_strerror(errno));
            return -1;
        }
    }

    if (pvr->freq < 0) {
        int freq = get_v4l2_freq(pvr);
        MP_INFO(pvr, "Using current set frequency %d, to set channel\n", freq);

        if (0 < freq)
            return set_station_by_channelname_or_freq(pvr, NULL, freq, 1);
    }

    if (0 < pvr->freq)
        return set_v4l2_freq(pvr);

    return 0;
}

static int v4l2_list_capabilities(struct pvr_t *pvr)
{
    struct v4l2_audio vaudio;
    struct v4l2_standard vs;
    struct v4l2_input vin;
    int err = 0;

    if (!pvr)
        return -1;

    if (pvr->dev_fd < 0)
        return -1;

    /* list available video inputs */
    vin.index = 0;
    err = 1;
    MP_INFO(pvr, "Available video inputs: ");
    while (ioctl(pvr->dev_fd, VIDIOC_ENUMINPUT, &vin) >= 0) {
        err = 0;
        MP_INFO(pvr, "'#%d, %s' ", vin.index, vin.name);
        vin.index++;
    }
    if (err) {
        MP_INFO(pvr, "none\n");
        return -1;
    } else {
        MP_INFO(pvr, "\n");
    }

    /* list available audio inputs */
    vaudio.index = 0;
    err = 1;
    MP_INFO(pvr, "Available audio inputs: ");
    while (ioctl(pvr->dev_fd, VIDIOC_ENUMAUDIO, &vaudio) >= 0) {
        err = 0;
        MP_INFO(pvr, "'#%d, %s' ", vaudio.index, vaudio.name);
        vaudio.index++;
    }
    if (err) {
        MP_INFO(pvr, "none\n");
        return -1;
    } else {
        MP_INFO(pvr, "\n");
    }

    /* list available norms */
    vs.index = 0;
    MP_INFO(pvr, "Available norms: ");
    while (ioctl(pvr->dev_fd, VIDIOC_ENUMSTD, &vs) >= 0) {
        err = 0;
        MP_INFO(pvr, "'#%d, %s' ", vs.index, vs.name);
        vs.index++;
    }
    if (err) {
        MP_INFO(pvr, "none\n");
        return -1;
    } else {
        MP_INFO(pvr, "\n");
    }

    return 0;
}

static int v4l2_display_settings(struct pvr_t *pvr)
{
    struct v4l2_audio vaudio;
    struct v4l2_standard vs;
    struct v4l2_input vin;
    v4l2_std_id std;
    int input;

    if (!pvr)
        return -1;

    if (pvr->dev_fd < 0)
        return -1;

    /* get current video input */
    if (ioctl(pvr->dev_fd, VIDIOC_G_INPUT, &input) == 0) {
        vin.index = input;
        if (ioctl(pvr->dev_fd, VIDIOC_ENUMINPUT, &vin) < 0) {
            MP_ERR(pvr, "can't get input (%s).\n", mp_strerror(errno));
            return -1;
        } else {
            MP_INFO(pvr, "Video input: %s\n", vin.name);
        }
    } else {
        MP_ERR(pvr, "can't get input (%s).\n", mp_strerror(errno));
        return -1;
    }

    /* get current audio input */
    if (ioctl(pvr->dev_fd, VIDIOC_G_AUDIO, &vaudio) == 0) {
        MP_INFO(pvr, "Audio input: %s\n", vaudio.name);
    } else {
        MP_ERR(pvr, "can't get input (%s).\n", mp_strerror(errno));
        return -1;
    }

    /* get current video format */
    if (ioctl(pvr->dev_fd, VIDIOC_G_STD, &std) == 0) {
        vs.index = 0;

        while (ioctl(pvr->dev_fd, VIDIOC_ENUMSTD, &vs) >= 0) {
            if (vs.id == std) {
                MP_INFO(pvr, "Norm: %s.\n", vs.name);
                break;
            }
            vs.index++;
        }
    } else {
        MP_ERR(pvr, "can't get norm (%s)\n", mp_strerror(errno));
        return -1;
    }

    return 0;
}

/* stream layer */

static void pvr_stream_close(stream_t *stream)
{
    struct pvr_t *pvr;

    if (!stream)
        return;

    pvr = (struct pvr_t *) stream->priv;
    pvr_uninit(pvr);
}

static int pvr_stream_read(stream_t *stream, char *buffer, int size)
{
    struct pollfd pfds[1];
    struct pvr_t *pvr;
    int rk, fd, pos;

    if (!stream || !buffer)
        return 0;

    pvr = (struct pvr_t *) stream->priv;
    fd = pvr->dev_fd;
    pos = 0;

    if (fd < 0)
        return 0;

    while (pos < size) {
        pfds[0].fd = fd;
        pfds[0].events = POLLIN | POLLPRI;

        rk = size - pos;

        int r = poll(pfds, 1, 5000);
        if (r <= 0) {
            if (r < 0) {
                MP_ERR(pvr, "failed with '%s' when reading %d bytes\n",
                       mp_strerror(errno), size - pos);
            } else {
                MP_ERR(pvr, "timeout when trying to read from device\n");
            }
            break;
        }

        rk = read(fd, &buffer[pos], rk);
        if (rk > 0) {
            pos += rk;
            MP_TRACE(pvr, "read (%d) bytes\n", pos);
        }
    }

    if (!pos)
        MP_ERR(pvr, "read %d bytes\n", pos);

    return pos;
}

static int pvr_stream_open(stream_t *stream)
{
    struct v4l2_capability vcap;
    struct v4l2_ext_controls ctrls;
    struct pvr_t *pvr = NULL;

    pvr = pvr_init();
    pvr->tv_params = stream->opts->tv_params;
    pvr->log = stream->log;
    pvr->params = stream->opts->stream_pvr_opts;

    /**
     * if the url, i.e. 'pvr://8', contains the channel, use it,
     * else use the tv parameter.
     */
    if (stream->url && strlen(stream->url) > 6 && stream->url[6] != '\0') {
        pvr->param_channel = strdup(stream->url + 6);
    } else if (pvr->tv_params->channel && strlen(pvr->tv_params->channel)) {
        pvr->param_channel = strdup(pvr->tv_params->channel);
    }

    parse_v4l2_tv_options(pvr);
    parse_encoder_options(pvr);

    /* open device */
    pvr->dev_fd = open(pvr->video_dev, O_RDWR | O_CLOEXEC);
    MP_INFO(pvr, "Using device %s\n", pvr->video_dev);
    if (pvr->dev_fd == -1) {
        MP_ERR(pvr, "error opening device %s\n", pvr->video_dev);
        pvr_uninit(pvr);
        return STREAM_ERROR;
    }

    /* query capabilities (i.e test V4L2 support) */
    if (ioctl(pvr->dev_fd, VIDIOC_QUERYCAP, &vcap) < 0) {
        MP_ERR(pvr, "device is not V4L2 compliant (%s).\n", mp_strerror(errno));
        pvr_uninit(pvr);
        return STREAM_ERROR;
    } else {
        MP_INFO(pvr, "Detected %s\n", vcap.card);
    }

    /* check for a valid V4L2 capture device */
    if (!(vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        MP_ERR(pvr, "device is not a valid V4L2 capture device.\n");
        pvr_uninit(pvr);
        return STREAM_ERROR;
    }

    /* check for device hardware MPEG encoding capability */
    ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ctrls.count = 0;
    ctrls.controls = NULL;

    if (ioctl(pvr->dev_fd, VIDIOC_G_EXT_CTRLS, &ctrls) < 0) {
        MP_ERR(pvr, "device do not support MPEG input.\n");
        pvr_uninit(pvr);
        return STREAM_ERROR;
    }

    /* list V4L2 capabilities */
    if (v4l2_list_capabilities(pvr) == -1) {
        MP_ERR(pvr, "can't get v4l2 capabilities\n");
        pvr_uninit(pvr);
        return STREAM_ERROR;
    }

    /* apply V4L2 settings */
    if (set_v4l2_settings(pvr) == -1) {
        MP_ERR(pvr, "can't set v4l2 settings\n");
        pvr_uninit(pvr);
        return STREAM_ERROR;
    }

    /* apply encoder settings */
    if (set_encoder_settings(pvr) == -1) {
        MP_ERR(pvr, "can't set encoder settings\n");
        pvr_uninit(pvr);
        return STREAM_ERROR;
    }

    /* display current V4L2 settings */
    if (v4l2_display_settings(pvr) == -1) {
        MP_ERR(pvr, "can't get v4l2 settings\n");
        pvr_uninit(pvr);
        return STREAM_ERROR;
    }

    stream->priv = pvr;
    stream->fill_buffer = pvr_stream_read;
    stream->close = pvr_stream_close;
    stream->control = pvr_stream_control;

    return STREAM_OK;
}

/* PVR Public API access */

#if 0
static const char *pvr_get_current_stationname(stream_t *stream)
{
    struct pvr_t *pvr;

    if (!stream || stream->type != STREAMTYPE_PVR)
        return NULL;

    pvr = (struct pvr_t *) stream->priv;

    if (pvr->stationlist.list &&
        pvr->stationlist.used > pvr->chan_idx &&
        pvr->chan_idx >= 0)
        return pvr->stationlist.list[pvr->chan_idx].station;

    return NULL;
}

static const char *pvr_get_current_channelname(stream_t *stream)
{
    struct pvr_t *pvr = (struct pvr_t *) stream->priv;

    if (pvr->stationlist.list &&
        pvr->stationlist.used > pvr->chan_idx &&
        pvr->chan_idx >= 0)
        return pvr->stationlist.list[pvr->chan_idx].name;

    return NULL;
}
#endif

static int pvr_get_current_frequency(stream_t *stream)
{
    struct pvr_t *pvr = (struct pvr_t *) stream->priv;

    return pvr->freq;
}

static int pvr_set_channel(stream_t *stream, const char *channel)
{
    struct pvr_t *pvr = (struct pvr_t *) stream->priv;

    return set_station_by_channelname_or_freq(pvr, channel, -1, 1);
}

static int pvr_set_lastchannel(stream_t *stream)
{
    struct pvr_t *pvr = (struct pvr_t *) stream->priv;

    if (pvr->stationlist.list &&
        pvr->stationlist.used > pvr->chan_idx_last &&
        pvr->chan_idx_last >= 0)
        return set_station_by_channelname_or_freq(
            pvr, pvr->stationlist.list[pvr->chan_idx_last].name, -1, 1);

    return -1;
}

static int pvr_set_freq(stream_t *stream, int freq)
{
    struct pvr_t *pvr = (struct pvr_t *) stream->priv;

    return set_station_by_channelname_or_freq(pvr, NULL, freq, 1);
}

static int pvr_set_channel_step(stream_t *stream, int step)
{
    struct pvr_t *pvr = (struct pvr_t *) stream->priv;

    return set_station_by_step(pvr, step, 1);
}

static int pvr_stream_control(struct stream *s, int cmd, void *arg)
{
    switch (cmd) {
    case STREAM_CTRL_SET_TV_FREQ:
        pvr_set_freq(s, (int)(*(float *)arg + 0.5f));
        return STREAM_OK;
    case STREAM_CTRL_GET_TV_FREQ:
        *(float *)arg = pvr_get_current_frequency(s);
        return STREAM_OK;
    case STREAM_CTRL_TV_SET_CHAN:
        pvr_set_channel(s, (char *)arg);
        return STREAM_OK;
    case STREAM_CTRL_TV_STEP_CHAN:
        pvr_set_channel_step(s, *(int *)arg);
        return STREAM_OK;
    case STREAM_CTRL_TV_LAST_CHAN:
        pvr_set_lastchannel(s);
        return STREAM_OK;
    }
    return STREAM_UNSUPPORTED;
}

const stream_info_t stream_info_pvr = {
    .name = "pvr",
    .open = pvr_stream_open,
    .protocols = (const char *const[]){ "pvr", NULL },
};
