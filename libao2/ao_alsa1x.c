/*
  ao_alsa9 - ALSA-0.9.x output plugin for MPlayer

  (C) Alex Beregszaszi <alex@naxine.org>
  
  modified for better alsa-0.9.0beta8a-support by Joy Winter <joy@pingfm.org>
  additional AC3 passthrough support by Andy Lo A Foe <andy@alsaplayer.org>
  
  This driver is still at alpha stage. 
  If you want stable sound-support use the OSS emulation instead.
  
  Any bugreports regarding to this driver are welcome either to the mplayer-user-mailinglist or directly to the authors.
*/

#include <errno.h>

#include "../config.h"

#if HAVE_SYS_ASOUNDLIB_H
#include <sys/asoundlib.h>
#elif HAVE_ALSA_ASOUNDLIB_H
#include <alsa/asoundlib.h>
#else
#error "asoundlib.h is not in sys/ or alsa/ - please bugreport"
#endif

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"

extern int verbose;

static ao_info_t info = 
{
    "ALSA-0.9.x audio output",
    "alsa9",
    "Alex Beregszaszi <alex@naxine.org>, Joy Winter <joy@pingfm.org>",
    "under developement"
};

LIBAO_EXTERN(alsa9)


static snd_pcm_t *alsa_handler;
static snd_pcm_format_t alsa_format;
static snd_pcm_hw_params_t *alsa_hwparams;
static snd_pcm_sw_params_t *alsa_swparams;
static char *alsa_device;
#define ALSA_DEVICE_SIZE 48

static int alsa_fragsize = 8192; /* possible 4096, original 8192 */
static int alsa_fragcount = 8;

static int chunk_size = -1;
static int start_delay = 1;

snd_pcm_t *
spdif_init(int acard, int adevice)
{
	//char *pcm_name = "hw:0,2"; /* first card second device */
	char pcm_name[255];
	static snd_aes_iec958_t spdif;
	snd_pcm_info_t 	*info;
	snd_pcm_t *handler;
	snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
	unsigned int channels = 2;
	unsigned int rate = 48000;
	int err, c;

	if (err = snprintf(&pcm_name[0], 11, "hw:%1d,%1d", acard, adevice) <= 0)
	{
		return NULL;
	}

	if ((err = snd_pcm_open(&handler, pcm_name, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
	{
		fprintf(stderr, "open: %s\n", snd_strerror(err));
		return NULL;
	}

	snd_pcm_info_alloca(&info);

	if ((err = snd_pcm_info(handler, info)) < 0) {
		fprintf(stderr, "info: %s\n", snd_strerror(err));
		snd_pcm_close(handler);
		return NULL;
	}
        printf("device: %d, subdevice: %d\n", snd_pcm_info_get_device(info),
                snd_pcm_info_get_subdevice(info));                              
	{
        snd_ctl_elem_value_t *ctl;
        snd_ctl_t *ctl_handler;
        char ctl_name[12];
        int ctl_card;

	spdif.status[0] = IEC958_AES0_NONAUDIO |
			  IEC958_AES0_CON_EMPHASIS_NONE;
	spdif.status[1] = IEC958_AES1_CON_ORIGINAL |
			  IEC958_AES1_CON_PCM_CODER;
	spdif.status[2] = 0;
	spdif.status[3] = IEC958_AES3_CON_FS_48000;

        snd_ctl_elem_value_alloca(&ctl);
        snd_ctl_elem_value_set_interface(ctl, SND_CTL_ELEM_IFACE_PCM);
        snd_ctl_elem_value_set_device(ctl, snd_pcm_info_get_device(info));
        snd_ctl_elem_value_set_subdevice(ctl, snd_pcm_info_get_subdevice(info));
        snd_ctl_elem_value_set_name(ctl, SND_CTL_NAME_IEC958("", PLAYBACK,PCM_STREAM));
        snd_ctl_elem_value_set_iec958(ctl, &spdif);
        ctl_card = snd_pcm_info_get_card(info);
        if (ctl_card < 0) {
           fprintf(stderr, "Unable to setup the IEC958 (S/PDIF) interface - PCM has no assigned card");
           goto __diga_end;
        }
       sprintf(ctl_name, "hw:%d", ctl_card);
       printf("hw:%d\n", ctl_card);
       if ((err = snd_ctl_open(&ctl_handler, ctl_name, 0)) < 0) {
          fprintf(stderr, "Unable to open the control interface '%s': %s", ctl_name, snd_strerror(err));                                                    
          goto __diga_end;
       }
       if ((err = snd_ctl_elem_write(ctl_handler, ctl)) < 0) {
          fprintf(stderr, "Unable to update the IEC958 control: %s", snd_strerror(err));
          goto __diga_end;
       }
      snd_ctl_close(ctl_handler);
      __diga_end:                                                       

      }

	{
	  snd_pcm_hw_params_t *params;
	  snd_pcm_sw_params_t *swparams;
	  
	  snd_pcm_hw_params_alloca(&params);
	  snd_pcm_sw_params_alloca(&swparams);

          err = snd_pcm_hw_params_any(handler, params);
          if (err < 0) {
             fprintf(stderr, "Broken configuration for this PCM: no configurations available");                                                                        
	     return NULL;
	  }
	  err = snd_pcm_hw_params_set_access(handler, params,
	  				    SND_PCM_ACCESS_RW_INTERLEAVED);
	  if (err < 0) {
	    fprintf(stderr, "Access tyep not available");
	    return NULL;
	  }
	  err = snd_pcm_hw_params_set_format(handler, params, format);

	  if (err < 0) {
	    fprintf(stderr, "Sample format non available");
	    return NULL;
	  }

	  err = snd_pcm_hw_params_set_channels(handler, params, channels);

	  if (err < 0) {
	    fprintf(stderr, "Channels count non avaible");
	    return NULL;
	  }

          err = snd_pcm_hw_params_set_rate_near(handler, params, rate, 0);        assert(err >= 0);

	  err = snd_pcm_hw_params(handler, params);

	  if (err < 0) {
	    fprintf(stderr, "Cannot set buffer size\n");
	    return NULL;
	  }
	  snd_pcm_sw_params_current(handler, swparams);
	}  
	return handler;
}


/* to set/get/query special features/parameters */
static int control(int cmd, int arg)
{
    return(CONTROL_UNKNOWN);
}

#undef start /* orig. undef */
#define buffsize 
#define buffertime /* orig. undef? */
#define set_period
#define sw_params /* orig. undef */
#undef set_start_mode /* orig. undef */

/*
    open & setup audio device
    return: 1=success 0=fail
*/
static int init(int rate_hz, int channels, int format, int flags)
{
    int err;
    int cards = -1;
    snd_pcm_info_t *alsa_info;
    
    size_t xfer_align; //new
    snd_pcm_uframes_t start_threshold, stop_threshold; //new

    printf("alsa-init: this driver is still at alpha-stage. if you want stable sound support use the OSS emulation instead.\n");    
    printf("alsa-init: requested format: %d Hz, %d channels, %s\n", rate_hz,
	channels, audio_out_format_name(format));

    alsa_handler = NULL;

    if (verbose)
	printf("alsa-init: compiled for ALSA-%s\n", SND_LIB_VERSION_STR);

    if ((err = snd_card_next(&cards)) < 0 || cards < 0)
    {
	printf("alsa-init: no soundcards found: %s\n", snd_strerror(err));
	return(0);
    }

    ao_data.samplerate = rate_hz;
    ao_data.bps = channels; /* really this is bytes per frame so bad varname */
    ao_data.format = format;
    ao_data.channels = channels;
    ao_data.outburst = OUTBURST;
    ao_data.buffersize = 16384;

    switch (format)
    {
	case AFMT_S8:
	    alsa_format = SND_PCM_FORMAT_S8;
	    break;
	case AFMT_U8:
	    alsa_format = SND_PCM_FORMAT_U8;
	    break;
	case AFMT_U16_LE:
	    alsa_format = SND_PCM_FORMAT_U16_LE;
	    break;
	case AFMT_U16_BE:
	    alsa_format = SND_PCM_FORMAT_U16_BE;
	    break;
#ifndef WORDS_BIGENDIAN
	case AFMT_AC3:
#endif
	case AFMT_S16_LE:
	    alsa_format = SND_PCM_FORMAT_S16_LE;
	    break;
#ifdef WORDS_BIGENDIAN
	case AFMT_AC3:
#endif
	case AFMT_S16_BE:
	    alsa_format = SND_PCM_FORMAT_S16_BE;
	    break;
	default:
	    alsa_format = SND_PCM_FORMAT_MPEG;
	    break;
    }
    
    switch(alsa_format)
    {
	case SND_PCM_FORMAT_S16_LE:
	case SND_PCM_FORMAT_U16_LE:
	    ao_data.bps *= 2;
	    break;
	case -1:
	    printf("alsa-init: invalid format (%s) requested - output disabled\n",
		audio_out_format_name(format));
	    return(0);
	default:
	    break;
    }
    
    if ((err = snd_pcm_info_malloc(&alsa_info)) < 0)
    {
	printf("alsa-init: memory allocation error: %s\n", snd_strerror(err));
	return(0);
    }

    if (ao_subdevice != NULL)
	alsa_device = ao_subdevice;

    if (alsa_device == NULL)
    {
	if ((alsa_device = malloc(ALSA_DEVICE_SIZE)) == NULL)
	{
	    printf("alsa-init: memory allocation error: %s\n", strerror(errno));
	    return(0);
	}

	snprintf(alsa_device, ALSA_DEVICE_SIZE, "hw:%d,%d",
	    snd_pcm_info_get_device(alsa_info),
	    snd_pcm_info_get_subdevice(alsa_info));

	snd_pcm_info_free(alsa_info);
    }

    printf("alsa-init: %d soundcard%s found, using: %s\n", cards+1,
	(cards >= 0) ? "" : "s", alsa_device);

    if (format == AFMT_AC3) {
	    // Try to initialize the SPDIF interface
	    alsa_handler = spdif_init(0, 2);
    }	    
   
    if (!alsa_handler) {
	    if ((err = snd_pcm_open(&alsa_handler, alsa_device, SND_PCM_STREAM_PLAYBACK,
					    0)) < 0)
	    {
		    printf("alsa-init: playback open error: %s\n", snd_strerror(err));
		    return(0);
	    }
    }	    

    snd_pcm_hw_params_malloc(&alsa_hwparams);
    snd_pcm_sw_params_alloca(&alsa_swparams);
    if ((err = snd_pcm_hw_params_any(alsa_handler, alsa_hwparams)) < 0)
    {
	printf("alsa-init: unable to get initial parameters: %s\n",
	    snd_strerror(err));
	return(0);
    }
    
    if ((err = snd_pcm_hw_params_set_access(alsa_handler, alsa_hwparams,
	SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
	printf("alsa-init: unable to set access type: %s\n",
	    snd_strerror(err));
	return(0);
    }
    
    if ((err = snd_pcm_hw_params_set_format(alsa_handler, alsa_hwparams,
	alsa_format)) < 0)
    {
	printf("alsa-init: unable to set format: %s\n",
	    snd_strerror(err));
	return(0);
    }

    if ((err = snd_pcm_hw_params_set_channels(alsa_handler, alsa_hwparams,
	ao_data.channels)) < 0)
    {
	printf("alsa-init: unable to set channels: %s\n",
	    snd_strerror(err));
	return(0);
    }

        if ((err = snd_pcm_hw_params_set_rate_near(alsa_handler, alsa_hwparams, ao_data.samplerate, 0)) < 0) 
/* was originally only snd_pcm_hw_params_set_rate jp*/ 
        {
    	printf("alsa-init: unable to set samplerate-2: %s\n",
    	    snd_strerror(err));
	//snd_pcm_hw_params_dump(alsa_hwparams, errlog); jp
    	return(0);
        }

#ifdef set_period
    {
	if ((err = snd_pcm_hw_params_set_period_size(alsa_handler, alsa_hwparams, alsa_fragsize / 4, 0)) < 0)
	{
	    printf("alsa-init: unable to set periodsize: %s\n",
		snd_strerror(err));
	    return(0);
	}
		if ((err = snd_pcm_hw_params_set_periods(alsa_handler, alsa_hwparams, alsa_fragcount, 0)) < 0)
		{
		    printf("alsa-init: unable to set periods: %s\n",
			snd_strerror(err));
		    return(0);
		}
    }
#endif
#ifdef buffsize
    if ((err = snd_pcm_hw_params_get_buffer_size(alsa_hwparams)) < 0)
    {
	printf("alsa-init: unable to get buffer size: %s\n",
	    snd_strerror(err));
	return(0);
    } else
    {
	ao_data.buffersize = err;
        if (verbose)
	    printf("alsa-init: got buffersize %i\n", ao_data.buffersize);
    }
#endif

#ifdef buffertime
    {
      int alsa_buffer_time = 500000; /* original 60 */

	if ((err = snd_pcm_hw_params_set_buffer_time_near(alsa_handler, alsa_hwparams, alsa_buffer_time, 0)) < 0)
	{
	    printf("alsa-init: unable to set buffer time near: %s\n",
		snd_strerror(err));
	    return(0);
	} else
	    alsa_buffer_time = err;

	if ((err = snd_pcm_hw_params_set_period_time_near(alsa_handler, alsa_hwparams, alsa_buffer_time/4, 0)) < 0)
	  /* original: alsa_buffer_time/ao_data.bps */
	{
	    printf("alsa-init: unable to set period time: %s\n",
		snd_strerror(err));
	    return(0);
	}
	printf("alsa-init: buffer_time: %d, period_time :%d\n",
	    alsa_buffer_time, err);
    }
#endif

    if ((err = snd_pcm_hw_params(alsa_handler, alsa_hwparams)) < 0)
    {
	printf("alsa-init: unable to set parameters: %s\n",
	    snd_strerror(err));
	return(0);
    }

#ifdef sw_params
    {
    chunk_size = snd_pcm_hw_params_get_period_size(alsa_hwparams, 0);
    start_threshold = (double) ao_data.samplerate * start_delay / 1000000;
    xfer_align = snd_pcm_sw_params_get_xfer_align(alsa_swparams);

    if ((err = snd_pcm_sw_params_current(alsa_handler, alsa_swparams)) < 0)
    {
	printf("alsa-init: unable to get parameters: %s\n",
	    snd_strerror(err));
	return(0);
    }
	
	if ((err = snd_pcm_sw_params_set_avail_min(alsa_handler, alsa_swparams, chunk_size)) < 0)
	  {
	    printf("alsa-init: unable to set avail_min %s\n",snd_strerror(err));
	    return(0);
	  }


	
	if ((err = snd_pcm_sw_params_set_start_threshold(alsa_handler, alsa_swparams, start_threshold)) < 0)
	  {
	    printf("alsa-init: unable to set start_threshold %s\n",snd_strerror(err));
	    return(0);
	  }
    } 
    //      if ((err = snd_pcm_sw_params_set_xfer_align(alsa_handler, alsa_swparams, xfer_align)) < 0)
    //{
    //	printf("alsa-init: unable to set xfer_align: %s\n",
    //	    snd_strerror(err));
    //	return(0);
    //}

#ifdef set_start_mode
    if ((err = snd_pcm_sw_params_set_start_mode(alsa_handler, alsa_swparams,
	SND_PCM_START_DATA)) < 0)
    {
	printf("alsa-init: unable to set start mode: %s\n",
	    snd_strerror(err));
	return(0);
    }
#endif

    if ((err = snd_pcm_sw_params(alsa_handler, alsa_swparams)) < 0)
    {
	printf("alsa-init: unable to set parameters: %s\n",
	    snd_strerror(err));
	return(0);
    }

//    snd_pcm_sw_params_default(alsa_handler, alsa_swparams);
#endif
    if ((err = snd_pcm_prepare(alsa_handler)) < 0)
    {
	printf("alsa-init: pcm prepare error: %s\n", snd_strerror(err));
	return(0);
    }

#ifdef start
    if ((err = snd_pcm_start(alsa_handler)) < 0)
    {
	printf("alsa-init: pcm start error: %s\n", snd_strerror(err));
	if (err != -EPIPE)
	    return(0);
	if ((err = snd_pcm_start(alsa_handler)) < 0)
	{
	    printf("alsa-init: pcm start error: %s\n", snd_strerror(err));
		return(0);
	}
    }
#endif
    printf("AUDIO: %d Hz/%d channels/%d bpf/%d bytes buffer/%s\n",
	ao_data.samplerate, ao_data.channels, ao_data.bps, ao_data.buffersize,
	snd_pcm_format_description(alsa_format));
    return(1);
}

/* close audio device */
static void uninit()
{
    int err;

    if (alsa_device != NULL)
	free(alsa_device);

    snd_pcm_hw_params_free(alsa_hwparams);

    if ((err = snd_pcm_drain(alsa_handler)) < 0)
    {
	printf("alsa-uninit: pcm drain error: %s\n", snd_strerror(err));
	return;
    }

#ifdef start
    if ((err = snd_pcm_reset(alsa_handler)) < 0)
    {
	printf("alsa-uninit: pcm reset error: %s\n", snd_strerror(err));
	return;
    }
#endif

    if ((err = snd_pcm_close(alsa_handler)) < 0)
    {
	printf("alsa-uninit: pcm close error: %s\n", snd_strerror(err));
	return;
    }
}

static void audio_pause()
{
    int err;

    if ((err = snd_pcm_drain(alsa_handler)) < 0)
    {
	printf("alsa-pause: pcm drain error: %s\n", snd_strerror(err));
	return;
    }

#ifdef reset
    if ((err = snd_pcm_reset(alsa_handler)) < 0)
    {
	printf("alsa-pause: pcm reset error: %s\n", snd_strerror(err));
	return;
    }
#endif
}

static void audio_resume()
{
    int err;

    if ((err = snd_pcm_prepare(alsa_handler)) < 0)
    {
	printf("alsa-resume: pcm prepare error: %s\n", snd_strerror(err));
	return;
    }

#ifdef start
    if ((err = snd_pcm_start(alsa_handler)) < 0)
    {
	printf("alsa-resume: pcm start error: %s\n", snd_strerror(err));
	return;
    }
#endif
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset()
{
    int err;

    if ((err = snd_pcm_drain(alsa_handler)) < 0)
    {
	printf("alsa-reset: pcm drain error: %s\n", snd_strerror(err));
	return;
    }

#ifdef start
    if ((err = snd_pcm_reset(alsa_handler)) < 0)
    {
	printf("alsa-reset: pcm reset error: %s\n", snd_strerror(err));
	return;
    }
#endif

    if ((err = snd_pcm_prepare(alsa_handler)) < 0)
    {
	printf("alsa-reset: pcm prepare error: %s\n", snd_strerror(err));
	return;
    }

#ifdef start
    if ((err = snd_pcm_start(alsa_handler)) < 0)
    {
	printf("alsa-reset: pcm start error: %s\n", snd_strerror(err));
	return;
    }
#endif
}

/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
*/

static int play(void* data, int len, int flags)
{
    int got_len;
	
    got_len = snd_pcm_writei(alsa_handler, data, len / 4);
    
    //if ((got_len = snd_pcm_writei(alsa_handler, data, (len/ao_data.bps))) != (len/ao_data.bps)) {     
    //SHOULD BE FIXED      
	if (got_len == -EPIPE) /* underrun? */
	{
	    printf("alsa-play: alsa underrun, resetting stream\n");
	    if ((got_len = snd_pcm_prepare(alsa_handler)) < 0)
	    {
		printf("alsa-play: playback prepare error: %s\n", snd_strerror(got_len));
		return(0);
	    }
	    if ((got_len = snd_pcm_writei(alsa_handler, data, (len/ao_data.bps))) != (len/ao_data.bps))
	    {
		printf("alsa-play: write error after reset: %s - giving up\n",
		    snd_strerror(got_len));
		return(0);
	    }
	    return(len); /* 2nd write was ok */
    }
    return(len);
    //}
}

/* how many byes are free in the buffer */
static int get_space()
{
    snd_pcm_status_t *status;
    int ret;
    
    if ((ret = snd_pcm_status_malloc(&status)) < 0)
    {
	printf("alsa-space: memory allocation error: %s\n", snd_strerror(ret));
	return(0);
    }
    
    if ((ret = snd_pcm_status(alsa_handler, status)) < 0)
    {
	printf("alsa-space: cannot get pcm status: %s\n", snd_strerror(ret));
	return(0);
    }
    
    switch(snd_pcm_status_get_state(status))
    {
	case SND_PCM_STATE_OPEN:
	case SND_PCM_STATE_PREPARED:
	case SND_PCM_STATE_RUNNING:
	    ret = snd_pcm_status_get_avail(status) * ao_data.bps;
	    break;
	default:
	    ret = 0;
    }
    
    snd_pcm_status_free(status);
    
    if (ret < 0)
	ret = 0;
    return(ret);
}

/* delay in seconds between first and last sample in buffer */
static float get_delay()
{
    snd_pcm_status_t *status;
    float ret;
    
    if ((ret = snd_pcm_status_malloc(&status)) < 0)
    {
	printf("alsa-delay: memory allocation error: %s\n", snd_strerror(ret));
	return(0);
    }
    
    if ((ret = snd_pcm_status(alsa_handler, status)) < 0)
    {
	printf("alsa-delay: cannot get pcm status: %s\n", snd_strerror(ret));
	return(0);
    }
    
    switch(snd_pcm_status_get_state(status))
    {
	case SND_PCM_STATE_OPEN:
	case SND_PCM_STATE_PREPARED:
	case SND_PCM_STATE_RUNNING:
	    ret = (float)snd_pcm_status_get_delay(status)/(float)ao_data.samplerate;
	    break;
	default:
	    ret = 0;
    }
    
    snd_pcm_status_free(status);

    if (ret < 0)
	ret = 0;
    return(ret);
}
