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

#ifndef MPLAYER_AUDIO_OUT_H
#define MPLAYER_AUDIO_OUT_H

#include <stdbool.h>

#include "misc/bstr.h"
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
    // the AO does the equivalent of af_volume (return CONTROL_TRUE if yes)
    AOCONTROL_HAS_SOFT_VOLUME,
    // like above, but volume persists (per app), mpv won't restore volume
    AOCONTROL_HAS_PER_APP_VOLUME,
};

// If set, then the queued audio data is the last. Note that after a while, new
// data might be written again, instead of closing the AO.
#define AOPLAY_FINAL_CHUNK 1

enum {
    AO_EVENT_RELOAD = 1,
    AO_EVENT_HOTPLUG = 2,
};

typedef struct ao_control_vol {
    float left;
    float right;
} ao_control_vol_t;

struct ao_device_desc {
    const char *name;   // symbolic name; will be set on ao->device
    const char *desc;   // verbose human readable name
};

struct ao_device_list {
    struct ao_device_desc *devices;
    int num_devices;
};

struct ao;
struct mpv_global;
struct input_ctx;
struct encode_lavc_context;
struct mp_audio;

struct ao *ao_init_best(struct mpv_global *global,
                        struct input_ctx *input_ctx,
                        struct encode_lavc_context *encode_lavc_ctx,
                        int samplerate, int format, struct mp_chmap channels);
void ao_uninit(struct ao *ao);
void ao_get_format(struct ao *ao, struct mp_audio *format);
const char *ao_get_name(struct ao *ao);
const char *ao_get_description(struct ao *ao);
bool ao_untimed(struct ao *ao);
int ao_play(struct ao *ao, void **data, int samples, int flags);
int ao_control(struct ao *ao, enum aocontrol cmd, void *arg);
double ao_get_delay(struct ao *ao);
int ao_get_space(struct ao *ao);
void ao_reset(struct ao *ao);
void ao_pause(struct ao *ao);
void ao_resume(struct ao *ao);
void ao_drain(struct ao *ao);
bool ao_eof_reached(struct ao *ao);
int ao_query_and_reset_events(struct ao *ao, int events);
void ao_request_reload(struct ao *ao);
void ao_hotplug_event(struct ao *ao);

struct ao_hotplug;
struct ao_hotplug *ao_hotplug_create(struct mpv_global *global,
                                     struct input_ctx *input_ctx);
void ao_hotplug_destroy(struct ao_hotplug *hp);
bool ao_hotplug_check_update(struct ao_hotplug *hp);
const char *ao_hotplug_get_detected_device(struct ao_hotplug *hp);
struct ao_device_list *ao_hotplug_get_device_list(struct ao_hotplug *hp);

void ao_print_devices(struct mpv_global *global, struct mp_log *log);

#endif /* MPLAYER_AUDIO_OUT_H */
