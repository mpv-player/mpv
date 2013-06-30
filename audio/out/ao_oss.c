/*
 * OSS audio output driver
 *
 * This file is part of MPlayer.
 *
 * Support for >2 output channels added 2001-11-25
 * - Steve Davies <steve@daviesfam.org>
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "mpvcore/options.h"
#include "mpvcore/mp_msg.h"

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#ifdef HAVE_SOUNDCARD_H
#include <soundcard.h>
#endif
#endif

#include "audio/format.h"

#include "ao.h"

struct priv {
    int audio_fd;
    int prepause_space;
    int oss_mixer_channel;
    audio_buf_info zz;
    int audio_delay_method;
    int buffersize;
    int outburst;

    char *dsp;
    char *oss_mixer_device;
    char *cfg_oss_mixer_channel;
};

static const char *mixer_channels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;

static int format_table[][2] = {
    {AFMT_U8,           AF_FORMAT_U8},
    {AFMT_S8,           AF_FORMAT_S8},
    {AFMT_U16_LE,       AF_FORMAT_U16_LE},
    {AFMT_U16_BE,       AF_FORMAT_U16_BE},
    {AFMT_S16_LE,       AF_FORMAT_S16_LE},
    {AFMT_S16_BE,       AF_FORMAT_S16_BE},
#ifdef AFMT_S24_PACKED
    {AFMT_S24_PACKED,   AF_FORMAT_S24_LE},
#endif
#ifdef AFMT_U32_LE
    {AFMT_U32_LE,       AF_FORMAT_U32_LE},
#endif
#ifdef AFMT_U32_BE
    {AFMT_U32_BE,       AF_FORMAT_U32_BE},
#endif
#ifdef AFMT_S32_LE
    {AFMT_S32_LE,       AF_FORMAT_S32_LE},
#endif
#ifdef AFMT_S32_BE
    {AFMT_S32_BE,       AF_FORMAT_S32_BE},
#endif
#ifdef AFMT_FLOAT
    {AFMT_FLOAT,        AF_FORMAT_FLOAT_NE},
#endif
    // SPECIALS
#ifdef AFMT_MPEG
    {AFMT_MPEG,         AF_FORMAT_MPEG2},
#endif
#ifdef AFMT_AC3
    {AFMT_AC3,          AF_FORMAT_AC3_NE},
#endif
    {-1, -1}
};

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
    return -1;
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
    case AOCONTROL_SET_VOLUME:
    {
        ao_control_vol_t *vol = (ao_control_vol_t *)arg;
        int fd, v, devs;

#ifdef SNDCTL_DSP_GETPLAYVOL
        // Try OSS4 first
        if (volume_oss4(ao, vol, cmd) == CONTROL_OK)
            return CONTROL_OK;
#endif

        if (AF_FORMAT_IS_AC3(ao->format))
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
    }
        return CONTROL_ERROR;
    }
    return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 0=success -1=fail
static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    int oss_format;

    const char *mchan = NULL;
    if (p->cfg_oss_mixer_channel && p->cfg_oss_mixer_channel[0])
        mchan = p->cfg_oss_mixer_channel;

    MP_VERBOSE(ao, "%d Hz  %d chans  %s\n", ao->samplerate,
               ao->channels.num, af_fmt2str_short(ao->format));

    if (mchan) {
        int fd, devs, i;

        if ((fd = open(p->oss_mixer_device, O_RDONLY)) == -1) {
            MP_ERR(ao, "Can't open mixer device %s: %s\n",
                   p->oss_mixer_device, strerror(errno));
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

    MP_VERBOSE(ao, "using '%s' dsp device\n", p->dsp);
    MP_VERBOSE(ao, "using '%s' mixer device\n", p->oss_mixer_device);
    MP_VERBOSE(ao, "using '%s' mixer device\n", mixer_channels[p->oss_mixer_channel]);

#ifdef __linux__
    p->audio_fd = open(p->dsp, O_WRONLY | O_NONBLOCK);
#else
    p->audio_fd = open(p->dsp, O_WRONLY);
#endif
    if (p->audio_fd < 0) {
        MP_ERR(ao, "Can't open audio device %s: %s\n", p->dsp, strerror(errno));
        return -1;
    }

#ifdef __linux__
    /* Remove the non-blocking flag */
    if (fcntl(p->audio_fd, F_SETFL, 0) < 0) {
        MP_ERR(ao, "Can't make file descriptor blocking: %s\n",  strerror(errno));
        return -1;
    }
#endif

#if defined(FD_CLOEXEC) && defined(F_SETFD)
    fcntl(p->audio_fd, F_SETFD, FD_CLOEXEC);
#endif

    if (AF_FORMAT_IS_AC3(ao->format)) {
        ioctl(p->audio_fd, SNDCTL_DSP_SPEED, &ao->samplerate);
    }

ac3_retry:
    if (AF_FORMAT_IS_AC3(ao->format))
        ao->format = AF_FORMAT_AC3_NE;
    oss_format = format2oss(ao->format);
    if (oss_format == -1) {
        MP_VERBOSE(ao, "Unknown/not supported internal format: %s\n",
                   af_fmt2str_short(ao->format));
#if BYTE_ORDER == BIG_ENDIAN
        oss_format = AFMT_S16_BE;
#else
        oss_format = AFMT_S16_LE;
#endif
        ao->format = AF_FORMAT_S16_NE;
    }
    if (ioctl(p->audio_fd, SNDCTL_DSP_SETFMT, &oss_format) < 0 ||
        oss_format != format2oss(ao->format))
    {
        MP_WARN(ao, "Can't set audio device %s to %s output, trying %s...\n",
                p->dsp, af_fmt2str_short(ao->format),
                af_fmt2str_short(AF_FORMAT_S16_NE));
        ao->format = AF_FORMAT_S16_NE;
        goto ac3_retry;
    }

    ao->format = oss2format(oss_format);
    if (ao->format == -1) {
        MP_ERR(ao, "Unknown/Unsupported OSS format: %x.\n", oss_format);
        return -1;
    }

    MP_VERBOSE(ao, "sample format: %s\n", af_fmt2str_short(ao->format));

    if (!AF_FORMAT_IS_AC3(ao->format)) {
        struct mp_chmap_sel sel = {0};
        mp_chmap_sel_add_alsa_def(&sel);
        if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
            return -1;
        int reqchannels = ao->channels.num;
        // We only use SNDCTL_DSP_CHANNELS for >2 channels, in case some drivers don't have it
        if (reqchannels > 2) {
            int nchannels = reqchannels;
            if (ioctl(p->audio_fd, SNDCTL_DSP_CHANNELS, &nchannels) == -1 ||
                nchannels != reqchannels)
            {
                MP_ERR(ao, "Failed to set audio device to %d channels.\n",
                       reqchannels);
                return -1;
            }
        } else {
            int c = reqchannels - 1;
            if (ioctl(p->audio_fd, SNDCTL_DSP_STEREO, &c) == -1) {
                MP_ERR(ao, "Failed to set audio device to %d channels.\n",
                       reqchannels);
                return -1;
            }
            if (!ao_chmap_sel_get_def(ao, &sel, &ao->channels, c + 1))
                return -1;
        }
        MP_VERBOSE(ao, "using %d channels (requested: %d)\n",
                   ao->channels.num, reqchannels);
        // set rate
        ioctl(p->audio_fd, SNDCTL_DSP_SPEED, &ao->samplerate);
        MP_VERBOSE(ao, "using %d Hz samplerate\n", ao->samplerate);
    }

    if (ioctl(p->audio_fd, SNDCTL_DSP_GETOSPACE, &p->zz) == -1) {
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
                   p->zz.fragments, p->zz.fragstotal, p->zz.fragsize, p->zz.bytes);
        p->buffersize = p->zz.bytes;
        p->outburst = p->zz.fragsize;
    }

    if (p->buffersize == -1) {
        // Measuring buffer size:
        void *data;
        p->buffersize = 0;
#ifdef HAVE_AUDIO_SELECT
        data = malloc(p->outburst);
        memset(data, 0, p->outburst);
        while (p->buffersize < 0x40000) {
            fd_set rfds;
            struct timeval tv;
            FD_ZERO(&rfds);
            FD_SET(p->audio_fd, &rfds);
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            if (!select(p->audio_fd + 1, NULL, &rfds, NULL, &tv))
                break;
            write(p->audio_fd, data, p->outburst);
            p->buffersize += p->outburst;
        }
        free(data);
        if (p->buffersize == 0) {
            MP_ERR(ao, "***  Your audio driver DOES NOT support select()  ***\n");
            MP_ERR(ao, "Recompile mpv with #undef HAVE_AUDIO_SELECT in config.h!\n");
            return -1;
        }
#endif
    }

    ao->bps = ao->channels.num * (af_fmt2bits(ao->format) / 8);
    p->outburst -= p->outburst % ao->bps; // round down
    ao->bps *= ao->samplerate;

    return 0;
}

// close audio device
static void uninit(struct ao *ao, bool immed)
{
    struct priv *p = ao->priv;
    if (p->audio_fd == -1)
        return;
#ifdef SNDCTL_DSP_SYNC
    // to get the buffer played
    if (!immed)
        ioctl(p->audio_fd, SNDCTL_DSP_SYNC, NULL);
#endif
#ifdef SNDCTL_DSP_RESET
    if (immed)
        ioctl(p->audio_fd, SNDCTL_DSP_RESET, NULL);
#endif
    close(p->audio_fd);
    p->audio_fd = -1;
}

static void close_device(struct ao *ao)
{
    struct priv *p = ao->priv;
#ifdef SNDCTL_DSP_RESET
    ioctl(p->audio_fd, SNDCTL_DSP_RESET, NULL);
#endif
    close(p->audio_fd);
    p->audio_fd = -1;
}

// stop playing and empty buffers (for seeking/pause)
static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    int oss_format;
    close_device(ao);
    p->audio_fd = open(p->dsp, O_WRONLY);
    if (p->audio_fd < 0) {
        MP_ERR(ao, "Fatal error: *** CANNOT "
               "RE-OPEN / RESET AUDIO DEVICE *** %s\n", strerror(errno));
        return;
    }

#if defined(FD_CLOEXEC) && defined(F_SETFD)
    fcntl(p->audio_fd, F_SETFD, FD_CLOEXEC);
#endif

    oss_format = format2oss(ao->format);
    if (AF_FORMAT_IS_AC3(ao->format))
        ioctl(p->audio_fd, SNDCTL_DSP_SPEED, &ao->samplerate);
    ioctl(p->audio_fd, SNDCTL_DSP_SETFMT, &oss_format);
    if (!AF_FORMAT_IS_AC3(ao->format)) {
        if (ao->channels.num > 2)
            ioctl(p->audio_fd, SNDCTL_DSP_CHANNELS, &ao->channels.num);
        else {
            int c = ao->channels.num - 1;
            ioctl(p->audio_fd, SNDCTL_DSP_STEREO, &c);
        }
        ioctl(p->audio_fd, SNDCTL_DSP_SPEED, &ao->samplerate);
    }
}

// return: how many bytes can be played without blocking
static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;
    int playsize = p->outburst;

#ifdef SNDCTL_DSP_GETOSPACE
    if (ioctl(p->audio_fd, SNDCTL_DSP_GETOSPACE, &p->zz) != -1) {
        // calculate exact buffer space:
        playsize = p->zz.fragments * p->zz.fragsize;
        return playsize;
    }
#endif

    // check buffer
#ifdef HAVE_AUDIO_SELECT
    {
        fd_set rfds;
        struct timeval tv;
        FD_ZERO(&rfds);
        FD_SET(p->audio_fd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        if (!select(p->audio_fd + 1, NULL, &rfds, NULL, &tv))
            return 0;                                            // not block!
    }
#endif

    return p->outburst;
}

// stop playing, keep buffers (for pause)
static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;
    p->prepause_space = get_space(ao);
#ifdef SNDCTL_DSP_RESET
    ioctl(p->audio_fd, SNDCTL_DSP_RESET, NULL);
#else
    close_device(ao);
#endif
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(struct ao *ao, void *data, int len, int flags)
{
    struct priv *p = ao->priv;
    if (len == 0)
        return len;
    if (len > p->outburst || !(flags & AOPLAY_FINAL_CHUNK)) {
        len /= p->outburst;
        len *= p->outburst;
    }
    len = write(p->audio_fd, data, len);
    return len;
}

// resume playing, after audio_pause()
static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    int fillcnt;
#ifndef SNDCTL_DSP_RESET
    reset(ao);
#endif
    fillcnt = get_space(ao) - p->prepause_space;
    if (fillcnt > 0 && !(ao->format & AF_FORMAT_SPECIAL_MASK)) {
        void *silence = calloc(fillcnt, 1);
        play(ao, silence, fillcnt, 0);
        free(silence);
    }
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(struct ao *ao)
{
    struct priv *p = ao->priv;
    /* Calculate how many bytes/second is sent out */
    if (p->audio_delay_method == 2) {
#ifdef SNDCTL_DSP_GETODELAY
        int r = 0;
        if (ioctl(p->audio_fd, SNDCTL_DSP_GETODELAY, &r) != -1)
            return ((float)r) / (float)ao->bps;
#endif
        p->audio_delay_method = 1; // fallback if not supported
    }
    if (p->audio_delay_method == 1) {
        // SNDCTL_DSP_GETOSPACE
        if (ioctl(p->audio_fd, SNDCTL_DSP_GETOSPACE, &p->zz) != -1) {
            return ((float)(p->buffersize -
                            p->zz.bytes)) / (float)ao->bps;
        }
        p->audio_delay_method = 0; // fallback if not supported
    }
    return ((float)p->buffersize) / (float)ao->bps;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_oss = {
    .info = &(const struct ao_info) {
        "OSS/ioctl audio output",
        "oss",
        "A'rpi",
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
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .audio_fd = -1,
        .audio_delay_method = 2,
        .buffersize = -1,
        .outburst = 512,
        .oss_mixer_channel = SOUND_MIXER_PCM,

        .dsp = PATH_DEV_DSP,
        .oss_mixer_device = PATH_DEV_MIXER,
    },
    .options = (const struct m_option[]) {
        OPT_STRING("device", dsp, 0),
        OPT_STRING("mixer-device", oss_mixer_device, 0),
        OPT_STRING("mixer-channel", cfg_oss_mixer_channel, 0),
        {0}
    },
};
