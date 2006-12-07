#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libavutil/common.h"
#include "mpbswap.h"
#include "subopt-helper.h"
#include "libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "mp_msg.h"
#include "help_mp.h"


static ao_info_t info = 
{
	"RAW PCM/WAVE file writer audio output",
	"pcm",
	"Atmosfear",
	""
};

LIBAO_EXTERN(pcm)

extern int vo_pts;

static char *ao_outputfilename = NULL;
static int ao_pcm_waveheader = 1;
static int fast = 0;

#define WAV_ID_RIFF 0x46464952 /* "RIFF" */
#define WAV_ID_WAVE 0x45564157 /* "WAVE" */
#define WAV_ID_FMT  0x20746d66 /* "fmt " */
#define WAV_ID_DATA 0x61746164 /* "data" */
#define WAV_ID_PCM  0x0001

struct WaveHeader
{
	uint32_t riff;
	uint32_t file_length;
	uint32_t wave;
	uint32_t fmt;
	uint32_t fmt_length;
	uint16_t fmt_tag;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t bytes_per_second;
	uint16_t block_align;
	uint16_t bits;
	uint32_t data;
	uint32_t data_length;
};

/* init with default values */
static struct WaveHeader wavhdr;

static FILE *fp = NULL;

// to set/get/query special features/parameters
static int control(int cmd,void *arg){
    return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){
	int bits;
	opt_t subopts[] = {
	  {"waveheader", OPT_ARG_BOOL, &ao_pcm_waveheader, NULL},
	  {"file",       OPT_ARG_MSTRZ, &ao_outputfilename, NULL},
	  {"fast",       OPT_ARG_BOOL, &fast, NULL},
	  {NULL}
	};
	// set defaults
	ao_pcm_waveheader = 1;

	if (subopt_parse(ao_subdevice, subopts) != 0) {
	  return 0;
	}
	if (!ao_outputfilename){
	  ao_outputfilename =
	    strdup(ao_pcm_waveheader?"audiodump.wav":"audiodump.pcm");
	}

	/* bits is only equal to format if (format == 8) or (format == 16);
	   this means that the following "if" is a kludge and should
	   really be a switch to be correct in all cases */

	bits=8;
	switch(format){
	case AF_FORMAT_S8:
	    format=AF_FORMAT_U8;
	case AF_FORMAT_U8:
	    break;
	default:
	    format=AF_FORMAT_S16_LE;
	    bits=16;
	    break;
	}

	ao_data.outburst = 65536;
	ao_data.buffersize= 2*65536;
	ao_data.channels=channels;
	ao_data.samplerate=rate;
	ao_data.format=format;
	ao_data.bps=channels*rate*(bits/8);

	wavhdr.riff = le2me_32(WAV_ID_RIFF);
	wavhdr.wave = le2me_32(WAV_ID_WAVE);
	wavhdr.fmt = le2me_32(WAV_ID_FMT);
	wavhdr.fmt_length = le2me_32(16);
	wavhdr.fmt_tag = le2me_16(WAV_ID_PCM);
	wavhdr.channels = le2me_16(ao_data.channels);
	wavhdr.sample_rate = le2me_32(ao_data.samplerate);
	wavhdr.bytes_per_second = le2me_32(ao_data.bps);
	wavhdr.bits = le2me_16(bits);
	wavhdr.block_align = le2me_16(ao_data.channels * (bits / 8));
	
	wavhdr.data = le2me_32(WAV_ID_DATA);
	wavhdr.data_length=le2me_32(0x7ffff000);
	wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;

	mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_PCM_FileInfo, ao_outputfilename, 
	       (ao_pcm_waveheader?"WAVE":"RAW PCM"), rate, 
	       (channels > 1) ? "Stereo" : "Mono", af_fmt2str_short(format));
	mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_PCM_HintInfo);

	fp = fopen(ao_outputfilename, "wb");
	if(fp) {
		if(ao_pcm_waveheader){ /* Reserve space for wave header */
			fwrite(&wavhdr,sizeof(wavhdr),1,fp);
			wavhdr.file_length=wavhdr.data_length=0;
		}
		return 1;
	}
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_PCM_CantOpenOutputFile, 
               ao_outputfilename);
	return 0;
}

// close audio device
static void uninit(int immed){
	
	if(ao_pcm_waveheader && fseek(fp, 0, SEEK_SET) == 0){ /* Write wave header */
		wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;
		wavhdr.file_length = le2me_32(wavhdr.file_length);
		wavhdr.data_length = le2me_32(wavhdr.data_length);
		fwrite(&wavhdr,sizeof(wavhdr),1,fp);
	}
	fclose(fp);
	if (ao_outputfilename)
	  free(ao_outputfilename);
	ao_outputfilename = NULL;
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){

}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
}

// return: how many bytes can be played without blocking
static int get_space(void){

    if(vo_pts)
      return ao_data.pts < vo_pts + fast * 30000 ? ao_data.outburst : 0;
    return ao_data.outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){

// let libaf to do the conversion...
#if 0
//#ifdef WORDS_BIGENDIAN
	if (ao_data.format == AFMT_S16_LE) {
	  unsigned short *buffer = (unsigned short *) data;
	  register int i;
	  for(i = 0; i < len/2; ++i) {
	    buffer[i] = le2me_16(buffer[i]);
	  }
	}
#endif 

	//printf("PCM: Writing chunk!\n");
	fwrite(data,len,1,fp);

	if(ao_pcm_waveheader)
		wavhdr.data_length += len;
	
	return len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void){

    return 0.0;
}






