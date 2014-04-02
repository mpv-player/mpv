#ifndef MPLAYER_ENCODE_H
#define MPLAYER_ENCODE_H

#include <stdbool.h>

#include "demux/demux.h"

struct mpv_global;
struct mp_log;
struct encode_lavc_context;
struct encode_output_conf;

// interface for mplayer.c
struct encode_lavc_context *encode_lavc_init(struct encode_output_conf *options,
                                             struct mpv_global *global);
void encode_lavc_finish(struct encode_lavc_context *ctx);
void encode_lavc_free(struct encode_lavc_context *ctx);
void encode_lavc_discontinuity(struct encode_lavc_context *ctx);
bool encode_lavc_showhelp(struct mp_log *log, struct encode_output_conf *options);
int encode_lavc_getstatus(struct encode_lavc_context *ctx, char *buf, int bufsize, float relative_position);
void encode_lavc_expect_stream(struct encode_lavc_context *ctx, int mt);
void encode_lavc_set_metadata(struct encode_lavc_context *ctx,
                              struct mp_tags *metadata);
void encode_lavc_set_video_fps(struct encode_lavc_context *ctx, float fps);
void encode_lavc_set_audio_pts(struct encode_lavc_context *ctx, double pts);
bool encode_lavc_didfail(struct encode_lavc_context *ctx); // check if encoding failed

#endif
