#ifndef MPLAYER_DEC_SUB_H
#define MPLAYER_DEC_SUB_H

#include <stdbool.h>
#include <stdint.h>

#include "sub/sub.h"

struct sh_sub;
struct ass_track;
struct MPOpts;

bool sub_accept_packets_in_advance(struct sh_sub *sh);
void sub_decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                int data_len, double pts, double duration);
void sub_get_bitmaps(struct osd_state *osd, struct mp_osd_res dim, double pts,
                     struct sub_bitmaps *res);
char *sub_get_text(struct osd_state *osd, double pts);
void sub_init(struct sh_sub *sh, struct osd_state *osd);
void sub_reset(struct sh_sub *sh, struct osd_state *osd);
void sub_switchoff(struct sh_sub *sh, struct osd_state *osd);
void sub_uninit(struct sh_sub *sh);

#ifdef CONFIG_ASS
struct ass_track *sub_get_ass_track(struct osd_state *osd);
#endif

#endif
