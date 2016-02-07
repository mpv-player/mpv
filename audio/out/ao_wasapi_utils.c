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
#include <wchar.h>
#include <windows.h>
#include <initguid.h>
#include <errors.h>
#include <ksguid.h>
#include <ksmedia.h>
#include <avrt.h>

#include "audio/format.h"
#include "osdep/timer.h"
#include "osdep/io.h"
#include "osdep/strnlen.h"
#include "ao_wasapi.h"

#define MIXER_DEFAULT_LABEL L"mpv - video player"

DEFINE_PROPERTYKEY(mp_PKEY_Device_FriendlyName,
                   0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20,
                   0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
DEFINE_PROPERTYKEY(mp_PKEY_Device_DeviceDesc,
                   0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20,
                   0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 2);
// CEA 861 subformats
// should work on vista
DEFINE_GUID(mp_KSDATAFORMAT_SUBTYPE_IEC61937_DTS,
            0x00000008, 0x0000, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(mp_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL,
            0x00000092, 0x0000, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
// might require 7+
DEFINE_GUID(mp_KSDATAFORMAT_SUBTYPE_IEC61937_AAC,
            0x00000006, 0x0cea, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(mp_KSDATAFORMAT_SUBTYPE_IEC61937_MPEG3,
            0x00000004, 0x0cea, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(mp_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS,
            0x0000000a, 0x0cea, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(mp_KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD,
            0x0000000b, 0x0cea, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(mp_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP,
            0x0000000c, 0x0cea, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

struct wasapi_fmt_mapping {
    const GUID *subtype;
    int format;
};

const struct wasapi_fmt_mapping wasapi_fmt_table[] = {
    {&mp_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL,      AF_FORMAT_S_AC3},
    {&mp_KSDATAFORMAT_SUBTYPE_IEC61937_DTS,                AF_FORMAT_S_DTS},
    {&mp_KSDATAFORMAT_SUBTYPE_IEC61937_AAC,                AF_FORMAT_S_AAC},
    {&mp_KSDATAFORMAT_SUBTYPE_IEC61937_MPEG3,              AF_FORMAT_S_MP3},
    {&mp_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP,          AF_FORMAT_S_TRUEHD},
    {&mp_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS, AF_FORMAT_S_EAC3},
    {&mp_KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD,             AF_FORMAT_S_DTSHD},
    {0}
};

static const GUID *format_to_subtype(int format)
{
    if (af_fmt_is_spdif(format)) {
        for (int i = 0; wasapi_fmt_table[i].format; i++) {
            if (wasapi_fmt_table[i].format == format)
                return wasapi_fmt_table[i].subtype;
        }
        return &KSDATAFORMAT_SPECIFIER_NONE;
    } else if (af_fmt_is_float(format)) {
        return &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    }
    return &KSDATAFORMAT_SUBTYPE_PCM;
}

// "solve" the under-determined inverse of format_to_subtype by assuming the
// input subtype is "special" (i.e. IEC61937)
static int special_subtype_to_format(const GUID *subtype) {
    for (int i = 0; wasapi_fmt_table[i].format; i++) {
        if (IsEqualGUID(subtype, wasapi_fmt_table[i].subtype))
            return wasapi_fmt_table[i].format;
    }
    return 0;
}

char *mp_PKEY_to_str_buf(char *buf, size_t buf_size, const PROPERTYKEY *pkey)
{
    buf = mp_GUID_to_str_buf(buf, buf_size, &pkey->fmtid);
    size_t guid_len = strnlen(buf, buf_size);
    snprintf(buf + guid_len, buf_size - guid_len, ",%"PRIu32,
             (uint32_t) pkey->pid);
    return buf;
}

static void update_waveformat_datarate(WAVEFORMATEXTENSIBLE *wformat)
{
    WAVEFORMATEX *wf = &wformat->Format;
    wf->nBlockAlign     = wf->nChannels      * wf->wBitsPerSample / 8;
    wf->nAvgBytesPerSec = wf->nSamplesPerSec * wf->nBlockAlign;
}

static void set_waveformat(WAVEFORMATEXTENSIBLE *wformat,
                           int format, WORD valid_bits,
                           DWORD samplerate, struct mp_chmap *channels)
{
    wformat->Format.wFormatTag     = WAVE_FORMAT_EXTENSIBLE;
    wformat->Format.nChannels      = channels->num;
    wformat->Format.nSamplesPerSec = samplerate;
    wformat->Format.wBitsPerSample = af_fmt_to_bytes(format) * 8;
    wformat->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    wformat->SubFormat                   = *format_to_subtype(format);
    wformat->Samples.wValidBitsPerSample =
        valid_bits ? valid_bits : wformat->Format.wBitsPerSample;
    wformat->dwChannelMask               = mp_chmap_to_waveext(channels);
    update_waveformat_datarate(wformat);
}

// This implicitly transforms all pcm formats to: interleaved / signed (except
// 8-bit is unsigned) / waveext channel order.  "Special" formats should be
// exempt as they should already satisfy these properties.
static void set_waveformat_with_ao(WAVEFORMATEXTENSIBLE *wformat, struct ao *ao)
{
    struct mp_chmap channels = ao->channels;
    mp_chmap_reorder_to_waveext(&channels);

    set_waveformat(wformat, ao->format, 0, ao->samplerate, &channels);
}

// other wformat parameters must already be set with set_waveformat
static void change_waveformat_samplerate(WAVEFORMATEXTENSIBLE *wformat,
                                         DWORD samplerate)
{
    wformat->Format.nSamplesPerSec = samplerate;
    update_waveformat_datarate(wformat);
}

// other wformat parameters must already be set with set_waveformat
static void change_waveformat_channels(WAVEFORMATEXTENSIBLE *wformat,
                                       struct mp_chmap *channels)
{
    wformat->Format.nChannels = channels->num;
    wformat->dwChannelMask    = mp_chmap_to_waveext(channels);
    update_waveformat_datarate(wformat);
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

static int format_from_waveformat(WAVEFORMATEX *wf)
{
    int format;
    switch (wf->wFormatTag) {
    case WAVE_FORMAT_EXTENSIBLE:
    {
        WAVEFORMATEXTENSIBLE *wformat = (WAVEFORMATEXTENSIBLE *)wf;
        if (IsEqualGUID(&wformat->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM)) {
            format = wf->wBitsPerSample == 8 ? AF_FORMAT_U8 : AF_FORMAT_S32;
        } else if (IsEqualGUID(&wformat->SubFormat,
                               &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            format = AF_FORMAT_FLOAT;
        } else {
            format = special_subtype_to_format(&wformat->SubFormat);
        }
        break;
    }
    case WAVE_FORMAT_PCM:
        format = wf->wBitsPerSample == 8 ? AF_FORMAT_U8 : AF_FORMAT_S32;
        break;
    case WAVE_FORMAT_IEEE_FLOAT:
        format = AF_FORMAT_FLOAT;
        break;
    default:
        return 0;
    }
    // https://msdn.microsoft.com/en-us/library/windows/hardware/ff538802%28v=vs.85%29.aspx:
    // Since mpv doesn't have the notion of "valid bits", we just specify a
    // format with the container size. The least significant, "invalid" bits
    // will be excess precision ignored by wasapi.  The change_bytes operations
    // should be a no-op for properly configured "special" formats, otherwise it
    // will return 0.
    if (wf->wBitsPerSample % 8)
        return 0;
    return af_fmt_change_bytes(format, wf->wBitsPerSample / 8);
}

static bool chmap_from_waveformat(struct mp_chmap *channels,
                                  const WAVEFORMATEX *wf)
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
    struct mp_chmap channels;
    chmap_from_waveformat(&channels, wf);

    unsigned valid_bits = waveformat_valid_bits(wf);
    char validstr[12] = "";
    if (valid_bits != wf->wBitsPerSample)
        snprintf(validstr, sizeof(validstr), " (%u valid)", valid_bits);

    snprintf(buf, buf_size, "%s %s%s @ %uhz",
             mp_chmap_to_str(&channels),
             af_fmt_to_str(format_from_waveformat(wf)),
             validstr, (unsigned) wf->nSamplesPerSec);
    return buf;
}
#define waveformat_to_str(wf) waveformat_to_str_buf((char[40]){0}, 40, (wf))

static void waveformat_copy(WAVEFORMATEXTENSIBLE* dst, WAVEFORMATEX* src)
{
    if (src->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        *dst = *(WAVEFORMATEXTENSIBLE *)src;
    } else {
        dst->Format = *src;
    }
}

static bool set_ao_format(struct ao *ao, WAVEFORMATEX *wf,
                          AUDCLNT_SHAREMODE share_mode)
{
    struct wasapi_state *state = ao->priv;
    int format = format_from_waveformat(wf);
    if (!format) {
        MP_ERR(ao, "Unable to construct sample format from WAVEFORMAT %s\n",
               waveformat_to_str(wf));
        return false;
    }

    // Do not touch the ao for passthrough, just assume that we set WAVEFORMATEX
    // correctly.
    if (af_fmt_is_pcm(format)) {
        struct mp_chmap channels;
        if (!chmap_from_waveformat(&channels, wf)) {
            MP_ERR(ao, "Unable to construct channel map from WAVEFORMAT %s\n",
                   waveformat_to_str(wf));
            return false;
        }
        ao->samplerate = wf->nSamplesPerSec;
        ao->format     = format;
        ao->channels   = channels;
    }
    waveformat_copy(&state->format, wf);
    state->share_mode = share_mode;
    return true;
}

static bool try_format_exclusive(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat)
{
    struct wasapi_state *state = ao->priv;
    MP_VERBOSE(ao, "Trying %s (exclusive)\n",
               waveformat_to_str(&wformat->Format));
    HRESULT hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                                AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                &wformat->Format, NULL);
    if (hr != AUDCLNT_E_UNSUPPORTED_FORMAT)
        EXIT_ON_ERROR(hr);

    return hr == S_OK;
exit_label:
    MP_ERR(state, "Error testing exclusive format: %s\n", mp_HRESULT_to_str(hr));
    return false;
}

// This works like try_format_exclusive(), but will try to fallback to the AC3
// format if the format is a non-AC3 passthrough format. *wformat will be
// adjusted accordingly.
static bool try_format_exclusive_with_spdif_fallback(struct ao *ao,
                                                WAVEFORMATEXTENSIBLE *wformat)
{
    if (try_format_exclusive(ao, wformat))
        return true;
    int special_format = special_subtype_to_format(&wformat->SubFormat);
    if (special_format && special_format != AF_FORMAT_S_AC3) {
        MP_VERBOSE(ao, "Retrying as AC3.\n");
        wformat->SubFormat = *format_to_subtype(AF_FORMAT_S_AC3);
        return try_format_exclusive(ao, wformat);
    }
    return false;
}

static bool search_sample_formats(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat,
                                  int samplerate, struct mp_chmap *channels)
{
    // some common bit depths / container sizes (requests welcome)
    int try[]        = {AF_FORMAT_DOUBLE, AF_FORMAT_FLOAT, AF_FORMAT_S32,
                        AF_FORMAT_S24   , AF_FORMAT_S32  , AF_FORMAT_S16,
                        AF_FORMAT_U8    , 0};
    unsigned valid[] = {0               ,               0,             0,
                        0               ,              24,             0,
                        0               };
    for (int i = 0; try[i]; i++) {
        set_waveformat(wformat, try[i], valid[i], samplerate, channels);
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
    struct wasapi_state *state = ao->priv;
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
                MP_VERBOSE(ao, "%s is supported\n",
                           waveformat_to_str(&wformat->Format));
            }
        } else {
            change_waveformat_channels(wformat, &entry);
            if (try_format_exclusive(ao, wformat))
                mp_chmap_sel_add_map(&chmap_sel, &entry);
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

static bool find_formats_exclusive(struct ao *ao, bool do_search)
{
    WAVEFORMATEXTENSIBLE wformat;
    set_waveformat_with_ao(&wformat, ao);

    // Try the requested format as is. If that doesn't work, and the do_search
    // argument is set, do the pcm format search.
    if (!try_format_exclusive_with_spdif_fallback(ao, &wformat) &&
        (!do_search || !search_channels(ao, &wformat)))
        return false;

    if (!set_ao_format(ao, &wformat.Format, AUDCLNT_SHAREMODE_EXCLUSIVE))
        return false;

    MP_VERBOSE(ao, "Accepted as %s %s @ %dhz (exclusive)\n",
               mp_chmap_to_str(&ao->channels),
               af_fmt_to_str(ao->format), ao->samplerate);
    return true;
}

static bool find_formats_shared(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;

    WAVEFORMATEXTENSIBLE wformat;
    set_waveformat_with_ao(&wformat, ao);

    MP_VERBOSE(ao, "Trying %s (shared)\n", waveformat_to_str(&wformat.Format));
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

    if (!set_ao_format(ao, &wformat.Format, AUDCLNT_SHAREMODE_SHARED))
        return false;

    MP_VERBOSE(ao, "Accepted as %s %s @ %dhz (shared)\n",
               mp_chmap_to_str(&ao->channels),
               af_fmt_to_str(ao->format), ao->samplerate);
    return true;
exit_label:
    MP_ERR(state, "Error finding shared mode format: %s\n",
           mp_HRESULT_to_str(hr));
    return false;
}

static bool find_formats(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;

    if (state->opt_exclusive) {
        // If exclusive is requested, try the requested format (which
        // might be passthrough). If that fails, do a pcm format
        // search.
        return find_formats_exclusive(ao, true);
    } else if (af_fmt_is_spdif(ao->format)) {
        // If a passthrough format is requested, but exclusive mode
        // was not explicitly set, try only the requested passthrough
        // format in exclusive mode. Fall back on shared mode if that
        // fails without doing the exclusive pcm format search.
        if (find_formats_exclusive(ao, false))
            return true;
    }
    // Default is to use shared mode
    return find_formats_shared(ao);
}

static HRESULT init_clock(struct wasapi_state *state) {
    HRESULT hr = IAudioClient_GetService(state->pAudioClient,
                                         &IID_IAudioClock,
                                         (void **)&state->pAudioClock);
    EXIT_ON_ERROR(hr);
    hr = IAudioClock_GetFrequency(state->pAudioClock, &state->clock_frequency);
    EXIT_ON_ERROR(hr);

    QueryPerformanceFrequency(&state->qpc_frequency);

    atomic_store(&state->sample_count, 0);

    MP_VERBOSE(state,
               "IAudioClock::GetFrequency gave a frequency of %"PRIu64".\n",
               (uint64_t) state->clock_frequency);

    return S_OK;
exit_label:
    MP_ERR(state, "Error obtaining the audio device's timing: %s\n",
           mp_HRESULT_to_str(hr));
    return hr;
}

static void init_session_display(struct wasapi_state *state) {
    HRESULT hr = IAudioClient_GetService(state->pAudioClient,
                                         &IID_IAudioSessionControl,
                                         (void **)&state->pSessionControl);
    EXIT_ON_ERROR(hr);

    wchar_t path[MAX_PATH+12] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    lstrcatW(path, L",-IDI_ICON1");
    hr = IAudioSessionControl_SetIconPath(state->pSessionControl, path, NULL);
    if (FAILED(hr)) {
        // don't goto exit_label here since SetDisplayName might still work
        MP_WARN(state, "Error setting audio session icon: %s\n",
                mp_HRESULT_to_str(hr));
    }

    hr = IAudioSessionControl_SetDisplayName(state->pSessionControl,
                                             MIXER_DEFAULT_LABEL, NULL);
    EXIT_ON_ERROR(hr);
    return;
exit_label:
    // if we got here then the session control is useless - release it
    SAFE_RELEASE(state->pSessionControl,
                 IAudioSessionControl_Release(state->pSessionControl));
    MP_WARN(state, "Error setting audio session display name: %s\n",
            mp_HRESULT_to_str(hr));
    return;
}

static void init_volume_control(struct wasapi_state *state)
{
    HRESULT hr;
    if (state->share_mode == AUDCLNT_SHAREMODE_EXCLUSIVE) {
        MP_DBG(state, "Activating pEndpointVolume interface\n");
        hr = IMMDeviceActivator_Activate(state->pDevice,
                                         &IID_IAudioEndpointVolume,
                                         CLSCTX_ALL, NULL,
                                         (void **)&state->pEndpointVolume);
        EXIT_ON_ERROR(hr);

        MP_DBG(state, "IAudioEndpointVolume::QueryHardwareSupport\n");
        hr = IAudioEndpointVolume_QueryHardwareSupport(state->pEndpointVolume,
                                                       &state->vol_hw_support);
        EXIT_ON_ERROR(hr);
    } else {
        MP_DBG(state, "IAudioClient::Initialize pAudioVolume\n");
        hr = IAudioClient_GetService(state->pAudioClient,
                                     &IID_ISimpleAudioVolume,
                                     (void **)&state->pAudioVolume);
        EXIT_ON_ERROR(hr);
    }
    return;
exit_label:
    state->vol_hw_support = 0;
    SAFE_RELEASE(state->pEndpointVolume,
                 IAudioEndpointVolume_Release(state->pEndpointVolume));
    SAFE_RELEASE(state->pAudioVolume,
                 ISimpleAudioVolume_Release(state->pAudioVolume));
    MP_WARN(state, "Error setting up volume control: %s\n",
            mp_HRESULT_to_str(hr));
}

static HRESULT fix_format(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;

    REFERENCE_TIME devicePeriod, bufferDuration, bufferPeriod;
    MP_DBG(state, "IAudioClient::GetDevicePeriod\n");
    HRESULT hr = IAudioClient_GetDevicePeriod(state->pAudioClient,&devicePeriod,
                                              NULL);
    MP_VERBOSE(state, "Device period: %.2g ms\n",
               (double) devicePeriod / 10000.0 );

    // integer multiple of device period close to 50ms
    bufferPeriod = bufferDuration =
                   ceil(50.0 * 10000.0 / devicePeriod) * devicePeriod;

    // handle unsupported buffer size hopefully this shouldn't happen because of
    // the above integer device period
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd370875%28v=vs.85%29.aspx
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
    // something about buffer sizes on Win7
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        if (retries > 0) {
            EXIT_ON_ERROR(hr);
        } else {
            retries ++;
        }
        MP_VERBOSE(state, "IAudioClient::Initialize negotiation failed with %s,"
                   "used %lld * 100ns\n",
                   mp_HRESULT_to_str(hr), bufferDuration);

        IAudioClient_GetBufferSize(state->pAudioClient,
                                   &state->bufferFrameCount);
        bufferPeriod = bufferDuration = (REFERENCE_TIME) (0.5 +
            (10000.0 * 1000 / state->format.Format.nSamplesPerSec
             * state->bufferFrameCount));

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

    MP_DBG(state, "IAudioClient::Initialize IAudioClient_SetEventHandle\n");
    hr = IAudioClient_SetEventHandle(state->pAudioClient, state->hWake);
    EXIT_ON_ERROR(hr);

    MP_DBG(state, "IAudioClient::Initialize IAudioClient_GetBufferSize\n");
    hr = IAudioClient_GetBufferSize(state->pAudioClient,
                                    &state->bufferFrameCount);
    EXIT_ON_ERROR(hr);

    ao->device_buffer = state->bufferFrameCount;
    bufferDuration = (REFERENCE_TIME) (0.5 +
        (10000.0 * 1000 / state->format.Format.nSamplesPerSec
         * state->bufferFrameCount));
    MP_VERBOSE(state, "Buffer frame count: %"PRIu32" (%.2g ms)\n",
               state->bufferFrameCount, (double) bufferDuration / 10000.0 );

    hr = init_clock(state);
    EXIT_ON_ERROR(hr);

    init_session_display(state);
    init_volume_control(state);

    state->hTask = AvSetMmThreadCharacteristics(L"Pro Audio", &(DWORD){0});
    if (!state->hTask) {
        MP_WARN(state, "Failed to set AV thread to Pro Audio: %s\n",
                mp_LastError_to_str());
    }

    return S_OK;
exit_label:
    MP_ERR(state, "Error initializing device: %s\n", mp_HRESULT_to_str(hr));
    return hr;
}

struct device_desc {
    LPWSTR deviceID;
    char *id;
    char *name;
};

static char* get_device_name(struct mp_log *l, void *talloc_ctx, IMMDevice *pDevice)
{
    char *namestr = NULL;
    IPropertyStore *pProps = NULL;
    PROPVARIANT devname;
    PropVariantInit(&devname);

    HRESULT hr = IMMDevice_OpenPropertyStore(pDevice, STGM_READ, &pProps);
    EXIT_ON_ERROR(hr);

    hr = IPropertyStore_GetValue(pProps, &mp_PKEY_Device_FriendlyName,
                                 &devname);
    EXIT_ON_ERROR(hr);

    namestr = mp_to_utf8(talloc_ctx, devname.pwszVal);

exit_label:
    if (FAILED(hr))
        mp_warn(l, "Failed getting device name: %s\n", mp_HRESULT_to_str(hr));
    PropVariantClear(&devname);
    SAFE_RELEASE(pProps, IPropertyStore_Release(pProps));
    return namestr ? namestr : talloc_strdup(talloc_ctx, "");
}

static struct device_desc *get_device_desc(struct mp_log *l, IMMDevice *pDevice)
{
    LPWSTR deviceID;
    HRESULT hr = IMMDevice_GetId(pDevice, &deviceID);
    if (FAILED(hr)) {
        mp_err(l, "Failed getting device id: %s\n", mp_HRESULT_to_str(hr));
        return NULL;
    }
    struct device_desc *d = talloc_zero(NULL, struct device_desc);
    d->deviceID = talloc_memdup(d, deviceID,
                                (wcslen(deviceID) + 1) * sizeof(wchar_t));
    SAFE_RELEASE(deviceID, CoTaskMemFree(deviceID));

    char *full_id = mp_to_utf8(NULL, d->deviceID);
    bstr id = bstr0(full_id);
    bstr_eatstart0(&id, "{0.0.0.00000000}.");
    d->id = bstrdup0(d, id);
    talloc_free(full_id);

    d->name = get_device_name(l, d, pDevice);
    return d;
}

struct enumerator {
    struct mp_log *log;
    IMMDeviceEnumerator *pEnumerator;
    IMMDeviceCollection *pDevices;
    UINT count;
};

static void destroy_enumerator(struct enumerator *e)
{
    if (!e)
        return;
    SAFE_RELEASE(e->pDevices, IMMDeviceCollection_Release(e->pDevices));
    SAFE_RELEASE(e->pEnumerator, IMMDeviceEnumerator_Release(e->pEnumerator));
    talloc_free(e);
}

static struct enumerator *create_enumerator(struct mp_log *log)
{
    struct enumerator *e = talloc_zero(NULL, struct enumerator);
    e->log = log;
    HRESULT hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator,
        (void **)&e->pEnumerator);
    EXIT_ON_ERROR(hr);

    hr = IMMDeviceEnumerator_EnumAudioEndpoints(
        e->pEnumerator, eRender, DEVICE_STATE_ACTIVE, &e->pDevices);
    EXIT_ON_ERROR(hr);

    hr = IMMDeviceCollection_GetCount(e->pDevices, &e->count);
    EXIT_ON_ERROR(hr);

    return e;
exit_label:
    mp_err(log, "Error getting device enumerator: %s\n", mp_HRESULT_to_str(hr));
    destroy_enumerator(e);
    return NULL;
}

static struct device_desc *device_desc_for_num(struct enumerator *e, UINT i)
{
    IMMDevice *pDevice = NULL;
    HRESULT hr = IMMDeviceCollection_Item(e->pDevices, i, &pDevice);
    if (FAILED(hr)) {
        MP_ERR(e, "Failed getting device #%d: %s\n", i, mp_HRESULT_to_str(hr));
        return NULL;
    }
    struct device_desc *d = get_device_desc(e->log, pDevice);
    SAFE_RELEASE(pDevice, IMMDevice_Release(pDevice));
    return d;
}

static struct device_desc *default_device_desc(struct enumerator *e)
{
    IMMDevice *pDevice = NULL;
    HRESULT hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(
        e->pEnumerator, eRender, eMultimedia, &pDevice);
    if (FAILED(hr)) {
        MP_ERR(e, "Error from GetDefaultAudioEndpoint: %s\n",
               mp_HRESULT_to_str(hr));
        return NULL;
    }
    struct device_desc *d = get_device_desc(e->log, pDevice);
    SAFE_RELEASE(pDevice, IMMDevice_Release(pDevice));
    return d;
}

void wasapi_list_devs(struct ao *ao, struct ao_device_list *list)
{
    struct enumerator *enumerator = create_enumerator(ao->log);
    if (!enumerator)
        return;

    for (UINT i = 0; i < enumerator->count; i++) {
        struct device_desc *d = device_desc_for_num(enumerator, i);
        if (!d)
            goto exit_label;
        ao_device_list_add(list, ao, &(struct ao_device_desc){d->id, d->name});
        talloc_free(d);
    }

exit_label:
    destroy_enumerator(enumerator);
}

static HRESULT load_device(struct mp_log *l,
                           IMMDevice **ppDevice, LPWSTR deviceID)
{
    IMMDeviceEnumerator *pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                  &IID_IMMDeviceEnumerator,
                                  (void **)&pEnumerator);
    EXIT_ON_ERROR(hr);

    hr = IMMDeviceEnumerator_GetDevice(pEnumerator, deviceID, ppDevice);
    EXIT_ON_ERROR(hr);

exit_label:
    if (FAILED(hr))
        mp_err(l, "Error loading selected device: %s\n", mp_HRESULT_to_str(hr));
    SAFE_RELEASE(pEnumerator, IMMDeviceEnumerator_Release(pEnumerator));
    return hr;
}

static LPWSTR select_device(struct mp_log *l, struct device_desc *d)
{
    if (!d)
        return NULL;
    mp_verbose(l, "Selecting device \'%s\' (%s)\n", d->id, d->name);
    return talloc_memdup(NULL, d->deviceID,
                         (wcslen(d->deviceID) + 1) * sizeof(wchar_t));
}

bstr wasapi_get_specified_device_string(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    bstr device = bstr_strip(bstr0(state->opt_device));
    if (!device.len)
        device = bstr_strip(bstr0(ao->device));
    return device;
}

LPWSTR wasapi_find_deviceID(struct ao *ao)
{
    LPWSTR deviceID = NULL;
    bstr device = wasapi_get_specified_device_string(ao);
    MP_DBG(ao, "Find device \'%.*s\'\n", BSTR_P(device));

    struct device_desc *d = NULL;
    struct enumerator *enumerator = create_enumerator(ao->log);
    if (!enumerator)
        goto exit_label;

    if (!enumerator->count) {
        MP_ERR(ao, "There are no playback devices available\n");
        goto exit_label;
    }

    if (!device.len) {
        MP_VERBOSE(ao, "No device specified. Selecting default.\n");
        d = default_device_desc(enumerator);
        deviceID = select_device(ao->log, d);
        goto exit_label;
    }

    // try selecting by number
    bstr rest;
    long long devno = bstrtoll(device, &rest, 10);
    if (!rest.len && 0 <= devno && devno < (long long)enumerator->count) {
        MP_VERBOSE(ao, "Selecting device by number: #%lld\n", devno);
        d = device_desc_for_num(enumerator, devno);
        deviceID = select_device(ao->log, d);
        goto exit_label;
    }

    // select by id or name
    bstr_eatstart0(&device, "{0.0.0.00000000}.");
    for (UINT i = 0; i < enumerator->count; i++) {
        d = device_desc_for_num(enumerator, i);
        if (!d)
            goto exit_label;

        if (bstrcmp(device, bstr_strip(bstr0(d->id))) == 0) {
            MP_VERBOSE(ao, "Selecting device by id: \'%.*s\'\n", BSTR_P(device));
            deviceID = select_device(ao->log, d);
            goto exit_label;
        }

        if (bstrcmp(device, bstr_strip(bstr0(d->name))) == 0) {
            if (!deviceID) {
                MP_VERBOSE(ao, "Selecting device by name: \'%.*s\'\n", BSTR_P(device));
                deviceID = select_device(ao->log, d);
            } else {
                MP_WARN(ao, "Multiple devices matched \'%.*s\'."
                        "Ignoring device \'%s\' (%s).\n",
                        BSTR_P(device), d->id, d->name);
            }
        }
        SAFE_RELEASE(d, talloc_free(d));
    }

    if (!deviceID)
        MP_ERR(ao, "Failed to find device \'%.*s\'\n", BSTR_P(device));

exit_label:
    talloc_free(d);
    destroy_enumerator(enumerator);
    return deviceID;
}

static void *unmarshal(struct wasapi_state *state, REFIID type, IStream **from)
{
    if (!*from)
        return NULL;
    void *to_proxy = NULL;
    HRESULT hr = CoGetInterfaceAndReleaseStream(*from, type, &to_proxy);
    *from = NULL; // the stream is released even on failure
    EXIT_ON_ERROR(hr);
    return to_proxy;
exit_label:
    MP_WARN(state, "Error reading COM proxy: %s\n", mp_HRESULT_to_str(hr));
    return to_proxy;
}

void wasapi_receive_proxies(struct wasapi_state *state) {
    state->pAudioVolumeProxy    = unmarshal(state, &IID_ISimpleAudioVolume,
                                            &state->sAudioVolume);
    state->pEndpointVolumeProxy = unmarshal(state, &IID_IAudioEndpointVolume,
                                            &state->sEndpointVolume);
    state->pSessionControlProxy = unmarshal(state, &IID_IAudioSessionControl,
                                            &state->sSessionControl);
}

void wasapi_release_proxies(wasapi_state *state) {
    SAFE_RELEASE(state->pAudioVolumeProxy,
                 ISimpleAudioVolume_Release(state->pAudioVolumeProxy));
    SAFE_RELEASE(state->pEndpointVolumeProxy,
                 IAudioEndpointVolume_Release(state->pEndpointVolumeProxy));
    SAFE_RELEASE(state->pSessionControlProxy,
                 IAudioSessionControl_Release(state->pSessionControlProxy));
}

// Must call CoReleaseMarshalData to decrement marshalled object's reference
// count.
#define SAFE_RELEASE_INTERFACE_STREAM(stream) do { \
        if ((stream) != NULL) {                    \
            CoReleaseMarshalData((stream));        \
            IStream_Release((stream));             \
            (stream) = NULL;                       \
        }                                          \
    } while(0)

static IStream *marshal(struct wasapi_state *state,
                         REFIID type, void *from_obj)
{
    if (!from_obj)
        return NULL;
    IStream *to;
    HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &to);
    EXIT_ON_ERROR(hr);
    hr = CoMarshalInterThreadInterfaceInStream(type, (IUnknown *)from_obj, &to);
    EXIT_ON_ERROR(hr);
    return to;
exit_label:
    SAFE_RELEASE_INTERFACE_STREAM(to);
    MP_WARN(state, "Error creating COM proxy stream: %s\n",
            mp_HRESULT_to_str(hr));
    return to;
}

static void create_proxy_streams(struct wasapi_state *state) {
    state->sAudioVolume    = marshal(state, &IID_ISimpleAudioVolume,
                                     state->pAudioVolume);
    state->sEndpointVolume = marshal(state, &IID_IAudioEndpointVolume,
                                     state->pEndpointVolume);
    state->sSessionControl = marshal(state, &IID_IAudioSessionControl,
                                     state->pSessionControl);
}

static void destroy_proxy_streams(struct wasapi_state *state) {
    // This is only to handle error conditions.
    // During normal operation, these will already have been released by
    // unmarshaling.
    SAFE_RELEASE_INTERFACE_STREAM(state->sAudioVolume);
    SAFE_RELEASE_INTERFACE_STREAM(state->sEndpointVolume);
    SAFE_RELEASE_INTERFACE_STREAM(state->sSessionControl);
}

void wasapi_dispatch(struct ao *ao)
{
    MP_DBG(ao, "Dispatch\n");
    // dispatch any possible pending messages
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        DispatchMessage(&msg);
}

HRESULT wasapi_thread_init(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    MP_DBG(ao, "Init wasapi thread\n");
    int64_t retry_wait = 1;
retry: ;
    HRESULT hr = load_device(ao->log, &state->pDevice, state->deviceID);
    EXIT_ON_ERROR(hr);

    MP_DBG(ao, "Activating pAudioClient interface\n");
    hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioClient,
                                     CLSCTX_ALL, NULL,
                                     (void **)&state->pAudioClient);
    EXIT_ON_ERROR(hr);

    MP_DBG(ao, "Probing formats\n");
    if (!find_formats(ao)) {
        hr = E_FAIL;
        EXIT_ON_ERROR(hr);
    }

    MP_DBG(ao, "Fixing format\n");
    hr = fix_format(ao);
    if ((hr == AUDCLNT_E_DEVICE_IN_USE || hr == AUDCLNT_E_DEVICE_INVALIDATED) &&
        retry_wait <= 8)
    {
        wasapi_thread_uninit(ao);
        MP_WARN(ao, "Retrying in %"PRId64" us\n", retry_wait);
        mp_sleep_us(retry_wait);
        retry_wait *= 2;
        goto retry;
    }
    EXIT_ON_ERROR(hr);

    MP_DBG(ao, "Creating proxies\n");
    create_proxy_streams(state);

    MP_DBG(ao, "Init wasapi thread done\n");
    return S_OK;
exit_label:
    MP_ERR(state, "Error setting up audio thread: %s\n", mp_HRESULT_to_str(hr));
    return hr;
}

void wasapi_thread_uninit(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    MP_DBG(ao, "Thread shutdown\n");
    wasapi_dispatch(ao);

    if (state->pAudioClient)
        IAudioClient_Stop(state->pAudioClient);

    destroy_proxy_streams(state);

    SAFE_RELEASE(state->pRenderClient,   IAudioRenderClient_Release(state->pRenderClient));
    SAFE_RELEASE(state->pAudioClock,     IAudioClock_Release(state->pAudioClock));
    SAFE_RELEASE(state->pAudioVolume,    ISimpleAudioVolume_Release(state->pAudioVolume));
    SAFE_RELEASE(state->pEndpointVolume, IAudioEndpointVolume_Release(state->pEndpointVolume));
    SAFE_RELEASE(state->pSessionControl, IAudioSessionControl_Release(state->pSessionControl));
    SAFE_RELEASE(state->pAudioClient,    IAudioClient_Release(state->pAudioClient));
    SAFE_RELEASE(state->pDevice,         IMMDevice_Release(state->pDevice));
    SAFE_RELEASE(state->hTask,           AvRevertMmThreadCharacteristics(state->hTask));
    MP_DBG(ao, "Thread uninit done\n");
}
