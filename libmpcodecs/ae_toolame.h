#ifndef MPAE_TOOLAME_H
#define MPAE_TOOLAME_H

#include "ae.h"
#include <toolame.h>

typedef struct {
	toolame_options *toolame_ctx;
	int channels, srate, bitrate;
	int vbr;
	int16_t left_pcm[1152], right_pcm[1152];
} mpae_toolame_ctx;

int mpae_init_toolame(audio_encoder_t *encoder);

#endif
