#ifndef MPLAYER_SD_H
#define MPLAYER_SD_H

struct osd_state;
struct sh_sub;
struct sub_bitmaps;

struct sd_functions {
    int  (*init)(struct sh_sub *sh, struct osd_state *osd);
    void (*decode)(struct sh_sub *sh, struct osd_state *osd,
                   void *data, int data_len, double pts, double duration);
    void (*get_bitmaps)(struct sh_sub *sh, struct osd_state *osd,
                        struct sub_bitmaps *res);
    void (*reset)(struct sh_sub *sh, struct osd_state *osd);
    void (*switch_off)(struct sh_sub *sh, struct osd_state *osd);
    void (*uninit)(struct sh_sub *sh);
};

#endif
