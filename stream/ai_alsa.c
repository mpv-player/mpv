#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <alloca.h>

#include "config.h"

#include <alsa/asoundlib.h>
#include "audio_in.h"
#include "mp_msg.h"
#include "help_mp.h"

int ai_alsa_setup(audio_in_t *ai)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    int buffer_size;
    int err;
    unsigned int rate;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    err = snd_pcm_hw_params_any(ai->alsa.handle, params);
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig);
	return -1;
    }
    err = snd_pcm_hw_params_set_access(ai->alsa.handle, params,
				       SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_UnavailableAccessType);
	return -1;
    }
    err = snd_pcm_hw_params_set_format(ai->alsa.handle, params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt);
	return -1;
    }
    err = snd_pcm_hw_params_set_channels(ai->alsa.handle, params, ai->req_channels);
    if (err < 0) {
	ai->channels = snd_pcm_hw_params_get_channels(params);
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_UnavailableChanCount,
	       ai->channels);
    } else {
	ai->channels = ai->req_channels;
    }

    err = snd_pcm_hw_params_set_rate_near(ai->alsa.handle, params, ai->req_samplerate, 0);
    assert(err >= 0);
    rate = err;
    ai->samplerate = rate;

    ai->alsa.buffer_time = 1000000;
    ai->alsa.buffer_time = snd_pcm_hw_params_set_buffer_time_near(ai->alsa.handle, params,
							       ai->alsa.buffer_time, 0);
    assert(ai->alsa.buffer_time >= 0);
    ai->alsa.period_time = ai->alsa.buffer_time / 4;
    ai->alsa.period_time = snd_pcm_hw_params_set_period_time_near(ai->alsa.handle, params,
							       ai->alsa.period_time, 0);
    assert(ai->alsa.period_time >= 0);
    err = snd_pcm_hw_params(ai->alsa.handle, params);
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams);
	snd_pcm_hw_params_dump(params, ai->alsa.log);
	return -1;
    }
    ai->alsa.chunk_size = snd_pcm_hw_params_get_period_size(params, 0);
    buffer_size = snd_pcm_hw_params_get_buffer_size(params);
    if (ai->alsa.chunk_size == buffer_size) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize, ai->alsa.chunk_size, (long)buffer_size);
	return -1;
    }
    snd_pcm_sw_params_current(ai->alsa.handle, swparams);
    err = snd_pcm_sw_params_set_sleep_min(ai->alsa.handle, swparams,0);
    assert(err >= 0);
    err = snd_pcm_sw_params_set_avail_min(ai->alsa.handle, swparams, ai->alsa.chunk_size);
    assert(err >= 0);

    err = snd_pcm_sw_params_set_start_threshold(ai->alsa.handle, swparams, 0);
    assert(err >= 0);
    err = snd_pcm_sw_params_set_stop_threshold(ai->alsa.handle, swparams, buffer_size);
    assert(err >= 0);

    assert(err >= 0);
    if (snd_pcm_sw_params(ai->alsa.handle, swparams) < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_CannotInstallSWParams);
	snd_pcm_sw_params_dump(swparams, ai->alsa.log);
	return -1;
    }

    if (mp_msg_test(MSGT_TV, MSGL_V)) {
	snd_pcm_dump(ai->alsa.handle, ai->alsa.log);
    }

    ai->alsa.bits_per_sample = snd_pcm_format_physical_width(SND_PCM_FORMAT_S16_LE);
    ai->alsa.bits_per_frame = ai->alsa.bits_per_sample * ai->channels;
    ai->blocksize = ai->alsa.chunk_size * ai->alsa.bits_per_frame / 8;
    ai->samplesize = ai->alsa.bits_per_sample;
    ai->bytes_per_sample = ai->alsa.bits_per_sample/8;

    return 0;
}

int ai_alsa_init(audio_in_t *ai)
{
    int err;
    
    err = snd_pcm_open(&ai->alsa.handle, ai->alsa.device, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_ErrorOpeningAudio, snd_strerror(err));
	return -1;
    }
    
    err = snd_output_stdio_attach(&ai->alsa.log, stderr, 0);
    
    if (err < 0) {
	return -1;
    }
    
    err = ai_alsa_setup(ai);

    return err;
}

#ifndef timersub
#define	timersub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	if ((result)->tv_usec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_usec += 1000000; \
	} \
} while (0)
#endif

int ai_alsa_xrun(audio_in_t *ai)
{
    snd_pcm_status_t *status;
    int res;
	
    snd_pcm_status_alloca(&status);
    if ((res = snd_pcm_status(ai->alsa.handle, status))<0) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_AlsaStatusError, snd_strerror(res));
	return -1;
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
	struct timeval now, diff, tstamp;
	gettimeofday(&now, 0);
	snd_pcm_status_get_trigger_tstamp(status, &tstamp);
	timersub(&now, &tstamp, &diff);
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_AlsaXRUN,
	       diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
	if (mp_msg_test(MSGT_TV, MSGL_V)) {
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_AlsaStatus);
	    snd_pcm_status_dump(status, ai->alsa.log);
	}
	if ((res = snd_pcm_prepare(ai->alsa.handle))<0) {
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_AlsaXRUNPrepareError, snd_strerror(res));
	    return -1;
	}
	return 0;		/* ok, data should be accepted again */
    }
    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_MPDEMUX_AIALSA_AlsaReadWriteError);
    return -1;
}
