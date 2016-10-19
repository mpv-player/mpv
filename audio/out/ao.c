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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mpv_talloc.h"

#include "config.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "audio/audio.h"

#include "options/options.h"
#include "options/m_config.h"
#include "common/msg.h"
#include "common/common.h"
#include "common/global.h"

extern const struct ao_driver audio_out_oss;
extern const struct ao_driver audio_out_coreaudio;
extern const struct ao_driver audio_out_coreaudio_exclusive;
extern const struct ao_driver audio_out_rsound;
extern const struct ao_driver audio_out_sndio;
extern const struct ao_driver audio_out_pulse;
extern const struct ao_driver audio_out_jack;
extern const struct ao_driver audio_out_openal;
extern const struct ao_driver audio_out_opensles;
extern const struct ao_driver audio_out_null;
extern const struct ao_driver audio_out_alsa;
extern const struct ao_driver audio_out_wasapi;
extern const struct ao_driver audio_out_pcm;
extern const struct ao_driver audio_out_lavc;
extern const struct ao_driver audio_out_sdl;

static const struct ao_driver * const audio_out_drivers[] = {
// native:
#if HAVE_COREAUDIO
    &audio_out_coreaudio,
#endif
#if HAVE_PULSE
    &audio_out_pulse,
#endif
#if HAVE_ALSA
    &audio_out_alsa,
#endif
#if HAVE_WASAPI
    &audio_out_wasapi,
#endif
#if HAVE_OSS_AUDIO
    &audio_out_oss,
#endif
    // wrappers:
#if HAVE_JACK
    &audio_out_jack,
#endif
#if HAVE_OPENAL
    &audio_out_openal,
#endif
#if HAVE_OPENSLES
    &audio_out_opensles,
#endif
#if HAVE_SDL1 || HAVE_SDL2
    &audio_out_sdl,
#endif
#if HAVE_SNDIO
    &audio_out_sndio,
#endif
    &audio_out_null,
#if HAVE_COREAUDIO
    &audio_out_coreaudio_exclusive,
#endif
    &audio_out_pcm,
#if HAVE_ENCODING
    &audio_out_lavc,
#endif
#if HAVE_RSOUND
    &audio_out_rsound,
#endif
    NULL
};

static bool get_desc(struct m_obj_desc *dst, int index)
{
    if (index >= MP_ARRAY_SIZE(audio_out_drivers) - 1)
        return false;
    const struct ao_driver *ao = audio_out_drivers[index];
    *dst = (struct m_obj_desc) {
        .name = ao->name,
        .description = ao->description,
        .priv_size = ao->priv_size,
        .priv_defaults = ao->priv_defaults,
        .options = ao->options,
        .global_opts = ao->global_opts,
        .legacy_prefix = ao->legacy_prefix,
        .hidden = ao->encode,
        .p = ao,
    };
    return true;
}

// For the ao option
const struct m_obj_list ao_obj_list = {
    .get_desc = get_desc,
    .description = "audio outputs",
    .allow_unknown_entries = true,
    .allow_trailer = true,
    .disallow_positional_parameters = true,
};

static struct ao *ao_alloc(bool probing, struct mpv_global *global,
                           void (*wakeup_cb)(void *ctx), void *wakeup_ctx,
                           char *name, char **args)
{
    assert(wakeup_cb);

    struct MPOpts *opts = global->opts;
    struct mp_log *log = mp_log_new(NULL, global->log, "ao");
    struct m_obj_desc desc;
    if (!m_obj_list_find(&desc, &ao_obj_list, bstr0(name))) {
        mp_msg(log, MSGL_ERR, "Audio output %s not found!\n", name);
        talloc_free(log);
        return NULL;
    };
    struct ao *ao = talloc_ptrtype(NULL, ao);
    talloc_steal(ao, log);
    *ao = (struct ao) {
        .driver = desc.p,
        .probing = probing,
        .global = global,
        .wakeup_cb = wakeup_cb,
        .wakeup_ctx = wakeup_ctx,
        .log = mp_log_new(ao, log, name),
        .def_buffer = opts->audio_buffer,
        .client_name = talloc_strdup(ao, opts->audio_client_name),
    };
    struct m_config *config =
        m_config_from_obj_desc_and_args(ao, ao->log, global, &desc,
                                        name, opts->ao_defs, args);
    if (!config)
        goto error;
    ao->priv = config->optstruct;
    return ao;
error:
    talloc_free(ao);
    return NULL;
}

static struct ao *ao_init(bool probing, struct mpv_global *global,
                          void (*wakeup_cb)(void *ctx), void *wakeup_ctx,
                          struct encode_lavc_context *encode_lavc_ctx, int flags,
                          int samplerate, int format, struct mp_chmap channels,
                          char *dev, char *name, char **args)
{
    struct ao *ao = ao_alloc(probing, global, wakeup_cb, wakeup_ctx, name, args);
    if (!ao)
        return NULL;
    ao->samplerate = samplerate;
    ao->channels = channels;
    ao->format = format;
    ao->encode_lavc_ctx = encode_lavc_ctx;
    ao->init_flags = flags;
    if (ao->driver->encode != !!ao->encode_lavc_ctx)
        goto fail;

    MP_VERBOSE(ao, "requested format: %d Hz, %s channels, %s\n",
               ao->samplerate, mp_chmap_to_str(&ao->channels),
               af_fmt_to_str(ao->format));

    ao->device = talloc_strdup(ao, dev);

    ao->api = ao->driver->play ? &ao_api_push : &ao_api_pull;
    ao->api_priv = talloc_zero_size(ao, ao->api->priv_size);
    assert(!ao->api->priv_defaults && !ao->api->options);

    ao->stream_silence = flags & AO_INIT_STREAM_SILENCE;

    int r = ao->driver->init(ao);
    if (r < 0) {
        // Silly exception for coreaudio spdif redirection
        if (ao->redirect) {
            char redirect[80], rdevice[80];
            snprintf(redirect, sizeof(redirect), "%s", ao->redirect);
            snprintf(rdevice, sizeof(rdevice), "%s", ao->device ? ao->device : "");
            talloc_free(ao);
            return ao_init(probing, global, wakeup_cb, wakeup_ctx,
                           encode_lavc_ctx, flags, samplerate, format, channels,
                           rdevice, redirect, NULL);
        }
        goto fail;
    }

    ao->sstride = af_fmt_to_bytes(ao->format);
    ao->num_planes = 1;
    if (af_fmt_is_planar(ao->format)) {
        ao->num_planes = ao->channels.num;
    } else {
        ao->sstride *= ao->channels.num;
    }
    ao->bps = ao->samplerate * ao->sstride;

    if (!ao->device_buffer && ao->driver->get_space)
        ao->device_buffer = ao->driver->get_space(ao);
    if (ao->device_buffer)
        MP_VERBOSE(ao, "device buffer: %d samples.\n", ao->device_buffer);
    ao->buffer = MPMAX(ao->device_buffer, ao->def_buffer * ao->samplerate);

    int align = af_format_sample_alignment(ao->format);
    ao->buffer = (ao->buffer + align - 1) / align * align;
    MP_VERBOSE(ao, "using soft-buffer of %d samples.\n", ao->buffer);

    if (ao->api->init(ao) < 0)
        goto fail;
    return ao;

fail:
    talloc_free(ao);
    return NULL;
}

static void split_ao_device(void *tmp, char *opt, char **out_ao, char **out_dev)
{
    *out_ao = NULL;
    *out_dev = NULL;
    if (!opt)
        return;
    if (!opt[0] || strcmp(opt, "auto") == 0)
        return;
    // Split on "/". If there's no "/", leave out_device NULL.
    bstr b_dev, b_ao;
    if (bstr_split_tok(bstr0(opt), "/", &b_ao, &b_dev))
        *out_dev = bstrto0(tmp, b_dev);
    *out_ao = bstrto0(tmp, b_ao);
}

struct ao *ao_init_best(struct mpv_global *global,
                        int init_flags,
                        void (*wakeup_cb)(void *ctx), void *wakeup_ctx,
                        struct encode_lavc_context *encode_lavc_ctx,
                        int samplerate, int format, struct mp_chmap channels)
{
    struct MPOpts *opts = global->opts;
    void *tmp = talloc_new(NULL);
    struct mp_log *log = mp_log_new(tmp, global->log, "ao");
    struct ao *ao = NULL;
    struct m_obj_settings *ao_list = NULL;
    int ao_num = 0;

    for (int n = 0; opts->audio_driver_list && opts->audio_driver_list[n].name; n++)
        MP_TARRAY_APPEND(tmp, ao_list, ao_num, opts->audio_driver_list[n]);

    bool forced_dev = false;
    char *pref_ao, *pref_dev;
    split_ao_device(tmp, opts->audio_device, &pref_ao, &pref_dev);
    if (!ao_num && pref_ao) {
        // Reuse the autoselection code
        MP_TARRAY_APPEND(tmp, ao_list, ao_num,
            (struct m_obj_settings){.name = pref_ao});
        forced_dev = true;
    }

    bool autoprobe = ao_num == 0;

    // Something like "--ao=a,b," means do autoprobing after a and b fail.
    if (ao_num && strlen(ao_list[ao_num - 1].name) == 0) {
        ao_num -= 1;
        autoprobe = true;
    }

    if (autoprobe) {
        for (int n = 0; audio_out_drivers[n]; n++) {
            const struct ao_driver *driver = audio_out_drivers[n];
            if (driver == &audio_out_null)
                break;
            MP_TARRAY_APPEND(tmp, ao_list, ao_num,
                (struct m_obj_settings){.name = (char *)driver->name});
        }
    }

    if (init_flags & AO_INIT_NULL_FALLBACK) {
        MP_TARRAY_APPEND(tmp, ao_list, ao_num,
            (struct m_obj_settings){.name = "null"});
    }

    for (int n = 0; n < ao_num; n++) {
        struct m_obj_settings *entry = &ao_list[n];
        bool probing = n + 1 != ao_num;
        mp_verbose(log, "Trying audio driver '%s'\n", entry->name);
        char *dev = NULL;
        if (pref_ao && pref_dev && strcmp(entry->name, pref_ao) == 0) {
            dev = pref_dev;
            mp_verbose(log, "Using preferred device '%s'\n", dev);
        }
        ao = ao_init(probing, global, wakeup_cb, wakeup_ctx, encode_lavc_ctx,
                     init_flags, samplerate, format, channels, dev,
                     entry->name, entry->attribs);
        if (ao)
            break;
        if (!probing)
            mp_err(log, "Failed to initialize audio driver '%s'\n", entry->name);
        if (dev && forced_dev) {
            mp_err(log, "This audio driver/device was forced with the "
                        "--audio-device option.\nTry unsetting it.\n");
        }
    }

    talloc_free(tmp);
    return ao;
}

// Uninitialize and destroy the AO. Remaining audio must be dropped.
void ao_uninit(struct ao *ao)
{
    if (ao)
        ao->api->uninit(ao);
    talloc_free(ao);
}

// Queue the given audio data. Start playback if it hasn't started yet. Return
// the number of samples that was accepted (the core will try to queue the rest
// again later). Should never block.
//  data: start pointer for each plane. If the audio data is packed, only
//        data[0] is valid, otherwise there is a plane for each channel.
//  samples: size of the audio data (see ao->sstride)
//  flags: currently AOPLAY_FINAL_CHUNK can be set
int ao_play(struct ao *ao, void **data, int samples, int flags)
{
    return ao->api->play(ao, data, samples, flags);
}

int ao_control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    return ao->api->control ? ao->api->control(ao, cmd, arg) : CONTROL_UNKNOWN;
}

// Return size of the buffered data in seconds. Can include the device latency.
// Basically, this returns how much data there is still to play, and how long
// it takes until the last sample in the buffer reaches the speakers. This is
// used for audio/video synchronization, so it's very important to implement
// this correctly.
double ao_get_delay(struct ao *ao)
{
    return ao->api->get_delay(ao);
}

// Return free size of the internal audio buffer. This controls how much audio
// the core should decode and try to queue with ao_play().
int ao_get_space(struct ao *ao)
{
    return ao->api->get_space(ao);
}

// Stop playback and empty buffers. Essentially go back to the state after
// ao->init().
void ao_reset(struct ao *ao)
{
    if (ao->api->reset)
        ao->api->reset(ao);
}

// Pause playback. Keep the current buffer. ao_get_delay() must return the
// same value as before pausing.
void ao_pause(struct ao *ao)
{
    if (ao->api->pause)
        ao->api->pause(ao);
}

// Resume playback. Play the remaining buffer. If the driver doesn't support
// pausing, it has to work around this and e.g. use ao_play_silence() to fill
// the lost audio.
void ao_resume(struct ao *ao)
{
    if (ao->api->resume)
        ao->api->resume(ao);
}

// Block until the current audio buffer has played completely.
void ao_drain(struct ao *ao)
{
    if (ao->api->drain)
        ao->api->drain(ao);
}

bool ao_eof_reached(struct ao *ao)
{
    return ao->api->get_eof ? ao->api->get_eof(ao) : true;
}

// Query the AO_EVENT_*s as requested by the events parameter, and return them.
int ao_query_and_reset_events(struct ao *ao, int events)
{
    return atomic_fetch_and(&ao->events_, ~(unsigned)events) & events;
}

static void ao_add_events(struct ao *ao, int events)
{
    atomic_fetch_or(&ao->events_, events);
    ao->wakeup_cb(ao->wakeup_ctx);
}

// Request that the player core destroys and recreates the AO. Fully thread-safe.
void ao_request_reload(struct ao *ao)
{
    ao_add_events(ao, AO_EVENT_RELOAD);
}

// Notify the player that the device list changed. Fully thread-safe.
void ao_hotplug_event(struct ao *ao)
{
    ao_add_events(ao, AO_EVENT_HOTPLUG);
}

bool ao_chmap_sel_adjust(struct ao *ao, const struct mp_chmap_sel *s,
                         struct mp_chmap *map)
{
    MP_VERBOSE(ao, "Channel layouts:\n");
    mp_chmal_sel_log(s, ao->log, MSGL_V);
    bool r = mp_chmap_sel_adjust(s, map);
    if (r)
        MP_VERBOSE(ao, "result: %s\n", mp_chmap_to_str(map));
    return r;
}

// safe_multichannel=true behaves like ao_chmap_sel_adjust.
// safe_multichannel=false is a helper for callers which do not support safe
// handling of arbitrary channel layouts. If the multichannel layouts are not
// considered "always safe" (e.g. HDMI), then allow only stereo or mono, if
// they are part of the list in *s.
bool ao_chmap_sel_adjust2(struct ao *ao, const struct mp_chmap_sel *s,
                          struct mp_chmap *map, bool safe_multichannel)
{
    if (!safe_multichannel && (ao->init_flags & AO_INIT_SAFE_MULTICHANNEL_ONLY)) {
        struct mp_chmap res = *map;
        if (mp_chmap_sel_adjust(s, &res)) {
            if (!mp_chmap_equals(&res, &(struct mp_chmap)MP_CHMAP_INIT_MONO) &&
                !mp_chmap_equals(&res, &(struct mp_chmap)MP_CHMAP_INIT_STEREO))
            {
                MP_WARN(ao, "Disabling multichannel output.\n");
                *map = (struct mp_chmap)MP_CHMAP_INIT_STEREO;
            }
        }
    }

    return ao_chmap_sel_adjust(ao, s, map);
}

bool ao_chmap_sel_get_def(struct ao *ao, const struct mp_chmap_sel *s,
                          struct mp_chmap *map, int num)
{
    return mp_chmap_sel_get_def(s, map, num);
}

// --- The following functions just return immutable information.

void ao_get_format(struct ao *ao, struct mp_audio *format)
{
    *format = (struct mp_audio){0};
    mp_audio_set_format(format, ao->format);
    mp_audio_set_channels(format, &ao->channels);
    format->rate = ao->samplerate;
}

const char *ao_get_name(struct ao *ao)
{
    return ao->driver->name;
}

const char *ao_get_description(struct ao *ao)
{
    return ao->driver->description;
}

bool ao_untimed(struct ao *ao)
{
    return ao->untimed;
}

// ---

struct ao_hotplug {
    struct mpv_global *global;
    void (*wakeup_cb)(void *ctx);
    void *wakeup_ctx;
    // A single AO instance is used to listen to hotplug events. It wouldn't
    // make much sense to allow multiple AO drivers; all sane platforms have
    // a single such audio API.
    // This is _not_ the same AO instance as used for playing audio.
    struct ao *ao;
    // cached
    struct ao_device_list *list;
    bool needs_update;
};

struct ao_hotplug *ao_hotplug_create(struct mpv_global *global,
                                     void (*wakeup_cb)(void *ctx),
                                     void *wakeup_ctx)
{
    struct ao_hotplug *hp = talloc_ptrtype(NULL, hp);
    *hp = (struct ao_hotplug){
        .global = global,
        .wakeup_cb = wakeup_cb,
        .wakeup_ctx = wakeup_ctx,
        .needs_update = true,
    };
    return hp;
}

static void get_devices(struct ao *ao, struct ao_device_list *list)
{
    int num = list->num_devices;
    if (ao->driver->list_devs) {
        ao->driver->list_devs(ao, list);
    } else {
        char name[80] = "Default";
        if (num > 1)
            mp_snprintf_cat(name, sizeof(name), " (%s)", ao->driver->name);
        ao_device_list_add(list, ao, &(struct ao_device_desc){"", name});
    }
}

bool ao_hotplug_check_update(struct ao_hotplug *hp)
{
    if (hp->ao && ao_query_and_reset_events(hp->ao, AO_EVENT_HOTPLUG)) {
        hp->needs_update = true;
        return true;
    }
    return false;
}

const char *ao_hotplug_get_detected_device(struct ao_hotplug *hp)
{
    if (!hp || !hp->ao)
        return NULL;
    return hp->ao->detected_device;
}

// The return value is valid until the next call to this API.
struct ao_device_list *ao_hotplug_get_device_list(struct ao_hotplug *hp)
{
    if (hp->list && !hp->needs_update)
        return hp->list;

    talloc_free(hp->list);
    struct ao_device_list *list = talloc_zero(hp, struct ao_device_list);
    hp->list = list;

    MP_TARRAY_APPEND(list, list->devices, list->num_devices,
        (struct ao_device_desc){"auto", "Autoselect device"});

    for (int n = 0; audio_out_drivers[n]; n++) {
        const struct ao_driver *d = audio_out_drivers[n];
        if (d == &audio_out_null)
            break; // don't add unsafe/special entries

        struct ao *ao = ao_alloc(true, hp->global, hp->wakeup_cb, hp->wakeup_ctx,
                                 (char *)d->name, NULL);
        if (!ao)
            continue;

        if (ao->driver->hotplug_init) {
            if (!hp->ao && ao->driver->hotplug_init(ao) >= 0)
                hp->ao = ao; // keep this one
            if (hp->ao && hp->ao->driver == d)
                get_devices(hp->ao, list);
        } else {
            get_devices(ao, list);
        }
        if (ao != hp->ao)
            talloc_free(ao);
    }
    hp->needs_update = false;
    return list;
}

void ao_device_list_add(struct ao_device_list *list, struct ao *ao,
                        struct ao_device_desc *e)
{
    struct ao_device_desc c = *e;
    const char *dname = ao->driver->name;
    c.name = c.name[0] ? talloc_asprintf(list, "%s/%s", dname, c.name)
                       : talloc_strdup(list, dname);
    c.desc = talloc_strdup(list, c.desc);
    MP_TARRAY_APPEND(list, list->devices, list->num_devices, c);
}

void ao_hotplug_destroy(struct ao_hotplug *hp)
{
    if (!hp)
        return;
    if (hp->ao && hp->ao->driver->hotplug_uninit)
        hp->ao->driver->hotplug_uninit(hp->ao);
    talloc_free(hp->ao);
    talloc_free(hp);
}

static void dummy_wakeup(void *ctx)
{
}

void ao_print_devices(struct mpv_global *global, struct mp_log *log)
{
    struct ao_hotplug *hp = ao_hotplug_create(global, dummy_wakeup, NULL);
    struct ao_device_list *list = ao_hotplug_get_device_list(hp);
    mp_info(log, "List of detected audio devices:\n");
    for (int n = 0; n < list->num_devices; n++) {
        struct ao_device_desc *desc = &list->devices[n];
        mp_info(log, "  '%s' (%s)\n", desc->name, desc->desc);
    }
    ao_hotplug_destroy(hp);
}
