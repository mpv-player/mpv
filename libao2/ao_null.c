#include <stdio.h>
#include <stdlib.h>

#include "audio_out.h"
#include "audio_out_internal.h"

static ao_info_t info = 
{
	"Null audio output",
	"null",
	"A'rpi",
	""
};

LIBAO_EXTERN(null)

// there are some globals:
// ao_samplerate
// ao_channels
// ao_format
// ao_bps
// ao_outburst
// ao_buffersize

// to set/get/query special features/parameters
static int control(int cmd,int arg){
    return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

    ao_outburst=4096;

    return 1;
}

// close audio device
static void uninit(){

}

// stop playing and empty buffers (for seeking/pause)
static void reset(){

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

    return ao_outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){

    return len;
}

// return: how many unplayed bytes are in the buffer
static int get_delay(){

    return 0;
}






