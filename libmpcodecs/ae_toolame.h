#ifndef MPAE_TOOLAME_H
#define MPAE_TOOLAME_H

#include <toolame.h>

typedef struct {
	toolame_options *toolame_ctx;
	int channels, srate, bitrate;
	int16_t left_pcm[1152], right_pcm[1152];
} mpae_toolame_ctx;

mpae_toolame_ctx *mpae_init_toolame(int channels, int srate);
int mpae_encode_toolame(mpae_toolame_ctx *ctx, uint8_t *dest, int nsamples, void *src, int max_size);

#endif
