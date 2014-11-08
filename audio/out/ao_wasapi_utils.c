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

#include <initguid.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <avrt.h>

#include "audio/out/ao_wasapi_utils.h"

#include "audio/format.h"
#include "osdep/io.h"

#define MIXER_DEFAULT_LABEL L"mpv - video player"

#define EXIT_ON_ERROR(hres)  \
              do { if (FAILED(hres)) { goto exit_label; } } while(0)
#define SAFE_RELEASE(unk, release) \
              do { if ((unk) != NULL) { release; (unk) = NULL; } } while(0)

#ifndef PKEY_Device_FriendlyName
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName,
                   0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20,
                   0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
DEFINE_PROPERTYKEY(PKEY_Device_DeviceDesc,
                   0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20,
                   0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 2);
#endif

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

int wasapi_fill_VistaBlob(wasapi_state *state)
{
    if (!state)
        return 1;
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
    return 1;
}

const char *wasapi_explain_err(const HRESULT hr)
{
#define E(x) case x : return # x ;
    switch (hr) {
    E(S_OK)
    E(E_OUTOFMEMORY)
    E(E_POINTER)
    E(E_INVALIDARG)
    E(AUDCLNT_E_NOT_INITIALIZED)
    E(AUDCLNT_E_ALREADY_INITIALIZED)
    E(AUDCLNT_E_WRONG_ENDPOINT_TYPE)
    E(AUDCLNT_E_DEVICE_INVALIDATED)
    E(AUDCLNT_E_NOT_STOPPED)
    E(AUDCLNT_E_BUFFER_TOO_LARGE)
    E(AUDCLNT_E_OUT_OF_ORDER)
    E(AUDCLNT_E_UNSUPPORTED_FORMAT)
    E(AUDCLNT_E_INVALID_SIZE)
    E(AUDCLNT_E_DEVICE_IN_USE)
    E(AUDCLNT_E_BUFFER_OPERATION_PENDING)
    E(AUDCLNT_E_THREAD_NOT_REGISTERED)
    E(AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED)
    E(AUDCLNT_E_ENDPOINT_CREATE_FAILED)
    E(AUDCLNT_E_SERVICE_NOT_RUNNING)
    E(AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED)
    E(AUDCLNT_E_EXCLUSIVE_MODE_ONLY)
    E(AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL)
    E(AUDCLNT_E_EVENTHANDLE_NOT_SET)
    E(AUDCLNT_E_INCORRECT_BUFFER_SIZE)
    E(AUDCLNT_E_BUFFER_SIZE_ERROR)
    E(AUDCLNT_E_CPUUSAGE_EXCEEDED)
    E(AUDCLNT_E_BUFFER_ERROR)
    E(AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    E(AUDCLNT_E_INVALID_DEVICE_PERIOD)
    E(AUDCLNT_E_INVALID_STREAM_FLAG)
    E(AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE)
    E(AUDCLNT_E_RESOURCES_INVALIDATED)
    E(AUDCLNT_S_BUFFER_EMPTY)
    E(AUDCLNT_S_THREAD_ALREADY_REGISTERED)
    E(AUDCLNT_S_POSITION_STALLED)
    default:
        return "<Unknown>";
    }
#undef E
}

static void set_format(WAVEFORMATEXTENSIBLE *wformat, WORD bytepersample,
                       DWORD samplerate, WORD channels, DWORD chanmask)
{
    int block_align = channels * bytepersample;
    wformat->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE; /* Only PCM is supported */
    wformat->Format.nChannels = channels;
    wformat->Format.nSamplesPerSec = samplerate;
    wformat->Format.nAvgBytesPerSec = samplerate * block_align;
    wformat->Format.nBlockAlign = block_align;
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

static int format_set_bits(int old_format, int bits, int fp)
{
    if (fp) {
        switch (bits) {
        case 64: return AF_FORMAT_DOUBLE;
        case 32: return AF_FORMAT_FLOAT;
        default: return 0;
        }
    }

    return af_fmt_change_bits(old_format, bits);
}

static int set_ao_format(struct wasapi_state *state,
                         struct ao *const ao,
                         WAVEFORMATEXTENSIBLE wformat)
{
    if (wformat.SubFormat.Data1 != 1 && wformat.SubFormat.Data1 != 3) {
        MP_ERR(ao, "unknown SubFormat %"PRIu32"\n",
               (uint32_t)wformat.SubFormat.Data1);
        return 0;
    }

    // .Data1 == 1 is PCM, .Data1 == 3 is IEEE_FLOAT
    int format = format_set_bits(ao->format,
        wformat.Format.wBitsPerSample, wformat.SubFormat.Data1 == 3);

    if (!format)
        return 0;

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
    if (!af_format)
        return 0;

    MP_VERBOSE(ao, "trying %dch %s @ %dhz\n",
               channels.num, af_fmt_to_str(af_format), samplerate);

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
            MP_VERBOSE(ao, "accepted as %dch %s @ %dhz\n",
                       ao->channels.num, af_fmt_to_str(ao->format), ao->samplerate);

            return 1;
        }
    } if (hr == S_OK || (!state->opt_exclusive && hr == AUDCLNT_E_UNSUPPORTED_FORMAT)) {
        // AUDCLNT_E_UNSUPPORTED_FORMAT here means "works in shared, doesn't in exclusive"
        if (set_ao_format(state, ao, wformat)) {
            MP_VERBOSE(ao, "%dch %s @ %dhz accepted\n",
                       ao->channels.num, af_fmt_to_str(af_format), samplerate);
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
    EXIT_ON_ERROR(hr);

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
            .nSamplesPerSec = ao->samplerate,
            .nAvgBytesPerSec = (ao->samplerate) * (ao->channels.num * 2),
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

    MP_VERBOSE(ao, "trying passthrough for %s...\n", af_fmt_to_str(ao->format));

    HRESULT hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                                state->share_mode,
                                                u.ex, NULL);
    if (!FAILED(hr)) {
        ao->format = ao->format;
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

        MP_ERR(ao, "couldn't use passthrough!");
        if (!state->opt_exclusive)
            MP_ERR(ao, " (try exclusive mode)");
        MP_ERR(ao, "\n");
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

        MP_WARN(ao, "couldn't use default mix format!\n");
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
            MP_ERR(ao, "couldn't find acceptable audio format!\n");
            return -1;
        }
    }
}

static int init_clock(struct wasapi_state *state) {
    HRESULT hr;

    hr = IAudioClient_GetService(state->pAudioClient,
                                 &IID_IAudioClock,
                                 (void **)&state->pAudioClock);
    EXIT_ON_ERROR(hr);
    hr = IAudioClock_GetFrequency(state->pAudioClock, &state->clock_frequency);
    EXIT_ON_ERROR(hr);

    QueryPerformanceFrequency(&state->qpc_frequency);

    atomic_store(&state->sample_count, 0);

    MP_VERBOSE(state, "IAudioClock::GetFrequency gave a frequency of %"PRIu64".\n", (uint64_t) state->clock_frequency);

    return 0;
exit_label:
    MP_ERR(state, "init_clock failed with %s, unable to obtain the audio device's timing!\n",
           wasapi_explain_err(hr));
    return 1;
}

static int init_session_display(struct wasapi_state *state) {
    HRESULT hr;
    wchar_t path[MAX_PATH+12] = {0};

    hr = IAudioClient_GetService(state->pAudioClient,
                                 &IID_IAudioSessionControl,
                                 (void **) &state->pSessionControl);
    EXIT_ON_ERROR(hr);

    GetModuleFileNameW(NULL, path, MAX_PATH);
    lstrcatW(path, L",-IDI_ICON1");

    hr = IAudioSessionControl_SetDisplayName(state->pSessionControl, MIXER_DEFAULT_LABEL, NULL);
    EXIT_ON_ERROR(hr);
    hr = IAudioSessionControl_SetIconPath(state->pSessionControl, path, NULL);
    EXIT_ON_ERROR(hr);

    return 0;

exit_label:
    MP_ERR(state, "init_session_display failed with %s.\n",
           wasapi_explain_err(hr));
    return 1;
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
    MP_VERBOSE(state, "IAudioClient::Initialize GetDevicePeriod: %s\n", wasapi_explain_err(hr));
reinit:
    MP_VERBOSE(state, "IAudioClient::Initialize reinit\n");
    hr = IAudioClient_Initialize(state->pAudioClient,
                                 state->share_mode,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                 state->defaultRequestedDuration,
                                 state->defaultRequestedDuration,
                                 &(state->format.Format),
                                 NULL);
    /* something about buffer sizes on Win7, fixme might loop forever */
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        MP_VERBOSE(state, "IAudioClient::Initialize negotiation failed with %s, used %"PRId64" * 100ns\n",
                   wasapi_explain_err(hr), state->defaultRequestedDuration);
        if (offset > 10.0)
            goto exit_label;                /* is 10 enough to break out of the loop?*/
        IAudioClient_GetBufferSize(state->pAudioClient, &state->bufferFrameCount);
        state->defaultRequestedDuration =
            (REFERENCE_TIME)((10000.0 * 1000 / state->format.Format.nSamplesPerSec *
                              state->bufferFrameCount) + offset);
        offset += 0.5;
        IAudioClient_Release(state->pAudioClient);
        state->pAudioClient = NULL;
        hr = IMMDeviceActivator_Activate(state->pDevice,
                                         &IID_IAudioClient, CLSCTX_ALL,
                                         NULL, (void **)&state->pAudioClient);
        goto reinit;
    }
    EXIT_ON_ERROR(hr);

    MP_VERBOSE(state, "IAudioClient::Initialize pRenderClient\n");
    hr = IAudioClient_GetService(state->pAudioClient,
                                 &IID_IAudioRenderClient,
                                 (void **)&state->pRenderClient);
    EXIT_ON_ERROR(hr);
    MP_VERBOSE(state, "IAudioClient::Initialize pAudioVolume\n");
    hr = IAudioClient_GetService(state->pAudioClient,
                                 &IID_ISimpleAudioVolume,
                                 (void **) &state->pAudioVolume);
    EXIT_ON_ERROR(hr);

    if (!state->hFeed)
        goto exit_label;
    MP_VERBOSE(state, "IAudioClient::Initialize IAudioClient_SetEventHandle\n");
    hr = IAudioClient_SetEventHandle(state->pAudioClient, state->hFeed);
    EXIT_ON_ERROR(hr);
    MP_VERBOSE(state, "IAudioClient::Initialize IAudioClient_GetBufferSize\n");
    hr = IAudioClient_GetBufferSize(state->pAudioClient,
                                    &state->bufferFrameCount);
    EXIT_ON_ERROR(hr);
    state->buffer_block_size = state->format.Format.nChannels *
                               state->format.Format.wBitsPerSample / 8 *
                               state->bufferFrameCount;

    if (init_clock(state))
        return 1;
    if (init_session_display(state))
        return 1;

    state->hTask =
        state->VistaBlob.pAvSetMmThreadCharacteristicsW(L"Pro Audio", &state->taskIndex);
    MP_VERBOSE(state, "fix_format OK, using %"PRId64" byte buffer block size!\n",
               (long long) state->buffer_block_size);
    return 0;
exit_label:
    MP_ERR(state, "fix_format fails with %s, failed to determine buffer block size!\n",
           wasapi_explain_err(hr));
    return 1;
}

static char* get_device_id(IMMDevice *pDevice) {
    if (!pDevice) {
        return NULL;
    }

    LPWSTR devid = NULL;
    char *idstr = NULL;

    HRESULT hr = IMMDevice_GetId(pDevice, &devid);
    EXIT_ON_ERROR(hr);

    idstr = mp_to_utf8(NULL, devid);

    if (strstr(idstr, "{0.0.0.00000000}.")) {
        char *stripped = talloc_strdup(NULL, idstr + strlen("{0.0.0.00000000}."));
        talloc_free(idstr);
        idstr = stripped;
    }

exit_label:
    SAFE_RELEASE(devid, CoTaskMemFree(devid));
    return idstr;
}

static char* get_device_name(IMMDevice *pDevice) {
    if (!pDevice) {
        return NULL;
    }

    IPropertyStore *pProps = NULL;
    char *namestr = NULL;

    HRESULT hr = IMMDevice_OpenPropertyStore(pDevice, STGM_READ, &pProps);
    EXIT_ON_ERROR(hr);

    PROPVARIANT devname;
    PropVariantInit(&devname);

    hr = IPropertyStore_GetValue(pProps, &PKEY_Device_FriendlyName, &devname);
    EXIT_ON_ERROR(hr);

    namestr = mp_to_utf8(NULL, devname.pwszVal);

exit_label:
    PropVariantClear(&devname);
    SAFE_RELEASE(pProps, IPropertyStore_Release(pProps));
    return namestr;
}

static char* get_device_desc(IMMDevice *pDevice) {
    if (!pDevice) {
        return NULL;
    }

    IPropertyStore *pProps = NULL;
    char *desc = NULL;

    HRESULT hr = IMMDevice_OpenPropertyStore(pDevice, STGM_READ, &pProps);
    EXIT_ON_ERROR(hr);

    PROPVARIANT devdesc;
    PropVariantInit(&devdesc);

    hr = IPropertyStore_GetValue(pProps, &PKEY_Device_DeviceDesc, &devdesc);
    EXIT_ON_ERROR(hr);

    desc = mp_to_utf8(NULL, devdesc.pwszVal);

exit_label:
    PropVariantClear(&devdesc);
    SAFE_RELEASE(pProps, IPropertyStore_Release(pProps));
    return desc;
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
    talloc_free(idstr);
    return found;
}

// Warning: ao and list are NULL in the "--ao=wasapi:device=help" path!
static HRESULT enumerate_with_state(struct mp_log *log, struct ao *ao,
                                    struct ao_device_list *list,
                                    char *header, int status, int with_id)
{
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDeviceCollection *pDevices = NULL;
    IMMDevice *pDevice = NULL;
    char *defid = NULL;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&pEnumerator);
    EXIT_ON_ERROR(hr);

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnumerator,
                                                     eRender, eConsole,
                                                     &pDevice);
    EXIT_ON_ERROR(hr);

    defid = get_device_id(pDevice);

    SAFE_RELEASE(pDevice, IMMDevice_Release(pDevice));

    hr = IMMDeviceEnumerator_EnumAudioEndpoints(pEnumerator, eRender,
                                                status, &pDevices);
    EXIT_ON_ERROR(hr);

    int count;
    IMMDeviceCollection_GetCount(pDevices, &count);
    if (count > 0) {
        mp_info(log, "%s\n", header);
    }

    for (int i = 0; i < count; i++) {
        hr = IMMDeviceCollection_Item(pDevices, i, &pDevice);
        EXIT_ON_ERROR(hr);

        char *name = get_device_name(pDevice);
        char *id = get_device_id(pDevice);

        char *mark = "";
        if (strcmp(id, defid) == 0)
            mark = " (default)";

        if (with_id) {
            mp_info(log, "Device #%d: %s, ID: %s%s\n", i, name, id, mark);
        } else {
            mp_info(log, "%s, ID: %s%s\n", name, id, mark);
        }

        if (ao) {
            char *desc = talloc_asprintf(NULL, "%s, ID: %s%s", name, id, mark);
            struct ao_device_desc e = {id, desc};
            ao_device_list_add(list, ao, &e);
        }

        talloc_free(name);
        talloc_free(id);
        SAFE_RELEASE(pDevice, IMMDevice_Release(pDevice));
    }
    talloc_free(defid);
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices));
    SAFE_RELEASE(pEnumerator, IMMDeviceEnumerator_Release(pEnumerator));
    return hr;

exit_label:
    talloc_free(defid);
    SAFE_RELEASE(pDevice, IMMDevice_Release(pDevice));
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices));
    SAFE_RELEASE(pEnumerator, IMMDeviceEnumerator_Release(pEnumerator));
    return hr;
}

int wasapi_enumerate_devices(struct mp_log *log, struct ao *ao,
                             struct ao_device_list *list)
{
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    hr = enumerate_with_state(log, ao, list, "Active devices:",
                              DEVICE_STATE_ACTIVE, 1);
    EXIT_ON_ERROR(hr);
    hr = enumerate_with_state(log, ao, list, "Unplugged devices:",
                              DEVICE_STATE_UNPLUGGED, 0);
    EXIT_ON_ERROR(hr);
    CoUninitialize();
    return 0;
exit_label:
    mp_err(log, "Error enumerating devices: HRESULT %08"PRIx32" \"%s\"\n",
           (uint32_t)hr, wasapi_explain_err(hr));
    CoUninitialize();
    return 1;
}

static HRESULT find_and_load_device(struct ao *ao, IMMDevice **ppDevice,
                                    char *search)
{
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDeviceCollection *pDevices = NULL;
    IMMDevice *pTempDevice = NULL;
    LPWSTR deviceID = NULL;

    char *end;
    int devno = (int) strtol(search, &end, 10);

    char *devid = NULL;
    if (end == search || *end) {
        devid = search;
    }

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    EXIT_ON_ERROR(hr);

    int search_err = 0;

    if (devid == NULL) {
        hr = IMMDeviceEnumerator_EnumAudioEndpoints(pEnumerator, eRender,
                                                    DEVICE_STATE_ACTIVE, &pDevices);
        EXIT_ON_ERROR(hr);

        int count;
        IMMDeviceCollection_GetCount(pDevices, &count);

        if (devno >= count) {
            MP_ERR(ao, "no device #%d!\n", devno);
        } else {
            MP_VERBOSE(ao, "finding device #%d\n", devno);
            hr = IMMDeviceCollection_Item(pDevices, devno, &pTempDevice);
            EXIT_ON_ERROR(hr);

            hr = IMMDevice_GetId(pTempDevice, &deviceID);
            EXIT_ON_ERROR(hr);

            MP_VERBOSE(ao, "found device #%d\n", devno);
        }
    } else {
        hr = IMMDeviceEnumerator_EnumAudioEndpoints(pEnumerator, eRender,
                                                    DEVICE_STATE_ACTIVE|DEVICE_STATE_UNPLUGGED,
                                                    &pDevices);
        EXIT_ON_ERROR(hr);

        int count;
        IMMDeviceCollection_GetCount(pDevices, &count);

        MP_VERBOSE(ao, "finding device %s\n", devid);

        IMMDevice *prevDevice = NULL;

        for (int i = 0; i < count; i++) {
            hr = IMMDeviceCollection_Item(pDevices, i, &pTempDevice);
            EXIT_ON_ERROR(hr);

            if (device_id_match(get_device_id(pTempDevice), devid)) {
                hr = IMMDevice_GetId(pTempDevice, &deviceID);
                EXIT_ON_ERROR(hr);
                break;
            }
            char *desc = get_device_desc(pTempDevice);
            if (strstr(desc, devid)) {
                if (deviceID) {
                    char *name;
                    if (!search_err) {
                        MP_ERR(ao, "multiple matching devices found!\n");
                        name = get_device_name(prevDevice);
                        MP_ERR(ao, "%s\n", name);
                        talloc_free(name);
                        search_err = 1;
                    }
                    name = get_device_name(pTempDevice);
                    MP_ERR(ao, "%s\n", name);
                    talloc_free(name);
                }
                hr = IMMDevice_GetId(pTempDevice, &deviceID);
                prevDevice = pTempDevice;
            }
            talloc_free(desc);

            SAFE_RELEASE(pTempDevice, IMMDevice_Release(pTempDevice));
        }

        if (deviceID == NULL) {
            MP_ERR(ao, "could not find device %s!\n", devid);
        }
    }

    SAFE_RELEASE(pTempDevice, IMMDevice_Release(pTempDevice));
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices));

    if (deviceID == NULL || search_err) {
        hr = E_NOTFOUND;
    } else {
        MP_VERBOSE(ao, "loading device %S\n", deviceID);

        hr = IMMDeviceEnumerator_GetDevice(pEnumerator, deviceID, ppDevice);

        if (FAILED(hr)) {
            MP_ERR(ao, "could not load requested device!\n");
        }
    }

exit_label:
    SAFE_RELEASE(pTempDevice, IMMDevice_Release(pTempDevice));
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices));
    SAFE_RELEASE(pEnumerator, IMMDeviceEnumerator_Release(pEnumerator));
    return hr;
}

int wasapi_validate_device(struct mp_log *log, const m_option_t *opt,
                           struct bstr name, struct bstr param)
{
    if (bstr_equals0(param, "help")) {
        wasapi_enumerate_devices(log, NULL, NULL);
        return M_OPT_EXIT;
    }

    mp_dbg(log, "validating device=%s\n", param.start);

    char *end;
    int devno = (int) strtol(param.start, &end, 10);

    int ret = 1;
    if ((end == (void*)param.start || *end) && devno < 0)
        ret = M_OPT_OUT_OF_RANGE;

    mp_dbg(log, "device=%s %svalid\n", param.start, ret == 1 ? "" : "not ");
    return ret;
}

HRESULT wasapi_setup_proxies(struct wasapi_state *state) {
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

#define UNMARSHAL(type, to, from) do {                                    \
    hr = CoGetInterfaceAndReleaseStream((from), &(type), (void**) &(to)); \
    (from) = NULL;                                                        \
    EXIT_ON_ERROR(hr);                                                    \
} while (0)

    UNMARSHAL(IID_IAudioClient,         state->pAudioClientProxy,    state->sAudioClient);
    UNMARSHAL(IID_ISimpleAudioVolume,   state->pAudioVolumeProxy,    state->sAudioVolume);
    UNMARSHAL(IID_IAudioEndpointVolume, state->pEndpointVolumeProxy, state->sEndpointVolume);
    UNMARSHAL(IID_IAudioSessionControl, state->pSessionControlProxy, state->sSessionControl);

#undef UNMARSHAL

exit_label:
    if (hr != S_OK) {
        MP_ERR(state, "error reading COM proxy: %08x %s\n", (unsigned int)hr,
               wasapi_explain_err(hr));
    }
    return hr;
}

void wasapi_release_proxies(wasapi_state *state) {
    SAFE_RELEASE(state->pAudioClientProxy,    IUnknown_Release(state->pAudioClientProxy));
    SAFE_RELEASE(state->pAudioVolumeProxy,    IUnknown_Release(state->pAudioVolumeProxy));
    SAFE_RELEASE(state->pEndpointVolumeProxy, IUnknown_Release(state->pEndpointVolumeProxy));
    SAFE_RELEASE(state->pSessionControlProxy, IUnknown_Release(state->pSessionControlProxy));

    CoUninitialize();
}

static HRESULT create_proxies(struct wasapi_state *state) {
    HRESULT hr;

#define MARSHAL(type, to, from) do {                               \
    hr = CreateStreamOnHGlobal(NULL, TRUE, &(to));                 \
    EXIT_ON_ERROR(hr);                                             \
    hr = CoMarshalInterThreadInterfaceInStream(&(type),            \
                                               (IUnknown*) (from), \
                                               &(to));             \
    EXIT_ON_ERROR(hr);                                             \
} while (0)

    MARSHAL(IID_IAudioClient,         state->sAudioClient,    state->pAudioClient);
    MARSHAL(IID_ISimpleAudioVolume,   state->sAudioVolume,    state->pAudioVolume);
    MARSHAL(IID_IAudioEndpointVolume, state->sEndpointVolume, state->pEndpointVolume);
    MARSHAL(IID_IAudioSessionControl, state->sSessionControl, state->pSessionControl);

exit_label:
    return hr;
}

int wasapi_thread_init(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&state->pEnumerator);
    EXIT_ON_ERROR(hr);

    char *device = state->opt_device;
    if (!device || !device[0])
        device = ao->device;

    if (!device || !device[0]) {
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(state->pEnumerator,
                                                         eRender, eConsole,
                                                         &state->pDevice);
        char *id = get_device_id(state->pDevice);
        MP_VERBOSE(ao, "default device ID: %s\n", id);
        talloc_free(id);
    } else {
        hr = find_and_load_device(ao, &state->pDevice, device);
    }
    EXIT_ON_ERROR(hr);

    char *name = get_device_name(state->pDevice);
    MP_VERBOSE(ao, "device loaded: %s\n", name);
    talloc_free(name);

    MP_VERBOSE(ao, "wasapi_thread_init: activating pAudioClient %p\n", state->pAudioClient);
    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioClient,
                                     CLSCTX_ALL, NULL, (void **)&state->pAudioClient);
    EXIT_ON_ERROR(hr);

    MP_VERBOSE(ao, "wasapi_thread_init: activating pEndpointVolume\n");
    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioEndpointVolume,
                                     CLSCTX_ALL, NULL,
                                     (void **)&state->pEndpointVolume);
    EXIT_ON_ERROR(hr);
    IAudioEndpointVolume_QueryHardwareSupport(state->pEndpointVolume,
                                              &state->vol_hw_support);

    MP_VERBOSE(ao, "wasapi_thread_init: probing formats\n");
    state->init_ret = find_formats(ao); /* Probe support formats */
    if (state->init_ret)
        goto exit_label;
    if (!fix_format(state)) { /* now that we're sure what format to use */
    MP_VERBOSE(ao, "wasapi_thread_init: fixing formats\n");
        EXIT_ON_ERROR(create_proxies(state));

        if (state->opt_exclusive)
            IAudioEndpointVolume_GetMasterVolumeLevelScalar(state->pEndpointVolume,
                                                            &state->initial_volume);
        else
            ISimpleAudioVolume_GetMasterVolume(state->pAudioVolume,
                                               &state->initial_volume);

        state->previous_volume = state->initial_volume;

        MP_VERBOSE(ao, "thread_init OK!\n");
        SetEvent(state->init_done);
        return state->init_ret;
    }
exit_label:
   MP_ERR(state, "wasapi_thread_init fails with %"PRIx32": %s!\n", (uint32_t)hr, wasapi_explain_err(hr));
    state->init_ret = -1;
    SetEvent(state->init_done);
    return -1;
}

void wasapi_thread_uninit(wasapi_state *state)
{
    if (state->pAudioClient)
        IAudioClient_Stop(state->pAudioClient);

    if (state->opt_exclusive)
        IAudioEndpointVolume_SetMasterVolumeLevelScalar(state->pEndpointVolume,
                                                        state->initial_volume, NULL);

    SAFE_RELEASE(state->pRenderClient,   IAudioRenderClient_Release(state->pRenderClient));
    SAFE_RELEASE(state->pAudioClock,     IAudioClock_Release(state->pAudioClock));
    SAFE_RELEASE(state->pAudioVolume,    ISimpleAudioVolume_Release(state->pAudioVolume));
    SAFE_RELEASE(state->pEndpointVolume, IAudioEndpointVolume_Release(state->pEndpointVolume));
    SAFE_RELEASE(state->pSessionControl, IAudioSessionControl_Release(state->pSessionControl));
    SAFE_RELEASE(state->pAudioClient,    IAudioClient_Release(state->pAudioClient));
    SAFE_RELEASE(state->pDevice,         IMMDevice_Release(state->pDevice));
    SAFE_RELEASE(state->pEnumerator,     IMMDeviceEnumerator_Release(state->pEnumerator));

    if (state->hTask)
        state->VistaBlob.pAvRevertMmThreadCharacteristics(state->hTask);
    CoUninitialize();
    ExitThread(0);
}
