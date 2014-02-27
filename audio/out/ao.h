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

#include "bstr/bstr.h"
#include "common/common.h"
#include "audio/chmap.h"
#include "audio/chmap_sel.h"

enum aocontrol {
    // _VOLUME commands take struct ao_control_vol pointer for input/output.
    // If there's only one volume, SET should use average of left/right.
    AOCONTROL_GET_VOLUME,
    AOCONTROL_SET_VOLUME,
    // _MUTE commands take a pointer to bool
    AOCONTROL_GET_MUTE,
    AOCONTROL_SET_MUTE,
    // Has char* as argument, which contains the desired stream title.
    AOCONTROL_UPDATE_STREAM_TITLE,
};

// If set, then the queued audio data is the last. Note that after a while, new
// data might be written again, instead of closing the AO.
#define AOPLAY_FINAL_CHUNK 1

typedef struct ao_control_vol {
    float left;
    float right;
} ao_control_vol_t;

struct ao;

struct ao_driver {
    // If true, use with encoding only.
    bool encode;
    // Name used for --ao.
    const char *name;
    // Description shown with --ao=help.
    const char *description;
    // Init the device using ao->format/ao->channels/ao->samplerate. If the
    // device doesn't accept these parameters, you can attempt to negotiate
    // fallback parameters, and set the ao format fields accordingly.
    int (*init)(struct ao *ao);
    // See ao_control() etc. in ao.c
    int (*control)(struct ao *ao, enum aocontrol cmd, void *arg);
    void (*uninit)(struct ao *ao, bool cut_audio);
    void (*reset)(struct ao*ao);
    int (*get_space)(struct ao *ao);
    int (*play)(struct ao *ao, void **data, int samples, int flags);
    float (*get_delay)(struct ao *ao);
    void (*pause)(struct ao *ao);
    void (*resume)(struct ao *ao);

    // For option parsing (see vo.h)
    int priv_size;
    const void *priv_defaults;
    const struct m_option *options;
};

/* global data used by mplayer and plugins */
struct ao {
    int samplerate;
    struct mp_chmap channels;
    int format;                 // one of AF_FORMAT_...
    int bps;                    // bytes per second
    int sstride;                // size of a sample on each plane
                                // (format_size*num_channels/num_planes)
    double pts;                 // some mplayer.c state (why is this here?)
    struct mp_audio_buffer *buffer; // queued audio; passed to play() later
    int buffer_playable_samples;// part of the part of the buffer the AO hasn't
                                // accepted yet with play()
    bool probing;               // if true, don't fail loudly on init
    bool untimed;               // don't assume realtime playback
    bool no_persistent_volume;  // the AO does the equivalent of af_volume
    bool per_application_mixer; // like above, but volume persists (per app)
    const struct ao_driver *driver;
    void *priv;
    struct encode_lavc_context *encode_lavc_ctx;
    struct MPOpts *opts;
    struct input_ctx *input_ctx;
    struct mp_log *log; // Using e.g. "[ao/coreaudio]" as prefix
};

struct mpv_global;
struct ao *ao_init_best(struct mpv_global *global,
                        struct input_ctx *input_ctx,
                        struct encode_lavc_context *encode_lavc_ctx,
                        int samplerate, int format, struct mp_chmap channels);
void ao_uninit(struct ao *ao, bool cut_audio);
int ao_play(struct ao *ao, void **data, int samples, int flags);
int ao_control(struct ao *ao, enum aocontrol cmd, void *arg);
double ao_get_delay(struct ao *ao);
int ao_get_space(struct ao *ao);
void ao_reset(struct ao *ao);
void ao_pause(struct ao *ao);
void ao_resume(struct ao *ao);

int ao_play_silence(struct ao *ao, int samples);

bool ao_chmap_sel_adjust(struct ao *ao, const struct mp_chmap_sel *s,
                         struct mp_chmap *map);
bool ao_chmap_sel_get_def(struct ao *ao, const struct mp_chmap_sel *s,
                          struct mp_chmap *map, int num);

#endif /* MPLAYER_AUDIO_OUT_H */
