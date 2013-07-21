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
#include <inttypes.h>
#include <process.h>
#include <initguid.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <avrt.h>

#include "config.h"
#include "core/subopt-helper.h"
#include "core/m_option.h"
#include "core/m_config.h"
#include "audio/format.h"
#include "core/mp_msg.h"
#include "core/mp_ring.h"
#include "ao.h"

#ifndef BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE
#define BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE 0x00000001
#endif

#ifndef PKEY_Device_FriendlyName
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName,
                   0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20,
                   0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
#endif

#define RING_BUFFER_COUNT 64

/* 20 millisecond buffer? */
#define BUFFER_TIME 20000000.0
#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto exit_label; }
#define SAFE_RELEASE(unk, release) \
              if ((unk) != NULL) { release; (unk) = NULL; }

/* Supposed to use __uuidof, but it is C++ only, declare our own */
static const GUID local_KSDATAFORMAT_SUBTYPE_PCM = {
    0x1, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};
static const GUID local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {
    0x3, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

union WAVEFMT {
    WAVEFORMATEX *ex;
    WAVEFORMATEXTENSIBLE *extensible;
};

typedef struct wasapi_state {
    HANDLE threadLoop;

    /* Init phase */
    int init_ret;
    HANDLE init_done;
    HANDLE fatal_error; /* signal to indicate unrecoverable error */
    int share_mode;

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

    int opt_exclusive;
    int opt_list;
    char *opt_device;

    /* We still need to support XP, don't use these functions directly, blob owned by main thread */
    struct {
        HMODULE hAvrt;
        HANDLE (WINAPI *pAvSetMmThreadCharacteristicsW)(LPCWSTR, LPDWORD);
        WINBOOL (WINAPI *pAvRevertMmThreadCharacteristics)(HANDLE);
    } VistaBlob;
} wasapi_state;

static int fill_VistaBlob(wasapi_state *state)
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
    if (bytepersample == 4)
        wformat->SubFormat = local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    else
        wformat->SubFormat = local_KSDATAFORMAT_SUBTYPE_PCM;
    wformat->Samples.wValidBitsPerSample = wformat->Format.wBitsPerSample;
    wformat->dwChannelMask = chanmask;
}

static int format_set_bits(int old_format, int bits, int fp) {
    int format = old_format;
    format &= (~AF_FORMAT_BITS_MASK) & (~AF_FORMAT_POINT_MASK) & (~AF_FORMAT_SIGN_MASK);
    format |= AF_FORMAT_SI;

    switch (bits) {
    case 32:
        format |= AF_FORMAT_32BIT;
        break;
    case 24:
        format |= AF_FORMAT_24BIT;
        break;
    case 16:
        format |= AF_FORMAT_16BIT;
        break;
    default:
        abort(); // (should be) unreachable
    }

    if (fp) {
        format |= AF_FORMAT_F;
    } else {
        format |= AF_FORMAT_I;
    }

    return format;
}

static int set_ao_format(struct wasapi_state *state,
                         struct ao *const ao,
                         WAVEFORMATEXTENSIBLE wformat) {
    // .Data1 == 1 is PCM, .Data1 == 3 is IEEE_FLOAT
    int format = format_set_bits(ao->format,
        wformat.Format.wBitsPerSample, wformat.SubFormat.Data1 == 3);

    if (wformat.SubFormat.Data1 != 1 && wformat.SubFormat.Data1 != 3) {
        mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: unknown SubFormat %d\n",
            wformat.SubFormat.Data1);
        return 0;
    }

    ao->samplerate = wformat.Format.nSamplesPerSec;
    ao->bps = wformat.Format.nAvgBytesPerSec;
    ao->format = format;

    if (ao->channels.num != wformat.Format.nChannels) {
        mp_chmap_from_channels(&ao->channels, wformat.Format.nChannels);
    }

    state->format = wformat;
    return 1;
}

static int try_format(struct wasapi_state *state,
                      struct ao *const ao,
                      int bits, int samplerate,
                      const struct mp_chmap channels)
{
    WAVEFORMATEXTENSIBLE wformat;
    set_format(&wformat, bits / 8, samplerate, channels.num, mp_chmap_to_waveext(&channels));

    int af_format = format_set_bits(ao->format, bits, bits == 32);

    EnterCriticalSection(&state->print_lock);
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: trying %dch %s @ %dhz\n",
           channels.num, af_fmt2str_short(af_format), samplerate);
    LeaveCriticalSection(&state->print_lock);

    union WAVEFMT u;
    u.extensible = &wformat;

    WAVEFORMATEX *closestMatch;
    HRESULT hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                                state->share_mode,
                                                u.ex, &closestMatch);

    if (closestMatch) {
        if (closestMatch->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            u.ex = closestMatch;
            wformat = *u.extensible;
        } else {
            wformat.Format = *closestMatch;
        }

        CoTaskMemFree(closestMatch);
    }

    if (hr == S_FALSE) {
        if (set_ao_format(state, ao, wformat)) {
            EnterCriticalSection(&state->print_lock);
            mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: accepted as %dch %s @ %dhz\n",
                ao->channels.num, af_fmt2str_short(ao->format), ao->samplerate);
            LeaveCriticalSection(&state->print_lock);

            return 1;
        }
    } if (hr == S_OK || (!state->opt_exclusive && hr == AUDCLNT_E_UNSUPPORTED_FORMAT)) {
        // AUDCLNT_E_UNSUPPORTED_FORMAT here means "works in shared, doesn't in exclusive"
        if (set_ao_format(state, ao, wformat)) {
            EnterCriticalSection(&state->print_lock);
            mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: %dch %s @ %dhz accepted\n",
                   ao->channels.num, af_fmt2str_short(af_format), samplerate);
            LeaveCriticalSection(&state->print_lock);
            return 1;
        }
    }
    return 0;
}

static int try_mix_format(struct wasapi_state *state,
                          struct ao *const ao)
{
    WAVEFORMATEX *deviceFormat = NULL;
    WAVEFORMATEX *closestMatch = NULL;
    int ret = 0;

    HRESULT hr = IAudioClient_GetMixFormat(state->pAudioClient, &deviceFormat);
    EXIT_ON_ERROR(hr)

    union WAVEFMT u;
    u.ex = deviceFormat;
    WAVEFORMATEXTENSIBLE wformat = *u.extensible;

    ret = try_format(state, ao, wformat.Format.wBitsPerSample,
                     wformat.Format.nSamplesPerSec, ao->channels);
    if (ret)
        state->format = wformat;

exit_label:
    SAFE_RELEASE(deviceFormat, CoTaskMemFree(deviceFormat));
    SAFE_RELEASE(closestMatch, CoTaskMemFree(closestMatch));
    return ret;
}

static int try_passthrough(struct wasapi_state *state,
                           struct ao *const ao)
{
    WAVEFORMATEXTENSIBLE wformat = {
        .Format = {
            .wFormatTag = WAVE_FORMAT_EXTENSIBLE,
            .nChannels = ao->channels.num,
            .nSamplesPerSec = ao->samplerate * 4,
            .nAvgBytesPerSec = (ao->samplerate * 4) * (ao->channels.num * 2),
            .nBlockAlign = ao->channels.num * 2,
            .wBitsPerSample = 16,
            .cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX),
        },
        .Samples.wValidBitsPerSample = 16,
        .dwChannelMask = mp_chmap_to_waveext(&ao->channels),
        .SubFormat = local_KSDATAFORMAT_SUBTYPE_PCM,
    };
    wformat.SubFormat.Data1 = WAVE_FORMAT_DOLBY_AC3_SPDIF; // see INIT_WAVEFORMATEX_GUID macro

    union WAVEFMT u;
    u.extensible = &wformat;

    HRESULT hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                                state->share_mode,
                                                u.ex, NULL);
    if (!FAILED(hr)) {
        state->format = wformat;
        return 1;
    }
    return 0;
}

static int find_formats(struct ao *const ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

    if (AF_FORMAT_IS_IEC61937(ao->format)) {
        if (try_passthrough(state, ao))
            return 0;

        EnterCriticalSection(&state->print_lock);
        mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: couldn't use passthrough!");
        if (!state->opt_exclusive)
            mp_msg(MSGT_AO, MSGL_ERR, " (try exclusive mode)");
        mp_msg(MSGT_AO, MSGL_ERR, "\n");
        LeaveCriticalSection(&state->print_lock);
        return -1;
    }

    /* See if the format works as-is */
    int bits = af_fmt2bits(ao->format);
    /* don't try 8bits -- there are various 8bit modes other than PCM (*-law et al);
       let's just stick to PCM or float here. */
    if (bits == 8) {
        bits = 16;
    } else if (try_format(state, ao, bits, ao->samplerate, ao->channels)) {
        return 0;
    }
    if (!state->opt_exclusive) {
        /* shared mode, we can use the system default mix format. */
        if (try_mix_format(state, ao)) {
            return 0;
        }

        EnterCriticalSection(&state->print_lock);
        mp_msg(MSGT_AO, MSGL_WARN, "ao-wasapi: couldn't use default mix format!\n");
        LeaveCriticalSection(&state->print_lock);
    }

    /* Exclusive mode, we have to guess. */

    /* as far as testing shows, only PCM 16/24LE (44100Hz - 192kHz) is supported
     * Tested on Realtek High Definition Audio, (Realtek Semiconductor Corp. 6.0.1.6312)
     * Drivers dated 2/18/2011
     */

    /* try float first for non-16bit audio */
    if (bits != 16) {
        bits = 32;
    }

    int start_bits = bits;
    while (1) { // not infinite -- returns at bottom
        for (; bits > 8; bits -= 8) {
            int samplerate = ao->samplerate;
            if (try_format(state, ao, bits, samplerate, ao->channels)) {
                return 0;
            }

            // make samplerate fit in [44100 192000]
            // we check for samplerate > 96k so that we can upsample instead of downsampling later
            if (samplerate < 44100 || samplerate > 96000) {
                if (samplerate < 44100)
                    samplerate = 44100;
                if (samplerate > 96000)
                    samplerate = 192000;

                if (try_format(state, ao, bits, samplerate, ao->channels)) {
                    return 0;
                }
            }

            // try bounding to 96kHz
            if (samplerate > 48000) {
                samplerate = 96000;
                if (try_format(state, ao, bits, samplerate, ao->channels)) {
                    return 0;
                }
            }

            // try bounding to 48kHz
            if (samplerate > 44100) {
                samplerate = 48000;
                if (try_format(state, ao, bits, samplerate, ao->channels)) {
                    return 0;
                }
            }

            /* How bad is this? try 44100hz, but only on 16bit */
            if (bits == 16 && samplerate != 44100) {
                samplerate = 44100;

                if (try_format(state, ao, bits, samplerate, ao->channels)) {
                    return 0;
                }
            }
        }
        if (ao->channels.num > 6) {
            /* Maybe this is 5.1 hardware with no support for more. */
            bits = start_bits;
            mp_chmap_from_channels(&ao->channels, 6);
        } else if (ao->channels.num != 2) {
            /* Poor quality hardware? Try stereo mode, go through the list again. */
            bits = start_bits;
            mp_chmap_from_channels(&ao->channels, 2);
        } else {
            EnterCriticalSection(&state->print_lock);
            mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: couldn't find acceptable audio format!\n");
            LeaveCriticalSection(&state->print_lock);
            return -1;
        }
    }
}

static int fix_format(struct wasapi_state *state)
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
                                 state->share_mode,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                 state->defaultRequestedDuration,
                                 state->defaultRequestedDuration,
                                 &(state->format.Format),
                                 NULL);
    /* something about buffer sizes on Win7, fixme might loop forever */
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        EnterCriticalSection(&state->print_lock);
        mp_msg(
            MSGT_AO, MSGL_V,
            "ao-wasapi: IAudioClient::Initialize negotiation failed with %s, used %lld * 100ns\n",
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
    state->buffer_block_size = state->format.Format.nChannels *
                               state->format.Format.wBitsPerSample / 8 *
                               state->bufferFrameCount;
    state->hTask =
        state->VistaBlob.pAvSetMmThreadCharacteristicsW(L"Pro Audio", &state->taskIndex);
    EnterCriticalSection(&state->print_lock);
    mp_msg(MSGT_AO, MSGL_V,
           "ao-wasapi: fix_format OK, using %lld byte buffer block size!\n",
           (long long) state->buffer_block_size);
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

static char* wstring_to_utf8(wchar_t *wstring) {
    if (wstring) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wstring, -1, NULL, 0, NULL, NULL);
        char *ret = malloc(len);
        WideCharToMultiByte(CP_UTF8, 0, wstring, -1, ret, len, NULL, NULL);
        return ret;
    }
    return NULL;
}

static char* get_device_id(IMMDevice *pDevice) {
    LPWSTR devid = NULL;
    char *idstr = NULL;

    HRESULT hr = IMMDevice_GetId(pDevice, &devid);
    EXIT_ON_ERROR(hr)

    idstr = wstring_to_utf8(devid);

    if (strstr(idstr, "{0.0.0.00000000}.")) {
        char *stripped = strdup(idstr + strlen("{0.0.0.00000000}."));
        free(idstr);
        idstr = stripped;
    }

exit_label:
    SAFE_RELEASE(devid, CoTaskMemFree(devid));
    return idstr;
}

static char* get_device_name(IMMDevice *pDevice) {
    IPropertyStore *pProps = NULL;
    char *namestr = NULL;

    HRESULT hr = IMMDevice_OpenPropertyStore(pDevice, STGM_READ, &pProps);
    EXIT_ON_ERROR(hr)

    PROPVARIANT devname;
    PropVariantInit(&devname);

    hr = IPropertyStore_GetValue(pProps, &PKEY_Device_FriendlyName, &devname);
    EXIT_ON_ERROR(hr)

    namestr = wstring_to_utf8(devname.pwszVal);

exit_label:
    PropVariantClear(&devname);
    SAFE_RELEASE(pProps, IPropertyStore_Release(pProps));
    return namestr;
}

// frees *idstr
static int device_id_match(char *idstr, char *candidate) {
    if (idstr == NULL || candidate == NULL)
        return 0;

    int found = 0;
#define FOUND(x) do { found = (x); goto end; } while(0)
    if (strcmp(idstr, candidate) == 0)
        FOUND(1);
    if (strstr(idstr, "{0.0.0.00000000}.")) {
        char *start = idstr + strlen("{0.0.0.00000000}.");
        if (strcmp(start, candidate) == 0)
            FOUND(1);
    }
#undef FOUND
end:
    free(idstr);
    return found;
}

static HRESULT enumerate_with_state(char *header, int status, int with_id) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDeviceCollection *pDevices = NULL;
    IMMDevice *pDevice = NULL;
    IPropertyStore *pProps = NULL;
    char *defid = NULL;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnumerator,
                                                     eRender, eConsole,
                                                     &pDevice);
    EXIT_ON_ERROR(hr)

    defid = get_device_id(pDevice);

    SAFE_RELEASE(pDevice, IMMDevice_Release(pDevice));

    hr = IMMDeviceEnumerator_EnumAudioEndpoints(pEnumerator, eRender,
                                                status, &pDevices);
    EXIT_ON_ERROR(hr)

    int count;
    IMMDeviceCollection_GetCount(pDevices, &count);
    if (count > 0) {
        mp_msg(MSGT_AO, MSGL_INFO, "ao-wasapi: %s\n", header);
    }

    for (int i = 0; i < count; i++) {
        hr = IMMDeviceCollection_Item(pDevices, i, &pDevice);
        EXIT_ON_ERROR(hr)

        char *name = get_device_name(pDevice);
        char *id = get_device_id(pDevice);

        char *mark = "";
        if (strcmp(id, defid) == 0)
            mark = " (default)";

        if (with_id) {
            mp_msg(MSGT_AO, MSGL_INFO, "ao-wasapi: Device #%d: %s, ID: %s%s\n",
                i, name, id, mark);
        } else {
            mp_msg(MSGT_AO, MSGL_INFO, "ao-wasapi: %s, ID: %s%s\n",
                name, id, mark);
        }

        free(name);
        free(id);
        SAFE_RELEASE(pDevice, IMMDevice_Release(pDevice));
    }
    free(defid);
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices));
    SAFE_RELEASE(pEnumerator, IMMDeviceEnumerator_Release(pEnumerator));
    return hr;

exit_label:
    free(defid);
    SAFE_RELEASE(pProps, IPropertyStore_Release(pProps));
    SAFE_RELEASE(pDevice, IMMDevice_Release(pDevice));
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices));
    SAFE_RELEASE(pEnumerator, IMMDeviceEnumerator_Release(pEnumerator));
    return hr;
}

static int enumerate_devices(void) {
    HRESULT hr;
    hr = enumerate_with_state("Active devices:", DEVICE_STATE_ACTIVE, 1);
    EXIT_ON_ERROR(hr)
    hr = enumerate_with_state("Unplugged devices:", DEVICE_STATE_UNPLUGGED, 0);
    EXIT_ON_ERROR(hr)
    return 0;
exit_label:
    mp_msg(MSGT_AO, MSGL_ERR, "Error enumerating devices: HRESULT %08x \"%s\"\n",
        hr, explain_err(hr));
    return 1;
}

static HRESULT find_and_load_device(wasapi_state *state, int devno, char *devid) {
    HRESULT hr;
    IMMDeviceCollection *pDevices = NULL;
    IMMDevice *pTempDevice = NULL;
    LPWSTR deviceID = NULL;

    hr = IMMDeviceEnumerator_EnumAudioEndpoints(state->pEnumerator, eRender,
                                                DEVICE_STATE_ACTIVE, &pDevices);
    EXIT_ON_ERROR(hr)

    int count;
    IMMDeviceCollection_GetCount(pDevices, &count);

    if (devid == NULL) {
        if (devno >= count) {
            mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: no endpoind #%d!\n", devno);
        } else {
            mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: finding device #%d\n", devno);
            hr = IMMDeviceCollection_Item(pDevices, devno, &pTempDevice);
            EXIT_ON_ERROR(hr)

            hr = IMMDevice_GetId(pTempDevice, &deviceID);
            EXIT_ON_ERROR(hr)

            mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: found device #%d\n", devno);
        }
    } else {
        mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: finding device %s\n", devid);

        for (int i = 0; i < count; i++) {
            hr = IMMDeviceCollection_Item(pDevices, i, &pTempDevice);
            EXIT_ON_ERROR(hr)

            if (device_id_match(get_device_id(pTempDevice), devid)) {
                hr = IMMDevice_GetId(pTempDevice, &deviceID);
                EXIT_ON_ERROR(hr)
                break;
            }

            SAFE_RELEASE(pTempDevice, IMMDevice_Release(pTempDevice));
        }
        if (deviceID == NULL) {
            mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: could not find device %s!\n", devid);
        }
    }

    SAFE_RELEASE(pTempDevice, IMMDevice_Release(pTempDevice));
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices))

    if (deviceID == NULL) {
        mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: no device to load!\n");
    } else {
        mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: loading device %S\n", deviceID);

        hr = IMMDeviceEnumerator_GetDevice(state->pEnumerator, deviceID, &state->pDevice);

        if (FAILED(hr)) {
            mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: could not load requested device!\n");
        }
    }

exit_label:
    SAFE_RELEASE(pTempDevice, IMMDevice_Release(pTempDevice));
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices));
    return hr;
}

static int thread_init(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    HRESULT hr;
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&state->pEnumerator);
    EXIT_ON_ERROR(hr)

    if (!state->opt_device) {
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(state->pEnumerator,
                                                         eRender, eConsole,
                                                         &state->pDevice);

        char *id = get_device_id(state->pDevice);
        mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: default device ID: %s\n", id);
        free(id);
    } else {
        int devno = -1;
        char *devid = NULL;

        if (state->opt_device[0] == '{') { // ID as printed by list
            devid = state->opt_device;
        } else { // assume integer
            char *end;
            devno = (int) strtol(state->opt_device, &end, 10);

            if (*end != '\0' || devno < 0) {
                mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: invalid device number %s!", state->opt_device);
                goto exit_label;
            }
        }
        hr = find_and_load_device(state, devno, devid);
    }
    EXIT_ON_ERROR(hr)

    char *name = get_device_name(state->pDevice);
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: device loaded: %s\n", name);
    free(name);

    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioClient,
                                     CLSCTX_ALL, NULL, (void **)&state->pAudioClient);
    EXIT_ON_ERROR(hr)

    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioEndpointVolume,
                                     CLSCTX_ALL, NULL,
                                     (void **)&state->pEndpointVolume);
    EXIT_ON_ERROR(hr)
    IAudioEndpointVolume_QueryHardwareSupport(state->pEndpointVolume,
                                              &state->vol_hw_support);

    state->init_ret = find_formats(ao); /* Probe support formats */
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
    state->init_ret = -1;
    SetEvent(state->init_done);
    return -1;
}

static void thread_pause(wasapi_state *state)
{
    state->is_playing = 0;
    IAudioClient_Stop(state->pAudioClient);
}

/* force_feed - feed in even if available data is smaller than required buffer, to clear the buffer */
static void thread_feed(wasapi_state *state,int force_feed)
{
    BYTE *pData;
    int buffer_size;
    HRESULT hr;

    UINT32 frame_count = state->bufferFrameCount;
    UINT32 client_buffer = state->buffer_block_size;

    if (state->share_mode == AUDCLNT_SHAREMODE_SHARED) {
        UINT32 padding = 0;
        hr = IAudioClient_GetCurrentPadding(state->pAudioClient, &padding);
        EXIT_ON_ERROR(hr)

        frame_count -= padding;
        client_buffer = state->format.Format.nBlockAlign * frame_count;
    }

    hr = IAudioRenderClient_GetBuffer(state->pRenderClient,
                                      frame_count, &pData);
    EXIT_ON_ERROR(hr)
    buffer_size = mp_ring_buffered(state->ringbuff);
    if(buffer_size > client_buffer) { /* OK to copy! */
        mp_ring_read(state->ringbuff, (unsigned char *)pData,
                     client_buffer);
    } else if(force_feed) {
        /* should be smaller than buffer block size by now */
        memset(pData,0,client_buffer);
        mp_ring_read(state->ringbuff, (unsigned char *)pData, client_buffer);
    } else {
        /* buffer underrun?! abort */
        hr = IAudioRenderClient_ReleaseBuffer(state->pRenderClient,
                                              frame_count,
                                              AUDCLNT_BUFFERFLAGS_SILENT);
        EXIT_ON_ERROR(hr)
        return;
    }
    hr = IAudioRenderClient_ReleaseBuffer(state->pRenderClient,
                                          frame_count, 0);
    EXIT_ON_ERROR(hr)
    return;
exit_label:
    EnterCriticalSection(&state->print_lock);
    mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: thread_feed fails with %"PRIx32"!\n", hr);
    LeaveCriticalSection(&state->print_lock);
    return;
}

static void thread_play(wasapi_state *state)
{
    thread_feed(state, 0);
    state->is_playing = 1;
    IAudioClient_Start(state->pAudioClient);
    return;
}

static void thread_reset(wasapi_state *state)
{
    IAudioClient_Stop(state->pAudioClient);
    IAudioClient_Reset(state->pAudioClient);
    if (state->is_playing) {
        thread_play(state);
    }
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
    ExitThread(0);
}

static unsigned int __stdcall ThreadLoop(void *lpParameter)
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

static int get_space(struct ao *ao)
{
    if (!ao || !ao->priv)
        return -1;
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    return mp_ring_available(state->ringbuff);
}

static void reset_buffers(struct wasapi_state *state)
{
    mp_ring_reset(state->ringbuff);
}

static int setup_buffers(struct wasapi_state *state)
{
    state->ringbuff =
        mp_ring_new(state, RING_BUFFER_COUNT * state->buffer_block_size);
    return !state->ringbuff;
}

static void uninit(struct ao *ao, bool immed)
{
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: uninit!\n");
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
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
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: uninit END!\n");
}

#define OPT_BASE_STRUCT wasapi_state
const struct m_sub_options ao_wasapi_conf = {
    .opts = (m_option_t[]){
        OPT_FLAG("exclusive", opt_exclusive, 0),
        OPT_FLAG("list", opt_list, 0),
        OPT_STRING("device", opt_device, 0),
        {NULL},
    },
    .size = sizeof(wasapi_state),
};

static int init(struct ao *ao, char *params)
{
    mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: init!\n");
    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        return -1;
    struct wasapi_state *state = talloc_zero(ao, struct wasapi_state);
    if (!state)
        return -1;
    ao->priv = (void *)state;
    fill_VistaBlob(state);

    struct m_config *cfg = m_config_simple(state);
    m_config_register_options(cfg, ao_wasapi_conf.opts);
    m_config_parse_suboptions(cfg, "wasapi", params);

    if (state->opt_list) {
        enumerate_devices();
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
    state->threadLoop = (HANDLE)CreateThread(NULL, 0, &ThreadLoop, ao, 0, NULL);
    if (!state->threadLoop) {
        /* failed to init thread */
        mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: fail to create thread!\n");
        return -1;
    }
    WaitForSingleObject(state->init_done, INFINITE); /* wait on init complete */
    if (state->init_ret) {
        if (!ao->probing) {
            mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: thread_init failed!\n");
        }
    } else {
        mp_msg(MSGT_AO, MSGL_V, "ao-wasapi: Init Done!\n");
        if (setup_buffers(state))
            mp_msg(MSGT_AO, MSGL_ERR, "ao-wasapi: buffer setup failed!\n");
    }
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
    reset_buffers(state);
}

static int play(struct ao *ao, void *data, int len, int flags)
{
    int ret = 0;
    if (!ao || !ao->priv)
        return ret;
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
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
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    return (float)(RING_BUFFER_COUNT * state->buffer_block_size - get_space(ao)) /
           (float)state->format.Format.nAvgBytesPerSec;
}

const struct ao_driver audio_out_wasapi = {
    .info = &(const struct ao_info) {
        "Windows WASAPI audio output (event mode)",
        "wasapi",
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
