#include <stdio.h>
#include <stdlib.h>

#include "audio_out.h"
#include "audio_out_internal.h"

static ao_info_t info = 
{
	"SDLlib audio output",
	"sdl",
	"Felix Buenemann <atmosfear@users.sourceforge.net>",
	""
};

LIBAO_EXTERN(sdl)

// there are some globals:
// ao_samplerate
// ao_channels
// ao_format
// ao_bps
// ao_outburst
// ao_buffersize

int audiolen = 0;
int audioplayed = 0;
int audiobuffer = 0;

#ifdef __FreeBSD__
#include <SDL11/SDL.h>
#else
#include <SDL/SDL.h>
#endif

/*

typedef struct{
  int freq;
  Uint16 format;
  Uint8 channels;
  Uint8 silence;
  Uint16 samples;
  Uint32 size;
  void (*callback)(void *userdata, Uint8 *stream, int len);
  void *userdata;
} SDL_AudioSpec;

*/

//static struct sdl_priv_s {

	/* SDL Audio Specifications */
	SDL_AudioSpec aspec;

//} sdl_priv;

// to set/get/query special features/parameters
static int control(int cmd,int arg){
    return -1;
}

// Callback function
void mixaudio(void *datastream, Uint8 *stream, int len) {
	//printf("SDL: mixaudio called!\n");
	//printf("SDL pts: %u %u\n", aspec.userdata, stream);
	if(audiolen == 0) return;
	len = (len > audiolen ? audiolen : len);
	SDL_MixAudio(stream, aspec.userdata, len, SDL_MIX_MAXVOLUME);
	audiobuffer -= len;
	audioplayed = len;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

	printf("SDL: Audio Out - This driver is early alpha, do not use!\n");
	printf("SDL: rate: %iHz channels %i format %i Bit flags %i\n", rate, channels, format, flags);
//    ao_outburst=4096;
	
	/* The desired audio frequency in samples-per-second. */
	aspec.freq     = rate;

	/* The desired audio format (see SDL_AudioSpec) */
	aspec.format   = (format == 16) ? AUDIO_S16 : AUDIO_S8;

	/* Number of channels (mono/stereo) */
	aspec.channels = channels;

	/* The desired size of the audio buffer in samples. This number should be a power of two, and may be adjusted by the audio driver to a value more suitable for the hardware. Good values seem to range between 512 and 8192 inclusive, depending on the application and CPU speed. Smaller values yield faster response time, but can lead to underflow if the application is doing heavy processing and cannot fill the audio buffer in time. A stereo sample consists of both right and left channels in LR ordering. Note that the number of samples is directly related to time by the following formula: ms = (samples*1000)/freq */
	aspec.samples  = 4096;

	/* This should be set to a function that will be called when the audio device is ready for more data. It is passed a pointer to the audio buffer, and the length in bytes of the audio buffer. This function usually runs in a separate thread, and so you should protect data structures that it accesses by calling SDL_LockAudio and SDL_UnlockAudio in your code. The callback prototype is:
void callback(void *userdata, Uint8 *stream, int len); userdata is the pointer stored in userdata field of the SDL_AudioSpec. stream is a pointer to the audio buffer you want to fill with information and len is the length of the audio buffer in bytes. */
	aspec.callback = mixaudio;

	/* This pointer is passed as the first parameter to the callback function. */
	aspec.userdata = NULL;

	/* Open the audio device and start playing sound! */
	if(SDL_OpenAudio(&aspec, NULL) < 0) {
        	printf("SDL: Unable to open audio: %s\n", SDL_GetError());
        	return(0);
	} 
	
	/* unsilence audio, if callback is ready */
	SDL_PauseAudio(0);
	
	


	return 1;
}

// close audio device
static void uninit(){
	/* Wait for sound to complete */
	while ( audiolen > 0 ) {
		SDL_Delay(100);         /* Sleep 1/10 second */
	}
	SDL_CloseAudio();
}

// stop playing and empty buffers (for seeking/pause)
static void reset(){

}

// return: how many bytes can be played without blocking
static int get_space(){

    return aspec.samples;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
	
	//printf("SDL: play called!\n");
	
	audiolen = len;
	audiobuffer = len;

	SDL_LockAudio();
	// copy audio stream into mixaudio stream here
	aspec.userdata = data;
	//printf("SDL pt: %u %u\n", data, aspec.userdata);

	SDL_UnlockAudio();

    	return audioplayed;
}

// return: how many unplayed bytes are in the buffer
static int get_delay(){
	//printf("SDL: get_delay called (%i)!\n", audiobuffer);
    	return audiobuffer;
}






