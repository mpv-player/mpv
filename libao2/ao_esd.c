/*
 * ao_esd - EsounD audio output driver for MPlayer
 *
 * Juergen Keil <jk@tools.de>
 *
 * This driver is distributed under the terms of the GPL
 *
 * TODO / known problems:
 * - does not work well when the esd daemon has autostandby disabled
 *   (workaround: run esd with option "-as 2" - fortunatelly this is
 *    the default)
 * - plays noise on a linux 2.4.4 kernel with a SB16PCI card, when using
 *   a local tcp connection to the esd daemon; there is no noise when using
 *   a unix domain socket connection.
 *   (there are EIO errors reported by the sound card driver, so this is
 *   most likely a linux sound card driver problem)
 */

#include "../config.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#ifdef	__svr4__
#include <stropts.h>
#endif
#include <esd.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"
#include "../config.h"
#include "../mp_msg.h"


#undef	ESD_DEBUG

#if	ESD_DEBUG
#define	dprintf(...)	printf(__VA_ARGS__)
#else
#define	dprintf(...)	/**/
#endif


#define	ESD_CLIENT_NAME	"MPlayer"
#define	ESD_MAX_DELAY	(1.0f)	/* max amount of data buffered in esd (#sec) */


static ao_info_t info =
{
    "EsounD audio output",
    "esd",
    "Juergen Keil <jk@tools.de>",
    ""
};

LIBAO_EXTERN(esd)

static int esd_fd = -1;
static int esd_play_fd = -1;
static esd_server_info_t *esd_svinfo;
static int esd_latency;
static int esd_bytes_per_sample;
static unsigned long esd_samples_written;
static struct timeval esd_play_start;


/*
 * to set/get/query special features/parameters
 */
static int control(int cmd, int arg)
{
    esd_player_info_t *esd_pi;
    esd_info_t        *esd_i;
    time_t	       now;
    static time_t      vol_cache_time;
    static ao_control_vol_t vol_cache;

    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
	time(&now);
	if (now == vol_cache_time) {
	    *(ao_control_vol_t *)arg = vol_cache;
	    return CONTROL_OK;
	}

	dprintf("esd: get vol\n");
	if ((esd_i = esd_get_all_info(esd_fd)) == NULL)
	    return CONTROL_ERROR;

	for (esd_pi = esd_i->player_list; esd_pi != NULL; esd_pi = esd_pi->next)
	    if (strcmp(esd_pi->name, ESD_CLIENT_NAME) == 0)
		break;

	if (esd_pi != NULL) {
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    vol->left =  esd_pi->left_vol_scale  * 100 / ESD_VOLUME_BASE;
	    vol->right = esd_pi->right_vol_scale * 100 / ESD_VOLUME_BASE;

	    vol_cache = *vol;
	    vol_cache_time = now;
	}
	esd_free_all_info(esd_i);
	
	return CONTROL_OK;

    case AOCONTROL_SET_VOLUME:
	dprintf("esd: set vol\n");
	if ((esd_i = esd_get_all_info(esd_fd)) == NULL)
	    return CONTROL_ERROR;

	for (esd_pi = esd_i->player_list; esd_pi != NULL; esd_pi = esd_pi->next)
	    if (strcmp(esd_pi->name, ESD_CLIENT_NAME) == 0)
		break;

	if (esd_pi != NULL) {
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    esd_set_stream_pan(esd_fd, esd_pi->source_id,
			       vol->left  * ESD_VOLUME_BASE / 100,
			       vol->right * ESD_VOLUME_BASE / 100);

	    vol_cache = *vol;
	    time(&vol_cache_time);
	}
	esd_free_all_info(esd_i);
	return CONTROL_OK;

    default:
	return CONTROL_UNKNOWN;
    }
}


/*
 * open & setup audio device
 * return: 1=success 0=fail
 */
static int init(int rate_hz, int channels, int format, int flags)
{
    esd_format_t esd_fmt;
    int bytes_per_sample;
    int fl;

    if (esd_fd < 0) {
	esd_fd = esd_open_sound(NULL);
	if (esd_fd < 0) {
	    mp_msg(MSGT_AO, MSGL_ERR, "AO: [esd] esd_open_sound failed: %s\n",
		   strerror(errno));
	    return 0;
	}

	esd_svinfo = esd_get_server_info(esd_fd);
     /*
	if (esd_svinfo) {
	    mp_msg(MSGT_AO, MSGL_INFO, "AO: [esd] server info:\n");
	    esd_print_server_info(esd_svinfo);
	}
      */

	esd_latency = esd_get_latency(esd_fd);
	/* mp_msg(MSGT_AO, MSGL_INFO, "AO: [esd] latency: %d\n", esd_latency); */
    }

    esd_fmt = ESD_STREAM | ESD_PLAY;

#if	ESD_RESAMPLES
    /* let the esd daemon convert sample rate */
#else
    /* let mplayer's audio filter convert the sample rate */
    if (esd_svinfo != NULL)
	rate_hz = esd_svinfo->rate;
#endif
    ao_data.samplerate = rate_hz;

    
    /* EsounD can play mono or stereo */
    switch (channels) {
    case 1:
	esd_fmt |= ESD_MONO;
	ao_data.channels = bytes_per_sample = 1;
	break;
    default:
	esd_fmt |= ESD_STEREO;
	ao_data.channels = bytes_per_sample = 2;
	break;
    }

    /* EsounD can play 8bit unsigned and 16bit signed native */
    switch (format) {
    case AFMT_S8:
    case AFMT_U8:
	esd_fmt |= ESD_BITS8;
	ao_data.format = AFMT_U8;
	break;
    default:
	esd_fmt |= ESD_BITS16;
	ao_data.format = AFMT_S16_NE;
	bytes_per_sample *= 2;
	break;
    }

    esd_play_fd = esd_play_stream_fallback(esd_fmt, rate_hz,
					   NULL, ESD_CLIENT_NAME);
    if (esd_play_fd < 0) {
	mp_msg(MSGT_AO, MSGL_ERR,
	       "AO: [esd] failed to open esd playback stream: %s\n",
	       strerror(errno));
	return 0;
    }

    /* enable non-blocking i/o on the socket connection to the esd server */
    if ((fl = fcntl(esd_play_fd, F_GETFL)) >= 0)
	fcntl(esd_play_fd, F_SETFL, O_NDELAY|fl);

#if ESD_DEBUG
    {
	int sbuf, rbuf, len;
	len = sizeof(sbuf);
	getsockopt(esd_play_fd, SOL_SOCKET, SO_SNDBUF, &sbuf, &len);
	len = sizeof(rbuf);
	getsockopt(esd_play_fd, SOL_SOCKET, SO_RCVBUF, &rbuf, &len);
	dprintf("esd: send/receive socket buffer space %d/%d bytes\n",
		sbuf, rbuf);
    }
#endif

    ao_data.bps = bytes_per_sample * rate_hz;
    ao_data.outburst = ao_data.bps > 100000 ? 4*ESD_BUF_SIZE : 2*ESD_BUF_SIZE;

    esd_play_start.tv_sec = 0;
    esd_samples_written = 0;
    esd_bytes_per_sample = bytes_per_sample;

    return 1;
}


/*
 * close audio device
 */
static void uninit()
{
    if (esd_play_fd >= 0) {
	esd_close(esd_play_fd);
	esd_play_fd = -1;
    }

    if (esd_svinfo) {
	esd_free_server_info(esd_svinfo);
	esd_svinfo = NULL;
    }

    if (esd_fd >= 0) {
	esd_close(esd_fd);
	esd_fd = -1;
    }
}


/*
 * plays 'len' bytes of 'data'
 * it should round it down to outburst*n
 * return: number of bytes played
 */
static int play(void* data, int len, int flags)
{
    int offs;
    int nwritten;
    int nsamples;
    int remainder, n;
    int saved_fl;

    /* round down buffersize to a multiple of ESD_BUF_SIZE bytes */
    len = len / ESD_BUF_SIZE * ESD_BUF_SIZE;
    if (len <= 0)
	return 0;

#define	SINGLE_WRITE 0
#if	SINGLE_WRITE
    nwritten = write(esd_play_fd, data, len);
#else
    for (offs = 0; offs + ESD_BUF_SIZE <= len; offs += ESD_BUF_SIZE) {
	/*
	 * note: we're writing to a non-blocking socket here.
	 * A partial write means, that the socket buffer is full.
	 */
	nwritten = write(esd_play_fd, (char*)data + offs, ESD_BUF_SIZE);
	if (nwritten != ESD_BUF_SIZE) {
	    if (nwritten < 0 && errno != EAGAIN) {
		dprintf("esd play: write failed: %s\n", strerror(errno));
	    }
	    break;
	}
    }
    nwritten = offs;
#endif

    if (nwritten > 0 && nwritten % ESD_BUF_SIZE != 0) {

	/*
	 * partial write of an audio block of ESD_BUF_SIZE bytes.
	 *
	 * Send the remainder of that block as well; this avoids a busy
	 * polling loop in the esd daemon, which waits for the rest of
	 * the incomplete block using reads from a non-blocking
	 * socket. This busy polling loop wastes CPU cycles on the
	 * esd server machine, and we're trying to avoid that.
	 * (esd 0.2.28+ has the busy polling read loop, 0.2.22 inserts
	 * 0 samples which is bad as well)
	 *
	 * Let's hope the blocking write does not consume too much time.
	 *
	 * (fortunatelly, this piece of code is not used when playing
	 * sound on the local machine - on solaris at least)
	 */
	remainder = ESD_BUF_SIZE - nwritten % ESD_BUF_SIZE;
	dprintf("esd play: partial audio block written, remainder %d \n",
		remainder);

	/* blocking write of remaining bytes for the partial audio block */
	saved_fl = fcntl(esd_play_fd, F_GETFL);
	fcntl(esd_play_fd, F_SETFL, saved_fl & ~O_NDELAY);
	n = write(esd_play_fd, (char *)data + nwritten, remainder);
	fcntl(esd_play_fd, F_SETFL, saved_fl);

	if (n != remainder) {
	    mp_msg(MSGT_AO, MSGL_ERR,
		   "AO: [esd] send remainer of audio block failed, %d/%d\n",
		   n, remainder);
	} else
	    nwritten += n;
    }

    if (nwritten > 0) {
	if (!esd_play_start.tv_sec)
	    gettimeofday(&esd_play_start, NULL);
	nsamples = nwritten / esd_bytes_per_sample;
	esd_samples_written += nsamples;
 
	dprintf("esd play: %d %lu\n", nsamples, esd_samples_written);
    } else {
	dprintf("esd play: blocked / %lu\n", esd_samples_written);
    }

    return nwritten;
}


/*
 * stop playing, keep buffers (for pause)
 */
static void audio_pause()
{
    /*
     * not possible with esd.  the esd daemom will continue playing
     * buffered data (not more than ESD_MAX_DELAY seconds of samples)
     */
}


/*
 * resume playing, after audio_pause()
 */
static void audio_resume()
{
    /*
     * not possible with esd.
     *
     * Let's hope the pause was long enough that the esd ran out of
     * buffered data;  we restart our time based delay computation
     * for an audio resume.
     */
    esd_play_start.tv_sec = 0;
    esd_samples_written = 0;
}


/*
 * stop playing and empty buffers (for seeking/pause)
 */
static void reset()
{
#ifdef	__svr4__
    /* throw away data buffered in the esd connection */
    if (ioctl(esd_play_fd, I_FLUSH, FLUSHW)) 
	perror("I_FLUSH");
#endif
}


/*
 * return: how many bytes can be played without blocking
 */
static int get_space()
{
    struct timeval tmout;
    fd_set wfds;
    float current_delay;
    int space;

    /* 
     * Don't buffer too much data in the esd daemon.
     *
     * If we send too much, esd will block in write()s to the sound
     * device, and the consequence is a huge slow down for things like
     * esd_get_all_info().
     */
    if ((current_delay = get_delay()) >= ESD_MAX_DELAY) {
	dprintf("esd get_space: too much data buffered\n");
	return 0;
    }

    FD_ZERO(&wfds);
    FD_SET(esd_play_fd, &wfds);
    tmout.tv_sec = 0;
    tmout.tv_usec = 0;

    if (select(esd_play_fd + 1, NULL, &wfds, NULL, &tmout) != 1)
	return 0;

    if (!FD_ISSET(esd_play_fd, &wfds))
	return 0;

    /* try to fill 50% of the remaining "free" buffer space */
    space = (ESD_MAX_DELAY - current_delay) * ao_data.bps * 0.5f;

    /* round up to next multiple of ESD_BUF_SIZE */
    space = (space + ESD_BUF_SIZE-1) / ESD_BUF_SIZE * ESD_BUF_SIZE;

    dprintf("esd get_space: %d\n", space);
    return space;
}


/*
 * return: delay in seconds between first and last sample in buffer
 */
static float get_delay()
{
    struct timeval now;
    double buffered_samples_time;
    double play_time;

    if (!esd_play_start.tv_sec)
	return 0;

    buffered_samples_time = (float)esd_samples_written / ao_data.samplerate;
    gettimeofday(&now, NULL);
    play_time  =  now.tv_sec  - esd_play_start.tv_sec;
    play_time += (now.tv_usec - esd_play_start.tv_usec) / 1000000.;
    
    /* dprintf("esd delay: %f %f\n", play_time, buffered_samples_time); */

    if (play_time > buffered_samples_time) {
	dprintf("esd: underflow\n");
	esd_play_start.tv_sec = 0;
	esd_samples_written = 0;
	return 0;
    }

    dprintf("esd: get_delay %f\n", buffered_samples_time - play_time);
    return buffered_samples_time - play_time;
}
