/*
 * This file is part of mpv.
 *
 * Original author: Jonathan Yong <10walls@gmail.com>
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
#include <inttypes.h>
#include <process.h>
#include <initguid.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <avrt.h>

#include "audio/out/ao_wasapi.h"
#include "audio/out/ao_wasapi_utils.h"

#include "audio/format.h"
#include "osdep/timer.h"
#include "osdep/io.h"

static HRESULT get_device_delay(struct wasapi_state *state, double *delay) {
    UINT64 sample_count = atomic_load(&state->sample_count);
    UINT64 position, qpc_position;
    HRESULT hr;

    hr = IAudioClock_GetPosition(state->pAudioClock, &position, &qpc_position);
    /* GetPosition succeeded, but the result may be inaccurate due to the length of the call */
    /* http://msdn.microsoft.com/en-us/library/windows/desktop/dd370889%28v=vs.85%29.aspx */
    if (hr == S_FALSE) {
        MP_DBG(state, "Possibly inaccurate device position.\n");
        hr = S_OK;
    }
    EXIT_ON_ERROR(hr);

    LARGE_INTEGER qpc_count;
    QueryPerformanceCounter(&qpc_count);
    double qpc_diff = (qpc_count.QuadPart * 1e7 / state->qpc_frequency.QuadPart) - qpc_position;

    position += state->clock_frequency * (uint64_t) (qpc_diff / 1e7);

    /* convert position to the same base as sample_count */
    position = position * state->format.Format.nSamplesPerSec / state->clock_frequency;

    double diff = sample_count - position;
    *delay = diff / state->format.Format.nSamplesPerSec;

    MP_TRACE(state, "Device delay: %g samples (%g ms)\n", diff, *delay * 1000);

    return S_OK;
exit_label:
    MP_ERR(state, "Error getting device delay: %s\n", mp_HRESULT_to_str(hr));
    return hr;
}

static void thread_feed(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    HRESULT hr;

    UINT32 frame_count = state->bufferFrameCount;

    if (state->share_mode == AUDCLNT_SHAREMODE_SHARED) {
        UINT32 padding = 0;
        hr = IAudioClient_GetCurrentPadding(state->pAudioClient, &padding);
        EXIT_ON_ERROR(hr);

        frame_count -= padding;
        MP_TRACE(ao, "Frame to fill: %"PRIu32". Padding: %"PRIu32"\n", frame_count, padding);
    }
    double delay;
    hr = get_device_delay(state, &delay);
    EXIT_ON_ERROR(hr);

    BYTE *pData;
    hr = IAudioRenderClient_GetBuffer(state->pRenderClient,
                                      frame_count, &pData);
    EXIT_ON_ERROR(hr);

    BYTE *data[1] = {pData};

    ao_read_data(ao, (void**)data, frame_count, (int64_t) (
                     mp_time_us() + delay * 1e6 +
                     frame_count * 1e6 / state->format.Format.nSamplesPerSec));

    hr = IAudioRenderClient_ReleaseBuffer(state->pRenderClient,
                                          frame_count, 0);
    EXIT_ON_ERROR(hr);

    atomic_fetch_add(&state->sample_count, frame_count);

    return;
exit_label:
    MP_ERR(state, "Error feeding audio: %s\n", mp_HRESULT_to_str(hr));
    MP_VERBOSE(ao, "Requesting ao reload\n");
    ao_request_reload(ao);
    return;
}

static void thread_resume(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    HRESULT hr;

    MP_DBG(state, "Thread Resume\n");
    UINT32 padding = 0;
    hr = IAudioClient_GetCurrentPadding(state->pAudioClient, &padding);
    if (hr != S_OK) {
        MP_ERR(state, "IAudioClient_GetCurrentPadding returned %s\n",
               mp_HRESULT_to_str(hr));
    }

    /* Fill the buffer before starting, but only if there is no audio queued to play. */
    /* This prevents overfilling the buffer, which leads to problems in exclusive mode */
    if (padding < (UINT32) state->bufferFrameCount)
        thread_feed(ao);

    // start feeding next wakeup if something else hasn't been requested
    int expected = WASAPI_THREAD_RESUME;
    atomic_compare_exchange_strong(&state->thread_state, &expected, WASAPI_THREAD_FEED);
    hr = IAudioClient_Start(state->pAudioClient);
    if (hr != S_OK)
        MP_ERR(state, "IAudioClient_Start returned %s\n", mp_HRESULT_to_str(hr));

    return;
}

static void thread_reset(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    HRESULT hr;
    MP_DBG(state, "Thread Reset\n");
    hr = IAudioClient_Stop(state->pAudioClient);
    /* we may get S_FALSE if the stream is already stopped */
    if (hr != S_OK && hr != S_FALSE)
        MP_ERR(state, "IAudioClient_Stop returned: %s\n", mp_HRESULT_to_str(hr));

    /* we may get S_FALSE if the stream is already reset */
    hr = IAudioClient_Reset(state->pAudioClient);
    if (hr != S_OK && hr != S_FALSE)
        MP_ERR(state, "IAudioClient_Reset returned: %s\n", mp_HRESULT_to_str(hr));

    atomic_store(&state->sample_count, 0);
    // start feeding next wakeup if something else hasn't been requested
    int expected = WASAPI_THREAD_RESET;
    atomic_compare_exchange_strong(&state->thread_state, &expected, WASAPI_THREAD_FEED);
    return;
}

static DWORD __stdcall AudioThread(void *lpParameter)
{
    struct ao *ao = lpParameter;
    struct wasapi_state *state = ao->priv;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    state->init_ret = wasapi_thread_init(ao);
    SetEvent(state->hInitDone);
    if (state->init_ret != S_OK)
        goto exit_label;

    MP_DBG(ao, "Entering dispatch loop\n");
    while (true) { /* watch events */
        HANDLE events[] = {state->hWake};
        switch (MsgWaitForMultipleObjects(MP_ARRAY_SIZE(events), events, FALSE, INFINITE,
                                          QS_POSTMESSAGE | QS_SENDMESSAGE)) {
        /* AudioThread wakeup */
        case WAIT_OBJECT_0:
            switch (atomic_load(&state->thread_state)) {
            case WASAPI_THREAD_FEED:
                thread_feed(ao);
                break;
            case WASAPI_THREAD_RESET:
                thread_reset(ao);
                break;
            case WASAPI_THREAD_RESUME:
                thread_reset(ao);
                thread_resume(ao);
                break;
            case WASAPI_THREAD_SHUTDOWN:
                thread_reset(ao);
                goto exit_label;
            default:
                MP_ERR(ao, "Unhandled thread state\n");
                goto exit_label;
            }
            break;
        /* messages to dispatch (COM marshalling) */
        case (WAIT_OBJECT_0 + MP_ARRAY_SIZE(events)):
            wasapi_dispatch(ao);
            break;
        default:
            MP_ERR(ao, "Unhandled thread event\n");
            goto exit_label;
        }
    }
exit_label:
    wasapi_thread_uninit(ao);

    CoUninitialize();
    MP_DBG(ao, "Thread return\n");
    return 0;
}

static void set_thread_state(struct ao *ao, enum wasapi_thread_state thread_state)
{
    struct wasapi_state *state = ao->priv;
    atomic_store(&state->thread_state, thread_state);
    SetEvent(state->hWake);
}

static void uninit(struct ao *ao)
{
    MP_DBG(ao, "Uninit wasapi\n");
    struct wasapi_state *state = ao->priv;
    wasapi_release_proxies(state);
    if (state->hWake)
        set_thread_state(ao, WASAPI_THREAD_SHUTDOWN);

    /* wait up to 10 seconds */
    if (state->hAudioThread &&
        WaitForSingleObject(state->hAudioThread, 10000) == WAIT_TIMEOUT)
    {
        MP_ERR(ao, "Audio loop thread refuses to abort\n");
        return;
    }
    if (state->VistaBlob.hAvrt)
        FreeLibrary(state->VistaBlob.hAvrt);

    SAFE_RELEASE(state->hInitDone,   CloseHandle(state->hInitDone));
    SAFE_RELEASE(state->hWake,       CloseHandle(state->hWake));
    SAFE_RELEASE(state->hAudioThread,CloseHandle(state->hAudioThread));

    CoUninitialize();
    MP_DBG(ao, "Uninit wasapi done\n");
}

static int init(struct ao *ao)
{
    MP_DBG(ao, "Init wasapi\n");
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    struct wasapi_state *state = ao->priv;
    state->log = ao->log;
    if(!wasapi_fill_VistaBlob(state))
        MP_WARN(ao, "Error loading thread priority functions\n");

    state->hInitDone = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hWake     = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!state->hInitDone || !state->hWake) {
        MP_ERR(ao, "Error creating events\n");
        uninit(ao);
        return -1;
    }

    state->init_ret = E_FAIL;
    state->hAudioThread = CreateThread(NULL, 0, &AudioThread, ao, 0, NULL);
    if (!state->hAudioThread) {
        MP_ERR(ao, "Failed to create audio thread\n");
        uninit(ao);
        return -1;
    }

    WaitForSingleObject(state->hInitDone, INFINITE); /* wait on init complete */
    SAFE_RELEASE(state->hInitDone,CloseHandle(state->hInitDone));
    if (state->init_ret != S_OK) {
        if (!ao->probing)
            MP_ERR(ao, "Received failure from audio thread\n");
        uninit(ao);
        return -1;
    }

    wasapi_setup_proxies(state);
    MP_DBG(ao, "Init wasapi done\n");
    return 0;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct wasapi_state *state = ao->priv;
    ao_control_vol_t *vol = arg;
    BOOL mute;

    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        if (state->opt_exclusive)
            IAudioEndpointVolume_GetMasterVolumeLevelScalar(state->pEndpointVolumeProxy,
                                                            &state->audio_volume);
        else
            ISimpleAudioVolume_GetMasterVolume(state->pAudioVolumeProxy,
                                               &state->audio_volume);

        /* check to see if user manually changed volume through mixer;
           this information is used in exclusive mode for restoring the mixer volume on uninit */
        if (state->audio_volume != state->previous_volume) {
            MP_VERBOSE(state, "Mixer difference: %.2g now, expected %.2g\n",
                       state->audio_volume, state->previous_volume);
            state->initial_volume = state->audio_volume;
        }

        vol->left = vol->right = 100.0f * state->audio_volume;
        return CONTROL_OK;
    case AOCONTROL_SET_VOLUME:
        state->audio_volume = vol->left / 100.f;
        if (state->opt_exclusive)
            IAudioEndpointVolume_SetMasterVolumeLevelScalar(state->pEndpointVolumeProxy,
                                                            state->audio_volume, NULL);
        else
            ISimpleAudioVolume_SetMasterVolume(state->pAudioVolumeProxy,
                                               state->audio_volume, NULL);

        state->previous_volume = state->audio_volume;
        return CONTROL_OK;
    case AOCONTROL_GET_MUTE:
        if (state->opt_exclusive)
            IAudioEndpointVolume_GetMute(state->pEndpointVolumeProxy, &mute);
        else
            ISimpleAudioVolume_GetMute(state->pAudioVolumeProxy, &mute);
        *(bool*)arg = mute;

        return CONTROL_OK;
    case AOCONTROL_SET_MUTE:
        mute = *(bool*)arg;
        if (state->opt_exclusive)
            IAudioEndpointVolume_SetMute(state->pEndpointVolumeProxy, mute, NULL);
        else
            ISimpleAudioVolume_SetMute(state->pAudioVolumeProxy, mute, NULL);

        return CONTROL_OK;
    case AOCONTROL_HAS_PER_APP_VOLUME:
        return CONTROL_TRUE;
    case AOCONTROL_UPDATE_STREAM_TITLE: {
        MP_VERBOSE(state, "Updating stream title to \"%s\"\n", (char*)arg);
        wchar_t *title = mp_from_utf8(NULL, (char*)arg);

        wchar_t *tmp = NULL;

        /* There is a weird race condition in the IAudioSessionControl itself --
           it seems that *sometimes* the SetDisplayName does not take effect and it still shows
           the old title. Use this loop to insist until it works. */
        do {
            IAudioSessionControl_SetDisplayName(state->pSessionControlProxy, title, NULL);

            SAFE_RELEASE(tmp, CoTaskMemFree(tmp));
            IAudioSessionControl_GetDisplayName(state->pSessionControlProxy, &tmp);
        } while (lstrcmpW(title, tmp));
        SAFE_RELEASE(tmp, CoTaskMemFree(tmp));
        talloc_free(title);

        return CONTROL_OK;
    }
    default:
        return CONTROL_UNKNOWN;
    }
}

static void audio_reset(struct ao *ao)
{
    set_thread_state(ao, WASAPI_THREAD_RESET);
}

static void audio_resume(struct ao *ao)
{
    set_thread_state(ao, WASAPI_THREAD_RESUME);
}

static void hotplug_uninit(struct ao *ao)
{
    MP_DBG(ao, "Hotplug uninit\n");
    struct wasapi_state *state = ao->priv;
    wasapi_change_uninit(ao);
    SAFE_RELEASE(state->pEnumerator, IMMDeviceEnumerator_Release(state->pEnumerator));
    CoUninitialize();
}

static int hotplug_init(struct ao *ao)
{
    MP_DBG(ao, "Hotplug init\n");
    struct wasapi_state *state = ao->priv;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                  &IID_IMMDeviceEnumerator, (void **)&state->pEnumerator);
    EXIT_ON_ERROR(hr);
    hr = wasapi_change_init(ao, true);
    EXIT_ON_ERROR(hr);

    return 0;
    exit_label:
    MP_ERR(state, "Error setting up audio hotplug: %s\n", mp_HRESULT_to_str(hr));
    hotplug_uninit(ao);
    return -1;
}

#define OPT_BASE_STRUCT struct wasapi_state

const struct ao_driver audio_out_wasapi = {
    .description    = "Windows WASAPI audio output (event mode)",
    .name           = "wasapi",
    .init           = init,
    .uninit         = uninit,
    .control        = control,
    .reset          = audio_reset,
    .resume         = audio_resume,
    .list_devs      = wasapi_list_devs,
    .hotplug_init   = hotplug_init,
    .hotplug_uninit = hotplug_uninit,
    .priv_size      = sizeof(wasapi_state),
    .options        = (const struct m_option[]) {
        OPT_FLAG("exclusive", opt_exclusive, 0),
        OPT_STRING("device", opt_device, 0),
        {NULL},
    },
};
