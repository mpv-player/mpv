#include <stdio.h>
#include <stdlib.h>

#include "audio_out.h"
#include "audio_out_internal.h"

static ao_info_t info = 
{
	"PCM writer audio output",
	"pcm",
	"Atmosfear",
	""
};

LIBAO_EXTERN(pcm)

// there are some globals:
// ao_samplerate
// ao_channels
// ao_format
// ao_bps
// ao_outburst
// ao_buffersize

static FILE *fp = NULL;

// to set/get/query special features/parameters
static int control(int cmd,int arg){
    return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){
	
	printf("PCM: File: audiodump.pcm Samplerate: %iHz Channels: %s Format %s\n", rate, (channels > 1) ? "Stereo" : "Mono", audio_out_format_name(format));
	printf("PCM: Info - fastest dumping is achieved with -vo null -hardframedrop.\n");
	fp = fopen("audiodump.pcm", "wb");

	ao_outburst = 4096;


	if(fp) return 1;
	printf("PCM: Failed to open audiodump.pcm for writing!\n");
	return 0;
}

// close audio device
static void uninit(){
	fclose(fp);
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

	//printf("PCM: Writing chunk!\n");
	fwrite(data,len,1,fp);
	
	return len;
}

// return: how many unplayed bytes are in the buffer
static int get_delay(){

    return 0;
}






