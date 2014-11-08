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

#define COBJMACROS 1
#define _WIN32_WINNT 0x600

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

#define EXIT_ON_ERROR(hres)  \
              do { if (FAILED(hres)) { goto exit_label; } } while(0)
#define SAFE_RELEASE(unk, release) \
              do { if ((unk) != NULL) { release; (unk) = NULL; } } while(0)

static double get_device_delay(struct wasapi_state *state) {
    UINT64 sample_count = atomic_load(&state->sample_count);
    UINT64 position, qpc_position;
    HRESULT hr;

    switch (hr = IAudioClock_GetPosition(state->pAudioClock, &position, &qpc_position)) {
        case S_OK: case S_FALSE:
            break;
        default:
            MP_ERR(state, "IAudioClock::GetPosition returned %s\n", wasapi_explain_err(hr));
    }

    LARGE_INTEGER qpc_count;
    QueryPerformanceCounter(&qpc_count);
    double qpc_diff = (qpc_count.QuadPart * 1e7 / state->qpc_frequency.QuadPart) - qpc_position;

    position += state->clock_frequency * (uint64_t)(qpc_diff / 1e7);

    /* convert position to the same base as sample_count */
    position = position * state->format.Format.nSamplesPerSec / state->clock_frequency;

    double diff = sample_count - position;
    double delay = diff / state->format.Format.nSamplesPerSec;

    MP_TRACE(state, "device delay: %g samples (%g ms)\n", diff, delay * 1000);

    return delay;
}

static void thread_feed(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    HRESULT hr;

    UINT32 frame_count = state->bufferFrameCount;

    if (state->share_mode == AUDCLNT_SHAREMODE_SHARED) {
        UINT32 padding = 0;
        hr = IAudioClient_GetCurrentPadding(state->pAudioClient, &padding);
        EXIT_ON_ERROR(hr);

        frame_count -= padding;
    }

    BYTE *pData;
    hr = IAudioRenderClient_GetBuffer(state->pRenderClient,
                                      frame_count, &pData);
    EXIT_ON_ERROR(hr);

    BYTE *data[1] = {pData};
    ao_read_data(ao, (void**)data, frame_count, (int64_t) (
                 mp_time_us() + get_device_delay(state) * 1e6 +
                 frame_count * 1e6 / state->format.Format.nSamplesPerSec));

    hr = IAudioRenderClient_ReleaseBuffer(state->pRenderClient,
                                          frame_count, 0);
    EXIT_ON_ERROR(hr);

    atomic_fetch_add(&state->sample_count, frame_count);

    return;
exit_label:
    MP_ERR(state, "thread_feed fails with %"PRIx32"!\n", (uint32_t)hr);
    return;
}

static DWORD __stdcall ThreadLoop(void *lpParameter)
{
    struct ao *ao = lpParameter;
    if (!ao || !ao->priv)
        return -1;
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    if (wasapi_thread_init(ao))
        goto exit_label;

    MP_VERBOSE(ao, "Setting up device monitoring on playback device!\n");
    HRESULT monitor = wasapi_change_init(&state->changeNotify,state->pDevice);
    MP_VERBOSE(ao, "Monitoring device: %S - %s!\n", state->changeNotify.monitored, wasapi_explain_err(monitor));
    if (monitor == S_OK) {
        IMMDeviceEnumerator_RegisterEndpointNotificationCallback(
            state->pEnumerator,
            (IMMNotificationClient *)&state->changeNotify);
    }

    MSG msg;
    DWORD waitstatus = WAIT_FAILED;
    HANDLE playcontrol[] = {
        state->hUninit,
        state->hFeed,
        state->hForceFeed,
        state->changeNotify.OnDeviceRemoved,
        state->changeNotify.OnDeviceStateChanged,
        state->changeNotify.OnPropertyValueChanged,
        NULL};
    int reload_requested=0;
    MP_VERBOSE(ao, "Entering dispatch loop!\n");
    while (1) { /* watch events */
        waitstatus = MsgWaitForMultipleObjects(
            sizeof(playcontrol)/sizeof(HANDLE) - 1,
            playcontrol, FALSE, INFINITE,
            QS_POSTMESSAGE | QS_SENDMESSAGE);
        switch (waitstatus) {
        case WAIT_OBJECT_0: /*shutdown*/
            IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(
                state->pEnumerator,
                (IMMNotificationClient *)&state->changeNotify);
            wasapi_change_free(&state->changeNotify);
            wasapi_thread_uninit(state);
            goto exit_label;
        case (WAIT_OBJECT_0 + 1): /* feed */
            thread_feed(ao);
            break;
        case (WAIT_OBJECT_0 + 2): /* force feed */
            thread_feed(ao);
            SetEvent(state->hFeedDone);
            break;
        case (WAIT_OBJECT_0 + 3): /* force reset */
            MP_VERBOSE(ao, "OnDeviceRemoved Triggered!\n");
            if(!reload_requested){
                ao_request_reload(ao);
                reload_requested=1;
            }
            break;
        case (WAIT_OBJECT_0 + 4):
            MP_VERBOSE(ao, "OnDeviceStateChanged Triggered!\n");
            if(!reload_requested){
                ao_request_reload(ao);
                reload_requested=1;
            }
            break;
        case (WAIT_OBJECT_0 + 5):
            MP_VERBOSE(ao, "OnPropertyValueChanged Triggered!\n");
            MP_VERBOSE(ao,"  -->Changed device property "
                       "{%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x}#%d\n",
                       state->changeNotify.propChanged.fmtid.Data1,
                       state->changeNotify.propChanged.fmtid.Data2,
                       state->changeNotify.propChanged.fmtid.Data3,
                       state->changeNotify.propChanged.fmtid.Data4[0], state->changeNotify.propChanged.fmtid.Data4[1],
                       state->changeNotify.propChanged.fmtid.Data4[2], state->changeNotify.propChanged.fmtid.Data4[3],
                       state->changeNotify.propChanged.fmtid.Data4[4], state->changeNotify.propChanged.fmtid.Data4[5],
                       state->changeNotify.propChanged.fmtid.Data4[6], state->changeNotify.propChanged.fmtid.Data4[7],
                       state->changeNotify.propChanged.pid);
            if(!reload_requested){
                ao_request_reload(ao);
                reload_requested=1;
            }
            break;
        case (WAIT_OBJECT_0 + 6): /* messages to dispatch (COM marshalling) */
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                DispatchMessage(&msg);
            }
            break;
        case WAIT_FAILED: /* ??? */
            return -1;
        }
    }
exit_label:
    /* dispatch any possible pending messages */
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        DispatchMessage(&msg);
    }
    return state->init_ret;
}

static void closehandles(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    if (state->init_done)
        CloseHandle(state->init_done);
    if (state->hUninit)
        CloseHandle(state->hUninit);
    if (state->hFeed)
        CloseHandle(state->hFeed);
}

static void uninit(struct ao *ao)
{
    MP_VERBOSE(ao, "uninit!\n");
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    wasapi_release_proxies(state);
    SetEvent(state->hUninit);
    /* wait up to 10 seconds */
    if (WaitForSingleObject(state->threadLoop, 10000) == WAIT_TIMEOUT)
        MP_ERR(ao, "audio loop thread refuses to abort!");
    if (state->VistaBlob.hAvrt)
        FreeLibrary(state->VistaBlob.hAvrt);
    closehandles(ao);
    MP_VERBOSE(ao, "uninit END!\n");
}

static int init(struct ao *ao)
{
    MP_VERBOSE(ao, "init!\n");
    ao->format = af_fmt_from_planar(ao->format);
    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        return -1;
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    state->log = ao->log;
    wasapi_fill_VistaBlob(state);

    if (state->opt_list) {
        wasapi_enumerate_devices(state->log, NULL, NULL);
    }

    if (state->opt_exclusive) {
        state->share_mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
    } else {
        state->share_mode = AUDCLNT_SHAREMODE_SHARED;
    }

    state->init_done = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hUninit = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hFeed = CreateEventW(NULL, FALSE, FALSE, NULL); /* for wasapi event mode */
    state->hForceFeed = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hFeedDone = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!state->init_done || !state->hFeed || !state->hUninit ||
        !state->hForceFeed || !state->hFeedDone)
    {
        closehandles(ao);
        /* failed to init events */
        return -1;
    }
    state->init_ret = -1;
    state->threadLoop = (HANDLE)CreateThread(NULL, 0, &ThreadLoop, ao, 0, NULL);
    if (!state->threadLoop) {
        /* failed to init thread */
        MP_ERR(ao, "fail to create thread!\n");
        return -1;
    }
    WaitForSingleObject(state->init_done, INFINITE); /* wait on init complete */
    if (state->init_ret) {
        if (!ao->probing) {
            MP_ERR(ao, "thread_init failed!\n");
        }
    } else
        MP_VERBOSE(ao, "Init Done!\n");

    wasapi_setup_proxies(state);
    return state->init_ret;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
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
            MP_VERBOSE(state, "mixer difference: %.2g now, expected %.2g\n",
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
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

    IAudioClient_Stop(state->pAudioClientProxy);
    IAudioClient_Reset(state->pAudioClientProxy);
    atomic_store(&state->sample_count, 0);
}

static void audio_resume(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

    SetEvent(state->hForceFeed);
    WaitForSingleObject(state->hFeedDone, INFINITE);
    IAudioClient_Start(state->pAudioClientProxy);
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    wasapi_enumerate_devices(mp_null_log, ao, list);
}

#define OPT_BASE_STRUCT struct wasapi_state

const struct ao_driver audio_out_wasapi = {
    .description = "Windows WASAPI audio output (event mode)",
    .name      = "wasapi",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    //.reset     = audio_reset, <- doesn't wait for audio callback to return
    .resume    = audio_resume,
    .list_devs = list_devs,
    .priv_size = sizeof(wasapi_state),
    .options   = (const struct m_option[]) {
        OPT_FLAG("exclusive", opt_exclusive, 0),
        OPT_FLAG("list", opt_list, 0),
        OPT_STRING_VALIDATE("device", opt_device, 0, wasapi_validate_device),
        {NULL},
    },
};
