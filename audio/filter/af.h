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

#ifndef MPLAYER_AF_H
#define MPLAYER_AF_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

#include "config.h"

#include "mpvcore/options.h"
#include "audio/format.h"
#include "audio/chmap.h"
#include "audio/audio.h"
#include "mpvcore/mp_msg.h"

struct af_instance;

// Number of channels
#define AF_NCH MP_NUM_CHANNELS

// Flags used for defining the behavior of an audio filter
#define AF_FLAGS_REENTRANT      0x00000000
#define AF_FLAGS_NOT_REENTRANT  0x00000001

/* Audio filter information not specific for current instance, but for
   a specific filter */
struct af_info {
    const char *info;
    const char *name;
    const int flags;
    int (*open)(struct af_instance *vf);
    bool (*test_conversion)(int src_format, int dst_format);
    int priv_size;
    const void *priv_defaults;
    const struct m_option *options;
};

// Linked list of audio filters
struct af_instance {
    const struct af_info *info;
    int (*control)(struct af_instance *af, int cmd, void *arg);
    void (*uninit)(struct af_instance *af);
    struct mp_audio * (*play)(struct af_instance *af, struct mp_audio *data);
    void *setup;  // old field for priv structs
    void *priv;
    struct mp_audio *data; // configuration and buffer for outgoing data stream
    struct af_instance *next;
    struct af_instance *prev;
    double delay; /* Delay caused by the filter, in seconds of audio consumed
                   * without corresponding output */
    double mul; /* length multiplier: how much does this instance change
                 * the number of samples passed though. (Ratio of input
                 * and output, e.g. mul=4 => 1 sample becomes 4 samples) .*/
    bool auto_inserted; // inserted by af.c, such as conversion filters
};

// Current audio stream
struct af_stream {
    // The first and last filter in the list
    struct af_instance *first;
    struct af_instance *last;
    // The user sets the input format (what the decoder outputs), and sets some
    // or all fields in output to the output format the AO accepts.
    struct mp_audio input;
    struct mp_audio output;
    struct mp_audio filter_output;

    struct MPOpts *opts;
};

// Return values
#define AF_DETACH   2
#define AF_OK       1
#define AF_TRUE     1
#define AF_FALSE    0
#define AF_UNKNOWN -1
#define AF_ERROR   -2
#define AF_FATAL   -3

// Parameters for af_control_*
enum af_control {
    AF_CONTROL_REINIT = 1,
    AF_CONTROL_RESET,
    AF_CONTROL_COMMAND_LINE,
    AF_CONTROL_SET_RESAMPLE_RATE,
    AF_CONTROL_SET_FORMAT,
    AF_CONTROL_SET_CHANNELS,
    AF_CONTROL_SET_VOLUME,
    AF_CONTROL_GET_VOLUME,
    AF_CONTROL_SET_PAN_LEVEL,
    AF_CONTROL_SET_PAN_NOUT,
    AF_CONTROL_SET_PAN_BALANCE,
    AF_CONTROL_GET_PAN_BALANCE,
    AF_CONTROL_SET_PLAYBACK_SPEED,
};

// Argument for AF_CONTROL_SET_PAN_LEVEL
typedef struct af_control_ext_s {
    void* arg;  // Argument
    int ch;     // Chanel number
} af_control_ext_t;

struct af_stream *af_new(struct MPOpts *opts);
void af_destroy(struct af_stream *s);
int af_init(struct af_stream *s);
void af_uninit(struct af_stream *s);
struct af_instance *af_add(struct af_stream *s, char *name, char **args);
struct mp_audio *af_play(struct af_stream *s, struct mp_audio *data);
struct af_instance *af_control_any_rev(struct af_stream *s, int cmd, void *arg);
void af_control_all(struct af_stream *s, int cmd, void *arg);

double af_calc_filter_multiplier(struct af_stream *s);
double af_calc_delay(struct af_stream *s);

int af_test_output(struct af_instance *af, struct mp_audio *out);

int af_from_dB(int n, float *in, float *out, float k, float mi, float ma);
int af_to_dB(int n, float *in, float *out, float k);
int af_from_ms(int n, float *in, int *out, int rate, float mi, float ma);
int af_to_ms(int n, int *in, float *out, int rate);
float af_softclip(float a);

#endif /* MPLAYER_AF_H */
