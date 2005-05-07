#ifndef MPAE_TWOLAME_H
#define MPAE_TWOLAME_H

#include "ae.h"
#include <twolame.h>

typedef struct {
	twolame_options *twolame_ctx;
	int vbr;
} mpae_twolame_ctx;

int mpae_init_twolame(audio_encoder_t *encoder);

#endif
