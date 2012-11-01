#ifndef MPLAYER_DEC_SUB_H
#define MPLAYER_DEC_SUB_H

#include <stdbool.h>
#include <stdint.h>

#include "sub/sub.h"

struct sh_sub;
struct ass_track;
struct MPOpts;

static inline bool is_text_sub(int type)
{
    return type == 't' || type == 'm' || type == 'a';
}

void sub_decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                int data_len, double pts, double duration);
void sub_get_bitmaps(struct osd_state *osd, struct mp_osd_res dim, double pts,
                     struct sub_bitmaps *res);
void sub_init(struct sh_sub *sh, struct osd_state *osd);
void sub_reset(struct sh_sub *sh, struct osd_state *osd);
void sub_switchoff(struct sh_sub *sh, struct osd_state *osd);
void sub_uninit(struct sh_sub *sh);

struct sh_sub *sd_ass_create_from_track(struct ass_track *track,
                                        bool vsfilter_aspect,
                                        struct MPOpts *opts);

#ifdef CONFIG_ASS
struct ass_track *sub_get_ass_track(struct osd_state *osd);
#endif

#endif
