#include <stdio.h>
#include <stdlib.h>

#include <linux/em8300.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../config.h"

#include "afmt.h"

#include "audio_out.h"
#include "audio_out_internal.h"

void perror( const char *s );
#include <errno.h>
int sys_nerr;
extern int verbose;

static ao_info_t info = 
{
	"DXR3/H+ audio out",
	"dxr3",
	"David Holm <dholm@iname.com>",
	""
};

LIBAO_EXTERN(dxr3)

static audio_buf_info dxr3_buf_info;
static int fd_control = 0, fd_audio = 0;

// to set/get/query special features/parameters
static int control(int cmd,int arg)
{
    switch(cmd)
    {
	case AOCONTROL_QUERY_FORMAT:
	    return CONTROL_TRUE;
	case AOCONTROL_GET_VOLUME:
	case AOCONTROL_SET_VOLUME:
		return CONTROL_OK;
	return CONTROL_ERROR;
    }
    return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags)
{
  int ioval;
  fd_audio = open( "/dev/em8300_ma", O_WRONLY );  
  if( fd_audio < 0 )
  {
    printf("AO: [dxr3] Can't open audio device /dev/em8300_ma  -> nosound\n");
    return 0;
  }
  
  fd_control = open( "/dev/em8300", O_WRONLY );
  if( fd_control < 0 )
  {
    printf("AO: [dxr3] Can't open em8300 control /dev/em8300\n");
    return 0;
  }

  ioctl(fd_audio, SNDCTL_DSP_RESET, NULL);
  ao_data.format = format;
  if( ioctl (fd_audio, SNDCTL_DSP_SETFMT, &ao_data.format) < 0 )
    printf( "AO: [dxr3] Unable to set audio format\n" );
  if(format == AFMT_AC3 && ao_data.format != AFMT_AC3) 
  {
      printf("AO: [dxr3] Can't set audio device /dev/em8300_ma to AC3 output\n");
      return 0;
  }
  printf("AO: [dxr3] Sample format: %s (requested: %s)\n",
    audio_out_format_name(ao_data.format), audio_out_format_name(format));
  
  if(format != AFMT_AC3) 
  {
	ao_data.channels=channels-1;
        if( ioctl (fd_audio, SNDCTL_DSP_STEREO, &ao_data.channels) < 0 )
	    printf( "AO: [dxr3] Unable to set number of channels\n" );
  
	// set rate
	ao_data.bps = (channels+1)*rate;
	ao_data.samplerate=rate;
	if( ioctl (fd_audio, SNDCTL_DSP_SPEED, &ao_data.samplerate) < 0 )
	    printf( "AO: [dxr3] Unable to set samplerate\n" );
	printf("AO: [dxr3] Using %d Hz samplerate (requested: %d)\n",ao_data.samplerate,rate);
  }
  else ao_data.bps *= 2;

  if( ioctl(fd_audio, SNDCTL_DSP_GETOSPACE, &dxr3_buf_info)==-1 )
  {
      int r=0;
      printf("AO: [dxr3] Driver doesn't support SNDCTL_DSP_GETOSPACE :-(\n");
      if( ioctl( fd_audio, SNDCTL_DSP_GETBLKSIZE, &r) ==-1 )
      {
          printf( "AO: [dxr3] %d bytes/frag (config.h)\n", ao_data.outburst );
      } 
      else 
      { 
          ao_data.outburst=r;
          printf( "AO: [dxr3] %d bytes/frag (GETBLKSIZE)\n",ao_data.outburst);
      }
  } 
  else 
  {
      printf("AO: [dxr3] frags: %3d/%d  (%d bytes/frag)  free: %6d\n",
          dxr3_buf_info.fragments+1, dxr3_buf_info.fragstotal, dxr3_buf_info.fragsize, dxr3_buf_info.bytes);
      ao_data.buffersize=(dxr3_buf_info.bytes/2)-1;
      ao_data.outburst=dxr3_buf_info.fragsize;
  }

  ioval = EM8300_PLAYMODE_PLAY;
  if( ioctl( fd_control, EM8300_IOCTL_SET_PLAYMODE, &ioval ) < 0 )
    printf( "AO: [dxr3] Unable to set playmode\n" );
  close( fd_control );

  return 1;
}

// close audio device
static void uninit()
{
    printf( "AO: [dxr3] Uninitializing\n" );
    if( ioctl(fd_audio, SNDCTL_DSP_RESET, NULL) < 0 )
	printf( "AO: [dxr3] Unable to reset device\n" );
    close( fd_audio );
    close( fd_control ); /* Just in case */
}

// stop playing and empty buffers (for seeking/pause)
static void reset()
{
    if( ioctl(fd_audio, SNDCTL_DSP_RESET, NULL) < 0 )
	printf( "AO: [dxr3] Unable to reset device\n" );
}

// stop playing, keep buffers (for pause)
static void audio_pause()
{
  int ioval;
  fd_control = open( "/dev/em8300", O_WRONLY );
  if( fd_control < 0 )
    printf( "AO: [dxr3] Oops, unable to pause playback\n" );
  else
  {
    ioval = EM8300_PLAYMODE_PAUSED;
    if( ioctl( fd_control, EM8300_IOCTL_SET_PLAYMODE, &ioval ) < 0 )
	printf( "AO: [dxr3] Unable to pause playback\n" );
    close( fd_control );
  }
}

// resume playing, after audio_pause()
static void audio_resume()
{
  int ioval;
  fd_control = open( "/dev/em8300", O_WRONLY );
  if( fd_control < 0 )
    printf( "AO: [dxr3] Oops, unable to resume playback\n" );
  else
  {
    ioval = EM8300_PLAYMODE_PLAY;
    if( ioctl( fd_control, EM8300_IOCTL_SET_PLAYMODE, &ioval ) < 0 )
	printf( "AO: [dxr3] Unable to resume playback\n" );
    close( fd_control );
  }
}

// return: how many bytes can be played without blocking
static int get_space()
{
    int space = 0;
    if( ioctl(fd_audio, SNDCTL_DSP_GETODELAY, &space) < 0 )
    {
        printf( "AO: [dxr3] Unable to get unplayed bytes in buffer\n" );
	return ao_data.outburst;
    }
    space = ao_data.buffersize - space;
    return space;
}

static int play(void* data,int len,int flags)
{
    if( ioctl( fd_audio, EM8300_IOCTL_AUDIO_SETPTS, &ao_data.pts ) < 0 )
	printf( "AO: [dxr3] Unable to set pts\n" );
    return write(fd_audio,data,len);
}

// return: how many unplayed bytes are in the buffer
static float get_delay()
{
    int r=0;
    if( ioctl(fd_audio, SNDCTL_DSP_GETODELAY, &r) < 0 )
    {
        printf( "AO: [dxr3] Unable to get unplayed bytes in buffer\n" );
	return ((float)ao_data.buffersize)/(float)ao_data.bps;
    }
    return (((float)r)/(float)ao_data.bps);
}

