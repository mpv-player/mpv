/*
 * This file is part of mpv.
 *
 * Original author: Jonathan Yong <10walls@gmail.com>
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

#include <math.h>
#include <inttypes.h>
#include <libavutil/mathematics.h>

#include "options/m_option.h"
#include "osdep/threads.h"
#include "osdep/timer.h"
#include "osdep/io.h"
#include "misc/dispatch.h"
#include "ao_wasapi.h"

// naive av_rescale for unsigned
static UINT64 uint64_scale(UINT64 x, UINT64 num, UINT64 den)
{
    return (x / den) * num
        + ((x % den) * (num / den))
        + ((x % den) * (num % den)) / den;
}

static HRESULT get_device_delay(struct wasapi_state *state, double *delay_us) {
    UINT64 sample_count = atomic_load(&state->sample_count);
    UINT64 position, qpc_position;
    HRESULT hr;

    hr = IAudioClock_GetPosition(state->pAudioClock, &position, &qpc_position);
    EXIT_ON_ERROR(hr);
    // GetPosition succeeded, but the result may be
    // inaccurate due to the length of the call
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd370889%28v=vs.85%29.aspx
    if (hr == S_FALSE)
        MP_VERBOSE(state, "Possibly inaccurate device position.\n");

    // convert position to number of samples careful to avoid overflow
    UINT64 sample_position = uint64_scale(position,
                                          state->format.Format.nSamplesPerSec,
                                          state->clock_frequency);
    INT64 diff = sample_count - sample_position;
    *delay_us = diff * 1e6 / state->format.Format.nSamplesPerSec;

    // Correct for any delay in IAudioClock_GetPosition above.
    // This should normally be very small (<1 us), but just in case. . .
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    INT64 qpc_diff = av_rescale(qpc.QuadPart, 10000000, state->qpc_frequency.QuadPart)
                     - qpc_position;
    // ignore the above calculation if it yields more than 10 seconds (due to
    // possible overflow inside IAudioClock_GetPosition)
    if (qpc_diff < 10 * 10000000) {
        *delay_us -= qpc_diff / 10.0; // convert to us
    } else {
        MP_VERBOSE(state, "Insane qpc delay correction of %g seconds. "
                   "Ignoring it.\n", qpc_diff / 10000000.0);
    }

    if (sample_count > 0 && *delay_us <= 0) {
        MP_WARN(state, "Under-run: Device delay: %g us\n", *delay_us);
    } else {
        MP_TRACE(state, "Device delay: %g us\n", *delay_us);
    }

    return S_OK;
exit_label:
    MP_ERR(state, "Error getting device delay: %s\n", mp_HRESULT_to_str(hr));
    return hr;
}

static bool thread_feed(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    HRESULT hr;

    UINT32 frame_count = state->bufferFrameCount;
    UINT32 padding;
    hr = IAudioClient_GetCurrentPadding(state->pAudioClient, &padding);
    EXIT_ON_ERROR(hr);
    bool refill = false;
    if (state->share_mode == AUDCLNT_SHAREMODE_SHARED) {
        // Return if there's nothing to do.
        if (frame_count <= padding)
            return false;
        // In shared mode, there is only one buffer of size bufferFrameCount.
        // We must therefore take care not to overwrite the samples that have
        // yet to play.
        frame_count -= padding;
    } else if (padding >= 2 * frame_count) {
        // In exclusive mode, we exchange entire buffers of size
        // bufferFrameCount with the device. If there are already two such
        // full buffers waiting to play, there is no work to do.
        return false;
    } else if (padding < frame_count) {
        // If there is not at least one full buffer of audio queued to play in
        // exclusive mode, call this function again immediately to try and catch
        // up and avoid a cascade of under-runs. WASAPI doesn't seem to be smart
        // enough to send more feed events when it gets behind.
        refill = true;
    }
    MP_TRACE(ao, "Frame to fill: %"PRIu32". Padding: %"PRIu32"\n",
             frame_count, padding);

    double delay_us;
    hr = get_device_delay(state, &delay_us);
    EXIT_ON_ERROR(hr);
    // add the buffer delay
    delay_us += frame_count * 1e6 / state->format.Format.nSamplesPerSec;

    BYTE *pData;
    hr = IAudioRenderClient_GetBuffer(state->pRenderClient,
                                      frame_count, &pData);
    EXIT_ON_ERROR(hr);

    BYTE *data[1] = {pData};

    ao_read_data_converted(ao, &state->convert_format,
                           (void **)data, frame_count,
                           mp_time_us() + (int64_t)llrint(delay_us));

    // note, we can't use ao_read_data return value here since we already
    // committed to frame_count above in the GetBuffer call
    hr = IAudioRenderClient_ReleaseBuffer(state->pRenderClient,
                                          frame_count, 0);
    EXIT_ON_ERROR(hr);

    atomic_fetch_add(&state->sample_count, frame_count);

    return refill;
exit_label:
    MP_ERR(state, "Error feeding audio: %s\n", mp_HRESULT_to_str(hr));
    MP_VERBOSE(ao, "Requesting ao reload\n");
    ao_request_reload(ao);
    return false;
}

static void thread_reset(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    HRESULT hr;
    MP_DBG(state, "Thread Reset\n");
    hr = IAudioClient_Stop(state->pAudioClient);
    if (FAILED(hr))
        MP_ERR(state, "IAudioClient_Stop returned: %s\n", mp_HRESULT_to_str(hr));

    hr = IAudioClient_Reset(state->pAudioClient);
    if (FAILED(hr))
        MP_ERR(state, "IAudioClient_Reset returned: %s\n", mp_HRESULT_to_str(hr));

    atomic_store(&state->sample_count, 0);
}

static void thread_resume(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    MP_DBG(state, "Thread Resume\n");
    thread_reset(ao);
    thread_feed(ao);

    HRESULT hr = IAudioClient_Start(state->pAudioClient);
    if (FAILED(hr)) {
        MP_ERR(state, "IAudioClient_Start returned %s\n",
               mp_HRESULT_to_str(hr));
    }
}

static void thread_wakeup(void *ptr)
{
    struct ao *ao = ptr;
    struct wasapi_state *state = ao->priv;
    SetEvent(state->hWake);
}

static void set_thread_state(struct ao *ao,
                             enum wasapi_thread_state thread_state)
{
    struct wasapi_state *state = ao->priv;
    atomic_store(&state->thread_state, thread_state);
    thread_wakeup(ao);
}

static DWORD __stdcall AudioThread(void *lpParameter)
{
    struct ao *ao = lpParameter;
    struct wasapi_state *state = ao->priv;
    mpthread_set_name("wasapi event");
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    state->init_ok = wasapi_thread_init(ao);
    SetEvent(state->hInitDone);
    if (!state->init_ok)
        goto exit_label;

    MP_DBG(ao, "Entering dispatch loop\n");
    while (true) {
        if (WaitForSingleObject(state->hWake, INFINITE) != WAIT_OBJECT_0)
            MP_ERR(ao, "Unexpected return value from WaitForSingleObject\n");

        mp_dispatch_queue_process(state->dispatch, 0);

        int thread_state = atomic_load(&state->thread_state);
        switch (thread_state) {
        case WASAPI_THREAD_FEED:
            // fill twice on under-full buffer (see comment in thread_feed)
            if (thread_feed(ao) && thread_feed(ao))
                MP_ERR(ao, "Unable to fill buffer fast enough\n");
            break;
        case WASAPI_THREAD_RESET:
            thread_reset(ao);
            break;
        case WASAPI_THREAD_RESUME:
            thread_resume(ao);
            break;
        case WASAPI_THREAD_SHUTDOWN:
            thread_reset(ao);
            goto exit_label;
        default:
            MP_ERR(ao, "Unhandled thread state: %d\n", thread_state);
        }
        // the default is to feed unless something else is requested
        atomic_compare_exchange_strong(&state->thread_state, &thread_state,
                                       WASAPI_THREAD_FEED);
    }
exit_label:
    wasapi_thread_uninit(ao);

    CoUninitialize();
    MP_DBG(ao, "Thread return\n");
    return 0;
}

static void uninit(struct ao *ao)
{
    MP_DBG(ao, "Uninit wasapi\n");
    struct wasapi_state *state = ao->priv;
    if (state->hWake)
        set_thread_state(ao, WASAPI_THREAD_SHUTDOWN);

    if (state->hAudioThread &&
        WaitForSingleObject(state->hAudioThread, INFINITE) != WAIT_OBJECT_0)
    {
        MP_ERR(ao, "Unexpected return value from WaitForSingleObject "
               "while waiting for audio thread to terminate\n");
    }

    SAFE_DESTROY(state->hInitDone,   CloseHandle(state->hInitDone));
    SAFE_DESTROY(state->hWake,       CloseHandle(state->hWake));
    SAFE_DESTROY(state->hAudioThread,CloseHandle(state->hAudioThread));

    wasapi_change_uninit(ao);

    talloc_free(state->deviceID);

    CoUninitialize();
    MP_DBG(ao, "Uninit wasapi done\n");
}

static int init(struct ao *ao)
{
    MP_DBG(ao, "Init wasapi\n");
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    struct wasapi_state *state = ao->priv;
    state->log = ao->log;

    state->opt_exclusive |= ao->init_flags & AO_INIT_EXCLUSIVE;

#if !HAVE_UWP
    state->deviceID = wasapi_find_deviceID(ao);
    if (!state->deviceID) {
        uninit(ao);
        return -1;
    }
#endif

    if (state->deviceID)
        wasapi_change_init(ao, false);

    state->hInitDone = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hWake     = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!state->hInitDone || !state->hWake) {
        MP_FATAL(ao, "Error creating events\n");
        uninit(ao);
        return -1;
    }

    state->dispatch = mp_dispatch_create(state);
    mp_dispatch_set_wakeup_fn(state->dispatch, thread_wakeup, ao);

    state->init_ok = false;
    state->hAudioThread = CreateThread(NULL, 0, &AudioThread, ao, 0, NULL);
    if (!state->hAudioThread) {
        MP_FATAL(ao, "Failed to create audio thread\n");
        uninit(ao);
        return -1;
    }

    WaitForSingleObject(state->hInitDone, INFINITE); // wait on init complete
    SAFE_DESTROY(state->hInitDone,CloseHandle(state->hInitDone));
    if (!state->init_ok) {
        if (!ao->probing)
            MP_FATAL(ao, "Received failure from audio thread\n");
        uninit(ao);
        return -1;
    }

    MP_DBG(ao, "Init wasapi done\n");
    return 0;
}

static int thread_control_exclusive(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct wasapi_state *state = ao->priv;
    if (!state->pEndpointVolume)
        return CONTROL_UNKNOWN;

    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
    case AOCONTROL_SET_VOLUME:
        if (!(state->vol_hw_support & ENDPOINT_HARDWARE_SUPPORT_VOLUME))
            return CONTROL_FALSE;
        break;
    case AOCONTROL_GET_MUTE:
    case AOCONTROL_SET_MUTE:
        if (!(state->vol_hw_support & ENDPOINT_HARDWARE_SUPPORT_MUTE))
            return CONTROL_FALSE;
        break;
    case AOCONTROL_HAS_PER_APP_VOLUME:
        return CONTROL_FALSE;
    }

    float volume;
    BOOL mute;
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        IAudioEndpointVolume_GetMasterVolumeLevelScalar(
            state->pEndpointVolume, &volume);
        *(ao_control_vol_t *)arg = (ao_control_vol_t){
            .left  = 100.0f * volume,
            .right = 100.0f * volume,
        };
        return CONTROL_OK;
    case AOCONTROL_SET_VOLUME:
        volume = ((ao_control_vol_t *)arg)->left / 100.f;
        IAudioEndpointVolume_SetMasterVolumeLevelScalar(
            state->pEndpointVolume, volume, NULL);
        return CONTROL_OK;
    case AOCONTROL_GET_MUTE:
        IAudioEndpointVolume_GetMute(state->pEndpointVolume, &mute);
        *(bool *)arg = mute;
        return CONTROL_OK;
    case AOCONTROL_SET_MUTE:
        mute = *(bool *)arg;
        IAudioEndpointVolume_SetMute(state->pEndpointVolume, mute, NULL);
        return CONTROL_OK;
    }
    return CONTROL_UNKNOWN;
}

static int thread_control_shared(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct wasapi_state *state = ao->priv;
    if (!state->pAudioVolume)
        return CONTROL_UNKNOWN;

    float volume;
    BOOL mute;
    switch(cmd) {
    case AOCONTROL_GET_VOLUME:
        ISimpleAudioVolume_GetMasterVolume(state->pAudioVolume, &volume);
        *(ao_control_vol_t *)arg = (ao_control_vol_t){
            .left  = 100.0f * volume,
            .right = 100.0f * volume,
        };
        return CONTROL_OK;
    case AOCONTROL_SET_VOLUME:
        volume = ((ao_control_vol_t *)arg)->left / 100.f;
        ISimpleAudioVolume_SetMasterVolume(state->pAudioVolume, volume, NULL);
        return CONTROL_OK;
    case AOCONTROL_GET_MUTE:
        ISimpleAudioVolume_GetMute(state->pAudioVolume, &mute);
        *(bool *)arg = mute;
        return CONTROL_OK;
    case AOCONTROL_SET_MUTE:
        mute = *(bool *)arg;
        ISimpleAudioVolume_SetMute(state->pAudioVolume, mute, NULL);
        return CONTROL_OK;
    case AOCONTROL_HAS_PER_APP_VOLUME:
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static int thread_control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct wasapi_state *state = ao->priv;

    // common to exclusive and shared
    switch (cmd) {
    case AOCONTROL_UPDATE_STREAM_TITLE:
        if (!state->pSessionControl)
            return CONTROL_FALSE;

        wchar_t *title = mp_from_utf8(NULL, (char*)arg);
        wchar_t *tmp = NULL;
        // There is a weird race condition in the IAudioSessionControl itself --
        // it seems that *sometimes* the SetDisplayName does not take effect and
        // it still shows the old title. Use this loop to insist until it works.
        do {
            IAudioSessionControl_SetDisplayName(state->pSessionControl, title, NULL);

            SAFE_DESTROY(tmp, CoTaskMemFree(tmp));
            IAudioSessionControl_GetDisplayName(state->pSessionControl, &tmp);
        } while (wcscmp(title, tmp));
        SAFE_DESTROY(tmp, CoTaskMemFree(tmp));
        talloc_free(title);
        return CONTROL_OK;
    }

    return state->share_mode == AUDCLNT_SHAREMODE_EXCLUSIVE ?
        thread_control_exclusive(ao, cmd, arg) :
        thread_control_shared(ao, cmd, arg);
}

static void run_control(void *p)
{
    void **pp = p;
    struct ao *ao      = pp[0];
    enum aocontrol cmd = *(enum aocontrol *)pp[1];
    void *arg          = pp[2];
    *(int *)pp[3]      = thread_control(ao, cmd, arg);
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct wasapi_state *state = ao->priv;
    int ret;
    void *p[] = {ao, &cmd, arg, &ret};
    mp_dispatch_run(state->dispatch, run_control, p);
    return ret;
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
    wasapi_change_uninit(ao);
    CoUninitialize();
}

static int hotplug_init(struct ao *ao)
{
    MP_DBG(ao, "Hotplug init\n");
    struct wasapi_state *state = ao->priv;
    state->log = ao->log;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    HRESULT hr = wasapi_change_init(ao, true);
    EXIT_ON_ERROR(hr);

    return 0;
    exit_label:
    MP_FATAL(state, "Error setting up audio hotplug: %s\n", mp_HRESULT_to_str(hr));
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
};
