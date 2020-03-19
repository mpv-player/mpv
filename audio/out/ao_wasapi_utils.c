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

struct wasapi_sample_fmt {
    int mp_format;  // AF_FORMAT_*
    int bits;       // aka wBitsPerSample
    int used_msb;   // aka wValidBitsPerSample
    const GUID *subtype;
};

// some common bit depths / container sizes (requests welcome)
// Entries that have the same mp_format must be:
//  1. consecutive
//  2. sorted by preferred format (worst comes last)
static const struct wasapi_sample_fmt wasapi_formats[] = {
    {AF_FORMAT_U8,       8,  8, &KSDATAFORMAT_SUBTYPE_PCM},
    {AF_FORMAT_S16,     16, 16, &KSDATAFORMAT_SUBTYPE_PCM},
    {AF_FORMAT_S32,     32, 32, &KSDATAFORMAT_SUBTYPE_PCM},
    // compatible, assume LSBs are ignored
    {AF_FORMAT_S32,     32, 24, &KSDATAFORMAT_SUBTYPE_PCM},
    // aka S24 (with conversion on output)
    {AF_FORMAT_S32,     24, 24, &KSDATAFORMAT_SUBTYPE_PCM},
    {AF_FORMAT_FLOAT,   32, 32, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT},
    {AF_FORMAT_S_AC3,   16, 16, &mp_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL},
    {AF_FORMAT_S_DTS,   16, 16, &mp_KSDATAFORMAT_SUBTYPE_IEC61937_DTS},
    {AF_FORMAT_S_AAC,   16, 16, &mp_KSDATAFORMAT_SUBTYPE_IEC61937_AAC},
    {AF_FORMAT_S_MP3,   16, 16, &mp_KSDATAFORMAT_SUBTYPE_IEC61937_MPEG3},
    {AF_FORMAT_S_TRUEHD, 16, 16, &mp_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP},
    {AF_FORMAT_S_EAC3,  16, 16, &mp_KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS},
    {AF_FORMAT_S_DTSHD, 16, 16, &mp_KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD},
    {0},
};

static void wasapi_get_best_sample_formats(
    int src_format, struct wasapi_sample_fmt *out_formats)
{
    int mp_formats[AF_FORMAT_COUNT + 1];
    af_get_best_sample_formats(src_format, mp_formats);
    for (int n = 0; mp_formats[n]; n++) {
        for (int i = 0; wasapi_formats[i].mp_format; i++) {
            if (wasapi_formats[i].mp_format == mp_formats[n])
                *out_formats++ = wasapi_formats[i];
        }
    }
    *out_formats = (struct wasapi_sample_fmt) {0};
}

static const GUID *format_to_subtype(int format)
{
    for (int i = 0; wasapi_formats[i].mp_format; i++) {
        if (format == wasapi_formats[i].mp_format)
            return wasapi_formats[i].subtype;
    }
    return &KSDATAFORMAT_SPECIFIER_NONE;
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
                           struct wasapi_sample_fmt *format,
                           DWORD samplerate, struct mp_chmap *channels)
{
    wformat->Format.wFormatTag     = WAVE_FORMAT_EXTENSIBLE;
    wformat->Format.nChannels      = channels->num;
    wformat->Format.nSamplesPerSec = samplerate;
    wformat->Format.wBitsPerSample = format->bits;
    wformat->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    wformat->SubFormat                   = *format_to_subtype(format->mp_format);
    wformat->Samples.wValidBitsPerSample = format->used_msb;
    wformat->dwChannelMask               = mp_chmap_to_waveext(channels);
    update_waveformat_datarate(wformat);
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

static struct wasapi_sample_fmt format_from_waveformat(WAVEFORMATEX *wf)
{
    struct wasapi_sample_fmt res = {0};

    for (int n = 0; wasapi_formats[n].mp_format; n++) {
        const struct wasapi_sample_fmt *fmt = &wasapi_formats[n];
        int valid_bits = 0;

        if (wf->wBitsPerSample != fmt->bits)
            continue;

        const GUID *wf_guid = NULL;

        switch (wf->wFormatTag) {
        case WAVE_FORMAT_EXTENSIBLE: {
            WAVEFORMATEXTENSIBLE *wformat = (WAVEFORMATEXTENSIBLE *)wf;
            wf_guid = &wformat->SubFormat;
            if (IsEqualGUID(wf_guid, &KSDATAFORMAT_SUBTYPE_PCM))
                valid_bits = wformat->Samples.wValidBitsPerSample;
            break;
        }
        case WAVE_FORMAT_PCM:
            wf_guid = &KSDATAFORMAT_SUBTYPE_PCM;
            break;
        case WAVE_FORMAT_IEEE_FLOAT:
            wf_guid = &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;
        }

        if (!wf_guid || !IsEqualGUID(wf_guid, fmt->subtype))
            continue;

        res = *fmt;
        if (valid_bits > 0 && valid_bits < fmt->bits)
            res.used_msb = valid_bits;
        break;
    }

    return res;
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

    struct wasapi_sample_fmt format = format_from_waveformat(wf);

    snprintf(buf, buf_size, "%s %s (%d/%d bits) @ %uhz",
             mp_chmap_to_str(&channels),
             af_fmt_to_str(format.mp_format), format.bits, format.used_msb,
             (unsigned) wf->nSamplesPerSec);
    return buf;
}
#define waveformat_to_str(wf) waveformat_to_str_buf((char[64]){0}, 64, (wf))

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
    struct wasapi_sample_fmt format = format_from_waveformat(wf);
    if (!format.mp_format) {
        MP_ERR(ao, "Unable to construct sample format from WAVEFORMAT %s\n",
               waveformat_to_str(wf));
        return false;
    }

    // Do not touch the ao for passthrough, just assume that we set WAVEFORMATEX
    // correctly.
    if (af_fmt_is_pcm(format.mp_format)) {
        struct mp_chmap channels;
        if (!chmap_from_waveformat(&channels, wf)) {
            MP_ERR(ao, "Unable to construct channel map from WAVEFORMAT %s\n",
                   waveformat_to_str(wf));
            return false;
        }

        struct ao_convert_fmt conv = {
            .src_fmt    = format.mp_format,
            .channels   = channels.num,
            .dst_bits   = format.bits,
            .pad_lsb    = format.bits - format.used_msb,
        };
        if (!ao_can_convert_inplace(&conv)) {
            MP_ERR(ao, "Unable to convert to %s\n", waveformat_to_str(wf));
            return false;
        }

        state->convert_format = conv;
        ao->samplerate = wf->nSamplesPerSec;
        ao->format     = format.mp_format;
        ao->channels   = channels;
    }
    waveformat_copy(&state->format, wf);
    state->share_mode = share_mode;

    MP_VERBOSE(ao, "Accepted as %s %s @ %dhz -> %s (%s)\n",
               mp_chmap_to_str(&ao->channels),
               af_fmt_to_str(ao->format), ao->samplerate,
               waveformat_to_str(wf),
               state->share_mode == AUDCLNT_SHAREMODE_EXCLUSIVE
               ? "exclusive" : "shared");
    return true;
}

#define mp_format_res_str(hres) \
    (SUCCEEDED(hres) ? ((hres) == S_OK) ? "ok" : "close" \
                     : ((hres) == AUDCLNT_E_UNSUPPORTED_FORMAT) \
                       ? "unsupported" : mp_HRESULT_to_str(hres))

static bool try_format_exclusive(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat)
{
    struct wasapi_state *state = ao->priv;
    HRESULT hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                                AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                &wformat->Format, NULL);
    MP_VERBOSE(ao, "Trying %s (exclusive) -> %s\n",
               waveformat_to_str(&wformat->Format), mp_format_res_str(hr));
    return SUCCEEDED(hr);
}

static bool search_sample_formats(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat,
                                  int samplerate, struct mp_chmap *channels)
{
    struct wasapi_sample_fmt alt_formats[MP_ARRAY_SIZE(wasapi_formats)];
    wasapi_get_best_sample_formats(ao->format, alt_formats);
    for (int n = 0; alt_formats[n].mp_format; n++) {
        set_waveformat(wformat, &alt_formats[n], samplerate, channels);
        if (try_format_exclusive(ao, wformat))
            return true;
    }

    wformat->Format.wBitsPerSample = 0;
    return false;
}

static bool search_samplerates(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat,
                               struct mp_chmap *channels)
{
    // put common samplerates first so that we find format early
    int try[] = {48000, 44100, 96000, 88200, 192000, 176400,
                 32000, 22050, 11025, 8000, 16000, 352800, 384000, 0};

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

    int samplerate = af_select_best_samplerate(ao->samplerate, supported);
    if (samplerate > 0) {
        change_waveformat_samplerate(wformat, samplerate);
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
        {"stereo", "5.1", "7.1", "6.1", "mono", "2.1", "4.0", "5.0",
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
            if (search_samplerates(ao, wformat, &entry))
                mp_chmap_sel_add_map(&chmap_sel, &entry);
        } else {
            change_waveformat_channels(wformat, &entry);
            if (try_format_exclusive(ao, wformat))
                mp_chmap_sel_add_map(&chmap_sel, &entry);
        }
    }

    entry = ao->channels;
    if (ao_chmap_sel_adjust2(ao, &chmap_sel, &entry, !state->opt_exclusive)){
        change_waveformat_channels(wformat, &entry);
        return true;
    }

    MP_ERR(ao, "No suitable audio format found\n");
    return false;
}

static bool find_formats_exclusive(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat)
{
    // Try the specified format as is
    if (try_format_exclusive(ao, wformat))
        return true;

    if (af_fmt_is_spdif(ao->format)) {
        if (ao->format != AF_FORMAT_S_AC3) {
            // If the requested format failed and it is passthrough, but not
            // AC3, try lying and saying it is.
            MP_VERBOSE(ao, "Retrying as AC3.\n");
            wformat->SubFormat = *format_to_subtype(AF_FORMAT_S_AC3);
            if (try_format_exclusive(ao, wformat))
                return true;
        }
        return false;
    }

    // Fallback on the PCM format search
    return search_channels(ao, wformat);
}

static bool find_formats_shared(struct ao *ao, WAVEFORMATEXTENSIBLE *wformat)
{
    struct wasapi_state *state = ao->priv;

    struct mp_chmap channels;
    if (!chmap_from_waveformat(&channels, &wformat->Format)) {
        MP_ERR(ao, "Error converting channel map\n");
        return false;
    }

    HRESULT hr;
    WAVEFORMATEX *mix_format;
    hr = IAudioClient_GetMixFormat(state->pAudioClient, &mix_format);
    EXIT_ON_ERROR(hr);

    // WASAPI doesn't do any sample rate conversion on its own and
    // will typically only accept the mix format samplerate. Although
    // it will accept any PCM sample format, everything gets converted
    // to the mix format anyway (pretty much always float32), so just
    // use that.
    WAVEFORMATEXTENSIBLE try_format;
    waveformat_copy(&try_format, mix_format);
    CoTaskMemFree(mix_format);

    // WASAPI may accept channel maps other than the mix format
    // if a surround emulator is enabled.
    change_waveformat_channels(&try_format, &channels);

    hr = IAudioClient_IsFormatSupported(state->pAudioClient,
                                        AUDCLNT_SHAREMODE_SHARED,
                                        &try_format.Format,
                                        &mix_format);
    MP_VERBOSE(ao, "Trying %s (shared) -> %s\n",
               waveformat_to_str(&try_format.Format), mp_format_res_str(hr));
    if (hr != AUDCLNT_E_UNSUPPORTED_FORMAT)
        EXIT_ON_ERROR(hr);

    switch (hr) {
    case S_OK:
        waveformat_copy(wformat, &try_format.Format);
        break;
    case S_FALSE:
        waveformat_copy(wformat, mix_format);
        CoTaskMemFree(mix_format);
        MP_VERBOSE(ao, "Closest match is %s\n",
                   waveformat_to_str(&wformat->Format));
        break;
    default:
        hr = IAudioClient_GetMixFormat(state->pAudioClient, &mix_format);
        EXIT_ON_ERROR(hr);
        waveformat_copy(wformat, mix_format);
        CoTaskMemFree(mix_format);
        MP_VERBOSE(ao, "Fallback to mix format %s\n",
                   waveformat_to_str(&wformat->Format));

    }

    return true;
exit_label:
    MP_ERR(state, "Error finding shared mode format: %s\n",
           mp_HRESULT_to_str(hr));
    return false;
}

static bool find_formats(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    struct mp_chmap channels = ao->channels;

    if (mp_chmap_is_unknown(&channels))
        mp_chmap_from_channels(&channels, channels.num);
    mp_chmap_reorder_to_waveext(&channels);
    if (!mp_chmap_is_valid(&channels))
        mp_chmap_from_channels(&channels, 2);

    struct wasapi_sample_fmt alt_formats[MP_ARRAY_SIZE(wasapi_formats)];
    wasapi_get_best_sample_formats(ao->format, alt_formats);
    struct wasapi_sample_fmt wasapi_format =
        {AF_FORMAT_S16, 16, 16, &KSDATAFORMAT_SUBTYPE_PCM};;
    if (alt_formats[0].mp_format)
        wasapi_format = alt_formats[0];

    AUDCLNT_SHAREMODE share_mode;
    WAVEFORMATEXTENSIBLE wformat;
    set_waveformat(&wformat, &wasapi_format, ao->samplerate, &channels);

    if (state->opt_exclusive || af_fmt_is_spdif(ao->format)) {
        share_mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
        if(!find_formats_exclusive(ao, &wformat))
            return false;
    } else {
        share_mode = AUDCLNT_SHAREMODE_SHARED;
        if(!find_formats_shared(ao, &wformat))
            return false;
    }

    return set_ao_format(ao, &wformat.Format, share_mode);
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

    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
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
    SAFE_RELEASE(state->pSessionControl);
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
    SAFE_RELEASE(state->pEndpointVolume);
    SAFE_RELEASE(state->pAudioVolume);
    MP_WARN(state, "Error setting up volume control: %s\n",
            mp_HRESULT_to_str(hr));
}

static HRESULT fix_format(struct ao *ao, bool align_hack)
{
    struct wasapi_state *state = ao->priv;

    MP_DBG(state, "IAudioClient::GetDevicePeriod\n");
    REFERENCE_TIME devicePeriod;
    HRESULT hr = IAudioClient_GetDevicePeriod(state->pAudioClient,&devicePeriod,
                                              NULL);
    MP_VERBOSE(state, "Device period: %.2g ms\n",
               (double) devicePeriod / 10000.0 );

    REFERENCE_TIME bufferDuration = devicePeriod;
    if (state->share_mode == AUDCLNT_SHAREMODE_SHARED) {
        // for shared mode, use integer multiple of device period close to 50ms
        bufferDuration = devicePeriod * ceil(50.0 * 10000.0 / devicePeriod);
    }

    // handle unsupported buffer size if AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED was
    // returned in a previous attempt. hopefully this shouldn't happen because
    // of the above integer device period
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd370875%28v=vs.85%29.aspx
    if (align_hack) {
        bufferDuration = (REFERENCE_TIME) (0.5 +
            (10000.0 * 1000 / state->format.Format.nSamplesPerSec
             * state->bufferFrameCount));
    }

    REFERENCE_TIME bufferPeriod =
        state->share_mode == AUDCLNT_SHAREMODE_EXCLUSIVE ? bufferDuration : 0;

    MP_DBG(state, "IAudioClient::Initialize\n");
    hr = IAudioClient_Initialize(state->pAudioClient,
                                 state->share_mode,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                 bufferDuration,
                                 bufferPeriod,
                                 &(state->format.Format),
                                 NULL);
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

#if !HAVE_UWP
    state->hTask = AvSetMmThreadCharacteristics(L"Pro Audio", &(DWORD){0});
    if (!state->hTask) {
        MP_WARN(state, "Failed to set AV thread to Pro Audio: %s\n",
                mp_LastError_to_str());
    }
#endif

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
    SAFE_RELEASE(pProps);
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
    SAFE_DESTROY(deviceID, CoTaskMemFree(deviceID));

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
    SAFE_RELEASE(e->pDevices);
    SAFE_RELEASE(e->pEnumerator);
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
    SAFE_RELEASE(pDevice);
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
    SAFE_RELEASE(pDevice);
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

static bool load_device(struct mp_log *l,
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
    SAFE_RELEASE(pEnumerator);
    return SUCCEEDED(hr);
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
    return bstr_strip(bstr0(ao->device));
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
        SAFE_DESTROY(d, talloc_free(d));
    }

    if (!deviceID)
        MP_ERR(ao, "Failed to find device \'%.*s\'\n", BSTR_P(device));

exit_label:
    talloc_free(d);
    destroy_enumerator(enumerator);
    return deviceID;
}

bool wasapi_thread_init(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    MP_DBG(ao, "Init wasapi thread\n");
    int64_t retry_wait = 1;
    bool align_hack = false;
    HRESULT hr;

    ao->format = af_fmt_from_planar(ao->format);

retry:
    if (state->deviceID) {
        if (!load_device(ao->log, &state->pDevice, state->deviceID))
            return false;

        MP_DBG(ao, "Activating pAudioClient interface\n");
        hr = IMMDeviceActivator_Activate(state->pDevice, &IID_IAudioClient,
                                         CLSCTX_ALL, NULL,
                                         (void **)&state->pAudioClient);
        if (FAILED(hr)) {
            MP_FATAL(ao, "Error activating device: %s\n",
                     mp_HRESULT_to_str(hr));
            return false;
        }
    } else {
        MP_VERBOSE(ao, "Trying UWP wrapper.\n");

        HRESULT (*wuCreateDefaultAudioRenderer)(IUnknown **res) = NULL;
        HANDLE lib = LoadLibraryW(L"wasapiuwp2.dll");
        if (!lib) {
            MP_ERR(ao, "Wrapper not found: %d\n", (int)GetLastError());
            return false;
        }

        wuCreateDefaultAudioRenderer =
            (void*)GetProcAddress(lib, "wuCreateDefaultAudioRenderer");
        if (!wuCreateDefaultAudioRenderer) {
            MP_ERR(ao, "Function not found.\n");
            return false;
        }
        IUnknown *res = NULL;
        hr = wuCreateDefaultAudioRenderer(&res);
        MP_VERBOSE(ao, "Device: %s %p\n", mp_HRESULT_to_str(hr), res);
        if (FAILED(hr)) {
            MP_FATAL(ao, "Error activating device: %s\n",
                     mp_HRESULT_to_str(hr));
            return false;
        }
        hr = IUnknown_QueryInterface(res, &IID_IAudioClient,
                                     (void **)&state->pAudioClient);
        IUnknown_Release(res);
        if (FAILED(hr)) {
            MP_FATAL(ao, "Failed to get UWP audio client: %s\n",
                     mp_HRESULT_to_str(hr));
            return false;
        }
    }

    // In the event of an align hack, we've already done this.
    if (!align_hack) {
        MP_DBG(ao, "Probing formats\n");
        if (!find_formats(ao))
            return false;
    }

    MP_DBG(ao, "Fixing format\n");
    hr = fix_format(ao, align_hack);
    switch (hr) {
    case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:
        if (align_hack) {
            MP_FATAL(ao, "Align hack failed\n");
            break;
        }
        // According to MSDN, we must use this as base after the failure.
        hr = IAudioClient_GetBufferSize(state->pAudioClient,
                                        &state->bufferFrameCount);
        if (FAILED(hr)) {
            MP_FATAL(ao, "Error getting buffer size for align hack: %s\n",
                     mp_HRESULT_to_str(hr));
            return false;
        }
        wasapi_thread_uninit(ao);
        align_hack = true;
        MP_WARN(ao, "This appears to require a weird Windows 7 hack. Retrying.\n");
        goto retry;
    case AUDCLNT_E_DEVICE_IN_USE:
    case AUDCLNT_E_DEVICE_INVALIDATED:
        if (retry_wait > 8) {
            MP_FATAL(ao, "Bad device retry failed\n");
            return false;
        }
        wasapi_thread_uninit(ao);
        MP_WARN(ao, "Retrying in %"PRId64" us\n", retry_wait);
        mp_sleep_us(retry_wait);
        retry_wait *= 2;
        goto retry;
    }
    return SUCCEEDED(hr);
}

void wasapi_thread_uninit(struct ao *ao)
{
    struct wasapi_state *state = ao->priv;
    MP_DBG(ao, "Thread shutdown\n");

    if (state->pAudioClient)
        IAudioClient_Stop(state->pAudioClient);

    SAFE_RELEASE(state->pRenderClient);
    SAFE_RELEASE(state->pAudioClock);
    SAFE_RELEASE(state->pAudioVolume);
    SAFE_RELEASE(state->pEndpointVolume);
    SAFE_RELEASE(state->pSessionControl);
    SAFE_RELEASE(state->pAudioClient);
    SAFE_RELEASE(state->pDevice);
#if !HAVE_UWP
    SAFE_DESTROY(state->hTask, AvRevertMmThreadCharacteristics(state->hTask));
#endif
    MP_DBG(ao, "Thread uninit done\n");
}
