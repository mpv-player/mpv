#ifndef MPLAYER_DEC_SUB_H
#define MPLAYER_DEC_SUB_H

#include <stdbool.h>
#include <stdint.h>

#include "osd.h"

struct sh_stream;
struct mpv_global;
struct demux_packet;
struct mp_recorder_sink;
struct dec_sub;
struct sd;

enum sd_ctrl {
    SD_CTRL_SUB_STEP,
    SD_CTRL_SET_VIDEO_PARAMS,
    SD_CTRL_SET_TOP,
    SD_CTRL_SET_VIDEO_DEF_FPS,
    SD_CTRL_UPDATE_OPTS,
};

enum sd_text_type {
    SD_TEXT_TYPE_PLAIN,
    SD_TEXT_TYPE_ASS,
};

struct sd_times {
    double start;
    double end;
};

struct attachment_list {
    struct demux_attachment *entries;
    int num_entries;
};

struct dec_sub *sub_create(struct mpv_global *global, struct sh_stream *sh,
                           struct attachment_list *attachments, int order);
void sub_destroy(struct dec_sub *sub);

bool sub_can_preload(struct dec_sub *sub);
void sub_preload(struct dec_sub *sub);
bool sub_read_packets(struct dec_sub *sub, double video_pts);
struct sub_bitmaps *sub_get_bitmaps(struct dec_sub *sub, struct mp_osd_res dim,
                                    int format, double pts);
char *sub_get_text(struct dec_sub *sub, double pts, enum sd_text_type type);
struct sd_times sub_get_times(struct dec_sub *sub, double pts);
void sub_reset(struct dec_sub *sub);
void sub_select(struct dec_sub *sub, bool selected);
void sub_set_recorder_sink(struct dec_sub *sub, struct mp_recorder_sink *sink);
void sub_set_play_dir(struct dec_sub *sub, int dir);
bool sub_is_secondary_visible(struct dec_sub *sub);

int sub_control(struct dec_sub *sub, enum sd_ctrl cmd, void *arg);

#endif
