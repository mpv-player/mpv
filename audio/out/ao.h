/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_AUDIO_OUT_H
#define MPLAYER_AUDIO_OUT_H

#include <stdbool.h>

#include "misc/bstr.h"
#include "common/common.h"
#include "audio/chmap.h"
#include "audio/chmap_sel.h"

enum aocontrol {
    // _VOLUME commands take a pointer to float for input/output.
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
#define PLAYER_FINAL_CHUNK 1

enum {
    AO_EVENT_RELOAD = 1,
    AO_EVENT_HOTPLUG = 2,
};

enum {
    // Allow falling back to ao_null if nothing else works.
    AO_INIT_NULL_FALLBACK = 1 << 0,
    // Only accept multichannel configurations that are guaranteed to work
    // (i.e. not sending arbitrary layouts over HDMI).
    AO_INIT_SAFE_MULTICHANNEL_ONLY = 1 << 1,
    // Stream silence as long as no audio is playing.
    AO_INIT_STREAM_SILENCE = 1 << 2,
    // Force exclusive mode, i.e. lock out the system mixer.
    AO_INIT_EXCLUSIVE = 1 << 3,
    // Initialize with music role.
    AO_INIT_MEDIA_ROLE_MUSIC = 1 << 4,
};

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

struct ao_opts {
    struct m_obj_settings *audio_driver_list;
    char *audio_device;
    char *audio_client_name;
    double audio_buffer;
};

struct ao *ao_init_best(struct mpv_global *global,
                        int init_flags,
                        void (*wakeup_cb)(void *ctx), void *wakeup_ctx,
                        struct encode_lavc_context *encode_lavc_ctx,
                        int samplerate, int format, struct mp_chmap channels);
void ao_uninit(struct ao *ao);
void ao_get_format(struct ao *ao,
                   int *samplerate, int *format, struct mp_chmap *channels);
const char *ao_get_name(struct ao *ao);
const char *ao_get_description(struct ao *ao);
bool ao_untimed(struct ao *ao);
int ao_control(struct ao *ao, enum aocontrol cmd, void *arg);
void ao_set_gain(struct ao *ao, float gain);
double ao_get_delay(struct ao *ao);
void ao_reset(struct ao *ao);
void ao_start(struct ao *ao);
void ao_set_paused(struct ao *ao, bool paused, bool eof);
void ao_drain(struct ao *ao);
bool ao_is_playing(struct ao *ao);
struct mp_async_queue;
struct mp_async_queue *ao_get_queue(struct ao *ao);
int ao_query_and_reset_events(struct ao *ao, int events);
void ao_request_reload(struct ao *ao);
void ao_hotplug_event(struct ao *ao);

struct ao_hotplug;
struct ao_hotplug *ao_hotplug_create(struct mpv_global *global,
                                     void (*wakeup_cb)(void *ctx),
                                     void *wakeup_ctx);
void ao_hotplug_destroy(struct ao_hotplug *hp);
bool ao_hotplug_check_update(struct ao_hotplug *hp);
struct ao_device_list *ao_hotplug_get_device_list(struct ao_hotplug *hp, struct ao *playback_ao);

void ao_print_devices(struct mpv_global *global, struct mp_log *log, struct ao *playback_ao);

#endif /* MPLAYER_AUDIO_OUT_H */
