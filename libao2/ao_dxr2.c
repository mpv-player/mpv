#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <dxr2ioctl.h>
#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "libavutil/common.h"
#include "mpbswap.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include "libmpdemux/mpeg_packetizer.h"


static ao_info_t info =
{
	"DXR2 audio output",
	"dxr2",
	"Tobias Diedrich <ranma+mplayer@tdiedrich.de>",
	""
};

LIBAO_EXTERN(dxr2)

static int volume=19;
static int last_freq_id = -1;
extern int dxr2_fd;

// to set/get/query special features/parameters
static int control(int cmd,void *arg){
  switch(cmd){
  case AOCONTROL_GET_VOLUME:
    if(dxr2_fd > 0) {
      ao_control_vol_t* vol = (ao_control_vol_t*)arg;
      vol->left = vol->right = volume * 19.0 / 100.0;
      return CONTROL_OK;
    }
    return CONTROL_ERROR;
  case AOCONTROL_SET_VOLUME:
    if(dxr2_fd > 0) {
      dxr2_oneArg_t v;
      float diff;
      ao_control_vol_t* vol = (ao_control_vol_t*)arg;
      // We need this trick because the volume stepping is often too small
      diff = ((vol->left+vol->right) / 2 - (volume*19.0/100.0)) * 19.0 / 100.0;
      v.arg = volume + (diff > 0 ? ceil(diff) : floor(diff)); 
      if(v.arg > 19) v.arg = 19;
      if(v.arg < 0) v.arg = 0;
      if(v.arg != volume) {
	volume = v.arg;
	if( ioctl(dxr2_fd,DXR2_IOC_SET_AUDIO_VOLUME,&v) < 0) {
	  mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_DXR2_SetVolFailed,volume);
	  return CONTROL_ERROR;
	}
      }
      return CONTROL_OK;
    }
    return CONTROL_ERROR;
  }
  return CONTROL_UNKNOWN;
}

static int freq=0;
static int freq_id=0;

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

	if(dxr2_fd <= 0)
	  return 0;

        last_freq_id = -1;
        
	ao_data.outburst=2048;
	ao_data.samplerate=rate;
	ao_data.channels=channels;
	ao_data.buffersize=2048;
	ao_data.bps=rate*4;
	ao_data.format=format;
	freq=rate;

	switch(rate){
	case 48000:
		freq_id=DXR2_AUDIO_FREQ_48;
		break;
	case 96000:
		freq_id=DXR2_AUDIO_FREQ_96;
		break;
	case 44100:
		freq_id=DXR2_AUDIO_FREQ_441;
		break;
	case 32000:
		freq_id=DXR2_AUDIO_FREQ_32;
		break;
	case 22050:
		freq_id=DXR2_AUDIO_FREQ_2205;
		break;
#ifdef DXR2_AUDIO_FREQ_24
	// This is not yet in the dxr2 driver CVS
	// you can get the patch at
	// http://www.tdiedrich.de/~ranma/patches/dxr2.pcm1723.20020513
	case 24000:
		freq_id=DXR2_AUDIO_FREQ_24;
		break;
	case 64000:
		freq_id=DXR2_AUDIO_FREQ_64;
		break;
	case 88200:
		freq_id=DXR2_AUDIO_FREQ_882;
		break;
#endif
	default:
		mp_msg(MSGT_AO,MSGL_ERR,MSGTR_AO_DXR2_UnsupSamplerate,rate);
		return 0;
	}

	return 1;
}

// close audio device
static void uninit(int immed){

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

extern int vo_pts;
// return: how many bytes can be played without blocking
static int get_space(void){
    float x=(float)(vo_pts-ao_data.pts)/90000.0;
    int y;
    if(x<=0) return 0;
    y=freq*4*x;y/=ao_data.outburst;y*=ao_data.outburst;
    if(y>32768) y=32768;
    return y;
}

static void dxr2_send_lpcm_packet(unsigned char* data,int len,int id,unsigned int timestamp,int freq_id)
{
  extern int write_dxr2(unsigned char *data, int len);
  
  if(dxr2_fd < 0) {
    mp_msg(MSGT_AO,MSGL_ERR,"DXR2 fd is not valid\n");
    return;
  }    

  if(last_freq_id != freq_id) {
    ioctl(dxr2_fd, DXR2_IOC_SET_AUDIO_SAMPLE_FREQUENCY, &freq_id);
    last_freq_id = freq_id;
  }

  send_mpeg_lpcm_packet (data, len, id, timestamp, freq_id, write_dxr2);
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
  extern int write_dxr2(unsigned char *data, int len);

  // MPEG and AC3 don't work :-(
    if(ao_data.format==AF_FORMAT_MPEG2)
      send_mpeg_ps_packet (data, len, 0xC0, ao_data.pts, 2, write_dxr2);
    else if(ao_data.format==AF_FORMAT_AC3)
      send_mpeg_ps_packet (data, len, 0x80, ao_data.pts, 2, write_dxr2);
    else {
	int i;
	//unsigned short *s=data;
	uint16_t *s=data;
#ifndef WORDS_BIGENDIAN
	for(i=0;i<len/2;i++) s[i] = bswap_16(s[i]);
#endif
	dxr2_send_lpcm_packet(data,len,0xA0,ao_data.pts-10000,freq_id);
    }
    return len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void){

    return 0.0;
}

