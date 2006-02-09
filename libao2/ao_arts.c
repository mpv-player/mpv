/*
 * ao_arts - aRts audio output driver for MPlayer
 *
 * Michele Balistreri <brain87@gmx.net>
 *
 * This driver is distribuited under terms of GPL
 *
 */

#include <artsc.h>
#include <stdio.h>

#include "config.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include "mp_msg.h"
#include "help_mp.h"

#define OBTAIN_BITRATE(a) (((a != AF_FORMAT_U8) && (a != AF_FORMAT_S8)) ? 16 : 8)

/* Feel free to experiment with the following values: */
#define ARTS_PACKETS 10 /* Number of audio packets */
#define ARTS_PACKET_SIZE_LOG2 11 /* Log2 of audio packet size */

static arts_stream_t stream;

static ao_info_t info =
{
    "aRts audio output",
    "arts",
    "Michele Balistreri <brain87@gmx.net>",
    ""
};

LIBAO_EXTERN(arts)

static int control(int cmd, void *arg)
{
	return(CONTROL_UNKNOWN);
}

static int init(int rate_hz, int channels, int format, int flags)
{
	int err;
	int frag_spec;

	if( (err=arts_init()) ) {
		mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ARTS_CantInit, arts_error_text(err));
		return 0;
	}
	mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_ARTS_ServerConnect);

	/*
	 * arts supports 8bit unsigned and 16bit signed sample formats
	 * (16bit apparently in little endian format, even in the case
	 * when artsd runs on a big endian cpu).
	 *
	 * Unsupported formats are translated to one of these two formats
	 * using mplayer's audio filters.
	 */
	switch (format) {
	case AF_FORMAT_U8:
	case AF_FORMAT_S8:
	    format = AF_FORMAT_U8;
	    break;
	default:
	    format = AF_FORMAT_S16_LE;    /* artsd always expects little endian?*/
	    break;
	}

	ao_data.format = format;
	ao_data.channels = channels;
	ao_data.samplerate = rate_hz;
	ao_data.bps = (rate_hz*channels);

	if(format != AF_FORMAT_U8 && format != AF_FORMAT_S8)
		ao_data.bps*=2;

	stream=arts_play_stream(rate_hz, OBTAIN_BITRATE(format), channels, "MPlayer");

	if(stream == NULL) {
		mp_msg(MSGT_AO, MSGL_ERR, MSGTR_AO_ARTS_CantOpenStream);
		arts_free();
		return 0;
	}

	/* Set the stream to blocking: it will not block anyway, but it seems */
	/* to be working better */
	arts_stream_set(stream, ARTS_P_BLOCKING, 1);
	frag_spec = ARTS_PACKET_SIZE_LOG2 | ARTS_PACKETS << 16;
	arts_stream_set(stream, ARTS_P_PACKET_SETTINGS, frag_spec);
	ao_data.buffersize = arts_stream_get(stream, ARTS_P_BUFFER_SIZE);
	mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_ARTS_StreamOpen);

	mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_ARTS_BufferSize,
	    ao_data.buffersize);
	mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AO_ARTS_BufferSize,
	    arts_stream_get(stream, ARTS_P_PACKET_SIZE));

	return 1;
}

static void uninit(int immed)
{
	arts_close_stream(stream);
	arts_free();
}

static int play(void* data,int len,int flags)
{
	return arts_write(stream, data, len);
}

static void audio_pause(void)
{
}

static void audio_resume(void)
{
}

static void reset(void)
{
}

static int get_space(void)
{
	return arts_stream_get(stream, ARTS_P_BUFFER_SPACE);
}

static float get_delay(void)
{
	return ((float) (ao_data.buffersize - arts_stream_get(stream,
		ARTS_P_BUFFER_SPACE))) / ((float) ao_data.bps);
}

