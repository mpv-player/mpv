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
#include "audio_plugin.h"

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
int need_conversion = 0;

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
    ao_plugin_data.rate = rate;
    ao_plugin_data.channels = channels;
    ao_plugin_data.format = format;
    ao_plugin_data.sz_mult = 1;
    ao_plugin_data.sz_fix = 0;
    ao_plugin_data.delay_mult = 1;
    ao_plugin_data.delay_fix = 0;
    ao_plugin_cfg.pl_format_type = format;
    ao_plugin_cfg.pl_resample_fout = rate;
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
    if(format != ao_data.format)
    {
	need_conversion |= 0x1;
	ao_data.format = AFMT_S16_LE;
	ao_plugin_data.format = format;
	ao_plugin_cfg.pl_format_type = ao_data.format;
    }
  
    ao_data.channels=channels;
    if(format != AFMT_AC3)
	if(channels>2)
	    if( ioctl (fd_audio, SNDCTL_DSP_CHANNELS, &ao_data.channels) < 0 )
		printf( "AO: [dxr3] Unable to set number of channels\n" );
    else
    {
	int c = channels-1;
	if( ioctl(fd_audio,SNDCTL_DSP_STEREO,&c) < 0)
	    printf( "AO: [dxr3] Unable to set number of channels for AC3\n" );
    }
 
    ao_data.bps = channels*rate;
    if(format != AFMT_U8 && format != AFMT_S8)
	ao_data.bps*=2;
    ao_data.samplerate=rate;
    if( ioctl (fd_audio, SNDCTL_DSP_SPEED, &ao_data.samplerate) < 0 )
    {
        printf( "AO: [dxr3] Unable to set samplerate\n" );
        return 0;
    }
    if( rate != ao_data.samplerate )
    {
	need_conversion |= 0x2;
	ao_plugin_data.rate = rate;
	ao_plugin_cfg.pl_resample_fout = ao_data.samplerate;
    }

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
      ao_data.buffersize=(dxr3_buf_info.bytes/2);
      ao_data.outburst=dxr3_buf_info.fragsize;
  }

  if(need_conversion)
  {
    
    if(need_conversion & 0x1)
    {
	if(!audio_plugin_format.init())
	    return 0;
	ao_plugin_data.len = ao_data.buffersize*2;
	audio_plugin_format.control(AOCONTROL_PLUGIN_SET_LEN,0);
    }
    if(need_conversion & 0x2)
    {
	if(!audio_plugin_resample.init())
	    return 0;
	ao_plugin_data.len = ao_data.buffersize*2;
	audio_plugin_resample.control(AOCONTROL_PLUGIN_SET_LEN,0);
    }
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
    if(need_conversion & 0x1) audio_plugin_format.uninit();
    if(need_conversion & 0x2) audio_plugin_resample.uninit();
    close( fd_audio );
    close( fd_control ); /* Just in case */
}

// stop playing and empty buffers (for seeking/pause)
static void reset()
{
    if( ioctl(fd_audio, SNDCTL_DSP_RESET, NULL) < 0 )
	printf( "AO: [dxr3] Unable to reset device\n" );
    if(need_conversion & 0x1) audio_plugin_format.reset();
    if(need_conversion & 0x2) audio_plugin_resample.reset();
}

// stop playing, keep buffers (for pause)
static void audio_pause()
{
  int ioval;
  reset();
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
    space /= ao_data.outburst; /* This is a smart way of doing a fast modulo reduction */
    space *= ao_data.outburst; /* fetched from ao_mpegpes.c */
    return space;
}

// playes 'len' bytes of 'data'
// upsamples if samplerate < 44100
// return: number of bytes played
static int play(void* data,int len,int flags)
{
    int tmp = get_space();
    int size = (tmp<len)?tmp:len;
    ao_plugin_data.data = data;
    ao_plugin_data.len = size;
    if(need_conversion & 0x1) audio_plugin_format.play();
    if(need_conversion & 0x2) audio_plugin_resample.play();
    write(fd_audio,ao_plugin_data.data,ao_plugin_data.len);
    return size;
}

// return: delay in seconds between first and last sample in buffer
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

