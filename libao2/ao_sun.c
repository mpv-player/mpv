/*
 * SUN audio output driver
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
#ifdef	AUDIO_SWFEATURE_MIXER	/* solaris8 or newer? */
# define HAVE_SYS_MIXER_H 1
#endif
#if	HAVE_SYS_MIXER_H
# include <sys/mixer.h>
#endif
#ifdef	__svr4__
#include <stropts.h>
#endif

#include "config.h"
#include "mixer.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include "mp_msg.h"
#include "help_mp.h"

static const ao_info_t info =
{
    "Sun audio output",
    "sun",
    "Juergen Keil",
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


static char *sun_mixer_device = NULL;
static char *audio_dev = NULL;
static int queued_bursts = 0;
static int queued_samples = 0;
static int bytes_per_sample = 0;
static int byte_per_sec = 0;
static int audio_fd = -1;
static enum {
    RTSC_UNKNOWN = 0,
    RTSC_ENABLED,
    RTSC_DISABLED
} enable_sample_timing;


static void flush_audio(int fd) {
#ifdef AUDIO_FLUSH
  ioctl(fd, AUDIO_FLUSH, 0);
#elif defined(__svr4__)
  ioctl(fd, I_FLUSH, FLUSHW);
#endif
}

// convert an OSS audio format specification into a sun audio encoding
static int af2sunfmt(int format)
{
    switch (format){
    case AF_FORMAT_MU_LAW:
	return AUDIO_ENCODING_ULAW;
    case AF_FORMAT_A_LAW:
	return AUDIO_ENCODING_ALAW;
    case AF_FORMAT_S16_NE:
	return AUDIO_ENCODING_LINEAR;
#ifdef	AUDIO_ENCODING_LINEAR8	// Missing on SunOS 5.5.1...
    case AF_FORMAT_U8:
	return AUDIO_ENCODING_LINEAR8;
#endif
    case AF_FORMAT_S8:
	return AUDIO_ENCODING_LINEAR;
#ifdef	AUDIO_ENCODING_DVI	// Missing on NetBSD...
    case AF_FORMAT_IMA_ADPCM:
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
	if ( mp_msg_test(MSGT_AO,MSGL_V) )
	    mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_SUN_RtscSetinfoFailed);
	goto error;
    }

    if (write(fd, silence, len) != len) {
	if ( mp_msg_test(MSGT_AO,MSGL_V) )
	    mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_SUN_RtscWriteFailed);
	goto error;
    }

    if (ioctl(fd, AUDIO_GETINFO, &info)) {
	if ( mp_msg_test(MSGT_AO,MSGL_V) )
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
	    if ( mp_msg_test(MSGT_AO,MSGL_V) )
		perror("rtsc: GETINFO2 failed");
	    goto error;
	}
	if (info.play.samples < last_samplecnt) {
	    if ( mp_msg_test(MSGT_AO,MSGL_V) )
		mp_msg(MSGT_AO,MSGL_V,"rtsc: %d > %d?\n", last_samplecnt, info.play.samples);
	    goto error;
	}

	if ((increment = info.play.samples - last_samplecnt) > 0) {
	    if ( mp_msg_test(MSGT_AO,MSGL_V) )
	        mp_msg(MSGT_AO,MSGL_V,"ao_sun: sample counter increment: %d\n", increment);
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

    if ( mp_msg_test(MSGT_AO,MSGL_V) )
	mp_msg(MSGT_AO,MSGL_V,"ao_sun: minimum sample counter increment per 10msec interval: %d\n"
	       "\t%susing sample counter based timing code\n",
	       min_increment, rtsc_ok == RTSC_ENABLED ? "" : "not ");


error:
    if (silence != NULL) free(silence);
    if (fd >= 0) {
	// remove the 0 bytes from the above measurement from the
	// audio driver's STREAMS queue
        flush_audio(fd);
	close(fd);
    }

    return rtsc_ok;
}


// match the requested sample rate |sample_rate| against the
// sample rates supported by the audio device |dev|.  Return
// a supported sample rate,  if that sample rate is close to
// (< 1% difference) the requested rate; return 0 otherwise.

#define	MAX_RATE_ERR	1

static unsigned
find_close_samplerate_match(int dev, unsigned sample_rate)
{
#if	HAVE_SYS_MIXER_H
    am_sample_rates_t *sr;
    unsigned i, num, err, best_err, best_rate;

    for (num = 16; num < 1024; num *= 2) {
	sr = malloc(AUDIO_MIXER_SAMP_RATES_STRUCT_SIZE(num));
	if (!sr)
	    return 0;
	sr->type = AUDIO_PLAY;
	sr->flags = 0;
	sr->num_samp_rates = num;
	if (ioctl(dev, AUDIO_MIXER_GET_SAMPLE_RATES, sr)) {
	    free(sr);
	    return 0;
	}
	if (sr->num_samp_rates <= num)
	    break;
	free(sr);
    }

    if (sr->flags & MIXER_SR_LIMITS) {
	/*
	 * HW can playback any rate between
	 * sr->samp_rates[0] .. sr->samp_rates[1]
	 */
	free(sr);
	return 0;
    } else {
	/* HW supports fixed sample rates only */

	best_err = 65535;
	best_rate = 0;

	for (i = 0; i < sr->num_samp_rates; i++) {
	    err = abs(sr->samp_rates[i] - sample_rate);
	    if (err == 0) {
		/*
		 * exact supported sample rate match, no need to
		 * retry something else
		 */
		best_rate = 0;
		break;
	    }
	    if (err < best_err) {
		best_err = err;
		best_rate = sr->samp_rates[i];
	    }
	}

	free(sr);

	if (best_rate > 0 && (100/MAX_RATE_ERR)*best_err < sample_rate) {
	    /* found a supported sample rate with <1% error? */
	    return best_rate;
	}
	return 0;
    }
#else	/* old audioio driver, cannot return list of supported rates */
    /* XXX: hardcoded sample rates */
    unsigned i, err;
    unsigned audiocs_rates[] = {
	5510, 6620, 8000, 9600, 11025, 16000, 18900, 22050,
	27420, 32000, 33075, 37800, 44100, 48000, 0
    };

    for (i = 0; audiocs_rates[i]; i++) {
	err = abs(audiocs_rates[i] - sample_rate);
	if (err == 0) {
	    /*
	     * exact supported sample rate match, no need to
	     * retry something elise
	     */
	    return 0;
	}
	if ((100/MAX_RATE_ERR)*err < audiocs_rates[i]) {
	    /* <1% error? */
	    return audiocs_rates[i];
	}
    }

    return 0;
#endif
}


// return the highest sample rate supported by audio device |dev|.
static unsigned
find_highest_samplerate(int dev)
{
#if	HAVE_SYS_MIXER_H
    am_sample_rates_t *sr;
    unsigned i, num, max_rate;

    for (num = 16; num < 1024; num *= 2) {
	sr = malloc(AUDIO_MIXER_SAMP_RATES_STRUCT_SIZE(num));
	if (!sr)
	    return 0;
	sr->type = AUDIO_PLAY;
	sr->flags = 0;
	sr->num_samp_rates = num;
	if (ioctl(dev, AUDIO_MIXER_GET_SAMPLE_RATES, sr)) {
	    free(sr);
	    return 0;
	}
	if (sr->num_samp_rates <= num)
	    break;
	free(sr);
    }

    if (sr->flags & MIXER_SR_LIMITS) {
	/*
	 * HW can playback any rate between
	 * sr->samp_rates[0] .. sr->samp_rates[1]
	 */
	max_rate = sr->samp_rates[1];
    } else {
	/* HW supports fixed sample rates only */
	max_rate = 0;
	for (i = 0; i < sr->num_samp_rates; i++) {
	    if (sr->samp_rates[i] > max_rate)
		max_rate = sr->samp_rates[i];
	}
    }
    free(sr);
    return max_rate;

#else	/* old audioio driver, cannot return list of supported rates */
    return 44100;	/* should be supported even on old ISA SB cards */
#endif
}


static void setup_device_paths(void)
{
    if (audio_dev == NULL) {
	if ((audio_dev = getenv("AUDIODEV")) == NULL)
	    audio_dev = "/dev/audio";
    }

    if (sun_mixer_device == NULL) {
	if ((sun_mixer_device = mixer_device) == NULL || !sun_mixer_device[0]) {
	    sun_mixer_device = malloc(strlen(audio_dev) + 4);
	    strcpy(sun_mixer_device, audio_dev);
	    strcat(sun_mixer_device, "ctl");
	}
    }

    if (ao_subdevice) audio_dev = ao_subdevice;
}

// to set/get/query special features/parameters
static int control(int cmd,void *arg){
    switch(cmd){
    case AOCONTROL_SET_DEVICE:
	audio_dev=(char*)arg;
	return CONTROL_OK;
    case AOCONTROL_QUERY_FORMAT:
	return CONTROL_TRUE;
    case AOCONTROL_GET_VOLUME:
    {
        int fd;

	if ( !sun_mixer_device )    /* control function is used before init? */
	    setup_device_paths();

	fd=open( sun_mixer_device,O_RDONLY );
	if ( fd != -1 )
	{
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    float volume;
	    struct audio_info info;
	    ioctl( fd,AUDIO_GETINFO,&info);
	    volume = info.play.gain * 100. / AUDIO_MAX_GAIN;
	    if ( info.play.balance == AUDIO_MID_BALANCE ) {
		vol->right = vol->left = volume;
	    } else if ( info.play.balance < AUDIO_MID_BALANCE ) {
		vol->left  = volume;
		vol->right = volume * info.play.balance / AUDIO_MID_BALANCE;
	    } else {
		vol->left  = volume * (AUDIO_RIGHT_BALANCE-info.play.balance)
							/ AUDIO_MID_BALANCE;
		vol->right = volume;
	    }
	    close( fd );
	    return CONTROL_OK;
	}
	return CONTROL_ERROR;
    }
    case AOCONTROL_SET_VOLUME:
    {
	ao_control_vol_t *vol = (ao_control_vol_t *)arg;
        int fd;

	if ( !sun_mixer_device )    /* control function is used before init? */
	    setup_device_paths();

	fd=open( sun_mixer_device,O_RDONLY );
	if ( fd != -1 )
	{
	    struct audio_info info;
	    float volume;
	    AUDIO_INITINFO(&info);
	    volume = vol->right > vol->left ? vol->right : vol->left;
	    if ( volume != 0 ) {
		info.play.gain = volume * AUDIO_MAX_GAIN / 100;
		if ( vol->right == vol->left )
		    info.play.balance = AUDIO_MID_BALANCE;
		else
		    info.play.balance = (vol->right - vol->left + volume) * AUDIO_RIGHT_BALANCE / (2*volume);
	    }
#if !defined (__OpenBSD__) && !defined (__NetBSD__)
	    info.output_muted = (volume == 0);
#endif
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
    int pass;
    int ok;
    int convert_u8_s8;

    setup_device_paths();

    if (enable_sample_timing == RTSC_UNKNOWN
	&& !getenv("AO_SUN_DISABLE_SAMPLE_TIMING")) {
	enable_sample_timing = realtime_samplecounter_available(audio_dev);
    }

    mp_msg(MSGT_AO,MSGL_STATUS,"ao2: %d Hz  %d chans  %s [0x%X]\n",
	   rate,channels,af_fmt2str_short(format),format);

    audio_fd=open(audio_dev, O_WRONLY);
    if(audio_fd<0){
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_SUN_CantOpenAudioDev, audio_dev, strerror(errno));
	return 0;
    }

    if (af2sunfmt(format) == AUDIO_ENCODING_NONE)
      format = AF_FORMAT_S16_NE;

    for (ok = pass = 0; pass <= 5; pass++) { /* pass 6&7 not useful */

	AUDIO_INITINFO(&info);
	info.play.encoding = af2sunfmt(ao_data.format = format);
	info.play.precision =
	    (format==AF_FORMAT_S16_NE
	     ? AUDIO_PRECISION_16
	     : AUDIO_PRECISION_8);
	info.play.channels = ao_data.channels = channels;
	info.play.sample_rate = ao_data.samplerate = rate;

	convert_u8_s8 = 0;

	if (pass & 1) {
	    /*
	     * on some sun audio drivers, 8-bit unsigned LINEAR8 encoding is
	     * not supported, but 8-bit signed encoding is.
	     *
	     * Try S8, and if it works, use our own U8->S8 conversion before
	     * sending the samples to the sound driver.
	     */
#ifdef AUDIO_ENCODING_LINEAR8
	    if (info.play.encoding != AUDIO_ENCODING_LINEAR8)
#endif
		continue;
	    info.play.encoding = AUDIO_ENCODING_LINEAR;
	    convert_u8_s8 = 1;
	}

	if (pass & 2) {
	    /*
	     * on some sun audio drivers, only certain fixed sample rates are
	     * supported.
	     *
	     * In case the requested sample rate is very close to one of the
	     * supported rates,  use the fixed supported rate instead.
	     */
	    if (!(info.play.sample_rate =
		  find_close_samplerate_match(audio_fd, rate)))
	      continue;

	    /*
	     * I'm not returning the correct sample rate in
	     * |ao_data.samplerate|, to avoid software resampling.
	     *
	     * ao_data.samplerate = info.play.sample_rate;
	     */
	}

	if (pass & 4) {
	    /* like "pass & 2", but use the highest supported sample rate */
	    if (!(info.play.sample_rate
		  = ao_data.samplerate
		  = find_highest_samplerate(audio_fd)))
		continue;
	}

	ok = ioctl(audio_fd, AUDIO_SETINFO, &info) >= 0;
	if (ok) {
	    /* audio format accepted by audio driver */
	    break;
	}

	/*
	 * format not supported?
	 * retry with different encoding and/or sample rate
	 */
    }

    if (!ok) {
	char buf[128];
	mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_SUN_UnsupSampleRate,
	       channels, af_fmt2str(format, buf, 128), rate);
	return 0;
    }

    if (convert_u8_s8)
      ao_data.format = AF_FORMAT_S8;

    bytes_per_sample = channels * info.play.precision / 8;
    ao_data.bps = byte_per_sec = bytes_per_sample * ao_data.samplerate;
    ao_data.outburst = byte_per_sec > 100000 ? 16384 : 8192;

    reset();

    return 1;
}

// close audio device
static void uninit(int immed){
    // throw away buffered data in the audio driver's STREAMS queue
    if (immed)
	flush_audio(audio_fd);
    else
	ioctl(audio_fd, AUDIO_DRAIN, 0);
    close(audio_fd);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){
    audio_info_t info;
    flush_audio(audio_fd);

    AUDIO_INITINFO(&info);
    info.play.samples = 0;
    info.play.eof = 0;
    info.play.error = 0;
    ioctl(audio_fd, AUDIO_SETINFO, &info);

    queued_bursts = 0;
    queued_samples = 0;
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    struct audio_info info;
    AUDIO_INITINFO(&info);
    info.play.pause = 1;
    ioctl(audio_fd, AUDIO_SETINFO, &info);
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
    struct audio_info info;
    AUDIO_INITINFO(&info);
    info.play.pause = 0;
    ioctl(audio_fd, AUDIO_SETINFO, &info);
}


// return: how many bytes can be played without blocking
static int get_space(void){
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

    ioctl(audio_fd, AUDIO_GETINFO, &info);
#if !defined (__OpenBSD__) && !defined(__NetBSD__)
    if (queued_bursts - info.play.eof > 2)
	return 0;
    return ao_data.outburst;
#else
    return info.hiwat * info.blocksize - info.play.seek;
#endif

}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
    if (len < ao_data.outburst) return 0;
    len /= ao_data.outburst;
    len *= ao_data.outburst;

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
static float get_delay(void){
    audio_info_t info;
    ioctl(audio_fd, AUDIO_GETINFO, &info);
#if defined (__OpenBSD__) || defined(__NetBSD__)
    return (float) info.play.seek/ (float)byte_per_sec ;
#else
    if (info.play.samples && enable_sample_timing == RTSC_ENABLED)
	return (float)(queued_samples - info.play.samples) / (float)ao_data.samplerate;
    else
	return (float)((queued_bursts - info.play.eof) * ao_data.outburst) / (float)byte_per_sec;
#endif
}
