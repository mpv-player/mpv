#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libdxr3/api.h>

#include "../config.h"

#include "afmt.h"

#include "audio_out.h"
#include "audio_out_internal.h"

extern int verbose;

static ao_info_t info = 
{
	"DXR3/H+ audio out",
	"dxr3",
	"David Holm <dholm@iname.com>",
	""
};

LIBAO_EXTERN(dxr3)

// there are some globals:
// ao_samplerate
// ao_channels
// ao_format
// ao_bps
// ao_outburst
// ao_buffersize

static char *em8300_ma="/dev/em8300_ma";
static audio_buf_info dxr3_buf_info;

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
  if( dxr3_get_status() == DXR3_STATUS_CLOSED ) 
    if( dxr3_open( "/dev/em8300" ) < 0 )
    {
	printf( "Failed to initialize the DXR3\n" );
	return 0;
    }
  
  if( dxr3_audio_get_filedescriptor( ) < 0 )
  {
    printf("Can't open audio device %s  -> nosound\n",em8300_ma);
    return 0;
  }
  
  ao_format = format;
  ioctl (dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_SETFMT, &ao_format);
  if(format == AFMT_AC3 && ao_format != AFMT_AC3) 
  {
      printf("Can't set audio device %s to AC3 output\n", em8300_ma);
      return 0;
  }
  printf("audio_setup: sample format: %s (requested: %s)\n",
    audio_out_format_name(ao_format), audio_out_format_name(format));
  
  if(format != AFMT_AC3) 
  {
	ao_channels=channels-1;
        ioctl (dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_STEREO, &ao_channels);
  
	// set rate
	ao_samplerate=rate;
	ioctl (dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_SPEED, &ao_samplerate);
	printf("audio_setup: using %d Hz samplerate (requested: %d)\n",ao_samplerate,rate);
  }

  if(ioctl(dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_GETOSPACE, &dxr3_buf_info)==-1){
      int r=0;
      printf("audio_setup: driver doesn't support SNDCTL_DSP_GETOSPACE :-(\n");
      if(ioctl(dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_GETBLKSIZE, &r)==-1){
          printf("audio_setup: %d bytes/frag (config.h)\n",ao_outburst);
      } else { 
          ao_outburst=r;
          printf("audio_setup: %d bytes/frag (GETBLKSIZE)\n",ao_outburst);
      }
  } else {
      printf("audio_setup: frags: %3d/%d  (%d bytes/frag)  free: %6d\n",
          dxr3_buf_info.fragments, dxr3_buf_info.fragstotal, dxr3_buf_info.fragsize, dxr3_buf_info.bytes);
      if(ao_buffersize==-1) ao_buffersize=dxr3_buf_info.bytes;
      ao_outburst=dxr3_buf_info.fragsize;
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
      FD_ZERO(&rfds); FD_SET(dxr3_audio_get_filedescriptor( ),&rfds);
      tv.tv_sec=0; tv.tv_usec = 0;
      if(!select(dxr3_audio_get_filedescriptor( )+1, NULL, &rfds, NULL, &tv)) break;
      write(dxr3_audio_get_filedescriptor( ),data,ao_outburst);
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
static void uninit()
{
    ioctl(dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_RESET, NULL);
    dxr3_close( );
}

// stop playing and empty buffers (for seeking/pause)
static void reset()
{
    uninit();
    if(dxr3_audio_get_filedescriptor( )<0)
    {
	printf("\nFatal error: *** CANNOT RE-OPEN / RESET AUDIO DEVICE ***\n");
	return;
    }

  ioctl (dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_SETFMT, &ao_format);
  if(ao_format != AFMT_AC3) 
  {
    ioctl (dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_STEREO, &ao_channels);
    ioctl (dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_SPEED, &ao_samplerate);
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
static int get_space()
{
  if(ioctl(dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_GETOSPACE, &dxr3_buf_info)!=-1)
        return (dxr3_buf_info.fragments*dxr3_buf_info.fragsize);
}

static int play(void* data,int len,int flags)
{
    return write(dxr3_audio_get_filedescriptor( ),data,len);;
}

// return: how many unplayed bytes are in the buffer
static int get_delay()
{
      int r=0;
      if(ioctl(dxr3_audio_get_filedescriptor( ), SNDCTL_DSP_GETODELAY, &r)!=-1)
         return r;
}

