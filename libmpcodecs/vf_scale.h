//GPL

#ifndef MPLAYER_VF_SCALE_H
#define MPLAYER_VF_SCALE_H

int get_sws_cpuflags(void);
struct SwsContext *sws_getContextFromCmdLine(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat);

#endif /* MPLAYER_VF_SCALE_H */
