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

#define COBJMACROS 1
#define _WIN32_WINNT 0x600

#include <stdlib.h>
#include <process.h>
#include <initguid.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <avrt.h>

#include "config.h"
#include "core/subopt-helper.h"
#include "audio/format.h"
#include "core/mp_msg.h"
#include "core/mp_ring.h"
#include "ao.h"

#define RING_BUFFER_COUNT 64

/* 20 millisecond buffer? */
#define BUFFER_TIME 20000000.0
#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto exit_label; }

/* Supposed to use __uuidof, but it is C++ only, declare our own */
static const GUID local_KSDATAFORMAT_SUBTYPE_PCM = {
    0x1, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

typedef struct wasapi0_state {
    HANDLE threadLoop;

    /* Init phase */
    int init_ret;
    HANDLE init_done;
    HANDLE fatal_error; /* signal to indicate unrecoverable error */

    /* Events */
    HANDLE hPause;

    /* Play */
    HANDLE hPlay;
    int is_playing;

    /* Reset */
    HANDLE hReset;

    /* uninit */
    HANDLE hUninit;
    LONG immed;

    /* volume control */
    HANDLE hGetvol, hSetvol, hDoneVol;
    DWORD vol_hw_support, status;
    float audio_volume;

    /* Prints, for in case line buffers are disabled */
    CRITICAL_SECTION print_lock;

    /* Buffers */
    struct mp_ring *ringbuff;
    size_t buffer_block_size; /* Size of each block in bytes */
    REFERENCE_TIME
        minRequestedDuration; /* minimum wasapi buffer block size, in 100-nanosecond units */
    REFERENCE_TIME
        defaultRequestedDuration; /* default wasapi default block size, in 100-nanosecond units */
    UINT32 bufferFrameCount; /* wasapi buffer block size, number of frames, frame size at format.nBlockAlign */

    /* WASAPI handles, owned by other thread */
    IMMDeviceEnumerator *pEnumerator;
    IMMDevice *pDevice;
    IAudioClient *pAudioClient;
    IAudioRenderClient *pRenderClient;
    IAudioEndpointVolume *pEndpointVolume;
    HANDLE hFeed; /* wasapi event */
    HANDLE hTask; /* AV thread */
    DWORD taskIndex; /* AV task ID */
    WAVEFORMATEXTENSIBLE format;

    /* We still need to support XP, don't use these functions directly, blob owned by main thread */
    struct {
        HMODULE hAvrt;
        HANDLE (WINAPI *pAvSetMmThreadCharacteristicsW)(LPCWSTR, LPDWORD);
        WINBOOL (WINAPI *pAvRevertMmThreadCharacteristics)(HANDLE);
    } VistaBlob;
} wasapi0_state;

static int fill_VistaBlob(wasapi0_state *state)
{
    if (!state)
        return 1;
    HMODULE hkernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hkernel32)
        return 1;
    WINBOOL (WINAPI *pSetDllDirectory)(LPCWSTR lpPathName) =
        (WINBOOL (WINAPI *)(LPCWSTR))GetProcAddress(hkernel32, "SetDllDirectoryW");
    WINBOOL (WINAPI *pSetSearchPathMode)(DWORD Flags) =
        (WINBOOL (WINAPI *)(DWORD))GetProcAddress(hkernel32, "SetSearchPathMode");
    if (pSetSearchPathMode)
        pSetDllDirectory(L"");  /* Attempt to use safe search paths */
    if (pSetSearchPathMode)
        pSetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE);
    state->VistaBlob.hAvrt = LoadLibraryW(L"avrt.dll");
    if (!state->VistaBlob.hAvrt)
        goto exit_label;
    state->VistaBlob.pAvSetMmThreadCharacteristicsW =
        (HANDLE (WINAPI *)(LPCWSTR, LPDWORD))
            GetProcAddress(state->VistaBlob.hAvrt, "AvSetMmThreadCharacteristicsW");
    state->VistaBlob.pAvRevertMmThreadCharacteristics =
        (WINBOOL (WINAPI *)(HANDLE))
            GetProcAddress(state->VistaBlob.hAvrt, "AvRevertMmThreadCharacteristics");
    return 0;
exit_label:
    if (state->VistaBlob.hAvrt)
        FreeLibrary(state->VistaBlob.hAvrt);
    if (pSetSearchPathMode)
        pSetDllDirectory(NULL);
    return 1;
}

static const char *explain_err(const HRESULT hr)
{
    switch (hr) {
    case S_OK:
        return "S_OK";
    case AUDCLNT_E_NOT_INITIALIZED:
        return "AUDCLNT_E_NOT_INITIALIZED";
    case AUDCLNT_E_ALREADY_INITIALIZED:
        return "AUDCLNT_E_ALREADY_INITIALIZED";
    case AUDCLNT_E_WRONG_ENDPOINT_TYPE:
        return "AUDCLNT_E_WRONG_ENDPOINT_TYPE";
    case AUDCLNT_E_DEVICE_INVALIDATED:
        return "AUDCLNT_E_DEVICE_INVALIDATED";
    case AUDCLNT_E_NOT_STOPPED:
        return "AUDCLNT_E_NOT_STOPPED";
    case AUDCLNT_E_BUFFER_TOO_LARGE:
        return "AUDCLNT_E_BUFFER_TOO_LARGE";
    case AUDCLNT_E_OUT_OF_ORDER:
        return "AUDCLNT_E_OUT_OF_ORDER";
    case AUDCLNT_E_UNSUPPORTED_FORMAT:
        return "AUDCLNT_E_UNSUPPORTED_FORMAT";
    case AUDCLNT_E_INVALID_SIZE:
        return "AUDCLNT_E_INVALID_SIZE";
    case AUDCLNT_E_DEVICE_IN_USE:
        return "AUDCLNT_E_DEVICE_IN_USE";
    case AUDCLNT_E_BUFFER_OPERATION_PENDING:
        return "AUDCLNT_E_BUFFER_OPERATION_PENDING";
    case AUDCLNT_E_THREAD_NOT_REGISTERED:
        return "AUDCLNT_E_THREAD_NOT_REGISTERED";
    case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:
        return "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED";
    case AUDCLNT_E_ENDPOINT_CREATE_FAILED:
        return "AUDCLNT_E_ENDPOINT_CREATE_FAILED";
    case AUDCLNT_E_SERVICE_NOT_RUNNING:
        return "AUDCLNT_E_SERVICE_NOT_RUNNING";
    case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED:
        return "AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED";
    case AUDCLNT_E_EXCLUSIVE_MODE_ONLY:
        return "AUDCLNT_E_EXCLUSIVE_MODE_ONLY";
    case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL:
        return "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL";
    case AUDCLNT_E_EVENTHANDLE_NOT_SET:
        return "AUDCLNT_E_EVENTHANDLE_NOT_SET";
    case AUDCLNT_E_INCORRECT_BUFFER_SIZE:
        return "AUDCLNT_E_INCORRECT_BUFFER_SIZE";
    case AUDCLNT_E_BUFFER_SIZE_ERROR:
        return "AUDCLNT_E_BUFFER_SIZE_ERROR";
    case AUDCLNT_E_CPUUSAGE_EXCEEDED:
        return "AUDCLNT_E_CPUUSAGE_EXCEEDED";
    case AUDCLNT_E_BUFFER_ERROR:
        return "AUDCLNT_E_BUFFER_ERROR";
    case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:
        return "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";
    case AUDCLNT_E_INVALID_DEVICE_PERIOD:
        return "AUDCLNT_E_INVALID_DEVICE_PERIOD";
    case AUDCLNT_E_INVALID_STREAM_FLAG:
        return "AUDCLNT_E_INVALID_STREAM_FLAG";
    case AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE:
        return "AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE";
    case AUDCLNT_E_RESOURCES_INVALIDATED:
        return "AUDCLNT_E_RESOURCES_INVALIDATED";
    case AUDCLNT_S_BUFFER_EMPTY:
        return "AUDCLNT_S_BUFFER_EMPTY";
    case AUDCLNT_S_THREAD_ALREADY_REGISTERED:
        return "AUDCLNT_S_THREAD_ALREADY_REGISTERED";
    case AUDCLNT_S_POSITION_STALLED:
        return "AUDCLNT_S_POSITION_STALLED";
    default:
        return "<Unknown>";
    }
}

static void set_format(WAVEFORMATEXTENSIBLE *wformat, WORD bytepersample,
                       DWORD samplerate, WORD channels, DWORD chanmask)
{
    wformat->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE; /* Only PCM is supported */
    wformat->Format.nChannels = channels;
    wformat->Format.nSamplesPerSec = samplerate;
    wformat->Format.nAvgBytesPerSec = wformat->Format.nChannels *
                                      bytepersample *
                                      wformat->Format.nSamplesPerSec;
    wformat->Format.nBlockAlign = wformat->Format.nChannels * bytepersample;
    wformat->Format.wBitsPerSample = bytepersample * 8;
    wformat->Format.cbSize =
        22; /* must be at least 22 for WAVE_FORMAT_EXTENSIBLE */
    wformat->SubFormat = local_KSDATAFORMAT_SUBTYPE_PCM;
    wformat->Samples.wValidBitsPerSample = wformat->Format.wBitsPerSample;
    wformat->dwChannelMask = chanmask;
}

static HRESULT check_support(struct wasapi0_state *state,
                             const WAVEFORMATEXTENSIBLE *wformat)
{
    HRESULT hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                                AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                &(wformat->Format), NULL);
    if (hr != S_OK) {
        EnterCriticalSection(&state->print_lock);
        mp_msg(
            MSGT_AO, MSGL_ERR,
            "IAudioClient::IsFormatSupported failed with %s (%d at %ldHz %dchannels, channelmask = %lx)\n",
            explain_err(
                hr), wformat->Format.wBitsPerSample,
            wformat->Format.nSamplesPerSec,
            wformat->Format.nChannels, wformat->dwChannelMask);
        LeaveCriticalSection(&state->print_lock);
    }
    return hr;
}

static int enum_formats(struct ao *const ao)
{
    WAVEFORMATEXTENSIBLE wformat;
    DWORD chanmask;
    int bytes_per_sample;
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;

    /* as far as testing shows, only PCM 16/24LE (44100Hz - 192kHz) is supported
     * Tested on Realtek High Definition Audio, (Realtek Semiconductor Corp. 6.0.1.6312)
     * Drivers dated 2/18/2011
     */
    switch (ao->format) {
    case AF_FORMAT_S24_LE:
        bytes_per_sample = 3;
        break;
    case AF_FORMAT_S16_LE:
        bytes_per_sample = 2;
        break;
    default:
        bytes_per_sample = 0; /* unsupported, fail it */
    }

    chanmask = mp_chmap_to_waveext(&ao->channels);
    set_format(&wformat, bytes_per_sample, ao->samplerate, ao->channels.num, chanmask);
    /* See if chosen format works */
    if (check_support(state, &wformat) == S_OK) {
        state->format = wformat;
        ao->bps = wformat.Format.nAvgBytesPerSec;
        return 0;
    } else { /* Try default, 16bit @ 44.1kHz */
        EnterCriticalSection(&state->print_lock);
        mp_msg(MSGT_AO, MSGL_WARN, "Trying 16LE at 44100Hz\n");
        LeaveCriticalSection(&state->print_lock);
        set_format(&wformat, 2, 44100, ao->channels.num, chanmask);
        if (check_support(state, &wformat) == S_OK) {
            ao->samplerate = wformat.Format.nSamplesPerSec;
            ao->bps = wformat.Format.nAvgBytesPerSec;
            ao->format = AF_FORMAT_S16_LE;
            state->format = wformat;
            return 0;
        } else { /* Poor quality hardware? Try stereo mode */
            EnterCriticalSection(&state->print_lock);
            mp_msg(MSGT_AO, MSGL_WARN, "Trying Stereo\n");
            LeaveCriticalSection(&state->print_lock);
            mp_chmap_from_channels(&ao->channels, 2);
            wformat.Format.nChannels = 2;
            set_format(&wformat, 2, 44100, ao->channels.num,
                       mp_chmap_to_waveext(&ao->channels));
            if (check_support(state, &wformat) == S_OK) {
                ao->samplerate = wformat.Format.nSamplesPerSec;
                ao->bps = wformat.Format.nAvgBytesPerSec;
                ao->format = AF_FORMAT_S16_LE;
                state->format = wformat;
                return 0;
            }
        }
    }
    return -1;
}

static int fix_format(struct wasapi0_state *state)
{
    HRESULT hr;
    double offset = 0.5;

    /* cargo cult code to negotiate buffer block size, affected by hardware/drivers combinations,
       gradually grow it to 10s, by 0.5s, consider failure if it still doesn't work
     */
    hr = IAudioClient_GetDevicePeriod(state->pAudioClient,
                                      &state->defaultRequestedDuration,
                                      &state->minRequestedDuration);
reinit:
    hr = IAudioClient_Initialize(state->pAudioClient,
                                 AUDCLNT_SHAREMODE_EXCLUSIVE,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                 state->defaultRequestedDuration,
                                 state->defaultRequestedDuration,
                                 &(state->format.Format),
                                 NULL);
    /* something about buffer sizes on Win7, fixme might loop forever */
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        EnterCriticalSection(&state->print_lock);
        mp_msg(
            MSGT_AO, MSGL_ERR,
            "IAudioClient::Initialize negotiation failed with %s, used %lld * 100ns\n",
            explain_err(hr), state->defaultRequestedDuration);
        LeaveCriticalSection(&state->print_lock);
        if (offset > 10.0)
            goto exit_label;                /* is 10 enough to break out of the loop?*/
        IAudioClient_GetBufferSize(state->pAudioClient, &state->bufferFrameCount);
        state->defaultRequestedDuration =
            (REFERENCE_TIME)((BUFFER_TIME / state->format.Format.nSamplesPerSec *
                              state->bufferFrameCount) + offset);
        offset += 0.5;
        IAudioClient_Release(state->pAudioClient);
        state->pAudioClient = NULL;
        hr = IMMDeviceActivator_Activate(state->pDevice,
                                         &IID_IAudioClient, CLSCTX_ALL,
                                         NULL, (void **)&state->pAudioClient);
        goto reinit;
    }
    EXIT_ON_ERROR(hr)
    hr = IAudioClient_GetService(state->pAudioClient,
                                 &IID_IAudioRenderClient,
                                 (void **)&state->pRenderClient);
    EXIT_ON_ERROR(hr)
    if (!state->hFeed)
        goto exit_label;
    hr = IAudioClient_SetEventHandle(state->pAudioClient, state->hFeed);
    EXIT_ON_ERROR(hr)
    hr = IAudioClient_GetBufferSize(state->pAudioClient,
                                    &state->bufferFrameCount);
    EXIT_ON_ERROR(hr)
    state->buffer_block_size = state->format.Format.nBlockAlign *
                               state->bufferFrameCount;
    state->hTask =
        state->VistaBlob.pAvSetMmThreadCharacteristicsW(L"Pro Audio", &state->taskIndex);
    EnterCriticalSection(&state->print_lock);
    mp_msg(MSGT_AO, MSGL_V,
           "ao-wasapi: fix_format OK, using %lld byte buffer block size!\n",
           state->buffer_block_size);
    LeaveCriticalSection(&state->print_lock);
    return 0;
exit_label:
    EnterCriticalSection(&state->print_lock);
    mp_msg(MSGT_AO, MSGL_ERR,
           "ao-wasapi: fix_format fails with %s, failed to determine buffer block size!\n",
           explain_err(hr));
    LeaveCriticalSection(&state->print_lock);
    SetEvent(state->fatal_error);
    return 1;
}

static int thread_init(struct ao *ao)
{
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
    HRESULT hr;
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&state->pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(state->pEnumerator,
                                                     eRender, eConsole,
                                                     &state->pDevice);
    EXIT_ON_ERROR(hr)

    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioClient,
                                     CLSCTX_ALL, NULL, (void **)&state->pAudioClient);
    EXIT_ON_ERROR(hr)

    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioEndpointVolume,
                                     CLSCTX_ALL, NULL,
                                     (void **)&state->pEndpointVolume);
    EXIT_ON_ERROR(hr)
    IAudioEndpointVolume_QueryHardwareSupport(state->pEndpointVolume,
                                              &state->vol_hw_support);

    state->init_ret = enum_formats(ao); /* Probe support formats */
    if (state->init_ret)
        goto exit_label;
    if (!fix_format(state)) { /* now that we're sure what format to use */
        EnterCriticalSection(&state->print_lock);
        mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: thread_init OK!\n");
        LeaveCriticalSection(&state->print_lock);
        SetEvent(state->init_done);
        return state->init_ret;
    }
exit_label:
    EnterCriticalSection(&state->print_lock);
    mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: thread_init failed!\n");
    LeaveCriticalSection(&state->print_lock);
    state->init_ret = -1;
    SetEvent(state->init_done);
    return -1;
}

static void thread_pause(wasapi0_state *state)
{
    state->is_playing = 0;
    IAudioClient_Stop(state->pAudioClient);
}

/* force_feed - feed in even if available data is smaller than required buffer, to clear the buffer */
static void thread_feed(wasapi0_state *state,int force_feed)
{
    BYTE *pData;
    int buffer_size;
    HRESULT hr = IAudioRenderClient_GetBuffer(state->pRenderClient,
                                              state->bufferFrameCount, &pData);
    EXIT_ON_ERROR(hr)
    buffer_size = mp_ring_buffered(state->ringbuff);
    if( buffer_size > state->buffer_block_size) { /* OK to copy! */
        mp_ring_read(state->ringbuff, (unsigned char *)pData,
                     state->buffer_block_size);
    } else if(force_feed) {
        /* should be smaller than buffer block size by now */
        memset(pData,0,state->buffer_block_size);
        mp_ring_read(state->ringbuff, (unsigned char *)pData, buffer_size);
    } else {
        /* buffer underrun?! abort */
        hr = IAudioRenderClient_ReleaseBuffer(state->pRenderClient,
                                              state->bufferFrameCount,
                                              AUDCLNT_BUFFERFLAGS_SILENT);
        EXIT_ON_ERROR(hr)
        return;
    }
    hr = IAudioRenderClient_ReleaseBuffer(state->pRenderClient,
                                          state->bufferFrameCount, 0);
    EXIT_ON_ERROR(hr)
    return;
exit_label:
    EnterCriticalSection(&state->print_lock);
    mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: thread_feed fails with %lx!\n", hr);
    LeaveCriticalSection(&state->print_lock);
    return;
}

static void thread_play(wasapi0_state *state)
{
    thread_feed(state, 0);
    state->is_playing = 1;
    IAudioClient_Start(state->pAudioClient);
    return;
}

static void thread_reset(wasapi0_state *state)
{
    IAudioClient_Stop(state->pAudioClient);
    IAudioClient_Reset(state->pAudioClient);
    if (state->is_playing) {
        thread_play(state);
    }
}

static void thread_getVol(wasapi0_state *state)
{
    IAudioEndpointVolume_GetMasterVolumeLevelScalar(state->pEndpointVolume,
                                                    &state->audio_volume);
    SetEvent(state->hDoneVol);
}

static void thread_setVol(wasapi0_state *state)
{
    IAudioEndpointVolume_SetMasterVolumeLevelScalar(state->pEndpointVolume,
                                                    state->audio_volume, NULL);
    SetEvent(state->hDoneVol);
}

static void thread_uninit(wasapi0_state *state)
{
    if (!state->immed) {
        /* feed until empty */
        while (1) {
            if (WaitForSingleObject(state->hFeed,2000) == WAIT_OBJECT_0 &&
                mp_ring_buffered(state->ringbuff))
            {
                thread_feed(state, 1);
            } else
                break;
        }
    }
    if (state->pAudioClient)
        IAudioClient_Stop(state->pAudioClient);
    if (state->pRenderClient)
        IAudioRenderClient_Release(state->pRenderClient);
    if (state->pAudioClient)
        IAudioClient_Release(state->pAudioClient);
    if (state->pEnumerator)
        IMMDeviceEnumerator_Release(state->pEnumerator);
    if (state->pDevice)
        IMMDevice_Release(state->pDevice);
    if (state->hTask)
        state->VistaBlob.pAvRevertMmThreadCharacteristics(state->hTask);
    CoUninitialize();
    _endthreadex(0);
}

static unsigned int __stdcall ThreadLoop(void *lpParameter)
{
    struct ao *ao = lpParameter;
    int feedwatch = 0;
    if (!ao || !ao->priv)
        return -1;
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
    if (thread_init(ao))
        goto exit_label;

    DWORD waitstatus = WAIT_FAILED;
    HANDLE playcontrol[] =
        {state->hUninit, state->hPause, state->hReset, state->hGetvol,
         state->hSetvol, state->hPlay, state->hFeed, NULL};
    EnterCriticalSection(&state->print_lock);
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: Entering dispatch loop!\n");
    LeaveCriticalSection(&state->print_lock);
    while (1) { /* watch events, poll at least every 2 seconds */
        waitstatus = WaitForMultipleObjects(7, playcontrol, FALSE, 2000);
        switch (waitstatus) {
        case WAIT_OBJECT_0: /*shutdown*/
            feedwatch = 0;
            thread_uninit(state);
            goto exit_label;
        case (WAIT_OBJECT_0 + 1): /* pause */
            feedwatch = 0;
            thread_pause(state);
            break;
        case (WAIT_OBJECT_0 + 2): /* reset */
            feedwatch = 0;
            thread_reset(state);
            break;
        case (WAIT_OBJECT_0 + 3): /* getVolume */
            thread_getVol(state);
            break;
        case (WAIT_OBJECT_0 + 4): /* setVolume */
            thread_setVol(state);
            break;
        case (WAIT_OBJECT_0 + 5): /* play */
            feedwatch = 0;
            thread_play(state);
            break;
        case (WAIT_OBJECT_0 + 6): /* feed */
            feedwatch = 1;
            thread_feed(state, 0);
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
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
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

static int get_space(struct ao *ao)
{
    if (!ao || !ao->priv)
        return -1;
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
    return mp_ring_available(state->ringbuff);
}

static void reset_buffers(struct wasapi0_state *state)
{
    mp_ring_reset(state->ringbuff);
}

static int setup_buffers(struct wasapi0_state *state)
{
    state->ringbuff =
        mp_ring_new(state, RING_BUFFER_COUNT * state->buffer_block_size);
    return !state->ringbuff;
}

static void uninit(struct ao *ao, bool immed)
{
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi0: uninit!\n");
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
    state->immed = immed;
    SetEvent(state->hUninit);
    /* wait up to 10 seconds */
    if (WaitForSingleObject(state->threadLoop, 10000) == WAIT_TIMEOUT)
        SetEvent(state->fatal_error);
    if (state->VistaBlob.hAvrt)
        FreeLibrary(state->VistaBlob.hAvrt);
    closehandles(ao);
    DeleteCriticalSection(&state->print_lock);
    talloc_free(state);
    ao->priv = NULL;
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi0: uninit END!\n");
}

static int init(struct ao *ao, char *params)
{
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi0: init!\n");
    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        return -1;
    struct wasapi0_state *state = talloc_zero(ao, struct wasapi0_state);
    if (!state)
        return -1;
    ao->priv = (void *)state;
    fill_VistaBlob(state);
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
    InitializeCriticalSection(&state->print_lock);
    if (!state->init_done || !state->fatal_error || !state->hPlay ||
        !state->hPause || !state->hFeed || !state->hReset || !state->hGetvol ||
        !state->hSetvol || !state->hDoneVol)
    {
        closehandles(ao);
        /* failed to init events */
        return -1;
    }
    state->init_ret = -1;
    state->threadLoop = (HANDLE)_beginthreadex(NULL, 0, &ThreadLoop, ao, 0, NULL);
    if (!state->threadLoop) {
        /* failed to init thread */
        mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi0: fail to create thread!\n");
        return -1;
    }
    WaitForSingleObject(state->init_done, INFINITE); /* wait on init complete */
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi0: Init Done!\n");
    if (setup_buffers(state))
        mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi0: buffer setup failed!\n");
    return state->init_ret;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
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
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
    ResetEvent(state->hPause);
    ResetEvent(state->hReset);
    SetEvent(state->hPlay);
}

static void audio_pause(struct ao *ao)
{
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
    ResetEvent(state->hPlay);
    SetEvent(state->hPause);
}

static void reset(struct ao *ao)
{
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
    ResetEvent(state->hPlay);
    SetEvent(state->hReset);
    reset_buffers(state);
}

static int play(struct ao *ao, void *data, int len, int flags)
{
    int ret = 0;
    if (!ao || !ao->priv)
        return ret;
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
    if (WaitForSingleObject(state->fatal_error, 0) == WAIT_OBJECT_0) {
        /* something bad happened */
        return ret;
    }

    ret = mp_ring_write(state->ringbuff, data, len);

    if (!state->is_playing) {
        /* start playing */
        state->is_playing = 1;
        SetEvent(state->hPlay);
    }
    return ret;
}

static float get_delay(struct ao *ao)
{
    if (!ao || !ao->priv)
        return -1.0f;
    struct wasapi0_state *state = (struct wasapi0_state *)ao->priv;
    return (float)(RING_BUFFER_COUNT * state->buffer_block_size - get_space(ao)) /
           (float)state->format.Format.nAvgBytesPerSec;
}

const struct ao_driver audio_out_wasapi0 = {
    .info = &(const struct ao_info) {
        "Windows WASAPI audio output (event mode)",
        "wasapi0",
        "Jonathan Yong <10walls@gmail.com>",
        ""
    },
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .reset     = reset,
};
