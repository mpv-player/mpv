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
#include "core/options.h"
#include "core/mp_msg.h"
#include "audio/mixer.h"

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
    char *dsp;
    int audio_fd;
    int prepause_space;
    const char *oss_mixer_device;
    int oss_mixer_channel;
    audio_buf_info zz;
    int audio_delay_method;
};

static int format2oss(int format)
{
    switch (format) {
    case AF_FORMAT_U8: return AFMT_U8;
    case AF_FORMAT_S8: return AFMT_S8;
    case AF_FORMAT_U16_LE: return AFMT_U16_LE;
    case AF_FORMAT_U16_BE: return AFMT_U16_BE;
    case AF_FORMAT_S16_LE: return AFMT_S16_LE;
    case AF_FORMAT_S16_BE: return AFMT_S16_BE;
#ifdef AFMT_S24_PACKED
    case AF_FORMAT_S24_LE: return AFMT_S24_PACKED;
#endif
#ifdef AFMT_U32_LE
    case AF_FORMAT_U32_LE: return AFMT_U32_LE;
#endif
#ifdef AFMT_U32_BE
    case AF_FORMAT_U32_BE: return AFMT_U32_BE;
#endif
#ifdef AFMT_S32_LE
    case AF_FORMAT_S32_LE: return AFMT_S32_LE;
#endif
#ifdef AFMT_S32_BE
    case AF_FORMAT_S32_BE: return AFMT_S32_BE;
#endif
#ifdef AFMT_FLOAT
    case AF_FORMAT_FLOAT_NE: return AFMT_FLOAT;
#endif
        // SPECIALS
#ifdef AFMT_MPEG
    case AF_FORMAT_MPEG2: return AFMT_MPEG;
#endif
#ifdef AFMT_AC3
    case AF_FORMAT_AC3_NE: return AFMT_AC3;
#endif
    }
    mp_msg(MSGT_AO, MSGL_V, "OSS: Unknown/not supported internal format: %s\n",
           af_fmt2str_short(format));
    return -1;
}

static int oss2format(int format)
{
    switch (format) {
    case AFMT_U8: return AF_FORMAT_U8;
    case AFMT_S8: return AF_FORMAT_S8;
    case AFMT_U16_LE: return AF_FORMAT_U16_LE;
    case AFMT_U16_BE: return AF_FORMAT_U16_BE;
    case AFMT_S16_LE: return AF_FORMAT_S16_LE;
    case AFMT_S16_BE: return AF_FORMAT_S16_BE;
#ifdef AFMT_S24_PACKED
    case AFMT_S24_PACKED: return AF_FORMAT_S24_LE;
#endif
#ifdef AFMT_U32_LE
    case AFMT_U32_LE: return AF_FORMAT_U32_LE;
#endif
#ifdef AFMT_U32_BE
    case AFMT_U32_BE: return AF_FORMAT_U32_BE;
#endif
#ifdef AFMT_S32_LE
    case AFMT_S32_LE: return AF_FORMAT_S32_LE;
#endif
#ifdef AFMT_S32_BE
    case AFMT_S32_BE: return AF_FORMAT_S32_BE;
#endif
#ifdef AFMT_FLOAT
    case AFMT_FLOAT: return AF_FORMAT_FLOAT_NE;
#endif
        // SPECIALS
#ifdef AFMT_MPEG
    case AFMT_MPEG: return AF_FORMAT_MPEG2;
#endif
#ifdef AFMT_AC3
    case AFMT_AC3: return AF_FORMAT_AC3_NE;
#endif
    }
    mp_tmsg(MSGT_GLOBAL, MSGL_ERR, "[AO OSS] Unknown/Unsupported OSS format: %x.\n",
            format);
    return -1;
}


#ifdef SNDCTL_DSP_GETPLAYVOL
static int volume_oss4(ao_control_vol_t *vol, int cmd)
{
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
        if (volume_oss4(vol, cmd) == CONTROL_OK)
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
static int init(struct ao *ao, char *params)
{
    struct MPOpts *opts = ao->opts;
    char *mixer_channels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;
    int oss_format;
    char *mdev = opts->mixer_device, *mchan = opts->mixer_channel;

    mp_msg(MSGT_AO, MSGL_V, "ao2: %d Hz  %d chans  %s\n", ao->samplerate,
           ao->channels.num, af_fmt2str_short(ao->format));

    struct priv *p = talloc(ao, struct priv);
    *p = (struct priv) {
        .dsp = PATH_DEV_DSP,
        .audio_fd = -1,
        .oss_mixer_device = mdev ? mdev : PATH_DEV_MIXER,
        .oss_mixer_channel = SOUND_MIXER_PCM,
        .audio_delay_method = 2,
    };
    ao->priv = p;

    if (params) {
        char *m, *c;
        m = strchr(params, ':');
        if (m) {
            c = strchr(m + 1, ':');
            if (c) {
                mchan = c + 1;
                c[0] = '\0';
            }
            mdev = m + 1;
            m[0] = '\0';
        }
        p->dsp = talloc_strdup(ao, params);
    }

    if (mchan) {
        int fd, devs, i;

        if ((fd = open(p->oss_mixer_device, O_RDONLY)) == -1) {
            mp_tmsg(MSGT_AO, MSGL_ERR,
                    "[AO OSS] audio_setup: Can't open mixer device %s: %s\n",
                    p->oss_mixer_device, strerror(errno));
        } else {
            ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
            close(fd);

            for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
                if (!strcasecmp(mixer_channels[i], mchan)) {
                    if (!(devs & (1 << i))) {
                        mp_tmsg(MSGT_AO, MSGL_ERR, "[AO OSS] audio_setup: "
                                "Audio card mixer does not have channel '%s', "
                                "using default.\n", mchan);
                        i = SOUND_MIXER_NRDEVICES + 1;
                        break;
                    }
                    p->oss_mixer_channel = i;
                    break;
                }
            }
            if (i == SOUND_MIXER_NRDEVICES) {
                mp_tmsg(MSGT_AO, MSGL_ERR, "[AO OSS] audio_setup: Audio card "
                        "mixer does not have channel '%s', using default.\n",
                        mchan);
            }
        }
    } else {
        p->oss_mixer_channel = SOUND_MIXER_PCM;
    }

    mp_msg(MSGT_AO, MSGL_V, "audio_setup: using '%s' dsp device\n", p->dsp);
    mp_msg(MSGT_AO, MSGL_V, "audio_setup: using '%s' mixer device\n",
           p->oss_mixer_device);
    mp_msg(MSGT_AO, MSGL_V, "audio_setup: using '%s' mixer device\n",
           mixer_channels[p->oss_mixer_channel]);

#ifdef __linux__
    p->audio_fd = open(p->dsp, O_WRONLY | O_NONBLOCK);
#else
    p->audio_fd = open(p->dsp, O_WRONLY);
#endif
    if (p->audio_fd < 0) {
        mp_tmsg(MSGT_AO, MSGL_ERR,
                "[AO OSS] audio_setup: Can't open audio device %s: %s\n",
                p->dsp, strerror(errno));
        return -1;
    }

#ifdef __linux__
    /* Remove the non-blocking flag */
    if (fcntl(p->audio_fd, F_SETFL, 0) < 0) {
        mp_tmsg(MSGT_AO, MSGL_ERR, "[AO OSS] audio_setup: Can't make file "
                "descriptor blocking: %s\n", strerror(errno));
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
        mp_tmsg(MSGT_AO, MSGL_WARN, "[AO OSS] Can't set audio device %s to %s "
                "output, trying %s...\n", p->dsp, af_fmt2str_short(ao->format),
                af_fmt2str_short(AF_FORMAT_S16_NE));
        ao->format = AF_FORMAT_S16_NE;
        goto ac3_retry;
    }

    ao->format = oss2format(oss_format);
    if (ao->format == -1)
        return -1;

    mp_msg(MSGT_AO, MSGL_V, "audio_setup: sample format: %s\n",
           af_fmt2str_short(ao->format));

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
                mp_tmsg(MSGT_AO, MSGL_ERR, "[AO OSS] audio_setup: Failed to "
                        "set audio device to %d channels.\n", reqchannels);
                return -1;
            }
        } else {
            int c = reqchannels - 1;
            if (ioctl(p->audio_fd, SNDCTL_DSP_STEREO, &c) == -1) {
                mp_tmsg(MSGT_AO, MSGL_ERR, "[AO OSS] audio_setup: Failed to "
                        "set audio device to %d channels.\n", reqchannels);
                return -1;
            }
            if (!ao_chmap_sel_get_def(ao, &sel, &ao->channels, c + 1))
                return -1;
        }
        mp_msg(MSGT_AO, MSGL_V,
               "audio_setup: using %d channels (requested: %d)\n",
               ao->channels.num, reqchannels);
        // set rate
        ioctl(p->audio_fd, SNDCTL_DSP_SPEED, &ao->samplerate);
        mp_msg(MSGT_AO, MSGL_V, "audio_setup: using %d Hz samplerate\n",
               ao->samplerate);
    }

    if (ioctl(p->audio_fd, SNDCTL_DSP_GETOSPACE, &p->zz) == -1) {
        int r = 0;
        mp_tmsg(MSGT_AO, MSGL_WARN, "[AO OSS] audio_setup: driver doesn't "
                "support SNDCTL_DSP_GETOSPACE\n");
        if (ioctl(p->audio_fd, SNDCTL_DSP_GETBLKSIZE, &r) == -1)
            mp_msg(MSGT_AO, MSGL_V, "audio_setup: %d bytes/frag (config.h)\n",
                   ao->outburst);
        else {
            ao->outburst = r;
            mp_msg(MSGT_AO, MSGL_V, "audio_setup: %d bytes/frag (GETBLKSIZE)\n",
                   ao->outburst);
        }
    } else {
        mp_msg(MSGT_AO, MSGL_V,
               "audio_setup: frags: %3d/%d  (%d bytes/frag)  free: %6d\n",
               p->zz.fragments, p->zz.fragstotal, p->zz.fragsize, p->zz.bytes);
        if (ao->buffersize == -1)
            ao->buffersize = p->zz.bytes;
        ao->outburst = p->zz.fragsize;
    }

    if (ao->buffersize == -1) {
        // Measuring buffer size:
        void *data;
        ao->buffersize = 0;
#ifdef HAVE_AUDIO_SELECT
        data = malloc(ao->outburst);
        memset(data, 0, ao->outburst);
        while (ao->buffersize < 0x40000) {
            fd_set rfds;
            struct timeval tv;
            FD_ZERO(&rfds);
            FD_SET(p->audio_fd, &rfds);
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            if (!select(p->audio_fd + 1, NULL, &rfds, NULL, &tv))
                break;
            write(p->audio_fd, data, ao->outburst);
            ao->buffersize += ao->outburst;
        }
        free(data);
        if (ao->buffersize == 0) {
            mp_tmsg(MSGT_AO, MSGL_ERR, "[AO OSS]\n   ***  Your audio driver "
                    "DOES NOT support select()  ***\n Recompile mpv with "
                    "#undef HAVE_AUDIO_SELECT in config.h !\n\n");
            return -1;
        }
#endif
    }

    ao->bps = ao->channels.num * (af_fmt2bits(ao->format) / 8);
    ao->outburst -= ao->outburst % ao->bps; // round down
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
        mp_tmsg(MSGT_AO, MSGL_ERR, "[AO OSS]\nFatal error: *** CANNOT "
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
    int playsize = ao->outburst;

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

    return ao->outburst;
}

// stop playing, keep buffers (for pause)
static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;
    p->prepause_space = get_space(ao);
    close_device(ao);
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(struct ao *ao, void *data, int len, int flags)
{
    struct priv *p = ao->priv;
    if (len == 0)
        return len;
    if (len > ao->outburst || !(flags & AOPLAY_FINAL_CHUNK)) {
        len /= ao->outburst;
        len *= ao->outburst;
    }
    len = write(p->audio_fd, data, len);
    return len;
}

// resume playing, after audio_pause()
static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    int fillcnt;
    reset(ao);
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
            return ((float)(ao->buffersize -
                            p->zz.bytes)) / (float)ao->bps;
        }
        p->audio_delay_method = 0; // fallback if not supported
    }
    return ((float)ao->buffersize) / (float)ao->bps;
}

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
};
