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

#include "options/m_option.h"
#include "options/m_config.h"
#include "audio/format.h"
#include "common/msg.h"
#include "misc/ring.h"
#include "ao.h"
#include "internal.h"
#include "compat/atomics.h"
#include "osdep/timer.h"

#define EXIT_ON_ERROR(hres)  \
              do { if (FAILED(hres)) { goto exit_label; } } while(0)
#define SAFE_RELEASE(unk, release) \
              do { if ((unk) != NULL) { release; (unk) = NULL; } } while(0)

static int thread_init(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    HRESULT hr;
    CoInitialize(NULL);

    if (!state->opt_device) {
        IMMDeviceEnumerator *pEnumerator;
        hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                              &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
        EXIT_ON_ERROR(hr);

        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnumerator,
                                                         eRender, eConsole,
                                                         &state->pDevice);
        SAFE_RELEASE(pEnumerator, IMMDeviceEnumerator_Release(pEnumerator));

        char *id = wasapi_get_device_id(state->pDevice);
        MP_VERBOSE(ao, "default device ID: %s\n", id);
        free(id);
    } else {
        hr = wasapi_find_and_load_device(ao, &state->pDevice, state->opt_device);
    }
    EXIT_ON_ERROR(hr);

    char *name = wasapi_get_device_name(state->pDevice);
    MP_VERBOSE(ao, "device loaded: %s\n", name);
    free(name);

    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioClient,
                                     CLSCTX_ALL, NULL, (void **)&state->pAudioClient);
    EXIT_ON_ERROR(hr);

    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioEndpointVolume,
                                     CLSCTX_ALL, NULL,
                                     (void **)&state->pEndpointVolume);
    EXIT_ON_ERROR(hr);
    IAudioEndpointVolume_QueryHardwareSupport(state->pEndpointVolume,
                                              &state->vol_hw_support);

    state->init_ret = wasapi_find_formats(ao); /* Probe support formats */
    if (state->init_ret)
        goto exit_label;
    if (!wasapi_fix_format(state)) { /* now that we're sure what format to use */
        MP_VERBOSE(ao, "thread_init OK!\n");
        SetEvent(state->init_done);
        return state->init_ret;
    }
exit_label:
    state->init_ret = -1;
    SetEvent(state->init_done);
    return -1;
}


static double get_device_delay(struct wasapi_state *state) {
    UINT64 sample_count = state->sample_count;
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

    state->sample_count += frame_count;

    return;
exit_label:
    MP_ERR(state, "thread_feed fails with %"PRIx32"!\n", (uint32_t)hr);
    return;
}

static void thread_pause(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

    state->is_playing = 0;
    IAudioClient_Stop(state->pAudioClient);
    IAudioClient_Reset(state->pAudioClient);
    state->sample_count = 0;
    mp_memory_barrier();
}

static void thread_resume(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

    state->is_playing = 1;
    thread_feed(ao);
    IAudioClient_Start(state->pAudioClient);
}

static void thread_reset(struct ao *ao)
{
    thread_pause(ao);
}

static void thread_getVol(wasapi_state *state)
{
    IAudioEndpointVolume_GetMasterVolumeLevelScalar(state->pEndpointVolume,
                                                    &state->audio_volume);
    SetEvent(state->hDoneVol);
}

static void thread_setVol(wasapi_state *state)
{
    IAudioEndpointVolume_SetMasterVolumeLevelScalar(state->pEndpointVolume,
                                                    state->audio_volume, NULL);
    SetEvent(state->hDoneVol);
}

static void thread_uninit(wasapi_state *state)
{
    if (state->pAudioClient)
        IAudioClient_Stop(state->pAudioClient);
    if (state->pRenderClient)
        IAudioRenderClient_Release(state->pRenderClient);
    if (state->pAudioClock)
        IAudioClock_Release(state->pAudioClock);
    if (state->pAudioClient)
        IAudioClient_Release(state->pAudioClient);
    if (state->pDevice)
        IMMDevice_Release(state->pDevice);
    if (state->hTask)
        state->VistaBlob.pAvRevertMmThreadCharacteristics(state->hTask);
    CoUninitialize();
    ExitThread(0);
}

static void audio_drain(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    while (1) {
        if (WaitForSingleObject(state->hFeed,2000) == WAIT_OBJECT_0 &&
            ao->api->get_delay(ao))
        {
            thread_feed(ao);
        } else
            break;
    }
}

static DWORD __stdcall ThreadLoop(void *lpParameter)
{
    struct ao *ao = lpParameter;
    int feedwatch = 0;
    if (!ao || !ao->priv)
        return -1;
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    if (thread_init(ao))
        goto exit_label;

    DWORD waitstatus = WAIT_FAILED;
    HANDLE playcontrol[] =
        {state->hUninit, state->hPause, state->hReset, state->hGetvol,
         state->hSetvol, state->hPlay, state->hFeed, NULL};
    MP_VERBOSE(ao, "Entering dispatch loop!\n");
    while (1) { /* watch events, poll at least every 2 seconds */
        waitstatus = WaitForMultipleObjects(7, playcontrol, FALSE, 2000);
        switch (waitstatus) {
        case WAIT_OBJECT_0: /*shutdown*/
            feedwatch = 0;
            thread_uninit(state);
            goto exit_label;
        case (WAIT_OBJECT_0 + 1): /* pause */
            feedwatch = 0;
            thread_pause(ao);
            break;
        case (WAIT_OBJECT_0 + 2): /* reset */
            feedwatch = 0;
            thread_reset(ao);
            break;
        case (WAIT_OBJECT_0 + 3): /* getVolume */
            thread_getVol(state);
            break;
        case (WAIT_OBJECT_0 + 4): /* setVolume */
            thread_setVol(state);
            break;
        case (WAIT_OBJECT_0 + 5): /* play */
            feedwatch = 0;
            thread_resume(ao);
            break;
        case (WAIT_OBJECT_0 + 6): /* feed */
            if (state->is_playing)
                feedwatch = 1;
            thread_feed(ao);
            break;
        case WAIT_TIMEOUT: /* Did our feed die? */
            if (feedwatch)
                return -1;
            break;
        default:
        case WAIT_FAILED: /* ??? */
            return -1;
        }
    }
exit_label:
    return state->init_ret;
}

static void closehandles(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    if (state->init_done)
        CloseHandle(state->init_done);
    if (state->hPlay)
        CloseHandle(state->hPlay);
    if (state->hPause)
        CloseHandle(state->hPause);
    if (state->hReset)
        CloseHandle(state->hReset);
    if (state->hUninit)
        CloseHandle(state->hUninit);
    if (state->hFeed)
        CloseHandle(state->hFeed);
    if (state->hGetvol)
        CloseHandle(state->hGetvol);
    if (state->hSetvol)
        CloseHandle(state->hSetvol);
    if (state->hDoneVol)
        CloseHandle(state->hDoneVol);
}

static void uninit(struct ao *ao)
{
    MP_VERBOSE(ao, "uninit!\n");
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    state->immed = 1;
    SetEvent(state->hUninit);
    /* wait up to 10 seconds */
    if (WaitForSingleObject(state->threadLoop, 10000) == WAIT_TIMEOUT)
        SetEvent(state->fatal_error);
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
        wasapi_enumerate_devices(state->log);
    }

    if (state->opt_exclusive) {
        state->share_mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
    } else {
        state->share_mode = AUDCLNT_SHAREMODE_SHARED;
    }

    state->init_done = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hPlay = CreateEventW(NULL, FALSE, FALSE, NULL); /* kick start audio feed */
    state->hPause = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hReset = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hGetvol = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hSetvol = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hDoneVol = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->hUninit = CreateEventW(NULL, FALSE, FALSE, NULL);
    state->fatal_error = CreateEventW(NULL, TRUE, FALSE, NULL);
    state->hFeed = CreateEvent(NULL, FALSE, FALSE, NULL); /* for wasapi event mode */
    if (!state->init_done || !state->fatal_error || !state->hPlay ||
        !state->hPause || !state->hFeed || !state->hReset || !state->hGetvol ||
        !state->hSetvol || !state->hDoneVol)
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
    return state->init_ret;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    if (!(state->vol_hw_support & ENDPOINT_HARDWARE_SUPPORT_VOLUME))
        return CONTROL_UNKNOWN;  /* hw does not support volume controls in exclusive mode */

    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
    ResetEvent(state->hDoneVol);

    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        SetEvent(state->hGetvol);
        if (WaitForSingleObject(state->hDoneVol, 100) == WAIT_OBJECT_0) {
            vol->left = vol->right = 100.0f * state->audio_volume;
            return CONTROL_OK;
        }
        return CONTROL_UNKNOWN;
    case AOCONTROL_SET_VOLUME:
        state->audio_volume = vol->left / 100.f;
        SetEvent(state->hSetvol);
        if (WaitForSingleObject(state->hDoneVol, 100) == WAIT_OBJECT_0)
            return CONTROL_OK;
        return CONTROL_UNKNOWN;
    default:
        return CONTROL_UNKNOWN;
    }
}

static void audio_resume(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    ResetEvent(state->hPause);
    ResetEvent(state->hReset);
    SetEvent(state->hPlay);
}

static void audio_pause(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    ResetEvent(state->hPlay);
    SetEvent(state->hPause);
}

static void reset(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    ResetEvent(state->hPlay);
    SetEvent(state->hReset);
}

#define OPT_BASE_STRUCT struct wasapi_state

const struct ao_driver audio_out_wasapi = {
    .description = "Windows WASAPI audio output (event mode)",
    .name      = "wasapi",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .reset     = reset,
    .drain     = audio_drain,
    .priv_size = sizeof(wasapi_state),
    .options   = (const struct m_option[]) {
        OPT_FLAG("exclusive", opt_exclusive, 0),
        OPT_FLAG("list", opt_list, 0),
        OPT_STRING_VALIDATE("device", opt_device, 0, wasapi_validate_device),
        {NULL},
    },
};
