/*
 * OSS audio output driver
 *
 * Original author: A'rpi
 * Support for >2 output channels added 2001-11-25
 * - Steve Davies <steve@daviesfam.org>
 * Rozhuk Ivan <rozhuk.im@gmail.com> 2020-2026
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
#include <stdlib.h>

#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/stat.h>
#if defined(__DragonFly__) || defined(__FreeBSD__)
#   include <sys/sysctl.h>
#   include <sys/socket.h>
#   include <sys/un.h>
#   include <string.h>
#   include <paths.h>
#   define DEVD_SOCK_PATH       _PATH_VARRUN "devd.seqpacket.pipe"
#   define DEVD_EVENT_NOTIFY    '!'
#endif
#include <sys/types.h>
#include <dirent.h>

#include "audio/format.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/options.h"
#include "osdep/endian.h"
#include "osdep/io.h"
#include "osdep/threads.h"
#include "misc/natural_sort.h"
#include "ao.h"
#include "internal.h"

#ifndef AFMT_AC3
#   define AFMT_AC3         -1
#endif

#define PATH_DEV_DSP        "/dev/dsp"
#define PATH_DEV_MIXER      "/dev/mixer"



struct priv {
    int dsp_fd;
    int hotplug_fd;
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
        __FUNCTION__, errno, mp_strerror(errno))


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
        strncpy(buf, mi.name, buf_size - 1);
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
        nchannels = reqchannels = channels.num;
        if (ioctl(p->dsp_fd, SNDCTL_DSP_CHANNELS, &nchannels) == -1) {
            MP_ERR(ao, "Failed to set audio device to %d channels.\n",
                reqchannels);
            goto err_out_ioctl;
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
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    float *vol = arg;
    int v;

    if (p->dsp_fd < 0)
        return CONTROL_ERROR;

    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        if (ioctl(p->dsp_fd, SNDCTL_DSP_GETPLAYVOL, &v) == -1) {
            MP_WARN_IOCTL_ERR(ao);
            return CONTROL_ERROR;
        }
        *vol = ((v & 0x00ff) + ((v & 0xff00) >> 8)) / 2.0;
        return CONTROL_OK;
    case AOCONTROL_SET_VOLUME:
        v = ((int)*vol << 8) | (int)*vol;
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
            errno, mp_strerror(errno));
        return false;
    }
    if ((size_t)rc != size) {
        MP_WARN(ao, "audio_write: unexpected partial write: required: %zu, written: %zu.\n",
            size, (size_t)rc);
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
        memset(state, 0x00, sizeof(struct mp_pcm_state));
        state->delay = 0.0;
        return;
    }
    state->free_samples = (info.bytes / ao->sstride);
    state->queued_samples = (ao->device_buffer - state->free_samples);
    state->delay = (odelay / p->bps);
    state->playing = (state->queued_samples != 0);
}

static int mp_natural_sort_cmp_de(const struct dirent **a, const struct dirent **b)
{
    return mp_natural_sort_cmp((*a)->d_name, (*b)->d_name);
}

static int
scandir_filter_cb(const struct dirent *de) {

    if (de == NULL || de->d_fileno == 0)
            return 0;
    /* Only few types allowed. */
    switch (de->d_type) {
    case DT_CHR:
    case DT_LNK:
        break;
    default: /* Filter out all other. */
        return 0;
    }
    /* Allow only "dsp*". */
    return (de->d_namlen > 3 && memcmp("dsp", de->d_name, 3) == 0);
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    int rc, dev_idx;
    struct dirent **dirp = NULL;
    struct stat st;
    char dev_path[32] = PATH_DEV_DSP, dev_descr[256] = "Default";
    struct ao_device_desc dev = {.name = dev_path, .desc = dev_descr};

    if (stat(PATH_DEV_DSP, &st) == 0) {
        ao_device_list_add(list, ao, &dev);
    }

    /* Auto detect. */
    rc = scandir("/dev", &dirp, scandir_filter_cb, mp_natural_sort_cmp_de);
    if (rc == -1 || dirp == NULL)
        return;
    for (size_t i = 0; i < (size_t)rc; i ++) {
        dev_idx = atoi((dirp[i]->d_name + 3));
        snprintf(dev_path, sizeof(dev_path), PATH_DEV_DSP"%i", dev_idx);
        device_descr_get((size_t)dev_idx, dev_descr, sizeof(dev_descr));
        ao_device_list_add(list, ao, &dev);
        free(dirp[i]);
    }
    free(dirp);
}

#ifdef DEVD_SOCK_PATH
static void *hotplug_proc(void *data) {
    struct ao *ao = data;
    struct priv *p = ao->priv;
    ssize_t ios;
    uint8_t buf[4096];

    mp_thread_set_name("ao/oss hotplug");
    mp_thread_detach(mp_thread_current_id());

    for (;;) {
        ios = recv(p->hotplug_fd, buf, sizeof(buf), MSG_WAITALL);
        if (ios <= 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        /* Check: is notify and system=DEVFS and subsystem=CDEV and cdev=dsp. */
        if (DEVD_EVENT_NOTIFY == buf[0] &&
            NULL != memmem(buf, (size_t)ios, "system=DEVFS", 12) &&
            NULL != memmem(buf, (size_t)ios, "subsystem=CDEV", 14) &&
            NULL != memmem(buf, (size_t)ios, "cdev=dsp", 8))
        {
            ao_hotplug_event(ao); /* Fire hotplug event! */
        }
    }

    return NULL;
}

static int hotplug_init(struct ao *ao)
{
    struct priv *p = ao->priv;
    static const struct sockaddr_un sun = {
        .sun_len = sizeof(struct sockaddr_un),
        .sun_family = AF_UNIX,
        .sun_path = DEVD_SOCK_PATH
    };
    mp_thread tid;

    p->hotplug_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (p->hotplug_fd == -1)
        return -1;
    /* Attempt to connect to devd socket. */
    if (connect(p->hotplug_fd, (struct sockaddr*)&sun, sizeof(sun)) < 0 ||
        mp_thread_create(&tid, hotplug_proc, (void*)ao))
    {
        close(p->hotplug_fd);
        p->hotplug_fd = -1;
        MP_WARN(ao, "%s: fail to connect to devd socket or thread create: err = %i: %s\n",
            __FUNCTION__, errno, mp_strerror(errno));
        return -1;
    }

    return 0;
}

static void hotplug_uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->hotplug_fd == -1)
        return;
    close(p->hotplug_fd);
    p->hotplug_fd = -1;
}
#endif

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
#ifdef DEVD_SOCK_PATH
    .hotplug_init = hotplug_init,
    .hotplug_uninit = hotplug_uninit,
#endif
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .dsp_fd = -1,
        .hotplug_fd = -1,
    },
};
