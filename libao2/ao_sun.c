#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/audioio.h>
#ifdef	__svr4__
#include <stropts.h>
#endif

#include "../config.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"

static ao_info_t info = 
{
	"Sun audio output",
	"sun",
	"jk@tools.de",
	""
};

LIBAO_EXTERN(sun)


#ifndef	AUDIO_PRECISION_8
#define AUDIO_PRECISION_8  8
#define AUDIO_PRECISION_16 16
#endif


// there are some globals:
// ao_samplerate
// ao_channels
// ao_format
// ao_bps
// ao_outburst
// ao_buffersize

static char *dsp="/dev/audio";
static int queued_bursts = 0;
static int audio_fd=-1;

// convert an OSS audio format specification into a sun audio encoding
static int oss2sunfmt(int oss_format)
{
  switch (oss_format){
  case AFMT_MU_LAW:
    return AUDIO_ENCODING_ULAW;
  case AFMT_A_LAW:
    return AUDIO_ENCODING_ALAW;
  case AFMT_S16_LE:
    return AUDIO_ENCODING_LINEAR;
  case AFMT_U8:
    return AUDIO_ENCODING_LINEAR8;
#ifdef	AUDIO_ENCODING_DVI	// Missing on NetBSD...
  case AFMT_IMA_ADPCM:
    return AUDIO_ENCODING_DVI;
#endif
  default:
    return AUDIO_ENCODING_NONE;
  }
}

// to set/get/query special features/parameters
static int control(int cmd,int arg){
    switch(cmd){
	case AOCONTROL_SET_DEVICE:
	    dsp=(char*)arg;
	    return CONTROL_OK;
	case AOCONTROL_QUERY_FORMAT:
	    return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

  audio_info_t info;
  int byte_per_sec;

  printf("ao2: %d Hz  %d chans  0x%X\n",rate,channels,format);

  audio_fd=open(dsp, O_WRONLY);
  if(audio_fd<0){
    printf("Can't open audio device %s  -> nosound\n",dsp);
    return 0;
  }

  ioctl(audio_fd, AUDIO_DRAIN, 0);

  AUDIO_INITINFO(&info);
  info.play.encoding = oss2sunfmt(ao_format = format);
  info.play.precision = (format==AFMT_S16_LE? AUDIO_PRECISION_16:AUDIO_PRECISION_8);
  info.play.channels = ao_channels = channels;
  --ao_channels;
  info.play.sample_rate = ao_samplerate = rate;
  info.play.samples = 0;
  info.play.eof = 0;
  if(ioctl (audio_fd, AUDIO_SETINFO, &info)<0)
    printf("audio_setup: your card doesn't support %d channel, %s, %d Hz samplerate\n",channels,audio_out_format_name(format),rate);
  byte_per_sec = (channels * info.play.precision * rate);
  ao_outburst=byte_per_sec > 100000 ? 16384 : 8192;
  queued_bursts = 0;

  if(ao_buffersize==-1){
    // Measuring buffer size:
    void* data;
    ao_buffersize=0;
#ifdef HAVE_AUDIO_SELECT
    data=malloc(ao_outburst); memset(data,0,ao_outburst);
    while(ao_buffersize<0x40000){
      fd_set rfds;
      struct timeval tv;
      FD_ZERO(&rfds); FD_SET(audio_fd,&rfds);
      tv.tv_sec=0; tv.tv_usec = 0;
      if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) break;
      write(audio_fd,data,ao_outburst);
      ao_buffersize+=ao_outburst;
    }
    free(data);
    if(ao_buffersize==0){
        printf("\n   ***  Your audio driver DOES NOT support select()  ***\n");
          printf("Recompile mplayer with #undef HAVE_AUDIO_SELECT in config.h !\n\n");
        return 0;
    }
#ifdef	__svr4__
    // remove the 0 bytes from the above ao_buffersize measurement from the
    // audio driver's STREAMS queue
    ioctl(audio_fd, I_FLUSH, FLUSHW);
#endif
    ioctl(audio_fd, AUDIO_DRAIN, 0);
#endif
  }

    return 1;
}

// close audio device
static void uninit(){
    close(audio_fd);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(){
    audio_info_t info;

#ifdef	__svr4__
    // throw away buffered data in the audio driver's STREAMS queue
    ioctl(audio_fd, I_FLUSH, FLUSHW);
#endif
    uninit();
    audio_fd=open(dsp, O_WRONLY);
    if(audio_fd<0){
	printf("\nFatal error: *** CANNOT RE-OPEN / RESET AUDIO DEVICE ***\n");
	return;
    }

    ioctl(audio_fd, AUDIO_DRAIN, 0);

    AUDIO_INITINFO(&info);
    info.play.encoding = oss2sunfmt(ao_format);
    info.play.precision = (ao_format==AFMT_S16_LE? AUDIO_PRECISION_16:AUDIO_PRECISION_8);
    info.play.channels = ao_channels+1;
    info.play.sample_rate = ao_samplerate;
    info.play.samples = 0;
    info.play.eof = 0;
    ioctl (audio_fd, AUDIO_SETINFO, &info);
    queued_bursts = 0;
}

// stop playing, keep buffers (for pause)
static void audio_pause()
{
    struct audio_info info;
    AUDIO_INITINFO(&info);
    info.play.pause = 1;
    ioctl(audio_fd, AUDIO_SETINFO, &info);
}

// resume playing, after audio_pause()
static void audio_resume()
{
    struct audio_info info;
    AUDIO_INITINFO(&info);
    info.play.pause = 0;
    ioctl(audio_fd, AUDIO_SETINFO, &info);
}


// return: how many bytes can be played without blocking
static int get_space(){
  int playsize=ao_outburst;

    // check buffer
#ifdef HAVE_AUDIO_SELECT
    {  fd_set rfds;
       struct timeval tv;
       FD_ZERO(&rfds);
       FD_SET(audio_fd, &rfds);
       tv.tv_sec = 0;
       tv.tv_usec = 0;
       if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) return 0; // not block!
    }
#endif

  {
    audio_info_t info;
    ioctl(audio_fd, AUDIO_GETINFO, &info);
    if(queued_bursts - info.play.eof > 2)
      return 0;
  }
  return ao_outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
    len/=ao_outburst;
    len=write(audio_fd,data,len*ao_outburst);
    if(len>0) {
      queued_bursts ++;
      write(audio_fd,data,0);
    }
    return len;
}

static int audio_delay_method=2;

// return: how many unplayed bytes are in the buffer
static int get_delay(){
    int q;
    audio_info_t info;
    ioctl(audio_fd, AUDIO_GETINFO, &info);
    return (queued_bursts - info.play.eof) * ao_outburst;
}

