#ifndef _MPLAYER_SPUDEC_H
#define _MPLAYER_SPUDEC_H

void spudec_heartbeat(void *this, int pts100);
void spudec_assemble(void *this, unsigned char *packet, int len, int pts100);
void spudec_draw(void *this, void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
void *spudec_new();
void spudec_free(void *this);

#endif
