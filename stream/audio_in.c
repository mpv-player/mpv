/*
 * This file is part of MPlayer.
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
#include <unistd.h>

#include "config.h"

#include "audio_in.h"
#include "mp_msg.h"
#include "help_mp.h"
#include <string.h>
#include <errno.h>

// sanitizes ai structure before calling other functions
int audio_in_init(audio_in_t *ai, int type)
{
    ai->type = type;
    ai->setup = 0;

    ai->channels = -1;
    ai->samplerate = -1;
    ai->blocksize = -1;
    ai->bytes_per_sample = -1;
    ai->samplesize = -1;

    switch (ai->type) {
#ifdef CONFIG_ALSA
    case AUDIO_IN_ALSA:
	ai->alsa.handle = NULL;
	ai->alsa.log = NULL;
	ai->alsa.device = strdup("default");
	return 0;
#endif
#ifdef CONFIG_OSS_AUDIO
    case AUDIO_IN_OSS:
	ai->oss.audio_fd = -1;
	ai->oss.device = strdup("/dev/dsp");
	return 0;
#endif
    default:
	return -1;
    }
}

int audio_in_setup(audio_in_t *ai)
{

    switch (ai->type) {
#ifdef CONFIG_ALSA
    case AUDIO_IN_ALSA:
	if (ai_alsa_init(ai) < 0) return -1;
	ai->setup = 1;
	return 0;
#endif
#ifdef CONFIG_OSS_AUDIO
    case AUDIO_IN_OSS:
	if (ai_oss_init(ai) < 0) return -1;
	ai->setup = 1;
	return 0;
#endif
    default:
	return -1;
    }
}

int audio_in_set_samplerate(audio_in_t *ai, int rate)
{
    switch (ai->type) {
#ifdef CONFIG_ALSA
    case AUDIO_IN_ALSA:
	ai->req_samplerate = rate;
	if (!ai->setup) return 0;
	if (ai_alsa_setup(ai) < 0) return -1;
	return ai->samplerate;
#endif
#ifdef CONFIG_OSS_AUDIO
    case AUDIO_IN_OSS:
	ai->req_samplerate = rate;
	if (!ai->setup) return 0;
	if (ai_oss_set_samplerate(ai) < 0) return -1;
	return ai->samplerate;
#endif
    default:
	return -1;
    }
}

int audio_in_set_channels(audio_in_t *ai, int channels)
{
    switch (ai->type) {
#ifdef CONFIG_ALSA
    case AUDIO_IN_ALSA:
	ai->req_channels = channels;
	if (!ai->setup) return 0;
	if (ai_alsa_setup(ai) < 0) return -1;
	return ai->channels;
#endif
#ifdef CONFIG_OSS_AUDIO
    case AUDIO_IN_OSS:
	ai->req_channels = channels;
	if (!ai->setup) return 0;
	if (ai_oss_set_channels(ai) < 0) return -1;
	return ai->channels;
#endif
    default:
	return -1;
    }
}

int audio_in_set_device(audio_in_t *ai, char *device)
{
#ifdef CONFIG_ALSA
    int i;
#endif
    if (ai->setup) return -1;
    switch (ai->type) {
#ifdef CONFIG_ALSA
    case AUDIO_IN_ALSA:
	if (ai->alsa.device) free(ai->alsa.device);
	ai->alsa.device = strdup(device);
	/* mplayer cannot handle colons in arguments */
	for (i = 0; i < (int)strlen(ai->alsa.device); i++) {
	    if (ai->alsa.device[i] == '.') ai->alsa.device[i] = ':';
	}
	return 0;
#endif
#ifdef CONFIG_OSS_AUDIO
    case AUDIO_IN_OSS:
	if (ai->oss.device) free(ai->oss.device);
	ai->oss.device = strdup(device);
	return 0;
#endif
    default:
	return -1;
    }
}

int audio_in_uninit(audio_in_t *ai)
{
    if (ai->setup) {
	switch (ai->type) {
#ifdef CONFIG_ALSA
	case AUDIO_IN_ALSA:
	    if (ai->alsa.log)
		snd_output_close(ai->alsa.log);
	    if (ai->alsa.handle) {
		snd_pcm_close(ai->alsa.handle);
	    }
	    ai->setup = 0;
	    return 0;
#endif
#ifdef CONFIG_OSS_AUDIO
	case AUDIO_IN_OSS:
	    close(ai->oss.audio_fd);
	    ai->setup = 0;
	    return 0;
#endif
	}
    }
    return -1;
}

int audio_in_start_capture(audio_in_t *ai)
{
    switch (ai->type) {
#ifdef CONFIG_ALSA
    case AUDIO_IN_ALSA:
	return snd_pcm_start(ai->alsa.handle);
#endif
#ifdef CONFIG_OSS_AUDIO
    case AUDIO_IN_OSS:
	return 0;
#endif
    default:
	return -1;
    }
}

int audio_in_read_chunk(audio_in_t *ai, unsigned char *buffer)
{
    int ret;

    switch (ai->type) {
#ifdef CONFIG_ALSA
    case AUDIO_IN_ALSA:
	ret = snd_pcm_readi(ai->alsa.handle, buffer, ai->alsa.chunk_size);
	if (ret != ai->alsa.chunk_size) {
	    if (ret < 0) {
		mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio, snd_strerror(ret));
		if (ret == -EPIPE) {
		    if (ai_alsa_xrun(ai) == 0) {
			mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut);
		    } else {
			mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover);
		    }
		}
	    } else {
		mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples);
	    }
	    return -1;
	}
	return ret;
#endif
#ifdef CONFIG_OSS_AUDIO
    case AUDIO_IN_OSS:
	ret = read(ai->oss.audio_fd, buffer, ai->blocksize);
	if (ret != ai->blocksize) {
	    if (ret < 0) {
		mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio, strerror(errno));
	    } else {
		mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples);
	    }
	    return -1;
	}
	return ret;
#endif
    default:
	return -1;
    }
}
