#include "m_option.h"
#include "../mp_msg.h"
#include <stdlib.h>
#include <inttypes.h>
#include "ae_toolame.h"


static int 
    param_bitrate = 192,
    param_srate = 48000,
    param_psy = 3,
    param_maxvbr = 192,
    param_errprot = 0,
    param_debug = 0;
    
float param_vbr = 0;
static char *param_mode = "stereo";
    
m_option_t toolameopts_conf[] = {
	{"br", &param_bitrate, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"mode", &param_mode, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"psy", &param_psy, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
	{"vbr", &param_vbr, CONF_TYPE_FLOAT, CONF_RANGE, 0, 50, NULL},
	{"maxvbr", &param_maxvbr, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"errprot", &param_errprot, CONF_TYPE_INT, CONF_RANGE, 0, 1, NULL},
	{"debug", &param_debug, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};


mpae_toolame_ctx *mpae_init_toolame(int channels, int srate)
{
	int mode;
	mpae_toolame_ctx *ctx = NULL;
	
	if(channels == 1)
	{
		mp_msg(MSGT_MENCODER, MSGL_INFO, "ae_toolame, 1 audio channel, forcing mono mode\n");
		mode = MPG_MD_MONO;
	}
	else if(channels == 2)
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
		return NULL;
	}
	
	ctx->toolame_ctx = toolame_init();
	if(ctx->toolame_ctx == NULL)
	{
		mp_msg(MSGT_MENCODER, MSGL_ERR, "ae_toolame, couldn't initial parameters from libtoolame, exiting\n");
		free(ctx);
		return NULL;
	}
	ctx->channels = channels;
	ctx->srate = srate;

	if(toolame_setMode(ctx->toolame_ctx, mode) != 0)
		return NULL;
	
	if(toolame_setPsymodel(ctx->toolame_ctx, param_psy) != 0)
		return NULL;
	
	if(toolame_setSampleFreq(ctx->toolame_ctx, srate) != 0)
		return NULL;
	
	if(toolame_setBitrate(ctx->toolame_ctx, param_bitrate) != 0)
		return NULL;
	
	if(param_errprot)
		if(toolame_setErrorProtection(ctx->toolame_ctx, TRUE) != 0)
			return NULL;
	
	if(param_vbr > 0)
	{
		if(toolame_setVBR(ctx->toolame_ctx, TRUE) != 0)
			return NULL;
		if(toolame_setVBRLevel(ctx->toolame_ctx, param_maxvbr) != 0)
			return NULL;
		if(toolame_setPadding(ctx->toolame_ctx, FALSE) != 0)
			return NULL;
		if(toolame_setVBRUpperBitrate(ctx->toolame_ctx, param_maxvbr) != 0)
			return NULL;
	}
	
	if(toolame_setVerbosity(ctx->toolame_ctx, param_debug) != 0)
		return NULL;
	
	if(toolame_init_params(ctx->toolame_ctx) != 0)
		return NULL;
	
	ctx->bitrate = param_bitrate;
	
	return ctx;
}


int mpae_encode_toolame(mpae_toolame_ctx *ctx, uint8_t *dest, int nsamples, void *src, int max_size)
{
	int ret_size = 0, i;
	int16_t *buffer;
	
	buffer = (uint16_t *) src;
	for(i = 0; i < nsamples; i++)
	{
	    ctx->left_pcm[i] = buffer[2 * i];
	    ctx->right_pcm[i] = buffer[2 * i + (ctx->channels - 1)];
	}
	
	toolame_encode_buffer(ctx->toolame_ctx, ctx->left_pcm, ctx->right_pcm, nsamples, dest, max_size, &ret_size);
	
	return ret_size;
}
