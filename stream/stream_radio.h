#ifndef MPLAYER_STREAM_RADIO_H
#define MPLAYER_STREAM_RADIO_H

#include "stream.h"

#define RADIO_CHANNEL_LOWER 1
#define RADIO_CHANNEL_HIGHER 2

typedef struct radio_param_s{
    /** name of radio device file */
    char*   device;
#ifdef CONFIG_RADIO_BSDBT848
    /** minimal allowed frequency */
    float   freq_min;
    /** maximal allowed frequency */
    float   freq_max;
#endif
    /** radio driver (v4l,v4l2) */
    char*   driver;
    /** channels list (see man page) */
    char**  channels;
    /** initial volume for radio device */
    int     volume;
    /** name of audio device file to grab data from */
    char*   adevice;
    /** audio framerate (please also set -rawaudio rate
        parameter to the same value) */
    int     arate;
    /** number of audio channels */
    int     achannels;
    /** if channels parameter exist, here will be channel
        number otherwise - frequency */
    float   freq_channel;
    char*   capture;
} radio_param_t;

extern radio_param_t stream_radio_defaults;

int radio_set_freq(struct stream_st *stream, float freq);
int radio_get_freq(struct stream_st *stream, float* freq);
char* radio_get_channel_name(struct stream_st *stream);
int radio_set_channel(struct stream_st *stream, char *channel);
int radio_step_channel(struct stream_st *stream, int direction);
int radio_step_freq(struct stream_st *stream, float step_interval);

#endif /* MPLAYER_STREAM_RADIO_H */
