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

#include <math.h>
#include <libavutil/common.h>
#include <initguid.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <avrt.h>

#include "audio/out/ao_wasapi_utils.h"

#include "audio/format.h"
#include "osdep/io.h"
#include "osdep/timer.h"

#define MIXER_DEFAULT_LABEL L"mpv - video player"

DEFINE_PROPERTYKEY(mp_PKEY_Device_FriendlyName,
                   0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20,
                   0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
DEFINE_PROPERTYKEY(mp_PKEY_Device_DeviceDesc,
                   0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20,
                   0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 2);

DEFINE_GUID(mp_KSDATAFORMAT_SUBTYPE_PCM,
            0x00000001, 0x0000, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(mp_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
            0x00000003, 0x0000, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

char *mp_GUID_to_str_buf(char *buf, size_t buf_size, const GUID *guid)
{
    snprintf(buf, buf_size,
             "{%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x}",
             (unsigned) guid->Data1, guid->Data2, guid->Data3,
             guid->Data4[0], guid->Data4[1],
             guid->Data4[2], guid->Data4[3],
             guid->Data4[4], guid->Data4[5],
             guid->Data4[6], guid->Data4[7]);
    return buf;
}

char *mp_PKEY_to_str_buf(char *buf, size_t buf_size, const PROPERTYKEY *pkey)
{
    buf = mp_GUID_to_str_buf(buf, buf_size, &pkey->fmtid);
    size_t guid_len = strnlen(buf, buf_size);
    snprintf(buf + guid_len, buf_size - guid_len, ",%"PRIu32, (uint32_t) pkey->pid);
    return buf;
}

bool wasapi_fill_VistaBlob(wasapi_state *state)
{
    if (!state)
        goto exit_label;
    state->VistaBlob.hAvrt = LoadLibraryW(L"avrt.dll");
    if (!state->VistaBlob.hAvrt)
        goto exit_label;

    state->VistaBlob.pAvSetMmThreadCharacteristicsW =
        (HANDLE (WINAPI *)(LPCWSTR, LPDWORD))
        GetProcAddress(state->VistaBlob.hAvrt, "AvSetMmThreadCharacteristicsW");
    if (!state->VistaBlob.pAvSetMmThreadCharacteristicsW)
        goto exit_label;

    state->VistaBlob.pAvRevertMmThreadCharacteristics =
        (WINBOOL (WINAPI *)(HANDLE))
        GetProcAddress(state->VistaBlob.hAvrt, "AvRevertMmThreadCharacteristics");
    if (!state->VistaBlob.pAvRevertMmThreadCharacteristics)
        goto exit_label;

    return true;
exit_label:
    if (state->VistaBlob.hAvrt) {
        FreeLibrary(state->VistaBlob.hAvrt);
        state->VistaBlob.hAvrt = NULL;
    }
    return false;
}

const char *wasapi_explain_err(const HRESULT hr)
{
#define E(x) case x : return # x ;
    switch (hr) {
    E(S_OK)
    E(E_FAIL)
    E(E_OUTOFMEMORY)
    E(E_POINTER)
    E(E_HANDLE)
    E(E_NOTIMPL)
    E(E_INVALIDARG)
    E(REGDB_E_IIDNOTREG)
    E(CO_E_NOTINITIALIZED)
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

static void update_waveformat_datarate_pcm(WAVEFORMATEXTENSIBLE *wformat)
{
    WAVEFORMATEX *wf = &wformat->Format;
    wf->nBlockAlign     = wf->nChannels      * wf->wBitsPerSample / 8;
    wf->nAvgBytesPerSec = wf->nSamplesPerSec * wf->nBlockAlign;
}

static void set_waveformat(WAVEFORMATEXTENSIBLE *wformat,
                           WORD bits, WORD valid_bits, bool is_float,
                           DWORD samplerate, struct mp_chmap *channels)
{
    wformat->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wformat->Format.nChannels = channels->num;
    wformat->Format.nSamplesPerSec = samplerate;
    wformat->Format.wBitsPerSample = bits;
    wformat->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    if (is_float) {
        wformat->SubFormat = mp_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    } else {
        wformat->SubFormat = mp_KSDATAFORMAT_SUBTYPE_PCM;
    }
    wformat->Samples.wValidBitsPerSample = valid_bits;
    wformat->dwChannelMask = mp_chmap_to_waveext(channels);
    update_waveformat_datarate_pcm(wformat);
}

static void set_waveformat_with_ao(WAVEFORMATEXTENSIBLE *wformat, struct ao *ao)
{
    // This implicitly transforms all formats to:
    // interleaved / signed (except for 8 bit) / waveext channel order.
    // You must still call set_ao_format() to ensure consistency.

    struct mp_chmap channels = ao->channels;
    mp_chmap_reorder_to_waveext(&channels);

    set_waveformat(wformat, af_fmt2bits(ao->format), af_fmt2bits(ao->format),
                   af_fmt_is_float(ao->format), ao->samplerate, &channels);
}

static void change_waveformat_samplerate(WAVEFORMATEXTENSIBLE *wformat,
                                         DWORD samplerate)
{
    // other wformat parameters must already be set with set_waveformat
    wformat->Format.nSamplesPerSec = samplerate;
    update_waveformat_datarate_pcm(wformat);
}

static void change_waveformat_channels(WAVEFORMATEXTENSIBLE *wformat,
                                       struct mp_chmap *channels)
{
    // other wformat parameters must already be set with set_waveformat
    wformat->Format.nChannels = channels->num;
    wformat->dwChannelMask = mp_chmap_to_waveext(channels);
    update_waveformat_datarate_pcm(wformat);
}

static WORD waveformat_valid_bits(const WAVEFORMATEX *wf)
{
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE *wformat = (WAVEFORMATEXTENSIBLE *)wf;
        return wformat->Samples.wValidBitsPerSample;
    } else {
        return wf->wBitsPerSample;
    }
}

static bool waveformat_is_float(WAVEFORMATEX *wf)
{
    switch (wf->wFormatTag) {
    case WAVE_FORMAT_EXTENSIBLE:
    {
        WAVEFORMATEXTENSIBLE *wformat = (WAVEFORMATEXTENSIBLE *)wf;
        return IsEqualGUID(&mp_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, &wformat->SubFormat);
    }
    case WAVE_FORMAT_IEEE_FLOAT:
        return true;
    default:
        return false;
    }
}

static bool waveformat_is_pcm_int(WAVEFORMATEX *wf)
{
    switch (wf->wFormatTag) {
    case WAVE_FORMAT_EXTENSIBLE:
    {
        WAVEFORMATEXTENSIBLE *wformat = (WAVEFORMATEXTENSIBLE *)wf;
        return IsEqualGUID(&mp_KSDATAFORMAT_SUBTYPE_PCM, &wformat->SubFormat);
    }
    case WAVE_FORMAT_PCM:
        return true;
    default:
        return false;
    }
}

static int format_from_waveformat(WAVEFORMATEX *wf)
{
    int format = 0;
    // it is an undocumented fact that 8-bit pcm in WAVEFORMATEX implies unsigned
    if (waveformat_is_float(wf)) {
        format = AF_FORMAT_FLOAT;
    } else if (waveformat_is_pcm_int(wf)) {
        format = wf->wBitsPerSample == 8 ? AF_FORMAT_U8 : AF_FORMAT_S32;
    } else {
        return 0;
    }
    // https://msdn.microsoft.com/en-us/library/windows/hardware/ff538802%28v=vs.85%29.aspx:
    // Since mpv doesn't have the notion of "valid bits", we just specify a
    // format with the container size. The least significant, "invalid"
    // bits will be excess precision ignored by wasapi.
    return af_fmt_change_bits(format, wf->wBitsPerSample);
}

static bool chmap_from_waveformat(struct mp_chmap *channels, const WAVEFORMATEX *wf)
{
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE *wformat = (WAVEFORMATEXTENSIBLE *)wf;
        mp_chmap_from_waveext(channels, wformat->dwChannelMask);
    } else {
        mp_chmap_from_channels(channels, wf->nChannels);
    }

    if (channels->num != wf->nChannels) {
        mp_chmap_from_str(channels, bstr0("empty"));
        return false;
    }

    return true;
}

static char *waveformat_to_str_buf(char *buf, size_t buf_size, WAVEFORMATEX *wf)
{
    char* type = "?";
    if (waveformat_is_float(wf)) {
        type = "float";
    } else if (waveformat_is_pcm_int(wf)) {
        type = wf->wBitsPerSample == 8 ? "u" : "s";
    }

    unsigned valid_bits = waveformat_valid_bits(wf);
    struct mp_chmap channels;
    chmap_from_waveformat(&channels, wf);
    if (valid_bits == wf->wBitsPerSample) {
        snprintf(buf, buf_size, "%s %s%u @ %uhz",
                 mp_chmap_to_str(&channels), type, valid_bits,
                 (unsigned) wf->nSamplesPerSec);
    } else {
        snprintf(buf, buf_size, "%s %s%u (in %s%u) @ %uhz",
                 mp_chmap_to_str(&channels), type, valid_bits,
                 type, (unsigned) wf->wBitsPerSample,
                 (unsigned) wf->nSamplesPerSec);
    }
    return buf;
}
#define waveformat_to_str(wf) waveformat_to_str_buf((char[40]){0}, 40, (wf))

static void waveformat_copy(WAVEFORMATEXTENSIBLE* dst, WAVEFORMATEX* src)
{
    if (src->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        *dst = *(WAVEFORMATEXTENSIBLE *)src;
    else
        dst->Format = *src;
}

static bool set_ao_format(struct ao *ao, WAVEFORMATEX *wf)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

    int format = format_from_waveformat(wf);
    if (!format) {
        MP_ERR(ao, "Unable to construct sample format from WAVEFORMAT %s\n",
               waveformat_to_str(wf));
        return false;
    }
    struct mp_chmap channels;
    if (!chmap_from_waveformat(&channels, wf)) {
        MP_ERR(ao, "Unable to construct channel map from WAVEFORMAT %s\n",
               waveformat_to_str(wf));
        return false;
    }

    ao->samplerate = wf->nSamplesPerSec;
    ao->bps = wf->nAvgBytesPerSec;
    ao->format = format;
    ao->channels = channels;
    waveformat_copy(&state->format, wf);

    return true;
}

static bool try_format_exclusive(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    MP_VERBOSE(ao, "Trying %s\n", waveformat_to_str(&wformat->Format));
    HRESULT hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                                AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                &wformat->Format, NULL);
    if (hr != AUDCLNT_E_UNSUPPORTED_FORMAT)
        EXIT_ON_ERROR(hr);

    return hr == S_OK;
exit_label:
    MP_ERR(state, "Error testing exclusive format: %s (0x%"PRIx32")\n",
           wasapi_explain_err(hr), (uint32_t) hr);
    return false;
}

static bool search_sample_formats(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat,
                                  int samplerate, struct mp_chmap *channels)
{
    // try float
    int float_bits[] = {64, 32, 0};
    for (int i = 0; float_bits[i]; i++) {
        set_waveformat(wformat, float_bits[i], float_bits[i], true,
                       samplerate, channels);
        if (try_format_exclusive(ao, wformat))
            return true;
    }

    // try int
    // some common bit depths / container sizes (requests welcome)
    int bits[]       = {32, 24, 32, 16, 0};
    int valid_bits[] = {32, 24, 24, 16, 0};
    for (int i = 0; bits[i] && valid_bits[i]; i++) {
        set_waveformat(wformat, bits[i], valid_bits[i], false,
                       samplerate, channels);
        if (try_format_exclusive(ao, wformat))
            return true;
    }

    wformat->Format.wBitsPerSample = 0;
    return false;
}

static bool search_samplerates(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat,
                               struct mp_chmap *channels)
{
    // try list of typical sample rates (requests welcome)
    int try[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000,
                 176400, 192000, 352800, 384000, 0};

    // get a list of supported rates
    int n = 0;
    int supported[MP_ARRAY_SIZE(try)] = {0};

    wformat->Format.wBitsPerSample = 0;
    for (int i = 0; try[i]; i++) {
        if (!wformat->Format.wBitsPerSample) {
            if (search_sample_formats(ao, wformat, try[i], channels))
                supported[n++] = try[i];
        } else {
            change_waveformat_samplerate(wformat, try[i]);
            if (try_format_exclusive(ao, wformat))
                supported[n++] = try[i];
        }
    }

    for (int i = 0; supported[i]; i++) {
        // first choose the lowest integer multiple of the sample rate
        if (!(supported[i] % ao->samplerate)) {
            change_waveformat_samplerate(wformat, supported[i]);
            return true;
        }
    }

    // then choose the highest supported (if any)
    if (n) {
        change_waveformat_samplerate(wformat, supported[n-1]);
        return true;
    }

    // otherwise, this is probably an unsupported channel map
    wformat->Format.nSamplesPerSec = 0;
    return false;
}

static bool search_channels(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    struct mp_chmap_sel chmap_sel = {.tmp = state};
    struct mp_chmap entry;
    // put common layouts first so that we find sample rate/format early
    char *channel_layouts[] =
        {"mono", "stereo", "2.1", "4.0", "5.0", "5.1", "6.1", "7.1",
         "3.0", "3.0(back)",
         "quad", "quad(side)", "3.1",
         "5.0(side)", "4.1",
         "5.1(side)", "6.0", "6.0(front)", "hexagonal"
         "6.1(back)", "6.1(front)", "7.0", "7.0(front)",
         "7.1(wide)", "7.1(wide-side)", "7.1(rear)", "octagonal", NULL};

    wformat->Format.nSamplesPerSec = 0;
    for (int j = 0; channel_layouts[j]; j++) {
        mp_chmap_from_str(&entry, bstr0(channel_layouts[j]));
        if (!wformat->Format.nSamplesPerSec) {
            if (search_samplerates(ao, wformat, &entry)) {
                mp_chmap_sel_add_map(&chmap_sel, &entry);
                MP_VERBOSE(ao, "%s is supported\n", waveformat_to_str(&wformat->Format));
            }
        } else {
            change_waveformat_channels(wformat, &entry);
            if (try_format_exclusive(ao, wformat)) {
                mp_chmap_sel_add_map(&chmap_sel, &entry);
                MP_VERBOSE(ao, "%s is supported\n", mp_chmap_to_str(&entry));
            }
        }
    }

    entry = ao->channels;
    if (ao_chmap_sel_adjust(ao, &chmap_sel, &entry)){
        change_waveformat_channels(wformat, &entry);
        return true;
    }

    MP_ERR(ao, "No suitable audio format found\n");
    return false;
}

static bool find_formats_exclusive(struct ao *ao)
{
    WAVEFORMATEXTENSIBLE wformat;
    set_waveformat_with_ao(&wformat, ao);

    // If the format doesn't work as is, we have to manually try
    // all possible formats in search_channels(). Nice API Microsoft.
    if (!try_format_exclusive(ao, &wformat) && !search_channels(ao, &wformat))
        return false;

    if (!set_ao_format(ao, &wformat.Format))
        return false;

    MP_VERBOSE(ao, "Accepted as %s %s @ %dhz\n",
               mp_chmap_to_str(&ao->channels),
               af_fmt_to_str(ao->format), ao->samplerate);
    return true;
}

static bool find_formats_shared(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

    WAVEFORMATEXTENSIBLE wformat;
    set_waveformat_with_ao(&wformat, ao);

    MP_VERBOSE(ao, "Trying %s\n", waveformat_to_str(&wformat.Format));
    WAVEFORMATEX *closestMatch;
    HRESULT hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                                AUDCLNT_SHAREMODE_SHARED,
                                                &wformat.Format, &closestMatch);
    if (hr != AUDCLNT_E_UNSUPPORTED_FORMAT)
        EXIT_ON_ERROR(hr);

    switch (hr) {
    case S_OK:
        break;
    case S_FALSE:
        waveformat_copy(&wformat, closestMatch);
        CoTaskMemFree(closestMatch);
        MP_VERBOSE(ao, "Closest match is %s\n",
                   waveformat_to_str(&wformat.Format));
        break;
    default:
        hr = IAudioClient_GetMixFormat(state->pAudioClient, &closestMatch);
        EXIT_ON_ERROR(hr);
        waveformat_copy(&wformat, closestMatch);
        MP_VERBOSE(ao, "Fallback to mix format %s\n",
                   waveformat_to_str(&wformat.Format));
        CoTaskMemFree(closestMatch);
    }

    if (!set_ao_format(ao, &wformat.Format))
        return false;

    MP_VERBOSE(ao, "Accepted as %s %s @ %dhz\n",
               mp_chmap_to_str(&ao->channels),
               af_fmt_to_str(ao->format), ao->samplerate);
    return true;
exit_label:
    MP_ERR(state, "Error finding shared mode format: %s (0x%"PRIx32")\n",
           wasapi_explain_err(hr), (uint32_t) hr);
    return false;
}

static bool try_passthrough(struct ao *ao)
{
    // fixme: this will only do SPDIF AC3 and doesn't bother to check
    // that the resulting waveformat is actually consistent with the ao
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd370811%28v=vs.85%29.aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd316761(v=vs.85).aspx
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

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
        .SubFormat = mp_KSDATAFORMAT_SUBTYPE_PCM,
    };
    wformat.SubFormat.Data1 = WAVE_FORMAT_DOLBY_AC3_SPDIF; // see INIT_WAVEFORMATEX_GUID macro

    MP_VERBOSE(ao, "Trying passthrough for %s...\n", af_fmt_to_str(ao->format));

    HRESULT hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                                state->share_mode,
                                                &wformat.Format, NULL);
    if (!FAILED(hr)) {
        state->format = wformat;
        return true;
    }
    MP_ERR(ao, "Couldn't use passthrough\n");
    return false;
}

static bool find_formats(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

    if (state->opt_exclusive){
        // https://msdn.microsoft.com/en-us/library/windows/desktop/dd370811%28v=vs.85%29.aspx
        // "Many audio devices support both PCM and non-PCM stream formats.
        // However, the audio engine can mix only PCM streams. Thus, only
        // exclusive-mode streams can have non-PCM formats.
        if (AF_FORMAT_IS_IEC61937(ao->format))
            return try_passthrough(ao);
        else
            return find_formats_exclusive(ao);
    }

    return find_formats_shared(ao);
}

static HRESULT init_clock(struct wasapi_state *state) {
    HRESULT hr;

    hr = IAudioClient_GetService(state->pAudioClient,
                                 &IID_IAudioClock,
                                 (void **)&state->pAudioClock);
    EXIT_ON_ERROR(hr);
    hr = IAudioClock_GetFrequency(state->pAudioClock, &state->clock_frequency);
    EXIT_ON_ERROR(hr);

    QueryPerformanceFrequency(&state->qpc_frequency);

    atomic_store(&state->sample_count, 0);

    MP_VERBOSE(state, "IAudioClock::GetFrequency gave a frequency of %"PRIu64".\n",
               (uint64_t) state->clock_frequency);

    return S_OK;
exit_label:
    MP_ERR(state, "Error obtaining the audio device's timing: %s (0x%"PRIx32")\n",
           wasapi_explain_err(hr), (uint32_t) hr);
    return hr;
}

static HRESULT init_session_display(struct wasapi_state *state) {
    HRESULT hr;
    wchar_t path[MAX_PATH+12] = {0};

    hr = IAudioClient_GetService(state->pAudioClient,
                                 &IID_IAudioSessionControl,
                                 (void **)&state->pSessionControl);
    EXIT_ON_ERROR(hr);

    GetModuleFileNameW(NULL, path, MAX_PATH);
    lstrcatW(path, L",-IDI_ICON1");

    hr = IAudioSessionControl_SetDisplayName(state->pSessionControl, MIXER_DEFAULT_LABEL, NULL);
    EXIT_ON_ERROR(hr);
    hr = IAudioSessionControl_SetIconPath(state->pSessionControl, path, NULL);
    EXIT_ON_ERROR(hr);

    return S_OK;
exit_label:
    MP_WARN(state, "Error setting audio session display name: %s (0x%"PRIx32")\n",
            wasapi_explain_err(hr), (uint32_t) hr);
    return S_OK; // No reason to abort initialization.
}

static HRESULT fix_format(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    HRESULT hr;

    REFERENCE_TIME devicePeriod, bufferDuration, bufferPeriod;
    MP_DBG(state, "IAudioClient::GetDevicePeriod\n");
    hr = IAudioClient_GetDevicePeriod(state->pAudioClient,&devicePeriod, NULL);
    MP_VERBOSE(state, "Device period: %.2g ms\n", (double) devicePeriod / 10000.0 );

    /* integer multiple of device period close to 50ms */
    bufferPeriod = bufferDuration = ceil(50.0 * 10000.0 / devicePeriod) * devicePeriod;

    /* handle unsupported buffer size */
    /* hopefully this shouldn't happen because of the above integer device period */
    /* http://msdn.microsoft.com/en-us/library/windows/desktop/dd370875%28v=vs.85%29.aspx */
    int retries=0;
reinit:
    if (state->share_mode == AUDCLNT_SHAREMODE_SHARED)
        bufferPeriod = 0;

    MP_DBG(state, "IAudioClient::Initialize\n");
    hr = IAudioClient_Initialize(state->pAudioClient,
                                 state->share_mode,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                 bufferDuration,
                                 bufferPeriod,
                                 &(state->format.Format),
                                 NULL);
    /* something about buffer sizes on Win7 */
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        if (retries > 0)
            EXIT_ON_ERROR(hr);
        else
            retries ++;

        MP_VERBOSE(state, "IAudioClient::Initialize negotiation failed with %s (0x%"PRIx32"), used %lld * 100ns\n",
                   wasapi_explain_err(hr), (uint32_t) hr, bufferDuration);

        IAudioClient_GetBufferSize(state->pAudioClient, &state->bufferFrameCount);
        bufferPeriod = bufferDuration =
            (REFERENCE_TIME) ((10000.0 * 1000 / state->format.Format.nSamplesPerSec *
                               state->bufferFrameCount) + 0.5);

        IAudioClient_Release(state->pAudioClient);
        state->pAudioClient = NULL;
        hr = IMMDeviceActivator_Activate(state->pDevice,
                                         &IID_IAudioClient, CLSCTX_ALL,
                                         NULL, (void **)&state->pAudioClient);
        goto reinit;
    }
    EXIT_ON_ERROR(hr);

    MP_DBG(state, "IAudioClient::Initialize pRenderClient\n");
    hr = IAudioClient_GetService(state->pAudioClient,
                                 &IID_IAudioRenderClient,
                                 (void **)&state->pRenderClient);
    EXIT_ON_ERROR(hr);

    MP_DBG(state, "IAudioClient::Initialize pAudioVolume\n");
    hr = IAudioClient_GetService(state->pAudioClient,
                                 &IID_ISimpleAudioVolume,
                                 (void **)&state->pAudioVolume);
    EXIT_ON_ERROR(hr);

    MP_DBG(state, "IAudioClient::Initialize IAudioClient_SetEventHandle\n");
    hr = IAudioClient_SetEventHandle(state->pAudioClient, state->hFeed);
    EXIT_ON_ERROR(hr);

    MP_DBG(state, "IAudioClient::Initialize IAudioClient_GetBufferSize\n");
    hr = IAudioClient_GetBufferSize(state->pAudioClient,
                                    &state->bufferFrameCount);
    EXIT_ON_ERROR(hr);

    ao->device_buffer = state->bufferFrameCount;
    state->buffer_block_size = state->format.Format.nChannels *
                               state->format.Format.wBitsPerSample / 8 *
                               state->bufferFrameCount;
    bufferDuration =
        (REFERENCE_TIME) ((10000.0 * 1000 / state->format.Format.nSamplesPerSec *
                           state->bufferFrameCount) + 0.5);
    MP_VERBOSE(state, "Buffer frame count: %"PRIu32" (%.2g ms)\n",
               state->bufferFrameCount, (double) bufferDuration / 10000.0 );

    hr = init_clock(state);
    EXIT_ON_ERROR(hr);

    hr = init_session_display(state);
    EXIT_ON_ERROR(hr);

    if (state->VistaBlob.hAvrt) {
        state->hTask =
            state->VistaBlob.pAvSetMmThreadCharacteristicsW(L"Pro Audio", &state->taskIndex);
    }

    MP_VERBOSE(state, "Format fixed. Using %lld byte buffer block size\n",
               (long long) state->buffer_block_size);

    return S_OK;
exit_label:
    MP_ERR(state, "Error initializing device: %s (0x%"PRIx32")\n",
           wasapi_explain_err(hr), (uint32_t) hr);
    return hr;
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

    hr = IPropertyStore_GetValue(pProps, &mp_PKEY_Device_FriendlyName, &devname);
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

    hr = IPropertyStore_GetValue(pProps, &mp_PKEY_Device_DeviceDesc, &devdesc);
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
                                                     eRender, eMultimedia,
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

    return S_OK;
exit_label:
    talloc_free(defid);
    SAFE_RELEASE(pDevice, IMMDevice_Release(pDevice));
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices));
    SAFE_RELEASE(pEnumerator, IMMDeviceEnumerator_Release(pEnumerator));
    return hr;
}

bool wasapi_enumerate_devices(struct mp_log *log, struct ao *ao,
                             struct ao_device_list *list)
{
    HRESULT hr;
    hr = enumerate_with_state(log, ao, list, "Active devices:",
                              DEVICE_STATE_ACTIVE, 1);
    EXIT_ON_ERROR(hr);
    hr = enumerate_with_state(log, ao, list, "Unplugged devices:",
                              DEVICE_STATE_UNPLUGGED, 0);
    EXIT_ON_ERROR(hr);
    return true;
exit_label:
    mp_err(log, "Error enumerating devices: %s (0x%"PRIx32")\n",
           wasapi_explain_err(hr), (uint32_t) hr);
    return false;
}

static HRESULT load_default_device(struct ao *ao, IMMDeviceEnumerator* pEnumerator,
                                   IMMDevice **ppDevice)
{
    HRESULT hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnumerator,
                                                             eRender, eMultimedia,
                                                             ppDevice);
    EXIT_ON_ERROR(hr);

    char *id = get_device_id(*ppDevice);
    MP_VERBOSE(ao, "Default device ID: %s\n", id);
    talloc_free(id);

    return S_OK;
exit_label:
    MP_ERR(ao , "Error loading default device: %s (0x%"PRIx32")\n",
           wasapi_explain_err(hr), (uint32_t) hr);
    return hr;
}

static HRESULT find_and_load_device(struct ao *ao, IMMDeviceEnumerator* pEnumerator,
                                    IMMDevice **ppDevice, char *search)
{
    HRESULT hr;
    IMMDeviceCollection *pDevices = NULL;
    IMMDevice *pTempDevice = NULL;
    LPWSTR deviceID = NULL;

    char *end;
    int devno = (int) strtol(search, &end, 10);

    char *devid = NULL;
    if (end == search || *end) {
        devid = search;
    }

    int search_err = 0;

    if (devid == NULL) {
        hr = IMMDeviceEnumerator_EnumAudioEndpoints(pEnumerator, eRender,
                                                    DEVICE_STATE_ACTIVE, &pDevices);
        EXIT_ON_ERROR(hr);

        int count;
        IMMDeviceCollection_GetCount(pDevices, &count);

        if (devno >= count) {
            MP_ERR(ao, "No device #%d\n", devno);
        } else {
            MP_VERBOSE(ao, "Finding device #%d\n", devno);
            hr = IMMDeviceCollection_Item(pDevices, devno, &pTempDevice);
            EXIT_ON_ERROR(hr);

            hr = IMMDevice_GetId(pTempDevice, &deviceID);
            EXIT_ON_ERROR(hr);

            MP_VERBOSE(ao, "Found device #%d\n", devno);
        }
    } else {
        hr = IMMDeviceEnumerator_EnumAudioEndpoints(pEnumerator, eRender,
                                                    DEVICE_STATE_ACTIVE|DEVICE_STATE_UNPLUGGED,
                                                    &pDevices);
        EXIT_ON_ERROR(hr);

        int count;
        IMMDeviceCollection_GetCount(pDevices, &count);

        MP_VERBOSE(ao, "Finding device %s\n", devid);

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
                        MP_ERR(ao, "Multiple matching devices found\n");
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
            MP_ERR(ao, "Could not find device %s\n", devid);
        }
    }

    SAFE_RELEASE(pTempDevice, IMMDevice_Release(pTempDevice));
    SAFE_RELEASE(pDevices, IMMDeviceCollection_Release(pDevices));

    if (deviceID == NULL || search_err) {
        hr = E_NOTFOUND;
    } else {
        MP_VERBOSE(ao, "Loading device %S\n", deviceID);

        hr = IMMDeviceEnumerator_GetDevice(pEnumerator, deviceID, ppDevice);

        if (FAILED(hr)) {
            MP_ERR(ao, "Could not load requested device\n");
        }
    }

exit_label:
    SAFE_RELEASE(pTempDevice, IMMDevice_Release(pTempDevice));
    SAFE_RELEASE(pDevices,    IMMDeviceCollection_Release(pDevices));

    CoTaskMemFree(deviceID);
    return hr;
}

int wasapi_validate_device(struct mp_log *log, const m_option_t *opt,
                           struct bstr name, struct bstr param)
{
    if (bstr_equals0(param, "help")) {
        wasapi_enumerate_devices(log, NULL, NULL);
        return M_OPT_EXIT;
    }

    mp_dbg(log, "Validating device=%s\n", param.start);

    char *end;
    int devno = (int) strtol(param.start, &end, 10);

    int ret = 1;
    if ((end == (void *)param.start || *end) && devno < 0)
        ret = M_OPT_OUT_OF_RANGE;

    mp_dbg(log, "device=%s %svalid\n", param.start, ret == 1 ? "" : "not ");
    return ret;
}

HRESULT wasapi_setup_proxies(struct wasapi_state *state) {
    HRESULT hr;

#define UNMARSHAL(type, to, from) do {                                    \
    hr = CoGetInterfaceAndReleaseStream((from), &(type), (void **)&(to)); \
    (from) = NULL;                                                        \
    EXIT_ON_ERROR(hr);                                                    \
} while (0)

    UNMARSHAL(IID_ISimpleAudioVolume,   state->pAudioVolumeProxy,    state->sAudioVolume);
    UNMARSHAL(IID_IAudioEndpointVolume, state->pEndpointVolumeProxy, state->sEndpointVolume);
    UNMARSHAL(IID_IAudioSessionControl, state->pSessionControlProxy, state->sSessionControl);

#undef UNMARSHAL

    return S_OK;
exit_label:
    MP_ERR(state, "Error reading COM proxy: %s (0x%"PRIx32")\n",
           wasapi_explain_err(hr), (uint32_t) hr);
    return hr;
}

void wasapi_release_proxies(wasapi_state *state) {
    SAFE_RELEASE(state->pAudioVolumeProxy,    IUnknown_Release(state->pAudioVolumeProxy));
    SAFE_RELEASE(state->pEndpointVolumeProxy, IUnknown_Release(state->pEndpointVolumeProxy));
    SAFE_RELEASE(state->pSessionControlProxy, IUnknown_Release(state->pSessionControlProxy));
}

static HRESULT create_proxies(struct wasapi_state *state) {
    HRESULT hr;

#define MARSHAL(type, to, from) do {                               \
    hr = CreateStreamOnHGlobal(NULL, TRUE, &(to));                 \
    EXIT_ON_ERROR(hr);                                             \
    hr = CoMarshalInterThreadInterfaceInStream(&(type),            \
                                               (IUnknown *)(from), \
                                               &(to));             \
    EXIT_ON_ERROR(hr);                                             \
} while (0)

    MARSHAL(IID_ISimpleAudioVolume,   state->sAudioVolume,    state->pAudioVolume);
    MARSHAL(IID_IAudioEndpointVolume, state->sEndpointVolume, state->pEndpointVolume);
    MARSHAL(IID_IAudioSessionControl, state->sSessionControl, state->pSessionControl);

    return S_OK;
exit_label:
    MP_ERR(state, "Error creating COM proxy: %s (0x%"PRIx32")\n",
           wasapi_explain_err(hr), (uint32_t) hr);
    return hr;
}

static void destroy_proxies(struct wasapi_state *state) {
    SAFE_RELEASE(state->sAudioVolume,    IUnknown_Release(state->sAudioVolume));
    SAFE_RELEASE(state->sEndpointVolume, IUnknown_Release(state->sEndpointVolume));
    SAFE_RELEASE(state->sSessionControl, IUnknown_Release(state->sSessionControl));
}

void wasapi_dispatch(void)
{
    /* dispatch any possible pending messages */
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        DispatchMessage(&msg);
}

HRESULT wasapi_thread_init(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;
    HRESULT hr;
    MP_DBG(ao, "Init wasapi thread\n");
    int64_t retry_wait = 1;
retry:
    state->initial_volume = -1.0;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&state->pEnumerator);
    EXIT_ON_ERROR(hr);

    char *device = state->opt_device;
    if (!device || !device[0])
        device = ao->device;


    if (!device || !device[0])
        hr = load_default_device(ao, state->pEnumerator, &state->pDevice);
    else
        hr = find_and_load_device(ao, state->pEnumerator, &state->pDevice, device);
    EXIT_ON_ERROR(hr);

    char *name = get_device_name(state->pDevice);
    MP_VERBOSE(ao, "Device loaded: %s\n", name);
    talloc_free(name);

    MP_DBG(ao, "Activating pAudioClient interface\n");
    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioClient,
                                     CLSCTX_ALL, NULL, (void **)&state->pAudioClient);
    EXIT_ON_ERROR(hr);

    MP_DBG(ao, "Activating pEndpointVolume interface\n");
    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioEndpointVolume,
                                     CLSCTX_ALL, NULL,
                                     (void **)&state->pEndpointVolume);
    EXIT_ON_ERROR(hr);

    MP_DBG(ao, "Query hardware volume support\n");
    hr = IAudioEndpointVolume_QueryHardwareSupport(state->pEndpointVolume,
                                                   &state->vol_hw_support);
    if (hr != S_OK)
        MP_WARN(ao, "Error querying hardware volume control: %s (0x%"PRIx32")\n",
                wasapi_explain_err(hr), (uint32_t) hr);

    MP_DBG(ao, "Probing formats\n");
    if (!find_formats(ao)) {
        hr = E_FAIL;
        EXIT_ON_ERROR(hr);
    }

    MP_DBG(ao, "Fixing format\n");
    hr = fix_format(ao);
    if ((hr == AUDCLNT_E_DEVICE_IN_USE ||
         hr == AUDCLNT_E_DEVICE_INVALIDATED) &&
        retry_wait <= 8) {
        wasapi_thread_uninit(ao);
        MP_WARN(ao, "Retrying in %"PRId64" us\n", retry_wait);
        mp_sleep_us(retry_wait);
        retry_wait *= 2;
        goto retry;
    }
    EXIT_ON_ERROR(hr);

    MP_DBG(ao, "Creating proxies\n");
    hr = create_proxies(state);
    EXIT_ON_ERROR(hr);

    MP_DBG(ao, "Read volume levels\n");
    if (state->opt_exclusive)
        IAudioEndpointVolume_GetMasterVolumeLevelScalar(state->pEndpointVolume,
                                                        &state->initial_volume);
    else
        ISimpleAudioVolume_GetMasterVolume(state->pAudioVolume,
                                           &state->initial_volume);

    state->previous_volume = state->initial_volume;

    wasapi_change_init(ao);

    MP_DBG(ao, "Init wasapi thread done\n");
    return S_OK;
exit_label:
    MP_ERR(state, "Error setting up audio thread: %s (0x%"PRIx32")\n",
           wasapi_explain_err(hr), (uint32_t) hr);
    return hr;
}

void wasapi_thread_uninit(struct ao *ao)
{
    struct wasapi_state *state = (struct wasapi_state *)ao->priv;

    wasapi_dispatch();

    if (state->pAudioClient)
        IAudioClient_Stop(state->pAudioClient);

    wasapi_change_uninit(ao);

    if (state->opt_exclusive &&
        state->pEndpointVolume &&
        state->initial_volume > 0 )
        IAudioEndpointVolume_SetMasterVolumeLevelScalar(state->pEndpointVolume,
                                                        state->initial_volume, NULL);

    destroy_proxies(state);

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
    MP_DBG(ao, "Thread uninit done\n");
}
