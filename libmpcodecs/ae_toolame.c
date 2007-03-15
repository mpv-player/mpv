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
#include "libmpdemux/ms_hdr.h"
#include "stream/stream.h"
#include "libmpdemux/muxer.h"
#include "ae_toolame.h"
#include "libmpdemux/mp3_hdr.h"


static int 
    param_bitrate = 192,
    param_psy = 3,
    param_maxvbr = 0,
    param_errprot = 0,
    param_debug = 0;
    
static float param_vbr = 0;
static char *param_mode = "stereo";
    
m_option_t toolameopts_conf[] = {
	{"br", &param_bitrate, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"mode", &param_mode, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"psy", &param_psy, CONF_TYPE_INT, CONF_RANGE, -1, 4, NULL},
	{"vbr", &param_vbr, CONF_TYPE_FLOAT, CONF_RANGE, -50, 50, NULL},
	{"maxvbr", &param_maxvbr, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"errprot", &param_errprot, CONF_TYPE_INT, CONF_RANGE, 0, 1, NULL},
	{"debug", &param_debug, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};


static int bind_toolame(audio_encoder_t *encoder, muxer_stream_t *mux_a)
{
	mpae_toolame_ctx *ctx = (mpae_toolame_ctx *) encoder->priv;
	
	mux_a->wf = malloc(sizeof(WAVEFORMATEX)+256);
	mux_a->wf->wFormatTag = 0x50;
	mux_a->wf->nChannels = encoder->params.channels;
	mux_a->wf->nSamplesPerSec = encoder->params.sample_rate;
	mux_a->wf->nAvgBytesPerSec = 125 * encoder->params.bitrate;
	
	if(ctx->vbr || ((mux_a->wf->nAvgBytesPerSec * encoder->params.samples_per_frame) % mux_a->wf->nSamplesPerSec))
	{
		mux_a->h.dwScale = encoder->params.samples_per_frame;
		mux_a->h.dwRate = encoder->params.sample_rate;
		mux_a->h.dwSampleSize = 0; // Blocksize not constant
	}
	else
	{
		mux_a->h.dwScale = (mux_a->wf->nAvgBytesPerSec * encoder->params.samples_per_frame)/ mux_a->wf->nSamplesPerSec; /* for cbr */
		mux_a->h.dwRate = mux_a->wf->nAvgBytesPerSec;
		mux_a->h.dwSampleSize = mux_a->h.dwScale;
	}
	mux_a->wf->nBlockAlign = mux_a->h.dwScale;
	mux_a->h.dwSuggestedBufferSize = (encoder->params.audio_preload*mux_a->wf->nAvgBytesPerSec)/1000;
	mux_a->h.dwSuggestedBufferSize -= mux_a->h.dwSuggestedBufferSize % mux_a->wf->nBlockAlign;
	
	mux_a->wf->cbSize = 0; //12;
	mux_a->wf->wBitsPerSample = 0; /* does not apply */
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->wID = 1;
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->fdwFlags = 2;
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->nBlockSize = mux_a->wf->nBlockAlign;
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->nFramesPerBlock = 1;
	((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->nCodecDelay = 0;
	
	// Fix allocation    
	mux_a->wf = realloc(mux_a->wf, sizeof(WAVEFORMATEX)+mux_a->wf->cbSize);
	
	encoder->input_format = AF_FORMAT_S16_NE;
	encoder->min_buffer_size = mux_a->h.dwSuggestedBufferSize;
	encoder->max_buffer_size = mux_a->h.dwSuggestedBufferSize*2;

	return 1;
}

static int encode_toolame(audio_encoder_t *encoder, uint8_t *dest, void *src, int len, int max_size)
{
	mpae_toolame_ctx *ctx = (mpae_toolame_ctx *)encoder->priv;
	int ret_size = 0, r2, i, nsamples;
	int16_t *buffer;
	
	nsamples = len / (2*encoder->params.channels);
	buffer = (uint16_t *) src;
	for(i = 0; i < nsamples; i++)
	{
	    ctx->left_pcm[i] = buffer[ctx->channels * i];
	    ctx->right_pcm[i] = buffer[(ctx->channels * i) + (ctx->channels - 1)];
	}
	
	toolame_encode_buffer(ctx->toolame_ctx, ctx->left_pcm, ctx->right_pcm, nsamples, dest, max_size, &ret_size);
	r2 = mp_decode_mp3_header(dest);	
	mp_msg(MSGT_MENCODER, MSGL_DBG2, "\nSIZE: %d, max: %d, r2: %d\n", ret_size, max_size, r2);
	if(r2 > 0)
		ret_size = r2;
	return ret_size;
}

int close_toolame(audio_encoder_t *encoder)
{
	free(encoder->priv);
	return 1;
}

static int get_frame_size(audio_encoder_t *encoder)
{
	int sz;
	if(encoder->stream->buffer_len < 4)
		return 0;
	sz = mp_decode_mp3_header(encoder->stream->buffer);
	if(sz <= 0)
		return 0;
	return sz;
}


int mpae_init_toolame(audio_encoder_t *encoder)
{
	int mode;
	mpae_toolame_ctx *ctx = NULL;
	
	if(encoder->params.channels == 1)
	{
		mp_msg(MSGT_MENCODER, MSGL_INFO, "ae_toolame, 1 audio channel, forcing mono mode\n");
		mode = MPG_MD_MONO;
	}
	else if(encoder->params.channels == 2)
	{
		if(! strcasecmp(param_mode, "dual"))
			mode = MPG_MD_DUAL_CHANNEL;
		else if(! strcasecmp(param_mode, "jstereo"))
			mode = MPG_MD_JOINT_STEREO;
		else if(! strcasecmp(param_mode, "stereo"))
			mode = MPG_MD_STEREO;
		else
		{
			mp_msg(MSGT_MENCODER, MSGL_ERR, "ae_toolame, unknown mode %s, exiting\n", param_mode);
		}
	}
	else
		mp_msg(MSGT_MENCODER, MSGL_ERR, "ae_toolame, Toolame can't encode > 2 channels, exiting\n");
	
	ctx = (mpae_toolame_ctx *) calloc(1, sizeof(mpae_toolame_ctx));
	if(ctx == NULL)
	{
		mp_msg(MSGT_MENCODER, MSGL_ERR, "ae_toolame, couldn't alloc a %d bytes context, exiting\n", sizeof(mpae_toolame_ctx));
		return 0;
	}
	
	ctx->toolame_ctx = toolame_init();
	if(ctx->toolame_ctx == NULL)
	{
		mp_msg(MSGT_MENCODER, MSGL_ERR, "ae_toolame, couldn't initial parameters from libtoolame, exiting\n");
		free(ctx);
		return 0;
	}
	ctx->vbr = 0;
	ctx->channels = encoder->params.channels;
	ctx->srate = encoder->params.sample_rate;

	if(toolame_setMode(ctx->toolame_ctx, mode) != 0)
		return 0;
	
	if(toolame_setPsymodel(ctx->toolame_ctx, param_psy) != 0)
		return 0;
	
	if(toolame_setSampleFreq(ctx->toolame_ctx, encoder->params.sample_rate) != 0)
		return 0;
	
	if(toolame_setBitrate(ctx->toolame_ctx, param_bitrate) != 0)
		return 0;
	
	if(param_errprot)
		if(toolame_setErrorProtection(ctx->toolame_ctx, TRUE) != 0)
			return 0;
	
	if(param_vbr != 0)
	{
		if(toolame_setVBR(ctx->toolame_ctx, TRUE) != 0)
			return 0;
		if(toolame_setVBRLevel(ctx->toolame_ctx, param_vbr) != 0)
			return 0;
		if(toolame_setPadding(ctx->toolame_ctx, FALSE) != 0)
			return 0;
		if(param_maxvbr)
		{
			if(toolame_setVBRUpperBitrate(ctx->toolame_ctx, param_maxvbr) != 0)
				return 0;
		}
		ctx->vbr = 1;
	}
	
	if(toolame_setVerbosity(ctx->toolame_ctx, param_debug) != 0)
		return 0;
	
	if(toolame_init_params(ctx->toolame_ctx) != 0)
		return 0;
	
	ctx->bitrate = param_bitrate;
	encoder->params.bitrate = ctx->bitrate;
	encoder->params.samples_per_frame = 1152;
	encoder->priv = ctx;
	encoder->decode_buffer_size = 1152 * 2 * encoder->params.channels;
	
	encoder->bind = bind_toolame;
	encoder->get_frame_size = get_frame_size;
	encoder->encode = encode_toolame;
	encoder->close = close_toolame;
	
	return 1;
}

