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
#include <math.h>
#include <string.h>
#include <sys/poll.h>

#include "config.h"
#include "mixer.h"
#include "mp_msg.h"

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
#include "afmt.h"

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

/* possible 4096, original 8192 
 * was only needed for calculating chunksize? */
static int alsa_fragsize = 4096;
/* 16 sets buffersize to 16 * chunksize is as default 1024
 * which seems to be good avarge for most situations 
 * so buffersize is 16384 frames by default */
static int alsa_fragcount = 16;
static snd_pcm_uframes_t chunk_size = 1024;//is alsa_fragsize / 4

#define MIN_CHUNK_SIZE 1024

static size_t bits_per_sample, bytes_per_sample, bits_per_frame;
static size_t chunk_bytes;

int ao_mmap = 0;
int ao_noblock = 0;
int first = 1;

static int open_mode;
static int set_block_mode;
static int alsa_can_pause = 0;

#define ALSA_DEVICE_SIZE 256

#undef BUFFERTIME
#define SET_CHUNKSIZE
#undef USE_POLL

/* to set/get/query special features/parameters */
static int control(int cmd, void *arg)
{
  switch(cmd) {
  case AOCONTROL_QUERY_FORMAT:
    return CONTROL_TRUE;
#ifndef WORDS_BIGENDIAN 
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
	 if (test_mix_index = strchr(mix_name, ',')){
		*test_mix_index = 0;
		test_mix_index++;
		mix_index = strtol(test_mix_index, &test_mix_index, 0);

		if (*test_mix_index){
		  mp_msg(MSGT_AO,MSGL_ERR,
		    "alsa-control: invalid mixer index. Defaulting to 0\n");
		  mix_index = 0 ;
		}
	 }
      }
      if(mixer_device) card = mixer_device;

      if(ao_data.format == AFMT_AC3)
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
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-control: mixer open error: %s\n", snd_strerror(err));
	return CONTROL_ERROR;
      }

      if ((err = snd_mixer_attach(handle, card)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-control: mixer attach %s error: %s\n", 
	       card, snd_strerror(err));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
      }

      if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-control: mixer register error: %s\n", snd_strerror(err));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
      }
      err = snd_mixer_load(handle);
      if (err < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-control: mixer load error: %s\n", snd_strerror(err));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
      }

      elem = snd_mixer_find_selem(handle, sid);
      if (!elem) {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-control: unable to find simple control '%s',%i\n",
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
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-control: error setting left channel, %s\n", 
		 snd_strerror(err));
	  return CONTROL_ERROR;
	}
	mp_msg(MSGT_AO,MSGL_DBG2,"left=%li, ", set_vol);

	set_vol = vol->right / f_multi + pmin + 0.5;

	if ((err = snd_mixer_selem_set_playback_volume(elem, 1, set_vol)) < 0) {
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-control: error setting right channel, %s\n", 
		 snd_strerror(err));
	  return CONTROL_ERROR;
	}
	mp_msg(MSGT_AO,MSGL_DBG2,"right=%li, pmin=%li, pmax=%li, mult=%f\n", 
	       set_vol, pmin, pmax, f_multi);
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
#endif
    
  } //end switch
  return(CONTROL_UNKNOWN);
}

static void parse_device (char *dest, char *src, int len)
{
  char *tmp;
  strncpy (dest, src, len);
  while ((tmp = strrchr(dest, '.')))
    tmp[0] = ',';
  while ((tmp = strrchr(dest, '=')))
    tmp[0] = ':';
}

static void print_help ()
{
  mp_msg (MSGT_AO, MSGL_FATAL,
           "\n-ao alsa commandline help:\n"
           "Example: mplayer -ao alsa:mmap:device=hw=0.3\n"
           "  sets mmap-mode and first card fourth device\n"
           "\nOptions:\n"
           "  mmap\n"
           "    Set memory-mapped mode, experimental\n"
           "  noblock\n"
           "    Sets non-blocking mode\n"
           "  device=<device-name>\n"
           "    Sets device (change , to . and : to =)\n");
}

/*
    open & setup audio device
    return: 1=success 0=fail
*/
static int init(int rate_hz, int channels, int format, int flags)
{
    int err;
    int cards = -1;
    snd_pcm_info_t *alsa_info;
    char *str_block_mode;
    int device_set = 0;
    int dir = 0;
    snd_pcm_uframes_t bufsize;
    char alsa_device[ALSA_DEVICE_SIZE + 1];
    // make sure alsa_device is null-terminated even when using strncpy etc.
    memset(alsa_device, 0, ALSA_DEVICE_SIZE + 1);

    mp_msg(MSGT_AO,MSGL_V,"alsa-init: requested format: %d Hz, %d channels, %s\n", rate_hz,
	channels, audio_out_format_name(format));
    alsa_handler = NULL;
    mp_msg(MSGT_AO,MSGL_V,"alsa-init: compiled for ALSA-%s\n", SND_LIB_VERSION_STR);
    
    if ((err = snd_card_next(&cards)) < 0 || cards < 0)
    {
      mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: no soundcards found: %s\n", snd_strerror(err));
      return(0);
    }

    ao_data.samplerate = rate_hz;
    ao_data.bps = channels * rate_hz;
    ao_data.format = format;
    ao_data.channels = channels;
    ao_data.outburst = OUTBURST;

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
      case AFMT_S32_LE:
	alsa_format = SND_PCM_FORMAT_S32_LE;
	break;
      case AFMT_S32_BE:
	alsa_format = SND_PCM_FORMAT_S32_BE;
	break;
      case AFMT_FLOAT:
	alsa_format = SND_PCM_FORMAT_FLOAT_LE;
	break;

      default:
	alsa_format = SND_PCM_FORMAT_MPEG; //? default should be -1
	break;
      }
    
    //setting bw according to the input-format. resolution seems to be always s16_le or
    //u16_le so 32bit is probably obsolet. 
    switch(alsa_format)
      {
      case SND_PCM_FORMAT_S8:
      case SND_PCM_FORMAT_U8:
	ao_data.bps *= 1;
	break;
      case SND_PCM_FORMAT_S16_LE:
      case SND_PCM_FORMAT_U16_LE:
      case SND_PCM_FORMAT_S16_BE:
      case SND_PCM_FORMAT_U16_BE:
	ao_data.bps *= 2;
	break;
      case SND_PCM_FORMAT_S32_LE:
      case SND_PCM_FORMAT_S32_BE:
      case SND_PCM_FORMAT_FLOAT_LE:
	ao_data.bps *= 4;
	break;
      case -1:
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: invalid format (%s) requested - output disabled\n",
	       audio_out_format_name(format));
	return(0);
	break;
      default:
	ao_data.bps *= 2;
	mp_msg(MSGT_AO,MSGL_WARN,"alsa-init: couldn't convert to right format. setting bps to: %d", ao_data.bps);
      }

    //subdevice parsing
    if (ao_subdevice) {
      int parse_err = 0;
      char *parse_pos = &ao_subdevice[0];
      while (parse_pos[0] && !parse_err) {
        if (strncmp (parse_pos, "mmap", 4) == 0) {
          parse_pos = &parse_pos[4];
          ao_mmap = 1;
        } else if (strncmp (parse_pos, "noblock", 7) == 0) {
          parse_pos = &parse_pos[7];
          ao_noblock = 1;
        } else if (strncmp (parse_pos, "device=", 7) == 0) {
          int name_len;
          parse_pos = &parse_pos[7];
          name_len = strcspn (parse_pos, ":");
          if (name_len < 0 || name_len > ALSA_DEVICE_SIZE) {
            parse_err = 1;
            break;
          }
          parse_device (alsa_device, parse_pos, name_len);
          parse_pos = &parse_pos[name_len];
          device_set = 1;
        }
        if (parse_pos[0] == ':') parse_pos = &parse_pos[1];
        else if (parse_pos[0]) parse_err = 1;
      }
      if (parse_err) {
        print_help();
        return 0;
      }
    } else { //end parsing ao_subdevice

        /* in any case for multichannel playback we should select
         * appropriate device
         */
        switch (channels) {
	case 1:
	case 2:
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: setup for 1/2 channel(s)\n");
	  break;
	case 4:
	  if (alsa_format == SND_PCM_FORMAT_FLOAT_LE)
	    // hack - use the converter plugin
	    strncpy(alsa_device, "plug:surround40", ALSA_DEVICE_SIZE);
	  else
	    strncpy(alsa_device, "surround40", ALSA_DEVICE_SIZE);
	  device_set = 1;
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: device set to surround40\n");
	  break;
	case 6:
	  if (alsa_format == SND_PCM_FORMAT_FLOAT_LE)
	    strncpy(alsa_device, "plug:surround51", ALSA_DEVICE_SIZE);
	  else
	    strncpy(alsa_device, "surround51", ALSA_DEVICE_SIZE);
	  device_set = 1;
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: device set to surround51\n");
	  break;
	default:
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: %d channels are not supported\n",channels);
        }
    }

  if (!device_set) {
    /* switch for spdif
     * sets opening sequence for SPDIF
     * sets also the playback and other switches 'on the fly'
     * while opening the abstract alias for the spdif subdevice
     * 'iec958'
     */
    if (format == AFMT_AC3) {
      unsigned char s[4];

      switch (channels) {
      case 1:
      case 2:

	s[0] = IEC958_AES0_NONAUDIO | 
	  IEC958_AES0_CON_EMPHASIS_NONE;
	s[1] = IEC958_AES1_CON_ORIGINAL | 
	  IEC958_AES1_CON_PCM_CODER;
	s[2] = 0;
	s[3] = IEC958_AES3_CON_FS_48000;

	snprintf(alsa_device, ALSA_DEVICE_SIZE,
		"iec958:AES0=0x%x,AES1=0x%x,AES2=0x%x,AES3=0x%x", 
 		s[0], s[1], s[2], s[3]);

	mp_msg(MSGT_AO,MSGL_V,"alsa-spdif-init: playing AC3, %i channels\n", channels);
	break;
      case 4:
	strncpy(alsa_device, "surround40", ALSA_DEVICE_SIZE);
	break;
    
      case 6:
	strncpy(alsa_device, "surround51", ALSA_DEVICE_SIZE);
	break;

      default:
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-spdif-init: %d channels are not supported\n", channels);
      }
    }
  else

      {
	int tmp_device, tmp_subdevice, err;

	snd_pcm_info_alloca(&alsa_info);
	
	if ((tmp_device = snd_pcm_info_get_device(alsa_info)) < 0)
	  {
	    mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: can't get device\n");
	  }

	if ((tmp_subdevice = snd_pcm_info_get_subdevice(alsa_info)) < 0)
	  {
	    mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: can't get subdevice\n");
	  }
	
	mp_msg(MSGT_AO,MSGL_INFO,"alsa-init: got device=%i, subdevice=%i\n", 
	       tmp_device, tmp_subdevice);

	//we are setting here device to default cause it could be configured by the user
	//if its not set by the user, it defaults to hw:0,0
	if ((err = snprintf(alsa_device, ALSA_DEVICE_SIZE, "default")) <= 0)
	  {
	    mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: can't write device-id\n");
	  }

	mp_msg(MSGT_AO,MSGL_INFO,"alsa-init: %d soundcard%s found, using: %s\n", cards+1,(cards >= 0) ? "" : "s", alsa_device);
      }
      } else {
		mp_msg(MSGT_AO,MSGL_INFO,"alsa-init: soundcard set to %s\n", alsa_device);
      }

    //setting modes for block or nonblock-mode
    if (ao_noblock) {
      open_mode = SND_PCM_NONBLOCK;
      set_block_mode = 1;
      str_block_mode = "nonblock-mode";
    }
    else {
      open_mode = 0;
      set_block_mode = 0;
      str_block_mode = "block-mode";
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
	  if (ao_mmap)
	    chunk_size = 512;
	  else
	    chunk_size = 1024;
	  break;
	}
    }

    if (!alsa_handler) {
      //modes = 0, SND_PCM_NONBLOCK, SND_PCM_ASYNC
      if ((err = snd_pcm_open(&alsa_handler, alsa_device, SND_PCM_STREAM_PLAYBACK, open_mode)) < 0)
	{
	  if (err != -EBUSY && ao_noblock) {
	    mp_msg(MSGT_AO,MSGL_INFO,"alsa-init: open in nonblock-mode failed, trying to open in block-mode\n");
	    if ((err = snd_pcm_open(&alsa_handler, alsa_device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
	      mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: playback open error: %s\n", snd_strerror(err));
	      return(0);
	    } else {
	      set_block_mode = 0;
	      str_block_mode = "block-mode";
	    }
	  } else {
	    mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: playback open error: %s\n", snd_strerror(err));
	    return(0);
	  }
	}

      if ((err = snd_pcm_nonblock(alsa_handler, set_block_mode)) < 0) {
         mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: error set block-mode %s\n", snd_strerror(err));
      } else {
	mp_msg(MSGT_AO,MSGL_V,"alsa-init: pcm opend in %s\n", str_block_mode);
      }

      snd_pcm_hw_params_alloca(&alsa_hwparams);
      snd_pcm_sw_params_alloca(&alsa_swparams);

      // setting hw-parameters
      if ((err = snd_pcm_hw_params_any(alsa_handler, alsa_hwparams)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to get initial parameters: %s\n",
		 snd_strerror(err));
	  return(0);
	}
    
      if (ao_mmap) {
	snd_pcm_access_mask_t *mask = alloca(snd_pcm_access_mask_sizeof());
	snd_pcm_access_mask_none(mask);
	snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
	snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
	snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
	err = snd_pcm_hw_params_set_access_mask(alsa_handler, alsa_hwparams, mask);
	mp_msg(MSGT_AO,MSGL_INFO,"alsa-init: mmap set\n");
      } else {
	err = snd_pcm_hw_params_set_access(alsa_handler, alsa_hwparams,
					   SND_PCM_ACCESS_RW_INTERLEAVED);
      }
      if (err < 0) {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set access type: %s\n", 
	       snd_strerror(err));
	return (0);
      }

      /* workaround for nonsupported formats
	 sets default format to S16_LE if the given formats aren't supported */
      if ((err = snd_pcm_hw_params_test_format(alsa_handler, alsa_hwparams,
                                             alsa_format)) < 0)
      {
         mp_msg(MSGT_AO,MSGL_INFO,
		"alsa-init: format %s are not supported by hardware, trying default\n", 
		audio_out_format_name(format));
         alsa_format = SND_PCM_FORMAT_S16_LE;
         ao_data.format = AFMT_S16_LE;
         ao_data.bps = channels * rate_hz * 2;
      }

      bytes_per_sample = ao_data.bps / ao_data.samplerate; //it should be here

    
      if ((err = snd_pcm_hw_params_set_format(alsa_handler, alsa_hwparams,
					      alsa_format)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set format: %s\n",
		 snd_strerror(err));
	}

      if ((err = snd_pcm_hw_params_set_channels(alsa_handler, alsa_hwparams,
						ao_data.channels)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set channels: %s\n",
		 snd_strerror(err));
	}

      if ((err = snd_pcm_hw_params_set_rate_near(alsa_handler, alsa_hwparams, 
						 &ao_data.samplerate, &dir)) < 0) 
        {
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set samplerate-2: %s\n",
		 snd_strerror(err));
	  return(0);
        }

#ifdef BUFFERTIME
      {
	int alsa_buffer_time = 500000; /* original 60 */
	int alsa_period_time;
	alsa_period_time = alsa_buffer_time/4;
	if ((err = snd_pcm_hw_params_set_buffer_time_near(alsa_handler, alsa_hwparams, 
							  &alsa_buffer_time, &dir)) < 0)
	  {
	    mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set buffer time near: %s\n",
		   snd_strerror(err));
	    return(0);
	  } else
	    alsa_buffer_time = err;

	if ((err = snd_pcm_hw_params_set_period_time_near(alsa_handler, alsa_hwparams, 
							  &alsa_period_time, &dir)) < 0)
	  /* original: alsa_buffer_time/ao_data.bps */
	  {
	    mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set period time: %s\n",
		   snd_strerror(err));
	  }
	mp_msg(MSGT_AO,MSGL_INFO,"alsa-init: buffer_time: %d, period_time :%d\n",
	       alsa_buffer_time, err);
      } 
#endif//end SET_BUFFERTIME

#ifdef SET_CHUNKSIZE
      {
	//set chunksize
	dir=0;
	if ((err = snd_pcm_hw_params_set_period_size_near(alsa_handler, alsa_hwparams, 
							  &chunk_size, &dir)) < 0)
	  {
	    mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set periodsize(%d): %s\n",
			    chunk_size, snd_strerror(err));
	  }
	else {
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: chunksize set to %i\n", chunk_size);
	}
	if ((err = snd_pcm_hw_params_set_periods_near(alsa_handler, alsa_hwparams,
						      &alsa_fragcount, &dir)) < 0) {
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set periods: %s\n", 
		 snd_strerror(err));
	}
	else {
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: fragcount=%i\n", alsa_fragcount);
	}
      }
#endif//end SET_CHUNKSIZE

      /* finally install hardware parameters */
      if ((err = snd_pcm_hw_params(alsa_handler, alsa_hwparams)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set hw-parameters: %s\n",
		 snd_strerror(err));
	}
      // end setting hw-params


      // gets buffersize for control
      if ((err = snd_pcm_hw_params_get_buffer_size(alsa_hwparams, &bufsize)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to get buffersize: %s\n", snd_strerror(err));
	}
      else {
	ao_data.buffersize = bufsize * bytes_per_sample;
	  mp_msg(MSGT_AO,MSGL_V,"alsa-init: got buffersize=%i\n", ao_data.buffersize);
      }

      // setting sw-params (only avail-min) if noblocking mode was choosed
      if (ao_noblock)
	{

	  if ((err = snd_pcm_sw_params_current(alsa_handler, alsa_swparams)) < 0)
	    {
	      mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to get parameters: %s\n",
		     snd_strerror(err));

	    }

	  //set min available frames to consider pcm ready (4)
	  //increased for nonblock-mode should be set dynamically later
	  if ((err = snd_pcm_sw_params_set_avail_min(alsa_handler, alsa_swparams, 4)) < 0)
	    {
	      mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to set avail_min %s\n",
		     snd_strerror(err));
	    }

	  if ((err = snd_pcm_sw_params(alsa_handler, alsa_swparams)) < 0)
	    {
	      mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: unable to install sw-params\n");
	    }

	  bits_per_sample = snd_pcm_format_physical_width(alsa_format);
	  bits_per_frame = bits_per_sample * channels;
	  chunk_bytes = chunk_size * bits_per_frame / 8;

	    mp_msg(MSGT_AO,MSGL_V,"alsa-init: bits per sample (bps)=%i, bits per frame (bpf)=%i, chunk_bytes=%i\n",bits_per_sample,bits_per_frame,chunk_bytes);}
	//end swparams

      if ((err = snd_pcm_prepare(alsa_handler)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-init: pcm prepare error: %s\n", snd_strerror(err));
	}

      mp_msg(MSGT_AO,MSGL_INFO,"alsa: %d Hz/%d channels/%d bpf/%d bytes buffer/%s\n",
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

    if (!ao_noblock) {
      if ((err = snd_pcm_drop(alsa_handler)) < 0)
	{
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-uninit: pcm drop error: %s\n", snd_strerror(err));
	  return;
	}
    }

    if ((err = snd_pcm_close(alsa_handler)) < 0)
      {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-uninit: pcm close error: %s\n", snd_strerror(err));
	return;
      }
    else {
      alsa_handler = NULL;
      mp_msg(MSGT_AO,MSGL_INFO,"alsa-uninit: pcm closed\n");
    }
  }
  else {
    mp_msg(MSGT_AO,MSGL_ERR,"alsa-uninit: no handler defined!\n");
  }
}

static void audio_pause()
{
    int err;

    if (alsa_can_pause) {
        if ((err = snd_pcm_pause(alsa_handler, 1)) < 0)
        {
            mp_msg(MSGT_AO,MSGL_ERR,"alsa-pause: pcm pause error: %s\n", snd_strerror(err));
            return;
        }
          mp_msg(MSGT_AO,MSGL_V,"alsa-pause: pause supported by hardware\n");
    } else {
        if ((err = snd_pcm_drop(alsa_handler)) < 0)
        {
            mp_msg(MSGT_AO,MSGL_ERR,"alsa-pause: pcm drop error: %s\n", snd_strerror(err));
            return;
        }
    }
}

static void audio_resume()
{
    int err;

    if (alsa_can_pause) {
        if ((err = snd_pcm_pause(alsa_handler, 0)) < 0)
        {
            mp_msg(MSGT_AO,MSGL_ERR,"alsa-resume: pcm resume error: %s\n", snd_strerror(err));
            return;
        }
          mp_msg(MSGT_AO,MSGL_V,"alsa-resume: resume supported by hardware\n");
    } else {
        if ((err = snd_pcm_prepare(alsa_handler)) < 0)
        {
           mp_msg(MSGT_AO,MSGL_ERR,"alsa-resume: pcm prepare error: %s\n", snd_strerror(err));
            return;
        }
    }
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset()
{
    int err;

    if ((err = snd_pcm_drop(alsa_handler)) < 0)
    {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-reset: pcm drop error: %s\n", snd_strerror(err));
	return;
    }
    if ((err = snd_pcm_prepare(alsa_handler)) < 0)
    {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-reset: pcm prepare error: %s\n", snd_strerror(err));
	return;
    }
    return;
}

#ifdef USE_POLL
static int wait_for_poll(snd_pcm_t *handle, struct pollfd *ufds, unsigned int count)
{
  unsigned short revents;

  while (1) {
    poll(ufds, count, -1);
    snd_pcm_poll_descriptors_revents(handle, ufds, count, &revents);
    if (revents & POLLERR)
      return -EIO;
    if (revents & POLLOUT)
      return 0;
  }
} 
#endif

#ifndef timersub
#define timersub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
  (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
  if ((result)->tv_usec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_usec += 1000000; \
	} \
} while (0)
#endif

/* I/O error handler */
static int xrun(u_char *str_mode)
{
  int err;
  snd_pcm_status_t *status;

  snd_pcm_status_alloca(&status);
  
  if ((err = snd_pcm_status(alsa_handler, status))<0) {
    mp_msg(MSGT_AO,MSGL_ERR,"status error: %s", snd_strerror(err));
    return(0);
  }

  if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
    struct timeval now, diff, tstamp;
    gettimeofday(&now, 0);
    snd_pcm_status_get_trigger_tstamp(status, &tstamp);
    timersub(&now, &tstamp, &diff);
    mp_msg(MSGT_AO,MSGL_INFO,"alsa-%s: xrun of at least %.3f msecs. resetting stream\n",
	   str_mode,
	   diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
  }

  if ((err = snd_pcm_prepare(alsa_handler))<0) {
    mp_msg(MSGT_AO,MSGL_ERR,"xrun: prepare error: %s", snd_strerror(err));
    return(0);
  }

  return(1); /* ok, data should be accepted again */
}

static int play_normal(void* data, int len);
static int play_mmap(void* data, int len);

static int play(void* data, int len, int flags)
{
  int result;
  if (ao_mmap)
    result = play_mmap(data, len);
  else
    result = play_normal(data, len);

  return result;
}

/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
    modified last at 29.06.02 by jp
    thanxs for marius <marius@rospot.com> for giving us the light ;)
*/

static int play_normal(void* data, int len)
{

  //bytes_per_sample is always 4 for 2 chn S16_LE
  int num_frames = len / bytes_per_sample;
  char *output_samples = (char *)data;
  snd_pcm_sframes_t res = 0;

  //mp_msg(MSGT_AO,MSGL_ERR,"alsa-play: frames=%i, len=%i\n",num_frames,len);

  if (!alsa_handler) {
    mp_msg(MSGT_AO,MSGL_ERR,"alsa-play: device configuration error");
    return 0;
  }

  while (num_frames > 0) {

    res = snd_pcm_writei(alsa_handler, (void *)output_samples, num_frames);

      if (res == -EAGAIN) {
	snd_pcm_wait(alsa_handler, 1000);
      }
      else if (res == -EPIPE) {  /* underrun */
	if (xrun("play") <= 0) {
	  mp_msg(MSGT_AO,MSGL_ERR,"alsa-play: xrun reset error");
	  return(0);
	}
      }
      else if (res == -ESTRPIPE) {	/* suspend */
	mp_msg(MSGT_AO,MSGL_INFO,"alsa-play: pcm in suspend mode. trying to resume\n");
	while ((res = snd_pcm_resume(alsa_handler)) == -EAGAIN)
	  sleep(1);
      }
      else if (res < 0) {
	mp_msg(MSGT_AO,MSGL_INFO,"alsa-play: unknown status, trying to reset soundcard\n");
	if ((res = snd_pcm_prepare(alsa_handler)) < 0) {
	   mp_msg(MSGT_AO,MSGL_ERR,"alsa-play: snd prepare error");
	  return(0);
	  break;
	}
      }

      if (res > 0) {

	/* output_samples += ao_data.channels * res; */
	output_samples += res * bytes_per_sample;

	num_frames -= res;
      }

  } //end while

  if (res < 0) {
    mp_msg(MSGT_AO,MSGL_ERR,"alsa-play: write error %s", snd_strerror(res));
    return 0;
  }
  return len - len % bytes_per_sample;
}

/* mmap-mode mainly based on descriptions by Joshua Haberman <joshua@haberman.com>
 * 'An overview of the ALSA API' http://people.debian.org/~joshua/x66.html
 * and some help by Paul Davis <pbd@op.net> */

static int play_mmap(void* data, int len)
{
  snd_pcm_sframes_t commitres, frames_available;
  snd_pcm_uframes_t frames_transmit, size, offset;
  const snd_pcm_channel_area_t *area;
  void *outbuffer;
  int result;

#ifdef USE_POLL //seems not really be needed
  struct pollfd *ufds;
  int count;

  count = snd_pcm_poll_descriptors_count (alsa_handler);
  ufds = malloc(sizeof(struct pollfd) * count);
  snd_pcm_poll_descriptors(alsa_handler, ufds, count);

  //first wait_for_poll
    if (err = (wait_for_poll(alsa_handler, ufds, count) < 0)) {
      if (snd_pcm_state(alsa_handler) == SND_PCM_STATE_XRUN || 
	  snd_pcm_state(alsa_handler) == SND_PCM_STATE_SUSPENDED) {
        xrun("play");
      }
    }
#endif

  outbuffer = alloca(ao_data.buffersize);

  //don't trust get_space() ;)
  frames_available = snd_pcm_avail_update(alsa_handler) * bytes_per_sample;
  if (frames_available < 0)
    xrun("play");

  if (frames_available < 4) {
    if (first) {
      first = 0;
      snd_pcm_start(alsa_handler);
    }
    else { //FIXME should break and return 0?
      snd_pcm_wait(alsa_handler, -1);
      first = 1;
    }
  }

  /* len is simply the available bufferspace got by get_space() 
   * but real avail_buffer in frames is ab/bytes_per_sample */
  size = len / bytes_per_sample;

  //mp_msg(MSGT_AO,MSGL_V,"len: %i size %i, f_avail %i, bps %i ...\n", len, size, frames_available, bytes_per_sample);

  frames_transmit = size;

  /* prepare areas and set sw-pointers
   * frames_transmit returns the real available buffer-size
   * sometimes != frames_available cause of ringbuffer 'emulation' */
  snd_pcm_mmap_begin(alsa_handler, &area, &offset, &frames_transmit);

  /* this is specific to interleaved streams (or non-interleaved
   * streams with only one channel) */
  outbuffer = ((char *) area->addr + (area->first + area->step * offset) / 8); //8

  //write data
  memcpy(outbuffer, data, (frames_transmit * bytes_per_sample));

  commitres = snd_pcm_mmap_commit(alsa_handler, offset, frames_transmit);

  if (commitres < 0 || commitres != frames_transmit) {
    if (snd_pcm_state(alsa_handler) == SND_PCM_STATE_XRUN || 
	snd_pcm_state(alsa_handler) == SND_PCM_STATE_SUSPENDED) {
      xrun("play");
    }
  }

  //mp_msg(MSGT_AO,MSGL_V,"mmap ft: %i, cres: %i\n", frames_transmit, commitres);

  /* 	err = snd_pcm_area_copy(&area, offset, &data, offset, len, alsa_format); */
  /* 	if (err < 0) { */
  /* 	  mp_msg(MSGT_AO,MSGL_ERR,"area-copy-error\n"); */
  /* 	  return 0; */
  /* 	} */


  //calculate written frames!
  result = commitres * bytes_per_sample;


  /* if (verbose) { */
  /* if (len == result) */
  /* mp_msg(MSGT_AO,MSGL_V,"result: %i, frames written: %i ...\n", result, frames_transmit); */
  /* else */
  /* mp_msg(MSGT_AO,MSGL_V,"result: %i, frames written: %i, result != len ...\n", result, frames_transmit); */
  /* } */

  //mplayer doesn't like -result
  if (result < 0)
    result = 0;

#ifdef USE_POLL
  free(ufds);
#endif

  return result;
}

/* how many byes are free in the buffer */
static int get_space()
{
    snd_pcm_status_t *status;
    int ret;
    char *str_status;

    //snd_pcm_sframes_t avail_frames = 0;
    
    snd_pcm_status_alloca(&status);
    
    if ((ret = snd_pcm_status(alsa_handler, status)) < 0)
    {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-space: cannot get pcm status: %s\n", snd_strerror(ret));
	return(0);
    }
    
    switch(snd_pcm_status_get_state(status))
    {
    case SND_PCM_STATE_OPEN:
      str_status = "open";
      ret = snd_pcm_status_get_avail(status) * bytes_per_sample;
      break;
    case SND_PCM_STATE_PREPARED:
	str_status = "prepared";
	first = 1;
	ret = snd_pcm_status_get_avail(status) * bytes_per_sample;
	if (ret == 0) //ugly workaround for hang in mmap-mode
	  ret = 10;
	break;
    case SND_PCM_STATE_RUNNING:
      ret = snd_pcm_status_get_avail(status) * bytes_per_sample;
      //avail_frames = snd_pcm_avail_update(alsa_handler) * bytes_per_sample;
      if (str_status != "open" && str_status != "prepared")
	str_status = "running";
      break;
    case SND_PCM_STATE_PAUSED:
      mp_msg(MSGT_AO,MSGL_V,"alsa-space: paused");
      str_status = "paused";
      ret = 0;
      break;
    case SND_PCM_STATE_XRUN:
      xrun("space");
      str_status = "xrun";
      first = 1;
      ret = 0;
      break;
    default:
      str_status = "undefined";
      ret = snd_pcm_status_get_avail(status) * bytes_per_sample;
      if (ret <= 0) {
	xrun("space");
      }
    }

    if (snd_pcm_status_get_state(status) != SND_PCM_STATE_RUNNING)
      mp_msg(MSGT_AO,MSGL_V,"alsa-space: free space = %i, %s --\n", ret, str_status);
    
    if (ret < 0) {
      mp_msg(MSGT_AO,MSGL_ERR,"negative value!!\n");
      ret = 0;
    }
 
    // workaround for too small value returned
    if (ret < MIN_CHUNK_SIZE)
      ret = 0;

    return(ret);
}

/* delay in seconds between first and last sample in buffer */
static float get_delay()
{

  if (alsa_handler) {

    snd_pcm_status_t *status;
    float ret;
    
    snd_pcm_status_alloca(&status);
    
    if ((ret = snd_pcm_status(alsa_handler, status)) < 0)
    {
	mp_msg(MSGT_AO,MSGL_ERR,"alsa-delay: cannot get pcm status: %s\n", snd_strerror(ret));
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
    

    if (ret < 0)
      ret = 0;
    return(ret);
    
  } else {
    return(0);
  }
}
