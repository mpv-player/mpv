#include <stdio.h>
#include <stdlib.h>

#include "audio_out.h"
#include "audio_out_internal.h"

static ao_info_t info = 
{
	"RAW PCM/WAVE file writer audio output",
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

char *ao_outputfilename = NULL;
int ao_pcm_waveheader = 1;

#define WAV_ID_RIFF 0x46464952 /* "RIFF" */
#define WAV_ID_WAVE 0x45564157 /* "WAVE" */
#define WAV_ID_FMT  0x20746d66 /* "fmt " */
#define WAV_ID_DATA 0x61746164 /* "data" */
#define WAV_ID_PCM  0x0001

struct WaveHeader
{
	unsigned long riff;
	unsigned long file_length;
	unsigned long wave;
	unsigned long fmt;
	unsigned long fmt_length;
	short fmt_tag;
	short channels;
	unsigned long sample_rate;
	unsigned long bytes_per_second;
	short block_align;
	short bits;
	unsigned long data;
	unsigned long data_length;
};


static struct WaveHeader wavhdr = {
	WAV_ID_RIFF,
	0x00000000,
	WAV_ID_WAVE,
	WAV_ID_FMT,
	16,
	WAV_ID_PCM,
	2,
	44100,
	192000,
	4,
	16,
	WAV_ID_DATA,
	0x00000000
};

static FILE *fp = NULL;

// to set/get/query special features/parameters
static int control(int cmd,int arg){
    return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){
	if(!ao_outputfilename) {
		ao_outputfilename = (char *) malloc(sizeof(char) * 14);
		strcpy(ao_outputfilename,(ao_pcm_waveheader ? "audiodump.wav" : "audiodump.pcm"));
	}
	
	wavhdr.channels = channels;
	wavhdr.sample_rate = rate;
	wavhdr.bytes_per_second = rate * (format / 8) * channels;
	wavhdr.bits = format;

	printf("PCM: File: %s (%s) Samplerate: %iHz Channels: %s Format %s\n", ao_outputfilename, (ao_pcm_waveheader?"WAVE":"RAW PCM"), rate, (channels > 1) ? "Stereo" : "Mono", audio_out_format_name(format));
	printf("PCM: Info - fastest dumping is achieved with -vo null -hardframedrop.\n");
	printf("PCM: Info - to write WAVE files use -waveheader (default), for RAW PCM -nowaveheader.\n");
	fp = fopen(ao_outputfilename, "wb");

	ao_outburst = 4096;


	if(fp) {
		if(ao_pcm_waveheader) /* Reserve space for wave header */
			fseek(fp, sizeof(wavhdr), SEEK_SET);
		return 1;
	}
	printf("PCM: Failed to open %s for writing!\n", ao_outputfilename);
	return 0;
}

// close audio device
static void uninit(){
	
	if(ao_pcm_waveheader){ /* Write wave header */
		wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr);
		fseek(fp, 0, SEEK_SET);
		fwrite(&wavhdr,sizeof(wavhdr),1,fp);
	}
	fclose(fp);
	free(ao_outputfilename);
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
	
	if(ao_pcm_waveheader)
		wavhdr.data_length += len;
	
	return len;
}

// return: how many unplayed bytes are in the buffer
static int get_delay(){

    return 0;
}






