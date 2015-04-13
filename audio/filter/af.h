/*
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

#ifndef MPLAYER_AF_H
#define MPLAYER_AF_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

#include "options/options.h"
#include "audio/format.h"
#include "audio/chmap.h"
#include "audio/audio.h"
#include "common/msg.h"

struct af_instance;
struct mpv_global;

// Number of channels
#define AF_NCH MP_NUM_CHANNELS

// Flags used for defining the behavior of an audio filter
#define AF_FLAGS_REENTRANT      0x00000000
#define AF_FLAGS_NOT_REENTRANT  0x00000001

// Flags for af->filter()
#define AF_FILTER_FLAG_EOF 1

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
    struct mp_log *log;
    struct replaygain_data *replaygain_data;
    int (*control)(struct af_instance *af, int cmd, void *arg);
    void (*uninit)(struct af_instance *af);
    /* Feed a frame. The frame is NULL if EOF was reached, and the filter
     * should drain all remaining buffered data.
     * Use af_add_output_frame() to output data. The optional filter_out
     * callback can be set to produce output frames gradually.
     */
    int (*filter_frame)(struct af_instance *af, struct mp_audio *frame);
    int (*filter_out)(struct af_instance *af);
    void *priv;
    struct mp_audio *data; // configuration and buffer for outgoing data stream

    struct af_instance *next;
    struct af_instance *prev;
    double delay; /* Delay caused by the filter, in seconds of audio consumed
                   * without corresponding output */
    bool auto_inserted; // inserted by af.c, such as conversion filters
    char *label;

    struct mp_audio fmt_in, fmt_out;

    struct mp_audio **out_queued;
    int num_out_queued;

    struct mp_audio_pool *out_pool;
};

// Current audio stream
struct af_stream {
    int initialized; // 0: no, 1: yes, -1: attempted to, but failed

    // The first and last filter in the list
    struct af_instance *first;
    struct af_instance *last;
    // The user sets the input format (what the decoder outputs), and sets some
    // or all fields in output to the output format the AO accepts.
    struct mp_audio input;
    struct mp_audio output;
    struct mp_audio filter_output;

    struct mp_log *log;
    struct MPOpts *opts;
    struct replaygain_data *replaygain_data;
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
    AF_CONTROL_SET_PLAYBACK_SPEED_RESAMPLE,
};

// Argument for AF_CONTROL_SET_PAN_LEVEL
typedef struct af_control_ext_s {
    void* arg;  // Argument
    int ch;     // Chanel number
} af_control_ext_t;

struct af_stream *af_new(struct mpv_global *global);
void af_destroy(struct af_stream *s);
int af_init(struct af_stream *s);
void af_uninit(struct af_stream *s);
struct af_instance *af_add(struct af_stream *s, char *name, char *label,
                           char **args);
int af_remove_by_label(struct af_stream *s, char *label);
struct af_instance *af_find_by_label(struct af_stream *s, char *label);
struct af_instance *af_control_any_rev(struct af_stream *s, int cmd, void *arg);
void af_control_all(struct af_stream *s, int cmd, void *arg);
void af_seek_reset(struct af_stream *s);

void af_add_output_frame(struct af_instance *af, struct mp_audio *frame);
int af_filter_frame(struct af_stream *s, struct mp_audio *frame);
int af_output_frame(struct af_stream *s, bool eof);
struct mp_audio *af_read_output_frame(struct af_stream *s);
int af_make_writeable(struct af_instance *af, struct mp_audio *frame);

double af_calc_delay(struct af_stream *s);

int af_test_output(struct af_instance *af, struct mp_audio *out);

int af_from_dB(int n, float *in, float *out, float k, float mi, float ma);
int af_from_ms(int n, float *in, int *out, int rate, float mi, float ma);
float af_softclip(float a);

#endif /* MPLAYER_AF_H */
