/*
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <libavutil/common.h>
#include <portaudio.h>

#include "config.h"
#include "options/m_option.h"
#include "audio/format.h"
#include "common/msg.h"
#include "osdep/timer.h"
#include "ao.h"
#include "internal.h"

struct priv {
    PaStream *stream;

    // Options
    char *cfg_device;
};

struct format_map {
    int mp_format;
    PaSampleFormat pa_format;
};

static const struct format_map format_maps[] = {
    // first entry is the default format
    {AF_FORMAT_S16,      paInt16},
    {AF_FORMAT_S24,      paInt24},
    {AF_FORMAT_S32,      paInt32},
    {AF_FORMAT_S8,       paInt8},
    {AF_FORMAT_U8,       paUInt8},
    {AF_FORMAT_FLOAT,    paFloat32},
    {AF_FORMAT_UNKNOWN,     0}
};

static bool check_pa_ret(struct mp_log *log, int ret)
{
    if (ret < 0) {
        mp_err(log, "%s\n", Pa_GetErrorText(ret));
        if (ret == paUnanticipatedHostError) {
            const PaHostErrorInfo* hosterr = Pa_GetLastHostErrorInfo();
            mp_err(log, "Host error: %s\n", hosterr->errorText);
        }
        return false;
    }
    return true;
}

#define CHECK_PA_RET(ret) check_pa_ret(ao->log, (ret))

static int to_int(const char *s, int return_on_error)
{
    char *endptr;
    int res = strtol(s, &endptr, 10);
    return (s[0] && !endptr[0]) ? res : return_on_error;
}

static int find_device(struct mp_log *log, const char *name)
{
    int found = paNoDevice;
    if (!name)
        return found;
    int help = strcmp(name, "help") == 0;
    int count = Pa_GetDeviceCount();
    check_pa_ret(log, count);
    int index = to_int(name, -1);
    if (help)
        mp_info(log, "PortAudio devices:\n");
    for (int n = 0; n < count; n++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(n);
        if (help) {
            if (info->maxOutputChannels < 1)
                continue;
            mp_info(log, "  %d '%s', %d channels, latency: %.2f "
                   "ms, sample rate: %.0f\n", n, info->name,
                   info->maxOutputChannels,
                   info->defaultHighOutputLatency * 1000,
                   info->defaultSampleRate);
        }
        if (strcmp(name, info->name) == 0 || n == index) {
            found = n;
            break;
        }
    }
    if (found == paNoDevice && !help)
        mp_warn(log, "Device '%s' not found!\n", name);
    return found;
}

static int validate_device_opt(struct mp_log *log, const m_option_t *opt,
                               struct bstr name, struct bstr param)
{
    // Note: we do not check whether the device actually exist, because this
    //       might break elaborate configs with several AOs trying several
    //       devices. We do it merely for making "help" special.
    if (bstr_equals0(param, "help")) {
        if (!check_pa_ret(log, Pa_Initialize()))
            return M_OPT_EXIT;
        find_device(log, "help");
        Pa_Terminate();
        return M_OPT_EXIT - 1;
    }
    return 0;
}

static int stream_callback(const void *input,
                           void *output_v,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{
    struct ao *ao = userData;

    // NOTE: PA + ALSA in dmix mode seems to pretend that there is no latency
    //       (outputBufferDacTime == currentTime)
    double play_time = timeInfo->outputBufferDacTime
                       + frameCount / (float)ao->samplerate;
    double latency = play_time - timeInfo->currentTime;
    int64_t end = mp_time_us() + MPMAX(0, latency * 1000000.0);

    ao_read_data(ao, &output_v, frameCount, end);

    return paContinue;
}

static void uninit(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (priv->stream) {
        if (Pa_IsStreamActive(priv->stream) == 1)
            CHECK_PA_RET(Pa_StopStream(priv->stream));
        CHECK_PA_RET(Pa_CloseStream(priv->stream));
    }

    Pa_Terminate();
}

static int init(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (!CHECK_PA_RET(Pa_Initialize()))
        return -1;

    int pa_device = Pa_GetDefaultOutputDevice();
    if (priv->cfg_device && priv->cfg_device[0])
        pa_device = find_device(ao->log, priv->cfg_device);
    if (pa_device == paNoDevice)
        goto error_exit;

    // The actual channel order probably depends on the platform.
    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext_def(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        goto error_exit;

    PaStreamParameters sp = {
        .device = pa_device,
        .channelCount = ao->channels.num,
        .suggestedLatency
            = Pa_GetDeviceInfo(pa_device)->defaultHighOutputLatency,
    };

    ao->format = af_fmt_from_planar(ao->format);

    const struct format_map *fmt = format_maps;
    while (fmt->pa_format) {
        if (fmt->mp_format == ao->format) {
            PaStreamParameters test = sp;
            test.sampleFormat = fmt->pa_format;
            if (Pa_IsFormatSupported(NULL, &test, ao->samplerate) == paNoError)
                break;
        }
        fmt++;
    }
    if (!fmt->pa_format) {
        MP_VERBOSE(ao, "Unsupported format, using default.\n");
        fmt = format_maps;
    }

    ao->format = fmt->mp_format;
    sp.sampleFormat = fmt->pa_format;
    int framelen = ao->channels.num * (af_fmt2bits(ao->format) / 8);
    ao->bps = ao->samplerate * framelen;

    if (!CHECK_PA_RET(Pa_IsFormatSupported(NULL, &sp, ao->samplerate)))
        goto error_exit;
    if (!CHECK_PA_RET(Pa_OpenStream(&priv->stream, NULL, &sp, ao->samplerate,
                                    paFramesPerBufferUnspecified, paNoFlag,
                                    stream_callback, ao)))
        goto error_exit;

    return 0;

error_exit:
    uninit(ao);
    return -1;
}

static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (Pa_IsStreamStopped(priv->stream) != 1)
        CHECK_PA_RET(Pa_AbortStream(priv->stream));
}

static void resume(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (Pa_IsStreamStopped(priv->stream) == 1)
        CHECK_PA_RET(Pa_StartStream(priv->stream));
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_portaudio = {
    .description = "PortAudio",
    .name      = "portaudio",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .pause     = reset,
    .resume    = resume,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_STRING_VALIDATE("device", cfg_device, 0, validate_device_opt),
        {0}
    },
};
