#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
//#include <sys/soundcard.h>

#include "../config.h"
#include "../mp_msg.h"
#include "../mixer.h"

#include "afmt.h"

#include "audio_out.h"
#include "audio_out_internal.h"

extern int verbose;

static ao_info_t info = 
{
	"OSS/ioctl audio output",
	"oss",
	"A'rpi",
	""
};

/* Support for >2 output channels added 2001-11-25 - Steve Davies <steve@daviesfam.org> */

LIBAO_EXTERN(oss)

static char *dsp=PATH_DEV_DSP;
static audio_buf_info zz;
static int audio_fd=-1;

char *oss_mixer_device = PATH_DEV_MIXER;

// to set/get/query special features/parameters
static int control(int cmd,int arg){
    switch(cmd){
	case AOCONTROL_SET_DEVICE:
	    dsp=(char*)arg;
	    return CONTROL_OK;
	case AOCONTROL_GET_DEVICE:
	    (char*)arg=dsp;
	    return CONTROL_OK;
	case AOCONTROL_QUERY_FORMAT:
	    return CONTROL_TRUE;
	case AOCONTROL_GET_VOLUME:
	case AOCONTROL_SET_VOLUME:
	{
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    int fd, v, mcmd, devs;

	    if(ao_data.format == AFMT_AC3)
		return CONTROL_TRUE;
    
	    if ((fd = open(oss_mixer_device, O_RDONLY)) > 0)
	    {
		ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
		if (devs & SOUND_MASK_PCM)
		{
		    if (cmd == AOCONTROL_GET_VOLUME)
		    {
		        ioctl(fd, SOUND_MIXER_READ_PCM, &v);
			vol->right = (v & 0xFF00) >> 8;
			vol->left = v & 0x00FF;
		    }
		    else
		    {
		        v = ((int)vol->right << 8) | (int)vol->left;
			ioctl(fd, SOUND_MIXER_WRITE_PCM, &v);
		    }
		}
		else
		{
		    close(fd);
		    return CONTROL_ERROR;
		}
		close(fd);
		return CONTROL_OK;
	    }
	}
	return CONTROL_ERROR;
    }
    return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

  mp_msg(MSGT_AO,MSGL_V,"ao2: %d Hz  %d chans  %s\n",rate,channels,
    audio_out_format_name(format));

  if (ao_subdevice)
    dsp = ao_subdevice;

  if(mixer_device)
    oss_mixer_device=mixer_device;

  mp_msg(MSGT_AO,MSGL_V,"audio_setup: using '%s' dsp device\n", dsp);

#ifdef __linux__
  audio_fd=open(dsp, O_WRONLY | O_NONBLOCK);
#else
  audio_fd=open(dsp, O_WRONLY);
#endif
  if(audio_fd<0){
    mp_msg(MSGT_AO,MSGL_ERR,"audio_setup: Can't open audio device %s: %s\n", dsp, strerror(errno));
    return 0;
  }

#ifdef __linux__
  /* Remove the non-blocking flag */
  if(fcntl(audio_fd, F_SETFL, 0) < 0) {
   mp_msg(MSGT_AO,MSGL_ERR,"audio_setup: Can't make filedescriptor non-blocking: %s\n", strerror(errno));
   return 0;
  }  
#endif
  
  ao_data.bps=channels;
  if(format != AFMT_U8 && format != AFMT_S8)
    ao_data.bps*=2;

  if(format == AFMT_AC3) {
    ao_data.samplerate=rate;
    ioctl (audio_fd, SNDCTL_DSP_SPEED, &ao_data.samplerate);
  }

ac3_retry:  
  ao_data.format=format;
  if( ioctl(audio_fd, SNDCTL_DSP_SETFMT, &ao_data.format)<0 ||
      ao_data.format != format) if(format == AFMT_AC3){
    mp_msg(MSGT_AO,MSGL_WARN,"Can't set audio device %s to AC3 output, trying S16...\n", dsp);
#ifdef WORDS_BIGENDIAN
    format=AFMT_S16_BE;
#else
    format=AFMT_S16_LE;
#endif
    goto ac3_retry;
  }
  mp_msg(MSGT_AO,MSGL_V,"audio_setup: sample format: %s (requested: %s)\n",
    audio_out_format_name(ao_data.format), audio_out_format_name(format));
  
  if(format != AFMT_AC3) {
    // We only use SNDCTL_DSP_CHANNELS for >2 channels, in case some drivers don't have it
    ao_data.channels = channels;
    if (ao_data.channels > 2) {
      if ( ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &ao_data.channels) == -1 ||
	   ao_data.channels != channels ) {
	mp_msg(MSGT_AO,MSGL_ERR,"audio_setup: Failed to set audio device to %d channels\n", channels);
	return 0;
      }
    }
    else {
      int c = ao_data.channels-1;
      if (ioctl (audio_fd, SNDCTL_DSP_STEREO, &c) == -1) {
	mp_msg(MSGT_AO,MSGL_ERR,"audio_setup: Failed to set audio device to %d channels\n", ao_data.channels);
	return 0;
      }
    }
    mp_msg(MSGT_AO,MSGL_V,"audio_setup: using %d channels (requested: %d)\n", ao_data.channels, channels);
    // set rate
    ao_data.samplerate=rate;
    ioctl (audio_fd, SNDCTL_DSP_SPEED, &ao_data.samplerate);
    mp_msg(MSGT_AO,MSGL_V,"audio_setup: using %d Hz samplerate (requested: %d)\n",ao_data.samplerate,rate);
    if(ao_data.samplerate!=rate)
	mp_msg(MSGT_AO,MSGL_WARN,"WARNING! Your soundcard does NOT support %d Hz samplerate! A-V sync problems or wrong speed are possible! Try with '-aop list=resample:fout=%d'\n",rate,ao_data.samplerate);
  }

  if(ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &zz)==-1){
      int r=0;
      mp_msg(MSGT_AO,MSGL_WARN,"audio_setup: driver doesn't support SNDCTL_DSP_GETOSPACE :-(\n");
      if(ioctl(audio_fd, SNDCTL_DSP_GETBLKSIZE, &r)==-1){
          mp_msg(MSGT_AO,MSGL_V,"audio_setup: %d bytes/frag (config.h)\n",ao_data.outburst);
      } else {
          ao_data.outburst=r;
          mp_msg(MSGT_AO,MSGL_V,"audio_setup: %d bytes/frag (GETBLKSIZE)\n",ao_data.outburst);
      }
  } else {
      mp_msg(MSGT_AO,MSGL_V,"audio_setup: frags: %3d/%d  (%d bytes/frag)  free: %6d\n",
          zz.fragments, zz.fragstotal, zz.fragsize, zz.bytes);
      if(ao_data.buffersize==-1) ao_data.buffersize=zz.bytes;
      ao_data.outburst=zz.fragsize;
  }

  if(ao_data.buffersize==-1){
    // Measuring buffer size:
    void* data;
    ao_data.buffersize=0;
#ifdef HAVE_AUDIO_SELECT
    data=malloc(ao_data.outburst); memset(data,0,ao_data.outburst);
    while(ao_data.buffersize<0x40000){
      fd_set rfds;
      struct timeval tv;
      FD_ZERO(&rfds); FD_SET(audio_fd,&rfds);
      tv.tv_sec=0; tv.tv_usec = 0;
      if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) break;
      write(audio_fd,data,ao_data.outburst);
      ao_data.buffersize+=ao_data.outburst;
    }
    free(data);
    if(ao_data.buffersize==0){
        mp_msg(MSGT_AO,MSGL_ERR,"\n   ***  Your audio driver DOES NOT support select()  ***\n"
          "Recompile mplayer with #undef HAVE_AUDIO_SELECT in config.h !\n\n");
        return 0;
    }
#endif
  }

  ao_data.outburst-=ao_data.outburst % ao_data.bps; // round down
  ao_data.bps*=rate;

    return 1;
}

// close audio device
static void uninit(){
#ifdef SNDCTL_DSP_RESET
    ioctl(audio_fd, SNDCTL_DSP_RESET, NULL);
#endif
    close(audio_fd);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(){
    uninit();
    audio_fd=open(dsp, O_WRONLY);
    if(audio_fd < 0){
	mp_msg(MSGT_AO,MSGL_ERR,"\nFatal error: *** CANNOT RE-OPEN / RESET AUDIO DEVICE *** %s\n", strerror(errno));
	return;
    }

  ioctl (audio_fd, SNDCTL_DSP_SETFMT, &ao_data.format);
  if(ao_data.format != AFMT_AC3) {
    if (ao_data.channels > 2)
      ioctl (audio_fd, SNDCTL_DSP_CHANNELS, &ao_data.channels);
    else {
      int c = ao_data.channels-1;
      ioctl (audio_fd, SNDCTL_DSP_STEREO, &c);
    }
    ioctl (audio_fd, SNDCTL_DSP_SPEED, &ao_data.samplerate);
  }
}

// stop playing, keep buffers (for pause)
static void audio_pause()
{
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
static void audio_resume()
{
}


// return: how many bytes can be played without blocking
static int get_space(){
  int playsize=ao_data.outburst;

#ifdef SNDCTL_DSP_GETOSPACE
  if(ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &zz)!=-1){
      // calculate exact buffer space:
      playsize = zz.fragments*zz.fragsize;
      if (playsize > MAX_OUTBURST)
	playsize = (MAX_OUTBURST / zz.fragsize) * zz.fragsize;
      return playsize;
  }
#endif

    // check buffer
#ifdef HAVE_AUDIO_SELECT
    {  fd_set rfds;
       struct timeval tv;
       FD_ZERO(&rfds);
       FD_SET(audio_fd, &rfds);
       tv.tv_sec = 0;
       tv.tv_usec = 0;
       if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) return 0; // not block!
    }
#endif

  return ao_data.outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
    len/=ao_data.outburst;
    len=write(audio_fd,data,len*ao_data.outburst);
    return len;
}

static int audio_delay_method=2;

// return: delay in seconds between first and last sample in buffer
static float get_delay(){
  /* Calculate how many bytes/second is sent out */
  if(audio_delay_method==2){
#ifdef SNDCTL_DSP_GETODELAY
      int r=0;
      if(ioctl(audio_fd, SNDCTL_DSP_GETODELAY, &r)!=-1)
         return ((float)r)/(float)ao_data.bps;
#endif
      audio_delay_method=1; // fallback if not supported
  }
  if(audio_delay_method==1){
      // SNDCTL_DSP_GETOSPACE
      if(ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &zz)!=-1)
         return ((float)(ao_data.buffersize-zz.bytes))/(float)ao_data.bps;
      audio_delay_method=0; // fallback if not supported
  }
  return ((float)ao_data.buffersize)/(float)ao_data.bps;
}
