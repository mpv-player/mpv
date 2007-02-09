/*
  ao_alsa9/1.x - ALSA-0.9.x-1.x output plugin for MPlayer

  (C) Alex Beregszaszi
  
  modified for real alsa-0.9.0-support by Zsolt Barat <joy@streamminister.de>
  additional AC3 passthrough support by Andy Lo A Foe <andy@alsaplayer.org>  
  08/22/2002 iec958-init rewritten and merged with common init, zsolt
  04/13/2004 merged with ao_alsa1.x, fixes provided by Jindrich Makovicka
  04/25/2004 printfs converted to mp_msg, Zsolt.
  
  Any bugreports regarding to this driver are welcome.
*/

#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "subopt-helper.h"
#include "mixer.h"
#include "mp_msg.h"
#include "help_mp.h"

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

#if HAVE_SYS_ASOUNDLIB_H
#include <sys/asoundlib.h>
#elif HAVE_ALSA_ASOUNDLIB_H
#include <alsa/asoundlib.h>
#else
#error "asoundlib.h is not in sys/ or alsa/ - please bugreport"
#endif


#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"

static ao_info_t info = 
{
    "ALSA-0.9.x-1.x audio output",
    "alsa",
    "Alex Beregszaszi, Zsolt Barat <joy@streamminister.de>",
    "under developement"
};

LIBAO_EXTERN(alsa)

static snd_pcm_t *alsa_handler;
static snd_pcm_format_t alsa_format;
static snd_pcm_hw_params_t *alsa_hwparams;
static snd_pcm_sw_params_t *alsa_swparams;

/* 16 sets buffersize to 16 * chunksize is as default 1024
 * which seems to be good avarge for most situations 
 * so buffersize is 16384 frames by default */
static int alsa_fragcount = 16;
static snd_pcm_uframes_t chunk_size = 1024;

static size_t bytes_per_sample;

static int ao_noblock = 0;

static int open_mode;
static int alsa_can_pause = 0;

#define ALSA_DEVICE_SIZE 256

#undef BUFFERTIME
#define SET_CHUNKSIZE

static void alsa_error_handler(const char *file, int line, const char *function,
			       int err, const char *format, ...)
{
  char tmp[0xc00];
  va_list va;

  va_start(va, format);
  vsnprintf(tmp, sizeof tmp, format, va);
  va_end(va);
  tmp[sizeof tmp - 1] = '\0';

  if (err)
    mp_msg(MSGT_AO, MSGL_ERR, "[AO_ALSA] alsa-lib: %s:%i:(%s) %s: %s\n",
	   file, line, function, tmp, snd_strerror(err));
  else
    mp_msg(MSGT_AO, MSGL_ERR, "[AO_ALSA] alsa-lib: %s:%i:(%s) %s\n",
	   file, line, function, tmp);
}

/* to set/get/query special features/parameters */
static int control(int cmd, void *arg)
{
  switch(cmd) {
  case AOCONTROL_QUERY_FORMAT:
    return CONTROL_TRUE;
  case AOCONTROL_GET_VOLUME:
  case AOCONTROL_SET_VOLUME:
    {
      ao_control_vol_t *vol = (ao_control_vol_t *)arg;

      int err;
      snd_mixer_t *handle;
      snd_mixer_elem_t *elem;
      snd_mixer_selem_id_t *sid;

      static char *mix_name = "PCM";
      static char *card = "default";
      static int mix_index = 0;

      long pmin, pmax;
      long get_vol, set_vol;
      float f_multi;

      if(mixer_channel) {
	 char *test_mix_index;

	 mix_name = strdup(mixer_channel);
	 if ((test_mix_index = strchr(mix_name, ','))){
		*test_mix_index = 0;
		test_mix_index++;
		mix_index = strtol(test_mix_index, &test_mix_index, 0);

		if (*test_mix_index){
		  mp_msg(MSGT_AO,MSGL_ERR,
		    MSGTR_AO_ALSA_InvalidMixerIndexDefaultingToZero);
		  mix_index = 0 ;
		}
	 }
      }
      if(mixer_device) card = mixer_device;

      if(ao_data.format == AF_FORMAT_AC3)
	return CONTROL_TRUE;

      //allocate simple id
      snd_mixer_selem_id_alloca(&sid);
	
      //sets simple-mixer index and name
      snd_mixer_selem_id_set_index(sid, mix_index);
      snd_mixer_selem_id_set_name(sid, mix_name);

      if (mixer_channel) {
	free(mix_name);
	mix_name = NULL;
      }

      if ((err = snd_mixer_open(&handle, 0)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_MixerOpenError, snd_strerror(err));
	return CONTROL_ERROR;
      }

      if ((err = snd_mixer_attach(handle, card)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_MixerAttachError, 
	       card, snd_strerror(err));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
      }

      if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_MixerRegisterError, snd_strerror(err));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
      }
      err = snd_mixer_load(handle);
      if (err < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_MixerLoadError, snd_strerror(err));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
      }

      elem = snd_mixer_find_selem(handle, sid);
      if (!elem) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToFindSimpleControl,
	       snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
	}

      snd_mixer_selem_get_playback_volume_range(elem,&pmin,&pmax);
      f_multi = (100 / (float)(pmax - pmin));

      if (cmd == AOCONTROL_SET_VOLUME) {

	set_vol = vol->left / f_multi + pmin + 0.5;

	//setting channels
	if ((err = snd_mixer_selem_set_playback_volume(elem, 0, set_vol)) < 0) {
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_ErrorSettingLeftChannel, 
		 snd_strerror(err));
	  return CONTROL_ERROR;
	}
	mp_msg(MSGT_AO,MSGL_DBG2,"left=%li, ", set_vol);

	set_vol = vol->right / f_multi + pmin + 0.5;

	if ((err = snd_mixer_selem_set_playback_volume(elem, 1, set_vol)) < 0) {
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_ErrorSettingRightChannel, 
		 snd_strerror(err));
	  return CONTROL_ERROR;
	}
	mp_msg(MSGT_AO,MSGL_DBG2,"right=%li, pmin=%li, pmax=%li, mult=%f\n", 
	       set_vol, pmin, pmax, f_multi);

	if (snd_mixer_selem_has_playback_switch(elem)) {
	  int lmute = (vol->left == 0.0);
	  int rmute = (vol->right == 0.0);
	  if (snd_mixer_selem_has_playback_switch_joined(elem)) {
	    lmute = rmute = lmute && rmute;
	  } else {
	    snd_mixer_selem_set_playback_switch(elem, SND_MIXER_SCHN_FRONT_RIGHT, !rmute);
	  }
	  snd_mixer_selem_set_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, !lmute);
	}
      }
      else {
	snd_mixer_selem_get_playback_volume(elem, 0, &get_vol);
	vol->left = (get_vol - pmin) * f_multi;
	snd_mixer_selem_get_playback_volume(elem, 1, &get_vol);
	vol->right = (get_vol - pmin) * f_multi;

	mp_msg(MSGT_AO,MSGL_DBG2,"left=%f, right=%f\n",vol->left,vol->right);
      }
      snd_mixer_close(handle);
      return CONTROL_OK;
    }
    
  } //end switch
  return(CONTROL_UNKNOWN);
}

static void parse_device (char *dest, const char *src, int len)
{
  char *tmp;
  memmove(dest, src, len);
  dest[len] = 0;
  while ((tmp = strrchr(dest, '.')))
    tmp[0] = ',';
  while ((tmp = strrchr(dest, '=')))
    tmp[0] = ':';
}

static void print_help (void)
{
  mp_msg (MSGT_AO, MSGL_FATAL,
           MSGTR_AO_ALSA_CommandlineHelp);
}

static int str_maxlen(strarg_t *str) {
  if (str->len > ALSA_DEVICE_SIZE)
    return 0;
  return 1;
}

static int try_open_device(const char *device, int open_mode, int try_ac3)
{
  int err, len;
  char *ac3_device, *args;

  if (try_ac3) {
    /* to set the non-audio bit, use AES0=6 */
    len = strlen(device);
    ac3_device = malloc(len + 7 + 1);
    if (!ac3_device)
      return -ENOMEM;
    strcpy(ac3_device, device);
    args = strchr(ac3_device, ':');
    if (!args) {
      /* no existing parameters: add it behind device name */
      strcat(ac3_device, ":AES0=6");
    } else {
      do
	++args;
      while (isspace(*args));
      if (*args == '\0') {
	/* ":" but no parameters */
	strcat(ac3_device, "AES0=6");
      } else if (*args != '{') {
	/* a simple list of parameters: add it at the end of the list */
	strcat(ac3_device, ",AES0=6");
      } else {
	/* parameters in config syntax: add it inside the { } block */
	do
	  --len;
	while (len > 0 && isspace(ac3_device[len]));
	if (ac3_device[len] == '}')
	  strcpy(ac3_device + len, " AES0=6}");
      }
    }
    err = snd_pcm_open(&alsa_handler, ac3_device, SND_PCM_STREAM_PLAYBACK,
		       open_mode);
    free(ac3_device);
  }
  if (!try_ac3 || err < 0)
    err = snd_pcm_open(&alsa_handler, device, SND_PCM_STREAM_PLAYBACK,
		       open_mode);
  return err;
}

/*
    open & setup audio device
    return: 1=success 0=fail
*/
static int init(int rate_hz, int channels, int format, int flags)
{
    int err;
    int block;
    strarg_t device;
    snd_pcm_uframes_t bufsize;
    snd_pcm_uframes_t boundary;
    opt_t subopts[] = {
      {"block", OPT_ARG_BOOL, &block, NULL},
      {"device", OPT_ARG_STR, &device, (opt_test_f)str_maxlen},
      {NULL}
    };

    char alsa_device[ALSA_DEVICE_SIZE + 1];
    // make sure alsa_device is null-terminated even when using strncpy etc.
    memset(alsa_device, 0, ALSA_DEVICE_SIZE + 1);

    mp_msg(MSGT_AO,MSGL_V,"alsa-init: requested format: %d Hz, %d channels, %x\n", rate_hz,
	channels, format);
    alsa_handler = NULL;
#if SND_LIB_VERSION >= 0x010005
    mp_msg(MSGT_AO,MSGL_V,"alsa-init: using ALSA %s\n", snd_asoundlib_version());
#else
    mp_msg(MSGT_AO,MSGL_V,"alsa-init: compiled for ALSA-%s\n", SND_LIB_VERSION_STR);
#endif

    snd_lib_error_set_handler(alsa_error_handler);
    
    ao_data.samplerate = rate_hz;
    ao_data.format = format;
    ao_data.channels = channels;

    switch (format)
      {
      case AF_FORMAT_S8:
	alsa_format = SND_PCM_FORMAT_S8;
	break;
      case AF_FORMAT_U8:
	alsa_format = SND_PCM_FORMAT_U8;
	break;
      case AF_FORMAT_U16_LE:
	alsa_format = SND_PCM_FORMAT_U16_LE;
	break;
      case AF_FORMAT_U16_BE:
	alsa_format = SND_PCM_FORMAT_U16_BE;
	break;
#ifndef WORDS_BIGENDIAN
      case AF_FORMAT_AC3:
#endif
      case AF_FORMAT_S16_LE:
	alsa_format = SND_PCM_FORMAT_S16_LE;
	break;
#ifdef WORDS_BIGENDIAN
      case AF_FORMAT_AC3:
#endif
      case AF_FORMAT_S16_BE:
	alsa_format = SND_PCM_FORMAT_S16_BE;
	break;
      case AF_FORMAT_U32_LE:
	alsa_format = SND_PCM_FORMAT_U32_LE;
	break;
      case AF_FORMAT_U32_BE:
	alsa_format = SND_PCM_FORMAT_U32_BE;
	break;
      case AF_FORMAT_S32_LE:
	alsa_format = SND_PCM_FORMAT_S32_LE;
	break;
      case AF_FORMAT_S32_BE:
	alsa_format = SND_PCM_FORMAT_S32_BE;
	break;
      case AF_FORMAT_FLOAT_LE:
	alsa_format = SND_PCM_FORMAT_FLOAT_LE;
	break;
      case AF_FORMAT_FLOAT_BE:
	alsa_format = SND_PCM_FORMAT_FLOAT_BE;
	break;
      case AF_FORMAT_MU_LAW:
	alsa_format = SND_PCM_FORMAT_MU_LAW;
	break;
      case AF_FORMAT_A_LAW:
	alsa_format = SND_PCM_FORMAT_A_LAW;
	break;

      default:
	alsa_format = SND_PCM_FORMAT_MPEG; //? default should be -1
	break;
      }
    
    //subdevice parsing
    // set defaults
    block = 1;
    /* switch for spdif
     * sets opening sequence for SPDIF
     * sets also the playback and other switches 'on the fly'
     * while opening the abstract alias for the spdif subdevice
     * 'iec958'
     */
    if (format == AF_FORMAT_AC3) {
	device.str = "iec958";
	mp_msg(MSGT_AO,MSGL_V,"alsa-spdif-init: playing AC3, %i channels\n", channels);
    }
  else
        /* in any case for multichannel playback we should select
         * appropriate device
         */
        switch (channels) {
	case 1:
	case 2:
	  device.str = "default";
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: setup for 1/2 channel(s)\n");
	  break;
	case 4:
	  if (alsa_format == SND_PCM_FORMAT_FLOAT_LE)
	    // hack - use the converter plugin
	    device.str = "plug:surround40";
	  else
	    device.str = "surround40";
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: device set to surround40\n");
	  break;
	case 6:
	  if (alsa_format == SND_PCM_FORMAT_FLOAT_LE)
	    device.str = "plug:surround51";
	  else
	    device.str = "surround51";
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: device set to surround51\n");
	  break;
	default:
	  device.str = "default";
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_ChannelsNotSupported,channels);
        }
    device.len = strlen(device.str);
    if (subopt_parse(ao_subdevice, subopts) != 0) {
        print_help();
        return 0;
    }
    ao_noblock = !block;
    parse_device(alsa_device, device.str, device.len);

    mp_msg(MSGT_AO,MSGL_V,"alsa-init: using device %s\n", alsa_device);

    //setting modes for block or nonblock-mode
    if (ao_noblock) {
      open_mode = SND_PCM_NONBLOCK;
    }
    else {
      open_mode = 0;
    }

    //sets buff/chunksize if its set manually
    if (ao_data.buffersize) {
      switch (ao_data.buffersize)
	{
	case 1:
	  alsa_fragcount = 16;
	  chunk_size = 512;
	    mp_msg(MSGT_AO,MSGL_V,"alsa-init: buffersize set manually to 8192\n");
	    mp_msg(MSGT_AO,MSGL_V,"alsa-init: chunksize set manually to 512\n");
	  break;
	case 2:
	  alsa_fragcount = 8;
	  chunk_size = 1024;
	    mp_msg(MSGT_AO,MSGL_V,"alsa-init: buffersize set manually to 8192\n");
	    mp_msg(MSGT_AO,MSGL_V,"alsa-init: chunksize set manually to 1024\n");
	  break;
	case 3:
	  alsa_fragcount = 32;
	  chunk_size = 512;
	    mp_msg(MSGT_AO,MSGL_V,"alsa-init: buffersize set manually to 16384\n");
	    mp_msg(MSGT_AO,MSGL_V,"alsa-init: chunksize set manually to 512\n");
	  break;
	case 4:
	  alsa_fragcount = 16;
	  chunk_size = 1024;
	    mp_msg(MSGT_AO,MSGL_V,"alsa-init: buffersize set manually to 16384\n");
	    mp_msg(MSGT_AO,MSGL_V,"alsa-init: chunksize set manually to 1024\n");
	  break;
	default:
	  alsa_fragcount = 16;
	  chunk_size = 1024;
	  break;
	}
    }

    if (!alsa_handler) {
      //modes = 0, SND_PCM_NONBLOCK, SND_PCM_ASYNC
      if ((err = try_open_device(alsa_device, open_mode, format == AF_FORMAT_AC3)) < 0)
	{
	  if (err != -EBUSY && ao_noblock) {
	    mp_msg(MSGT_AO,MSGL_INFO,MSGTR_AO_ALSA_OpenInNonblockModeFailed);
	    if ((err = try_open_device(alsa_device, 0, format == AF_FORMAT_AC3)) < 0) {
	      mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PlaybackOpenError, snd_strerror(err));
	      return(0);
	    }
	  } else {
	    mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PlaybackOpenError, snd_strerror(err));
	    return(0);
	  }
	}

      if ((err = snd_pcm_nonblock(alsa_handler, 0)) < 0) {
         mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_ErrorSetBlockMode, snd_strerror(err));
      } else {
	mp_msg(MSGT_AO,MSGL_V,"alsa-init: pcm opened in blocking mode\n");
      }

      snd_pcm_hw_params_alloca(&alsa_hwparams);
      snd_pcm_sw_params_alloca(&alsa_swparams);

      // setting hw-parameters
      if ((err = snd_pcm_hw_params_any(alsa_handler, alsa_hwparams)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToGetInitialParameters,
		 snd_strerror(err));
	  return(0);
	}
    
      err = snd_pcm_hw_params_set_access(alsa_handler, alsa_hwparams,
					 SND_PCM_ACCESS_RW_INTERLEAVED);
      if (err < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetAccessType, 
	       snd_strerror(err));
	return (0);
      }

      /* workaround for nonsupported formats
	 sets default format to S16_LE if the given formats aren't supported */
      if ((err = snd_pcm_hw_params_test_format(alsa_handler, alsa_hwparams,
                                             alsa_format)) < 0)
      {
         mp_msg(MSGT_AO,MSGL_INFO,
		MSGTR_AO_ALSA_FormatNotSupportedByHardware, af_fmt2str_short(format));
         alsa_format = SND_PCM_FORMAT_S16_LE;
         ao_data.format = AF_FORMAT_S16_LE;
      }

      if ((err = snd_pcm_hw_params_set_format(alsa_handler, alsa_hwparams,
					      alsa_format)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetFormat,
		 snd_strerror(err));
	  return(0);
	}

      if ((err = snd_pcm_hw_params_set_channels_near(alsa_handler, alsa_hwparams,
						     &ao_data.channels)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetChannels,
		 snd_strerror(err));
	  return(0);
	}

      /* workaround for buggy rate plugin (should be fixed in ALSA 1.0.11)
         prefer our own resampler */
#if SND_LIB_VERSION >= 0x010009
      if ((err = snd_pcm_hw_params_set_rate_resample(alsa_handler, alsa_hwparams,
						     0)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToDisableResampling,
		 snd_strerror(err));
	  return(0);
	}
#endif

      if ((err = snd_pcm_hw_params_set_rate_near(alsa_handler, alsa_hwparams, 
						 &ao_data.samplerate, NULL)) < 0) 
        {
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetSamplerate2,
		 snd_strerror(err));
	  return(0);
        }

      bytes_per_sample = snd_pcm_format_physical_width(alsa_format) / 8;
      bytes_per_sample *= ao_data.channels;
      ao_data.bps = ao_data.samplerate * bytes_per_sample;

#ifdef BUFFERTIME
      {
	int alsa_buffer_time = 500000; /* original 60 */
	int alsa_period_time;
	alsa_period_time = alsa_buffer_time/4;
	if ((err = snd_pcm_hw_params_set_buffer_time_near(alsa_handler, alsa_hwparams, 
							  &alsa_buffer_time, NULL)) < 0)
	  {
	    mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetBufferTimeNear,
		   snd_strerror(err));
	    return(0);
	  } else
	    alsa_buffer_time = err;

	if ((err = snd_pcm_hw_params_set_period_time_near(alsa_handler, alsa_hwparams, 
							  &alsa_period_time, NULL)) < 0)
	  /* original: alsa_buffer_time/ao_data.bps */
	  {
	    mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetPeriodTime,
		   snd_strerror(err));
	    return 0;
	  }
	mp_msg(MSGT_AO,MSGL_INFO,MSGTR_AO_ALSA_BufferTimePeriodTime,
	       alsa_buffer_time, err);
      } 
#endif//end SET_BUFFERTIME

#ifdef SET_CHUNKSIZE
      {
	//set chunksize
	if ((err = snd_pcm_hw_params_set_period_size_near(alsa_handler, alsa_hwparams, 
							  &chunk_size, NULL)) < 0)
	  {
	    mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetPeriodSize,
			    chunk_size, snd_strerror(err));
	    return 0;
	  }
	else {
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: chunksize set to %li\n", chunk_size);
	}
	if ((err = snd_pcm_hw_params_set_periods_near(alsa_handler, alsa_hwparams,
						      &alsa_fragcount, NULL)) < 0) {
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetPeriods, 
		 snd_strerror(err));
	  return 0;
	}
	else {
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: fragcount=%i\n", alsa_fragcount);
	}
      }
#endif//end SET_CHUNKSIZE

      /* finally install hardware parameters */
      if ((err = snd_pcm_hw_params(alsa_handler, alsa_hwparams)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetHwParameters,
		 snd_strerror(err));
	  return 0;
	}
      // end setting hw-params


      // gets buffersize for control
      if ((err = snd_pcm_hw_params_get_buffer_size(alsa_hwparams, &bufsize)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToGetBufferSize, snd_strerror(err));
	  return 0;
	}
      else {
	ao_data.buffersize = bufsize * bytes_per_sample;
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: got buffersize=%i\n", ao_data.buffersize);
      }

      if ((err = snd_pcm_hw_params_get_period_size(alsa_hwparams, &chunk_size, NULL)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToGetPeriodSize, snd_strerror(err));
	return 0;
      } else {
	mp_msg(MSGT_AO,MSGL_V,"alsa-init: got period size %li\n", chunk_size);
      }
      ao_data.outburst = chunk_size * bytes_per_sample;

      /* setting software parameters */
      if ((err = snd_pcm_sw_params_current(alsa_handler, alsa_swparams)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToGetSwParameters,
	       snd_strerror(err));
	return 0;
      }
#if SND_LIB_VERSION >= 0x000901
      if ((err = snd_pcm_sw_params_get_boundary(alsa_swparams, &boundary)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToGetBoundary,
	       snd_strerror(err));
	return 0;
      }
#else
      boundary = 0x7fffffff;
#endif
      /* start playing when one period has been written */
      if ((err = snd_pcm_sw_params_set_start_threshold(alsa_handler, alsa_swparams, chunk_size)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetStartThreshold,
	       snd_strerror(err));
	return 0;
      }
      /* disable underrun reporting */
      if ((err = snd_pcm_sw_params_set_stop_threshold(alsa_handler, alsa_swparams, boundary)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetStopThreshold,
	       snd_strerror(err));
	return 0;
      }
#if SND_LIB_VERSION >= 0x000901
      /* play silence when there is an underrun */
      if ((err = snd_pcm_sw_params_set_silence_size(alsa_handler, alsa_swparams, boundary)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToSetSilenceSize,
	       snd_strerror(err));
	return 0;
      }
#endif
      if ((err = snd_pcm_sw_params(alsa_handler, alsa_swparams)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_UnableToGetSwParameters,
	       snd_strerror(err));
	return 0;
      }
      /* end setting sw-params */

      mp_msg(MSGT_AO,MSGL_V,"alsa: %d Hz/%d channels/%d bpf/%d bytes buffer/%s\n",
	     ao_data.samplerate, ao_data.channels, bytes_per_sample, ao_data.buffersize,
	     snd_pcm_format_description(alsa_format));

    } // end switch alsa_handler (spdif)
    alsa_can_pause = snd_pcm_hw_params_can_pause(alsa_hwparams);
    return(1);
} // end init


/* close audio device */
static void uninit(int immed)
{

  if (alsa_handler) {
    int err;

    if (!immed)
      snd_pcm_drain(alsa_handler);

    if ((err = snd_pcm_close(alsa_handler)) < 0)
      {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PcmCloseError, snd_strerror(err));
	return;
      }
    else {
      alsa_handler = NULL;
      mp_msg(MSGT_AO,MSGL_V,"alsa-uninit: pcm closed\n");
    }
  }
  else {
    mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_NoHandlerDefined);
  }
}

static void audio_pause(void)
{
    int err;

    if (alsa_can_pause) {
        if ((err = snd_pcm_pause(alsa_handler, 1)) < 0)
        {
            mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PcmPauseError, snd_strerror(err));
            return;
        }
          mp_msg(MSGT_AO,MSGL_V,"alsa-pause: pause supported by hardware\n");
    } else {
        if ((err = snd_pcm_drop(alsa_handler)) < 0)
        {
            mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PcmDropError, snd_strerror(err));
            return;
        }
    }
}

static void audio_resume(void)
{
    int err;

    if (alsa_can_pause) {
        if ((err = snd_pcm_pause(alsa_handler, 0)) < 0)
        {
            mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PcmResumeError, snd_strerror(err));
            return;
        }
          mp_msg(MSGT_AO,MSGL_V,"alsa-resume: resume supported by hardware\n");
    } else {
        if ((err = snd_pcm_prepare(alsa_handler)) < 0)
        {
           mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PcmPrepareError, snd_strerror(err));
            return;
        }
    }
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset(void)
{
    int err;

    if ((err = snd_pcm_drop(alsa_handler)) < 0)
    {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PcmPrepareError, snd_strerror(err));
	return;
    }
    if ((err = snd_pcm_prepare(alsa_handler)) < 0)
    {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PcmPrepareError, snd_strerror(err));
	return;
    }
    return;
}

/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
    modified last at 29.06.02 by jp
    thanxs for marius <marius@rospot.com> for giving us the light ;)
*/

static int play(void* data, int len, int flags)
{
  int num_frames = len / bytes_per_sample;
  snd_pcm_sframes_t res = 0;

  //mp_msg(MSGT_AO,MSGL_ERR,"alsa-play: frames=%i, len=%i\n",num_frames,len);

  if (!alsa_handler) {
    mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_DeviceConfigurationError);
    return 0;
  }

  if (num_frames == 0)
    return 0;

  do {
    res = snd_pcm_writei(alsa_handler, data, num_frames);

      if (res == -EINTR) {
	/* nothing to do */
	res = 0;
      }
      else if (res == -ESTRPIPE) {	/* suspend */
	mp_msg(MSGT_AO,MSGL_INFO,MSGTR_AO_ALSA_PcmInSuspendModeTryingResume);
	while ((res = snd_pcm_resume(alsa_handler)) == -EAGAIN)
	  sleep(1);
      }
      if (res < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_WriteError, snd_strerror(res));
	mp_msg(MSGT_AO,MSGL_INFO,MSGTR_AO_ALSA_TryingToResetSoundcard);
	if ((res = snd_pcm_prepare(alsa_handler)) < 0) {
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_PcmPrepareError, snd_strerror(res));
	  return(0);
	  break;
	}
      }
  } while (res == 0);

  return res < 0 ? res : res * bytes_per_sample;
}

/* how many byes are free in the buffer */
static int get_space(void)
{
    snd_pcm_status_t *status;
    int ret;
    
    snd_pcm_status_alloca(&status);
    
    if ((ret = snd_pcm_status(alsa_handler, status)) < 0)
    {
	mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_ALSA_CannotGetPcmStatus, snd_strerror(ret));
	return(0);
    }
    
    ret = snd_pcm_status_get_avail(status) * bytes_per_sample;
    if (ret > MAX_OUTBURST)
	    ret = MAX_OUTBURST;
    return(ret);
}

/* delay in seconds between first and last sample in buffer */
static float get_delay(void)
{
  if (alsa_handler) {
    snd_pcm_sframes_t delay;
    
    if (snd_pcm_delay(alsa_handler, &delay) < 0)
      return 0;
    
    if (delay < 0) {
      /* underrun - move the application pointer forward to catch up */
#if SND_LIB_VERSION >= 0x000901 /* snd_pcm_forward() exists since 0.9.0rc8 */
      snd_pcm_forward(alsa_handler, -delay);
#endif
      delay = 0;
    }
    return (float)delay / (float)ao_data.samplerate;
  } else {
    return(0);
  }
}
