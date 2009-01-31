/*
 * ALSA 0.5.x audio output driver
 *
 * Copyright (C) 2001 Alex Beregszaszi
 *
 * Thanks to Arpi for helping me ;)
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

#include <errno.h>
#include <sys/asoundlib.h>

#include "config.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"

#include "mp_msg.h"
#include "help_mp.h"

static ao_info_t info = 
{
    "ALSA-0.5.x audio output",
    "alsa5",
    "Alex Beregszaszi",
    ""
};

LIBAO_EXTERN(alsa5)

static snd_pcm_t *alsa_handler;
static snd_pcm_format_t alsa_format;
static int alsa_rate = SND_PCM_RATE_CONTINUOUS;

/* to set/get/query special features/parameters */
static int control(int cmd, void *arg)
{
    return CONTROL_UNKNOWN;
}

/*
    open & setup audio device
    return: 1=success 0=fail
*/
static int init(int rate_hz, int channels, int format, int flags)
{
    int err;
    int cards = -1;
    snd_pcm_channel_params_t params;
    snd_pcm_channel_setup_t setup;
    snd_pcm_info_t info;
    snd_pcm_channel_info_t chninfo;

    mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_ALSA5_InitInfo, rate_hz,
	channels, af_fmt2str_short(format));

    alsa_handler = NULL;

    mp_msg(MSGT_AO, MSGL_V, "alsa-init: compiled for ALSA-%s (%d)\n", SND_LIB_VERSION_STR,
        SND_LIB_VERSION);

    if ((cards = snd_cards()) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_SoundCardNotFound);
	return 0;
    }

    ao_data.format = format;
    ao_data.channels = channels;
    ao_data.samplerate = rate_hz;
    ao_data.bps = ao_data.samplerate*ao_data.channels;
    ao_data.outburst = OUTBURST;
    ao_data.buffersize = 16384;

    memset(&alsa_format, 0, sizeof(alsa_format));
    switch (format)
    {
	case AF_FORMAT_S8:
	    alsa_format.format = SND_PCM_SFMT_S8;
	    break;
	case AF_FORMAT_U8:
	    alsa_format.format = SND_PCM_SFMT_U8;
	    break;
	case AF_FORMAT_U16_LE:
	    alsa_format.format = SND_PCM_SFMT_U16_LE;
	    break;
	case AF_FORMAT_U16_BE:
	    alsa_format.format = SND_PCM_SFMT_U16_BE;
	    break;
#ifndef WORDS_BIGENDIAN
	case AF_FORMAT_AC3:
#endif
	case AF_FORMAT_S16_LE:
	    alsa_format.format = SND_PCM_SFMT_S16_LE;
	    break;
#ifdef WORDS_BIGENDIAN
	case AF_FORMAT_AC3:
#endif
	case AF_FORMAT_S16_BE:
	    alsa_format.format = SND_PCM_SFMT_S16_BE;
	    break;
	default:
	    alsa_format.format = SND_PCM_SFMT_MPEG;
	    break;
    }
    
    switch(alsa_format.format)
    {
	case SND_PCM_SFMT_S16_LE:
	case SND_PCM_SFMT_U16_LE:
	    ao_data.bps *= 2;
	    break;
	case -1:
	    mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_InvalidFormatReq,af_fmt2str_short(format));
	    return 0;
	default:
	    break;
    }

    switch(rate_hz)
    {
	case 8000:
	    alsa_rate = SND_PCM_RATE_8000;
	    break;
	case 11025:
	    alsa_rate = SND_PCM_RATE_11025;
	    break;
	case 16000:
	    alsa_rate = SND_PCM_RATE_16000;
	    break;
	case 22050:
	    alsa_rate = SND_PCM_RATE_22050;
	    break;
	case 32000:
	    alsa_rate = SND_PCM_RATE_32000;
	    break;
	case 44100:
	    alsa_rate = SND_PCM_RATE_44100;
	    break;
	case 48000:
	    alsa_rate = SND_PCM_RATE_48000;
	    break;
	case 88200:
	    alsa_rate = SND_PCM_RATE_88200;
	    break;
	case 96000:
	    alsa_rate = SND_PCM_RATE_96000;
	    break;
	case 176400:
	    alsa_rate = SND_PCM_RATE_176400;
	    break;
	case 192000:
	    alsa_rate = SND_PCM_RATE_192000;
	    break;
	default:
	    alsa_rate = SND_PCM_RATE_CONTINUOUS;
	    break;
    }

    alsa_format.rate = ao_data.samplerate;
    alsa_format.voices = ao_data.channels;
    alsa_format.interleave = 1;

    if ((err = snd_pcm_open(&alsa_handler, 0, 0, SND_PCM_OPEN_PLAYBACK)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_PlayBackError, snd_strerror(err));
	return 0;
    }

    if ((err = snd_pcm_info(alsa_handler, &info)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_PcmInfoError, snd_strerror(err));
	return 0;
    }

    mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_ALSA5_SoundcardsFound,
	cards, info.name);

    if (info.flags & SND_PCM_INFO_PLAYBACK)
    {
	memset(&chninfo, 0, sizeof(chninfo));
	chninfo.channel = SND_PCM_CHANNEL_PLAYBACK;
	if ((err = snd_pcm_channel_info(alsa_handler, &chninfo)) < 0)
	{
	    mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_PcmChanInfoError, snd_strerror(err));
	    return 0;
	}

#ifndef __QNX__
	if (chninfo.buffer_size)
	    ao_data.buffersize = chninfo.buffer_size;
#endif

	mp_msg(MSGT_AO, MSGL_V, "alsa-init: setting preferred buffer size from driver: %d bytes\n",
	    ao_data.buffersize);
    }

    memset(&params, 0, sizeof(params));
    params.channel = SND_PCM_CHANNEL_PLAYBACK;
    params.mode = SND_PCM_MODE_STREAM;
    params.format = alsa_format;
    params.start_mode = SND_PCM_START_DATA;
    params.stop_mode = SND_PCM_STOP_ROLLOVER;
    params.buf.stream.queue_size = ao_data.buffersize;
    params.buf.stream.fill = SND_PCM_FILL_NONE;

    if ((err = snd_pcm_channel_params(alsa_handler, &params)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_CantSetParms, snd_strerror(err));
	return 0;
    }

    memset(&setup, 0, sizeof(setup));
    setup.channel = SND_PCM_CHANNEL_PLAYBACK;
    setup.mode = SND_PCM_MODE_STREAM;
    setup.format = alsa_format;
    setup.buf.stream.queue_size = ao_data.buffersize;
    setup.msbits_per_sample = ao_data.bps;
    
    if ((err = snd_pcm_channel_setup(alsa_handler, &setup)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_CantSetChan, snd_strerror(err));
	return 0;
    }

    if ((err = snd_pcm_channel_prepare(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_ChanPrepareError, snd_strerror(err));
	return 0;
    }

    mp_msg(MSGT_AO, MSGL_INFO, "AUDIO: %d Hz/%d channels/%d bps/%d bytes buffer/%s\n",
	ao_data.samplerate, ao_data.channels, ao_data.bps, ao_data.buffersize,
	snd_pcm_get_format_name(alsa_format.format));
    return 1;
}

/* close audio device */
static void uninit(int immed)
{
    int err;

    if ((err = snd_pcm_playback_drain(alsa_handler)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_DrainError, snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_channel_flush(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_FlushError, snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_close(alsa_handler)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_PcmCloseError, snd_strerror(err));
	return;
    }
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset(void)
{
    int err;

    if ((err = snd_pcm_playback_drain(alsa_handler)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_ResetDrainError, snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_channel_flush(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_ResetFlushError, snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_channel_prepare(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_ResetChanPrepareError, snd_strerror(err));
	return;
    }
}

/* stop playing, keep buffers (for pause) */
static void audio_pause(void)
{
    int err;

    if ((err = snd_pcm_playback_drain(alsa_handler)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_PauseDrainError, snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_channel_flush(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_PauseFlushError, snd_strerror(err));
	return;
    }
}

/* resume playing, after audio_pause() */
static void audio_resume(void)
{
    int err;
    if ((err = snd_pcm_channel_prepare(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_ResumePrepareError, snd_strerror(err));
	return;
    }
}

/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
*/
static int play(void* data, int len, int flags)
{
    int got_len;
    
    if (!len)
	return 0;
    
    if ((got_len = snd_pcm_write(alsa_handler, data, len)) < 0)
    {
	if (got_len == -EPIPE) /* underrun? */
	{
	    mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_Underrun);
	    if ((got_len = snd_pcm_channel_prepare(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
	    {
		mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_PlaybackPrepareError, snd_strerror(got_len));
		return 0;
	    }
	    if ((got_len = snd_pcm_write(alsa_handler, data, len)) < 0)
	    {
		mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_WriteErrorAfterReset,
		    snd_strerror(got_len));
		return 0;
	    }
	    return got_len; /* 2nd write was ok */
	}
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ALSA5_OutPutError, snd_strerror(got_len));
	return 0;
    }
    return got_len;
}

/* how many byes are free in the buffer */
static int get_space(void)
{
    snd_pcm_channel_status_t ch_stat;
    
    ch_stat.channel = SND_PCM_CHANNEL_PLAYBACK;

    if (snd_pcm_channel_status(alsa_handler, &ch_stat) < 0)
	return 0; /* error occurred */
    else
	return ch_stat.free;
}

/* delay in seconds between first and last sample in buffer */
static float get_delay(void)
{
    snd_pcm_channel_status_t ch_stat;
    
    ch_stat.channel = SND_PCM_CHANNEL_PLAYBACK;
    
    if (snd_pcm_channel_status(alsa_handler, &ch_stat) < 0)
	return (float)ao_data.buffersize/(float)ao_data.bps; /* error occurred */
    else
	return (float)ch_stat.count/(float)ao_data.bps;
}
