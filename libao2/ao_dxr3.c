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
	"Deprecated"
};

LIBAO_EXTERN(dxr3)

static audio_buf_info dxr3_buf_info;
static int fd_control = 0, fd_audio = 0;
static int need_conversion = 0;

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
  printf( "AO: [dxr3] ERROR: Use of -ao dxr3 deprecated, use -ao oss:/dev/em8300_ma instead\n" );
  return 0;
}

// close audio device
static void uninit()
{
}

// stop playing and empty buffers (for seeking/pause)
static void reset()
{
}

// stop playing, keep buffers (for pause)
static void audio_pause()
{
}

// resume playing, after audio_pause()
static void audio_resume()
{
}

// return: how many bytes can be played without blocking
static int get_space()
{
    return 0;
}

// playes 'len' bytes of 'data'
// upsamples if samplerate < 44100
// return: number of bytes played
static int play(void* data,int len,int flags)
{
    return 0;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay()
{
    return 0.0;
}

