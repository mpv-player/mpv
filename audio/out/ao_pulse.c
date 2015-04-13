/*
 * PulseAudio audio output driver.
 * Copyright (C) 2006 Lennart Poettering
 * Copyright (C) 2007 Reimar Doeffinger
 *
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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include <pulse/pulseaudio.h>

#include "config.h"
#include "audio/format.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "ao.h"
#include "internal.h"

#define VOL_PA2MP(v) ((v) * 100 / PA_VOLUME_NORM)
#define VOL_MP2PA(v) ((v) * PA_VOLUME_NORM / 100)

struct priv {
    // PulseAudio playback stream object
    struct pa_stream *stream;

    // PulseAudio connection context
    struct pa_context *context;

    // Main event loop object
    struct pa_threaded_mainloop *mainloop;

    // temporary during control()
    struct pa_sink_input_info pi;

    int retval;

    // for wakeup handling
    pthread_mutex_t wakeup_lock;
    pthread_cond_t wakeup;
    int wakeup_status;

    char *cfg_host;
    char *cfg_sink;
    int cfg_buffer;
    int cfg_latency_hacks;
};

#define GENERIC_ERR_MSG(str) \
    MP_ERR(ao, str": %s\n", \
           pa_strerror(pa_context_errno(((struct priv *)ao->priv)->context)))

static void context_state_cb(pa_context *c, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY:
    case PA_CONTEXT_TERMINATED:
    case PA_CONTEXT_FAILED:
        pa_threaded_mainloop_signal(priv->mainloop, 0);
        break;
    }
}

static void subscribe_cb(pa_context *c, pa_subscription_event_type_t t,
                         uint32_t idx, void *userdata)
{
    struct ao *ao = userdata;
    int type = t & PA_SUBSCRIPTION_MASK_SINK;
    int fac = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    if ((type == PA_SUBSCRIPTION_EVENT_NEW || type == PA_SUBSCRIPTION_EVENT_REMOVE)
        && fac == PA_SUBSCRIPTION_EVENT_SINK)
    {
        ao_hotplug_event(ao);
    }
}

static void context_success_cb(pa_context *c, int success, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    priv->retval = success;
    pa_threaded_mainloop_signal(priv->mainloop, 0);
}

static void stream_state_cb(pa_stream *s, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    switch (pa_stream_get_state(s)) {
    case PA_STREAM_FAILED:
        MP_VERBOSE(ao, "Stream failed.\n");
        ao_request_reload(ao);
        pa_threaded_mainloop_signal(priv->mainloop, 0);
        break;
    case PA_STREAM_READY:
    case PA_STREAM_TERMINATED:
        pa_threaded_mainloop_signal(priv->mainloop, 0);
        break;
    }
}

static void wakeup(struct ao *ao)
{
    struct priv *priv = ao->priv;
    pthread_mutex_lock(&priv->wakeup_lock);
    priv->wakeup_status = 1;
    pthread_cond_signal(&priv->wakeup);
    pthread_mutex_unlock(&priv->wakeup_lock);
}

static void stream_request_cb(pa_stream *s, size_t length, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    wakeup(ao);
    pa_threaded_mainloop_signal(priv->mainloop, 0);
}

static int wait_audio(struct ao *ao, pthread_mutex_t *lock)
{
    struct priv *priv = ao->priv;
    // We don't use this mutex, because pulse like to call stream_request_cb
    // while we have the central mutex held.
    pthread_mutex_unlock(lock);
    pthread_mutex_lock(&priv->wakeup_lock);
    while (!priv->wakeup_status)
        pthread_cond_wait(&priv->wakeup, &priv->wakeup_lock);
    priv->wakeup_status = 0;
    pthread_mutex_unlock(&priv->wakeup_lock);
    pthread_mutex_lock(lock);
    return 0;
}

static void stream_latency_update_cb(pa_stream *s, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_signal(priv->mainloop, 0);
}

static void success_cb(pa_stream *s, int success, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    priv->retval = success;
    pa_threaded_mainloop_signal(priv->mainloop, 0);
}

/**
 * \brief waits for a pulseaudio operation to finish, frees it and
 *        unlocks the mainloop
 * \param op operation to wait for
 * \return 1 if operation has finished normally (DONE state), 0 otherwise
 */
static int waitop(struct priv *priv, pa_operation *op)
{
    if (!op) {
        pa_threaded_mainloop_unlock(priv->mainloop);
        return 0;
    }
    pa_operation_state_t state = pa_operation_get_state(op);
    while (state == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(priv->mainloop);
        state = pa_operation_get_state(op);
    }
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(priv->mainloop);
    return state == PA_OPERATION_DONE;
}

static const struct format_map {
    int mp_format;
    pa_sample_format_t pa_format;
} format_maps[] = {
    {AF_FORMAT_S16, PA_SAMPLE_S16NE},
    {AF_FORMAT_S32, PA_SAMPLE_S32NE},
    {AF_FORMAT_FLOAT, PA_SAMPLE_FLOAT32NE},
    {AF_FORMAT_U8, PA_SAMPLE_U8},
    {AF_FORMAT_UNKNOWN, 0}
};

static pa_encoding_t map_digital_format(int format)
{
    switch (format) {
    case AF_FORMAT_S_AC3:   return PA_ENCODING_AC3_IEC61937;
    case AF_FORMAT_S_EAC3:  return PA_ENCODING_EAC3_IEC61937;
    case AF_FORMAT_S_MP3:   return PA_ENCODING_MPEG_IEC61937;
    case AF_FORMAT_S_DTS:
    case AF_FORMAT_S_DTSHD: return PA_ENCODING_DTS_IEC61937;
#ifdef PA_ENCODING_MPEG2_AAC_IEC61937
    case AF_FORMAT_S_AAC:   return PA_ENCODING_MPEG2_AAC_IEC61937;
#endif
    default:
        if (AF_FORMAT_IS_IEC61937(format))
            return PA_ENCODING_ANY;
        return PA_ENCODING_PCM;
    }
}

static const int speaker_map[][2] = {
  {PA_CHANNEL_POSITION_FRONT_LEFT,              MP_SPEAKER_ID_FL},
  {PA_CHANNEL_POSITION_FRONT_RIGHT,             MP_SPEAKER_ID_FR},
  {PA_CHANNEL_POSITION_FRONT_CENTER,            MP_SPEAKER_ID_FC},
  {PA_CHANNEL_POSITION_REAR_CENTER,             MP_SPEAKER_ID_BC},
  {PA_CHANNEL_POSITION_REAR_LEFT,               MP_SPEAKER_ID_BL},
  {PA_CHANNEL_POSITION_REAR_RIGHT,              MP_SPEAKER_ID_BR},
  {PA_CHANNEL_POSITION_LFE,                     MP_SPEAKER_ID_LFE},
  {PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,    MP_SPEAKER_ID_FLC},
  {PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER,   MP_SPEAKER_ID_FRC},
  {PA_CHANNEL_POSITION_SIDE_LEFT,               MP_SPEAKER_ID_SL},
  {PA_CHANNEL_POSITION_SIDE_RIGHT,              MP_SPEAKER_ID_SR},
  {PA_CHANNEL_POSITION_TOP_CENTER,              MP_SPEAKER_ID_TC},
  {PA_CHANNEL_POSITION_TOP_FRONT_LEFT,          MP_SPEAKER_ID_TFL},
  {PA_CHANNEL_POSITION_TOP_FRONT_RIGHT,         MP_SPEAKER_ID_TFR},
  {PA_CHANNEL_POSITION_TOP_FRONT_CENTER,        MP_SPEAKER_ID_TFC},
  {PA_CHANNEL_POSITION_TOP_REAR_LEFT,           MP_SPEAKER_ID_TBL},
  {PA_CHANNEL_POSITION_TOP_REAR_RIGHT,          MP_SPEAKER_ID_TBR},
  {PA_CHANNEL_POSITION_TOP_REAR_CENTER,         MP_SPEAKER_ID_TBC},
  {PA_CHANNEL_POSITION_INVALID,                 -1}
};

static bool chmap_pa_from_mp(pa_channel_map *dst, struct mp_chmap *src)
{
    if (src->num > PA_CHANNELS_MAX)
        return false;
    dst->channels = src->num;
    if (mp_chmap_equals(src, &(const struct mp_chmap)MP_CHMAP_INIT_MONO)) {
        dst->map[0] = PA_CHANNEL_POSITION_MONO;
        return true;
    }
    for (int n = 0; n < src->num; n++) {
        int mp_speaker = src->speaker[n];
        int pa_speaker = PA_CHANNEL_POSITION_INVALID;
        for (int i = 0; speaker_map[i][1] != -1; i++) {
            if (speaker_map[i][1] == mp_speaker) {
                pa_speaker = speaker_map[i][0];
                break;
            }
        }
        if (pa_speaker == PA_CHANNEL_POSITION_INVALID)
            return false;
        dst->map[n] = pa_speaker;
    }
    return true;
}

static bool select_chmap(struct ao *ao, pa_channel_map *dst)
{
    struct mp_chmap_sel sel = {0};
    for (int n = 0; speaker_map[n][1] != -1; n++)
        mp_chmap_sel_add_speaker(&sel, speaker_map[n][1]);
    return ao_chmap_sel_adjust(ao, &sel, &ao->channels) &&
           chmap_pa_from_mp(dst, &ao->channels);
}

static void drain(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (priv->stream) {
        pa_threaded_mainloop_lock(priv->mainloop);
        waitop(priv, pa_stream_drain(priv->stream, success_cb, ao));
    }
}

static void uninit(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (priv->mainloop)
        pa_threaded_mainloop_stop(priv->mainloop);

    if (priv->stream) {
        pa_stream_disconnect(priv->stream);
        pa_stream_unref(priv->stream);
        priv->stream = NULL;
    }

    if (priv->context) {
        pa_context_disconnect(priv->context);
        pa_context_unref(priv->context);
        priv->context = NULL;
    }

    if (priv->mainloop) {
        pa_threaded_mainloop_free(priv->mainloop);
        priv->mainloop = NULL;
    }

    pthread_cond_destroy(&priv->wakeup);
    pthread_mutex_destroy(&priv->wakeup_lock);
}

static int pa_init_boilerplate(struct ao *ao)
{
    struct priv *priv = ao->priv;
    char *host = priv->cfg_host && priv->cfg_host[0] ? priv->cfg_host : NULL;
    bool locked = false;

    pthread_mutex_init(&priv->wakeup_lock, NULL);
    pthread_cond_init(&priv->wakeup, NULL);

    if (!(priv->mainloop = pa_threaded_mainloop_new())) {
        MP_ERR(ao, "Failed to allocate main loop\n");
        goto fail;
    }

    if (pa_threaded_mainloop_start(priv->mainloop) < 0)
        goto fail;

    pa_threaded_mainloop_lock(priv->mainloop);
    locked = true;

    if (!(priv->context = pa_context_new(pa_threaded_mainloop_get_api(
                                         priv->mainloop), ao->client_name)))
    {
        MP_ERR(ao, "Failed to allocate context\n");
        goto fail;
    }

    MP_VERBOSE(ao, "Library version: %s\n", pa_get_library_version());
    MP_VERBOSE(ao, "Proto: %lu\n",
        (long)pa_context_get_protocol_version(priv->context));
    MP_VERBOSE(ao, "Server proto: %lu\n",
        (long)pa_context_get_server_protocol_version(priv->context));

    pa_context_set_state_callback(priv->context, context_state_cb, ao);
    pa_context_set_subscribe_callback(priv->context, subscribe_cb, ao);

    if (pa_context_connect(priv->context, host, 0, NULL) < 0)
        goto fail;

    /* Wait until the context is ready */
    while (1) {
        int state = pa_context_get_state(priv->context);
        if (state == PA_CONTEXT_READY)
            break;
        if (!PA_CONTEXT_IS_GOOD(state))
            goto fail;
        pa_threaded_mainloop_wait(priv->mainloop);
    }

    pa_threaded_mainloop_unlock(priv->mainloop);
    return 0;

fail:
    if (locked)
        pa_threaded_mainloop_unlock(priv->mainloop);

    if (priv->context) {
        pa_threaded_mainloop_lock(priv->mainloop);
        if (!(pa_context_errno(priv->context) == PA_ERR_CONNECTIONREFUSED
              && ao->probing))
            GENERIC_ERR_MSG("Init failed");
        pa_threaded_mainloop_unlock(priv->mainloop);
    }
    uninit(ao);
    return -1;
}

static int init(struct ao *ao)
{
    struct pa_channel_map map;
    pa_proplist *proplist = NULL;
    pa_format_info *format = NULL;
    struct priv *priv = ao->priv;
    char *sink = priv->cfg_sink && priv->cfg_sink[0] ? priv->cfg_sink : ao->device;

    if (pa_init_boilerplate(ao) < 0)
        return -1;

    pa_threaded_mainloop_lock(priv->mainloop);

    if (!(format = pa_format_info_new()))
        goto unlock_and_fail;

    ao->format = af_fmt_from_planar(ao->format);

    format->encoding = map_digital_format(ao->format);
    if (format->encoding == PA_ENCODING_PCM) {
        const struct format_map *fmt_map = format_maps;

        while (fmt_map->mp_format != ao->format) {
            if (fmt_map->mp_format == AF_FORMAT_UNKNOWN) {
                MP_VERBOSE(ao, "Unsupported format, using default\n");
                fmt_map = format_maps;
                break;
            }
            fmt_map++;
        }
        ao->format = fmt_map->mp_format;

        pa_format_info_set_sample_format(format, fmt_map->pa_format);
    }

    if (!select_chmap(ao, &map))
        goto unlock_and_fail;

    if (!(proplist = pa_proplist_new())) {
        MP_ERR(ao, "Failed to allocate proplist\n");
        goto unlock_and_fail;
    }
    (void)pa_proplist_sets(proplist, PA_PROP_MEDIA_ICON_NAME, ao->client_name);

    pa_format_info_set_rate(format, ao->samplerate);
    pa_format_info_set_channels(format, ao->channels.num);
    pa_format_info_set_channel_map(format, &map);

    if (!pa_format_info_valid(format)) {
        MP_ERR(ao, "Invalid audio format\n");
        goto unlock_and_fail;
    }

    if (!(priv->stream = pa_stream_new_extended(priv->context, "audio stream",
                                                &format, 1, proplist)))
        goto unlock_and_fail;

    pa_format_info_free(format);
    format = NULL;

    pa_proplist_free(proplist);
    proplist = NULL;

    pa_stream_set_state_callback(priv->stream, stream_state_cb, ao);
    pa_stream_set_write_callback(priv->stream, stream_request_cb, ao);
    pa_stream_set_latency_update_callback(priv->stream,
                                          stream_latency_update_cb, ao);
    int buf_size = af_fmt_seconds_to_bytes(ao->format, priv->cfg_buffer / 1000.0,
                                           ao->channels.num, ao->samplerate);
    pa_buffer_attr bufattr = {
        .maxlength = -1,
        .tlength = buf_size > 0 ? buf_size : (uint32_t)-1,
        .prebuf = -1,
        .minreq = -1,
        .fragsize = -1,
    };

    int flags = PA_STREAM_NOT_MONOTONIC;
    if (!priv->cfg_latency_hacks)
        flags |= PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_AUTO_TIMING_UPDATE;

    if (pa_stream_connect_playback(priv->stream, sink, &bufattr,
                                   flags, NULL, NULL) < 0)
        goto unlock_and_fail;

    /* Wait until the stream is ready */
    while (1) {
        int state = pa_stream_get_state(priv->stream);
        if (state == PA_STREAM_READY)
            break;
        if (!PA_STREAM_IS_GOOD(state))
            goto unlock_and_fail;
        pa_threaded_mainloop_wait(priv->mainloop);
    }

    if (pa_stream_is_suspended(priv->stream)) {
        MP_ERR(ao, "The stream is suspended. Bailing out.\n");
        goto unlock_and_fail;
    }

    pa_threaded_mainloop_unlock(priv->mainloop);
    return 0;

unlock_and_fail:
    pa_threaded_mainloop_unlock(priv->mainloop);

    if (format)
        pa_format_info_free(format);

    if (proplist)
        pa_proplist_free(proplist);

    uninit(ao);
    return -1;
}

static void cork(struct ao *ao, bool pause)
{
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    priv->retval = 0;
    if (!waitop(priv, pa_stream_cork(priv->stream, pause, success_cb, ao)) ||
        !priv->retval)
        GENERIC_ERR_MSG("pa_stream_cork() failed");
}

// Play the specified data to the pulseaudio server
static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    if (pa_stream_write(priv->stream, data[0], samples * ao->sstride, NULL, 0,
                        PA_SEEK_RELATIVE) < 0) {
        GENERIC_ERR_MSG("pa_stream_write() failed");
        samples = -1;
    }
    if (flags & AOPLAY_FINAL_CHUNK) {
        // Force start in case the stream was too short for prebuf
        pa_operation *op = pa_stream_trigger(priv->stream, NULL, NULL);
        pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(priv->mainloop);
    return samples;
}

// Reset the audio stream, i.e. flush the playback buffer on the server side
static void reset(struct ao *ao)
{
    // pa_stream_flush() works badly if not corked
    cork(ao, true);
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    priv->retval = 0;
    if (!waitop(priv, pa_stream_flush(priv->stream, success_cb, ao)) ||
        !priv->retval)
        GENERIC_ERR_MSG("pa_stream_flush() failed");
    cork(ao, false);
}

// Pause the audio stream by corking it on the server
static void pause(struct ao *ao)
{
    cork(ao, true);
}

// Resume the audio stream by uncorking it on the server
static void resume(struct ao *ao)
{
    cork(ao, false);
}

// Return number of samples that may be written to the server without blocking
static int get_space(struct ao *ao)
{
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    size_t space = pa_stream_writable_size(priv->stream);
    pa_threaded_mainloop_unlock(priv->mainloop);
    return space / ao->sstride;
}

static double get_delay_hackfixed(struct ao *ao)
{
    /* This code basically does what pa_stream_get_latency() _should_
     * do, but doesn't due to multiple known bugs in PulseAudio (at
     * PulseAudio version 2.1). In particular, the timing interpolation
     * mode (PA_STREAM_INTERPOLATE_TIMING) can return completely bogus
     * values, and the non-interpolating code has a bug causing too
     * large results at end of stream (so a stream never seems to finish).
     * This code can still return wrong values in some cases due to known
     * PulseAudio bugs that can not be worked around on the client side.
     *
     * We always query the server for latest timing info. This may take
     * too long to work well with remote audio servers, but at least
     * this should be enough to fix the normal local playback case.
     */
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    if (!waitop(priv, pa_stream_update_timing_info(priv->stream, NULL, NULL))) {
        GENERIC_ERR_MSG("pa_stream_update_timing_info() failed");
        return 0;
    }
    pa_threaded_mainloop_lock(priv->mainloop);
    const pa_timing_info *ti = pa_stream_get_timing_info(priv->stream);
    if (!ti) {
        pa_threaded_mainloop_unlock(priv->mainloop);
        GENERIC_ERR_MSG("pa_stream_get_timing_info() failed");
        return 0;
    }
    const struct pa_sample_spec *ss = pa_stream_get_sample_spec(priv->stream);
    if (!ss) {
        pa_threaded_mainloop_unlock(priv->mainloop);
        GENERIC_ERR_MSG("pa_stream_get_sample_spec() failed");
        return 0;
    }
    // data left in PulseAudio's main buffers (not written to sink yet)
    int64_t latency = pa_bytes_to_usec(ti->write_index - ti->read_index, ss);
    // since this info may be from a while ago, playback has progressed since
    latency -= ti->transport_usec;
    // data already moved from buffers to sink, but not played yet
    int64_t sink_latency = ti->sink_usec;
    if (!ti->playing)
        /* At the end of a stream, part of the data "left" in the sink may
         * be padding silence after the end; that should be subtracted to
         * get the amount of real audio from our stream. This adjustment
         * is missing from Pulseaudio's own get_latency calculations
         * (as of PulseAudio 2.1). */
        sink_latency -= pa_bytes_to_usec(ti->since_underrun, ss);
    if (sink_latency > 0)
        latency += sink_latency;
    if (latency < 0)
        latency = 0;
    pa_threaded_mainloop_unlock(priv->mainloop);
    return latency / 1e6;
}

static double get_delay_pulse(struct ao *ao)
{
    struct priv *priv = ao->priv;
    pa_usec_t latency = (pa_usec_t) -1;
    pa_threaded_mainloop_lock(priv->mainloop);
    while (pa_stream_get_latency(priv->stream, &latency, NULL) < 0) {
        if (pa_context_errno(priv->context) != PA_ERR_NODATA) {
            GENERIC_ERR_MSG("pa_stream_get_latency() failed");
            break;
        }
        /* Wait until latency data is available again */
        pa_threaded_mainloop_wait(priv->mainloop);
    }
    pa_threaded_mainloop_unlock(priv->mainloop);
    return latency == (pa_usec_t) -1 ? 0 : latency / 1000000.0;
}

// Return the current latency in seconds
static double get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (priv->cfg_latency_hacks) {
        return get_delay_hackfixed(ao);
    } else {
        return get_delay_pulse(ao);
    }
}

/* A callback function that is called when the
 * pa_context_get_sink_input_info() operation completes. Saves the
 * volume field of the specified structure to the global variable volume.
 */
static void info_func(struct pa_context *c, const struct pa_sink_input_info *i,
                      int is_last, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    if (is_last < 0) {
        GENERIC_ERR_MSG("Failed to get sink input info");
        return;
    }
    if (!i)
        return;
    priv->pi = *i;
    pa_threaded_mainloop_signal(priv->mainloop, 0);
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *priv = ao->priv;
    switch (cmd) {
    case AOCONTROL_GET_MUTE:
    case AOCONTROL_GET_VOLUME: {
        uint32_t devidx = pa_stream_get_index(priv->stream);
        pa_threaded_mainloop_lock(priv->mainloop);
        if (!waitop(priv, pa_context_get_sink_input_info(priv->context, devidx,
                                                         info_func, ao))) {
            GENERIC_ERR_MSG("pa_context_get_sink_input_info() failed");
            return CONTROL_ERROR;
        }
        // Warning: some information in pi might be unaccessible, because
        // we naively copied the struct, without updating pointers etc.
        // Pointers might point to invalid data, accessors might fail.
        if (cmd == AOCONTROL_GET_VOLUME) {
            ao_control_vol_t *vol = arg;
            if (priv->pi.volume.channels != 2)
                vol->left = vol->right =
                    VOL_PA2MP(pa_cvolume_avg(&priv->pi.volume));
            else {
                vol->left = VOL_PA2MP(priv->pi.volume.values[0]);
                vol->right = VOL_PA2MP(priv->pi.volume.values[1]);
            }
        } else if (cmd == AOCONTROL_GET_MUTE) {
            bool *mute = arg;
            *mute = priv->pi.mute;
        }
        return CONTROL_OK;
    }

    case AOCONTROL_SET_MUTE:
    case AOCONTROL_SET_VOLUME: {
        pa_operation *o;

        pa_threaded_mainloop_lock(priv->mainloop);
        uint32_t stream_index = pa_stream_get_index(priv->stream);
        if (cmd == AOCONTROL_SET_VOLUME) {
            const ao_control_vol_t *vol = arg;
            struct pa_cvolume volume;

            pa_cvolume_reset(&volume, ao->channels.num);
            if (volume.channels != 2)
                pa_cvolume_set(&volume, volume.channels, VOL_MP2PA(vol->left));
            else {
                volume.values[0] = VOL_MP2PA(vol->left);
                volume.values[1] = VOL_MP2PA(vol->right);
            }
            o = pa_context_set_sink_input_volume(priv->context, stream_index,
                                                 &volume, NULL, NULL);
            if (!o) {
                pa_threaded_mainloop_unlock(priv->mainloop);
                GENERIC_ERR_MSG("pa_context_set_sink_input_volume() failed");
                return CONTROL_ERROR;
            }
        } else if (cmd == AOCONTROL_SET_MUTE) {
            const bool *mute = arg;
            o = pa_context_set_sink_input_mute(priv->context, stream_index,
                                               *mute, NULL, NULL);
            if (!o) {
                pa_threaded_mainloop_unlock(priv->mainloop);
                GENERIC_ERR_MSG("pa_context_set_sink_input_mute() failed");
                return CONTROL_ERROR;
            }
        } else
            abort();
        /* We don't wait for completion here */
        pa_operation_unref(o);
        pa_threaded_mainloop_unlock(priv->mainloop);
        return CONTROL_OK;
    }

    case AOCONTROL_HAS_PER_APP_VOLUME:
        return CONTROL_TRUE;

    case AOCONTROL_UPDATE_STREAM_TITLE: {
        char *title = (char *)arg;
        pa_threaded_mainloop_lock(priv->mainloop);
        if (!waitop(priv, pa_stream_set_name(priv->stream, title,
                                             success_cb, ao)))
        {
            GENERIC_ERR_MSG("pa_stream_set_name() failed");
            return CONTROL_ERROR;
        }
        return CONTROL_OK;
    }

    default:
        return CONTROL_UNKNOWN;
    }
}

struct sink_cb_ctx {
    struct ao *ao;
    struct ao_device_list *list;
};

static void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *ud)
{
    struct sink_cb_ctx *ctx = ud;
    struct priv *priv = ctx->ao->priv;

    if (eol) {
        pa_threaded_mainloop_signal(priv->mainloop, 0); // wakeup waitop()
        return;
    }

    struct ao_device_desc entry = {.name = i->name, .desc = i->description};
    ao_device_list_add(ctx->list, ctx->ao, &entry);
}

static int hotplug_init(struct ao *ao)
{
    struct priv *priv = ao->priv;
    if (pa_init_boilerplate(ao) < 0)
        return -1;

    pa_threaded_mainloop_lock(priv->mainloop);
    waitop(priv, pa_context_subscribe(priv->context, PA_SUBSCRIPTION_MASK_SINK,
                                      context_success_cb, ao));

    return 0;
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    struct priv *priv = ao->priv;
    struct sink_cb_ctx ctx = {ao, list};

    pa_threaded_mainloop_lock(priv->mainloop);
    waitop(priv, pa_context_get_sink_info_list(priv->context, sink_info_cb, &ctx));
}

static void hotplug_uninit(struct ao *ao)
{
    uninit(ao);
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_pulse = {
    .description = "PulseAudio audio output",
    .name      = "pulse",
    .control   = control,
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = pause,
    .resume    = resume,
    .drain     = drain,
    .wait      = wait_audio,
    .wakeup    = wakeup,
    .hotplug_init = hotplug_init,
    .hotplug_uninit = hotplug_uninit,
    .list_devs = list_devs,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .cfg_buffer = 250,
    },
    .options = (const struct m_option[]) {
        OPT_STRING("host", cfg_host, 0),
        OPT_STRING("sink", cfg_sink, 0),
        OPT_CHOICE_OR_INT("buffer", cfg_buffer, 0, 1, 2000, ({"native", 0})),
        OPT_FLAG("latency-hacks", cfg_latency_hacks, 0),
        {0}
    },
};
