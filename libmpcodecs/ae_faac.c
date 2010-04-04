/*
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
#include <faac.h>
#include "ae.h"


static faacEncHandle faac;
static faacEncConfigurationPtr config = NULL;
static int
	param_bitrate = 128,
	param_quality = 0,
	param_object_type = 1,
	param_mpeg = 2,
	param_tns = 0,
	param_raw = 0,
	param_cutoff = 0,
	param_format = 16,
	param_debug = 0;

static int enc_frame_size = 0, divisor;
static unsigned long samples_input, max_bytes_output;
static unsigned char *decoder_specific_buffer = NULL;
static unsigned long decoder_specific_len = 0;

const m_option_t faacopts_conf[] = {
	{"br", &param_bitrate, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"quality", &param_quality, CONF_TYPE_INT, CONF_RANGE, 0, 1000, NULL},
	{"object", &param_object_type, CONF_TYPE_INT, CONF_RANGE, 1, 4, NULL},
	{"mpeg", &param_mpeg, CONF_TYPE_INT, CONF_RANGE, 2, 4, NULL},
	{"tns", &param_tns, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"cutoff", &param_cutoff, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"format", &param_format, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"raw", &param_raw, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"debug", &param_debug, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};


static int bind_faac(audio_encoder_t *encoder, muxer_stream_t *mux_a)
{
	mux_a->wf = calloc(1, sizeof(WAVEFORMATEX) + decoder_specific_len + 256);
	mux_a->wf->wFormatTag = 0x706D;
	mux_a->wf->nChannels = encoder->params.channels;
	mux_a->h.dwSampleSize=0; // VBR
	mux_a->h.dwRate=encoder->params.sample_rate;
	mux_a->h.dwScale=encoder->params.samples_per_frame;
	mux_a->wf->nSamplesPerSec=mux_a->h.dwRate;
	mux_a->wf->nAvgBytesPerSec = encoder->params.bitrate / 8;

	mux_a->wf->nBlockAlign = mux_a->h.dwScale;
	mux_a->h.dwSuggestedBufferSize = (encoder->params.audio_preload*mux_a->wf->nAvgBytesPerSec)/1000;
	mux_a->h.dwSuggestedBufferSize -= mux_a->h.dwSuggestedBufferSize % mux_a->wf->nBlockAlign;

	mux_a->wf->cbSize = decoder_specific_len;
	mux_a->wf->wBitsPerSample = 0; /* does not apply */
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->wID = 1;
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->fdwFlags = 2;
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->nBlockSize = mux_a->wf->nBlockAlign;
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->nFramesPerBlock = 1;
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->nCodecDelay = 0;

	// Fix allocation
	mux_a->wf = realloc(mux_a->wf, sizeof(WAVEFORMATEX)+mux_a->wf->cbSize);

	if(config->inputFormat == FAAC_INPUT_FLOAT)
		encoder->input_format = AF_FORMAT_FLOAT_NE;
	else if(config->inputFormat == FAAC_INPUT_32BIT)
		encoder->input_format = AF_FORMAT_S32_NE;
	else
		encoder->input_format = AF_FORMAT_S16_NE;

	encoder->min_buffer_size = mux_a->h.dwSuggestedBufferSize;
	encoder->max_buffer_size = mux_a->h.dwSuggestedBufferSize*2;

	if(decoder_specific_buffer && decoder_specific_len)
		memcpy(mux_a->wf + 1, decoder_specific_buffer, decoder_specific_len);

	return 1;
}

static int get_frame_size(audio_encoder_t *encoder)
{
	int sz = enc_frame_size;
	enc_frame_size = 0;
	return sz;
}

static int encode_faac(audio_encoder_t *encoder, uint8_t *dest, void *src, int len, int max_size)
{
	if (encoder->params.channels >= 5)
		reorder_channel_nch(src, AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
		                    AF_CHANNEL_LAYOUT_AAC_DEFAULT,
		                    encoder->params.channels,
		                    len / divisor, divisor);

	// len is divided by the number of bytes per sample
	enc_frame_size = faacEncEncode(faac,  (int32_t*) src,  len / divisor, dest, max_size);

	return enc_frame_size;
}

int close_faac(audio_encoder_t *encoder)
{
	return 1;
}

int mpae_init_faac(audio_encoder_t *encoder)
{
	if(encoder->params.channels < 1 || encoder->params.channels > 6 || (param_mpeg != 2 && param_mpeg != 4))
	{
		mp_msg(MSGT_MENCODER, MSGL_FATAL, "AE_FAAC, unsupported number of channels: %d, or mpeg version: %d, exit\n", encoder->params.channels, param_mpeg);
		return 0;
	}

	faac = faacEncOpen(encoder->params.sample_rate, encoder->params.channels, &samples_input, &max_bytes_output);
	if(!faac)
	{
		mp_msg(MSGT_MENCODER, MSGL_FATAL, "AE_FAAC, couldn't init, exit\n");
		return 0;
	}
	mp_msg(MSGT_MENCODER, MSGL_V, "AE_FAAC, sample_input: %lu, max_bytes_output: %lu\n", samples_input, max_bytes_output);
	config = faacEncGetCurrentConfiguration(faac);
	if(!config)
	{
		mp_msg(MSGT_MENCODER, MSGL_FATAL, "AE_FAAC, couldn't get init configuration, exit\n");
		return 0;
	}

	param_bitrate *= 1000;
	if(param_quality)
		config->quantqual = param_quality;
	else
		config->bitRate = param_bitrate / encoder->params.channels;

	if(param_format==33)
	{
		config->inputFormat = FAAC_INPUT_FLOAT;
		divisor = 4;
	}
	else if(param_format==32)
	{
		config->inputFormat = FAAC_INPUT_32BIT;
		divisor = 4;
	}
	else
	{
		config->inputFormat = FAAC_INPUT_16BIT;
		divisor = 2;
	}
	config->outputFormat = param_raw ? 0 : 1; // 1 is ADTS
	config->aacObjectType = param_object_type;
	if(MAIN==0) config->aacObjectType--;
	config->mpegVersion = (param_mpeg == 4 ? MPEG4 : MPEG2);
	config->useTns = param_tns;
	config->allowMidside = 1;
	config->shortctl = SHORTCTL_NORMAL;
	param_cutoff = param_cutoff ? param_cutoff : encoder->params.sample_rate / 2;
	if(param_cutoff > encoder->params.sample_rate / 2)
		param_cutoff = encoder->params.sample_rate / 2;
	config->bandWidth = param_cutoff;
	if(encoder->params.channels == 6)
		config->useLfe = 1;

	if(!faacEncSetConfiguration(faac, config))
	{
		mp_msg(MSGT_MENCODER, MSGL_FATAL, "AE_FAAC, counldn't set specified parameters, exiting\n");
		return 0;
	}

	if(param_raw)
		faacEncGetDecoderSpecificInfo(faac, &decoder_specific_buffer, &decoder_specific_len);
	else
		decoder_specific_len = 0;

	encoder->params.bitrate = param_bitrate;
	encoder->params.samples_per_frame = 1024;
	encoder->decode_buffer_size =  divisor * samples_input;	//samples * 16 bits_per_sample

	encoder->bind = bind_faac;
	encoder->get_frame_size = get_frame_size;
	encoder->encode = encode_faac;
	encoder->close = close_faac;

	return 1;
}
