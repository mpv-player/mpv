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

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"
#include "../config.h"
#include "../mp_msg.h"

#define OBTAIN_BITRATE(a) (((a != AFMT_U8) || (a != AFMT_S8)) ? 16 : 8)

static arts_stream_t stream;

static ao_info_t info =
{
    "aRts audio output",
    "arts",
    "Michele Balistreri <brain87@gmx.net>",
    ""
};

LIBAO_EXTERN(arts)

static int control(int cmd, int arg)
{
	return(CONTROL_UNKNOWN);
}

static int init(int rate_hz, int channels, int format, int flags)
{
	int err;
	int frag_spec;

	if(err=arts_init()) {
		mp_msg(MSGT_AO, MSGL_ERR, "AO: [arts] %s\n", arts_error_text(err));
		return 0;
	}

	else
		mp_msg(MSGT_AO, MSGL_INFO, "AO: [arts] Connected to sound server\n");

	ao_data.format = format;
	ao_data.channels = channels;
	ao_data.samplerate = rate_hz;
	ao_data.bps = (rate_hz*channels);
	ao_data.buffersize = 4096;

	if(format != AFMT_U8 && format != AFMT_S8)
		ao_data.bps*=2;

	for (frag_spec = 0; (0x01<<frag_spec) < ao_data.buffersize; ++frag_spec)
		;
	frag_spec |= 0x00020000;
	stream=arts_play_stream(rate_hz, OBTAIN_BITRATE(format), channels, "Mplayer");
	arts_stream_set(stream, ARTS_P_PACKET_SETTINGS, frag_spec);
	arts_stream_set(stream, ARTS_P_BLOCKING, 0);

	if(stream == NULL) {
		mp_msg(MSGT_AO, MSGL_ERR, "AO: [arts] Unable to open a stream\n");
		return 0;
	}

	else
		mp_msg(MSGT_AO, MSGL_INFO, "AO: [arts] Stream opened\n");


	return 1;
}

static void uninit()
{
	arts_close_stream(stream);
	arts_free();
}

static int play(void* data,int len,int flags)
{
	arts_write(stream, data, len);
}

static void audio_pause()
{
}

static void audio_resume()
{
}

static void reset()
{
}

static int get_space()
{
	return arts_stream_get(stream, ARTS_P_BUFFER_SPACE);
}

static float get_delay()
{
	return ((float) arts_stream_get(stream, ARTS_P_BUFFER_SIZE) / (float) ao_data.bps);
}







