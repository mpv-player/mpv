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
#include "core/subopt-helper.h"
#include "audio/format.h"
#include "core/mp_msg.h"
#include "ao.h"

struct priv {
    PaStream *stream;
    int framelen;

    pthread_mutex_t ring_mutex;

    // protected by ring_mutex
    unsigned char *ring;
    int ring_size;      // max size of the ring
    int read_pos;       // points to first byte that can be read
    int read_len;       // number of bytes that can be read
    double play_time;   // time when last packet returned to PA is on speaker
                        // 0 is N/A (0 is not a valid PA time value)
    int play_silence;   // play this many bytes of silence, before real data
    bool play_remaining;// play what's left in the buffer, then stop stream
};

struct format_map {
    int mp_format;
    PaSampleFormat pa_format;
};

static const struct format_map format_maps[] = {
    // first entry is the default format
    {AF_FORMAT_S16_NE,      paInt16},
    {AF_FORMAT_S24_NE,      paInt24},
    {AF_FORMAT_S32_NE,      paInt32},
    {AF_FORMAT_S8,          paInt8},
    {AF_FORMAT_U8,          paUInt8},
    {AF_FORMAT_FLOAT_NE,    paFloat32},
    {AF_FORMAT_UNKNOWN,     0}
};

static void print_help(void)
{
    mp_msg(MSGT_AO, MSGL_FATAL,
           "\n-ao portaudio commandline help:\n"
          "Example: mpv -ao portaudio:device=subdevice\n"
          "\nOptions:\n"
          "   device=subdevice\n"
          "      Audio device PortAudio should use. Devices can be listed\n"
          "      with -ao portaudio:device=help\n"
          "      The subdevice can be passed as index, or as complete name.\n");
}

static bool check_pa_ret(int ret)
{
    if (ret < 0) {
        mp_msg(MSGT_AO, MSGL_ERR, "[portaudio] %s\n",
               Pa_GetErrorText(ret));
        if (ret == paUnanticipatedHostError) {
            const PaHostErrorInfo* hosterr = Pa_GetLastHostErrorInfo();
            mp_msg(MSGT_AO, MSGL_ERR, "[portaudio] Host error: %s\n",
                   hosterr->errorText);
        }
        return false;
    }
    return true;
}

// Amount of bytes that contain audio of the given duration, aligned to frames.
static int seconds_to_bytes(struct ao *ao, double duration_seconds)
{
    struct priv *priv = ao->priv;

    int bytes = duration_seconds * ao->bps;
    if (bytes % priv->framelen)
        bytes += priv->framelen - (bytes % priv->framelen);
    return bytes;
}

static int to_int(const char *s, int return_on_error)
{
    char *endptr;
    int res = strtol(s, &endptr, 10);
    return (s[0] && !endptr[0]) ? res : return_on_error;
}

static int find_device(struct ao *ao, const char *name)
{
    int help = strcmp(name, "help") == 0;
    int count = Pa_GetDeviceCount();
    check_pa_ret(count);
    int found = paNoDevice;
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
        mp_msg(MSGT_AO, MSGL_FATAL, "[portaudio] Device '%s' not found!\n",
               name);
    return found;
}

static int ring_write(struct ao *ao, unsigned char *data, int len)
{
    struct priv *priv = ao->priv;

    int free = priv->ring_size - priv->read_len;
    int write_pos = (priv->read_pos + priv->read_len) % priv->ring_size;
    int write_len = FFMIN(len, free);
    int len1 = FFMIN(priv->ring_size - write_pos, write_len);
    int len2 = write_len - len1;

    memcpy(priv->ring + write_pos, data, len1);
    memcpy(priv->ring, data + len1, len2);

    priv->read_len += write_len;

    return write_len;
}

static int ring_read(struct ao *ao, unsigned char *data, int len)
{
    struct priv *priv = ao->priv;

    int read_len = FFMIN(len, priv->read_len);
    int len1 = FFMIN(priv->ring_size - priv->read_pos, read_len);
    int len2 = read_len - len1;

    memcpy(data, priv->ring + priv->read_pos, len1);
    memcpy(data + len1, priv->ring, len2);

    priv->read_len -= read_len;
    priv->read_pos = (priv->read_pos + read_len) % priv->ring_size;

    return read_len;
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
    int read = ring_read(ao, output, len_bytes);
    len_bytes -= read;
    output += read;

    if (len_bytes > 0) {
        if (priv->play_remaining) {
            res = paComplete;
            priv->play_remaining = false;
        } else {
            mp_msg(MSGT_AO, MSGL_ERR, "[portaudio] Buffer underflow!\n");
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

static int init(struct ao *ao, char *params)
{
    struct priv *priv = talloc_zero(ao, struct priv);
    ao->priv = priv;

    if (!check_pa_ret(Pa_Initialize()))
        return -1;

    pthread_mutex_init(&priv->ring_mutex, NULL);

    char *device = NULL;
    const opt_t subopts[] = {
        {"device", OPT_ARG_MSTRZ, &device, NULL},
        {NULL}
    };
    if (subopt_parse(params, subopts) != 0) {
        print_help();
        goto error_exit;
    }

    int pa_device = Pa_GetDefaultOutputDevice();
    if (device)
        pa_device = find_device(ao, device);
    if (pa_device == paNoDevice)
        goto error_exit;

    mp_chmap_reorder_to_alsa(&ao->channels);

    PaStreamParameters sp = {
        .device = pa_device,
        .channelCount = ao->channels.num,
        .suggestedLatency
            = Pa_GetDeviceInfo(pa_device)->defaultHighOutputLatency,
    };

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
        mp_msg(MSGT_AO, MSGL_V,
               "[portaudio] Unsupported format, using default.\n");
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

    priv->ring_size = seconds_to_bytes(ao, 0.5);
    priv->ring = talloc_zero_size(priv, priv->ring_size);

    free(device);
    return 0;

error_exit:
    uninit(ao, true);
    free(device);
    return -1;
}

static int play(struct ao *ao, void *data, int len, int flags)
{
    struct priv *priv = ao->priv;

    pthread_mutex_lock(&priv->ring_mutex);

    int write_len = ring_write(ao, data, len);
    if (flags & AOPLAY_FINAL_CHUNK)
        priv->play_remaining = true;

    pthread_mutex_unlock(&priv->ring_mutex);

    if (Pa_IsStreamStopped(priv->stream) == 1)
        check_pa_ret(Pa_StartStream(priv->stream));

    return write_len;
}

static int get_space(struct ao *ao)
{
    struct priv *priv = ao->priv;

    pthread_mutex_lock(&priv->ring_mutex);

    int free = priv->ring_size - priv->read_len;

    pthread_mutex_unlock(&priv->ring_mutex);

    return free;
}

static float get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;

    double stream_time = Pa_GetStreamTime(priv->stream);

    pthread_mutex_lock(&priv->ring_mutex);

    float frame_time = priv->play_time ? priv->play_time - stream_time : 0;
    float buffer_latency = (priv->read_len + priv->play_silence)
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

    priv->read_len = 0;
    priv->read_pos = 0;
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

const struct ao_driver audio_out_portaudio = {
    .info = &(const struct ao_info) {
        "PortAudio",
        "portaudio",
        "wm4",
        "",
    },
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = pause,
    .resume    = resume,
};
