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

#include <stdint.h>
#include <stdatomic.h>
#include <string.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <windows.h>

#include <initguid.h>
#include <spatialaudioclient.h>

#include "ao.h"
#include "ao_wasapi.h"
#include "audio/chmap_sel.h"
#include "audio/format.h"
#include "common/common.h"
#include "common/msg.h"
#include "internal.h"
#include "mpv_talloc.h"
#include "osdep/threads.h"
#include "osdep/timer.h"
#include "osdep/windows_utils.h"

struct spatial_object {
    AudioObjectType type;
    ISpatialAudioObject *object;
};

struct priv {
    mp_thread thread;
    HANDLE init_done;
    HANDLE command_event;
    HANDLE render_event;
    atomic_bool shutdown;
    atomic_bool reset_requested;
    atomic_bool running_requested;
    bool init_ok;
    bool started;

    LPWSTR device_id;
    IMMDevice *device;
    ISpatialAudioClient *client;
    ISpatialAudioObjectRenderStream *stream;
    IAudioClock *clock;
    UINT64 clock_frequency;
    WAVEFORMATEX *object_format;

    AudioObjectType native_mask;
    AudioObjectType static_mask;
    UINT32 max_frames;
    struct spatial_object objects[MP_NUM_CHANNELS];
    int num_objects;
    uint64_t submitted_frames;
};

static AudioObjectType speaker_to_object(int speaker)
{
    switch (speaker) {
    case MP_SPEAKER_ID_FL:  return AudioObjectType_FrontLeft;
    case MP_SPEAKER_ID_FR:  return AudioObjectType_FrontRight;
    case MP_SPEAKER_ID_FC:  return AudioObjectType_FrontCenter;
    case MP_SPEAKER_ID_LFE: return AudioObjectType_LowFrequency;
    case MP_SPEAKER_ID_SL:  return AudioObjectType_SideLeft;
    case MP_SPEAKER_ID_SR:  return AudioObjectType_SideRight;
    case MP_SPEAKER_ID_BL:  return AudioObjectType_BackLeft;
    case MP_SPEAKER_ID_BR:  return AudioObjectType_BackRight;
    case MP_SPEAKER_ID_BC:  return AudioObjectType_BackCenter;
    case MP_SPEAKER_ID_TFL: return AudioObjectType_TopFrontLeft;
    case MP_SPEAKER_ID_TFR: return AudioObjectType_TopFrontRight;
    case MP_SPEAKER_ID_TBL: return AudioObjectType_TopBackLeft;
    case MP_SPEAKER_ID_TBR: return AudioObjectType_TopBackRight;
    case MP_SPEAKER_ID_BFL: return AudioObjectType_BottomFrontLeft;
    case MP_SPEAKER_ID_BFR: return AudioObjectType_BottomFrontRight;
    default:                return AudioObjectType_None;
    }
}

static bool mask_has(AudioObjectType mask, AudioObjectType value)
{
    return value != AudioObjectType_None &&
           (((UINT32)mask & (UINT32)value) == (UINT32)value);
}

static void release_objects(struct ao *ao)
{
    struct priv *p = ao->priv;

    for (int n = 0; n < p->num_objects; n++) {
        if (p->objects[n].object) {
            ISpatialAudioObject_Release(p->objects[n].object);
            p->objects[n].object = NULL;
        }
    }
}

static bool stop_stream(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (!p->stream || !p->started)
        return true;

    HRESULT hr = ISpatialAudioObjectRenderStream_Stop(p->stream);
    p->started = false;
    if (FAILED(hr)) {
        MP_ERR(ao, "Stopping the spatial audio stream failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }
    return true;
}

static bool reset_stream(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (!stop_stream(ao))
        return false;

    HRESULT hr = ISpatialAudioObjectRenderStream_Reset(p->stream);
    release_objects(ao);
    p->submitted_frames = 0;
    if (FAILED(hr)) {
        MP_ERR(ao, "Resetting the spatial audio stream failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }
    return true;
}

static bool start_stream(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->started)
        return true;

    HRESULT hr = ISpatialAudioObjectRenderStream_Start(p->stream);
    if (FAILED(hr)) {
        MP_ERR(ao, "Starting the spatial audio stream failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }
    p->started = true;
    return true;
}

static void release_spatial(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->stream) {
        stop_stream(ao);
        ISpatialAudioObjectRenderStream_Reset(p->stream);
    }
    release_objects(ao);
    p->num_objects = 0;

    if (p->clock) {
        IAudioClock_Release(p->clock);
        p->clock = NULL;
    }
    if (p->stream) {
        ISpatialAudioObjectRenderStream_Release(p->stream);
        p->stream = NULL;
    }
    if (p->object_format) {
        CoTaskMemFree(p->object_format);
        p->object_format = NULL;
    }
    if (p->client) {
        ISpatialAudioClient_Release(p->client);
        p->client = NULL;
    }
    if (p->device) {
        IMMDevice_Release(p->device);
        p->device = NULL;
    }
    talloc_free(p->device_id);
    p->device_id = NULL;
}

static bool is_float32_object_format(const WAVEFORMATEX *format)
{
    if (format->nChannels != 1 || format->wBitsPerSample != 32)
        return false;

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        return true;

    if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
        format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
        return false;

    static const GUID ieee_float_subtype = {
        WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010,
        {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71},
    };
    const WAVEFORMATEXTENSIBLE *ext = (const WAVEFORMATEXTENSIBLE *)format;
    return memcmp(&ext->SubFormat, &ieee_float_subtype,
                  sizeof(ieee_float_subtype)) == 0;
}

static bool select_object_format(struct ao *ao)
{
    struct priv *p = ao->priv;
    IAudioFormatEnumerator *formats = NULL;
    HRESULT hr = ISpatialAudioClient_GetSupportedAudioObjectFormatEnumerator(
        p->client, &formats);
    if (FAILED(hr) || !formats) {
        MP_ERR(ao, "Enumerating spatial audio formats failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }

    UINT32 count = 0;
    hr = IAudioFormatEnumerator_GetCount(formats, &count);
    if (SUCCEEDED(hr)) {
        for (UINT32 n = 0; n < count; n++) {
            WAVEFORMATEX *format = NULL;
            hr = IAudioFormatEnumerator_GetFormat(formats, n, &format);
            if (FAILED(hr))
                break;
            if (format && is_float32_object_format(format)) {
                p->object_format = format;
                break;
            }
            CoTaskMemFree(format);
        }
    }
    IAudioFormatEnumerator_Release(formats);

    if (FAILED(hr)) {
        MP_ERR(ao, "Reading a spatial audio format failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }
    if (!p->object_format) {
        MP_ERR(ao, "No mono 32-bit float spatial audio format is available.\n");
        return false;
    }
    return true;
}

static bool select_channel_layout(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct mp_chmap_sel sel = {.tmp = ao};

    for (int speaker = 0; speaker < MP_SPEAKER_ID_COUNT; speaker++) {
        AudioObjectType type = speaker_to_object(speaker);
        if (mask_has(p->native_mask, type))
            mp_chmap_sel_add_speaker(&sel, speaker);
    }

    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels)) {
        MP_ERR(ao, "The spatial renderer can't represent the requested "
               "channel layout.\n");
        return false;
    }

    p->static_mask = AudioObjectType_None;
    p->num_objects = ao->channels.num;
    for (int n = 0; n < p->num_objects; n++) {
        AudioObjectType type = speaker_to_object(ao->channels.speaker[n]);
        if (!mask_has(p->native_mask, type)) {
            MP_ERR(ao, "The spatial renderer doesn't expose speaker %d.\n",
                   ao->channels.speaker[n]);
            return false;
        }
        p->objects[n].type = type;
        p->static_mask = (AudioObjectType)((UINT32)p->static_mask |
                                            (UINT32)type);
    }
    return true;
}

static bool activate_spatial(struct ao *ao)
{
    struct priv *p = ao->priv;

    p->device_id = wasapi_find_deviceID(ao);
    if (!p->device_id)
        return false;

    if (!wasapi_load_device(ao->log, &p->device, p->device_id))
        return false;

    HRESULT hr = IMMDevice_Activate(p->device, &IID_ISpatialAudioClient,
                            CLSCTX_INPROC_SERVER, NULL,
                            (void **)&p->client);
    if (FAILED(hr)) {
        MP_ERR(ao, "Activating Windows Spatial Audio failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }

    if (!select_object_format(ao))
        return false;

    hr = ISpatialAudioClient_GetNativeStaticObjectTypeMask(
        p->client, &p->native_mask);
    if (FAILED(hr)) {
        MP_ERR(ao, "Querying static spatial audio objects failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }

    if (!select_channel_layout(ao))
        return false;

    ao->samplerate = p->object_format->nSamplesPerSec;
    ao->format = AF_FORMAT_FLOATP;

    hr = ISpatialAudioClient_GetMaxFrameCount(
        p->client, p->object_format, &p->max_frames);
    if (FAILED(hr) || p->max_frames == 0) {
        MP_ERR(ao, "Querying the spatial audio buffer size failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }

    SpatialAudioObjectRenderStreamActivationParams params = {
        .ObjectFormat = p->object_format,
        .StaticObjectTypeMask = p->static_mask,
        .MinDynamicObjectCount = 0,
        .MaxDynamicObjectCount = 0,
        .Category = AudioCategory_Movie,
        .EventHandle = p->render_event,
        .NotifyObject = NULL,
    };
    PROPVARIANT activation = {0};
    activation.vt = VT_BLOB;
    activation.blob.cbSize = sizeof(params);
    activation.blob.pBlobData = (BYTE *)&params;

    hr = ISpatialAudioClient_ActivateSpatialAudioStream(
        p->client, &activation, &IID_ISpatialAudioObjectRenderStream,
        (void **)&p->stream);
    if (FAILED(hr) || !p->stream) {
        MP_ERR(ao, "Activating the spatial audio stream failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }

    hr = ISpatialAudioObjectRenderStream_GetService(
        p->stream, &IID_IAudioClock, (void **)&p->clock);
    if (SUCCEEDED(hr) && p->clock) {
        hr = IAudioClock_GetFrequency(p->clock, &p->clock_frequency);
        if (FAILED(hr) || p->clock_frequency == 0) {
            IAudioClock_Release(p->clock);
            p->clock = NULL;
            p->clock_frequency = 0;
        }
    }

    MP_INFO(ao, "Using Windows Spatial Audio: %s at %d Hz "
            "(%d static objects).\n",
            mp_chmap_to_str(&ao->channels), ao->samplerate,
            p->num_objects);
    return true;
}

static int64_t estimate_output_time(struct ao *ao, UINT32 frames)
{
    struct priv *p = ao->priv;
    double queued_frames = frames;

    if (p->clock && p->clock_frequency) {
        UINT64 position = 0;
        UINT64 qpc_position = 0;
        if (SUCCEEDED(IAudioClock_GetPosition(
                p->clock, &position, &qpc_position)))
        {
            double played = (double)position * ao->samplerate /
                            (double)p->clock_frequency;
            if ((double)p->submitted_frames > played)
                queued_frames += (double)p->submitted_frames - played;
        }
    } else {
        queued_frames += frames;
    }

    return mp_time_ns() + (int64_t)(queued_frames * 1000000000.0 /
                                    ao->samplerate);
}

static bool feed_spatial(struct ao *ao)
{
    struct priv *p = ao->priv;
    UINT32 available_dynamic = 0;
    UINT32 frames = 0;
    HRESULT hr = ISpatialAudioObjectRenderStream_BeginUpdatingAudioObjects(
        p->stream, &available_dynamic, &frames);
    if (FAILED(hr)) {
        MP_ERR(ao, "Starting a spatial audio update failed: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }
    (void)available_dynamic;

    void *planes[MP_NUM_CHANNELS] = {0};
    bool valid = frames > 0 && frames <= p->max_frames;
    for (int n = 0; valid && n < p->num_objects; n++) {
        if (!p->objects[n].object) {
            hr = ISpatialAudioObjectRenderStream_ActivateSpatialAudioObject(
                p->stream, p->objects[n].type, &p->objects[n].object);
            if (FAILED(hr) || !p->objects[n].object) {
                MP_ERR(ao, "Activating spatial object %d failed: %s\n",
                       n, mp_HRESULT_to_str(hr));
                valid = false;
                break;
            }
        }

        BYTE *buffer = NULL;
        UINT32 bytes = 0;
        hr = ISpatialAudioObject_GetBuffer(p->objects[n].object,
                                           &buffer, &bytes);
        if (FAILED(hr) || !buffer || bytes < frames * sizeof(float)) {
            MP_ERR(ao, "Getting the buffer for spatial object %d failed: %s\n",
                   n, mp_HRESULT_to_str(hr));
            valid = false;
            break;
        }
        planes[n] = buffer;
    }

    bool eof = 0;
    if (valid) {
        ao_read_data(ao, planes, frames, estimate_output_time(ao, frames),
                     &eof, true, true);
    }

    hr = ISpatialAudioObjectRenderStream_EndUpdatingAudioObjects(p->stream);
    if (FAILED(hr)) {
        MP_ERR(ao, "Finishing a spatial audio update failed: %s\n",
               mp_HRESULT_to_str(hr));
        valid = false;
    }

    if (valid)
        p->submitted_frames += frames;
    if (eof)
        ao_stop_streaming(ao);
    return valid;
}

static bool process_requests(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (atomic_load(&p->shutdown))
        return false;

    if (atomic_exchange(&p->reset_requested, false) &&
        !reset_stream(ao))
        return false;

    bool should_run = atomic_load(&p->running_requested);
    if (should_run && !p->started)
        return start_stream(ao);
    if (!should_run && p->started)
        return stop_stream(ao);
    return true;
}

static MP_THREAD_VOID spatial_thread(void *ctx)
{
    struct ao *ao = ctx;
    struct priv *p = ao->priv;
    mp_thread_set_name("ao/wasapi-spatial");

    HRESULT co_hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    p->init_ok = SUCCEEDED(co_hr) && activate_spatial(ao);
    SetEvent(p->init_done);
    if (!p->init_ok)
        goto done;

    int render_timeouts = 0;
    HANDLE waits[] = {p->command_event, p->render_event};
    while (true) {
        DWORD timeout = p->started ? 100 : INFINITE;
        DWORD result = WaitForMultipleObjects(MP_ARRAY_SIZE(waits), waits,
                                              false, timeout);
        if (result == WAIT_OBJECT_0) {
            render_timeouts = 0;
            if (!process_requests(ao))
                break;
        } else if (result == WAIT_OBJECT_0 + 1) {
            render_timeouts = 0;
            if (p->started && !feed_spatial(ao)) {
                ao_request_reload(ao);
                break;
            }
        } else if (result == WAIT_TIMEOUT) {
            if (++render_timeouts >= 10) {
                MP_WARN(ao, "The spatial audio render event stopped. "
                        "Reloading the audio output.\n");
                ao_request_reload(ao);
                break;
            }
        } else {
            MP_ERR(ao, "Waiting for the spatial audio stream failed: %lu\n",
                   GetLastError());
            ao_request_reload(ao);
            break;
        }
    }

done:
    release_spatial(ao);
    if (SUCCEEDED(co_hr))
        CoUninitialize();
    MP_THREAD_RETURN();
}

static void signal_requests(struct ao *ao)
{
    struct priv *p = ao->priv;
    SetEvent(p->command_event);
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->thread) {
        atomic_store(&p->shutdown, true);
        signal_requests(ao);
        mp_thread_join(p->thread);
        p->thread = NULL;
    }
    if (p->init_done) {
        CloseHandle(p->init_done);
        p->init_done = NULL;
    }
    if (p->command_event) {
        CloseHandle(p->command_event);
        p->command_event = NULL;
    }
    if (p->render_event) {
        CloseHandle(p->render_event);
        p->render_event = NULL;
    }
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;

    if ((ao->init_flags & AO_INIT_EXCLUSIVE) || af_fmt_is_spdif(ao->format)) {
        MP_ERR(ao, "Windows Spatial Audio accepts decoded PCM only and "
               "doesn't support exclusive mode.\n");
        return -1;
    }

    p->init_done = CreateEventW(NULL, false, false, NULL);
    p->command_event = CreateEventW(NULL, false, false, NULL);
    p->render_event = CreateEventW(NULL, false, false, NULL);
    if (!p->init_done || !p->command_event || !p->render_event) {
        MP_ERR(ao, "Creating spatial audio events failed.\n");
        uninit(ao);
        return -1;
    }

    atomic_store(&p->shutdown, false);
    atomic_store(&p->reset_requested, false);
    atomic_store(&p->running_requested, false);
    if (mp_thread_create(&p->thread, spatial_thread, ao)) {
        MP_ERR(ao, "Creating the spatial audio thread failed.\n");
        uninit(ao);
        return -1;
    }

    DWORD result = WaitForSingleObject(p->init_done, 15000);
    if (result != WAIT_OBJECT_0 || !p->init_ok) {
        if (result == WAIT_TIMEOUT)
            MP_ERR(ao, "Windows Spatial Audio initialization timed out.\n");
        uninit(ao);
        return -1;
    }
    return 0;
}

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    atomic_store(&p->running_requested, false);
    atomic_store(&p->reset_requested, true);
    signal_requests(ao);
}

static void start(struct ao *ao)
{
    struct priv *p = ao->priv;
    atomic_store(&p->running_requested, true);
    signal_requests(ao);
}

static bool set_pause(struct ao *ao, bool paused)
{
    struct priv *p = ao->priv;
    atomic_store(&p->running_requested, !paused);
    signal_requests(ao);
    return true;
}

const struct ao_driver audio_out_wasapi_spatial = {
    .description = "Windows Spatial Audio output",
    .name = "wasapi-spatial",
    .init = init,
    .uninit = uninit,
    .reset = reset,
    .start = start,
    .set_pause = set_pause,
    .list_devs = wasapi_list_devs,
    .priv_size = sizeof(struct priv),
};
