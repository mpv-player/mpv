#include "config.h"
#ifdef USE_DVDREAD
#ifndef _MPLAYER_SPUDEC_H
#define _MPLAYER_SPUDEC_H

#include "stream.h"
void spudec_heartbeat(void *this, int pts100);
void spudec_assemble(void *this, unsigned char *packet, int len, int pts100);
void spudec_draw(void *this, void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
void *spudec_new(dvd_priv_t *dvd_info);
void spudec_free(void *this);

#endif
#endif

