#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <libdxr3/api.h>

#include "../config.h"

#include "afmt.h"

#include "audio_out.h"
#include "audio_out_internal.h"

struct 
{
    int ao_format;
    int ao_rate;
    int ao_channels;
} ao_device;

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

// to set/get/query special features/parameters
static int control(int cmd,int arg)
{
    switch(cmd)
    {
	case AOCONTROL_SET_DEVICE:
	    return CONTROL_OK;
	case AOCONTROL_QUERY_FORMAT:
	    return CONTROL_TRUE;
	case AOCONTROL_GET_VOLUME:
	case AOCONTROL_SET_VOLUME:
	{
    	    return CONTROL_OK;
	}
    }
    return CONTROL_TRUE;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags)
{
    ao_device.ao_format = format;
    ao_device.ao_rate = rate;
    ao_device.ao_channels = channels;
    
    if( dxr3_get_status() == DXR3_STATUS_CLOSED )
    {
	if( dxr3_open( "/dev/em8300", "/etc/dxr3.ux" ) != 0 ) printf( "Error loading /dev/em8300 with /etc/dxr3.ux microcode\n" );
	printf( "DXR3 status: &s\n", dxr3_get_status() ? "opened":"closed" );
    }
    else
	printf( "DXR3 already open\n" );

    if( dxr3_set_playmode( DXR3_PLAYMODE_PLAY ) != 0 ) printf( "Error setting playmode of DXR3\n" );    
    
    if( format == AFMT_AC3 )
    {
	if( dxr3_audio_set_mode( DXR3_AUDIOMODE_DIGITALAC3 ) != 0 )
	{
	    printf( "Cannot set DXR3 to AC3 playback!\n" );
	    return -1;
	}
    }
    else
    {
	if( dxr3_audio_set_mode( DXR3_AUDIOMODE_ANALOG ) != 0 )
	{
	    printf( "Cannot set DXR3 to analog playback!\n" );
	    return -1;
	}
	if( format == AFMT_U8 )
	    dxr3_audio_set_samplesize( 8 );
	else if( format == AFMT_S16_LE )
	    dxr3_audio_set_samplesize( 16 );	
	else
	{
	    printf( "Unsupported audio format\n" );
	    return -1;
	}
    }
    
    dxr3_audio_set_stereo( (channels > 1) ? "true":"false" );
    dxr3_audio_set_rate( rate );

    return 1;
}

// close audio device
static void uninit()
{
    dxr3_close();
}

// stop playing and empty buffers (for seeking/pause)
static void reset()
{
    uninit();
    if( !init( ao_device.ao_rate, ao_device.ao_channels, ao_device.ao_format, 0 ) )
	printf("\nFatal error: *** CANNOT RE-OPEN / RESET AUDIO DEVICE ***\n");
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
    return dxr3_audio_get_buffersize()-dxr3_audio_get_bytesleft();
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags)
{
    if(len)
        if( ao_device.ao_format == AFMT_AC3 )
	    return dxr3_audio_write_ac3( data, len );
	else
    	    return dxr3_audio_write_ac3( data, len );

    printf( "Invalid audio data\n" );
    return 0;
}

// return: how many unplayed bytes are in the buffer
static int get_delay()
{
    return dxr3_audio_get_bytesleft();
}

