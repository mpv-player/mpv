/*
 * Copyright (c) 2020 Nia Alarie <nia@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/audioio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "options/m_option.h"
#include "common/msg.h"

#include "audio/format.h"
#include "ao.h"
#include "internal.h"

#ifndef NETBSD_MAX_CHANNELS
#define NETBSD_MAX_CHANNELS (12)
#endif

#ifndef NETBSD_MAX_DEVS
#define NETBSD_MAX_DEVS (8)
#endif

struct priv {
    int fd;
};

static int init(struct ao *ao)
{
    char device[16] = "/dev/audio";
    struct priv *p = ao->priv;
    struct audio_info info;
    struct audio_info hw_info;
    struct audio_prinfo *pinfo;
    struct mp_chmap_sel sel = {0};

    AUDIO_INITINFO(&info);
    AUDIO_INITINFO(&hw_info);
    pinfo = &info.play;

    if (ao->device != NULL) {
         (void)snprintf(device, sizeof(device), "/dev/audio%s",
                        ao->device + sizeof("netbsd/") - 1);
    }

    MP_ERR(ao, "Opening device %s\n", device);
    if ((p->fd = open(device, O_WRONLY | O_NONBLOCK)) == -1) {
        MP_ERR(ao, "Can't open audio device %s: %s\n",
               device, mp_strerror(errno));
        goto fail;
    }

    if (ioctl(p->fd, AUDIO_GETFORMAT, &hw_info) == -1) {
        MP_ERR(ao, "AUDIO_GETFORMAT failed: %s\n", mp_strerror(errno));
        goto fail;
    }

    info.mode = AUMODE_PLAY;

    for (int n = 1; n <= NETBSD_MAX_CHANNELS; n++) {
        struct mp_chmap map;

        mp_chmap_from_channels(&map, n);
        mp_chmap_sel_add_map(&sel, &map);
    }

    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels)) {
        MP_ERR(ao, "Failed to select a valid channel map\n");
        goto fail;
    }

    pinfo->channels = ao->channels.num;
    pinfo->sample_rate = ao->samplerate = hw_info.play.sample_rate;

    switch (ao->format) {
    case AF_FORMAT_U8:
        pinfo->precision = 8;
        pinfo->encoding = AUDIO_ENCODING_ULINEAR;
        break;
    case AF_FORMAT_S16:
        pinfo->precision = 16;
        pinfo->encoding = AUDIO_ENCODING_SLINEAR;
        break;
    default:
        pinfo->precision = 32;
        pinfo->encoding = AUDIO_ENCODING_SLINEAR;
        ao->format = AF_FORMAT_S32;
        break;
    }

    if (ioctl(p->fd, AUDIO_SETINFO, &info) == -1) {
        MP_ERR(ao, "AUDIO_SETINFO failed: %s\n", mp_strerror(errno));
        goto fail;
    }

    if (ioctl(p->fd, AUDIO_GETINFO, &info) == -1) {
        MP_ERR(ao, "AUDIO_GETINFO failed: %s\n", mp_strerror(errno));
        goto fail;
    }

    ao->period_size = info.blocksize / (pinfo->precision / 8) / pinfo->channels;
    ao->device_buffer = info.play.buffer_size / (pinfo->precision / 8) / pinfo->channels;

    return 0;

fail:
    if (p->fd != -1) {
        close(p->fd);
    }
    return -1;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    (void)ioctl(p->fd, AUDIO_FLUSH, NULL);
    (void)close(p->fd);
    p->fd = -1;
}

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct audio_info info;

    if (ioctl(p->fd, AUDIO_GETBUFINFO, &info) == -1) {
        MP_ERR(ao, "AUDIO_GETBUFINFO failed: %s\n", mp_strerror(errno));
        return;
    }

    (void)ioctl(p->fd, AUDIO_FLUSH, NULL);
}

static void drain(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct audio_info info;

    if (ioctl(p->fd, AUDIO_GETBUFINFO, &info) == -1) {
        MP_ERR(ao, "AUDIO_GETBUFINFO failed: %s\n", mp_strerror(errno));
        return;
    }

    (void)ioctl(p->fd, AUDIO_DRAIN, NULL);
}

static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct audio_info info;
    unsigned int nblk;

    if (ioctl(p->fd, AUDIO_GETBUFINFO, &info) == -1) {
        MP_ERR(ao, "AUDIO_GETBUFINFO failed: %s\n", mp_strerror(errno));
        return 0;
    }
    nblk = info.hiwat - (info.play.seek / info.blocksize);
    return nblk * ao->period_size;
}

static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct audio_info info;

    if (ioctl(p->fd, AUDIO_GETINFO, &info) == -1) {
        MP_ERR(ao, "AUDIO_GETINFO failed: %s\n", mp_strerror(errno));
        return;
    }
    info.play.pause = 1;
    (void)ioctl(p->fd, AUDIO_SETINFO, &info);
}

static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct audio_info info;

    if (ioctl(p->fd, AUDIO_GETINFO, &info) == -1) {
        MP_ERR(ao, "AUDIO_GETINFO failed: %s\n", mp_strerror(errno));
        return;
    }
    info.play.pause = 0;
    (void)ioctl(p->fd, AUDIO_SETINFO, &info);
    reset(ao);
}

static double get_delay(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct audio_info info;

    if (ioctl(p->fd, AUDIO_GETBUFINFO, &info) == -1) {
        MP_ERR(ao, "AUDIO_GETBUFINFO failed: %s\n", mp_strerror(errno));
        return 0;
    }
    return (info.blocksize + info.play.seek) / (double)ao->bps;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *p = ao->priv;
    int len = samples * ao->sstride;
    int ret;

    if (len == 0)
        return 0;

    errno = 0;
    if ((ret = write(p->fd, data[0], len)) == -1) {
        MP_ERR(ao, "audio write failed: %s\n", mp_strerror(errno));
        return 0;
    }
    return ret / ao->sstride;
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    char name[16];
    struct audio_device dev;
    int fd;

    for (int i = 0; 0 < NETBSD_MAX_DEVS; ++i) {
        (void)snprintf(name, sizeof(name), "/dev/audio%d", i);
        fd = open(name, O_WRONLY);
        if (fd == -1 || ioctl(fd, AUDIO_GETDEV, &dev) == -1) {
            break;
        }
        (void)snprintf(name, sizeof(name), "%d", i);
        ao_device_list_add(list, ao, &(struct ao_device_desc){name, dev.name});
        close(fd);
    }
}

const struct ao_driver audio_out_netbsd = {
    .description = "NetBSD audio output",
    .name      = "netbsd",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .drain     = drain,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .get_space = get_space,
    .get_delay = get_delay,
    .play      = play,
    .list_devs = list_devs,
    .priv_size = sizeof(struct priv),
};
