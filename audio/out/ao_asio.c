/*
 * This file is part of mpv.
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

// Steinberg ASIO output for mpv. Pull-style AO: the ASIO driver owns the
// real-time callback thread, our bufferSwitch handler reads f32 planar
// samples via ao_read_data() and converts per-channel into the ASIO
// output buffers. mpv's native resampler (f_swresample) handles rate
// conversion AND --video-sync=display-resample AV-sync compensation in
// front of us; the AO just declares the negotiated rate and reports an
// accurate delay so AV-sync works.

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "mpv_talloc.h"

#include "ao_asio.h"
#include "audio/format.h"
#include "audio/chmap.h"
#include "audio/chmap_sel.h"
#include "common/common.h"
#include "options/m_option.h"
#include "osdep/io.h"
#include "osdep/timer.h"

// ---------- Limits & constants ----------

#define ASIO_MAX_DRIVERS  32
#define ASIO_NAME_MAX     128

enum asio_state_flag {
    ASIO_STATE_INIT = 0,
    ASIO_STATE_RUNNING,
    ASIO_STATE_PAUSED,
    ASIO_STATE_SHUTDOWN,
};

enum asio_opt_sample_type {
    ASIO_OPT_TYPE_AUTO  = 0,
    ASIO_OPT_TYPE_I16   = 1,
    ASIO_OPT_TYPE_I24   = 2,
    ASIO_OPT_TYPE_I32   = 3,
    ASIO_OPT_TYPE_F32   = 4,
};

struct asio_driver_entry {
    wchar_t name[ASIO_NAME_MAX];
    CLSID   clsid;
};

struct asio_state {
    struct mp_log *log;
    struct ao     *ao;

    // ASIO driver instance
    IASIO *iasio;
    CLSID  clsid;
    char   driver_name[ASIO_NAME_MAX]; // UTF-8

    // Negotiated configuration
    long           num_in;
    long           num_out;
    int            num_used;       // ao->channels.num
    int            channel_offset; // first physical output channel index used
    ASIOSampleRate sample_rate;
    long           buffer_size;
    long           buf_min, buf_max, buf_pref, buf_gran;
    long           input_lat;
    long           output_lat;
    ASIOSampleType sample_type;
    asio_convert_fn convert_fn;

    // ASIO buffer descriptors (one per used output channel)
    ASIOBufferInfo *buffer_infos;     // talloc array of size num_used
    ASIOChannelInfo *channel_infos;   // talloc array of size num_used
    ASIOCallbacks  callbacks;

    // Scratch planar f32 buffers for ao_read_data
    float **temp_planar;         // talloc array of pointers (num_used)
    float  *temp_planar_storage; // talloc buffer of num_used*buffer_size floats

    // Runtime flags
    _Atomic int state_flag;          // enum asio_state_flag
    _Atomic bool supports_output_ready;
    bool com_initialized;
    bool driver_inited;              // IASIO::init() succeeded
    bool buffers_created;            // IASIO::createBuffers() succeeded

    // Options
    char *opt_device;
    int   opt_buffer_size;
    int   opt_sample_rate;
    int   opt_sample_type;
    int   opt_channel_offset;
};

// ASIO callbacks are plain C function pointers with no user data, so we
// must shuttle through a single global pointer. ASIO drivers are
// single-instance per process anyway (SDK contract), so this is fine.
static struct asio_state *g_active_state;

// ---------- Sample-type converters ----------

static inline float clampf(float v) {
    return v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
}

static void conv_f32(void *dst, const float *src, int n, float vol)
{
    (void)vol;
    memcpy(dst, src, (size_t)n * sizeof(float));
}

static void conv_s32(void *dst, const float *src, int n, float vol)
{
    (void)vol;
    int32_t *d = dst;
    // 2147483647 is not representable as float: scaling by 2147483647.0f
    // rounds full scale to 2^31, and lrintf() of that overflows the 32-bit
    // long, turning +1.0f into INT32_MIN — a full-scale polarity flip on
    // every clipped peak. Scale in double, where INT32_MAX is exact.
    for (int i = 0; i < n; i++)
        d[i] = (int32_t)lrint(clampf(src[i]) * 2147483647.0);
}

static void conv_s24(void *dst, const float *src, int n, float vol)
{
    (void)vol;
    uint8_t *d = dst;
    for (int i = 0; i < n; i++) {
        int32_t v = (int32_t)lrintf(clampf(src[i]) * 8388607.0f);
        // 24-bit LSB-first, packed
        d[i*3 + 0] = (uint8_t)(v & 0xFF);
        d[i*3 + 1] = (uint8_t)((v >> 8) & 0xFF);
        d[i*3 + 2] = (uint8_t)((v >> 16) & 0xFF);
    }
}

static void conv_s16(void *dst, const float *src, int n, float vol)
{
    (void)vol;
    int16_t *d = dst;
    for (int i = 0; i < n; i++)
        d[i] = (int16_t)lrintf(clampf(src[i]) * 32767.0f);
}

static size_t sample_bytes(ASIOSampleType t)
{
    switch (t) {
    case ASIOSTInt16LSB:   return 2;
    case ASIOSTInt24LSB:   return 3;
    case ASIOSTInt32LSB:   return 4;
    case ASIOSTFloat32LSB: return 4;
    default:               return 0;
    }
}

static asio_convert_fn convert_fn_for(ASIOSampleType t)
{
    switch (t) {
    case ASIOSTInt16LSB:   return conv_s16;
    case ASIOSTInt24LSB:   return conv_s24;
    case ASIOSTInt32LSB:   return conv_s32;
    case ASIOSTFloat32LSB: return conv_f32;
    default:               return NULL;
    }
}

static const char *sample_type_name(ASIOSampleType t)
{
    switch (t) {
    case ASIOSTInt16LSB:   return "Int16LSB";
    case ASIOSTInt24LSB:   return "Int24LSB";
    case ASIOSTInt32LSB:   return "Int32LSB";
    case ASIOSTFloat32LSB: return "Float32LSB";
    case ASIOSTInt16MSB:   return "Int16MSB (unsupported)";
    case ASIOSTInt24MSB:   return "Int24MSB (unsupported)";
    case ASIOSTInt32MSB:   return "Int32MSB (unsupported)";
    case ASIOSTFloat32MSB: return "Float32MSB (unsupported)";
    case ASIOSTFloat64MSB: return "Float64MSB (unsupported)";
    case ASIOSTFloat64LSB: return "Float64LSB (unsupported)";
    default:               return "unknown";
    }
}

// ---------- Registry-based driver enumeration ----------

// Reads all ASIO drivers registered under HKLM\SOFTWARE\ASIO. Each
// subkey is a driver display name; the "CLSID" string value holds the
// COM CLSID of the driver. For ASIO, CLSID and IID are the same GUID
// per driver (Steinberg convention).
static int asio_enum_drivers(struct asio_driver_entry *out, int max,
                             struct mp_log *log)
{
    HKEY hkey;
    LONG err = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", 0,
                             KEY_READ, &hkey);
    if (err != ERROR_SUCCESS) {
        if (log)
            mp_verbose(log, "No ASIO drivers registered (RegOpenKeyExW=%ld)\n",
                       err);
        return 0;
    }

    int n = 0;
    for (DWORD i = 0; n < max; i++) {
        wchar_t name[ASIO_NAME_MAX];
        DWORD name_len = ASIO_NAME_MAX;
        if (RegEnumKeyExW(hkey, i, name, &name_len, NULL, NULL, NULL, NULL)
            != ERROR_SUCCESS)
        {
            break;
        }
        HKEY sub;
        if (RegOpenKeyExW(hkey, name, 0, KEY_READ, &sub) != ERROR_SUCCESS)
            continue;
        wchar_t clsid_str[64];
        DWORD clsid_len = sizeof(clsid_str);
        DWORD type = 0;
        if (RegQueryValueExW(sub, L"CLSID", NULL, &type, (BYTE *)clsid_str,
                             &clsid_len) == ERROR_SUCCESS && type == REG_SZ)
        {
            clsid_str[(clsid_len / sizeof(wchar_t)) - 1] = 0;
            CLSID clsid;
            if (CLSIDFromString(clsid_str, &clsid) == NOERROR) {
                wcsncpy(out[n].name, name, ASIO_NAME_MAX - 1);
                out[n].name[ASIO_NAME_MAX - 1] = 0;
                out[n].clsid = clsid;
                n++;
            }
        }
        RegCloseKey(sub);
    }
    RegCloseKey(hkey);
    return n;
}

// Load the driver matching `wanted` (UTF-8 display name). If `wanted`
// is NULL/empty, picks the first available driver. Returns 0 on
// success; on success, *out is the live IASIO* pointer, and *clsid is
// the driver's CLSID.
static int asio_load_driver(struct mp_log *log, const char *wanted_utf8,
                            IASIO **out_iasio, CLSID *out_clsid,
                            wchar_t *out_name)
{
    struct asio_driver_entry entries[ASIO_MAX_DRIVERS];
    int n = asio_enum_drivers(entries, ASIO_MAX_DRIVERS, log);
    if (n == 0) {
        mp_err(log, "No ASIO drivers found in HKLM\\SOFTWARE\\ASIO\n");
        return -1;
    }

    wchar_t *wwanted = NULL;
    if (wanted_utf8 && wanted_utf8[0])
        wwanted = mp_from_utf8(NULL, wanted_utf8);

    int idx = -1;
    if (!wwanted) {
        idx = 0;
        mp_verbose(log, "No ASIO driver requested; using first available\n");
    } else {
        for (int i = 0; i < n; i++) {
            if (wcscmp(entries[i].name, wwanted) == 0) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            mp_err(log, "ASIO driver not found. Available drivers:\n");
            for (int i = 0; i < n; i++) {
                char *u = mp_to_utf8(NULL, entries[i].name);
                mp_err(log, "  %s\n", u);
                talloc_free(u);
            }
        }
        talloc_free(wwanted);
        if (idx < 0)
            return -1;
    }

    // ASIO drivers use CLSID == IID (Steinberg convention). Try that
    // first; if it fails, fall back to QI from IUnknown.
    IASIO *iasio = NULL;
    HRESULT hr = CoCreateInstance(&entries[idx].clsid, NULL,
                                  CLSCTX_INPROC_SERVER,
                                  &entries[idx].clsid, (void **)&iasio);
    if (FAILED(hr) || !iasio) {
        IUnknown *unk = NULL;
        hr = CoCreateInstance(&entries[idx].clsid, NULL,
                              CLSCTX_INPROC_SERVER,
                              &IID_IUnknown, (void **)&unk);
        if (SUCCEEDED(hr) && unk) {
            hr = IUnknown_QueryInterface(unk, &entries[idx].clsid,
                                         (void **)&iasio);
            IUnknown_Release(unk);
        }
    }
    if (FAILED(hr) || !iasio) {
        char *u = mp_to_utf8(NULL, entries[idx].name);
        mp_err(log, "CoCreateInstance failed for ASIO driver '%s': %s\n",
               u, mp_HRESULT_to_str(hr));
        talloc_free(u);
        return -1;
    }

    *out_iasio = iasio;
    *out_clsid = entries[idx].clsid;
    if (out_name) {
        wcsncpy(out_name, entries[idx].name, ASIO_NAME_MAX - 1);
        out_name[ASIO_NAME_MAX - 1] = 0;
    }
    return 0;
}

// ---------- ASIO host callbacks (driver -> us) ----------

static void cb_buffer_switch(long idx, ASIOBool direct)
{
    (void)direct;
    struct asio_state *s = g_active_state;
    if (!s)
        return;

    int flag = atomic_load(&s->state_flag);
    size_t bytes_per_sample = sample_bytes(s->sample_type);

    if (flag != ASIO_STATE_RUNNING) {
        for (int c = 0; c < s->num_used; c++) {
            memset(s->buffer_infos[c].buffers[idx], 0,
                   (size_t)s->buffer_size * bytes_per_sample);
        }
        return;
    }

    // Delay accounting: the last sample of this block will physically emit
    // after the current block drains plus the driver's reported output
    // latency. ao_read_data uses this for AV-sync and display-resample.
    long frames_ahead = s->output_lat + s->buffer_size;
    int64_t delay_ns = (int64_t)((double)frames_ahead * 1e9 / s->sample_rate);
    int64_t out_time = mp_time_ns() + delay_ns;

    void *planes[MP_NUM_CHANNELS];
    for (int c = 0; c < s->num_used; c++)
        planes[c] = s->temp_planar[c];

    ao_read_data(s->ao, planes, s->buffer_size, out_time, NULL,
                 /* pad_silence= */ true, /* blocking= */ false);

    for (int c = 0; c < s->num_used; c++) {
        s->convert_fn(s->buffer_infos[c].buffers[idx],
                      s->temp_planar[c], s->buffer_size, 1.0f);
    }

    if (atomic_load(&s->supports_output_ready))
        IASIO_OutputReady(s->iasio);
}

static ASIOTime *cb_buffer_switch_time_info(ASIOTime *params, long idx,
                                            ASIOBool direct)
{
    // For v1 we ignore the supplied time info and reuse the simple path;
    // ASIOGetLatencies + buffer_size already gives us accurate enough
    // delay reporting. A v2 refinement could use params->timeInfo for
    // sub-block-precision delay.
    cb_buffer_switch(idx, direct);
    return params;
}

static void cb_sample_rate_did_change(ASIOSampleRate r)
{
    struct asio_state *s = g_active_state;
    if (!s)
        return;
    MP_INFO(s, "ASIO driver reports sample rate change to %.0f Hz; "
            "requesting reload\n", (double)r);
    ao_request_reload(s->ao);
}

static long cb_asio_message(long selector, long value, void *msg, double *opt)
{
    (void)msg; (void)opt;
    struct asio_state *s = g_active_state;
    if (!s)
        return 0;

    switch (selector) {
    case kAsioSelectorSupported:
        switch (value) {
        case kAsioEngineVersion:
        case kAsioResetRequest:
        case kAsioBufferSizeChange:
        case kAsioResyncRequest:
        case kAsioLatenciesChanged:
        case kAsioSupportsTimeInfo:
        case kAsioOverload:
            return 1;
        default:
            return 0;
        }
    case kAsioEngineVersion:
        return 2;
    case kAsioResetRequest:
        MP_INFO(s, "ASIO reset requested; reloading\n");
        ao_request_reload(s->ao);
        return 1;
    case kAsioBufferSizeChange:
        MP_INFO(s, "ASIO buffer size change requested (%ld); reloading\n",
                value);
        ao_request_reload(s->ao);
        return 1;
    case kAsioResyncRequest:
        MP_WARN(s, "ASIO resync requested\n");
        return 1;
    case kAsioLatenciesChanged:
        if (s->iasio) {
            long in, out;
            if (IASIO_GetLatencies(s->iasio, &in, &out) == ASE_OK) {
                s->input_lat = in;
                s->output_lat = out;
                MP_VERBOSE(s, "ASIO latencies updated: in=%ld out=%ld frames\n",
                           in, out);
            }
        }
        return 1;
    case kAsioSupportsTimeInfo:
        return 1;
    case kAsioOverload:
        MP_WARN(s, "ASIO driver reported overload\n");
        return 1;
    default:
        return 0;
    }
}

// ---------- Lifecycle helpers ----------

static void asio_teardown(struct ao *ao)
{
    struct asio_state *s = ao->priv;

    if (s->iasio) {
        if (s->buffers_created) {
            // ASIOStop is implied by ASIODisposeBuffers but explicit is
            // safer; ignore errors.
            IASIO_Stop(s->iasio);
            IASIO_DisposeBuffers(s->iasio);
            s->buffers_created = false;
        }
        IASIO_Release(s->iasio);
        s->iasio = NULL;
        s->driver_inited = false;
    }

    talloc_free(s->buffer_infos);
    s->buffer_infos = NULL;
    talloc_free(s->channel_infos);
    s->channel_infos = NULL;
    talloc_free(s->temp_planar);
    s->temp_planar = NULL;
    talloc_free(s->temp_planar_storage);
    s->temp_planar_storage = NULL;

    if (s->com_initialized) {
        CoUninitialize();
        s->com_initialized = false;
    }

    if (g_active_state == s)
        g_active_state = NULL;
}

static int negotiate_sample_rate(struct asio_state *s)
{
    static const double candidates[] = {
        48000.0, 44100.0, 96000.0, 192000.0,
        88200.0, 176400.0, 32000.0,
    };

    double primary = s->opt_sample_rate > 0
        ? (double)s->opt_sample_rate
        : (double)s->ao->samplerate;

    if (primary > 0 && IASIO_CanSampleRate(s->iasio, primary) == ASE_OK) {
        if (IASIO_SetSampleRate(s->iasio, primary) == ASE_OK) {
            s->sample_rate = primary;
            return 0;
        }
    }

    for (size_t i = 0; i < MP_ARRAY_SIZE(candidates); i++) {
        double r = candidates[i];
        if (IASIO_CanSampleRate(s->iasio, r) != ASE_OK)
            continue;
        if (IASIO_SetSampleRate(s->iasio, r) == ASE_OK) {
            s->sample_rate = r;
            return 0;
        }
    }

    // Last chance: whatever the driver is set to.
    ASIOSampleRate r = 0;
    if (IASIO_GetSampleRate(s->iasio, &r) == ASE_OK && r > 0) {
        s->sample_rate = r;
        return 0;
    }
    return -1;
}

static long pick_buffer_size(struct asio_state *s)
{
    if (s->opt_buffer_size <= 0)
        return s->buf_pref;

    long want = s->opt_buffer_size;
    if (want < s->buf_min) want = s->buf_min;
    if (want > s->buf_max) want = s->buf_max;
    if (s->buf_gran > 0) {
        // Snap to granularity from the minimum.
        long delta = want - s->buf_min;
        long snap = (delta + s->buf_gran / 2) / s->buf_gran;
        want = s->buf_min + snap * s->buf_gran;
        if (want > s->buf_max) want = s->buf_max;
        if (want < s->buf_min) want = s->buf_min;
    } else if (s->buf_gran == -1) {
        // Powers of two from min to max.
        long p = s->buf_min;
        while (p * 2 <= want && p * 2 <= s->buf_max)
            p *= 2;
        want = p;
    }
    return want;
}

static int pick_sample_type(struct asio_state *s)
{
    // ASIO does not let you force a sample type — each channel has a
    // fixed native type. We honor the driver's type unless the user
    // explicitly requested one that doesn't match (in which case fail).
    ASIOSampleType native = s->channel_infos[0].type;
    for (int c = 1; c < s->num_used; c++) {
        if (s->channel_infos[c].type != native) {
            MP_ERR(s, "ASIO channels report mixed sample types (%s vs %s); "
                   "unsupported\n",
                   sample_type_name(native),
                   sample_type_name(s->channel_infos[c].type));
            return -1;
        }
    }

    if (s->opt_sample_type != ASIO_OPT_TYPE_AUTO) {
        ASIOSampleType wanted;
        switch (s->opt_sample_type) {
        case ASIO_OPT_TYPE_I16: wanted = ASIOSTInt16LSB;   break;
        case ASIO_OPT_TYPE_I24: wanted = ASIOSTInt24LSB;   break;
        case ASIO_OPT_TYPE_I32: wanted = ASIOSTInt32LSB;   break;
        case ASIO_OPT_TYPE_F32: wanted = ASIOSTFloat32LSB; break;
        default:                wanted = native;           break;
        }
        if (wanted != native) {
            MP_ERR(s, "ASIO driver uses %s but --asio-sample-type forces "
                   "a different type. The driver does not allow this.\n",
                   sample_type_name(native));
            return -1;
        }
    }

    asio_convert_fn fn = convert_fn_for(native);
    if (!fn) {
        MP_ERR(s, "Unsupported ASIO sample type: %s. Only Int16LSB, "
               "Int24LSB, Int32LSB and Float32LSB are supported.\n",
               sample_type_name(native));
        return -1;
    }
    s->sample_type = native;
    s->convert_fn = fn;
    return 0;
}

// ---------- ao_driver entry points ----------

static void uninit(struct ao *ao)
{
    struct asio_state *s = ao->priv;
    MP_DBG(s, "Uninit ASIO\n");
    atomic_store(&s->state_flag, ASIO_STATE_SHUTDOWN);
    asio_teardown(ao);
}

static int init(struct ao *ao)
{
    struct asio_state *s = ao->priv;
    s->log = ao->log;
    s->ao  = ao;
    atomic_store(&s->state_flag, ASIO_STATE_INIT);
    atomic_store(&s->supports_output_ready, false);

    if (g_active_state) {
        MP_ERR(ao, "Another ao_asio instance is already active; ASIO is "
               "single-instance per process.\n");
        return -1;
    }
    g_active_state = s;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    // S_OK or S_FALSE means we own a refcount; RPC_E_CHANGED_MODE means
    // someone else already chose MTA on this thread, which is fine for
    // most ASIO drivers in practice.
    s->com_initialized = SUCCEEDED(hr);

    // Resolve driver name: --audio-device=asio/<name> stripped to <name>
    // is in ao->device; --asio-device is the explicit option.
    const char *wanted = (ao->device && ao->device[0]) ? ao->device
                                                       : s->opt_device;
    wchar_t wname[ASIO_NAME_MAX] = {0};
    MP_VERBOSE(ao, "Loading ASIO driver '%s' (this may take a moment)...\n",
               wanted ? wanted : "<first available>");
    if (asio_load_driver(ao->log, wanted, &s->iasio, &s->clsid, wname) < 0)
        goto fail;

    char *u_name = mp_to_utf8(NULL, wname);
    snprintf(s->driver_name, sizeof(s->driver_name), "%s", u_name);
    talloc_free(u_name);
    MP_VERBOSE(ao, "Loaded ASIO driver: %s\n", s->driver_name);

    // ASIO drivers historically expect a HWND in sysHandle on Windows
    // even though most ignore it. The desktop window is a safe default.
    if (!IASIO_Init(s->iasio, (void *)GetDesktopWindow())) {
        char errbuf[128] = {0};
        IASIO_GetErrorMessage(s->iasio, errbuf);
        MP_ERR(ao, "IASIO::init failed: %s\n", errbuf);
        goto fail;
    }
    s->driver_inited = true;

    if (IASIO_GetChannels(s->iasio, &s->num_in, &s->num_out) != ASE_OK
        || s->num_out <= 0)
    {
        MP_ERR(ao, "IASIO::getChannels failed or no output channels\n");
        goto fail;
    }
    MP_VERBOSE(ao, "Driver reports %ld input / %ld output channels\n",
               s->num_in, s->num_out);

    s->channel_offset = s->opt_channel_offset;
    if (s->channel_offset >= s->num_out) {
        MP_ERR(ao, "channel-offset=%d exceeds driver outputs (%ld)\n",
               s->channel_offset, s->num_out);
        goto fail;
    }
    int avail = (int)s->num_out - s->channel_offset;

    // Channel layout negotiation: advertise the full waveext set (mono up to
    // 9.1.6, including 5.1.4 and 7.1.4 height layouts) so spatial inputs
    // (Atmos beds + objects rendered to N speakers) aren't downmixed to 7.1
    // before they reach the driver. waveext also forces a stable, well-known
    // channel order that ASIO drivers expose on their physical outputs.
    // mp_chmap_from_channels only knows layouts up to 7.1 (default_layouts[]
    // stops at 8) so it would otherwise cap the negotiated output at 8 ch.
    struct mp_chmap_sel chmap_sel = {0};
    mp_chmap_sel_add_waveext(&chmap_sel);
    // Also accept any chmap that fits the available physical outputs — covers
    // unusual driver configurations (e.g. 24-ch MOTU surfaces).
    if (avail > 0)
        chmap_sel.allow_any = true;
    if (!ao_chmap_sel_adjust(ao, &chmap_sel, &ao->channels)) {
        MP_ERR(ao, "Could not negotiate a channel layout within the %d "
               "available ASIO output channels\n", avail);
        goto fail;
    }
    s->num_used = ao->channels.num;
    MP_VERBOSE(ao, "Using %d channels starting at physical channel %d\n",
               s->num_used, s->channel_offset);

    if (negotiate_sample_rate(s) < 0) {
        MP_ERR(ao, "Could not select a sample rate on the ASIO driver\n");
        goto fail;
    }
    MP_VERBOSE(ao, "Sample rate: %.0f Hz\n", (double)s->sample_rate);

    if (IASIO_GetBufferSize(s->iasio, &s->buf_min, &s->buf_max,
                            &s->buf_pref, &s->buf_gran) != ASE_OK)
    {
        MP_ERR(ao, "IASIO::getBufferSize failed\n");
        goto fail;
    }
    s->buffer_size = pick_buffer_size(s);
    MP_VERBOSE(ao, "Buffer size: %ld frames (min=%ld max=%ld pref=%ld gran=%ld)\n",
               s->buffer_size, s->buf_min, s->buf_max, s->buf_pref, s->buf_gran);

    s->channel_infos = talloc_zero_array(NULL, ASIOChannelInfo, s->num_used);
    for (int c = 0; c < s->num_used; c++) {
        s->channel_infos[c].channel = s->channel_offset + c;
        s->channel_infos[c].isInput = ASIOFalse;
        if (IASIO_GetChannelInfo(s->iasio, &s->channel_infos[c]) != ASE_OK) {
            MP_ERR(ao, "IASIO::getChannelInfo failed for channel %ld\n",
                   s->channel_infos[c].channel);
            goto fail;
        }
    }

    if (pick_sample_type(s) < 0)
        goto fail;
    MP_VERBOSE(ao, "Driver sample type: %s\n", sample_type_name(s->sample_type));

    s->buffer_infos = talloc_zero_array(NULL, ASIOBufferInfo, s->num_used);
    for (int c = 0; c < s->num_used; c++) {
        s->buffer_infos[c].isInput   = ASIOFalse;
        s->buffer_infos[c].channelNum = s->channel_offset + c;
    }

    s->callbacks.bufferSwitch         = cb_buffer_switch;
    s->callbacks.sampleRateDidChange  = cb_sample_rate_did_change;
    s->callbacks.asioMessage          = cb_asio_message;
    s->callbacks.bufferSwitchTimeInfo = cb_buffer_switch_time_info;

    if (IASIO_CreateBuffers(s->iasio, s->buffer_infos, s->num_used,
                            s->buffer_size, &s->callbacks) != ASE_OK)
    {
        MP_ERR(ao, "IASIO::createBuffers failed (size=%ld, channels=%d)\n",
               s->buffer_size, s->num_used);
        goto fail;
    }
    s->buffers_created = true;

    if (IASIO_GetLatencies(s->iasio, &s->input_lat, &s->output_lat) != ASE_OK) {
        MP_WARN(ao, "IASIO::getLatencies failed; assuming output_lat=0\n");
        s->input_lat = 0;
        s->output_lat = 0;
    }
    MP_VERBOSE(ao, "Latencies: input=%ld output=%ld frames\n",
               s->input_lat, s->output_lat);

    // Probe ASIOOutputReady support (defaults to "no" if call fails).
    atomic_store(&s->supports_output_ready,
                 IASIO_OutputReady(s->iasio) == ASE_OK);

    // Allocate scratch f32 buffers (one per used channel).
    size_t total = (size_t)s->num_used * (size_t)s->buffer_size;
    s->temp_planar_storage = talloc_array(NULL, float, total);
    memset(s->temp_planar_storage, 0, total * sizeof(float));
    s->temp_planar = talloc_array(NULL, float *, s->num_used);
    for (int c = 0; c < s->num_used; c++)
        s->temp_planar[c] = s->temp_planar_storage + (size_t)c * s->buffer_size;

    // Publish negotiated values back to mpv. f_swresample will be wired
    // upstream with these as the target.
    ao->samplerate    = (int)s->sample_rate;
    ao->format        = AF_FORMAT_FLOATP;
    ao->device_buffer = s->buffer_size;

    MP_VERBOSE(ao, "ao_asio init done: %d Hz, %d ch, FLOATP, %ld-frame buffer\n",
               ao->samplerate, s->num_used, s->buffer_size);
    return 0;

fail:
    asio_teardown(ao);
    return -1;
}

static void audio_start(struct ao *ao)
{
    struct asio_state *s = ao->priv;
    atomic_store(&s->state_flag, ASIO_STATE_RUNNING);
    ASIOError err = IASIO_Start(s->iasio);
    if (err != ASE_OK) {
        MP_ERR(ao, "IASIO::start failed (%ld); requesting reload\n", (long)err);
        atomic_store(&s->state_flag, ASIO_STATE_PAUSED);
        ao_request_reload(ao);
    }
}

static void audio_reset(struct ao *ao)
{
    struct asio_state *s = ao->priv;
    atomic_store(&s->state_flag, ASIO_STATE_PAUSED);
    IASIO_Stop(s->iasio);
}

static bool audio_set_pause(struct ao *ao, bool paused)
{
    struct asio_state *s = ao->priv;
    if (paused) {
        atomic_store(&s->state_flag, ASIO_STATE_PAUSED);
        IASIO_Stop(s->iasio);
    } else {
        atomic_store(&s->state_flag, ASIO_STATE_RUNNING);
        ASIOError err = IASIO_Start(s->iasio);
        if (err != ASE_OK) {
            MP_ERR(ao, "IASIO::start failed on unpause (%ld)\n", (long)err);
            atomic_store(&s->state_flag, ASIO_STATE_PAUSED);
            return false;
        }
    }
    return true;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    (void)ao; (void)arg;
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
    case AOCONTROL_SET_VOLUME:
    case AOCONTROL_GET_MUTE:
    case AOCONTROL_SET_MUTE:
        // ASIO exposes no hardware mixer; mpv's software volume gain is
        // already applied by ao_read_data() upstream.
        return CONTROL_FALSE;
    case AOCONTROL_UPDATE_STREAM_TITLE:
        // ASIO drivers have no display channel.
        return CONTROL_FALSE;
    }
    return CONTROL_UNKNOWN;
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    bool need_uninit = false;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
        need_uninit = true;

    struct asio_driver_entry entries[ASIO_MAX_DRIVERS];
    int n = asio_enum_drivers(entries, ASIO_MAX_DRIVERS, ao->log);
    for (int i = 0; i < n; i++) {
        char *u = mp_to_utf8(NULL, entries[i].name);
        ao_device_list_add(list, ao, &(struct ao_device_desc){u, u});
        talloc_free(u);
    }

    if (need_uninit)
        CoUninitialize();
}

#define OPT_BASE_STRUCT struct asio_state

const struct ao_driver audio_out_asio = {
    .description    = "Steinberg ASIO audio output",
    .name           = "asio",
    .init           = init,
    .uninit         = uninit,
    .control        = control,
    .reset          = audio_reset,
    .start          = audio_start,
    .set_pause      = audio_set_pause,
    .list_devs      = list_devs,
    .priv_size      = sizeof(struct asio_state),
    .options_prefix = "asio",
    .options        = (const struct m_option[]) {
        {"device",         OPT_STRING(opt_device)},
        {"buffer-size",    OPT_INT(opt_buffer_size), M_RANGE(0, 65536)},
        {"sample-rate",    OPT_INT(opt_sample_rate), M_RANGE(0, 384000)},
        {"sample-type",    OPT_CHOICE(opt_sample_type,
                              {"auto",    ASIO_OPT_TYPE_AUTO},
                              {"int16",   ASIO_OPT_TYPE_I16},
                              {"int24",   ASIO_OPT_TYPE_I24},
                              {"int32",   ASIO_OPT_TYPE_I32},
                              {"float32", ASIO_OPT_TYPE_F32})},
        {"channel-offset", OPT_INT(opt_channel_offset), M_RANGE(0, 255)},
        {0}
    },
};
