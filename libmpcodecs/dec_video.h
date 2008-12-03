#ifndef MPLAYER_DEC_VIDEO_H
#define MPLAYER_DEC_VIDEO_H

#include "libmpdemux/stheader.h"

// dec_video.c:
void vfm_help(void);

int init_best_video_codec(sh_video_t *sh_video, char** video_codec_list, char** video_fm_list);
void uninit_video(sh_video_t *sh_video);

void *decode_video(sh_video_t *sh_video, unsigned char *start, int in_size, int drop_frame, double pts);
int filter_video(sh_video_t *sh_video, void *frame, double pts);

int get_video_quality_max(sh_video_t *sh_video);
void set_video_quality(sh_video_t *sh_video, int quality);

int get_video_colors(sh_video_t *sh_video, const char *item, int *value);
int set_video_colors(sh_video_t *sh_video, const char *item, int value);
int set_rectangle(sh_video_t *sh_video, int param, int value);
void resync_video_stream(sh_video_t *sh_video);
int get_current_video_decoder_lag(sh_video_t *sh_video);

extern int divx_quality;

#endif /* MPLAYER_DEC_VIDEO_H */
