#include <stdio.h>
#include <stdlib.h>

#include "audio_out.h"
#include "audio_out_internal.h"

#include "afmt.h"

static ao_info_t info = 
{
	"mpeg-pes audio output",
	"mpegpes",
	"A'rpi",
	""
};

LIBAO_EXTERN(mpegpes)

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

    ao_outburst=2000;
    ao_format=format;

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

void send_pes_packet(unsigned char* data,int len,int id,int timestamp);
void send_lpcm_packet(unsigned char* data,int len,int id,int timestamp);
extern int vo_pts;

// return: how many bytes can be played without blocking
static int get_space(){
    float x=(float)(vo_pts-ao_pts)/90000.0-0.5;
    int y;
    if(x<=0) return 0;
    y=48000*4*x;y/=ao_outburst;y*=ao_outburst;
//    printf("diff: %5.3f -> %d  \n",x,y);
    return y;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
    if(ao_format==AFMT_MPEG)
	send_pes_packet(data,len,0x1C0,ao_pts);
    else {
	int i;
	unsigned short *s=data;
	for(i=0;i<len/2;i++) s[i]=(s[i]>>8)|(s[i]<<8); // le<->be
	send_lpcm_packet(data,len,0xA0,ao_pts);
    }
    return len;
}

// return: how many unplayed bytes are in the buffer
static int get_delay(){

    return 0;
}

