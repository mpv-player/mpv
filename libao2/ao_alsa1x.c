/*
  ao_alsa9 - ALSA-0.9.x output plugin for MPlayer

  (C) Alex Beregszaszi <alex@naxine.org>
  
  modified for better alsa-0.9.0beta12(rc1)-support by Joy Winter <joy@pingfm.org>
  additional AC3 passthrough support by Andy Lo A Foe <andy@alsaplayer.org>
  
  Any bugreports regarding to this driver are welcome either to the mplayer-user-mailinglist or directly to the authors.
*/

#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/poll.h>

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

/* possible 4096, original 8192 
 * was only needed for calculating chunksize? */
static int alsa_fragsize = 4096;
/* 16 sets buffersize to 16 * chunksize is as default 1024
 * which seems to be good avarge for most situations 
 * so buffersize is 16384 frames by default */
static int alsa_fragcount = 16;
static int chunk_size = 1024; //is alsa_fragsize / 4

static size_t bits_per_sample, bits_per_frame;
static size_t chunk_bytes;

int ao_mmap = 0;
int ao_noblock = 0;
int first = 1;

static int open_mode;
static int set_block_mode;

#define ALSA_DEVICE_SIZE 48

#undef BUFFERTIME
#define SET_CHUNKSIZE
#undef USE_POLL

snd_pcm_t *spdif_init(char *pcm_name)
{
	//char *pcm_name = "hw:0,2"; /* first card second device */
	static snd_aes_iec958_t spdif;
	static snd_aes_iec958_t spdif_test;
	snd_pcm_info_t 	*info;
	snd_pcm_t *handler;
	snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
	unsigned int channels = 2;
	unsigned int rate = 48000;
	int err, c;

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
	snd_ctl_elem_id_t *elem_id;
	unsigned int elem_device;
	const char *elem_name;
        char ctl_name[12];
        int ctl_card;
	//elem_id = 36;

	spdif.status[0] = IEC958_AES0_NONAUDIO |
			  IEC958_AES0_CON_EMPHASIS_NONE;
	spdif.status[1] = IEC958_AES1_CON_ORIGINAL |
			  IEC958_AES1_CON_PCM_CODER;
	spdif.status[2] = 0;
	spdif.status[3] = IEC958_AES3_CON_FS_48000;

        snd_ctl_elem_value_alloca(&ctl);
	snd_ctl_elem_id_alloca(&elem_id); 
	snd_ctl_elem_value_set_interface(ctl, SND_CTL_ELEM_IFACE_PCM); //SND_CTL_ELEM_IFACE_MIXER
        snd_ctl_elem_value_set_device(ctl, snd_pcm_info_get_device(info));
        snd_ctl_elem_value_set_subdevice(ctl, snd_pcm_info_get_subdevice(info));
        snd_ctl_elem_value_set_name(ctl,SND_CTL_NAME_IEC958("", PLAYBACK, PCM_STREAM)); //SWITCH
	snd_ctl_elem_value_set_iec958(ctl, &spdif);
        ctl_card = snd_pcm_info_get_card(info);
        if (ctl_card < 0) {
           fprintf(stderr, "Unable to setup the IEC958 (S/PDIF) interface - PCM has no assigned card");
           goto __diga_end;
        }

       sprintf(ctl_name, "hw:%d", ctl_card);
       printf("hw:%d\n", ctl_card);
       if ((err = snd_ctl_open(&ctl_handler, ctl_name, 0)) < 0) {
          fprintf(stderr, "Unable to open the control interface '%s': %s\n", ctl_name, snd_strerror(err));                                                    
          goto __diga_end;
       }
       if ((err = snd_ctl_elem_write(ctl_handler, ctl)) < 0) {
	 //fprintf(stderr, "Unable to update the IEC958 control: %s\n", snd_strerror(err));
	 printf("alsa-spdif-init: cant set spdif-trough automatically\n");
        goto __diga_end;
       }
       	//test area
       /* elem_device = snd_ctl_elem_id_get_device(elem_id); */
       /* elem_name = snd_ctl_elem_value_get_name(ctl);  */
       /* snd_ctl_elem_value_get_iec958(ctl, &spdif); */
       /* printf("spdif = %i, device = %i\n", &spdif, elem_device); */
       /* printf("name = %s\n", elem_name); */
	//end test area


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

	  err = snd_pcm_hw_params_set_access(handler, params,SND_PCM_ACCESS_RW_INTERLEAVED);
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

      const char *mix_name = "PCM";
      char *card = "default";

      long pmin, pmax;
      long get_vol, set_vol;
      float calc_vol, diff, f_multi;

      if(ao_data.format == AFMT_AC3)
	return CONTROL_TRUE;

      //allocate simple id
      snd_mixer_selem_id_alloca(&sid);
	
      //sets simple-mixer index and name
      snd_mixer_selem_id_set_index(sid, 0);
      snd_mixer_selem_id_set_name(sid, mix_name);

      if ((err = snd_mixer_open(&handle, 0)) < 0) {
	printf("alsa-control: mixer open error: %s\n", snd_strerror(err));
	return CONTROL_ERROR;
      }

      if ((err = snd_mixer_attach(handle, card)) < 0) {
	printf("alsa-control: mixer attach %s error: %s", card, snd_strerror(err));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
      }

      if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
	printf("alsa-control: mixer register error: %s", snd_strerror(err));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
      }
      err = snd_mixer_load(handle);
      if (err < 0) {
	printf("alsa-control: mixer load error: %s", snd_strerror(err));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
      }

      elem = snd_mixer_find_selem(handle, sid);
      if (!elem) {
	printf("alsa-control: unable to find simple control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
	snd_mixer_close(handle);
	return CONTROL_ERROR;
	}

      snd_mixer_selem_get_playback_volume_range(elem,&pmin,&pmax);
      f_multi = (100 / (float)pmax);

      if (cmd == AOCONTROL_SET_VOLUME) {

	diff = (vol->left+vol->right) / 2;
	set_vol = rint(diff / f_multi);
	
	if (set_vol < 0)
	  set_vol = 0;
	else if (set_vol > pmax)
	  set_vol = pmax;

	//setting channels
	if ((err = snd_mixer_selem_set_playback_volume(elem, 0, set_vol)) < 0) {
	  printf("alsa-control: error setting left channel, %s",snd_strerror(err));
	  return CONTROL_ERROR;
	}
	if ((err = snd_mixer_selem_set_playback_volume(elem, 1, set_vol)) < 0) {
	  printf("alsa-control: error setting right channel, %s",snd_strerror(err));
	  return CONTROL_ERROR;
	}

	//printf("diff=%f, set_vol=%i, pmax=%i, mult=%f\n", diff, set_vol, pmax, f_multi);
      }
      else {
	snd_mixer_selem_get_playback_volume(elem, 0, &get_vol);
	calc_vol = get_vol;
	calc_vol = rintf(calc_vol * f_multi);

	vol->left = vol->right = (int)calc_vol;

	//printf("get_vol = %i, calc=%i\n",get_vol, calc_vol);
      }
      snd_mixer_close(handle);
      return CONTROL_OK;
    }
  }
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
    int period_val;
    snd_pcm_info_t *alsa_info;
    char *str_block_mode;
    int device_set = 0;
    
    printf("alsa-init: testing and bugreports are welcome.\n");    
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
    //ao_data.buffersize = MAX_OUTBURST; // was 16384

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
    
    if (ao_subdevice) {
      //start parsing ao_subdevice, ugly and not thread safe!
      //maybe there's a better way?
      int i2 = 1;
      int i3 = 0;
      char *sub_str;

      char *token_str[3];
      char* test_str = strdup(ao_subdevice);

      //printf("subd=%s, test=%s\n", ao_subdevice,test_str);

      if ((strcspn(ao_subdevice, ":")) > 0) {

	sub_str = strtok(test_str, ":");
	*(token_str) = sub_str;

	while (((sub_str = strtok(NULL, ":")) != NULL) && (i2 <= 3)) {
	  *(token_str+i2) = sub_str;
	  //printf("token %i: %s\n", i2, *(token_str+i2));
	  i2 += 1;
	}

	for (i3=0; i3 <= i2-1; i3++) {
	  //printf("test %i, %s\n", i3, *(token_str+i3));
	  if (strcmp(*(token_str + i3), "mmap") == 0) {
	    ao_mmap = 1;
	  }
	  else if (strcmp(*(token_str+i3), "noblock") == 0) {
	    ao_noblock = 1;
	  }
	  else if (strcmp(*(token_str+i3), "hw") == 0) {
	    if ((i3 < i2-1) && (strcmp(*(token_str+i3+1), "noblock") != 0) && (strcmp(*(token_str+i3+1), "mmap") != 0)) {
	      //printf("next tok = %s\n", *(token_str+(i3+1)));
	      alsa_device = alloca(ALSA_DEVICE_SIZE);
	      snprintf(alsa_device, ALSA_DEVICE_SIZE, "hw:%s", *(token_str+(i3+1)));
	      device_set = 1;
	    }
		else {
		  //printf("setting hw\n");
		  alsa_device = *(token_str+i3);
		  device_set = 1;
		}
	  }
	  else if (device_set == 0 && (!ao_mmap || !ao_noblock)) {
	    //printf("setting common, %s\n", *(token_str+i3));
	    alsa_device = *(token_str+i3);
	    device_set = 1;
	  }
	}
      }
    } //end parsing ao_subdevice

    if (alsa_device == NULL)
      {
	int tmp_device, tmp_subdevice, err;

	if ((err = snd_pcm_info_malloc(&alsa_info)) < 0)
	  {
	    printf("alsa-init: memory allocation error: %s\n", snd_strerror(err));
	    return(0);
	  }
	
	if ((alsa_device = alloca(ALSA_DEVICE_SIZE)) == NULL)
	  {
	    printf("alsa-init: memory allocation error: %s\n", strerror(errno));
	    return(0);
	  }

	if ((tmp_device = snd_pcm_info_get_device(alsa_info)) < 0)
	  {
	    printf("alsa-init: cant get device\n");
	    return(0);
	  }

	if ((tmp_subdevice = snd_pcm_info_get_subdevice(alsa_info)) < 0)
	  {
	    printf("alsa-init: cant get subdevice\n");
	    return(0);
	  }
	
	if (verbose)
	  printf("alsa-init: got device=%i, subdevice=%i\n", tmp_device, tmp_subdevice);

	if ((err = snprintf(alsa_device, ALSA_DEVICE_SIZE, "hw:%1d,%1d", tmp_device, tmp_subdevice)) <= 0)
	  {
	    printf("alsa-init: cant wrote device-id\n");
	  }

	snd_pcm_info_free(alsa_info);
	printf("alsa-init: %d soundcard%s found, using: %s\n", cards+1,
	       (cards >= 0) ? "" : "s", alsa_device);
      } else if (strcmp(alsa_device, "help") == 0) {
	printf("alsa-help: available options are:\n");
	printf("           mmap: sets mmap-mode\n");
	printf("           noblock: sets noblock-mode\n");
	printf("           device-name: sets device name\n");
	printf("           example -ao alsa9:mmap:noblock:hw:0,3 sets noblock-mode,\n");
	printf("           mmap-mode and the device-name as first card third device\n");
	return(0);
      } else {
		printf("alsa-init: soundcard set to %s\n", alsa_device);
      }

    // switch for spdif
    // Try to initialize the SPDIF interface
    if (format == AFMT_AC3) {
      if (device_set)
	alsa_handler = spdif_init(alsa_device);
      else
	alsa_handler = spdif_init("hw:0,2");
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
    //cvs cosmetics fix
    //sets buff/chunksize if its set manually
    if (ao_data.buffersize) {
      switch (ao_data.buffersize)
	{
	case 1:
	  alsa_fragcount = 16;
	  chunk_size = 512;
	  if (verbose) {
	    printf("alsa-init: buffersize set manually to 8192\n");
	    printf("alsa-init: chunksize set manually to 512\n");
	  }
	  break;
	case 2:
	  alsa_fragcount = 8;
	  chunk_size = 1024;
	  if (verbose) {
	    printf("alsa-init: buffersize set manually to 8192\n");
	    printf("alsa-init: chunksize set manually to 1024\n");
	  }
	  break;
	case 3:
	  alsa_fragcount = 32;
	  chunk_size = 512;
	  if (verbose) {
	    printf("alsa-init: buffersize set manually to 16384\n");
	    printf("alsa-init: chunksize set manually to 512\n");
	  }
	  break;
	case 4:
	  alsa_fragcount = 16;
	  chunk_size = 1024;
	  if (verbose) {
	    printf("alsa-init: buffersize set manually to 16384\n");
	    printf("alsa-init: chunksize set manually to 1024\n");
	  }
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
	  if (ao_noblock) {
	    printf("alsa-init: open in nonblock-mode failed, trying to open in block-mode\n");
	    if ((err = snd_pcm_open(&alsa_handler, alsa_device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
	      printf("alsa-init: playback open error: %s\n", snd_strerror(err));
	      return(0);
	    } else {
	      set_block_mode = 0;
	      str_block_mode = "block-mode";
	    }
	  } else {
	    printf("alsa-init: playback open error: %s\n", snd_strerror(err));
	    return(0);
	  }
	}

      if ((err = snd_pcm_nonblock(alsa_handler, set_block_mode)) < 0) {
	printf("alsa-init: error set block-mode %s\n", snd_strerror(err));
      }
      else if (verbose) {
	printf("alsa-init: pcm opend in %s\n", str_block_mode);
      }
      
      snd_pcm_hw_params_alloca(&alsa_hwparams);
      snd_pcm_sw_params_alloca(&alsa_swparams);

      // setting hw-parameters
      if ((err = snd_pcm_hw_params_any(alsa_handler, alsa_hwparams)) < 0)
	{
	  printf("alsa-init: unable to get initial parameters: %s\n",
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
	printf("alsa-init: mmap set\n");
      } else {
	err = snd_pcm_hw_params_set_access(alsa_handler, alsa_hwparams,SND_PCM_ACCESS_RW_INTERLEAVED);
      }
      if (err < 0) {
	printf("alsa-init: unable to set access type: %s\n", snd_strerror(err));
	return (0);
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
        {
	  printf("alsa-init: unable to set samplerate-2: %s\n",
		 snd_strerror(err));
	  return(0);
        }

#ifdef BUFFERTIME
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
	if (verbose)
	  printf("alsa-init: buffer_time: %d, period_time :%d\n",alsa_buffer_time, err);
      }
#endif

#ifdef SET_CHUNKSIZE
      {
	//set chunksize
	if ((err = snd_pcm_hw_params_set_period_size(alsa_handler, alsa_hwparams, chunk_size, 0)) < 0)
	  {
	    printf("alsa-init: unable to set periodsize: %s\n", snd_strerror(err));
	    return(0);
	  }
	else if (verbose) {
	  printf("alsa-init: chunksize set to %i\n", chunk_size);
	}

	//set period_count
	if ((period_val = snd_pcm_hw_params_get_periods_max(alsa_hwparams, 0)) < alsa_fragcount) {
	  alsa_fragcount = period_val;			
	}

	if (verbose)
	  printf("alsa-init: current val=%i, fragcount=%i\n", period_val, alsa_fragcount);

	if ((err = snd_pcm_hw_params_set_periods(alsa_handler, alsa_hwparams, alsa_fragcount, 0)) < 0) {
	  printf("alsa-init: unable to set periods: %s\n", snd_strerror(err));
	}
      }
#endif

      /* finally install hardware parameters */
      if ((err = snd_pcm_hw_params(alsa_handler, alsa_hwparams)) < 0)
	{
	  printf("alsa-init: unable to set hw-parameters: %s\n",
		 snd_strerror(err));
	  return(0);
	}
      // end setting hw-params


      // gets buffersize for control
      if ((err = snd_pcm_hw_params_get_buffer_size(alsa_hwparams)) < 0)
	{
	  printf("alsa-init: unable to get buffersize: %s\n", snd_strerror(err));
	  return(0);
	}
      else {
	ao_data.buffersize = err;
	if (verbose)
	  printf("alsa-init: got buffersize=%i\n", ao_data.buffersize);
      }

      // setting sw-params (only avail-min) if noblocking mode was choosed
      if (ao_noblock)
	{

	  if ((err = snd_pcm_sw_params_current(alsa_handler, alsa_swparams)) < 0)
	    {
	      printf("alsa-init: unable to get parameters: %s\n",snd_strerror(err));
	      return(0);
	    }

	  //set min available frames to consider pcm ready (4)
	  //increased for nonblock-mode should be set dynamically later
	  if ((err = snd_pcm_sw_params_set_avail_min(alsa_handler, alsa_swparams, 4)) < 0)
	    {
	      printf("alsa-init: unable to set avail_min %s\n",snd_strerror(err));
	      return(0);
	    }

	  if ((err = snd_pcm_sw_params(alsa_handler, alsa_swparams)) < 0)
	    {
	      printf("alsa-init: unable to install sw-params\n");
	      return(0);
	    }

	  bits_per_sample = snd_pcm_format_physical_width(alsa_format);
	  bits_per_frame = bits_per_sample * channels;
	  chunk_bytes = chunk_size * bits_per_frame / 8;

	  if (verbose) {
	    printf("alsa-init: bits per sample (bps)=%i, bits per frame (bpf)=%i, chunk_bytes=%i\n",bits_per_sample,bits_per_frame,chunk_bytes);}

	}//end swparams

      if ((err = snd_pcm_prepare(alsa_handler)) < 0)
	{
	  printf("alsa-init: pcm prepare error: %s\n", snd_strerror(err));
	  return(0);
	}

      printf("alsa9: %d Hz/%d channels/%d bpf/%d bytes buffer/%s\n",
	     ao_data.samplerate, ao_data.channels, ao_data.bps, ao_data.buffersize,
	     snd_pcm_format_description(alsa_format));

    } // end switch alsa_handler (spdif)
    return(1);
} // end init


/* close audio device */
static void uninit()
{

  if (alsa_handler) {
    int err;

    if (!ao_noblock) {
      if ((err = snd_pcm_drain(alsa_handler)) < 0)
	{
	  printf("alsa-uninit: pcm drain error: %s\n", snd_strerror(err));
	  return;
	}
    }

    if ((err = snd_pcm_close(alsa_handler)) < 0)
      {
	printf("alsa-uninit: pcm close error: %s\n", snd_strerror(err));
	return;
      }
    else {
      alsa_handler = NULL;
      alsa_device = NULL;
      printf("alsa-uninit: pcm closed\n");
    }
  }
  else {
    printf("alsa-uninit: no handler defined!\n");
  }
}

static void audio_pause()
{
    int err;

    if (!ao_noblock) {
      //drain causes error in nonblock-mode!
      if ((err = snd_pcm_drain(alsa_handler)) < 0)
	{
	  printf("alsa-pause: pcm drain error: %s\n", snd_strerror(err));
	  return;
	}
    }
    else {
      if (verbose)
	printf("alsa-pause: paused nonblock\n");

      return;
    }
}

static void audio_resume()
{
    int err;

    if ((err = snd_pcm_prepare(alsa_handler)) < 0)
    {
	printf("alsa-resume: pcm prepare error: %s\n", snd_strerror(err));
	return;
    }
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset()
{
    int err;

    if (!ao_noblock) {
      //drain causes error in nonblock-mode!
      if ((err = snd_pcm_drain(alsa_handler)) < 0)
	{
	  printf("alsa-pause: pcm drain error: %s\n", snd_strerror(err));
	  return;
	}

      if ((err = snd_pcm_prepare(alsa_handler)) < 0)
	{
	  printf("alsa-reset: pcm prepare error: %s\n", snd_strerror(err));
	  return;
	}
    } else {
      if (verbose)
	printf("alsa-reset: reset nonblocked");
      return;
    }
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
    printf("status error: %s", snd_strerror(err));
    return(0);
  }

  if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
    struct timeval now, diff, tstamp;
    gettimeofday(&now, 0);
    snd_pcm_status_get_trigger_tstamp(status, &tstamp);
    timersub(&now, &tstamp, &diff);
    printf("alsa-%s: xrun of at least %.3f msecs. resetting stream\n",
	   str_mode,
	   diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
  }

  if ((err = snd_pcm_prepare(alsa_handler))<0) {
    printf("xrun: prepare error: %s", snd_strerror(err));
    return(0);
  }

  return(1); /* ok, data should be accepted again */
}

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

  //ao_data.bps is always 4 for 2 chn S16_LE
  int num_frames = len / ao_data.bps;
  signed short *output_samples=data;
  snd_pcm_sframes_t res = 0;

  //printf("alsa-play: frames=%i, len=%i",num_frames,len);

  if (!alsa_handler) {
    printf("alsa-play: device configuration error");
    return 0;
  }

  while (num_frames > 0) {

    res = snd_pcm_writei(alsa_handler, (void *)output_samples, num_frames);

      if (res == -EAGAIN) {
	snd_pcm_wait(alsa_handler, 1000);
      }
      else if (res == -EPIPE) {  /* underrun */
	if (xrun("play") <= 0) {
	  printf("alsa-play: xrun reset error");
	  return(0);
	}
      }
      else if (res == -ESTRPIPE) {	/* suspend */
	printf("alsa-play: pcm in suspend mode. trying to resume\n");
	while ((res = snd_pcm_resume(alsa_handler)) == -EAGAIN)
	  sleep(1);
      }
      else if (res < 0) {
	printf("alsa-play: unknown status, trying to reset soundcard\n");
	if ((res = snd_pcm_prepare(alsa_handler)) < 0) {
	  printf("alsa-play: snd prepare error");
	  return(0);
	  break;
	}
      }

      if (res > 0) {
	output_samples += ao_data.channels * res;
	num_frames -= res;
      }

  } //end while

  if (res < 0) {
    printf("alsa-play: write error %s", snd_strerror(res));
    return 0;
  }
  return res < 0 ? (int)res : len;
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
  int err, result;

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
  frames_available = snd_pcm_avail_update(alsa_handler) * ao_data.bps;
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
   * but real avail_buffer in frames is ab/ao_data.bps */
  size = len / ao_data.bps;

  //if (verbose)
  //printf("len: %i size %i, f_avail %i, bps %i ...\n", len, size, frames_available, ao_data.bps);

  frames_transmit = size;

  /* prepare areas and set sw-pointers
   * frames_transmit returns the real available buffer-size
   * sometimes != frames_available cause of ringbuffer 'emulation' */
  snd_pcm_mmap_begin(alsa_handler, &area, &offset, &frames_transmit);

  /* this is specific to interleaved streams (or non-interleaved
   * streams with only one channel) */
  outbuffer = ((char *) area->addr + (area->first + area->step * offset) / 8); //8

  //write data
  memcpy(outbuffer, data, (frames_transmit * ao_data.bps));

  commitres = snd_pcm_mmap_commit(alsa_handler, offset, frames_transmit);

  if (commitres < 0 || commitres != frames_transmit) {
    if (snd_pcm_state(alsa_handler) == SND_PCM_STATE_XRUN || 
	snd_pcm_state(alsa_handler) == SND_PCM_STATE_SUSPENDED) {
      xrun("play");
    }
  }

  //if (verbose)
  //printf("mmap ft: %i, cres: %i\n", frames_transmit, commitres);

  /* 	err = snd_pcm_area_copy(&area, offset, &data, offset, len, alsa_format); */
  /* 	if (err < 0) { */
  /* 	  printf("area-copy-error\n"); */
  /* 	  return 0; */
  /* 	} */


  //calculate written frames!
  result = commitres * ao_data.bps;


  /* if (verbose) { */
  /* if (len == result) */
  /* printf("result: %i, frames written: %i ...\n", result, frames_transmit); */
  /* else */
  /* printf("result: %i, frames written: %i, result != len ...\n", result, frames_transmit); */
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
      str_status = "open";
    case SND_PCM_STATE_PREPARED:
      if (str_status != "open") {
	str_status = "prepared";
	first = 1;
	ret = snd_pcm_status_get_avail(status) * ao_data.bps;
	if (ret == 0) //ugly workaround for hang in mmap-mode
	  ret = 10;
	break;
      }
    case SND_PCM_STATE_RUNNING:
      ret = snd_pcm_status_get_avail(status) * ao_data.bps;
      //avail_frames = snd_pcm_avail_update(alsa_handler) * ao_data.bps;
      if (str_status != "open" && str_status != "prepared")
	str_status = "running";
      break;
    case SND_PCM_STATE_PAUSED:
      if (verbose) printf("alsa-space: paused");
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
      ret = snd_pcm_status_get_avail(status) * ao_data.bps;
      if (ret <= 0) {
	xrun("space");
      }
    }

    if (verbose && str_status != "running")
      printf("alsa-space: free space = %i, status=%i, %s --\n", ret, status, str_status);
    snd_pcm_status_free(status);
    
    if (ret < 0) {
      printf("negative value!!\n");
      ret = 0;
    }
 
    return(ret);
}

/* delay in seconds between first and last sample in buffer */
static float get_delay()
{

  if (alsa_handler) {

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
    
  } else {
    return(0);
  }
}
