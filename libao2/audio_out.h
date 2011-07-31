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

#ifndef MPLAYER_AUDIO_OUT_H
#define MPLAYER_AUDIO_OUT_H

#include <stdbool.h>

#include "bstr.h"

typedef struct ao_info {
    /* driver name ("Matrox Millennium G200/G400" */
    const char *name;
    /* short name (for config strings) ("mga") */
    const char *short_name;
    /* author ("Aaron Holtzman <aholtzma@ess.engr.uvic.ca>") */
    const char *author;
    /* any additional comments */
    const char *comment;
} ao_info_t;

/* interface towards mplayer and */
typedef struct ao_old_functions {
    int (*control)(int cmd, void *arg);
    int (*init)(int rate, int channels, int format, int flags);
    void (*uninit)(int immed);
    void (*reset)(void);
    int (*get_space)(void);
    int (*play)(void *data, int len, int flags);
    float (*get_delay)(void);
    void (*pause)(void);
    void (*resume)(void);
} ao_functions_t;

struct ao;

struct ao_driver {
    bool is_new;
    const struct ao_info *info;
    const struct ao_old_functions *old_functions;
    int (*control)(struct ao *ao, int cmd, void *arg);
    int (*init)(struct ao *ao, char *params);
    void (*uninit)(struct ao *ao, bool cut_audio);
    void (*reset)(struct ao*ao);
    int (*get_space)(struct ao *ao);
    int (*play)(struct ao *ao, void *data, int len, int flags);
    float (*get_delay)(struct ao *ao);
    void (*pause)(struct ao *ao);
    void (*resume)(struct ao *ao);
};

/* global data used by mplayer and plugins */
struct ao {
    int samplerate;
    int channels;
    int format;
    int bps;
    int outburst;
    int buffersize;
    int brokenpts;
    double pts;
    struct bstr buffer;
    int buffer_playable_size;
    bool initialized;
    bool untimed;
    const struct ao_driver *driver;
    void *priv;
};

extern char *ao_subdevice;

void list_audio_out(void);

#define CONTROL_OK 1
#define CONTROL_TRUE 1
#define CONTROL_FALSE 0
#define CONTROL_UNKNOWN -1
#define CONTROL_ERROR -2
#define CONTROL_NA -3

#define AOCONTROL_SET_DEVICE 1
#define AOCONTROL_GET_DEVICE 2
#define AOCONTROL_QUERY_FORMAT 3 /* test for availabilty of a format */
#define AOCONTROL_GET_VOLUME 4
#define AOCONTROL_SET_VOLUME 5
#define AOCONTROL_SET_PLUGIN_DRIVER 6
#define AOCONTROL_SET_PLUGIN_LIST 7

#define AOPLAY_FINAL_CHUNK 1

typedef struct ao_control_vol {
    float left;
    float right;
} ao_control_vol_t;

struct ao *ao_create(void);
void ao_init(struct ao *ao, char **ao_list);
void ao_uninit(struct ao *ao, bool cut_audio);
int ao_play(struct ao *ao, void *data, int len, int flags);
int ao_control(struct ao *ao, int cmd, void *arg);
double ao_get_delay(struct ao *ao);
int ao_get_space(struct ao *ao);
void ao_reset(struct ao *ao);
void ao_pause(struct ao *ao);
void ao_resume(struct ao *ao);

int old_ao_control(struct ao *ao, int cmd, void *arg);
int old_ao_init(struct ao *ao, char *params);
void old_ao_uninit(struct ao *ao, bool cut_audio);
void old_ao_reset(struct ao*ao);
int old_ao_get_space(struct ao *ao);
int old_ao_play(struct ao *ao, void *data, int len, int flags);
float old_ao_get_delay(struct ao *ao);
void old_ao_pause(struct ao *ao);
void old_ao_resume(struct ao *ao);

#endif /* MPLAYER_AUDIO_OUT_H */
