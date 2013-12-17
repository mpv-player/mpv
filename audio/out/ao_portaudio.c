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
#include <pthread.h>

#include <libavutil/common.h>
#include <portaudio.h>

#include "config.h"
#include "options/m_option.h"
#include "audio/format.h"
#include "common/msg.h"
#include "misc/ring.h"
#include "ao.h"

struct priv {
    PaStream *stream;
    int framelen;

    pthread_mutex_t ring_mutex;

    // following variables are protected by ring_mutex
    struct mp_ring *ring;
    double play_time;   // time when last packet returned to PA is on speaker
                        // 0 is N/A (0 is not a valid PA time value)
    int play_silence;   // play this many bytes of silence, before real data
    bool play_remaining;// play what's left in the buffer, then stop stream

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

static bool check_pa_ret(int ret)
{
    if (ret < 0) {
        mp_msg(MSGT_AO, MSGL_ERR, "[ao/portaudio] %s\n",
               Pa_GetErrorText(ret));
        if (ret == paUnanticipatedHostError) {
            const PaHostErrorInfo* hosterr = Pa_GetLastHostErrorInfo();
            mp_msg(MSGT_AO, MSGL_ERR, "[ao/portaudio] Host error: %s\n",
                   hosterr->errorText);
        }
        return false;
    }
    return true;
}

static int seconds_to_bytes(struct ao *ao, double seconds)
{
    return af_fmt_seconds_to_bytes(ao->format, seconds, ao->channels.num,
                                   ao->samplerate);
}

static int to_int(const char *s, int return_on_error)
{
    char *endptr;
    int res = strtol(s, &endptr, 10);
    return (s[0] && !endptr[0]) ? res : return_on_error;
}

static int find_device(const char *name)
{
    int found = paNoDevice;
    if (!name)
        return found;
    int help = strcmp(name, "help") == 0;
    int count = Pa_GetDeviceCount();
    check_pa_ret(count);
    int index = to_int(name, -1);
    if (help)
        mp_msg(MSGT_AO, MSGL_INFO, "PortAudio devices:\n");
    for (int n = 0; n < count; n++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(n);
        if (help) {
            if (info->maxOutputChannels < 1)
                continue;
            mp_msg(MSGT_AO, MSGL_INFO, "  %d '%s', %d channels, latency: %.2f "
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
        mp_msg(MSGT_AO, MSGL_WARN, "[ao/portaudio] Device '%s' not found!\n",
               name);
    return found;
}

static int validate_device_opt(const m_option_t *opt, struct bstr name,
                               struct bstr param)
{
    // Note: we do not check whether the device actually exist, because this
    //       might break elaborate configs with several AOs trying several
    //       devices. We do it merely for making "help" special.
    if (bstr_equals0(param, "help")) {
        if (!check_pa_ret(Pa_Initialize()))
            return M_OPT_EXIT;
        find_device("help");
        Pa_Terminate();
        return M_OPT_EXIT - 1;
    }
    return 0;
}

static void fill_silence(unsigned char *ptr, int len)
{
    memset(ptr, 0, len);
}

static int stream_callback(const void *input,
                           void *output_v,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{
    struct ao *ao = userData;
    struct priv *priv = ao->priv;
    int res = paContinue;
    unsigned char *output = output_v;
    int len_bytes = frameCount * priv->framelen;

    pthread_mutex_lock(&priv->ring_mutex);

    // NOTE: PA + ALSA in dmix mode seems to pretend that there is no latency
    //       (outputBufferDacTime == currentTime)
    priv->play_time = timeInfo->outputBufferDacTime
                      + len_bytes / (float)ao->bps;

    if (priv->play_silence > 0) {
        int bytes = FFMIN(priv->play_silence, len_bytes);
        fill_silence(output, bytes);
        priv->play_silence -= bytes;
        len_bytes -= bytes;
        output += bytes;
    }
    int read = mp_ring_read(priv->ring, output, len_bytes);
    len_bytes -= read;
    output += read;

    if (len_bytes > 0) {
        if (priv->play_remaining) {
            res = paComplete;
            priv->play_remaining = false;
        } else {
            MP_ERR(ao, "Buffer underflow!\n");
        }
        fill_silence(output, len_bytes);
    }

    pthread_mutex_unlock(&priv->ring_mutex);

    return res;
}

static void uninit(struct ao *ao, bool cut_audio)
{
    struct priv *priv = ao->priv;

    if (priv->stream) {
        if (!cut_audio && Pa_IsStreamActive(priv->stream) == 1) {
            pthread_mutex_lock(&priv->ring_mutex);

            priv->play_remaining = true;

            pthread_mutex_unlock(&priv->ring_mutex);

            check_pa_ret(Pa_StopStream(priv->stream));
        }
        check_pa_ret(Pa_CloseStream(priv->stream));
    }

    pthread_mutex_destroy(&priv->ring_mutex);
    Pa_Terminate();
}

static int init(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (!check_pa_ret(Pa_Initialize()))
        return -1;

    pthread_mutex_init(&priv->ring_mutex, NULL);

    int pa_device = Pa_GetDefaultOutputDevice();
    if (priv->cfg_device && priv->cfg_device[0])
        pa_device = find_device(priv->cfg_device);
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
    priv->framelen = ao->channels.num * (af_fmt2bits(ao->format) / 8);
    ao->bps = ao->samplerate * priv->framelen;

    if (!check_pa_ret(Pa_IsFormatSupported(NULL, &sp, ao->samplerate)))
        goto error_exit;
    if (!check_pa_ret(Pa_OpenStream(&priv->stream, NULL, &sp, ao->samplerate,
                                    paFramesPerBufferUnspecified, paNoFlag,
                                    stream_callback, ao)))
        goto error_exit;

    priv->ring = mp_ring_new(priv, seconds_to_bytes(ao, 0.5));

    return 0;

error_exit:
    uninit(ao, true);
    return -1;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *priv = ao->priv;

    pthread_mutex_lock(&priv->ring_mutex);

    int write_len = mp_ring_write(priv->ring, data[0], samples * ao->sstride);
    if (flags & AOPLAY_FINAL_CHUNK)
        priv->play_remaining = true;

    pthread_mutex_unlock(&priv->ring_mutex);

    if (Pa_IsStreamStopped(priv->stream) == 1)
        check_pa_ret(Pa_StartStream(priv->stream));

    return write_len / ao->sstride;
}

static int get_space(struct ao *ao)
{
    struct priv *priv = ao->priv;

    pthread_mutex_lock(&priv->ring_mutex);

    int free = mp_ring_available(priv->ring);

    pthread_mutex_unlock(&priv->ring_mutex);

    return free / ao->sstride;
}

static float get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;

    double stream_time = Pa_GetStreamTime(priv->stream);

    pthread_mutex_lock(&priv->ring_mutex);

    float frame_time = priv->play_time ? priv->play_time - stream_time : 0;
    float buffer_latency = (mp_ring_buffered(priv->ring) + priv->play_silence)
                           / (float)ao->bps;

    pthread_mutex_unlock(&priv->ring_mutex);

    return buffer_latency + frame_time;
}

static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (Pa_IsStreamStopped(priv->stream) != 1)
        check_pa_ret(Pa_AbortStream(priv->stream));

    pthread_mutex_lock(&priv->ring_mutex);

    mp_ring_reset(priv->ring);
    priv->play_remaining = false;
    priv->play_time = 0;
    priv->play_silence = 0;

    pthread_mutex_unlock(&priv->ring_mutex);
}

static void pause(struct ao *ao)
{
    struct priv *priv = ao->priv;

    check_pa_ret(Pa_AbortStream(priv->stream));

    double stream_time = Pa_GetStreamTime(priv->stream);

    pthread_mutex_lock(&priv->ring_mutex);

    // When playback resumes, replace the lost audio (due to dropping the
    // portaudio/driver/hardware internal buffers) with silence.
    float frame_time = priv->play_time ? priv->play_time - stream_time : 0;
    priv->play_silence += seconds_to_bytes(ao, FFMAX(frame_time, 0));
    priv->play_time = 0;

    pthread_mutex_unlock(&priv->ring_mutex);
}

static void resume(struct ao *ao)
{
    struct priv *priv = ao->priv;

    check_pa_ret(Pa_StartStream(priv->stream));
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_portaudio = {
    .description = "PortAudio",
    .name      = "portaudio",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = pause,
    .resume    = resume,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_STRING_VALIDATE("device", cfg_device, 0, validate_device_opt),
        {0}
    },
};
