/*
 * Video 4 Linux 2 input
 *
 * copyright (c) 2003 Martin Olschewski <olschewski@zpr.uni-koeln.de>
 * copyright (c) 2003 Jindrich Makovicka <makovick@gmail.com>
 *
 * Some ideas are based on works from
 *   Alex Beregszaszi <alex@fsn.hu>
 *   Gerd Knorr <kraxel@bytesex.org>
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

/*
known issues:
- norm setting isn't consistent with tvi_v4l
- the same for volume/bass/treble/balance
*/

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#if HAVE_SYS_VIDEOIO_H
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif
#if HAVE_LIBV4L2
#include <libv4l2.h>
#endif
#include "common/msg.h"
#include "common/common.h"
#include "audio/format.h"
#include "tv.h"
#include "audio_in.h"

#if !HAVE_LIBV4L2
#define v4l2_open   open
#define v4l2_close  close
#define v4l2_ioctl  ioctl
#define v4l2_mmap   mmap
#define v4l2_munmap munmap
#endif

// flag introduced in kernel 3.10
#ifndef V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC
#define V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC 0x2000
#endif

#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
#define HAVE_CLOCK_GETTIME 1
#else
#define HAVE_CLOCK_GETTIME 0
#endif

#define info tvi_info_v4l2
static tvi_handle_t *tvi_init_v4l2(struct mp_log *log, tv_param_t* tv_param);
/* information about this file */
const tvi_info_t tvi_info_v4l2 = {
    tvi_init_v4l2,
    "Video 4 Linux 2 input",
    "v4l2",
};

struct map {
    struct v4l2_buffer buf;
    void   *addr;
    size_t len;
};

#define BUFFER_COUNT 6

/** video ringbuffer entry */
typedef struct {
    unsigned char               *data;     ///< frame contents
    long long                   timestamp; ///< frame timestamp
    int                         framesize; ///< actual frame size
} video_buffer_entry;

/* private data */
typedef struct priv {
    /* video */
    struct mp_log               *log;
    char                        *video_dev;
    int                         video_fd;
    int                         mp_format;
    struct v4l2_capability      capability;
    struct v4l2_input           input;
    struct v4l2_format          format;
    struct v4l2_standard        standard;
    struct v4l2_tuner           tuner;
    struct map                  *map;
    int                         mapcount;
    int                         frames;
    volatile long long          first_frame; ///< number of useconds
    long long                   curr_frame;  ///< usec, using kernel timestamps
    int                         clk_id;      /**< clk_id from clock_gettime
                                                  used in frame timestamps */
    /* audio video interleaving ;-) */
    volatile int                streamon;
    pthread_t                   audio_grabber_thread;
    pthread_mutex_t             skew_mutex;

    /* 2nd level video buffers */
    int                         first;
    int                         immediate_mode;

    int                         video_buffer_size_max;
    volatile int                video_buffer_size_current;
    video_buffer_entry          *video_ringbuffer;
    volatile int                video_head;
    volatile int                video_tail;
    volatile int                video_cnt;
    pthread_t                   video_grabber_thread;
    pthread_mutex_t             video_buffer_mutex;

    /* audio */
    char                        *audio_dev;
    audio_in_t                  audio_in;

    long long                   audio_start_time;
    int                         audio_buffer_size;
    int                         aud_skew_cnt;
    unsigned char               *audio_ringbuffer;
    long long                   *audio_skew_buffer;
    long long                   *audio_skew_delta_buffer;
    volatile int                audio_head;
    volatile int                audio_tail;
    volatile int                audio_cnt;
    volatile long long          audio_skew;
    volatile double             audio_skew_factor;
    volatile long long          audio_skew_measure_time;
    volatile int                audio_drop;
    volatile int                shutdown;

    int                         audio_initialized;
    double                      audio_secs_per_block;
    long long                   audio_usecs_per_block;
    long long                   audio_skew_total;
    long long                   audio_skew_delta_total;
    long                        audio_recv_blocks_total;
    long                        audio_sent_blocks_total;
    pthread_mutex_t             audio_mutex;
    int                         audio_insert_null_samples;
    volatile long               audio_null_blocks_inserted;
    volatile long long          dropped_frames_timeshift;
    long long                   dropped_frames_compensated;

    tv_param_t                  *tv_param;
} priv_t;

typedef struct tt_stream_props_s{
    int sampling_rate;
    int samples_per_line;
    int offset;
    int count[2];     ///< number of lines in first and second fields
    int interlaced;   ///< vbi data are interlaced
    int bufsize;      ///< required buffer size
} tt_stream_props;

#include "tvi_def.h"

static void *audio_grabber(void *data);
static void *video_grabber(void *data);

/**********************************************************************\

    Only few of the fourccs are the same in v4l2 and mplayer:

    MP_FOURCC_YVU9 == V4L2_PIX_FMT_YVU410
    MP_FOURCC_YV12 == V4L2_PIX_FMT_YVU420
    MP_FOURCC_NV12 == V4L2_PIX_FMT_NV12
    MP_FOURCC_422P == V4L2_PIX_FMT_YUV422P
    MP_FOURCC_411P == V4L2_PIX_FMT_YUV411P
    MP_FOURCC_UYVY == V4L2_PIX_FMT_UYVY
    MP_FOURCC_Y41P == V4L2_PIX_FMT_Y41P

    This may be an useful translation table for some others:

    MP_FOURCC_RGB8  == V4L2_PIX_FMT_RGB332
    MP_FOURCC_BGR15 == V4L2_PIX_FMT_RGB555
    MP_FOURCC_BGR16 == V4L2_PIX_FMT_RGB565
    MP_FOURCC_RGB24 == V4L2_PIX_FMT_RGB24
    MP_FOURCC_RGB32 == V4L2_PIX_FMT_RGB32
    MP_FOURCC_BGR24 == V4L2_PIX_FMT_BGR24
    MP_FOURCC_BGR32 == V4L2_PIX_FMT_BGR32
    MP_FOURCC_Y800  == V4L2_PIX_FMT_GREY
    MP_FOURCC_YUV9  == V4L2_PIX_FMT_YUV410
    MP_FOURCC_I420  == V4L2_PIX_FMT_YUV420
    MP_FOURCC_YUY2  == V4L2_PIX_FMT_YUYV

\**********************************************************************/

/*
** Translate a mplayer fourcc to a video4linux2 pixel format.
*/
static int fcc_mp2vl(int fcc)
{
    switch (fcc) {
    case MP_FOURCC_RGB8:   return V4L2_PIX_FMT_RGB332;
    case MP_FOURCC_BGR15:  return V4L2_PIX_FMT_RGB555;
    case MP_FOURCC_BGR16:  return V4L2_PIX_FMT_RGB565;
    case MP_FOURCC_RGB24:  return V4L2_PIX_FMT_RGB24;
    case MP_FOURCC_RGB32:  return V4L2_PIX_FMT_RGB32;
    case MP_FOURCC_BGR24:  return V4L2_PIX_FMT_BGR24;
    case MP_FOURCC_BGR32:  return V4L2_PIX_FMT_BGR32;
    case MP_FOURCC_Y800:   return V4L2_PIX_FMT_GREY;
    case MP_FOURCC_YUV9:   return V4L2_PIX_FMT_YUV410;
    case MP_FOURCC_I420:   return V4L2_PIX_FMT_YUV420;
    case MP_FOURCC_YUY2:   return V4L2_PIX_FMT_YUYV;
    case MP_FOURCC_YV12:   return V4L2_PIX_FMT_YVU420;
    case MP_FOURCC_UYVY:   return V4L2_PIX_FMT_UYVY;
    case MP_FOURCC_MJPEG:  return V4L2_PIX_FMT_MJPEG;
    case MP_FOURCC_JPEG:   return V4L2_PIX_FMT_JPEG;
    }
    return fcc;
}

/*
** Translate a video4linux2 fourcc aka pixel format to mplayer.
*/
static int fcc_vl2mp(int fcc)
{
    switch (fcc) {
    case V4L2_PIX_FMT_RGB332:   return MP_FOURCC_RGB8;
    case V4L2_PIX_FMT_RGB555:   return MP_FOURCC_BGR15;
    case V4L2_PIX_FMT_RGB565:   return MP_FOURCC_BGR16;
    case V4L2_PIX_FMT_RGB24:    return MP_FOURCC_RGB24;
    case V4L2_PIX_FMT_RGB32:    return MP_FOURCC_RGB32;
    case V4L2_PIX_FMT_BGR24:    return MP_FOURCC_BGR24;
    case V4L2_PIX_FMT_BGR32:    return MP_FOURCC_BGR32;
    case V4L2_PIX_FMT_GREY:     return MP_FOURCC_Y800;
    case V4L2_PIX_FMT_YUV410:   return MP_FOURCC_YUV9;
    case V4L2_PIX_FMT_YUV420:   return MP_FOURCC_I420;
    case V4L2_PIX_FMT_YVU420:   return MP_FOURCC_YV12;
    case V4L2_PIX_FMT_YUYV:     return MP_FOURCC_YUY2;
    case V4L2_PIX_FMT_UYVY:     return MP_FOURCC_UYVY;
    case V4L2_PIX_FMT_MJPEG:    return MP_FOURCC_MJPEG;
    case V4L2_PIX_FMT_JPEG:     return MP_FOURCC_JPEG;
    }
    return fcc;
}

/*
** Translate a video4linux2 fourcc aka pixel format
** to a human readable string.
*/
static const char *pixfmt2name(char *buf, int pixfmt)
{
    switch (pixfmt) {
    case V4L2_PIX_FMT_RGB332:       return "RGB332";
    case V4L2_PIX_FMT_RGB555:       return "RGB555";
    case V4L2_PIX_FMT_RGB565:       return "RGB565";
    case V4L2_PIX_FMT_RGB555X:      return "RGB555X";
    case V4L2_PIX_FMT_RGB565X:      return "RGB565X";
    case V4L2_PIX_FMT_BGR24:        return "BGR24";
    case V4L2_PIX_FMT_RGB24:        return "RGB24";
    case V4L2_PIX_FMT_BGR32:        return "BGR32";
    case V4L2_PIX_FMT_RGB32:        return "RGB32";
    case V4L2_PIX_FMT_GREY:         return "GREY";
    case V4L2_PIX_FMT_YVU410:       return "YVU410";
    case V4L2_PIX_FMT_YVU420:       return "YVU420";
    case V4L2_PIX_FMT_YUYV:         return "YUYV";
    case V4L2_PIX_FMT_UYVY:         return "UYVY";
/*    case V4L2_PIX_FMT_YVU422P:      return "YVU422P"; */
/*    case V4L2_PIX_FMT_YVU411P:      return "YVU411P"; */
    case V4L2_PIX_FMT_YUV422P:      return "YUV422P";
    case V4L2_PIX_FMT_YUV411P:      return "YUV411P";
    case V4L2_PIX_FMT_Y41P:         return "Y41P";
    case V4L2_PIX_FMT_NV12:         return "NV12";
    case V4L2_PIX_FMT_NV21:         return "NV21";
    case V4L2_PIX_FMT_YUV410:       return "YUV410";
    case V4L2_PIX_FMT_YUV420:       return "YUV420";
    case V4L2_PIX_FMT_YYUV:         return "YYUV";
    case V4L2_PIX_FMT_HI240:        return "HI240";
    case V4L2_PIX_FMT_WNVA:         return "WNVA";
    case V4L2_PIX_FMT_MJPEG:        return "MJPEG";
    case V4L2_PIX_FMT_JPEG:         return "JPEG";
    }
    sprintf(buf, "unknown (0x%x)", pixfmt);
    return buf;
}


/*
** Gives the depth of a video4linux2 fourcc aka pixel format in bits.
*/
static int pixfmt2depth(int pixfmt)
{
    switch (pixfmt) {
    case V4L2_PIX_FMT_RGB332:
        return 8;
    case V4L2_PIX_FMT_RGB555:
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB565X:
        return 16;
    case V4L2_PIX_FMT_BGR24:
    case V4L2_PIX_FMT_RGB24:
        return 24;
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB32:
        return 32;
    case V4L2_PIX_FMT_GREY:
        return 8;
    case V4L2_PIX_FMT_YVU410:
        return 9;
    case V4L2_PIX_FMT_YVU420:
        return 12;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_YUV422P:
    case V4L2_PIX_FMT_YUV411P:
        return 16;
    case V4L2_PIX_FMT_Y41P:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
        return 12;
    case V4L2_PIX_FMT_YUV410:
        return 9;
    case V4L2_PIX_FMT_YUV420:
        return 12;
    case V4L2_PIX_FMT_YYUV:
        return 16;
    case V4L2_PIX_FMT_HI240:
        return 8;

    }
    return 0;
}

static int amode2v4l(int amode)
{
    switch (amode) {
    case 0:
        return V4L2_TUNER_MODE_MONO;
    case 1:
        return V4L2_TUNER_MODE_STEREO;
    case 2:
        return V4L2_TUNER_MODE_LANG1;
    case 3:
        return V4L2_TUNER_MODE_LANG2;
    default:
        return -1;
    }
}


/*
** Get current FPS.
*/
static double getfps(priv_t *priv)
{
    if (priv->tv_param->fps > 0)
        return priv->tv_param->fps;
    if (priv->standard.frameperiod.denominator && priv->standard.frameperiod.numerator)
        return (double)priv->standard.frameperiod.denominator / priv->standard.frameperiod.numerator;
    return 25.0;
}

// sets and sanitizes audio buffer/block sizes
static void setup_audio_buffer_sizes(priv_t *priv)
{
    int bytes_per_sample = priv->audio_in.bytes_per_sample;
    int seconds = priv->video_buffer_size_max/getfps(priv);

    if (seconds < 5) seconds = 5;
    if (seconds > 500) seconds = 500;

    // make the audio buffer at least as the video buffer capacity (or 5 seconds) long
    priv->audio_buffer_size = 1 + seconds*priv->audio_in.samplerate
        *priv->audio_in.channels
        *bytes_per_sample/priv->audio_in.blocksize;
    if (priv->audio_buffer_size < 256) priv->audio_buffer_size = 256;

    // make the skew buffer at least 1 second long
    priv->aud_skew_cnt = 1 + 1*priv->audio_in.samplerate
        *priv->audio_in.channels
        *bytes_per_sample/priv->audio_in.blocksize;
    if (priv->aud_skew_cnt < 16) priv->aud_skew_cnt = 16;

    MP_VERBOSE(priv, "Audio capture - buffer %d blocks of %d bytes, skew average from %d meas.\n",
           priv->audio_buffer_size, priv->audio_in.blocksize, priv->aud_skew_cnt);
}

static void init_audio(priv_t *priv)
{
    if (priv->audio_initialized) return;

    if (priv->tv_param->audio) {
#if HAVE_ALSA
        if (priv->tv_param->alsa)
            audio_in_init(&priv->audio_in, priv->log, AUDIO_IN_ALSA);
        else
            audio_in_init(&priv->audio_in, priv->log, AUDIO_IN_OSS);
#else
        audio_in_init(&priv->audio_in, priv->log, AUDIO_IN_OSS);
#endif

        if (priv->audio_dev) {
            audio_in_set_device(&priv->audio_in, priv->audio_dev);
        }

        audio_in_set_samplerate(&priv->audio_in, 44100);
        if (priv->capability.capabilities & V4L2_CAP_TUNER) {
            if (priv->tuner.audmode == V4L2_TUNER_MODE_STEREO) {
                audio_in_set_channels(&priv->audio_in, 2);
            } else {
                audio_in_set_channels(&priv->audio_in, 1);
            }
        } else {
            if (priv->tv_param->forcechan >= 0) {
                audio_in_set_channels(&priv->audio_in, priv->tv_param->forcechan);
            } else {
                audio_in_set_channels(&priv->audio_in, 2);
            }
        }

        if (audio_in_setup(&priv->audio_in) < 0) return;

        priv->audio_initialized = 1;
    }
}

#if 0
/*
** the number of milliseconds elapsed between time0 and time1
*/
static size_t difftv(struct timeval time1, struct timeval time0)
{
    return        (time1.tv_sec  - time0.tv_sec)  * 1000 +
        (time1.tv_usec - time0.tv_usec) / 1000;
}
#endif

/*
** Get current video capture format.
*/
static int getfmt(priv_t *priv)
{
    int i;

    priv->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if ((i = v4l2_ioctl(priv->video_fd, VIDIOC_G_FMT, &priv->format)) < 0) {
        MP_ERR(priv, "ioctl get format failed: %s\n",
               mp_strerror(errno));
    }
    return i;
}


/*
** Get current video capture standard.
*/
static int getstd(priv_t *priv)
{
    v4l2_std_id id;
    int i=0;

    if (v4l2_ioctl(priv->video_fd, VIDIOC_G_STD, &id) < 0) {
        struct v4l2_streamparm      parm;

        parm.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(v4l2_ioctl(priv->video_fd, VIDIOC_G_PARM, &parm) >= 0) {
            MP_WARN(priv, "your device driver does not support VIDIOC_G_STD ioctl,"
                   " VIDIOC_G_PARM was used instead.\n");
            priv->standard.index=0;
            priv->standard.id=0;
            priv->standard.frameperiod=parm.parm.capture.timeperframe;
            return 0;
        }

        MP_ERR(priv, "ioctl get standard failed: %s\n", mp_strerror(errno));
        return -1;
    }
    do {
        priv->standard.index = i++;
        if (v4l2_ioctl(priv->video_fd, VIDIOC_ENUMSTD, &priv->standard) < 0) {
            return -1;
        }
    } while (priv->standard.id != id);
    return 0;
}

#if HAVE_CLOCK_GETTIME
/*
** Gets current timestamp, using specified clock id.
** @return number of microseconds.
*/
static long long get_curr_timestamp(int clk_id)
{
    struct timespec ts;
    clock_gettime(clk_id, &ts);
    return (long long)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#else
/*
** Gets current timestamp, using system time.
** @return number of microseconds.
*/
static long long get_curr_timestamp(int clk_id)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

/***********************************************************************\
 *                                                                     *
 *                                                                     *
 *        Interface to mplayer                                         *
 *                                                                     *
 *                                                                     *
\***********************************************************************/

static int set_mute(priv_t *priv, int value)
{
    struct v4l2_control control;
    control.id = V4L2_CID_AUDIO_MUTE;
    control.value = value;
    if (v4l2_ioctl(priv->video_fd, VIDIOC_S_CTRL, &control) < 0) {
        MP_ERR(priv, "ioctl set mute failed: %s\n", mp_strerror(errno));
        return 0;
    }
    return 1;
}

/*
** MPlayer uses values from -100 up to 100 for controls.
** Here they are scaled to what the tv card needs and applied.
*/
static int set_control(priv_t *priv, struct v4l2_control *control, int val_signed) {
    struct v4l2_queryctrl        qctrl;
    qctrl.id = control->id;
    if (v4l2_ioctl(priv->video_fd, VIDIOC_QUERYCTRL, &qctrl) < 0) {
        MP_ERR(priv, "ioctl query control failed: %s\n", mp_strerror(errno));
        return TVI_CONTROL_FALSE;
    }

    if (val_signed) {
        if (control->value < 0) {
            control->value = qctrl.default_value + control->value *
                (qctrl.default_value - qctrl.minimum) / 100;
        } else {
            control->value = qctrl.default_value + control->value *
                (qctrl.maximum - qctrl.default_value) / 100;
        }
    } else {
        if (control->value < 50) {
            control->value = qctrl.default_value + (control->value-50) *
                (qctrl.default_value - qctrl.minimum) / 50;
        } else {
            control->value = qctrl.default_value + (control->value-50) *
                (qctrl.maximum - qctrl.default_value) / 50;
        }
    }


    if (v4l2_ioctl(priv->video_fd, VIDIOC_S_CTRL, control) < 0) {
        MP_ERR(priv, "ioctl set %s %d failed: %s\n",
               qctrl.name, control->value, mp_strerror(errno));
        return TVI_CONTROL_FALSE;
    }
    MP_VERBOSE(priv, "set %s: %d [%d, %d]\n",
     qctrl.name, control->value, qctrl.minimum, qctrl.maximum);

    return TVI_CONTROL_TRUE;
}


/*
** Scale the control values back to what mplayer needs.
*/
static int get_control(priv_t *priv, struct v4l2_control *control, int val_signed) {
    struct v4l2_queryctrl        qctrl;

    qctrl.id = control->id;
    if (v4l2_ioctl(priv->video_fd, VIDIOC_QUERYCTRL, &qctrl) < 0) {
        MP_ERR(priv, "ioctl query control failed: %s\n", mp_strerror(errno));
        return TVI_CONTROL_FALSE;
    }

    if (v4l2_ioctl(priv->video_fd, VIDIOC_G_CTRL, control) < 0) {
        MP_ERR(priv, "ioctl get %s failed: %s\n", qctrl.name, mp_strerror(errno));
        return TVI_CONTROL_FALSE;
    }
    MP_VERBOSE(priv, "get %s: %d [%d, %d]\n",
     qctrl.name, control->value, qctrl.minimum, qctrl.maximum);

    if (val_signed) {
        if (control->value < qctrl.default_value) {
            control->value = (control->value - qctrl.default_value) * 100 /
                (qctrl.default_value - qctrl.minimum);
        } else {
            control->value = (control->value - qctrl.default_value) * 100 /
                (qctrl.maximum - qctrl.default_value);
        }
    } else {
        if (control->value < qctrl.default_value) {
            control->value = (control->value - qctrl.default_value) * 50 /
                (qctrl.default_value - qctrl.minimum) + 50;
        } else {
            control->value = (control->value - qctrl.default_value) * 50 /
                (qctrl.maximum - qctrl.default_value) + 50;
        }
    }

    return TVI_CONTROL_TRUE;
}

static int do_control(priv_t *priv, int cmd, void *arg)
{
    struct v4l2_control control;
    struct v4l2_frequency frequency;
    char buf[80];

    switch(cmd) {
    case TVI_CONTROL_IS_VIDEO:
        return priv->capability.capabilities & V4L2_CAP_VIDEO_CAPTURE?
            TVI_CONTROL_TRUE: TVI_CONTROL_FALSE;
    case TVI_CONTROL_IS_AUDIO:
        if (priv->tv_param->force_audio) return TVI_CONTROL_TRUE;
    case TVI_CONTROL_IS_TUNER:
        return priv->capability.capabilities & V4L2_CAP_TUNER?
            TVI_CONTROL_TRUE: TVI_CONTROL_FALSE;
    case TVI_CONTROL_IMMEDIATE:
        priv->immediate_mode = 1;
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_GET_FPS:
        *(float *)arg = getfps(priv);
        MP_VERBOSE(priv, "get fps: %f\n", *(float *)arg);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_GET_BITS:
        if (getfmt(priv) < 0) return TVI_CONTROL_FALSE;
        *(int *)arg = pixfmt2depth(priv->format.fmt.pix.pixelformat);
        MP_VERBOSE(priv, "get depth: %d\n", *(int *)arg);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_GET_FORMAT:
        if (getfmt(priv) < 0) return TVI_CONTROL_FALSE;
        *(int *)arg = fcc_vl2mp(priv->format.fmt.pix.pixelformat);
        MP_VERBOSE(priv, "get format: %s\n",
                   pixfmt2name(buf, priv->format.fmt.pix.pixelformat));
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_SET_FORMAT:
        if (getfmt(priv) < 0) return TVI_CONTROL_FALSE;
        priv->format.fmt.pix.pixelformat = fcc_mp2vl(*(int *)arg);
        priv->format.fmt.pix.field = V4L2_FIELD_ANY;

        priv->mp_format = *(int *)arg;
        MP_VERBOSE(priv, "set format: %s\n",
               pixfmt2name(buf, priv->format.fmt.pix.pixelformat));
        if (v4l2_ioctl(priv->video_fd, VIDIOC_S_FMT, &priv->format) < 0) {
            MP_ERR(priv, "ioctl set format failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        /* according to the v4l2 specs VIDIOC_S_FMT should not fail, inflexible drivers
          might even always return the default parameters -> update the format here*/
        priv->mp_format = fcc_vl2mp(priv->format.fmt.pix.pixelformat);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_GET_WIDTH:
        if (getfmt(priv) < 0) return TVI_CONTROL_FALSE;
        *(int *)arg = priv->format.fmt.pix.width;
        MP_VERBOSE(priv, "get width: %d\n", *(int *)arg);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_CHK_WIDTH:
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_SET_WIDTH_HEIGHT:
        if (getfmt(priv) < 0) return TVI_CONTROL_FALSE;
        priv->format.fmt.pix.width = ((int *)arg)[0];
        priv->format.fmt.pix.height = ((int *)arg)[1];
        priv->format.fmt.pix.field = V4L2_FIELD_ANY;
        if (v4l2_ioctl(priv->video_fd, VIDIOC_S_FMT, &priv->format) < 0)
            return TVI_CONTROL_FALSE;
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_SET_WIDTH:
        if (getfmt(priv) < 0) return TVI_CONTROL_FALSE;
        priv->format.fmt.pix.width = *(int *)arg;
        MP_VERBOSE(priv, "set width: %d\n", *(int *)arg);
        if (v4l2_ioctl(priv->video_fd, VIDIOC_S_FMT, &priv->format) < 0) {
            MP_ERR(priv, "ioctl set width failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_GET_HEIGHT:
        if (getfmt(priv) < 0) return TVI_CONTROL_FALSE;
        *(int *)arg = priv->format.fmt.pix.height;
        MP_VERBOSE(priv, "get height: %d\n", *(int *)arg);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_CHK_HEIGHT:
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_VID_SET_HEIGHT:
        if (getfmt(priv) < 0) return TVI_CONTROL_FALSE;
        priv->format.fmt.pix.height = *(int *)arg;
        priv->format.fmt.pix.field = V4L2_FIELD_ANY;
        MP_VERBOSE(priv, "set height: %d\n", *(int *)arg);
        if (v4l2_ioctl(priv->video_fd, VIDIOC_S_FMT, &priv->format) < 0) {
            MP_ERR(priv, "ioctl set height failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        return TVI_CONTROL_TRUE;
        case TVI_CONTROL_VID_GET_BRIGHTNESS:
            control.id = V4L2_CID_BRIGHTNESS;
            if (get_control(priv, &control, 1) == TVI_CONTROL_TRUE) {
                *(int *)arg = control.value;
                return TVI_CONTROL_TRUE;
            }
            return TVI_CONTROL_FALSE;
        case TVI_CONTROL_VID_SET_BRIGHTNESS:
            control.id = V4L2_CID_BRIGHTNESS;
            control.value = *(int *)arg;
            return set_control(priv, &control, 1);
        case TVI_CONTROL_VID_GET_HUE:
            control.id = V4L2_CID_HUE;
            if (get_control(priv, &control, 1) == TVI_CONTROL_TRUE) {
                *(int *)arg = control.value;
                return TVI_CONTROL_TRUE;
            }
            return TVI_CONTROL_FALSE;
        case TVI_CONTROL_VID_SET_HUE:
            control.id = V4L2_CID_HUE;
            control.value = *(int *)arg;
            return set_control(priv, &control, 1);
        case TVI_CONTROL_VID_GET_SATURATION:
            control.id = V4L2_CID_SATURATION;
            if (get_control(priv, &control, 1) == TVI_CONTROL_TRUE) {
                *(int *)arg = control.value;
                return TVI_CONTROL_TRUE;
            }
            return TVI_CONTROL_FALSE;
        case TVI_CONTROL_VID_SET_SATURATION:
            control.id = V4L2_CID_SATURATION;
            control.value = *(int *)arg;
            return set_control(priv, &control, 1);
        case TVI_CONTROL_VID_GET_GAIN:
        {

            control.id = V4L2_CID_AUTOGAIN;
            if(get_control(priv,&control,0)!=TVI_CONTROL_TRUE)
                return TVI_CONTROL_FALSE;

            if(control.value){ //Auto Gain control is enabled
                *(int*)arg=0;
                return TVI_CONTROL_TRUE;
            }

            //Manual Gain control
            control.id = V4L2_CID_GAIN;
            if(get_control(priv,&control,0)!=TVI_CONTROL_TRUE)
                return TVI_CONTROL_FALSE;

            *(int*)arg=control.value?control.value:1;

            return TVI_CONTROL_TRUE;
        }
        case TVI_CONTROL_VID_SET_GAIN:
        {
            //value==0 means automatic gain control
            int value=*(int*)arg;

            if (value < 0 || value>100)
                return TVI_CONTROL_FALSE;

            control.id=value?V4L2_CID_GAIN:V4L2_CID_AUTOGAIN;
            control.value=value?value:1;

            return set_control(priv,&control,0);
        }
        case TVI_CONTROL_VID_GET_CONTRAST:
            control.id = V4L2_CID_CONTRAST;
            if (get_control(priv, &control, 1) == TVI_CONTROL_TRUE) {
                *(int *)arg = control.value;
                return TVI_CONTROL_TRUE;
            }
            return TVI_CONTROL_FALSE;
        case TVI_CONTROL_VID_SET_CONTRAST:
            control.id = V4L2_CID_CONTRAST;
            control.value = *(int *)arg;
            return set_control(priv, &control, 1);
    case TVI_CONTROL_TUN_GET_FREQ:
        frequency.tuner = 0;
        frequency.type  = V4L2_TUNER_ANALOG_TV;
        if (v4l2_ioctl(priv->video_fd, VIDIOC_G_FREQUENCY, &frequency) < 0) {
            MP_ERR(priv, "ioctl get frequency failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        *(int *)arg = frequency.frequency;
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_TUN_SET_FREQ:
#if 0
        set_mute(priv, 1);
        usleep(100000); // wait to suppress noise during switching
#endif
        frequency.tuner = 0;
        frequency.type  = V4L2_TUNER_ANALOG_TV;
        frequency.frequency = *(int *)arg;
        if (v4l2_ioctl(priv->video_fd, VIDIOC_S_FREQUENCY, &frequency) < 0) {
            MP_ERR(priv, "ioctl set frequency failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
#if 0
        usleep(100000); // wait to suppress noise during switching
        set_mute(priv, 0);
#endif
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_TUN_GET_TUNER:
        MP_VERBOSE(priv, "get tuner\n");
        if (v4l2_ioctl(priv->video_fd, VIDIOC_G_TUNER, &priv->tuner) < 0) {
            MP_ERR(priv, "ioctl get tuner failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_TUN_SET_TUNER:
        MP_VERBOSE(priv, "set tuner\n");
        if (v4l2_ioctl(priv->video_fd, VIDIOC_S_TUNER, &priv->tuner) < 0) {
            MP_ERR(priv, "ioctl set tuner failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_TUN_GET_NORM:
        *(int *)arg = priv->standard.index;
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_TUN_GET_SIGNAL:
        if (v4l2_ioctl(priv->video_fd, VIDIOC_G_TUNER, &priv->tuner) < 0) {
            MP_ERR(priv, "ioctl get tuner failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        *(int*)arg=100*(priv->tuner.signal>>8)/255;
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_TUN_SET_NORM:
        priv->standard.index = *(int *)arg;
        if (v4l2_ioctl(priv->video_fd, VIDIOC_ENUMSTD, &priv->standard) < 0) {
            MP_ERR(priv, "ioctl enum norm failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        MP_VERBOSE(priv, "set norm: %s\n", priv->standard.name);
        if (v4l2_ioctl(priv->video_fd, VIDIOC_S_STD, &priv->standard.id) < 0) {
            MP_ERR(priv, "ioctl set norm failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_SPC_GET_NORMID:
        {
            int i;
            for (i = 0;; i++) {
                struct v4l2_standard standard;
                memset(&standard, 0, sizeof(standard));
                standard.index = i;
                if (-1 == v4l2_ioctl(priv->video_fd, VIDIOC_ENUMSTD, &standard))
                    return TVI_CONTROL_FALSE;
                if (!strcasecmp(standard.name, (char *)arg)) {
                    *(int *)arg = i;
                    return TVI_CONTROL_TRUE;
                }
            }
            return TVI_CONTROL_FALSE;
        }
    case TVI_CONTROL_SPC_GET_INPUT:
        if (v4l2_ioctl(priv->video_fd, VIDIOC_G_INPUT, (int *)arg) < 0) {
            MP_ERR(priv, "ioctl get input failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_SPC_SET_INPUT:
        MP_VERBOSE(priv, "set input: %d\n", *(int *)arg);
        priv->input.index = *(int *)arg;
        if (v4l2_ioctl(priv->video_fd, VIDIOC_ENUMINPUT, &priv->input) < 0) {
            MP_ERR(priv, "ioctl enum input failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        if (v4l2_ioctl(priv->video_fd, VIDIOC_S_INPUT, (int *)arg) < 0) {
            MP_ERR(priv, "ioctl set input failed: %s\n", mp_strerror(errno));
            return TVI_CONTROL_FALSE;
        }
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_AUD_GET_FORMAT:
        init_audio(priv);
        if (!priv->audio_initialized) return TVI_CONTROL_FALSE;
        *(int *)arg = AF_FORMAT_S16;
        MP_VERBOSE(priv, "get audio format: %d\n", *(int *)arg);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_AUD_GET_SAMPLERATE:
        init_audio(priv);
        if (!priv->audio_initialized) return TVI_CONTROL_FALSE;
        *(int *)arg = priv->audio_in.samplerate;
        MP_VERBOSE(priv, "get audio samplerate: %d\n", *(int *)arg);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_AUD_GET_CHANNELS:
        init_audio(priv);
        if (!priv->audio_initialized) return TVI_CONTROL_FALSE;
        *(int *)arg = priv->audio_in.channels;
        MP_VERBOSE(priv, "get audio channels: %d\n", *(int *)arg);
        return TVI_CONTROL_TRUE;
    case TVI_CONTROL_AUD_SET_SAMPLERATE:
        init_audio(priv);
        MP_VERBOSE(priv, "set audio samplerate: %d\n", *(int *)arg);
        if (audio_in_set_samplerate(&priv->audio_in, *(int*)arg) < 0) return TVI_CONTROL_FALSE;
//        setup_audio_buffer_sizes(priv);
        return TVI_CONTROL_TRUE;
    }
    MP_VERBOSE(priv, "unknown control: %d\n", cmd);
    return TVI_CONTROL_UNKNOWN;
}


#define PRIV ((priv_t *) (tvi_handle->priv))

/* handler creator - entry point ! */
static tvi_handle_t *tvi_init_v4l2(struct mp_log *log, tv_param_t* tv_param)
{
    tvi_handle_t *tvi_handle;

    tvi_handle = tv_new_handle(sizeof(priv_t), log, &functions);
    if (!tvi_handle) {
        return NULL;
    }
    PRIV->log = log;
    PRIV->video_fd = -1;

    PRIV->video_dev = strdup(tv_param->device? tv_param->device: "/dev/video0");
    if (!PRIV->video_dev) {
        tv_free_handle(tvi_handle);
        return NULL;
    }

    if (tv_param->adevice) {
        PRIV->audio_dev = strdup(tv_param->adevice);
        if (!PRIV->audio_dev) {
            free(PRIV->video_dev);
            tv_free_handle(tvi_handle);
            return NULL;
        }
    }

    PRIV->tv_param=tv_param;
    return tvi_handle;
}

#undef PRIV


static int uninit(priv_t *priv)
{
    int i, frames, dropped = 0;

    priv->shutdown = 1;
    if(priv->video_grabber_thread)
        pthread_join(priv->video_grabber_thread, NULL);
    pthread_mutex_destroy(&priv->video_buffer_mutex);

    if (priv->streamon) {
        /* get performance */
        frames = 1 + lrintf((double)(priv->curr_frame - priv->first_frame) / 1e6 * getfps(priv));
        dropped = frames - priv->frames;

        /* turn off streaming */
        if (v4l2_ioctl(priv->video_fd, VIDIOC_STREAMOFF, &(priv->map[0].buf.type)) < 0) {
            MP_ERR(priv, "ioctl streamoff failed: %s\n", mp_strerror(errno));
        }
        priv->streamon = 0;

        /* unqueue all remaining buffers (not sure if this code is correct) */
        for (i = 0; i < priv->mapcount; i++) {
            if (v4l2_ioctl(priv->video_fd, VIDIOC_DQBUF, &priv->map[i].buf) < 0) {
                MP_ERR(priv, "VIDIOC_DQBUF failed: %s\n", mp_strerror(errno));
            }
        }
    }

    /* unmap all buffers */
    for (i = 0; i < priv->mapcount; i++) {
        if (v4l2_munmap(priv->map[i].addr, priv->map[i].len) < 0) {
            MP_ERR(priv, "munmap capture buffer failed: %s\n", mp_strerror(errno));
        }
    }

    /* stop audio thread */
    if (priv->tv_param->audio && priv->audio_grabber_thread) {
        pthread_join(priv->audio_grabber_thread, NULL);
        pthread_mutex_destroy(&priv->skew_mutex);
        pthread_mutex_destroy(&priv->audio_mutex);
    }

    set_mute(priv, 1);

    /* free memory and close device */
    free(priv->map);
    priv->map = NULL;
    priv->mapcount = 0;
    if (priv->video_fd != -1) {
        v4l2_close(priv->video_fd);
        priv->video_fd = -1;
    }
    free(priv->video_dev);
    priv->video_dev = NULL;

    if (priv->video_ringbuffer) {
        for (int n = 0; n < priv->video_buffer_size_current; n++) {
            free(priv->video_ringbuffer[n].data);
        }
        free(priv->video_ringbuffer);
    }
    if (priv->tv_param->audio) {
        free(priv->audio_ringbuffer);
        free(priv->audio_skew_buffer);
        free(priv->audio_skew_delta_buffer);

        audio_in_uninit(&priv->audio_in);
    }

    /* show some nice statistics ;-) */
    MP_INFO(priv, "%d frames successfully processed, %d frames dropped.\n",
            priv->frames, dropped);
    MP_VERBOSE(priv, "up to %u video frames buffered.\n",
               priv->video_buffer_size_current);
    return 1;
}


/* initialisation */
static int init(priv_t *priv)
{
    int i;

    priv->audio_ringbuffer = NULL;
    priv->audio_skew_buffer = NULL;
    priv->audio_skew_delta_buffer = NULL;

    priv->audio_initialized = 0;

    /* Open the video device. */
    priv->video_fd = v4l2_open(priv->video_dev, O_RDWR);
    if (priv->video_fd < 0) {
        MP_ERR(priv, "unable to open '%s': %s\n", priv->video_dev, mp_strerror(errno));
        uninit(priv);
        return 0;
    }
    MP_DBG(priv, "video fd: %s: %d\n", priv->video_dev, priv->video_fd);

    /*
    ** Query the video capabilities and current settings
    ** for further control calls.
    */
    if (v4l2_ioctl(priv->video_fd, VIDIOC_QUERYCAP, &priv->capability) < 0) {
        MP_ERR(priv, "ioctl query capabilities failed: %s\n", mp_strerror(errno));
        uninit(priv);
        return 0;
    }

    if (!(priv->capability.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        MP_ERR(priv, "Device %s is not a video capture device.\n",
               priv->video_dev);
        return 0;
    }

    if (getfmt(priv) < 0) {
        uninit(priv);
        return 0;
    }
    getstd(priv);
    /*
    ** if this device has got a tuner query it's settings
    ** otherwise set some nice defaults
    */
    if (priv->capability.capabilities & V4L2_CAP_TUNER) {
        if (v4l2_ioctl(priv->video_fd, VIDIOC_G_TUNER, &priv->tuner) < 0) {
            MP_ERR(priv, "ioctl get tuner failed: %s\n", mp_strerror(errno));
            uninit(priv);
            return 0;
        }
    }
    MP_INFO(priv, "Selected device: %s\n", priv->capability.card);
    if (priv->capability.capabilities & V4L2_CAP_TUNER) {
        MP_INFO(priv, " Tuner cap:%s%s%s\n",
                (priv->tuner.capability & V4L2_TUNER_CAP_STEREO) ? " STEREO" : "",
                (priv->tuner.capability & V4L2_TUNER_CAP_LANG1)  ? " LANG1"  : "",
                (priv->tuner.capability & V4L2_TUNER_CAP_LANG2)  ? " LANG2"  : "");
        MP_INFO(priv, " Tuner rxs:%s%s%s%s\n",
                (priv->tuner.rxsubchans & V4L2_TUNER_SUB_MONO)   ? " MONO"   : "",
                (priv->tuner.rxsubchans & V4L2_TUNER_SUB_STEREO) ? " STEREO" : "",
                (priv->tuner.rxsubchans & V4L2_TUNER_SUB_LANG1)  ? " LANG1"  : "",
                (priv->tuner.rxsubchans & V4L2_TUNER_SUB_LANG2)  ? " LANG2"  : "");
    }
    MP_INFO(priv, " Capabilities:%s%s%s%s%s%s%s%s%s%s%s\n",
           priv->capability.capabilities & V4L2_CAP_VIDEO_CAPTURE?
           "  video capture": "",
           priv->capability.capabilities & V4L2_CAP_VIDEO_OUTPUT?
           "  video output": "",
           priv->capability.capabilities & V4L2_CAP_VIDEO_OVERLAY?
           "  video overlay": "",
           priv->capability.capabilities & V4L2_CAP_VBI_CAPTURE?
           "  VBI capture device": "",
           priv->capability.capabilities & V4L2_CAP_VBI_OUTPUT?
           "  VBI output": "",
           priv->capability.capabilities & V4L2_CAP_RDS_CAPTURE?
           "  RDS data capture": "",
           priv->capability.capabilities & V4L2_CAP_TUNER?
           "  tuner": "",
           priv->capability.capabilities & V4L2_CAP_AUDIO?
           "  audio": "",
           priv->capability.capabilities & V4L2_CAP_READWRITE?
           "  read/write": "",
           priv->capability.capabilities & V4L2_CAP_ASYNCIO?
           "  async i/o": "",
           priv->capability.capabilities & V4L2_CAP_STREAMING?
           "  streaming": "");
    MP_INFO(priv, " supported norms:");
    for (i = 0;; i++) {
        struct v4l2_standard standard;
        memset(&standard, 0, sizeof(standard));
        standard.index = i;
        if (-1 == v4l2_ioctl(priv->video_fd, VIDIOC_ENUMSTD, &standard))
            break;
        MP_INFO(priv, " %d = %s;", i, standard.name);
    }
    MP_INFO(priv, "\n inputs:");
    for (i = 0; 1; i++) {
        struct v4l2_input input;

        input.index = i;
        if (v4l2_ioctl(priv->video_fd, VIDIOC_ENUMINPUT, &input) < 0) {
            break;
        }
        MP_INFO(priv, " %d = %s;", i, input.name);
    }
    i = -1;
    if (v4l2_ioctl(priv->video_fd, VIDIOC_G_INPUT, &i) < 0) {
        MP_ERR(priv, "ioctl get input failed: %s\n", mp_strerror(errno));
    }
    char buf[80];
    MP_INFO(priv, "\n Current input: %d\n", i);
    for (i = 0; ; i++) {
        struct v4l2_fmtdesc fmtdesc;

        fmtdesc.index = i;
        fmtdesc.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (v4l2_ioctl(priv->video_fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0) {
            break;
        }
        MP_VERBOSE(priv, " Format %-6s (%2d bits, %s)\n",
               pixfmt2name(buf, fmtdesc.pixelformat), pixfmt2depth(fmtdesc.pixelformat),
               fmtdesc.description);
    }
    MP_INFO(priv, " Current format: %s\n",
           pixfmt2name(buf, priv->format.fmt.pix.pixelformat));

    /* set some nice defaults */
    if (getfmt(priv) < 0) return 0;
    priv->format.fmt.pix.width  = 640;
    priv->format.fmt.pix.height = 480;
    if (v4l2_ioctl(priv->video_fd, VIDIOC_S_FMT, &priv->format) < 0) {
        MP_ERR(priv, "ioctl set format failed: %s\n", mp_strerror(errno));
        uninit(priv);
        return 0;
    }

//    if (!(priv->capability.capabilities & V4L2_CAP_AUDIO) && !priv->tv_param->force_audio) priv->tv_param->noaudio = 1;

    if (priv->capability.capabilities & V4L2_CAP_TUNER) {
        struct v4l2_control control;
        if (priv->tv_param->amode >= 0) {
            MP_VERBOSE(priv, "setting audio mode\n");
            priv->tuner.audmode = amode2v4l(priv->tv_param->amode);
            if (v4l2_ioctl(priv->video_fd, VIDIOC_S_TUNER, &priv->tuner) < 0) {
                MP_ERR(priv, "ioctl set tuner failed: %s\n", mp_strerror(errno));
                return TVI_CONTROL_FALSE;
            }
        }
        MP_INFO(priv, "current audio mode is :%s%s%s%s\n",
                (priv->tuner.audmode == V4L2_TUNER_MODE_MONO)   ? " MONO"   : "",
                (priv->tuner.audmode == V4L2_TUNER_MODE_STEREO) ? " STEREO" : "",
                (priv->tuner.audmode == V4L2_TUNER_MODE_LANG1)  ? " LANG1"  : "",
                (priv->tuner.audmode == V4L2_TUNER_MODE_LANG2)  ? " LANG2"  : "");

        if (priv->tv_param->volume >= 0) {
            control.id = V4L2_CID_AUDIO_VOLUME;
            control.value = priv->tv_param->volume;
            set_control(priv, &control, 0);
        }
        if (priv->tv_param->bass >= 0) {
            control.id = V4L2_CID_AUDIO_BASS;
            control.value = priv->tv_param->bass;
            set_control(priv, &control, 0);
        }
        if (priv->tv_param->treble >= 0) {
            control.id = V4L2_CID_AUDIO_TREBLE;
            control.value = priv->tv_param->treble;
            set_control(priv, &control, 0);
        }
        if (priv->tv_param->balance >= 0) {
            control.id = V4L2_CID_AUDIO_BALANCE;
            control.value = priv->tv_param->balance;
            set_control(priv, &control, 0);
        }
    }

    return 1;
}

static int get_capture_buffer_size(priv_t *priv)
{
    uint64_t bufsize;
    int cnt;

    if (priv->tv_param->buffer_size >= 0) {
        bufsize = priv->tv_param->buffer_size*1024*1024;
    } else {
        bufsize = 16*1024*1024;
    }

    cnt = bufsize/priv->format.fmt.pix.sizeimage;
    if (cnt < 2) cnt = 2;

    return cnt;
}

/* that's the real start, we'got the format parameters (checked with control) */
static int start(priv_t *priv)
{
    struct v4l2_requestbuffers request;
    unsigned int i;

    /* setup audio parameters */

    init_audio(priv);
    if (priv->tv_param->audio && !priv->audio_initialized) return 0;

    priv->video_buffer_size_max = get_capture_buffer_size(priv);

    if (priv->tv_param->audio) {
        setup_audio_buffer_sizes(priv);
        priv->audio_skew_buffer = calloc(priv->aud_skew_cnt, sizeof(long long));
        if (!priv->audio_skew_buffer) {
            MP_ERR(priv, "cannot allocate skew buffer: %s\n", mp_strerror(errno));
            return 0;
        }
        priv->audio_skew_delta_buffer = calloc(priv->aud_skew_cnt, sizeof(long long));
        if (!priv->audio_skew_delta_buffer) {
            MP_ERR(priv, "cannot allocate skew buffer: %s\n", mp_strerror(errno));
            return 0;
        }

        priv->audio_ringbuffer = calloc(priv->audio_in.blocksize, priv->audio_buffer_size);
        if (!priv->audio_ringbuffer) {
            MP_ERR(priv, "cannot allocate audio buffer: %s\n", mp_strerror(errno));
            return 0;
        }

        priv->audio_secs_per_block = (double)priv->audio_in.blocksize/(priv->audio_in.samplerate
                                                                    *priv->audio_in.channels
                                                                    *priv->audio_in.bytes_per_sample);
        priv->audio_usecs_per_block = 1e6*priv->audio_secs_per_block;
        priv->audio_head = 0;
        priv->audio_tail = 0;
        priv->audio_cnt = 0;
        priv->audio_drop = 0;
        priv->audio_skew = 0;
        priv->audio_skew_total = 0;
        priv->audio_skew_delta_total = 0;
        priv->audio_recv_blocks_total = 0;
        priv->audio_sent_blocks_total = 0;
        priv->audio_null_blocks_inserted = 0;
        priv->audio_insert_null_samples = 0;
        priv->dropped_frames_timeshift = 0;
        priv->dropped_frames_compensated = 0;

        pthread_mutex_init(&priv->skew_mutex, NULL);
        pthread_mutex_init(&priv->audio_mutex, NULL);
    }

    /* setup video parameters */
    if (priv->tv_param->audio) {
        if (priv->video_buffer_size_max < 3*getfps(priv)*priv->audio_secs_per_block) {
            MP_ERR(priv, "Video buffer shorter than 3 times audio frame duration.\n"
                   "You will probably experience heavy framedrops.\n");
        }
    }

    {
        int bytesperline = priv->format.fmt.pix.width*pixfmt2depth(priv->format.fmt.pix.pixelformat)/8;

        MP_VERBOSE(priv, "Using a ring buffer for maximum %d frames, %d MB total size.\n",
               priv->video_buffer_size_max,
               priv->video_buffer_size_max*priv->format.fmt.pix.height*bytesperline/(1024*1024));
    }

    priv->video_ringbuffer = calloc(priv->video_buffer_size_max, sizeof(video_buffer_entry));
    if (!priv->video_ringbuffer) {
        MP_ERR(priv, "cannot allocate video buffer: %s\n", mp_strerror(errno));
        return 0;
    }
    pthread_mutex_init(&priv->video_buffer_mutex, NULL);

    priv->video_head = 0;
    priv->video_tail = 0;
    priv->video_cnt = 0;

    /* request buffers */
    request.count = BUFFER_COUNT;

    request.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (v4l2_ioctl(priv->video_fd, VIDIOC_REQBUFS, &request) < 0) {
        MP_ERR(priv, "ioctl request buffers failed: %s\n", mp_strerror(errno));
        return 0;
    }

    /* query buffers */
    if (!(priv->map = calloc(request.count, sizeof(struct map)))) {
        MP_ERR(priv, "malloc capture buffers failed: %s\n",  mp_strerror(errno));
        return 0;
    }

    /* map and queue buffers */
    for (i = 0; i < request.count; i++) {
        priv->map[i].buf.index = i;
        priv->map[i].buf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        priv->map[i].buf.memory  = V4L2_MEMORY_MMAP;
        if (v4l2_ioctl(priv->video_fd, VIDIOC_QUERYBUF, &(priv->map[i].buf)) < 0) {
            MP_ERR(priv, "ioctl query buffer failed: %s\n", mp_strerror(errno));
            free(priv->map);
            priv->map = NULL;
            return 0;
        }
        priv->map[i].addr = v4l2_mmap (0, priv->map[i].buf.length, PROT_READ |
                                  PROT_WRITE, MAP_SHARED, priv->video_fd, priv->map[i].buf.m.offset);
        if (priv->map[i].addr == MAP_FAILED) {
            MP_ERR(priv, "mmap capture buffer failed: %s\n",  mp_strerror(errno));
            priv->map[i].len = 0;
            return 0;
        }
        priv->map[i].len = priv->map[i].buf.length;
#ifdef HAVE_CLOCK_GETTIME
        priv->clk_id = (priv->map[i].buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC)
                           ? CLOCK_MONOTONIC : CLOCK_REALTIME;
#else
        if (priv->map[i].buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC)
            MP_WARN(priv, "compiled without clock_gettime() that is needed to handle monotone video timestamps from the kernel. Expect desync.\n");
#endif
        /* count up to make sure this is correct every time */
        priv->mapcount++;

        if (v4l2_ioctl(priv->video_fd, VIDIOC_QBUF, &(priv->map[i].buf)) < 0) {
            MP_ERR(priv, "ioctl queue buffer failed: %s\n", mp_strerror(errno));
            return 0;
        }
    }

    /* start audio thread */
    priv->shutdown = 0;
    priv->audio_skew_measure_time = 0;
    priv->first_frame = 0;
    priv->audio_skew = 0;
    priv->first = 1;

    set_mute(priv, 0);

    return 1;
}

// copies a video frame
static inline void copy_frame(priv_t *priv, video_buffer_entry *dest, unsigned char *source,int len)
{
    dest->framesize=len;
    if(priv->tv_param->automute>0){
        if (v4l2_ioctl(priv->video_fd, VIDIOC_G_TUNER, &priv->tuner) >= 0) {
            if(priv->tv_param->automute<<8>priv->tuner.signal){
                fill_blank_frame(dest->data,dest->framesize,fcc_vl2mp(priv->format.fmt.pix.pixelformat));
                set_mute(priv,1);
                return;
            }
        }
        set_mute(priv,0);
    }
    memcpy(dest->data, source, len);
}

// maximum skew change, in frames
#define MAX_SKEW_DELTA 0.6
static void *video_grabber(void *data)
{
    priv_t *priv = (priv_t*)data;
    long long skew, prev_skew, xskew, interval, prev_interval, delta;
    int i;
    int framesize = priv->format.fmt.pix.sizeimage;
    fd_set rdset;
    struct timeval timeout;
    struct v4l2_buffer buf;

    xskew = 0;
    skew = 0;
    interval = 0;
    prev_interval = 0;
    prev_skew = 0;

    MP_VERBOSE(priv, "going to capture\n");
    if (v4l2_ioctl(priv->video_fd, VIDIOC_STREAMON, &(priv->format.type)) < 0) {
        MP_ERR(priv, "ioctl streamon failed: %s\n", mp_strerror(errno));
        return 0;
    }
    priv->streamon = 1;

    if (priv->tv_param->audio) {
        pthread_create(&priv->audio_grabber_thread, NULL, audio_grabber, priv);
    }

    for (priv->frames = 0; !priv->shutdown;)
    {
        int ret;

        while (priv->video_cnt == priv->video_buffer_size_max) {
            usleep(10000);
            if (priv->shutdown) {
                return NULL;
            }
        }

        FD_ZERO (&rdset);
        FD_SET (priv->video_fd, &rdset);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        i = select(priv->video_fd + 1, &rdset, NULL, NULL, &timeout);
        if (i < 0) {
            MP_ERR(priv, "select failed: %s\n", mp_strerror(errno));
            continue;
        }
        else if (i == 0) {
            MP_ERR(priv, "select timeout\n");
            continue;
        }
        else if (!FD_ISSET(priv->video_fd, &rdset)) {
            continue;
        }

        memset(&buf,0,sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = v4l2_ioctl(priv->video_fd, VIDIOC_DQBUF, &buf);

        if (ret < 0) {
            /*
              if there's no signal, the buffer might me dequeued
              so we query all the buffers to see which one we should
              put back to queue

              observed with saa7134 0.2.8
              don't know if is it a bug or (mis)feature
             */
            MP_ERR(priv, "ioctl dequeue buffer failed: %s, idx = %d\n",
                   mp_strerror(errno), buf.index);
            for (i = 0; i < priv->mapcount; i++) {
                memset(&buf,0,sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;
                ret = v4l2_ioctl(priv->video_fd, VIDIOC_QUERYBUF, &buf);
                if (ret < 0) {
                    MP_ERR(priv, "ioctl query buffer failed: %s, idx = %d\n",
                           mp_strerror(errno), buf.index);
                    return 0;
                }
                if ((buf.flags & (V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_DONE)) == V4L2_BUF_FLAG_MAPPED) {
                    if (v4l2_ioctl(priv->video_fd, VIDIOC_QBUF, &(priv->map[i].buf)) < 0) {
                        MP_ERR(priv, "ioctl queue buffer failed: %s\n",
                               mp_strerror(errno));
                        return 0;
                    }
                }
            }
            continue;
        }

        /* store the timestamp of the very first frame as reference */
        if (!priv->frames++) {
            if (priv->tv_param->audio) pthread_mutex_lock(&priv->skew_mutex);
            priv->first_frame = buf.timestamp.tv_sec * 1000000LL + buf.timestamp.tv_usec;
            if (priv->tv_param->audio) pthread_mutex_unlock(&priv->skew_mutex);
        }
        priv->curr_frame = buf.timestamp.tv_sec * 1000000LL + buf.timestamp.tv_usec;
//        fprintf(stderr, "idx = %d, ts = %f\n", buf.index, (double)(priv->curr_frame) / 1e6);

        interval = priv->curr_frame - priv->first_frame;
        delta = interval - prev_interval;

        // interpolate the skew in time
        if (priv->tv_param->audio) pthread_mutex_lock(&priv->skew_mutex);
        xskew = priv->audio_skew + (interval - priv->audio_skew_measure_time)*priv->audio_skew_factor;
        if (priv->tv_param->audio) pthread_mutex_unlock(&priv->skew_mutex);
            // correct extreme skew changes to avoid (especially) moving backwards in time
        if (xskew - prev_skew > delta*MAX_SKEW_DELTA) {
            skew = prev_skew + delta*MAX_SKEW_DELTA;
        } else if (xskew - prev_skew < -delta*MAX_SKEW_DELTA) {
            skew = prev_skew - delta*MAX_SKEW_DELTA;
        } else {
            skew = xskew;
        }

        MP_TRACE(priv, "\nfps = %f, interval = %f, a_skew = %f, corr_skew = %f\n",
               delta ? (double)1e6/delta : -1,
               (double)1e-6*interval, (double)1e-6*xskew, (double)1e-6*skew);
        MP_TRACE(priv, "vcnt = %d, acnt = %d\n", priv->video_cnt, priv->audio_cnt);

        prev_skew = skew;
        prev_interval = interval;

        /* allocate a new buffer, if needed */
        pthread_mutex_lock(&priv->video_buffer_mutex);
        if (priv->video_buffer_size_current < priv->video_buffer_size_max) {
            if (priv->video_cnt == priv->video_buffer_size_current) {
                unsigned char *newbuf = malloc(framesize);
                if (newbuf) {
                    memmove(priv->video_ringbuffer+priv->video_tail+1, priv->video_ringbuffer+priv->video_tail,
                            (priv->video_buffer_size_current-priv->video_tail)*sizeof(video_buffer_entry));
                    priv->video_ringbuffer[priv->video_tail].data = newbuf;
                    if ((priv->video_head >= priv->video_tail) && (priv->video_cnt > 0)) priv->video_head++;
                    priv->video_buffer_size_current++;
                }
            }
        }
        pthread_mutex_unlock(&priv->video_buffer_mutex);

        if (priv->video_cnt == priv->video_buffer_size_current) {
            MP_ERR(priv, "\nvideo buffer full - dropping frame\n");
            if (!priv->immediate_mode || priv->audio_insert_null_samples) {
                pthread_mutex_lock(&priv->audio_mutex);
                priv->dropped_frames_timeshift += delta;
                pthread_mutex_unlock(&priv->audio_mutex);
            }
        } else {
            if (priv->immediate_mode) {
                priv->video_ringbuffer[priv->video_tail].timestamp = interval - skew;
            } else {
                // compensate for audio skew
                // negative skew => there are more audio samples, increase interval
                // positive skew => less samples, shorten the interval
                priv->video_ringbuffer[priv->video_tail].timestamp = interval - skew;
                if (priv->audio_insert_null_samples && priv->video_ringbuffer[priv->video_tail].timestamp > 0) {
                    pthread_mutex_lock(&priv->audio_mutex);
                    priv->video_ringbuffer[priv->video_tail].timestamp +=
                        (priv->audio_null_blocks_inserted
                         - priv->dropped_frames_timeshift/priv->audio_usecs_per_block)
                        *priv->audio_usecs_per_block;
                    pthread_mutex_unlock(&priv->audio_mutex);
                }
            }
            copy_frame(priv, priv->video_ringbuffer+priv->video_tail, priv->map[buf.index].addr,buf.bytesused);
            priv->video_tail = (priv->video_tail+1)%priv->video_buffer_size_current;
            priv->video_cnt++;
        }
        if (v4l2_ioctl(priv->video_fd, VIDIOC_QBUF, &buf) < 0) {
            MP_ERR(priv, "ioctl queue buffer failed: %s\n", mp_strerror(errno));
            return 0;
        }
    }
    return NULL;
}

#define MAX_LOOP 500
static double grab_video_frame(priv_t *priv, char *buffer, int len)
{
    int loop_cnt = 0;

    if (priv->first) {
        pthread_create(&priv->video_grabber_thread, NULL, video_grabber, priv);
        priv->first = 0;
    }

    while (priv->video_cnt == 0) {
        usleep(1000);
        if (loop_cnt++ > MAX_LOOP) return 0;
    }

    pthread_mutex_lock(&priv->video_buffer_mutex);
    long long interval = priv->video_ringbuffer[priv->video_head].timestamp;
    memcpy(buffer, priv->video_ringbuffer[priv->video_head].data, len);
    priv->video_cnt--;
    priv->video_head = (priv->video_head+1)%priv->video_buffer_size_current;
    pthread_mutex_unlock(&priv->video_buffer_mutex);

    return interval == -1 ? MP_NOPTS_VALUE : interval*1e-6;
}

static int get_video_framesize(priv_t *priv)
{
    /*
      this routine will be called before grab_video_frame
      thus let's return topmost frame's size
    */
    if (priv->video_cnt)
        return priv->video_ringbuffer[priv->video_head].framesize;
    /*
      no video frames yet available. i don't know what to do in this case,
      thus let's return some fallback result (for compressed format this will be
      maximum allowed frame size.
    */
    return priv->format.fmt.pix.sizeimage;
}

static void *audio_grabber(void *data)
{
    priv_t *priv = (priv_t*)data;
    int i, audio_skew_ptr = 0;
    long long current_time, prev_skew = 0, prev_skew_uncorr = 0;
    long long start_time_avg, curr_timestamp;

    start_time_avg = priv->audio_start_time = get_curr_timestamp(priv->clk_id);
    audio_in_start_capture(&priv->audio_in);
    for (i = 0; i < priv->aud_skew_cnt; i++)
        priv->audio_skew_buffer[i] = 0;
    for (i = 0; i < priv->aud_skew_cnt; i++)
        priv->audio_skew_delta_buffer[i] = 0;

    for (; !priv->shutdown;)
    {
        if (audio_in_read_chunk(&priv->audio_in, priv->audio_ringbuffer+priv->audio_tail*priv->audio_in.blocksize) < 0)
            continue;
        pthread_mutex_lock(&priv->skew_mutex);
        if (priv->first_frame == 0) {
            // there is no first frame yet (unlikely to happen)
            start_time_avg = priv->audio_start_time = get_curr_timestamp(priv->clk_id);
//            fprintf(stderr, "warning - first frame not yet available!\n");
            pthread_mutex_unlock(&priv->skew_mutex);
            continue;
        }
        pthread_mutex_unlock(&priv->skew_mutex);

        priv->audio_recv_blocks_total++;
        curr_timestamp = get_curr_timestamp(priv->clk_id);
        current_time = curr_timestamp - priv->audio_start_time;

        if (priv->audio_recv_blocks_total < priv->aud_skew_cnt*2) {
            start_time_avg += curr_timestamp - priv->audio_usecs_per_block*priv->audio_recv_blocks_total;
            priv->audio_start_time = start_time_avg/(priv->audio_recv_blocks_total+1);
        }

//        fprintf(stderr, "spb = %f, bs = %d, skew = %f\n", priv->audio_secs_per_block, priv->audio_in.blocksize,
//                (double)(current_time - 1e6*priv->audio_secs_per_block*priv->audio_recv_blocks_total)/1e6);

        // put the current skew into the ring buffer
        priv->audio_skew_total -= priv->audio_skew_buffer[audio_skew_ptr];
        priv->audio_skew_buffer[audio_skew_ptr] = current_time
            - priv->audio_usecs_per_block*priv->audio_recv_blocks_total;
        priv->audio_skew_total += priv->audio_skew_buffer[audio_skew_ptr];

        pthread_mutex_lock(&priv->skew_mutex);

        // skew calculation

        // compute the sliding average of the skews
        if (priv->audio_recv_blocks_total > priv->aud_skew_cnt) {
            priv->audio_skew = priv->audio_skew_total/priv->aud_skew_cnt;
        } else {
            priv->audio_skew = priv->audio_skew_total/priv->audio_recv_blocks_total;
        }

        // put the current skew change (skew-prev_skew) into the ring buffer
        priv->audio_skew_delta_total -= priv->audio_skew_delta_buffer[audio_skew_ptr];
        priv->audio_skew_delta_buffer[audio_skew_ptr] = priv->audio_skew - prev_skew_uncorr;
        priv->audio_skew_delta_total += priv->audio_skew_delta_buffer[audio_skew_ptr];
        prev_skew_uncorr = priv->audio_skew; // remember the _uncorrected_ average value

        audio_skew_ptr = (audio_skew_ptr+1) % priv->aud_skew_cnt; // rotate the buffer pointer

        // sliding average approximates the value in the middle of the interval
        // so interpolate the skew value further to the current time
        priv->audio_skew += priv->audio_skew_delta_total/2;

        // now finally, priv->audio_skew contains fairly good approximation
        // of the current value

        // current skew factor (assuming linearity)
        // used for further interpolation in video_grabber
        // probably overkill but seems to be necessary for
        // stress testing by dropping half of the audio frames ;)
        // especially when using ALSA with large block sizes
        // where audio_skew remains a long while behind
        if ((priv->audio_skew_measure_time != 0) && (current_time - priv->audio_skew_measure_time != 0)) {
            priv->audio_skew_factor = (double)(priv->audio_skew-prev_skew)/(current_time - priv->audio_skew_measure_time);
        } else {
            priv->audio_skew_factor = 0.0;
        }

        priv->audio_skew_measure_time = current_time;
        prev_skew = priv->audio_skew;
        priv->audio_skew += priv->audio_start_time - priv->first_frame;
        pthread_mutex_unlock(&priv->skew_mutex);

//        fprintf(stderr, "audio_skew = %f, delta = %f\n", (double)priv->audio_skew/1e6, (double)priv->audio_skew_delta_total/1e6);

        pthread_mutex_lock(&priv->audio_mutex);
        if ((priv->audio_tail+1) % priv->audio_buffer_size == priv->audio_head) {
            MP_ERR(priv, "\ntoo bad - dropping audio frame !\n");
            priv->audio_drop++;
        } else {
            priv->audio_tail = (priv->audio_tail+1) % priv->audio_buffer_size;
            priv->audio_cnt++;
        }
        pthread_mutex_unlock(&priv->audio_mutex);
    }
    return NULL;
}

static double grab_audio_frame(priv_t *priv, char *buffer, int len)
{
    MP_DBG(priv, "grab_audio_frame(priv=%p, buffer=%p, len=%d)\n",
        priv, buffer, len);

    // hack: if grab_audio_frame is called first, it means we are used by mplayer
    // => switch to the mode which outputs audio immediately, even if
    // it should be silence
    if (priv->first) priv->audio_insert_null_samples = 1;

    pthread_mutex_lock(&priv->audio_mutex);
    while (priv->audio_insert_null_samples
           && priv->dropped_frames_timeshift - priv->dropped_frames_compensated >= priv->audio_usecs_per_block) {
        // some frames were dropped - drop the corresponding number of audio blocks
        if (priv->audio_drop) {
            priv->audio_drop--;
        } else {
            if (priv->audio_head == priv->audio_tail) break;
            priv->audio_head = (priv->audio_head+1) % priv->audio_buffer_size;
        }
        priv->dropped_frames_compensated += priv->audio_usecs_per_block;
    }

    // compensate for dropped audio frames
    if (priv->audio_drop && (priv->audio_head == priv->audio_tail)) {
        priv->audio_drop--;
        memset(buffer, 0, len);
        goto out;
    }

    if (priv->audio_insert_null_samples && (priv->audio_head == priv->audio_tail)) {
        // return silence to avoid desync and stuttering
        memset(buffer, 0, len);
        priv->audio_null_blocks_inserted++;
        goto out;
    }

    pthread_mutex_unlock(&priv->audio_mutex);
    while (priv->audio_head == priv->audio_tail) {
        // this is mencoder => just wait until some audio is available
        usleep(10000);
    }
    pthread_mutex_lock(&priv->audio_mutex);
    memcpy(buffer, priv->audio_ringbuffer+priv->audio_head*priv->audio_in.blocksize, len);
    priv->audio_head = (priv->audio_head+1) % priv->audio_buffer_size;
    priv->audio_cnt--;
out:
    pthread_mutex_unlock(&priv->audio_mutex);
    priv->audio_sent_blocks_total++;
    return (double)priv->audio_sent_blocks_total*priv->audio_secs_per_block;
}

static int get_audio_framesize(priv_t *priv)
{
    return priv->audio_in.blocksize;
}
