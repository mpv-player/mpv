#ifndef _MPLAYER_SPUDEC_H
#define _MPLAYER_SPUDEC_H

void spudec_process_control(unsigned char *, int, int*, int*);
void spudec_decode(unsigned char *packet,int len);

#endif
