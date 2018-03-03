/*
 * TV interface
 *
 * Copyright (C) 2001 Alex Beregszászi
 * Copyright (C) 2007 Attila Ötvös
 * Copyright (C) 2007 Vladimir Voroshilov <voroshil@gmail.com>
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

#ifndef MPLAYER_TV_H
#define MPLAYER_TV_H

#include "osdep/endian.h"

#include "config.h"
#if !HAVE_GPL
#error GPL only
#endif

struct mp_log;

typedef struct tv_params {
    float freq;
    char *channel;
    char *chanlist;
    char *norm;
    int automute;
    int normid;
    char *device;
    char *driver;
    int width;
    int height;
    int input;
    int outfmt;
    float fps;
    char **channels;
    int audio;
    int immediate;
    int audiorate;
    int audio_id;
    int amode;
    int volume;
    int bass;
    int treble;
    int balance;
    int forcechan;
    int force_audio;
    int buffer_size;
    int mjpeg;
    int decimation;
    int quality;
    int alsa;
    char* adevice;
    int brightness;
    int contrast;
    int hue;
    int saturation;
    int gain;

    int scan;
    int scan_threshold;
    float scan_period;
} tv_param_t;

struct tv_stream_params {
    char *channel;
    int input;
};

typedef struct tvi_info_s
{
    struct tvi_handle_s * (*tvi_init)(struct mp_log *log, tv_param_t* tv_param);
    const char *name;
    const char *short_name;
} tvi_info_t;


struct priv;

typedef struct tvi_functions_s
{
    int (*init)(struct priv *priv);
    int (*uninit)(struct priv *priv);
    int (*control)(struct priv *priv, int cmd, void *arg);
    int (*start)(struct priv *priv);
    double (*grab_video_frame)(struct priv *priv, char *buffer, int len);
    int (*get_video_framesize)(struct priv *priv);
    double (*grab_audio_frame)(struct priv *priv, char *buffer, int len);
    int (*get_audio_framesize)(struct priv *priv);
} tvi_functions_t;

typedef struct tvi_handle_s {
    struct mp_log       *log;
    const tvi_functions_t       *functions;
    void                *priv;
    int                 seq;
    struct demuxer      *demuxer;

    /* specific */
    int                 norm;
    int                 chanlist;
    const struct CHANLIST *chanlist_s;
    int                 channel;
    tv_param_t          * tv_param;
    void                * scan;

    struct tv_channels_s *tv_channel_list;
    struct tv_channels_s *tv_channel_current, *tv_channel_last;
    char *tv_channel_last_real;
} tvi_handle_t;

typedef struct tv_channels_s {
    int index;
    char number[5];
    char name[20];
    int norm;
    int   freq;
    struct tv_channels_s *next;
    struct tv_channels_s *prev;
} tv_channels_t;

typedef struct {
    unsigned int     scan_timer;
    int     channel_num;
    int     new_channels;
} tv_scan_t;

#define TVI_CONTROL_FALSE               0
#define TVI_CONTROL_TRUE                1
#define TVI_CONTROL_NA                  -1
#define TVI_CONTROL_UNKNOWN             -2

/* ======================== CONTROLS =========================== */

/* GENERIC controls */
#define TVI_CONTROL_IS_AUDIO            0x1
#define TVI_CONTROL_IS_VIDEO            0x2
#define TVI_CONTROL_IS_TUNER            0x3
#define TVI_CONTROL_IMMEDIATE           0x4

/* VIDEO controls */
#define TVI_CONTROL_VID_GET_FPS         0x101
#define TVI_CONTROL_VID_GET_PLANES      0x102
#define TVI_CONTROL_VID_GET_BITS        0x103
#define TVI_CONTROL_VID_CHK_BITS        0x104
#define TVI_CONTROL_VID_SET_BITS        0x105
#define TVI_CONTROL_VID_GET_FORMAT      0x106
#define TVI_CONTROL_VID_CHK_FORMAT      0x107
#define TVI_CONTROL_VID_SET_FORMAT      0x108
#define TVI_CONTROL_VID_GET_WIDTH       0x109
#define TVI_CONTROL_VID_CHK_WIDTH       0x110
#define TVI_CONTROL_VID_SET_WIDTH       0x111
#define TVI_CONTROL_VID_GET_HEIGHT      0x112
#define TVI_CONTROL_VID_CHK_HEIGHT      0x113
#define TVI_CONTROL_VID_SET_HEIGHT      0x114
#define TVI_CONTROL_VID_GET_BRIGHTNESS  0x115
#define TVI_CONTROL_VID_SET_BRIGHTNESS  0x116
#define TVI_CONTROL_VID_GET_HUE         0x117
#define TVI_CONTROL_VID_SET_HUE         0x118
#define TVI_CONTROL_VID_GET_SATURATION  0x119
#define TVI_CONTROL_VID_SET_SATURATION  0x11a
#define TVI_CONTROL_VID_GET_CONTRAST    0x11b
#define TVI_CONTROL_VID_SET_CONTRAST    0x11c
#define TVI_CONTROL_VID_GET_PICTURE     0x11d
#define TVI_CONTROL_VID_SET_PICTURE     0x11e
#define TVI_CONTROL_VID_SET_GAIN        0x11f
#define TVI_CONTROL_VID_GET_GAIN        0x120
#define TVI_CONTROL_VID_SET_WIDTH_HEIGHT        0x121

/* TUNER controls */
#define TVI_CONTROL_TUN_GET_FREQ        0x201
#define TVI_CONTROL_TUN_SET_FREQ        0x202
#define TVI_CONTROL_TUN_GET_TUNER       0x203   /* update priv->tuner struct for used input */
#define TVI_CONTROL_TUN_SET_TUNER       0x204   /* update priv->tuner struct for used input */
#define TVI_CONTROL_TUN_GET_NORM        0x205
#define TVI_CONTROL_TUN_SET_NORM        0x206
#define TVI_CONTROL_TUN_GET_SIGNAL      0x207

/* AUDIO controls */
#define TVI_CONTROL_AUD_GET_FORMAT      0x301
#define TVI_CONTROL_AUD_GET_SAMPLERATE  0x302
#define TVI_CONTROL_AUD_GET_CHANNELS    0x304
#define TVI_CONTROL_AUD_SET_SAMPLERATE  0x305

/* SPECIFIC controls */
#define TVI_CONTROL_SPC_GET_INPUT       0x401   /* set input channel (tv,s-video,composite..) */
#define TVI_CONTROL_SPC_SET_INPUT       0x402   /* set input channel (tv,s-video,composite..) */
#define TVI_CONTROL_SPC_GET_NORMID      0x403   /* get normid from norm name */

int tv_set_color_options(tvi_handle_t *tvh, int opt, int val);
int tv_get_color_options(tvi_handle_t *tvh, int opt, int* val);

int tv_step_channel_real(tvi_handle_t *tvh, int direction);
int tv_step_channel(tvi_handle_t *tvh, int direction);
#define TV_CHANNEL_LOWER        1
#define TV_CHANNEL_HIGHER       2

int tv_last_channel(tvi_handle_t *tvh);

int tv_set_channel_real(tvi_handle_t *tvh, char *channel);
int tv_set_channel(tvi_handle_t *tvh, char *channel);

int tv_step_norm(tvi_handle_t *tvh);
int tv_step_chanlist(tvi_handle_t *tvh);

int tv_set_freq(tvi_handle_t *tvh, unsigned long freq);
int tv_get_freq(tvi_handle_t *tvh, unsigned long *freq);
int tv_get_signal(tvi_handle_t *tvh);
int tv_step_freq(tvi_handle_t *tvh, float step_interval);

int tv_set_norm(tvi_handle_t *tvh, char* norm);

void tv_start_scan(tvi_handle_t *tvh, int start);

tvi_handle_t *tv_new_handle(int size, struct mp_log *log, const tvi_functions_t *functions);
void tv_free_handle(tvi_handle_t *h);

#define TV_NORM_PAL             1
#define TV_NORM_NTSC            2
#define TV_NORM_SECAM           3
#define TV_NORM_PALNC           4
#define TV_NORM_PALM            5
#define TV_NORM_PALN            6
#define TV_NORM_NTSCJP          7

int tv_uninit(tvi_handle_t *tvh);
void tv_scan(tvi_handle_t *tvh);
int open_tv(tvi_handle_t *tvh);
tvi_handle_t *tv_begin(tv_param_t* tv_param, struct mp_log *log);
int tv_stream_control(tvi_handle_t *tvh, int cmd, void *arg);

extern const struct m_sub_options tv_params_conf;

#define MP_FOURCC(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((unsigned)(d)<<24))

#if BYTE_ORDER == BIG_ENDIAN
#define MP_FOURCC_E(a,b,c,d) MP_FOURCC(a,b,c,d)
#else
#define MP_FOURCC_E(a,b,c,d) MP_FOURCC(d,c,b,a)
#endif

#define MP_FOURCC_RGB8  MP_FOURCC_E(8,   'B', 'G', 'R')
#define MP_FOURCC_RGB12 MP_FOURCC_E(12,  'B', 'G', 'R')
#define MP_FOURCC_RGB15 MP_FOURCC_E(15,  'B', 'G', 'R')
#define MP_FOURCC_RGB16 MP_FOURCC_E(16,  'B', 'G', 'R')
#define MP_FOURCC_RGB24 MP_FOURCC_E(24,  'B', 'G', 'R')
#define MP_FOURCC_RGB32 MP_FOURCC_E('A', 'B', 'G', 'R')

#define MP_FOURCC_BGR8  MP_FOURCC_E(8,   'R', 'G', 'B')
#define MP_FOURCC_BGR12 MP_FOURCC_E(12,  'R', 'G', 'B')
#define MP_FOURCC_BGR15 MP_FOURCC_E(15,  'R', 'G', 'B')
#define MP_FOURCC_BGR16 MP_FOURCC_E(16,  'R', 'G', 'B')
#define MP_FOURCC_BGR24 MP_FOURCC_E(24,  'R', 'G', 'B')
#define MP_FOURCC_BGR32 MP_FOURCC_E('A', 'R', 'G', 'B')

#define MP_FOURCC_YVU9  MP_FOURCC('Y', 'U', 'V', '9')
#define MP_FOURCC_YUV9  MP_FOURCC('Y', 'V', 'U', '9')
#define MP_FOURCC_YV12  MP_FOURCC('Y', 'V', '1', '2')
#define MP_FOURCC_I420  MP_FOURCC('I', '4', '2', '0')
#define MP_FOURCC_IYUV  MP_FOURCC('I', 'Y', 'U', 'V')
#define MP_FOURCC_Y800  MP_FOURCC('Y', '8', '0', '0')
#define MP_FOURCC_NV12  MP_FOURCC('N', 'V', '1', '2')
#define MP_FOURCC_NV21  MP_FOURCC('N', 'V', '2', '1')

#define MP_FOURCC_UYVY  MP_FOURCC('U', 'Y', 'V', 'Y')
#define MP_FOURCC_YUY2  MP_FOURCC('Y', 'U', 'Y', '2')

#define MP_FOURCC_MJPEG MP_FOURCC('M', 'J', 'P', 'G')
#define MP_FOURCC_JPEG  MP_FOURCC('J', 'P', 'E', 'G')

#endif /* MPLAYER_TV_H */
