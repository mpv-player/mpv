#ifndef MPLAYER_ENCODE_H
#define MPLAYER_ENCODE_H

#include <stdbool.h>
#include <libavutil/avutil.h>

struct MPOpts;
struct encode_lavc_context;
struct encode_output_conf;

// interface for mplayer.c
struct encode_lavc_context *encode_lavc_init(struct encode_output_conf *options);
void encode_lavc_finish(struct encode_lavc_context *ctx);
void encode_lavc_free(struct encode_lavc_context *ctx);
void encode_lavc_discontinuity(struct encode_lavc_context *ctx);
bool encode_lavc_showhelp(struct MPOpts *opts);
int encode_lavc_getstatus(struct encode_lavc_context *ctx, char *buf, int bufsize, float relative_position, float playback_time);
void encode_lavc_expect_stream(struct encode_lavc_context *ctx, enum AVMediaType mt);
void encode_lavc_set_video_fps(struct encode_lavc_context *ctx, float fps);
bool encode_lavc_didfail(struct encode_lavc_context *ctx); // check if encoding failed

#endif
