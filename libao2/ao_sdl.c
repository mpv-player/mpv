/* 
 * ao_sdl.c - libao2 SDLlib Audio Output Driver for MPlayer
 *
 * This driver is under the same license as MPlayer.
 * (http://mplayer.sf.net)
 *
 * Copyleft 2001 by Felix Bünemann (atmosfear@users.sf.net)
 *
 * Thanks to Arpi for nice ringbuffer-code!
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"
#include <SDL.h>

#include "../libvo/fastmemcpy.h"

static ao_info_t info = 
{
	"SDLlib audio output",
	"sdl",
	"Felix Buenemann <atmosfear@users.sourceforge.net>",
	""
};

LIBAO_EXTERN(sdl)


extern int verbose;

// Samplesize used by the SDLlib AudioSpec struct
#define SAMPLESIZE 1024

// General purpose Ring-buffering routines

#define BUFFSIZE 4096
#define NUM_BUFS 16 

static unsigned char *buffer[NUM_BUFS];

static unsigned int buf_read=0;
static unsigned int buf_write=0;
static unsigned int buf_read_pos=0;
static unsigned int buf_write_pos=0;
static unsigned int volume=127;
static int full_buffers=0;
static int buffered_bytes=0;


static int write_buffer(unsigned char* data,int len){
  int len2=0;
  int x;
  while(len>0){
    if(full_buffers==NUM_BUFS) break;
    x=BUFFSIZE-buf_write_pos;
    if(x>len) x=len;
    memcpy(buffer[buf_write]+buf_write_pos,data+len2,x);
    len2+=x; len-=x;
    buffered_bytes+=x; buf_write_pos+=x;
    if(buf_write_pos>=BUFFSIZE){
       // block is full, find next!
       buf_write=(buf_write+1)%NUM_BUFS;
       ++full_buffers;
       buf_write_pos=0;
    }
  }
  return len2;
}

static int read_buffer(unsigned char* data,int len){
  int len2=0;
  int x;
  while(len>0){
    if(full_buffers==0) break; // no more data buffered!
    x=BUFFSIZE-buf_read_pos;
    if(x>len) x=len;
    memcpy(data+len2,buffer[buf_read]+buf_read_pos,x);
    SDL_MixAudio(data+len2, data+len2, x, volume);
    len2+=x; len-=x;
    buffered_bytes-=x; buf_read_pos+=x;
    if(buf_read_pos>=BUFFSIZE){
       // block is empty, find next!
       buf_read=(buf_read+1)%NUM_BUFS;
       --full_buffers;
       buf_read_pos=0;
    }
  }
  return len2;
}

// end ring buffer stuff

#if	 defined(HPUX) || defined(sun) && defined(__svr4__)
/* setenv is missing on solaris and HPUX */
static void setenv(const char *name, const char *val, int _xx)
{
  int len  = strlen(name) + strlen(val) + 2;
  char *env = malloc(len);

  if (env != NULL) {
    strcpy(env, name);
    strcat(env, "=");
    strcat(env, val);
    putenv(env);
  }
}
#endif


// to set/get/query special features/parameters
static int control(int cmd,int arg){
	switch (cmd) {
		case AOCONTROL_GET_VOLUME:
		{
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			vol->left = vol->right = (float)((volume + 127)/2.55);
			return CONTROL_OK;
		}
		case AOCONTROL_SET_VOLUME:
		{
			float diff;
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			diff = (vol->left+vol->right) / 2;
			volume = (int)(diff * 2.55) - 127;
			return CONTROL_OK;
		}
	}
	return -1;
}

// SDL Callback function
void outputaudio(void *unused, Uint8 *stream, int len) {
	//SDL_MixAudio(stream, read_buffer(buffers, len), len, SDL_MIX_MAXVOLUME);
	//if(!full_buffers) printf("SDL: Buffer underrun!\n");

	read_buffer(stream, len);
	//printf("SDL: Full Buffers: %i\n", full_buffers);
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

	/* SDL Audio Specifications */
	SDL_AudioSpec aspec;
	
	int i;
	/* Allocate ring-buffer memory */
	for(i=0;i<NUM_BUFS;i++) buffer[i]=(unsigned char *) malloc(BUFFSIZE);

	printf("SDL: Samplerate: %iHz Channels: %s Format %s\n", rate, (channels > 1) ? "Stereo" : "Mono", audio_out_format_name(format));

	if(ao_subdevice) {
		setenv("SDL_AUDIODRIVER", ao_subdevice, 1);
		printf("SDL: using %s audio driver\n", ao_subdevice);
	}

	ao_data.bps=channels*rate;
	if(format != AFMT_U8 && format != AFMT_S8)
	  ao_data.bps*=2;
	
	/* The desired audio format (see SDL_AudioSpec) */
	switch(format) {
	    case AFMT_U8:
		aspec.format = AUDIO_U8;
	    break;
	    case AFMT_S16_LE:
		aspec.format = AUDIO_S16LSB;
	    break;
	    case AFMT_S16_BE:
		aspec.format = AUDIO_S16MSB;
	    break;
	    case AFMT_S8:
		aspec.format = AUDIO_S8;
	    break;
	    case AFMT_U16_LE:
		aspec.format = AUDIO_U16LSB;
	    break;
	    case AFMT_U16_BE:
		aspec.format = AUDIO_U16MSB;
	    break;
	    default:
                printf("SDL: Unsupported audio format: 0x%x.\n", format);
                return 0;
	}

	/* The desired audio frequency in samples-per-second. */
	aspec.freq     = rate;

	/* Number of channels (mono/stereo) */
	aspec.channels = channels;

	/* The desired size of the audio buffer in samples. This number should be a power of two, and may be adjusted by the audio driver to a value more suitable for the hardware. Good values seem to range between 512 and 8192 inclusive, depending on the application and CPU speed. Smaller values yield faster response time, but can lead to underflow if the application is doing heavy processing and cannot fill the audio buffer in time. A stereo sample consists of both right and left channels in LR ordering. Note that the number of samples is directly related to time by the following formula: ms = (samples*1000)/freq */
	aspec.samples  = SAMPLESIZE;

	/* This should be set to a function that will be called when the audio device is ready for more data. It is passed a pointer to the audio buffer, and the length in bytes of the audio buffer. This function usually runs in a separate thread, and so you should protect data structures that it accesses by calling SDL_LockAudio and SDL_UnlockAudio in your code. The callback prototype is:
void callback(void *userdata, Uint8 *stream, int len); userdata is the pointer stored in userdata field of the SDL_AudioSpec. stream is a pointer to the audio buffer you want to fill with information and len is the length of the audio buffer in bytes. */
	aspec.callback = outputaudio;

	/* This pointer is passed as the first parameter to the callback function. */
	aspec.userdata = NULL;

	/* initialize the SDL Audio system */
        if (SDL_Init (SDL_INIT_AUDIO/*|SDL_INIT_NOPARACHUTE*/)) {
                printf("SDL: Initializing of SDL Audio failed: %s.\n", SDL_GetError());
                return 0;
        }

	/* Open the audio device and start playing sound! */
	if(SDL_OpenAudio(&aspec, NULL) < 0) {
        	printf("SDL: Unable to open audio: %s\n", SDL_GetError());
        	return(0);
	} 
	
	if(verbose) printf("SDL: buf size = %d\n",aspec.size);
	if(ao_data.buffersize==-1) ao_data.buffersize=aspec.size;
	
	/* unsilence audio, if callback is ready */
	SDL_PauseAudio(0);

	return 1;
}

// close audio device
static void uninit(){
	if(verbose) printf("SDL: Audio Subsystem shutting down!\n");
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(){

	//printf("SDL: reset called!\n");	

	/* Reset ring-buffer state */
	buf_read=0;
	buf_write=0;
	buf_read_pos=0;
	buf_write_pos=0;

	full_buffers=0;
	buffered_bytes=0;

}

// stop playing, keep buffers (for pause)
static void audio_pause()
{

	//printf("SDL: audio_pause called!\n");	
	SDL_LockAudio();
	
}

// resume playing, after audio_pause()
static void audio_resume()
{
	//printf("SDL: audio_resume called!\n");	
	SDL_UnlockAudio();
}


// return: how many bytes can be played without blocking
static int get_space(){
    return (NUM_BUFS-full_buffers)*BUFFSIZE - buf_write_pos;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){

#if 0	
	int ret;

	/* Audio locking prohibits call of outputaudio */
	SDL_LockAudio();
	// copy audio stream into ring-buffer 
	ret = write_buffer(data, len);
	SDL_UnlockAudio();

    	return ret;
#else
	return write_buffer(data, len);
#endif
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(){
    return (float)(buffered_bytes + ao_data.buffersize)/(float)ao_data.bps;
}






