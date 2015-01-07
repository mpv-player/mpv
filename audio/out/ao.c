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
    // should not be auto-selected:
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

// Return true if the ao with the given name matches the opt (--audio-device
// contents). If opt is empty, matching always succeeds. If out_device is not
// NULL, it will be set to the implied audio device (or NULL).
static bool match_ao_driver(const char *ao_name, char *opt, char **out_device)
{
    bstr ao_nameb = bstr0(ao_name);
    bstr optb = bstr0(opt);

    char *dummy;
    if (!out_device)
        out_device = &dummy;
    *out_device = NULL;

    if (!optb.len || bstr_equals0(optb, "auto"))
        return true;
    if (!bstr_startswith(optb, ao_nameb))
        return false;
    if (optb.len > ao_nameb.len && optb.start[ao_nameb.len] != '/')
        return false;
    char *split = strchr(opt, '/');
    *out_device = split ? split + 1 : NULL;
    return true;
}

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

static struct ao *ao_alloc_pb(bool probing, struct mpv_global *global,
                              struct input_ctx *input_ctx,
                              struct encode_lavc_context *encode_lavc_ctx,
                              int samplerate, int format, struct mp_chmap channels,
                              char *name, char **args)
{
    struct MPOpts *opts = global->opts;
    struct ao *ao = ao_alloc(probing, global, input_ctx, name, args);
    if (!ao)
        return NULL;
    ao->samplerate = samplerate;
    ao->channels = channels;
    ao->format = format;
    ao->encode_lavc_ctx = encode_lavc_ctx;
    if (ao->driver->encode != !!ao->encode_lavc_ctx) {
        talloc_free(ao);
        return NULL;
    }

    match_ao_driver(ao->driver->name, opts->audio_device, &ao->device);
    ao->device = talloc_strdup(ao, ao->device);

    ao->client_name = talloc_strdup(ao, opts->audio_client_name);

    return ao;
}

static int ao_init(struct ao *ao)
{
    MP_VERBOSE(ao, "requested format: %d Hz, %s channels, %s\n",
               ao->samplerate, mp_chmap_to_str(&ao->channels),
               af_fmt_to_str(ao->format));

    ao->api = ao->driver->play ? &ao_api_push : &ao_api_pull;
    ao->api_priv = talloc_zero_size(ao, ao->api->priv_size);
    assert(!ao->api->priv_defaults && !ao->api->options);

    int r = ao->driver->init(ao);
    if (r < 0)
        return r;

    ao->sstride = af_fmt2bps(ao->format);
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
    MP_VERBOSE(ao, "using soft-buffer of %d samples.\n", ao->buffer);

    return ao->api->init(ao);
}

static struct ao *ao_create(bool probing, struct mpv_global *global,
                            struct input_ctx *input_ctx,
                            struct encode_lavc_context *encode_lavc_ctx,
                            int samplerate, int format, struct mp_chmap channels,
                            char *name, char **args)
{
    struct ao *ao = ao_alloc_pb(probing, global, input_ctx, encode_lavc_ctx,
                                samplerate, format, channels, name, args);
    if (ao && ao_init(ao) >= 0)
        return ao;
    // Silly exception for coreaudio spdif redirection
    if (ao && ao->redirect) {
        char redirect[80], device[80];
        snprintf(redirect, sizeof(redirect), "%s", ao->redirect);
        snprintf(device, sizeof(device), "%s", ao->device ? ao->device : "");
        talloc_free(ao);
        ao = ao_alloc_pb(probing, global, input_ctx, encode_lavc_ctx,
                         samplerate, format, channels, redirect, args);
        if (ao) {
            ao->device = talloc_strdup(ao, device);
            if (ao_init(ao) >= 0)
                return ao;
        }
    }
    talloc_free(ao);
    return NULL;
}

struct ao *ao_init_best(struct mpv_global *global,
                        struct input_ctx *input_ctx,
                        struct encode_lavc_context *encode_lavc_ctx,
                        int samplerate, int format, struct mp_chmap channels)
{
    struct MPOpts *opts = global->opts;
    struct mp_log *log = mp_log_new(NULL, global->log, "ao");
    struct ao *ao = NULL;
    struct m_obj_settings *ao_list = opts->audio_driver_list;
    if (ao_list && ao_list[0].name) {
        for (int n = 0; ao_list[n].name; n++) {
            if (strlen(ao_list[n].name) == 0)
                goto autoprobe;
            mp_verbose(log, "Trying preferred audio driver '%s'\n",
                       ao_list[n].name);
            ao = ao_create(false, global, input_ctx, encode_lavc_ctx,
                           samplerate, format, channels,
                           ao_list[n].name, ao_list[n].attribs);
            if (ao)
                goto done;
            mp_warn(log, "Failed to initialize audio driver '%s'\n",
                    ao_list[n].name);
        }
        goto done;
    }
autoprobe: ;
    // now try the rest...
    bool matched_dev = false;
    for (int i = 0; audio_out_drivers[i]; i++) {
        char *name = (char *)audio_out_drivers[i]->name;
        if (!match_ao_driver(name, opts->audio_device, NULL))
            continue;
        matched_dev = true;
        ao = ao_create(true, global, input_ctx, encode_lavc_ctx,
                       samplerate, format, channels, name, NULL);
        if (ao)
            goto done;
    }
    if (!matched_dev)
        mp_err(log, "--audio-device option refers to missing output driver.\n");
done:
    talloc_free(log);
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
    int actual_events = 0;
    if (atomic_load(&ao->request_reload)) // don't need to reset it
        actual_events |= AO_EVENT_RELOAD;
    return actual_events & events;
}

// Request that the player core destroys and recreates the AO.
void ao_request_reload(struct ao *ao)
{
    atomic_store(&ao->request_reload, true);
    mp_input_wakeup(ao->input_ctx);
}

bool ao_chmap_sel_adjust(struct ao *ao, const struct mp_chmap_sel *s,
                         struct mp_chmap *map)
{
    if (mp_msg_test(ao->log, MSGL_DEBUG)) {
        for (int i = 0; i < s->num_chmaps; i++) {
            struct mp_chmap c = s->chmaps[i];
            MP_DBG(ao, "chmap_sel #%d: %s\n", i, mp_chmap_to_str(&c));
        }
    }
    return mp_chmap_sel_adjust(s, map);
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

struct ao_device_list *ao_get_device_list(struct mpv_global *global)
{
    struct ao_device_list *list = talloc_zero(NULL, struct ao_device_list);
    MP_TARRAY_APPEND(list, list->devices, list->num_devices,
        (struct ao_device_desc){"auto", "Autoselect device"});
    for (int n = 0; audio_out_drivers[n]; n++) {
        const struct ao_driver *d = audio_out_drivers[n];
        if (d == &audio_out_null)
            break; // don't add unsafe/special entries
        struct ao *ao = ao_alloc(true, global, NULL, (char *)d->name, NULL);
        if (!ao)
            continue;
        int num = list->num_devices;
        if (d->list_devs)
            d->list_devs(ao, list);
        // Add at least a default entry
        if (list->num_devices == num)
            ao_device_list_add(list, ao, &(struct ao_device_desc){"", "Default"});
        talloc_free(ao);
    }
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

void ao_print_devices(struct mpv_global *global, struct mp_log *log)
{
    struct ao_device_list *list = ao_get_device_list(global);
    mp_info(log, "List of detected audio devices:\n");
    for (int n = 0; n < list->num_devices; n++) {
        struct ao_device_desc *desc = &list->devices[n];
        mp_info(log, "  '%s' (%s)\n", desc->name, desc->desc);
    }
    talloc_free(list);
}
