/*
 * OSS audio output driver
 *
 * Original author: A'rpi
 * Support for >2 output channels added 2001-11-25
 * - Steve Davies <steve@daviesfam.org>
 * Rozhuk Ivan <rozhuk.im@gmail.com> 2020
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/stat.h>
#if defined(__DragonFly__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif
#include <sys/types.h>

#include "config.h"
#include "audio/format.h"
#include "common/msg.h"
#include "options/options.h"
#include "osdep/endian.h"
#include "osdep/io.h"
#include "ao.h"
#include "internal.h"

#ifndef AFMT_AC3
#define AFMT_AC3 -1
#endif

#define PATH_DEV_DSP "/dev/dsp"
#define PATH_DEV_MIXER "/dev/mixer"

struct priv {
    int dsp_fd;
    bool playing;
    double bps; /* Bytes per second. */
};

/* like alsa except for 6.1 and 7.1, from pcm/matrix_map.h */
static const struct mp_chmap oss_layouts[MP_NUM_CHANNELS + 1] = {
    {0},                                        /* empty */
    MP_CHMAP_INIT_MONO,                         /* mono */
    MP_CHMAP2(FL, FR),                          /* stereo */
    MP_CHMAP3(FL, FR, LFE),                     /* 2.1 */
    MP_CHMAP4(FL, FR, BL, BR),                  /* 4.0 */
    MP_CHMAP5(FL, FR, BL, BR, FC),              /* 5.0 */
    MP_CHMAP6(FL, FR, BL, BR, FC, LFE),         /* 5.1 */
    MP_CHMAP7(FL, FR, BL, BR, FC, LFE, BC),     /* 6.1 */
    MP_CHMAP8(FL, FR, BL, BR, FC, LFE, SL, SR), /* 7.1 */
};

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

#define MP_WARN_IOCTL_ERR(__ao) \
    MP_WARN((__ao), "%s: ioctl() fail, err = %i: %s\n", \
        __FUNCTION__, errno, strerror(errno))


static void uninit(struct ao *ao);


static void device_descr_get(size_t dev_idx, char *buf, size_t buf_size)
{
#if defined(__DragonFly__) || defined(__FreeBSD__)
    char dev_path[32];
    size_t tmp = (buf_size - 1);

    snprintf(dev_path, sizeof(dev_path), "dev.pcm.%zu.%%desc", dev_idx);
    if (sysctlbyname(dev_path, buf, &tmp, NULL, 0) != 0) {
        tmp = 0;
    }
    buf[tmp] = 0x00;
#elif defined(SOUND_MIXER_INFO)
    size_t tmp = 0;
    char dev_path[32];
    mixer_info mi;

    snprintf(dev_path, sizeof(dev_path), PATH_DEV_MIXER"%zu", dev_idx);
    int fd = open(dev_path, O_RDONLY);
    if (ioctl(fd, SOUND_MIXER_INFO, &mi) == 0) {
        strncpy(buf, mi.name, buf_size);
        tmp = (buf_size - 1);
    }
    close(fd);
    buf[tmp] = 0x00;
#else
    buf[0] = 0x00;
#endif
}

static int format2oss(int format)
{
    for (size_t i = 0; format_table[i][0] != -1; i++) {
        if (format_table[i][1] == format)
            return format_table[i][0];
    }
    return -1;
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

    return (ioctl(p->dsp_fd, SNDCTL_DSP_SETFMT, &oss_format) != -1);
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct mp_chmap channels = ao->channels;
    audio_buf_info info;
    size_t i;
    int format, samplerate, nchannels, reqchannels, trig = 0;
    int best_sample_formats[AF_FORMAT_COUNT + 1];
    const char *device = ((ao->device) ? ao->device : PATH_DEV_DSP);

    /* Opening device. */
    MP_VERBOSE(ao, "Using '%s' audio device.\n", device);
    p->dsp_fd = open(device, (O_WRONLY | O_CLOEXEC));
    if (p->dsp_fd < 0) {
        MP_ERR(ao, "Can't open audio device %s: %s.\n",
            device, mp_strerror(errno));
        goto err_out;
    }

    /* Selecting sound format. */
    format = af_fmt_from_planar(ao->format);
    af_get_best_sample_formats(format, best_sample_formats);
    for (i = 0; best_sample_formats[i]; i++) {
        format = best_sample_formats[i];
        if (try_format(ao, &format))
            break;
    }
    if (!format) {
        MP_ERR(ao, "Can't set sample format.\n");
        goto err_out;
    }
    MP_VERBOSE(ao, "Sample format: %s\n", af_fmt_to_str(format));

    /* Channels count. */
    if (af_fmt_is_spdif(format)) {
        /* Probably could be fixed by setting number of channels;
         * needs testing. */
        if (channels.num != 2) {
            MP_ERR(ao, "Format %s not implemented.\n", af_fmt_to_str(format));
            goto err_out;
        }
    } else {
        struct mp_chmap_sel sel = {0};
        for (i = 0; i < MP_ARRAY_SIZE(oss_layouts); i++) {
            mp_chmap_sel_add_map(&sel, &oss_layouts[i]);
        }
        if (!ao_chmap_sel_adjust(ao, &sel, &channels))
            goto err_out;
        nchannels = reqchannels = channels.num;
        if (ioctl(p->dsp_fd, SNDCTL_DSP_CHANNELS, &nchannels) == -1) {
            MP_ERR(ao, "Failed to set audio device to %d channels.\n",
                reqchannels);
            goto err_out_ioctl;
        }
        if (nchannels != reqchannels) {
            /* Update number of channels to OSS suggested value. */
            if (!ao_chmap_sel_get_def(ao, &sel, &channels, nchannels))
                goto err_out;
        }
        MP_VERBOSE(ao, "Using %d channels (requested: %d).\n",
            channels.num, reqchannels);
    }

    /* Sample rate. */
    samplerate = ao->samplerate;
    if (ioctl(p->dsp_fd, SNDCTL_DSP_SPEED, &samplerate) == -1)
        goto err_out_ioctl;
    MP_VERBOSE(ao, "Using %d Hz samplerate.\n", samplerate);

    /* Get buffer size. */
    if (ioctl(p->dsp_fd, SNDCTL_DSP_GETOSPACE, &info) == -1)
        goto err_out_ioctl;
    /* See ao.c ao->sstride initializations and get_state(). */
    ao->device_buffer = ((info.fragstotal * info.fragsize) /
        af_fmt_to_bytes(format));
    if (!af_fmt_is_planar(format)) {
        ao->device_buffer /= channels.num;
    }

    /* Do not start playback after data written. */
    if (ioctl(p->dsp_fd, SNDCTL_DSP_SETTRIGGER, &trig) == -1)
        goto err_out_ioctl;

    /* Update sound params. */
    ao->format = format;
    ao->samplerate = samplerate;
    ao->channels = channels;
    p->bps = (channels.num * samplerate * af_fmt_to_bytes(format));
    p->playing = false;

    return 0;

err_out_ioctl:
    MP_WARN_IOCTL_ERR(ao);
err_out:
    uninit(ao);
    return -1;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->dsp_fd == -1)
        return;
    ioctl(p->dsp_fd, SNDCTL_DSP_HALT, NULL);
    close(p->dsp_fd);
    p->dsp_fd = -1;
    p->playing = false;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
    int v;

    if (p->dsp_fd < 0)
        return CONTROL_ERROR;

    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        if (ioctl(p->dsp_fd, SNDCTL_DSP_GETPLAYVOL, &v) == -1) {
            MP_WARN_IOCTL_ERR(ao);
            return CONTROL_ERROR;
        }
        vol->right = ((v & 0xff00) >> 8);
        vol->left = (v & 0x00ff);
        return CONTROL_OK;
    case AOCONTROL_SET_VOLUME:
        v = ((int)vol->right << 8) | (int)vol->left;
        if (ioctl(p->dsp_fd, SNDCTL_DSP_SETPLAYVOL, &v) == -1) {
            MP_WARN_IOCTL_ERR(ao);
            return CONTROL_ERROR;
        }
        return CONTROL_OK;
    }

    return CONTROL_UNKNOWN;
}

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    int trig = 0;

    /* Clear buf and do not start playback after data written. */
    p->playing = false;
    if (ioctl(p->dsp_fd, SNDCTL_DSP_HALT, NULL) == -1 ||
        ioctl(p->dsp_fd, SNDCTL_DSP_SETTRIGGER, &trig) == -1)
    {
        MP_WARN_IOCTL_ERR(ao);
        MP_WARN(ao, "Force reinitialize audio device.\n");
        uninit(ao);
        init(ao);
    }
}

static void start(struct ao *ao)
{
    struct priv *p = ao->priv;
    int trig = PCM_ENABLE_OUTPUT;

    if (ioctl(p->dsp_fd, SNDCTL_DSP_SETTRIGGER, &trig) == -1) {
        MP_WARN_IOCTL_ERR(ao);
        return;
    }
    p->playing = true;
}

static bool audio_write(struct ao *ao, void **data, int samples)
{
    struct priv *p = ao->priv;
    ssize_t rc;
    const size_t size = (samples * ao->sstride);

    if (size == 0)
        return true;

    while ((rc = write(p->dsp_fd, data[0], size)) == -1) {
        if (errno == EINTR)
			continue;
        MP_WARN(ao, "audio_write: write() fail, err = %i: %s.\n",
            errno, strerror(errno));
        p->playing = false;
        return false;
    }
    if ((size_t)rc != size) {
        MP_WARN(ao, "audio_write: unexpected partial write: required: %zu, written: %zu.\n",
            size, (size_t)rc);
        p->playing = false;
        return false;
    }

    return true;
}

static void get_state(struct ao *ao, struct mp_pcm_state *state)
{
    struct priv *p = ao->priv;
    audio_buf_info info;
    int odelay;

    if (ioctl(p->dsp_fd, SNDCTL_DSP_GETOSPACE, &info) == -1 ||
        ioctl(p->dsp_fd, SNDCTL_DSP_GETODELAY, &odelay) == -1)
    {
        MP_WARN_IOCTL_ERR(ao);
        p->playing = false;
        memset(state, 0x00, sizeof(struct mp_pcm_state));
        state->delay = 0.0;
        return;
    }
    state->free_samples = (info.bytes / ao->sstride);
    state->queued_samples = (ao->device_buffer - state->free_samples);
    state->delay = (odelay / p->bps);
    state->playing = p->playing;
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    struct stat st;
    char dev_path[32] = PATH_DEV_DSP, dev_descr[256] = "Default";
    struct ao_device_desc dev = {.name = dev_path, .desc = dev_descr};

    if (stat(PATH_DEV_DSP, &st) == 0) {
        ao_device_list_add(list, ao, &dev);
    }

    /* Auto detect. */
    for (size_t i = 0, fail_cnt = 0; fail_cnt < 8; i ++, fail_cnt ++) {
        snprintf(dev_path, sizeof(dev_path), PATH_DEV_DSP"%zu", i);
        if (stat(dev_path, &st) != 0)
            continue;
        device_descr_get(i, dev_descr, sizeof(dev_descr));
        ao_device_list_add(list, ao, &dev);
        fail_cnt = 0; /* Reset fail counter. */
    }
}

const struct ao_driver audio_out_oss = {
    .name      = "oss",
    .description = "OSS/ioctl audio output",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .reset     = reset,
    .start     = start,
    .write     = audio_write,
    .get_state = get_state,
    .list_devs = list_devs,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .dsp_fd = -1,
        .playing = false,
    },
};
