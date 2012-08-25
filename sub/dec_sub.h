#ifndef MPLAYER_DEC_SUB_H
#define MPLAYER_DEC_SUB_H

struct sh_sub;
struct osd_state;

typedef struct mp_eosd_res {
    int w, h; // screen dimensions, including black borders
    int mt, mb, ml, mr; // borders (top, bottom, left, right)
} mp_eosd_res_t;

static inline bool is_text_sub(int type)
{
    return type == 't' || type == 'm' || type == 'a';
}

void sub_decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                int data_len, double pts, double duration);
void sub_init(struct sh_sub *sh, struct osd_state *osd);
void sub_reset(struct sh_sub *sh, struct osd_state *osd);
void sub_switchoff(struct sh_sub *sh, struct osd_state *osd);
void sub_uninit(struct sh_sub *sh);

#endif
