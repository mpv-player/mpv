#ifndef _MPLAYER_SPUDEC_H
#define _MPLAYER_SPUDEC_H

void spudec_heartbeat(void *this, unsigned int pts100);
void spudec_assemble(void *this, unsigned char *packet, unsigned int len, unsigned int pts100);
void spudec_draw(void *this, void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
void spudec_draw_scaled(void *this, unsigned int dxs, unsigned int dys, void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
void spudec_update_palette(void *this, unsigned int *palette);
void *spudec_new_scaled(unsigned int *palette, unsigned int frame_width, unsigned int frame_height);
void *spudec_new(unsigned int *palette);
void spudec_free(void *this);
void spudec_reset(void *this);	// called after seek
int spudec_visible(void *this); // check if spu is visible
void spudec_set_font_factor(double factor); // sets the equivalent to ffactor

#endif

