/*
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

#include "config.h"

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#if HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#if HAVE_SOUNDCARD_H
#include <soundcard.h>
#else
#include <linux/soundcard.h>
#endif
#endif

#include "osdep/io.h"

#include "audio_in.h"
#include "common/common.h"
#include "common/msg.h"

int ai_oss_set_samplerate(audio_in_t *ai)
{
    int tmp = ai->req_samplerate;
    if (ioctl(ai->oss.audio_fd, SNDCTL_DSP_SPEED, &tmp) == -1) return -1;
    ai->samplerate = tmp;
    return 0;
}

int ai_oss_set_channels(audio_in_t *ai)
{
    int err;
    int ioctl_param;

    if (ai->req_channels > 2)
    {
        ioctl_param = ai->req_channels;
        MP_VERBOSE(ai, "ioctl dsp channels: %d\n",
               err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_CHANNELS, &ioctl_param));
        if (err < 0) {
            MP_ERR(ai, "Unable to set channel count: %d\n",
                   ai->req_channels);
            return -1;
        }
        ai->channels = ioctl_param;
    }
    else
    {
        ioctl_param = (ai->req_channels == 2);
        MP_VERBOSE(ai, "ioctl dsp stereo: %d (req: %d)\n",
               err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_STEREO, &ioctl_param),
               ioctl_param);
        if (err < 0) {
            MP_ERR(ai, "Unable to set stereo: %d\n",
                   ai->req_channels == 2);
            return -1;
        }
        ai->channels = ioctl_param ? 2 : 1;
    }
    return 0;
}

int ai_oss_init(audio_in_t *ai)
{
    int err;
    int ioctl_param;

    ai->oss.audio_fd = open(ai->oss.device, O_RDONLY | O_CLOEXEC);
    if (ai->oss.audio_fd < 0)
    {
        MP_ERR(ai, "Unable to open '%s': %s\n",
               ai->oss.device, mp_strerror(errno));
        return -1;
    }

    ioctl_param = 0 ;
    MP_VERBOSE(ai, "ioctl dsp getfmt: %d\n",
           ioctl(ai->oss.audio_fd, SNDCTL_DSP_GETFMTS, &ioctl_param));

    MP_VERBOSE(ai, "Supported formats: %x\n", ioctl_param);
    if (!(ioctl_param & AFMT_S16_NE))
        MP_ERR(ai, "unsupported format\n");

    ioctl_param = AFMT_S16_NE;
    MP_VERBOSE(ai, "ioctl dsp setfmt: %d\n",
           err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_SETFMT, &ioctl_param));
    if (err < 0) {
        MP_ERR(ai, "Unable to set audio format.");
        return -1;
    }

    if (ai_oss_set_channels(ai) < 0) return -1;

    ioctl_param = ai->req_samplerate;
    MP_VERBOSE(ai, "ioctl dsp speed: %d\n",
           err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_SPEED, &ioctl_param));
    if (err < 0) {
        MP_ERR(ai, "Unable to set samplerate: %d\n",
               ai->req_samplerate);
        return -1;
    }
    ai->samplerate = ioctl_param;

    MP_VERBOSE(ai, "ioctl dsp trigger: %d\n",
           ioctl(ai->oss.audio_fd, SNDCTL_DSP_GETTRIGGER, &ioctl_param));
    MP_VERBOSE(ai, "trigger: %x\n", ioctl_param);
    ioctl_param = PCM_ENABLE_INPUT;
    MP_VERBOSE(ai, "ioctl dsp trigger: %d\n",
           err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_SETTRIGGER, &ioctl_param));
    if (err < 0) {
        MP_ERR(ai, "Unable to set trigger: %d\n",
               PCM_ENABLE_INPUT);
    }

    ai->blocksize = 0;
    MP_VERBOSE(ai, "ioctl dsp getblocksize: %d\n",
           err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_GETBLKSIZE, &ai->blocksize));
    if (err < 0) {
        MP_ERR(ai, "Unable to get block size!\n");
    }
    MP_VERBOSE(ai, "blocksize: %d\n", ai->blocksize);

    // correct the blocksize to a reasonable value
    if (ai->blocksize <= 0) {
        ai->blocksize = 4096*ai->channels*2;
        MP_ERR(ai, "Audio block size is zero, setting to %d!\n", ai->blocksize);
    } else if (ai->blocksize < 4096*ai->channels*2) {
        ai->blocksize *= 4096*ai->channels*2/ai->blocksize;
        MP_ERR(ai, "Audio block size too low, setting to %d!\n", ai->blocksize);
    }

    ai->samplesize = 16;
    ai->bytes_per_sample = 2;

    return 0;
}
