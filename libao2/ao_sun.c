#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/audioio.h>
#ifdef	__svr4__
#include <stropts.h>
#endif

#include "../config.h"
#include "../mixer.h"

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


/* These defines are missing on NetBSD */
#ifndef	AUDIO_PRECISION_8
#define AUDIO_PRECISION_8	8
#define AUDIO_PRECISION_16	16
#endif
#ifndef	AUDIO_CHANNELS_MONO
#define	AUDIO_CHANNELS_MONO	1
#define	AUDIO_CHANNELS_STEREO	2
#endif


static char *sun_mixer_device="/dev/audioctl";
static char *audio_dev = NULL;
static int queued_bursts = 0;
static int queued_samples = 0;
static int bytes_per_sample = 0;
static int byte_per_sec = 0;
static int convert_u8_s8;
static int audio_fd = -1;
static enum {
    RTSC_UNKNOWN = 0,
    RTSC_ENABLED,
    RTSC_DISABLED
} enable_sample_timing;

extern int verbose;


// convert an OSS audio format specification into a sun audio encoding
static int oss2sunfmt(int oss_format)
{
    switch (oss_format){
    case AFMT_MU_LAW:
	return AUDIO_ENCODING_ULAW;
    case AFMT_A_LAW:
	return AUDIO_ENCODING_ALAW;
    case AFMT_S16_BE:
    case AFMT_S16_LE:
	return AUDIO_ENCODING_LINEAR;
#ifdef	AUDIO_ENCODING_LINEAR8	// Missing on SunOS 5.5.1...
    case AFMT_U8:
	return AUDIO_ENCODING_LINEAR8;
#endif
#ifdef	AUDIO_ENCODING_DVI	// Missing on NetBSD...
    case AFMT_IMA_ADPCM:
	return AUDIO_ENCODING_DVI;
#endif
    default:
	return AUDIO_ENCODING_NONE;
  }
}

// try to figure out, if the soundcard driver provides usable (precise)
// sample counter information
static int realtime_samplecounter_available(char *dev)
{
    int fd = -1;
    audio_info_t info;
    int rtsc_ok = RTSC_DISABLED;
    int len;
    void *silence = NULL;
    struct timeval start, end;
    struct timespec delay;
    int usec_delay;
    unsigned last_samplecnt;
    unsigned increment;
    unsigned min_increment;

    len = 44100 * 4 / 4;    /* amount of data for 0.25sec of 44.1khz, stereo,
			     * 16bit.  44kbyte can be sent to all supported
			     * sun audio devices without blocking in the
			     * "write" below.
			     */
    silence = calloc(1, len);
    if (silence == NULL)
	goto error;
    
    if ((fd = open(dev, O_WRONLY)) < 0)
	goto error;

    AUDIO_INITINFO(&info);
    info.play.sample_rate = 44100;
    info.play.channels = AUDIO_CHANNELS_STEREO;
    info.play.precision = AUDIO_PRECISION_16;
    info.play.encoding = AUDIO_ENCODING_LINEAR;
    info.play.samples = 0;
    if (ioctl(fd, AUDIO_SETINFO, &info)) {
	if (verbose)
	    printf("rtsc: SETINFO failed\n");
	goto error;
    }
    
    if (write(fd, silence, len) != len) {
	if (verbose)
	    printf("rtsc: write failed");
	goto error;
    }

    if (ioctl(fd, AUDIO_GETINFO, &info)) {
	if (verbose)
	    perror("rtsc: GETINFO1");
	goto error;
    }

    last_samplecnt = info.play.samples;
    min_increment = ~0;

    gettimeofday(&start, NULL);
    for (;;) {
	delay.tv_sec = 0;
	delay.tv_nsec = 10000000;
	nanosleep(&delay, NULL);
	gettimeofday(&end, NULL);
	usec_delay = (end.tv_sec - start.tv_sec) * 1000000
	    + end.tv_usec - start.tv_usec;

	// stop monitoring sample counter after 0.2 seconds
	if (usec_delay > 200000)
	    break;

	if (ioctl(fd, AUDIO_GETINFO, &info)) {
	    if (verbose)
		perror("rtsc: GETINFO2 failed");
	    goto error;
	}
	if (info.play.samples < last_samplecnt) {
	    if (verbose)
		printf("rtsc: %d > %d?\n", last_samplecnt, info.play.samples);
	    goto error;
	}

	if ((increment = info.play.samples - last_samplecnt) > 0) {
	    if (verbose)
		printf("ao_sun: sample counter increment: %d\n", increment);
	    if (increment < min_increment) {
		min_increment = increment;
		if (min_increment < 2000)
		    break;	// looks good
	    }
	}
	last_samplecnt = info.play.samples;
    }

    /*
     * For 44.1kkz, stereo, 16-bit format we would send sound data in 16kbytes
     * chunks (== 4096 samples) to the audio device.  If we see a minimum
     * sample counter increment from the soundcard driver of less than
     * 2000 samples,  we assume that the driver provides a useable realtime
     * sample counter in the AUDIO_INFO play.samples field.  Timing based
     * on sample counts should be much more accurate than counting whole 
     * 16kbyte chunks.
     */
    if (min_increment < 2000)
	rtsc_ok = RTSC_ENABLED;

    if (verbose)
	printf("ao_sun: minimum sample counter increment per 10msec interval: %d\n"
	       "\t%susing sample counter based timing code\n",
	       min_increment, rtsc_ok == RTSC_ENABLED ? "" : "not ");
    

error:
    if (silence != NULL) free(silence);
    if (fd >= 0) {
#ifdef	__svr4__
	// remove the 0 bytes from the above measurement from the
	// audio driver's STREAMS queue
	ioctl(fd, I_FLUSH, FLUSHW);
#endif
	//ioctl(fd, AUDIO_DRAIN, 0);
	close(fd);
    }

    return rtsc_ok;
}

// to set/get/query special features/parameters
static int control(int cmd,int arg){
    switch(cmd){
    case AOCONTROL_SET_DEVICE:
	audio_dev=(char*)arg;
	return CONTROL_OK;
    case AOCONTROL_QUERY_FORMAT:
	return CONTROL_TRUE;
    case AOCONTROL_GET_VOLUME:
    {
        int fd,v,cmd,devs;

	fd=open( sun_mixer_device,O_RDONLY );
	if ( fd != -1 )
	{
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    struct audio_info info;
	    ioctl( fd,AUDIO_GETINFO,&info);
	    vol->left=info.play.gain * 100. / AUDIO_MAX_GAIN;
	    vol->right=info.play.gain * 100. / AUDIO_MAX_GAIN;
	    close( fd );
	    return CONTROL_OK;
	}	
	return CONTROL_ERROR;
    }
    case AOCONTROL_SET_VOLUME:
    {
	ao_control_vol_t *vol = (ao_control_vol_t *)arg;
        int fd,v,cmd,devs;

	fd=open( sun_mixer_device,O_RDONLY );
	if ( fd != -1 )
	{
	    struct audio_info info;
	    AUDIO_INITINFO(&info);
	    info.play.gain = (vol->right+vol->left) * AUDIO_MAX_GAIN / 100 / 2;
	    ioctl( fd,AUDIO_SETINFO,&info );
	    close( fd );
	    return CONTROL_OK;
	}	
	return CONTROL_ERROR;
    }
    }
    return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

    audio_info_t info;
    int ok;

    if(mixer_device)
      sun_mixer_device=mixer_device;

    if (audio_dev == NULL) {
	if ((audio_dev = getenv("AUDIODEV")) == NULL)
	    audio_dev = "/dev/audio";
    }

    if (ao_subdevice) audio_dev = ao_subdevice;

    if (enable_sample_timing == RTSC_UNKNOWN
	&& !getenv("AO_SUN_DISABLE_SAMPLE_TIMING")) {
	enable_sample_timing = realtime_samplecounter_available(audio_dev);
    }

//    printf("ao2: %d Hz  %d chans  %s [0x%X]\n",
//	   rate,channels,audio_out_format_name(format),format);

    audio_fd=open(audio_dev, O_WRONLY);
    if(audio_fd<0){
	printf("Can't open audio device %s, %s  -> nosound\n", audio_dev, strerror(errno));
	return 0;
    }

    ioctl(audio_fd, AUDIO_DRAIN, 0);

    AUDIO_INITINFO(&info);
    info.play.encoding = oss2sunfmt(ao_data.format = format);
    info.play.precision =
	(format==AFMT_S16_LE || format==AFMT_S16_BE
	 ? AUDIO_PRECISION_16
	 : AUDIO_PRECISION_8);
    info.play.channels = ao_data.channels = channels;
    info.play.sample_rate = ao_data.samplerate = rate;
    convert_u8_s8 = 0;
    ok = ioctl(audio_fd, AUDIO_SETINFO, &info) >= 0;
    if (!ok && info.play.encoding == AUDIO_ENCODING_LINEAR8) {
	/* sun audiocs hardware does not support U8 format, try S8... */
	info.play.encoding = AUDIO_ENCODING_LINEAR;
	ok = ioctl(audio_fd, AUDIO_SETINFO, &info) >= 0;
	if (ok) {
	    /* we must perform software U8 -> S8 conversion */
	    convert_u8_s8 = 1;
	}
    }
    if (!ok) {
	printf("audio_setup: your card doesn't support %d channel, %s, %d Hz samplerate\n",
	       channels, audio_out_format_name(format), rate);
	return 0;
    }

    bytes_per_sample = channels * info.play.precision / 8;
    byte_per_sec = bytes_per_sample * rate;
    ao_data.outburst = byte_per_sec > 100000 ? 16384 : 8192;

#ifdef	__not_used__
    /*
     * hmm, ao_data.buffersize is currently not used in this driver, do there's
     * no need to measure it
     */
    if(ao_data.buffersize==-1){
	// Measuring buffer size:
	void* data;
	ao_data.buffersize=0;
#ifdef HAVE_AUDIO_SELECT
	data = malloc(ao_data.outburst);
	memset(data, format==AFMT_U8 ? 0x80 : 0, ao_data.outburst);
	while(ao_data.buffersize<0x40000){
	    fd_set rfds;
	    struct timeval tv;
	    FD_ZERO(&rfds); FD_SET(audio_fd,&rfds);
	    tv.tv_sec=0; tv.tv_usec = 0;
	    if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) break;
	    write(audio_fd,data,ao_data.outburst);
	    ao_data.buffersize+=ao_data.outburst;
	}
	free(data);
	if(ao_data.buffersize==0){
	    printf("\n   ***  Your audio driver DOES NOT support select()  ***\n");
	    printf("Recompile mplayer with #undef HAVE_AUDIO_SELECT in config.h !\n\n");
	    return 0;
	}
#ifdef	__svr4__
	// remove the 0 bytes from the above ao_data.buffersize measurement from the
	// audio driver's STREAMS queue
	ioctl(audio_fd, I_FLUSH, FLUSHW);
#endif
	ioctl(audio_fd, AUDIO_DRAIN, 0);
#endif
    }
#endif	/* __not_used__ */

    AUDIO_INITINFO(&info);
    info.play.samples = 0;
    info.play.eof = 0;
    info.play.error = 0;
    ioctl (audio_fd, AUDIO_SETINFO, &info);

    queued_bursts = 0;
    queued_samples = 0;

    return 1;
}

// close audio device
static void uninit(){
#ifdef	__svr4__
    // throw away buffered data in the audio driver's STREAMS queue
    ioctl(audio_fd, I_FLUSH, FLUSHW);
#endif
    close(audio_fd);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(){
    audio_info_t info;

    uninit();
    audio_fd=open(audio_dev, O_WRONLY);
    if(audio_fd<0){
	printf("\nFatal error: *** CANNOT RE-OPEN / RESET AUDIO DEVICE (%s) ***\n", strerror(errno));
	return;
    }

    ioctl(audio_fd, AUDIO_DRAIN, 0);

    AUDIO_INITINFO(&info);
    info.play.encoding = oss2sunfmt(ao_data.format);
    info.play.precision =
	(ao_data.format==AFMT_S16_LE || ao_data.format==AFMT_S16_BE 
	 ? AUDIO_PRECISION_16
	 : AUDIO_PRECISION_8);
    info.play.channels = ao_data.channels;
    info.play.sample_rate = ao_data.samplerate;
    info.play.samples = 0;
    info.play.eof = 0;
    info.play.error = 0;
    ioctl (audio_fd, AUDIO_SETINFO, &info);
    queued_bursts = 0;
    queued_samples = 0;
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
    int playsize = ao_data.outburst;
    audio_info_t info;

    // check buffer
#ifdef HAVE_AUDIO_SELECT
    {
	fd_set rfds;
	struct timeval tv;
	FD_ZERO(&rfds);
	FD_SET(audio_fd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) return 0; // not block!
    }
#endif

#ifndef __OpenBSD__
    ioctl(audio_fd, AUDIO_GETINFO, &info);
    if (queued_bursts - info.play.eof > 2)
	return 0;
#endif

    return ao_data.outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
#if	WORDS_BIGENDIAN
    int native_endian = AFMT_S16_BE;
#else
    int native_endian = AFMT_S16_LE;
#endif

    if (len < ao_data.outburst) return 0;
    len /= ao_data.outburst;
    len *= ao_data.outburst;

    /* 16-bit format using the 'wrong' byteorder?  swap words */
    if ((ao_data.format == AFMT_S16_LE || ao_data.format == AFMT_S16_BE)
	&& ao_data.format != native_endian) {
	static void *swab_buf;
	static int swab_len;
	if (len > swab_len) {
	    if (swab_buf)
		swab_buf = realloc(swab_buf, len);
	    else
		swab_buf = malloc(len);
	    swab_len = len;
	    if (swab_buf == NULL) return 0;
	}
	swab(data, swab_buf, len);
	data = swab_buf;
    } else if (ao_data.format == AFMT_U8 && convert_u8_s8) {
	int i;
	unsigned char *p = data;

	for (i = 0, p = data; i < len; i++, p++)
	    *p ^= 0x80;
    }

    len = write(audio_fd, data, len);
    if(len > 0) {
	queued_samples += len / bytes_per_sample;
	if (write(audio_fd,data,0) < 0)
	    perror("ao_sun: send EOF audio record");
	else
	    queued_bursts ++;
    }
    return len;
}


// return: delay in seconds between first and last sample in buffer
static float get_delay(){
    audio_info_t info;
    ioctl(audio_fd, AUDIO_GETINFO, &info);
#ifdef __OpenBSD__
    return (float) info.play.seek/ (float)byte_per_sec ;
#else
    if (info.play.samples && enable_sample_timing == RTSC_ENABLED)
	return (float)(queued_samples - info.play.samples) / (float)byte_per_sec;
    else
	return (float)((queued_bursts - info.play.eof) * ao_data.outburst) / (float)byte_per_sec;
#endif
}

