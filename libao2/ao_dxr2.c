#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <inttypes.h>

#include "../config.h"
#include "mp_msg.h"
#include "bswap.h"

#include "audio_out.h"
#include "audio_out_internal.h"

#include "afmt.h"


static ao_info_t info =
{
	"DXR2 audio output",
	"dxr2",
	"Tobias Diedrich <ranma@gmx.at>",
	""
};

LIBAO_EXTERN(dxr2)

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  return CONTROL_UNKNOWN;
}

static int freq=0;
static int freq_id=0;

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

	ao_data.outburst=2048;
	ao_data.samplerate=rate;
	ao_data.channels=channels;
	ao_data.buffersize=2048;
	ao_data.bps=rate*4;
	ao_data.format=format;
	freq=rate;

	switch(rate){
	case 48000:
		freq_id=0;
		break;
	case 96000:
		freq_id=1;
		break;
	case 44100:
		freq_id=2;
		break;
	case 32000:
		freq_id=3;
		break;
	case 22050:
		freq_id=4;
		break;
#if 0
	case 24000:
		freq_id=5;
		break;
	case 64000:
		freq_id=6;
		break;
	case 88200:
		freq_id=7;
		break;
#endif
	default:
		mp_msg(MSGT_AO,MSGL_ERR,"[AO] dxr2: %d Hz not supported, try \"-aop list=resample\"\n",rate);
		return 0;
	}

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

extern void dxr2_send_packet(unsigned char* data,int len,int id,int timestamp);
extern void dxr2_send_lpcm_packet(unsigned char* data,int len,int id,int timestamp,int freq_id);
extern int vo_pts;
static int preload = 1;
// return: how many bytes can be played without blocking
static int get_space(){
    float x=(float)(vo_pts-ao_data.pts)/90000.0;
    int y;
    if(x<=0) return 0;
    y=freq*4*x;y/=ao_data.outburst;y*=ao_data.outburst;
    if(y>32768) y=32768;
    return y;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
  // MPEG and AC3 don't work :-(
    if(ao_data.format==AFMT_MPEG)
	dxr2_send_packet(data,len,0xC0,ao_data.pts);
    else if(ao_data.format==AFMT_AC3)
      	dxr2_send_packet(data,len,0x80,ao_data.pts);
    else {
	int i;
	//unsigned short *s=data;
	uint16_t *s=data;
#ifndef WORDS_BIGENDIAN
	for(i=0;i<len/2;i++) s[i] = bswap_16(s[i]); // (s[i]>>8)|(s[i]<<8); // le<->be bswap_16(s[i]);
#endif
	dxr2_send_lpcm_packet(data,len,0xA0,ao_data.pts-10000,freq_id);
    }
    return len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(){

    return 0.0;
}

