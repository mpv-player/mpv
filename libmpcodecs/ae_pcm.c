#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include "m_option.h"
#include "mp_msg.h"
#include "libmpdemux/aviheader.h"
#include "libaf/af_format.h"
#include "libaf/reorder_ch.h"
#include "libmpdemux/ms_hdr.h"
#include "stream/stream.h"
#include "libmpdemux/muxer.h"
#include "ae_pcm.h"


static int bind_pcm(audio_encoder_t *encoder, muxer_stream_t *mux_a)
{
	mux_a->h.dwScale=1;
	mux_a->h.dwRate=encoder->params.sample_rate;
	mux_a->wf=malloc(sizeof(WAVEFORMATEX));
	mux_a->wf->wFormatTag=0x1; // PCM
	mux_a->wf->nChannels=encoder->params.channels;
	mux_a->h.dwSampleSize=2*mux_a->wf->nChannels;
	mux_a->wf->nBlockAlign=mux_a->h.dwSampleSize;
	mux_a->wf->nSamplesPerSec=mux_a->h.dwRate;
	mux_a->wf->nAvgBytesPerSec=mux_a->h.dwSampleSize*mux_a->wf->nSamplesPerSec;
	mux_a->wf->wBitsPerSample=16;
	mux_a->wf->cbSize=0; // FIXME for l3codeca.acm
	
	encoder->input_format = (mux_a->wf->wBitsPerSample==8) ? AF_FORMAT_U8 : AF_FORMAT_S16_LE;
	encoder->min_buffer_size = 16384;
	encoder->max_buffer_size = mux_a->wf->nAvgBytesPerSec;
	
	return 1;
}

static int encode_pcm(audio_encoder_t *encoder, uint8_t *dest, void *src, int nsamples, int max_size)
{
	max_size = FFMIN(nsamples, max_size);
	if (encoder->params.channels == 6 || encoder->params.channels == 5) {
		max_size -= max_size % (encoder->params.channels * 2);
		reorder_channel_copy_nch(src, AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
		                         dest, AF_CHANNEL_LAYOUT_WAVEEX_DEFAULT,
		                         encoder->params.channels,
		                         max_size / 2, 2);
	}
	else
	memcpy(dest, src, max_size);
	return max_size;
}

static int set_decoded_len(audio_encoder_t *encoder, int len)
{
	return len;
}

static int close_pcm(audio_encoder_t *encoder)
{
	return 1;
}

static int get_frame_size(audio_encoder_t *encoder)
{
	return 0;
}

int mpae_init_pcm(audio_encoder_t *encoder)
{
	encoder->params.samples_per_frame = encoder->params.sample_rate;
	encoder->params.bitrate = encoder->params.sample_rate * encoder->params.channels * 2 * 8;
	
	encoder->decode_buffer_size = encoder->params.bitrate / 8;
	encoder->bind = bind_pcm;
	encoder->get_frame_size = get_frame_size;
	encoder->set_decoded_len = set_decoded_len;
	encoder->encode = encode_pcm;
	encoder->close = close_pcm;
	
	return 1;
}

