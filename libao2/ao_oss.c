#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <sys/soundcard.h>

#include "../config.h"

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

LIBAO_EXTERN(oss)

// there are some globals:
// ao_samplerate
// ao_channels
// ao_format
// ao_bps
// ao_outburst
// ao_buffersize

static char *dsp="/dev/dsp";
static audio_buf_info zz;
static int audio_fd=-1;

char *oss_mixer_device = "/dev/mixer";
int oss_mixer_usemaster = 0;

// to set/get/query special features/parameters
static int control(int cmd,int arg){
    switch(cmd){
	case AOCONTROL_SET_DEVICE:
	    dsp=(char*)arg;
	    return CONTROL_OK;
	case AOCONTROL_QUERY_FORMAT:
	    return CONTROL_TRUE;
	case AOCONTROL_GET_VOLUME:
	case AOCONTROL_SET_VOLUME:
	{
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    int fd, v, mcmd, devs;

	    if(ao_format == AFMT_AC3)
		return CONTROL_TRUE;
    
	    if ((fd = open("/dev/mixer", O_RDONLY)) > 0)
	    {
		ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
		if ((devs & SOUND_MASK_PCM) && (oss_mixer_usemaster == 0))
		    if (cmd == AOCONTROL_GET_VOLUME)
			mcmd = SOUND_MIXER_READ_PCM;
		    else
			mcmd = SOUND_MIXER_WRITE_PCM;
		else if ((devs & SOUND_MASK_VOLUME) && (oss_mixer_usemaster == 1))
		    if (cmd == AOCONTROL_GET_VOLUME)
			mcmd = SOUND_MIXER_READ_VOLUME;
		    else
			mcmd = SOUND_MIXER_WRITE_VOLUME;
		else
		{
		    close(fd);
		    return CONTROL_ERROR;
		}

		if (cmd == AOCONTROL_GET_VOLUME)
		{
		    ioctl(fd, cmd, &v);
		    vol->right = (v & 0xFF00) >> 8;
		    vol->left = v & 0x00FF;
		}
		else
		{
		    v = ((int)vol->right << 8) | (int)vol->left;
		    ioctl(fd, cmd, &v);
		}
		close(fd);
		return CONTROL_OK;
	    }
	    else
		return CONTROL_ERROR;
	}
	return CONTROL_ERROR;
	default:
    }
    return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

//  printf("ao2: %d Hz  %d chans  %s\n",rate,channels,
//    audio_out_format_name(format));

  if (ao_subdevice)
    dsp = ao_subdevice;

  if (verbose)
    printf("audio_setup: using '%s' dsp device\n", dsp);

  audio_fd=open(dsp, O_WRONLY);
  if(audio_fd<0){
    printf("Can't open audio device %s  -> nosound\n",dsp);
    return 0;
  }

  ao_format=format;
  ioctl (audio_fd, SNDCTL_DSP_SETFMT, &ao_format);
  if(format == AFMT_AC3 && ao_format != AFMT_AC3) {
      printf("Can't set audio device %s to AC3 output\n", dsp);
      return 0;
  }
  printf("audio_setup: sample format: %s (requested: %s)\n",
    audio_out_format_name(ao_format), audio_out_format_name(format));
  
  if(format != AFMT_AC3) {
  ao_channels=channels-1;
  ioctl (audio_fd, SNDCTL_DSP_STEREO, &ao_channels);
  
  // set rate
  ao_samplerate=rate;
  ioctl (audio_fd, SNDCTL_DSP_SPEED, &ao_samplerate);
  printf("audio_setup: using %d Hz samplerate (requested: %d)\n",ao_samplerate,rate);
  }

  if(ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &zz)==-1){
      int r=0;
      printf("audio_setup: driver doesn't support SNDCTL_DSP_GETOSPACE :-(\n");
      if(ioctl(audio_fd, SNDCTL_DSP_GETBLKSIZE, &r)==-1){
          printf("audio_setup: %d bytes/frag (config.h)\n",ao_outburst);
      } else {
          ao_outburst=r;
          printf("audio_setup: %d bytes/frag (GETBLKSIZE)\n",ao_outburst);
      }
  } else {
      printf("audio_setup: frags: %3d/%d  (%d bytes/frag)  free: %6d\n",
          zz.fragments, zz.fragstotal, zz.fragsize, zz.bytes);
      if(ao_buffersize==-1) ao_buffersize=zz.bytes;
      ao_outburst=zz.fragsize;
  }

  if(ao_buffersize==-1){
    // Measuring buffer size:
    void* data;
    ao_buffersize=0;
#ifdef HAVE_AUDIO_SELECT
    data=malloc(ao_outburst); memset(data,0,ao_outburst);
    while(ao_buffersize<0x40000){
      fd_set rfds;
      struct timeval tv;
      FD_ZERO(&rfds); FD_SET(audio_fd,&rfds);
      tv.tv_sec=0; tv.tv_usec = 0;
      if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) break;
      write(audio_fd,data,ao_outburst);
      ao_buffersize+=ao_outburst;
    }
    free(data);
    if(ao_buffersize==0){
        printf("\n   ***  Your audio driver DOES NOT support select()  ***\n");
          printf("Recompile mplayer with #undef HAVE_AUDIO_SELECT in config.h !\n\n");
        return 0;
    }
#endif
  }

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
    if(audio_fd<0){
	printf("\nFatal error: *** CANNOT RE-OPEN / RESET AUDIO DEVICE ***\n");
	return;
    }

  ioctl (audio_fd, SNDCTL_DSP_SETFMT, &ao_format);
  if(ao_format != AFMT_AC3) {
  ioctl (audio_fd, SNDCTL_DSP_STEREO, &ao_channels);
  ioctl (audio_fd, SNDCTL_DSP_SPEED, &ao_samplerate);
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
  int playsize=ao_outburst;

#ifdef SNDCTL_DSP_GETOSPACE
  if(ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &zz)!=-1){
      // calculate exact buffer space:
      return zz.fragments*zz.fragsize;
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

  return ao_outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
    len/=ao_outburst;
    len=write(audio_fd,data,len*ao_outburst);
    return len;
}

static int audio_delay_method=2;

// return: how many unplayed bytes are in the buffer
static int get_delay(){
  if(audio_delay_method==2){
      // 
      int r=0;
      if(ioctl(audio_fd, SNDCTL_DSP_GETODELAY, &r)!=-1)
         return r;
      audio_delay_method=1; // fallback if not supported
  }
  if(audio_delay_method==1){
      // SNDCTL_DSP_GETOSPACE
      if(ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &zz)!=-1)
         return ao_buffersize-zz.bytes;
      audio_delay_method=0; // fallback if not supported
  }
  return ao_buffersize;
}

