/*
 * OSS audio output driver
 *
 * Original author: A'rpi
 * Support for >2 output channels added 2001-11-25
 * - Steve Davies <steve@daviesfam.org>
 *
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

#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#include "config.h"
#include "options/options.h"
#include "common/common.h"
#include "common/msg.h"
#include "osdep/timer.h"
#include "osdep/endian.h"

#include <sys/soundcard.h>

#include "audio/format.h"

#include "ao.h"
#include "internal.h"

#if !HAVE_GPL
#error GPL only
#endif

// Define to 0 if the device must be reopened to reset it (stop all playback,
// clear the buffer), and the device should be closed when unused.
// Define to 1 if SNDCTL_DSP_RESET should be used to reset without close.
#if defined(SNDCTL_DSP_RESET) && !defined(__NetBSD__)
#define KEEP_DEVICE 1
#else
#define KEEP_DEVICE 0
#endif

#define PATH_DEV_DSP "/dev/dsp"
#define PATH_DEV_MIXER "/dev/mixer"

struct priv {
    int audio_fd;
    int prepause_samples;
    int oss_mixer_channel;
    int audio_delay_method;
    int buffersize;
    int outburst;
    bool device_failed;
    double audio_end;

    char *oss_mixer_device;
    char *cfg_oss_mixer_channel;
};

static const char *const mixer_channels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;

/* like alsa except for 6.1 and 7.1, from pcm/matrix_map.h */
static const struct mp_chmap oss_layouts[MP_NUM_CHANNELS + 1] = {
    {0},                                        // empty
    MP_CHMAP_INIT_MONO,                         // mono
    MP_CHMAP2(FL, FR),                          // stereo
    MP_CHMAP3(FL, FR, LFE),                     // 2.1
    MP_CHMAP4(FL, FR, BL, BR),                  // 4.0
    MP_CHMAP5(FL, FR, BL, BR, FC),              // 5.0
    MP_CHMAP6(FL, FR, BL, BR, FC, LFE),         // 5.1
    MP_CHMAP7(FL, FR, BL, BR, FC, LFE, BC),     // 6.1
    MP_CHMAP8(FL, FR, BL, BR, FC, LFE, SL, SR), // 7.1
};

#if !defined(AFMT_S16_NE) && defined(AFMT_S16_LE) && defined(AFMT_S16_BE)
#define AFMT_S16_NE MP_SELECT_LE_BE(AFMT_S16_LE, AFMT_S16_BE)
#endif

#if !defined(AFMT_S32_NE) && defined(AFMT_S32_LE) && defined(AFMT_S32_BE)
#define AFMT_S32_NE AFMT_S32MP_SELECT_LE_BE(AFMT_S32_LE, AFMT_S32_BE)
#endif

static const int format_table[][2] = {
    {AFMT_U8,           AF_FORMAT_U8},
    {AFMT_S16_NE,       AF_FORMAT_S16},
#ifdef AFMT_S32_NE
    {AFMT_S32_NE,       AF_FORMAT_S32},
#endif
#ifdef AFMT_FLOAT
    {AFMT_FLOAT,        AF_FORMAT_FLOAT},
#endif
#ifdef AFMT_MPEG
    {AFMT_MPEG,         AF_FORMAT_S_MP3},
#endif
    {-1, -1}
};

#ifndef AFMT_AC3
#define AFMT_AC3 -1
#endif

static int format2oss(int format)
{
    for (int n = 0; format_table[n][0] != -1; n++) {
        if (format_table[n][1] == format)
            return format_table[n][0];
    }
    return -1;
}

static int oss2format(int format)
{
    for (int n = 0; format_table[n][0] != -1; n++) {
        if (format_table[n][0] == format)
            return format_table[n][1];
    }
    return 0;
}


#ifdef SNDCTL_DSP_GETPLAYVOL
static int volume_oss4(struct ao *ao, ao_control_vol_t *vol, int cmd)
{
    struct priv *p = ao->priv;
    int v;

    if (p->audio_fd < 0)
        return CONTROL_ERROR;

    if (cmd == AOCONTROL_GET_VOLUME) {
        if (ioctl(p->audio_fd, SNDCTL_DSP_GETPLAYVOL, &v) == -1)
            return CONTROL_ERROR;
        vol->right = (v & 0xff00) >> 8;
        vol->left = v & 0x00ff;
        return CONTROL_OK;
    } else if (cmd == AOCONTROL_SET_VOLUME) {
        v = ((int) vol->right << 8) | (int) vol->left;
        if (ioctl(p->audio_fd, SNDCTL_DSP_SETPLAYVOL, &v) == -1)
            return CONTROL_ERROR;
        return CONTROL_OK;
    } else
        return CONTROL_UNKNOWN;
}
#endif

// to set/get/query special features/parameters
static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
    case AOCONTROL_SET_VOLUME: {
        ao_control_vol_t *vol = (ao_control_vol_t *)arg;
        int fd, v, devs;

#ifdef SNDCTL_DSP_GETPLAYVOL
        // Try OSS4 first
        if (volume_oss4(ao, vol, cmd) == CONTROL_OK)
            return CONTROL_OK;
#endif

        if (!af_fmt_is_pcm(ao->format))
            return CONTROL_TRUE;

        if ((fd = open(p->oss_mixer_device, O_RDONLY)) != -1) {
            ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
            if (devs & (1 << p->oss_mixer_channel)) {
                if (cmd == AOCONTROL_GET_VOLUME) {
                    ioctl(fd, MIXER_READ(p->oss_mixer_channel), &v);
                    vol->right = (v & 0xFF00) >> 8;
                    vol->left = v & 0x00FF;
                } else {
                    v = ((int)vol->right << 8) | (int)vol->left;
                    ioctl(fd, MIXER_WRITE(p->oss_mixer_channel), &v);
                }
            } else {
                close(fd);
                return CONTROL_ERROR;
            }
            close(fd);
            return CONTROL_OK;
        }
        return CONTROL_ERROR;
    }
#ifdef SNDCTL_DSP_GETPLAYVOL
    case AOCONTROL_HAS_SOFT_VOLUME:
        return CONTROL_TRUE;
#endif
    }
    return CONTROL_UNKNOWN;
}

// 1: ok, 0: not writable, -1: error
static int device_writable(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct pollfd fd = {.fd = p->audio_fd, .events = POLLOUT};
    return poll(&fd, 1, 0);
}

static void close_device(struct ao *ao)
{
    struct priv *p = ao->priv;
    p->device_failed = false;
    if (p->audio_fd == -1)
        return;
#if defined(SNDCTL_DSP_RESET)
    ioctl(p->audio_fd, SNDCTL_DSP_RESET, NULL);
#endif
    close(p->audio_fd);
    p->audio_fd = -1;
}

// close audio device
static void uninit(struct ao *ao)
{
    close_device(ao);
}

static bool try_format(struct ao *ao, int *format)
{
    struct priv *p = ao->priv;

    int oss_format = format2oss(*format);
    if (oss_format == -1 && af_fmt_is_spdif(*format))
        oss_format = AFMT_AC3;

    if (oss_format == -1) {
        MP_VERBOSE(ao, "Unknown/not supported internal format: %s\n",
                   af_fmt_to_str(*format));
        *format = 0;
        return false;
    }

    int actual_format = oss_format;
    if (ioctl(p->audio_fd, SNDCTL_DSP_SETFMT, &actual_format) < 0)
        actual_format = -1;

    if (actual_format == oss_format)
        return true;

    MP_WARN(ao, "Can't set audio device to %s output.\n", af_fmt_to_str(*format));
    *format = oss2format(actual_format);
    if (actual_format != -1 && !*format)
        MP_ERR(ao, "Unknown/Unsupported OSS format: 0x%x.\n", actual_format);
    return false;
}

static int reopen_device(struct ao *ao, bool allow_format_changes)
{
    struct priv *p = ao->priv;

    int samplerate = ao->samplerate;
    int format = ao->format;
    struct mp_chmap channels = ao->channels;

    const char *device = PATH_DEV_DSP;
    if (ao->device)
        device = ao->device;

    MP_VERBOSE(ao, "using '%s' dsp device\n", device);
#ifdef __linux__
    p->audio_fd = open(device, O_WRONLY | O_NONBLOCK);
#else
    p->audio_fd = open(device, O_WRONLY);
#endif
    if (p->audio_fd < 0) {
        MP_ERR(ao, "Can't open audio device %s: %s\n",
               device, mp_strerror(errno));
        goto fail;
    }

#ifdef __linux__
    /* Remove the non-blocking flag */
    if (fcntl(p->audio_fd, F_SETFL, 0) < 0) {
        MP_ERR(ao, "Can't make file descriptor blocking: %s\n",
               mp_strerror(errno));
        goto fail;
    }
#endif

#if defined(FD_CLOEXEC) && defined(F_SETFD)
    fcntl(p->audio_fd, F_SETFD, FD_CLOEXEC);
#endif

    if (af_fmt_is_spdif(format)) {
        if (ioctl(p->audio_fd, SNDCTL_DSP_SPEED, &samplerate) == -1)
            goto fail;
        // Probably could be fixed by setting number of channels; needs testing.
        if (channels.num != 2) {
            MP_ERR(ao, "Format %s not implemented.\n", af_fmt_to_str(format));
            goto fail;
        }
    }

    int try_formats[AF_FORMAT_COUNT + 1];
    af_get_best_sample_formats(format, try_formats);
    for (int n = 0; try_formats[n]; n++) {
        format = try_formats[n];
        if (try_format(ao, &format))
            break;
    }

    if (!format) {
        MP_ERR(ao, "Can't set sample format.\n");
        goto fail;
    }

    MP_VERBOSE(ao, "sample format: %s\n", af_fmt_to_str(format));

    if (!af_fmt_is_spdif(format)) {
        struct mp_chmap_sel sel = {0};
        for (int n = 0; n < MP_NUM_CHANNELS + 1; n++)
            mp_chmap_sel_add_map(&sel, &oss_layouts[n]);
        if (!ao_chmap_sel_adjust(ao, &sel, &channels))
            goto fail;
        int reqchannels = channels.num;
        // We only use SNDCTL_DSP_CHANNELS for >2 channels, in case some drivers don't have it
        if (reqchannels > 2) {
            int nchannels = reqchannels;
            if (ioctl(p->audio_fd, SNDCTL_DSP_CHANNELS, &nchannels) == -1 ||
                nchannels != reqchannels)
            {
                MP_ERR(ao, "Failed to set audio device to %d channels.\n",
                       reqchannels);
                goto fail;
            }
        } else {
            int c = reqchannels - 1;
            if (ioctl(p->audio_fd, SNDCTL_DSP_STEREO, &c) == -1) {
                MP_ERR(ao, "Failed to set audio device to %d channels.\n",
                       reqchannels);
                goto fail;
            }
            if (!ao_chmap_sel_get_def(ao, &sel, &channels, c + 1))
                goto fail;
        }
        MP_VERBOSE(ao, "using %d channels (requested: %d)\n",
                   channels.num, reqchannels);
        // set rate
        if (ioctl(p->audio_fd, SNDCTL_DSP_SPEED, &samplerate) == -1)
            goto fail;
        MP_VERBOSE(ao, "using %d Hz samplerate\n", samplerate);
    }

    audio_buf_info zz = {0};
    if (ioctl(p->audio_fd, SNDCTL_DSP_GETOSPACE, &zz) == -1) {
        int r = 0;
        MP_WARN(ao, "driver doesn't support SNDCTL_DSP_GETOSPACE\n");
        if (ioctl(p->audio_fd, SNDCTL_DSP_GETBLKSIZE, &r) == -1)
            MP_VERBOSE(ao, "%d bytes/frag (config.h)\n", p->outburst);
        else {
            p->outburst = r;
            MP_VERBOSE(ao, "%d bytes/frag (GETBLKSIZE)\n", p->outburst);
        }
    } else {
        MP_VERBOSE(ao, "frags: %3d/%d  (%d bytes/frag)  free: %6d\n",
                   zz.fragments, zz.fragstotal, zz.fragsize, zz.bytes);
        p->buffersize = zz.bytes;
        p->outburst = zz.fragsize;
    }

    if (allow_format_changes) {
        ao->format = format;
        ao->samplerate = samplerate;
        ao->channels = channels;
    } else {
        if (format != ao->format || samplerate != ao->samplerate ||
            !mp_chmap_equals(&channels, &ao->channels))
        {
            MP_ERR(ao, "Could not reselect previous audio format.\n");
            goto fail;
        }
    }

    int sstride = channels.num * af_fmt_to_bytes(format);
    p->outburst -= p->outburst % sstride; // round down
    ao->period_size = p->outburst / sstride;

    return 0;

fail:
    close_device(ao);
    return -1;
}

// open & setup audio device
// return: 0=success -1=fail
static int init(struct ao *ao)
{
    struct priv *p = ao->priv;

    const char *mchan = NULL;
    if (p->cfg_oss_mixer_channel && p->cfg_oss_mixer_channel[0])
        mchan = p->cfg_oss_mixer_channel;

    if (mchan) {
        int fd, devs, i;

        if ((fd = open(p->oss_mixer_device, O_RDONLY)) == -1) {
            MP_ERR(ao, "Can't open mixer device %s: %s\n",
                   p->oss_mixer_device, mp_strerror(errno));
        } else {
            ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
            close(fd);

            for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
                if (!strcasecmp(mixer_channels[i], mchan)) {
                    if (!(devs & (1 << i))) {
                        MP_ERR(ao, "Audio card mixer does not have "
                               "channel '%s', using default.\n", mchan);
                        i = SOUND_MIXER_NRDEVICES + 1;
                        break;
                    }
                    p->oss_mixer_channel = i;
                    break;
                }
            }
            if (i == SOUND_MIXER_NRDEVICES) {
                MP_ERR(ao, "Audio card mixer does not have "
                       "channel '%s', using default.\n", mchan);
            }
        }
    } else {
        p->oss_mixer_channel = SOUND_MIXER_PCM;
    }

    MP_VERBOSE(ao, "using '%s' mixer device\n", p->oss_mixer_device);
    MP_VERBOSE(ao, "using '%s' mixer channel\n", mixer_channels[p->oss_mixer_channel]);

    ao->format = af_fmt_from_planar(ao->format);

    if (reopen_device(ao, true) < 0)
        goto fail;

    if (p->buffersize == -1) {
        // Measuring buffer size:
        void *data = malloc(p->outburst);
        if (!data) {
            MP_ERR(ao, "Out of memory, or broken outburst size.\n");
            goto fail;
        }
        p->buffersize = 0;
        memset(data, 0, p->outburst);
        while (p->buffersize < 0x40000 && device_writable(ao) > 0) {
            (void)write(p->audio_fd, data, p->outburst);
            p->buffersize += p->outburst;
        }
        free(data);
        if (p->buffersize == 0) {
            MP_ERR(ao, "Your OSS audio driver DOES NOT support poll().\n");
            goto fail;
        }
    }

    return 0;

fail:
    uninit(ao);
    return -1;
}

static void drain(struct ao *ao)
{
#ifdef SNDCTL_DSP_SYNC
    struct priv *p = ao->priv;
    // to get the buffer played
    if (p->audio_fd != -1)
        ioctl(p->audio_fd, SNDCTL_DSP_SYNC, NULL);
#endif
}

// stop playing and empty buffers (for seeking/pause)
static void reset(struct ao *ao)
{
#if KEEP_DEVICE
    struct priv *p = ao->priv;
    ioctl(p->audio_fd, SNDCTL_DSP_RESET, NULL);
#else
    close_device(ao);
#endif
}

// plays 'len' samples of 'data'
// it should round it down to outburst*n
// return: number of samples played
static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *p = ao->priv;
    int len = samples * ao->sstride;
    if (len == 0)
        return len;

    if (p->audio_fd < 0 && !p->device_failed && reopen_device(ao, false) < 0)
        MP_ERR(ao, "Fatal error: *** CANNOT RE-OPEN / RESET AUDIO DEVICE ***\n");
    if (p->audio_fd < 0) {
        // Let playback continue normally, even with a closed device.
        p->device_failed = true;
        double now = mp_time_sec();
        if (p->audio_end < now)
            p->audio_end = now;
        p->audio_end += samples / (double)ao->samplerate;
        return samples;
    }

    if (len > p->outburst || !(flags & AOPLAY_FINAL_CHUNK)) {
        len /= p->outburst;
        len *= p->outburst;
    }
    len = write(p->audio_fd, data[0], len);
    return len / ao->sstride;
}

// return: delay in seconds between first and last sample in buffer
static double get_delay(struct ao *ao)
{
    struct priv *p = ao->priv;
    if (p->audio_fd < 0) {
        double rest = p->audio_end - mp_time_sec();
        if (rest > 0)
            return rest;
        return 0;
    }
    /* Calculate how many bytes/second is sent out */
    if (p->audio_delay_method == 2) {
#ifdef SNDCTL_DSP_GETODELAY
        int r = 0;
        if (ioctl(p->audio_fd, SNDCTL_DSP_GETODELAY, &r) != -1)
            return r / (double)ao->bps;
#endif
        p->audio_delay_method = 1; // fallback if not supported
    }
    if (p->audio_delay_method == 1) {
        audio_buf_info zz = {0};
        if (ioctl(p->audio_fd, SNDCTL_DSP_GETOSPACE, &zz) != -1) {
            return (p->buffersize - zz.bytes) / (double)ao->bps;
        }
        p->audio_delay_method = 0; // fallback if not supported
    }
    return p->buffersize / (double)ao->bps;
}


// return: how many samples can be played without blocking
static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;

    audio_buf_info zz = {0};
    if (ioctl(p->audio_fd, SNDCTL_DSP_GETOSPACE, &zz) != -1) {
        // calculate exact buffer space:
        return zz.fragments * zz.fragsize / ao->sstride;
    }

    if (p->audio_fd < 0 && p->device_failed && get_delay(ao) > 0.2)
        return 0;

    if (p->audio_fd < 0 || device_writable(ao) > 0)
        return p->outburst / ao->sstride;

    return 0;
}

// stop playing, keep buffers (for pause)
static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;
    p->prepause_samples = get_delay(ao) * ao->samplerate;
#if KEEP_DEVICE
    ioctl(p->audio_fd, SNDCTL_DSP_RESET, NULL);
#else
    close_device(ao);
#endif
}

// resume playing, after audio_pause()
static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    p->audio_end = 0;
    if (p->prepause_samples > 0)
        ao_play_silence(ao, p->prepause_samples);
}

static int audio_wait(struct ao *ao, pthread_mutex_t *lock)
{
    struct priv *p = ao->priv;

    struct pollfd fd = {.fd = p->audio_fd, .events = POLLOUT};
    int r = ao_wait_poll(ao, &fd, 1, lock);
    if (fd.revents & (POLLERR | POLLNVAL))
        return -1;
    return r;
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    if (stat(PATH_DEV_DSP, &(struct stat){0}) == 0)
        ao_device_list_add(list, ao, &(struct ao_device_desc){"", "Default"});
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_oss = {
    .description = "OSS/ioctl audio output",
    .name      = "oss",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .reset     = reset,
    .drain     = drain,
    .wait      = audio_wait,
    .wakeup    = ao_wakeup_poll,
    .list_devs = list_devs,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .audio_fd = -1,
        .audio_delay_method = 2,
        .buffersize = -1,
        .outburst = 512,
        .oss_mixer_channel = SOUND_MIXER_PCM,
        .oss_mixer_device = PATH_DEV_MIXER,
    },
    .options = (const struct m_option[]) {
        OPT_STRING("mixer-device", oss_mixer_device, 0),
        OPT_STRING("mixer-channel", cfg_oss_mixer_channel, 0),
        {0}
    },
    .options_prefix = "oss",
};
