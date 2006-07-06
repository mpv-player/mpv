#include <stdio.h>
#include <stdlib.h>

#include "config.h"

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
#include "help_mp.h"

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
	mp_msg(MSGT_TV, MSGL_V, "ioctl dsp channels: %d\n",
	       err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_CHANNELS, &ioctl_param));
	if (err < 0) {
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_Unable2SetChanCount,
		   ai->req_channels);
	    return -1;
	}
	ai->channels = ioctl_param;
    }
    else
    {
	ioctl_param = (ai->req_channels == 2);
	mp_msg(MSGT_TV, MSGL_V, "ioctl dsp stereo: %d (req: %d)\n",
	       err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_STEREO, &ioctl_param),
	       ioctl_param);
	if (err < 0) {
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_Unable2SetStereo,
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

    ai->oss.audio_fd = open(ai->oss.device, O_RDONLY);
    if (ai->oss.audio_fd < 0)
    {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_Unable2Open,
	       ai->oss.device, strerror(errno));
	return -1;
    }
	
    ioctl_param = 0 ;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp getfmt: %d\n",
	   ioctl(ai->oss.audio_fd, SNDCTL_DSP_GETFMTS, &ioctl_param));
	
    mp_msg(MSGT_TV, MSGL_V, "Supported formats: %x\n", ioctl_param);
    if (!(ioctl_param & AFMT_S16_LE))
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_UnsupportedFmt);

    ioctl_param = AFMT_S16_LE;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp setfmt: %d\n",
	   err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_SETFMT, &ioctl_param));
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt);
	return -1;
    }

    if (ai_oss_set_channels(ai) < 0) return -1;
	
    ioctl_param = ai->req_samplerate;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp speed: %d\n",
	   err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_SPEED, &ioctl_param));
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate,
	       ai->req_samplerate);
	return -1;
    }
    ai->samplerate = ioctl_param;

    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp trigger: %d\n",
	   ioctl(ai->oss.audio_fd, SNDCTL_DSP_GETTRIGGER, &ioctl_param));
    mp_msg(MSGT_TV, MSGL_V, "trigger: %x\n", ioctl_param);
    ioctl_param = PCM_ENABLE_INPUT;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp trigger: %d\n",
	   err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_SETTRIGGER, &ioctl_param));
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger,
	       PCM_ENABLE_INPUT);
    }

    ai->blocksize = 0;
    mp_msg(MSGT_TV, MSGL_V, "ioctl dsp getblocksize: %d\n",
	   err = ioctl(ai->oss.audio_fd, SNDCTL_DSP_GETBLKSIZE, &ai->blocksize));
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize);
    }
    mp_msg(MSGT_TV, MSGL_V, "blocksize: %d\n", ai->blocksize);

    // correct the blocksize to a reasonable value
    if (ai->blocksize <= 0) {
	ai->blocksize = 4096*ai->channels*2;
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero, ai->blocksize);
    } else if (ai->blocksize < 4096*ai->channels*2) {
	ai->blocksize *= 4096*ai->channels*2/ai->blocksize;
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low, ai->blocksize);
    }

    ai->samplesize = 16;
    ai->bytes_per_sample = 2;

    return 0;
}
