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
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>
#include "config.h"
#include "libmpdemux/aviheader.h"
#include "libmpdemux/ms_hdr.h"
#include "stream/stream.h"
#include "libmpdemux/muxer.h"
#include "ae.h"

#include "ae_pcm.h"

#ifdef CONFIG_TOOLAME
#include "ae_toolame.h"
#endif

#ifdef CONFIG_MP3LAME
#include "ae_lame.h"
#endif

#ifdef CONFIG_LIBAVCODEC
#include "ae_lavc.h"
#endif

#ifdef CONFIG_FAAC
#include "ae_faac.h"
#endif

#ifdef CONFIG_TWOLAME
#include "ae_twolame.h"
#endif

audio_encoder_t *new_audio_encoder(muxer_stream_t *stream, audio_encoding_params_t *params)
{
	int ris;
	audio_encoder_t *encoder;
	if(! params)
		return NULL;

	encoder = calloc(1, sizeof(audio_encoder_t));
	memcpy(&encoder->params, params, sizeof(audio_encoding_params_t));
	encoder->stream = stream;

	switch(stream->codec)
	{
		case ACODEC_PCM:
			ris = mpae_init_pcm(encoder);
			break;
#ifdef CONFIG_TOOLAME
		case ACODEC_TOOLAME:
			ris = mpae_init_toolame(encoder);
			break;
#endif
#ifdef CONFIG_LIBAVCODEC
		case ACODEC_LAVC:
			ris = mpae_init_lavc(encoder);
			break;
#endif
#ifdef CONFIG_MP3LAME
		case ACODEC_VBRMP3:
			ris = mpae_init_lame(encoder);
			break;
#endif
#ifdef CONFIG_FAAC
		case ACODEC_FAAC:
			ris = mpae_init_faac(encoder);
			break;
#endif
#ifdef CONFIG_TWOLAME
		case ACODEC_TWOLAME:
			ris = mpae_init_twolame(encoder);
			break;
#endif
		default:
			ris = 0;
			break;
	}

	if(! ris)
	{
		free(encoder);
		return NULL;
	}
	encoder->bind(encoder, stream);
	encoder->decode_buffer = malloc(encoder->decode_buffer_size);
	if(! encoder->decode_buffer)
	{
		free(encoder);
		return NULL;
	}

	encoder->codec = stream->codec;
	return encoder;
}
