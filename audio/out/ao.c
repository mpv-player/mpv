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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "mpv_talloc.h"

#include "config.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"

#include "options/options.h"
#include "options/m_config_frontend.h"
#include "osdep/endian.h"
#include "common/msg.h"
#include "common/common.h"
#include "common/global.h"

extern const struct ao_driver audio_out_oss;
extern const struct ao_driver audio_out_audiotrack;
extern const struct ao_driver audio_out_audiounit;
extern const struct ao_driver audio_out_coreaudio;
extern const struct ao_driver audio_out_coreaudio_exclusive;
extern const struct ao_driver audio_out_rsound;
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
#if HAVE_ANDROID
    &audio_out_audiotrack,
#endif
#if HAVE_AUDIOUNIT
    &audio_out_audiounit,
#endif
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
#if HAVE_SDL2_AUDIO
    &audio_out_sdl,
#endif
    &audio_out_null,
#if HAVE_COREAUDIO
    &audio_out_coreaudio_exclusive,
#endif
    &audio_out_pcm,
    &audio_out_lavc,
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
        .options_prefix = ao->options_prefix,
        .global_opts = ao->global_opts,
        .hidden = ao->encode,
        .p = ao,
    };
    return true;
}

// For the ao option
static const struct m_obj_list ao_obj_list = {
    .get_desc = get_desc,
    .description = "audio outputs",
    .allow_unknown_entries = true,
    .allow_trailer = true,
    .disallow_positional_parameters = true,
    .use_global_options = true,
};

#define OPT_BASE_STRUCT struct ao_opts
const struct m_sub_options ao_conf = {
    .opts = (const struct m_option[]) {
        {"ao", OPT_SETTINGSLIST(audio_driver_list, &ao_obj_list),
            .flags = UPDATE_AUDIO},
        {"audio-device", OPT_STRING(audio_device), .flags = UPDATE_AUDIO},
        {"audio-client-name", OPT_STRING(audio_client_name), .flags = UPDATE_AUDIO},
        {"audio-buffer", OPT_DOUBLE(audio_buffer),
            .flags = UPDATE_AUDIO, M_RANGE(0, 10)},
        {0}
    },
    .size = sizeof(OPT_BASE_STRUCT),
    .defaults = &(const OPT_BASE_STRUCT){
        .audio_buffer = 0.2,
        .audio_device = "auto",
        .audio_client_name = "mpv",
    },
};

static struct ao *ao_alloc(bool probing, struct mpv_global *global,
                           void (*wakeup_cb)(void *ctx), void *wakeup_ctx,
                           char *name)
{
    assert(wakeup_cb);

    struct mp_log *log = mp_log_new(NULL, global->log, "ao");
    struct m_obj_desc desc;
    if (!m_obj_list_find(&desc, &ao_obj_list, bstr0(name))) {
        mp_msg(log, MSGL_ERR, "Audio output %s not found!\n", name);
        talloc_free(log);
        return NULL;
    };
    struct ao_opts *opts = mp_get_config_group(NULL, global, &ao_conf);
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
    talloc_free(opts);
    ao->priv = m_config_group_from_desc(ao, ao->log, global, &desc, name);
    if (!ao->priv)
        goto error;
    ao_set_gain(ao, 1.0f);
    return ao;
error:
    talloc_free(ao);
    return NULL;
}

static struct ao *ao_init(bool probing, struct mpv_global *global,
                          void (*wakeup_cb)(void *ctx), void *wakeup_ctx,
                          struct encode_lavc_context *encode_lavc_ctx, int flags,
                          int samplerate, int format, struct mp_chmap channels,
                          char *dev, char *name)
{
    struct ao *ao = ao_alloc(probing, global, wakeup_cb, wakeup_ctx, name);
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
    ao->stream_silence = flags & AO_INIT_STREAM_SILENCE;

    init_buffer_pre(ao);

    int r = ao->driver->init(ao);
    if (r < 0) {
        // Silly exception for coreaudio spdif redirection
        if (ao->redirect) {
            char redirect[80], rdevice[80];
            snprintf(redirect, sizeof(redirect), "%s", ao->redirect);
            snprintf(rdevice, sizeof(rdevice), "%s", ao->device ? ao->device : "");
            ao_uninit(ao);
            return ao_init(probing, global, wakeup_cb, wakeup_ctx,
                           encode_lavc_ctx, flags, samplerate, format, channels,
                           rdevice, redirect);
        }
        goto fail;
    }
    ao->driver_initialized = true;

    ao->sstride = af_fmt_to_bytes(ao->format);
    ao->num_planes = 1;
    if (af_fmt_is_planar(ao->format)) {
        ao->num_planes = ao->channels.num;
    } else {
        ao->sstride *= ao->channels.num;
    }
    ao->bps = ao->samplerate * ao->sstride;

    if (ao->device_buffer <= 0 && ao->driver->write) {
        MP_ERR(ao, "Device buffer size not set.\n");
        goto fail;
    }
    if (ao->device_buffer)
        MP_VERBOSE(ao, "device buffer: %d samples.\n", ao->device_buffer);
    ao->buffer = MPMAX(ao->device_buffer, ao->def_buffer * ao->samplerate);
    ao->buffer = MPMAX(ao->buffer, 1);

    int align = af_format_sample_alignment(ao->format);
    ao->buffer = (ao->buffer + align - 1) / align * align;
    MP_VERBOSE(ao, "using soft-buffer of %d samples.\n", ao->buffer);

    if (!init_buffer_post(ao))
        goto fail;
    return ao;

fail:
    ao_uninit(ao);
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
    // Split on "/". If "/" is the final character, or absent, out_dev is NULL.
    bstr b_dev, b_ao;
    bstr_split_tok(bstr0(opt), "/", &b_ao, &b_dev);
    if (b_dev.len > 0)
        *out_dev = bstrto0(tmp, b_dev);
    *out_ao = bstrto0(tmp, b_ao);
}

struct ao *ao_init_best(struct mpv_global *global,
                        int init_flags,
                        void (*wakeup_cb)(void *ctx), void *wakeup_ctx,
                        struct encode_lavc_context *encode_lavc_ctx,
                        int samplerate, int format, struct mp_chmap channels)
{
    void *tmp = talloc_new(NULL);
    struct ao_opts *opts = mp_get_config_group(tmp, global, &ao_conf);
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
                     init_flags, samplerate, format, channels, dev, entry->name);
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

// Query the AO_EVENT_*s as requested by the events parameter, and return them.
int ao_query_and_reset_events(struct ao *ao, int events)
{
    return atomic_fetch_and(&ao->events_, ~(unsigned)events) & events;
}

// Returns events that were set by this calls.
int ao_add_events(struct ao *ao, int events)
{
    unsigned prev_events = atomic_fetch_or(&ao->events_, events);
    unsigned new = events & ~prev_events;
    if (new)
        ao->wakeup_cb(ao->wakeup_ctx);
    return new;
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
                MP_VERBOSE(ao, "Disabling multichannel output.\n");
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

void ao_get_format(struct ao *ao,
                   int *samplerate, int *format, struct mp_chmap *channels)
{
    *samplerate = ao->samplerate;
    *format = ao->format;
    *channels = ao->channels;
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
    if (ao->driver->list_devs) {
        ao->driver->list_devs(ao, list);
    } else {
        ao_device_list_add(list, ao, &(struct ao_device_desc){"", ""});
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
                                 (char *)d->name);
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
    char buf[80];
    if (!c.desc || !c.desc[0]) {
        if (c.name && c.name[0]) {
            c.desc = c.name;
        } else if (list->num_devices) {
            // Assume this is the default device.
            snprintf(buf, sizeof(buf), "Default (%s)", dname);
            c.desc = buf;
        } else {
            // First default device (and maybe the only one).
            c.desc = "Default";
        }
    }
    c.name = (c.name && c.name[0]) ? talloc_asprintf(list, "%s/%s", dname, c.name)
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

void ao_set_gain(struct ao *ao, float gain)
{
    atomic_store(&ao->gain, gain);
}

#define MUL_GAIN_i(d, num_samples, gain, low, center, high)                     \
    for (int n = 0; n < (num_samples); n++)                                     \
        (d)[n] = MPCLAMP(                                                       \
            ((((int64_t)((d)[n]) - (center)) * (gain) + 128) >> 8) + (center),  \
            (low), (high))

#define MUL_GAIN_f(d, num_samples, gain)                                        \
    for (int n = 0; n < (num_samples); n++)                                     \
        (d)[n] = MPCLAMP(((d)[n]) * (gain), -1.0, 1.0)

static void process_plane(struct ao *ao, void *data, int num_samples)
{
    float gain = atomic_load_explicit(&ao->gain, memory_order_relaxed);
    int gi = lrint(256.0 * gain);
    if (gi == 256)
        return;
    switch (af_fmt_from_planar(ao->format)) {
    case AF_FORMAT_U8:
        MUL_GAIN_i((uint8_t *)data, num_samples, gi, 0, 128, 255);
        break;
    case AF_FORMAT_S16:
        MUL_GAIN_i((int16_t *)data, num_samples, gi, INT16_MIN, 0, INT16_MAX);
        break;
    case AF_FORMAT_S32:
        MUL_GAIN_i((int32_t *)data, num_samples, gi, INT32_MIN, 0, INT32_MAX);
        break;
    case AF_FORMAT_FLOAT:
        MUL_GAIN_f((float *)data, num_samples, gain);
        break;
    case AF_FORMAT_DOUBLE:
        MUL_GAIN_f((double *)data, num_samples, gain);
        break;
    default:;
        // all other sample formats are simply not supported
    }
}

void ao_post_process_data(struct ao *ao, void **data, int num_samples)
{
    bool planar = af_fmt_is_planar(ao->format);
    int planes = planar ? ao->channels.num : 1;
    int plane_samples = num_samples * (planar ? 1: ao->channels.num);
    for (int n = 0; n < planes; n++)
        process_plane(ao, data[n], plane_samples);
}

static int get_conv_type(struct ao_convert_fmt *fmt)
{
    if (af_fmt_to_bytes(fmt->src_fmt) * 8 == fmt->dst_bits && !fmt->pad_msb)
        return 0; // passthrough
    if (fmt->src_fmt == AF_FORMAT_S32 && fmt->dst_bits == 24 && !fmt->pad_msb)
        return 1; // simple 32->24 bit conversion
    if (fmt->src_fmt == AF_FORMAT_S32 && fmt->dst_bits == 32 && fmt->pad_msb == 8)
        return 2; // simple 32->24 bit conversion, with MSB padding
    return -1; // unsupported
}

// Check whether ao_convert_inplace() can be called. As an exception, the
// planar-ness of the sample format and the number of channels is ignored.
// All other parameters must be as passed to ao_convert_inplace().
bool ao_can_convert_inplace(struct ao_convert_fmt *fmt)
{
    return get_conv_type(fmt) >= 0;
}

bool ao_need_conversion(struct ao_convert_fmt *fmt)
{
    return get_conv_type(fmt) != 0;
}

// The LSB is always ignored.
#if BYTE_ORDER == BIG_ENDIAN
#define SHIFT24(x) ((3-(x))*8)
#else
#define SHIFT24(x) (((x)+1)*8)
#endif

static void convert_plane(int type, void *data, int num_samples)
{
    switch (type) {
    case 0:
        break;
    case 1: /* fall through */
    case 2: {
        int bytes = type == 1 ? 3 : 4;
        for (int s = 0; s < num_samples; s++) {
            uint32_t val = *((uint32_t *)data + s);
            uint8_t *ptr = (uint8_t *)data + s * bytes;
            ptr[0] = val >> SHIFT24(0);
            ptr[1] = val >> SHIFT24(1);
            ptr[2] = val >> SHIFT24(2);
            if (type == 2)
                ptr[3] = 0;
        }
        break;
    }
    default:
        abort();
    }
}

// data[n] contains the pointer to the first sample of the n-th plane, in the
// format implied by fmt->src_fmt. src_fmt also controls whether the data is
// all in one plane, or if there is a plane per channel.
void ao_convert_inplace(struct ao_convert_fmt *fmt, void **data, int num_samples)
{
    int type = get_conv_type(fmt);
    bool planar = af_fmt_is_planar(fmt->src_fmt);
    int planes = planar ? fmt->channels : 1;
    int plane_samples = num_samples * (planar ? 1: fmt->channels);
    for (int n = 0; n < planes; n++)
        convert_plane(type, data[n], plane_samples);
}
