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
};

struct attachment_list {
    struct demux_attachment *entries;
    int num_entries;
};

struct dec_sub *sub_create(struct mpv_global *global, struct sh_stream *sh,
                           struct attachment_list *attachments);
void sub_destroy(struct dec_sub *sub);
void sub_lock(struct dec_sub *sub);
void sub_unlock(struct dec_sub *sub);

bool sub_can_preload(struct dec_sub *sub);
void sub_preload(struct dec_sub *sub);
bool sub_read_packets(struct dec_sub *sub, double video_pts);
void sub_get_bitmaps(struct dec_sub *sub, struct mp_osd_res dim, int format,
                     double pts, struct sub_bitmaps *res);
char *sub_get_text(struct dec_sub *sub, double pts);
void sub_reset(struct dec_sub *sub);
void sub_select(struct dec_sub *sub, bool selected);
void sub_update_opts(struct dec_sub *sub);
void sub_set_recorder_sink(struct dec_sub *sub, struct mp_recorder_sink *sink);

int sub_control(struct dec_sub *sub, enum sd_ctrl cmd, void *arg);

#endif
