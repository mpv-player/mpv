#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#if defined(USE_TV) && defined(HAVE_TV_V4L) && defined(USE_OSS_AUDIO)

#include <string.h> /* strerror */
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#ifdef HAVE_SOUNDCARD_H
#include <soundcard.h>
#else
#include <linux/soundcard.h>
#endif
#endif

#include "audio_in.h"
#include "mp_msg.h"

int ai_oss_set_samplerate(audio_in_t *ai)
{
    int tmp = ai->req_samplerate;
    if (ioctl(ai->oss.audio_fd, SNDCTL_DSP_SPEED, &tmp) == -1) return -1;
    ai->samplerate = ai->req_samplerate;
    return 0;
}

int ai_oss_set_channels(audio_in_t *ai)
{
    int err;
    int ioctl_param;

    if (ai->req_channels > 2)
    {
	ioctl_param = ai->req_channels;
	mp_msg(MSGT_TV, MSGL_V, "ioctl dsp channels: %d\n",
	       err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_CHANNELS, &ioctl_param));
	if (err < 0) {
	    mp_msg(MSGT_TV, MSGL_ERR, "Unable to set channel count: %d\n",
		   ai->req_channels);
	    return -1;
	}
    }
    else
    {
	ioctl_param = (ai->req_channels == 2);
	mp_msg(MSGT_TV, MSGL_V, "ioctl dsp stereo: %d (req: %d)\n",
	       err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_STEREO, &ioctl_param),
	       ioctl_param);
	if (err < 0) {
	    mp_msg(MSGT_TV, MSGL_ERR, "Unable to set stereo: %d\n",
		   ai->req_channels == 2);
	    return -1;
	}
    }
    ai->channels = ai->req_channels;
    return 0;
}

int ai_oss_init(audio_in_t *ai)
{
    int err;
    int ioctl_param;

    ai->oss.audio_fd = open(ai->oss.device, O_RDONLY);
    if (ai->oss.audio_fd < 0)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "unable to open '%s': %s\n",
	       ai->oss.device, strerror(errno));
	return -1;
    }
	
    ioctl_param = 0 ;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp getfmt: %d\n",
	   ioctl(ai->oss.audio_fd, SNDCTL_DSP_GETFMTS, &ioctl_param));
	
    mp_msg(MSGT_TV, MSGL_V, "Supported formats: %x\n", ioctl_param);
    if (!(ioctl_param & AFMT_S16_LE))
	mp_msg(MSGT_TV, MSGL_ERR, "notsupported format\n");

    ioctl_param = AFMT_S16_LE;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp setfmt: %d\n",
	   err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_SETFMT, &ioctl_param));
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, "Unable to set audio format.");
	return -1;
    }

    if (ai_oss_set_channels(ai) < 0) return -1;
	
    ioctl_param = ai->req_samplerate;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp speed: %d\n",
	   err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_SPEED, &ioctl_param));
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, "Unable to set samplerate: %d\n",
	       ai->req_samplerate);
	return -1;
    }
    ai->samplerate = ai->req_samplerate;

    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp trigger: %d\n",
	   ioctl(ai->oss.audio_fd, SNDCTL_DSP_GETTRIGGER, &ioctl_param));
    mp_msg(MSGT_TV, MSGL_V, "trigger: %x\n", ioctl_param);
    ioctl_param = PCM_ENABLE_INPUT;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp trigger: %d\n",
	   err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_SETTRIGGER, &ioctl_param));
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, "Unable to set trigger: %d\n",
	       PCM_ENABLE_INPUT);
	return -1;
    }

    ai->blocksize = 0;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp getblocksize: %d\n",
	   err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_GETBLKSIZE, &ai->blocksize));
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, "Unable to get block size!\n");
    }
    mp_msg(MSGT_TV, MSGL_V, "blocksize: %d\n", ai->blocksize);

    // correct the blocksize to a reasonable value
    if (ai->blocksize <= 0) {
	ai->blocksize = 4096*ai->channels*2;
	mp_msg(MSGT_TV, MSGL_ERR, "audio block size is zero, setting to %d!\n", ai->blocksize);
    } else if (ai->blocksize < 4096*ai->channels*2) {
	ai->blocksize *= 4096*ai->channels*2/ai->blocksize;
	mp_msg(MSGT_TV, MSGL_ERR, "audio block size too low, setting to %d!\n", ai->blocksize);
    }

    ai->samplesize = 16;
    ai->bytes_per_sample = 2;

    return 0;
}

#endif /* USE_OSS_AUDIO */
