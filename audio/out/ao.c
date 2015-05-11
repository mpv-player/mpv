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

#include "talloc.h"

#include "config.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "audio/audio.h"

#include "input/input.h"
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
extern const struct ao_driver audio_out_null;
extern const struct ao_driver audio_out_alsa;
extern const struct ao_driver audio_out_dsound;
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
#if HAVE_DSOUND
    &audio_out_dsound,
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
};

static struct ao *ao_alloc(bool probing, struct mpv_global *global,
                           struct input_ctx *input_ctx, char *name, char **args)
{
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
        .input_ctx = input_ctx,
        .log = mp_log_new(ao, log, name),
        .def_buffer = opts->audio_buffer,
        .client_name = talloc_strdup(ao, opts->audio_client_name),
    };
    struct m_config *config = m_config_from_obj_desc(ao, ao->log, &desc);
    if (m_config_apply_defaults(config, name, opts->ao_defs) < 0)
        goto error;
    if (m_config_set_obj_params(config, args) < 0)
        goto error;
    ao->priv = config->optstruct;
    return ao;
error:
    talloc_free(ao);
    return NULL;
}

static struct ao *ao_init(bool probing, struct mpv_global *global,
                          struct input_ctx *input_ctx,
                          struct encode_lavc_context *encode_lavc_ctx,
                          int samplerate, int format, struct mp_chmap channels,
                          char *dev, char *name, char **args)
{
    struct ao *ao = ao_alloc(probing, global, input_ctx, name, args);
    if (!ao)
        return NULL;
    ao->samplerate = samplerate;
    ao->channels = channels;
    ao->format = format;
    ao->encode_lavc_ctx = encode_lavc_ctx;
    if (ao->driver->encode != !!ao->encode_lavc_ctx)
        goto fail;

    MP_VERBOSE(ao, "requested format: %d Hz, %s channels, %s\n",
               ao->samplerate, mp_chmap_to_str(&ao->channels),
               af_fmt_to_str(ao->format));

    ao->device = talloc_strdup(ao, dev);

    ao->api = ao->driver->play ? &ao_api_push : &ao_api_pull;
    ao->api_priv = talloc_zero_size(ao, ao->api->priv_size);
    assert(!ao->api->priv_defaults && !ao->api->options);

    int r = ao->driver->init(ao);
    if (r < 0) {
        // Silly exception for coreaudio spdif redirection
        if (ao->redirect) {
            char redirect[80], rdevice[80];
            snprintf(redirect, sizeof(redirect), "%s", ao->redirect);
            snprintf(rdevice, sizeof(rdevice), "%s", ao->device ? ao->device : "");
            talloc_free(ao);
            return ao_init(probing, global, input_ctx, encode_lavc_ctx,
                           samplerate, format, channels, rdevice, redirect, args);
        }
        goto fail;
    }

    ao->sstride = af_fmt2bps(ao->format);
    ao->num_planes = 1;
    if (AF_FORMAT_IS_PLANAR(ao->format)) {
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
                        struct input_ctx *input_ctx,
                        struct encode_lavc_context *encode_lavc_ctx,
                        int samplerate, int format, struct mp_chmap channels)
{
    struct MPOpts *opts = global->opts;
    void *tmp = talloc_new(NULL);
    struct mp_log *log = mp_log_new(tmp, global->log, "ao");
    struct ao *ao = NULL;
    struct m_obj_settings *ao_list = opts->audio_driver_list;

    bool forced_dev = false;
    char *pref_ao, *pref_dev;
    split_ao_device(tmp, opts->audio_device, &pref_ao, &pref_dev);
    if (!(ao_list && ao_list[0].name) && pref_ao) {
        // Reuse the autoselection code
        ao_list = talloc_zero_array(tmp, struct m_obj_settings, 2);
        ao_list[0].name = pref_ao;
        forced_dev = true;
    }

    if (ao_list && ao_list[0].name) {
        for (int n = 0; ao_list[n].name; n++) {
            if (strlen(ao_list[n].name) == 0)
                goto autoprobe;
            mp_verbose(log, "Trying preferred audio driver '%s'\n",
                       ao_list[n].name);
            char *dev = NULL;
            if (pref_ao && strcmp(ao_list[n].name, pref_ao) == 0)
                dev = pref_dev;
            if (dev)
                mp_verbose(log, "Using preferred device '%s'\n", dev);
            ao = ao_init(false, global, input_ctx, encode_lavc_ctx,
                         samplerate, format, channels, dev,
                         ao_list[n].name, ao_list[n].attribs);
            if (ao)
                goto done;
            mp_err(log, "Failed to initialize audio driver '%s'\n",
                   ao_list[n].name);
            if (forced_dev) {
                mp_err(log, "This audio driver/device was forced with the "
                            "--audio-device option.\n"
                            "Try unsetting it.\n");
            }
        }
        goto done;
    }

autoprobe: ;
    // now try the rest...
    for (int i = 0; audio_out_drivers[i]; i++) {
        const struct ao_driver *driver = audio_out_drivers[i];
        if (driver == &audio_out_null)
            break;
        ao = ao_init(true, global, input_ctx, encode_lavc_ctx, samplerate,
                     format, channels, NULL, (char *)driver->name, NULL);
        if (ao)
            goto done;
    }

done:
    talloc_free(tmp);
    return ao;
}

// Uninitialize and destroy the AO. Remaining audio must be dropped.
void ao_uninit(struct ao *ao)
{
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
    if (ao->input_ctx)
        mp_input_wakeup(ao->input_ctx);
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
    if (mp_msg_test(ao->log, MSGL_DEBUG)) {
        for (int i = 0; i < s->num_chmaps; i++) {
            struct mp_chmap c = s->chmaps[i];
            struct mp_chmap cr = c;
            mp_chmap_reorder_norm(&cr);
            MP_DBG(ao, "chmap_sel #%d: %s (%s)\n", i, mp_chmap_to_str(&c),
                   mp_chmap_to_str(&cr));
        }
    }
    bool r = mp_chmap_sel_adjust(s, map);
    if (r) {
        struct mp_chmap mapr = *map;
        mp_chmap_reorder_norm(&mapr);
        MP_DBG(ao, "result: %s (%s)\n", mp_chmap_to_str(map),
               mp_chmap_to_str(&mapr));
    }
    return r;
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
    struct input_ctx *input_ctx;
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
                                     struct input_ctx *input_ctx)
{
    struct ao_hotplug *hp = talloc_ptrtype(NULL, hp);
    *hp = (struct ao_hotplug){
        .global = global,
        .input_ctx = input_ctx,
        .needs_update = true,
    };
    return hp;
}

static void get_devices(struct ao *ao, struct ao_device_list *list)
{
    int num = list->num_devices;
    if (ao->driver->list_devs)
        ao->driver->list_devs(ao, list);
    // Add at least a default entry
    if (list->num_devices == num)
        ao_device_list_add(list, ao, &(struct ao_device_desc){"", "Default"});
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

        struct ao *ao = ao_alloc(true, hp->global, hp->input_ctx,
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

void ao_print_devices(struct mpv_global *global, struct mp_log *log)
{
    struct ao_hotplug *hp = ao_hotplug_create(global, NULL);
    struct ao_device_list *list = ao_hotplug_get_device_list(hp);
    mp_info(log, "List of detected audio devices:\n");
    for (int n = 0; n < list->num_devices; n++) {
        struct ao_device_desc *desc = &list->devices[n];
        mp_info(log, "  '%s' (%s)\n", desc->name, desc->desc);
    }
    ao_hotplug_destroy(hp);
}
