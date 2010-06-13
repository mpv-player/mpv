/*
 * TV interface
 *
 * Copyright (C) 2001 Alex Beregszászi
 * Copyright (C) 2007 Attila Ötvös
 * Copyright (C) 2007 Vladimir Voroshilov <voroshil@gmail.com>
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

#ifndef MPLAYER_TV_H
#define MPLAYER_TV_H

#include "libmpcodecs/dec_teletext.h"
#include "libmpdemux/demuxer.h"

typedef struct tv_param_s {
    char *freq;
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
    int noaudio;
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
    struct tt_param teletext;

    int scan;
    int scan_threshold;
    float scan_period;
    /**
      Terminate stream with video renderer instead of Null renderer
      Will help if video freezes but audio does not.
      May not work with -vo directx and -vf crop combination.
    */
    int hidden_video_renderer;
    /**
      For VIVO cards VP pin have to be rendered too.
      This tweak will cause VidePort pin stream to be terminated with video renderer
      instead of removing it from graph.
      Use if your card have vp pin and video is still choppy.
      May not work with -vo directx and -vf crop combination.
    */
    int hidden_vp_renderer;
    /**
      Use system clock as sync source instead of default graph clock (usually the clock
      from one of live sources in graph.
    */
    int system_clock;
    /**
      Some audio cards creates audio chunks with about 0.5 sec size.
      This can cause choppy video when using mplayer with immediatemode=0
      Use followingtweak to decrease audio chunk sizes.
      It will create audio chunks with time length equal to one video frame time.
    */
    int normalize_audio_chunks;
} tv_param_t;

extern tv_param_t stream_tv_defaults;

typedef struct tvi_info_s
{
    struct tvi_handle_s * (*tvi_init)(tv_param_t* tv_param);
    const char *name;
    const char *short_name;
    const char *author;
    const char *comment;
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
    const tvi_functions_t	*functions;
    void		*priv;
    int 		seq;
    demuxer_t		*demuxer;

    /* specific */
    int			norm;
    int			chanlist;
    const struct CHANLIST *chanlist_s;
    int			channel;
    tv_param_t          * tv_param;
    void                * scan;
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

extern tv_channels_t *tv_channel_list;
extern tv_channels_t *tv_channel_current, *tv_channel_last;
extern char *tv_channel_last_real;

typedef struct {
    unsigned int     scan_timer;
    int     channel_num;
    int     new_channels;
} tv_scan_t;

#define TVI_CONTROL_FALSE		0
#define TVI_CONTROL_TRUE		1
#define TVI_CONTROL_NA			-1
#define TVI_CONTROL_UNKNOWN		-2

/* ======================== CONTROLS =========================== */

/* GENERIC controls */
#define TVI_CONTROL_IS_AUDIO		0x1
#define TVI_CONTROL_IS_VIDEO		0x2
#define TVI_CONTROL_IS_TUNER		0x3
#define TVI_CONTROL_IMMEDIATE           0x4

/* VIDEO controls */
#define TVI_CONTROL_VID_GET_FPS		0x101
#define TVI_CONTROL_VID_GET_PLANES	0x102
#define TVI_CONTROL_VID_GET_BITS	0x103
#define TVI_CONTROL_VID_CHK_BITS	0x104
#define TVI_CONTROL_VID_SET_BITS	0x105
#define TVI_CONTROL_VID_GET_FORMAT	0x106
#define TVI_CONTROL_VID_CHK_FORMAT	0x107
#define TVI_CONTROL_VID_SET_FORMAT	0x108
#define TVI_CONTROL_VID_GET_WIDTH	0x109
#define TVI_CONTROL_VID_CHK_WIDTH	0x110
#define TVI_CONTROL_VID_SET_WIDTH	0x111
#define TVI_CONTROL_VID_GET_HEIGHT	0x112
#define TVI_CONTROL_VID_CHK_HEIGHT	0x113
#define TVI_CONTROL_VID_SET_HEIGHT	0x114
#define TVI_CONTROL_VID_GET_BRIGHTNESS	0x115
#define TVI_CONTROL_VID_SET_BRIGHTNESS	0x116
#define TVI_CONTROL_VID_GET_HUE		0x117
#define TVI_CONTROL_VID_SET_HUE		0x118
#define TVI_CONTROL_VID_GET_SATURATION	0x119
#define TVI_CONTROL_VID_SET_SATURATION	0x11a
#define TVI_CONTROL_VID_GET_CONTRAST	0x11b
#define TVI_CONTROL_VID_SET_CONTRAST	0x11c
#define TVI_CONTROL_VID_GET_PICTURE	0x11d
#define TVI_CONTROL_VID_SET_PICTURE	0x11e
#define TVI_CONTROL_VID_SET_GAIN	0x11f
#define TVI_CONTROL_VID_GET_GAIN	0x120
#define TVI_CONTROL_VID_SET_WIDTH_HEIGHT	0x121

/* TUNER controls */
#define TVI_CONTROL_TUN_GET_FREQ	0x201
#define TVI_CONTROL_TUN_SET_FREQ	0x202
#define TVI_CONTROL_TUN_GET_TUNER	0x203	/* update priv->tuner struct for used input */
#define TVI_CONTROL_TUN_SET_TUNER	0x204	/* update priv->tuner struct for used input */
#define TVI_CONTROL_TUN_GET_NORM	0x205
#define TVI_CONTROL_TUN_SET_NORM	0x206
#define TVI_CONTROL_TUN_GET_SIGNAL	0x207

/* AUDIO controls */
#define TVI_CONTROL_AUD_GET_FORMAT	0x301
#define TVI_CONTROL_AUD_GET_SAMPLERATE	0x302
#define TVI_CONTROL_AUD_GET_SAMPLESIZE	0x303
#define TVI_CONTROL_AUD_GET_CHANNELS	0x304
#define TVI_CONTROL_AUD_SET_SAMPLERATE	0x305

/* SPECIFIC controls */
#define TVI_CONTROL_SPC_GET_INPUT	0x401	/* set input channel (tv,s-video,composite..) */
#define TVI_CONTROL_SPC_SET_INPUT	0x402	/* set input channel (tv,s-video,composite..) */
#define TVI_CONTROL_SPC_GET_NORMID	0x403	/* get normid from norm name */

//tvi_* ioctl (not dec_teletext.c !!!)
#define TVI_CONTROL_VBI_INIT           0x501   ///< vbi init
#define TVI_CONTROL_GET_VBI_PTR        0x502   ///< get teletext private pointer

int tv_set_color_options(tvi_handle_t *tvh, int opt, int val);
int tv_get_color_options(tvi_handle_t *tvh, int opt, int* val);
#define TV_COLOR_BRIGHTNESS	1
#define TV_COLOR_HUE		2
#define TV_COLOR_SATURATION	3
#define TV_COLOR_CONTRAST	4

int tv_step_channel_real(tvi_handle_t *tvh, int direction);
int tv_step_channel(tvi_handle_t *tvh, int direction);
#define TV_CHANNEL_LOWER	1
#define TV_CHANNEL_HIGHER	2

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

#define TV_NORM_PAL		1
#define TV_NORM_NTSC		2
#define TV_NORM_SECAM		3
#define TV_NORM_PALNC		4
#define TV_NORM_PALM		5
#define TV_NORM_PALN		6
#define TV_NORM_NTSCJP		7

#endif /* MPLAYER_TV_H */
