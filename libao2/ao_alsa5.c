/*
  ao_alsa5 - ALSA-0.5.x output plugin for MPlayer

  (C) Alex Beregszaszi <alex@naxine.org>

  Thanks to Arpi for helping me ;)
*/

#include <errno.h>
#include <sys/asoundlib.h>

#include "../config.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"

extern int verbose;

static ao_info_t info = 
{
    "ALSA-0.5.x audio output",
    "alsa5",
    "Alex Beregszaszi <alex@naxine.org>",
    ""
};

LIBAO_EXTERN(alsa5)

static snd_pcm_t *alsa_handler;
static snd_pcm_format_t alsa_format;
static int alsa_rate = SND_PCM_RATE_CONTINUOUS;

/* to set/get/query special features/parameters */
static int control(int cmd, int arg)
{
    return(CONTROL_UNKNOWN);
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

    printf("alsa-init: requested format: %d Hz, %d channels, %s\n", rate_hz,
	channels, audio_out_format_name(format));

    alsa_handler = NULL;

    if (verbose)
	printf("alsa-init: compiled for ALSA-%s (%d)\n", SND_LIB_VERSION_STR,
	    SND_LIB_VERSION);

    if ((cards = snd_cards()) < 0)
    {
	printf("alsa-init: no soundcards found\n");
	return(0);
    }

    ao_data.format = format;
    ao_data.channels = channels - 1;
    ao_data.samplerate = rate_hz;
    ao_data.bps = ao_data.samplerate*(ao_data.channels+1);
    ao_data.outburst = OUTBURST;
    ao_data.buffersize = 16384;

    memset(&alsa_format, 0, sizeof(alsa_format));
    switch (format)
    {
	case AFMT_S8:
	    alsa_format.format = SND_PCM_SFMT_S8;
	    break;
	case AFMT_U8:
	    alsa_format.format = SND_PCM_SFMT_U8;
	    break;
	case AFMT_U16_LE:
	    alsa_format.format = SND_PCM_SFMT_U16_LE;
	    break;
	case AFMT_U16_BE:
	    alsa_format.format = SND_PCM_SFMT_U16_BE;
	    break;
#ifndef WORDS_BIGENDIAN
	case AFMT_AC3:
#endif
	case AFMT_S16_LE:
	    alsa_format.format = SND_PCM_SFMT_S16_LE;
	    break;
#ifdef WORDS_BIGENDIAN
	case AFMT_AC3:
#endif
	case AFMT_S16_BE:
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
	    printf("alsa-init: invalid format (%s) requested - output disabled\n",
		audio_out_format_name(format));
	    return(0);
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
    alsa_format.voices = ao_data.channels*2;
    alsa_format.interleave = 1;

    if ((err = snd_pcm_open(&alsa_handler, 0, 0, SND_PCM_OPEN_PLAYBACK)) < 0)
    {
	printf("alsa-init: playback open error: %s\n", snd_strerror(err));
	return(0);
    }

    if ((err = snd_pcm_info(alsa_handler, &info)) < 0)
    {
	printf("alsa-init: pcm info error: %s\n", snd_strerror(err));
	return(0);
    }

    printf("alsa-init: %d soundcard%s found, using: %s\n", cards,
	(cards == 1) ? "" : "s", info.name);

    if (info.flags & SND_PCM_INFO_PLAYBACK)
    {
	memset(&chninfo, 0, sizeof(chninfo));
	chninfo.channel = SND_PCM_CHANNEL_PLAYBACK;
	if ((err = snd_pcm_channel_info(alsa_handler, &chninfo)) < 0)
	{
	    printf("alsa-init: pcm channel info error: %s\n", snd_strerror(err));
	    return(0);
	}

#ifndef __QNX__
	if (chninfo.buffer_size)
	    ao_data.buffersize = chninfo.buffer_size;
#endif

	if (verbose)
	    printf("alsa-init: setting preferred buffer size from driver: %d bytes\n",
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
	printf("alsa-init: error setting parameters: %s\n", snd_strerror(err));
	return(0);
    }

    memset(&setup, 0, sizeof(setup));
    setup.channel = SND_PCM_CHANNEL_PLAYBACK;
    setup.mode = SND_PCM_MODE_STREAM;
    setup.format = alsa_format;
    setup.buf.stream.queue_size = ao_data.buffersize;
    setup.msbits_per_sample = ao_data.bps;
    
    if ((err = snd_pcm_channel_setup(alsa_handler, &setup)) < 0)
    {
	printf("alsa-init: error setting up channel: %s\n", snd_strerror(err));
	return(0);
    }

    if ((err = snd_pcm_channel_prepare(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	printf("alsa-init: channel prepare error: %s\n", snd_strerror(err));
	return(0);
    }

    printf("AUDIO: %d Hz/%d channels/%d bps/%d bytes buffer/%s\n",
	ao_data.samplerate, ao_data.channels+1, ao_data.bps, ao_data.buffersize,
	snd_pcm_get_format_name(alsa_format.format));
    return(1);
}

/* close audio device */
static void uninit()
{
    int err;

    if ((err = snd_pcm_playback_drain(alsa_handler)) < 0)
    {
	printf("alsa-uninit: playback drain error: %s\n", snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_channel_flush(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	printf("alsa-uninit: playback flush error: %s\n", snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_close(alsa_handler)) < 0)
    {
	printf("alsa-uninit: pcm close error: %s\n", snd_strerror(err));
	return;
    }
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset()
{
    int err;

    if ((err = snd_pcm_playback_drain(alsa_handler)) < 0)
    {
	printf("alsa-reset: playback drain error: %s\n", snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_channel_flush(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	printf("alsa-reset: playback flush error: %s\n", snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_channel_prepare(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	printf("alsa-reset: channel prepare error: %s\n", snd_strerror(err));
	return;
    }
}

/* stop playing, keep buffers (for pause) */
static void audio_pause()
{
    int err;

    if ((err = snd_pcm_playback_drain(alsa_handler)) < 0)
    {
	printf("alsa-pause: playback drain error: %s\n", snd_strerror(err));
	return;
    }

    if ((err = snd_pcm_channel_flush(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	printf("alsa-pause: playback flush error: %s\n", snd_strerror(err));
	return;
    }
}

/* resume playing, after audio_pause() */
static void audio_resume()
{
    int err;
    if ((err = snd_pcm_channel_prepare(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
	printf("alsa-resume: channel prepare error: %s\n", snd_strerror(err));
	return;
    }
}

/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
*/
static int play(void* data, int len, int flags)
{
    if ((len = snd_pcm_write(alsa_handler, data, len)) != len)
    {
	if (len == -EPIPE) /* underrun? */
	{
	    printf("alsa-play: alsa underrun, resetting stream\n");
	    if ((len = snd_pcm_channel_prepare(alsa_handler, SND_PCM_CHANNEL_PLAYBACK)) < 0)
	    {
		printf("alsa-play: playback prepare error: %s\n", snd_strerror(len));
		return(0);
	    }
	    if ((len = snd_pcm_write(alsa_handler, data, len)) != len)
	    {
		printf("alsa-play: write error after reset: %s - giving up\n",
		    snd_strerror(len));
		return(0);
	    }
	    return(len); /* 2nd write was ok */
	}
	printf("alsa-play: output error: %s\n", snd_strerror(len));
    }
    return(len);
}

/* how many byes are free in the buffer */
static int get_space()
{
    snd_pcm_channel_status_t ch_stat;
    
    ch_stat.channel = SND_PCM_CHANNEL_PLAYBACK;

    if (snd_pcm_channel_status(alsa_handler, &ch_stat) < 0)
	return(0); /* error occured */
    else
	return(ch_stat.free);
}

/* delay in seconds between first and last sample in buffer */
static float get_delay()
{
    snd_pcm_channel_status_t ch_stat;
    
    ch_stat.channel = SND_PCM_CHANNEL_PLAYBACK;
    
    if (snd_pcm_channel_status(alsa_handler, &ch_stat) < 0)
	return((float)ao_data.buffersize/(float)ao_data.bps); /* error occured */
    else
	return((float)ch_stat.count/(float)ao_data.bps);
}
